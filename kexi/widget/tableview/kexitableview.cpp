/* This file is part of the KDE project
   Copyright (C) 2002 Till Busch <till@bux.at>
   Copyright (C) 2003 Lucijan Busch <lucijan@gmx.at>
   Copyright (C) 2003 Daniel Molkentin <molkentin@kde.org>
   Copyright (C) 2003 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2003-2005 Jaroslaw Staniek <js@iidea.pl>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Original Author:  Till Busch <till@bux.at>
   Original Project: buX (www.bux.at)
*/

#include <qpainter.h>
#include <qkeycode.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qwmatrix.h>
#include <qtimer.h>
#include <qpopupmenu.h>
#include <qcursor.h>
#include <qstyle.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <unistd.h>

#include <config.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kmessagebox.h>

#ifndef KEXI_NO_PRINT
# include <kprinter.h>
#endif

#include "kexitableview.h"
#include "kexi_utils.h"
#include "kexivalidator.h"

#include "kexidatetableedit.h"
#include "kexitimetableedit.h"
#include "kexidatetimetableedit.h"
#include "kexicelleditorfactory.h"
#include "kexitableedit.h"
#include "kexiinputtableedit.h"
#include "kexicomboboxtableedit.h"
#include "kexiblobtableedit.h"
#include "kexibooltableedit.h"
#include "kexitableview_p.h"
#include <widget/utils/kexirecordmarker.h>

KexiTableView::Appearance::Appearance(QWidget *widget)
 : alternateBackgroundColor( KGlobalSettings::alternateBackgroundColor() )
{
	//set defaults
	if (qApp) {
		QPalette p = widget ? widget->palette() : qApp->palette();
		baseColor = p.active().base();
		textColor = p.active().text();
		borderColor = QColor(200,200,200);
		emptyAreaColor = p.active().color(QColorGroup::Base);
		rowHighlightingColor = QColor(
			(alternateBackgroundColor.red()+baseColor.red())/2,
			(alternateBackgroundColor.green()+baseColor.green())/2,
			(alternateBackgroundColor.blue()+baseColor.blue())/2);
		rowHighlightingTextColor = textColor;
	}
	backgroundAltering = true;
	rowHighlightingEnabled = false;
	navigatorEnabled = true;
	fullRowSelection = false;
}


//-----------------------------------------

TableViewHeader::TableViewHeader(QWidget * parent, const char * name) 
	: QHeader(parent, name)
	, m_lastToolTipSection(-1)
{
	installEventFilter(this);
	connect(this, SIGNAL(sizeChange(int,int,int)), 
		this, SLOT(slotSizeChange(int,int,int)));
}

int TableViewHeader::addLabel ( const QString & s, int size )
{
	m_toolTips += "";
	slotSizeChange(0,0,0);//refresh
	return QHeader::addLabel(s, size);
}

int TableViewHeader::addLabel ( const QIconSet & iconset, const QString & s, int size )
{
	m_toolTips += "";
	slotSizeChange(0,0,0);//refresh
	return QHeader::addLabel(iconset, s, size);
}

void TableViewHeader::removeLabel( int section )
{
	if (section < 0 || section >= count())
		return;
	QStringList::Iterator it = m_toolTips.begin();
	it += section;
	m_toolTips.remove(it);
	slotSizeChange(0,0,0);//refresh
	QHeader::removeLabel(section);
}

void TableViewHeader::setToolTip( int section, const QString & toolTip )
{
	if (section < 0 || section >= (int)m_toolTips.count())
		return;
	m_toolTips[ section ] = toolTip;
}

bool TableViewHeader::eventFilter(QObject * watched, QEvent * e)
{
	if (e->type()==QEvent::MouseMove) {
		const int section = sectionAt( static_cast<QMouseEvent*>(e)->x() );
		if (section != m_lastToolTipSection && section >= 0 && section < (int)m_toolTips.count()) {
			QToolTip::remove(this, m_toolTipRect);
			QString tip = m_toolTips[ section ];
			if (tip.isEmpty()) { //try label
				QFontMetrics fm(font());
				int minWidth = fm.width( label( section ) ) + style().pixelMetric( QStyle::PM_HeaderMargin );
				QIconSet *iset = iconSet( section );
				if (iset)
					minWidth += (2+iset->pixmap( QIconSet::Small, QIconSet::Normal ).width()); //taken from QHeader::sectionSizeHint()
				if (minWidth > sectionSize( section ))
					tip = label( section );
			}
			if (tip.isEmpty()) {
				m_lastToolTipSection = -1;
			}
			else {
				QToolTip::add(this, m_toolTipRect = sectionRect(section), tip);
				m_lastToolTipSection = section;
			}
		}
	}
//			if (e->type()==QEvent::MouseButtonPress) {
//	todo
//			}
	return QHeader::eventFilter(watched, e);
}

void TableViewHeader::slotSizeChange(int /*section*/, int /*oldSize*/, int /*newSize*/ )
{
	if (m_lastToolTipSection>0)
		QToolTip::remove(this, m_toolTipRect);
	m_lastToolTipSection = -1; //tooltip's rect is now invalid
}


//-----------------------------------------

/*moved
//sanity check
#define CHECK_DATA(r) \
	if (!m_data) { kdWarning() << "KexiTableView: No data assigned!" << endl; return r; }
#define CHECK_DATA_ \
	if (!m_data) { kdWarning() << "KexiTableView: No data assigned!" << endl; return; }
*/

bool KexiTableView_cellEditorFactoriesInitialized = false;

// Initializes standard editor cell editor factories
void KexiTableView::initCellEditorFactories()
{
	if (KexiTableView_cellEditorFactoriesInitialized)
		return;
	KexiCellEditorFactoryItem* item;
	item = new KexiBlobEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::BLOB );

	item = new KexiDateEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::Date );

	item = new KexiTimeEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::Time );

	item = new KexiDateTimeEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::DateTime );

	item = new KexiComboBoxEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::Enum );

	item = new KexiBoolEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::Boolean );

	item = new KexiKIconTableEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::Text, "KIcon" );

	//default type
	item = new KexiInputEditorFactoryItem();
	KexiCellEditorFactory::registerItem( *item, KexiDB::Field::InvalidType );

	KexiTableView_cellEditorFactoriesInitialized = true;
}



KexiTableView::KexiTableView(KexiTableViewData* data, QWidget* parent, const char* name)
: QScrollView(parent, name, /*Qt::WRepaintNoErase | */Qt::WStaticContents /*| Qt::WResizeNoErase*/)
, KexiRecordNavigatorHandler()
, KexiSharedActionClient()
, KexiDataAwareObjectInterface()
{
	KexiTableView::initCellEditorFactories();

	d = new KexiTableViewPrivate(this);

	connect( kapp, SIGNAL( settingsChanged(int) ), SLOT( slotSettingsChanged(int) ) );
	slotSettingsChanged(KApplication::SETTINGS_SHORTCUTS);

	m_data = new KexiTableViewData(); //to prevent crash because m_data==0
	m_owner = true;                   //-this will be deleted if needed

	setResizePolicy(Manual);
	viewport()->setBackgroundMode(NoBackground);
//	viewport()->setFocusPolicy(StrongFocus);
	viewport()->setFocusPolicy(WheelFocus);
	setFocusPolicy(WheelFocus); //<--- !!!!! important (was NoFocus), 
	//                             otherwise QApplication::setActiveWindow() won't activate 
	//                             this widget when needed!
//	setFocusProxy(viewport());
	viewport()->installEventFilter(this);

	//setup colors defaults
	setBackgroundMode(PaletteBackground);
//	setEmptyAreaColor(d->appearance.baseColor);//palette().active().color(QColorGroup::Base));

//	d->baseColor = colorGroup().base();
//	d->textColor = colorGroup().text();

//	d->altColor = KGlobalSettings::alternateBackgroundColor();
//	d->grayColor = QColor(200,200,200);
	d->diagonalGrayPattern = QBrush(d->appearance.borderColor, BDiagPattern);

	setLineWidth(1);
	horizontalScrollBar()->installEventFilter(this);
	horizontalScrollBar()->raise();
	verticalScrollBar()->raise();
	
	// setup scrollbar tooltip
	d->scrollBarTip = new QLabel("abc",0, "scrolltip",WStyle_Customize |WStyle_NoBorder|WX11BypassWM|WStyle_StaysOnTop|WStyle_Tool);
	d->scrollBarTip->setPalette(QToolTip::palette());
	d->scrollBarTip->setMargin(2);
	d->scrollBarTip->setIndent(0);
	d->scrollBarTip->setAlignment(AlignCenter);
	d->scrollBarTip->setFrameStyle( QFrame::Plain | QFrame::Box );
	d->scrollBarTip->setLineWidth(1);
	connect(verticalScrollBar(),SIGNAL(sliderReleased()),this,SLOT(vScrollBarSliderReleased()));
	connect(&d->scrollBarTipTimer,SIGNAL(timeout()),this,SLOT(scrollBarTipTimeout()));
	
	//context menu
	m_popup = new KPopupMenu(this, "contextMenu");
#if 0 //moved to mainwindow's actions
	d->menu_id_addRecord = m_popup->insertItem(i18n("Add Record"), this, SLOT(addRecord()), CTRL+Key_Insert);
	d->menu_id_removeRecord = m_popup->insertItem(
		kapp->iconLoader()->loadIcon("button_cancel", KIcon::Small),
		i18n("Remove Record"), this, SLOT(removeRecord()), CTRL+Key_Delete);
#endif

#ifdef Q_WS_WIN
	d->rowHeight = fontMetrics().lineSpacing() + 4;
#else
	d->rowHeight = fontMetrics().lineSpacing() + 1;
#endif

	if(d->rowHeight < 17)
		d->rowHeight = 17;

	d->pUpdateTimer = new QTimer(this);

//	setMargins(14, fontMetrics().height() + 4, 0, 0);

	// Create headers
	d->pTopHeader = new TableViewHeader(this, "topHeader");
	d->pTopHeader->setOrientation(Horizontal);
	d->pTopHeader->setTracking(false);
	d->pTopHeader->setMovingEnabled(false);
	connect(d->pTopHeader, SIGNAL(sizeChange(int,int,int)), this, SLOT(slotTopHeaderSizeChange(int,int,int)));

	m_verticalHeader = new KexiRecordMarker(this, "rm");
	m_verticalHeader->setCellHeight(d->rowHeight);
//	m_verticalHeader->setFixedWidth(d->rowHeight);
	m_verticalHeader->setCurrentRow(-1);

	setMargins(
		QMIN(d->pTopHeader->sizeHint().height(), d->rowHeight),
		d->pTopHeader->sizeHint().height(), 0, 0);

	setupNavigator();

//	setMinimumHeight(horizontalScrollBar()->height() + d->rowHeight + topMargin());

//	navPanelLyr->addStretch(25);
//	enableClipper(true);

	if (data)
		setData( data );

#if 0//(js) doesn't work!
	d->scrollTimer = new QTimer(this);
	connect(d->scrollTimer, SIGNAL(timeout()), this, SLOT(slotAutoScroll()));
#endif

//	setBackgroundAltering(true);
//	setFullRowSelectionEnabled(false);

	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);

	// Connect header, table and scrollbars
	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), d->pTopHeader, SLOT(setOffset(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)), m_verticalHeader, SLOT(setOffset(int)));
	connect(d->pTopHeader, SIGNAL(sizeChange(int, int, int)), this, SLOT(slotColumnWidthChanged(int, int, int)));
	connect(d->pTopHeader, SIGNAL(sectionHandleDoubleClicked(int)), this, SLOT(slotSectionHandleDoubleClicked(int)));
	connect(d->pTopHeader, SIGNAL(clicked(int)), this, SLOT(sortColumnInternal(int)));

	connect(d->pUpdateTimer, SIGNAL(timeout()), this, SLOT(slotUpdate()));
	
//	horizontalScrollBar()->show();
	updateScrollBars();
//	resize(sizeHint());
//	updateContents();
//	setMinimumHeight(horizontalScrollBar()->height() + d->rowHeight + topMargin());

//TMP
//setVerticalHeaderVisible(false);
//setHorizontalHeaderVisible(false);

//will be updated by setAppearance:	updateFonts();
	setAppearance(d->appearance); //refresh
}

KexiTableView::~KexiTableView()
{
	cancelRowEdit();

	if (m_owner)
		delete m_data;
	m_data = 0;
	delete d;
}

void KexiTableView::clearVariables()
{
	KexiDataAwareObjectInterface::clearVariables();
	d->clearVariables();
}

/*void KexiTableView::initActions(KActionCollection *ac)
{
	emit reloadActions(ac);
}*/

void KexiTableView::setupNavigator()
{
	updateScrollBars();
	
	m_navPanel = new KexiRecordNavigator(this, leftMargin(), "navPanel");
	m_navPanel->setRecordHandler(this);
	m_navPanel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
/*
	QHBoxLayout *navPanelLyr = new QHBoxLayout(m_navPanel,0,0,"nav_lyr");
//	navPanelLyr->setAutoAdd(true);

	navPanelLyr->addWidget( new QLabel(QString(" ")+i18n("Row:")+" ",m_navPanel) );
		
	int bw = 6+SmallIcon("navigator_first").width(); //QMIN( horizontalScrollBar()->height(), 20);
	QFont f = m_navPanel->font();
	f.setPixelSize((bw > 12) ? 12 : bw);
	QFontMetrics fm(f);
	d->nav1DigitWidth = fm.width("8");

	navPanelLyr->addWidget( d->navBtnFirst = new QToolButton(m_navPanel) );
	d->navBtnFirst->setFixedWidth(bw);
	d->navBtnFirst->setFocusPolicy(NoFocus);
	d->navBtnFirst->setIconSet( SmallIconSet("navigator_first") );
	QToolTip::add(d->navBtnFirst, i18n("First row"));
	
	navPanelLyr->addWidget( d->navBtnPrev = new QToolButton(m_navPanel) );
	d->navBtnPrev->setFixedWidth(bw);
	d->navBtnPrev->setFocusPolicy(NoFocus);
	d->navBtnPrev->setIconSet( SmallIconSet("navigator_prev") );
	QToolTip::add(d->navBtnPrev, i18n("Previous row"));
	
//	QWidget *spc = new QFrame(m_navPanel);
//	spc->setFixedWidth(6);
	navPanelLyr->addSpacing( 6 );
	
	navPanelLyr->addWidget( d->navRowNumber = new KLineEdit(m_navPanel) );
	d->navRowNumber->setAlignment(AlignRight | AlignVCenter);
	d->navRowNumber->setFocusPolicy(ClickFocus);
//	d->navRowNumber->setFixedWidth(fw);
	d->navRowNumberValidator = new QIntValidator(1, INT_MAX, this);
	d->navRowNumber->setValidator(d->navRowNumberValidator);
	d->navRowNumber->installEventFilter(this);
	QToolTip::add(d->navRowNumber, i18n("Current row number"));
	
	KLineEdit *lbl_of = new KLineEdit(i18n("of"), m_navPanel);
	lbl_of->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
	lbl_of->setMaximumWidth(fm.width(lbl_of->text())+8);
	lbl_of->setReadOnly(true);
	lbl_of->setLineWidth(0);
	lbl_of->setFocusPolicy(NoFocus);
	lbl_of->setAlignment(AlignCenter);
	navPanelLyr->addWidget( lbl_of );
	
	navPanelLyr->addWidget( d->navRowCount = new KLineEdit(m_navPanel) );
//	d->navRowCount->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
	d->navRowCount->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
//	d->navRowCount->setMaximumWidth(fw);
	d->navRowCount->setReadOnly(true);
	d->navRowCount->setLineWidth(0);
	d->navRowCount->setFocusPolicy(NoFocus);
	d->navRowCount->setAlignment(AlignLeft | AlignVCenter);

	lbl_of->setFont(f);
	d->navRowNumber->setFont(f);
	d->navRowCount->setFont(f);
	m_navPanel->setFont(f);

	navPanelLyr->addWidget( d->navBtnNext = new QToolButton(m_navPanel) );
	d->navBtnNext->setFixedWidth(bw);
	d->navBtnNext->setFocusPolicy(NoFocus);
	d->navBtnNext->setIconSet( SmallIconSet("navigator_next") );
	QToolTip::add(d->navBtnNext, i18n("Next row"));
	
	navPanelLyr->addWidget( d->navBtnLast = new QToolButton(m_navPanel) );
	d->navBtnLast->setFixedWidth(bw);
	d->navBtnLast->setFocusPolicy(NoFocus);
	d->navBtnLast->setIconSet( SmallIconSet("navigator_last") );
	QToolTip::add(d->navBtnLast, i18n("Last row"));
	
	navPanelLyr->addSpacing( 6 );
	navPanelLyr->addWidget( d->navBtnNew = new QToolButton(m_navPanel) );
	d->navBtnNew->setFixedWidth(bw);
	d->navBtnNew->setFocusPolicy(NoFocus);
	d->navBtnNew->setIconSet( SmallIconSet("navigator_new") );
	QToolTip::add(d->navBtnNew, i18n("New row"));
	d->navBtnNext->setEnabled(isInsertingEnabled());
	
	navPanelLyr->addSpacing( 6 );
	navPanelLyr->addStretch(10);
*/

/* moved to rec nav. handler

	connect(m_navPanel, SIGNAL(recordNumberEntered(uint)), 
		this, SLOT(slotNavRecordNumberEntered(uint)));

	connect(m_navPanel,SIGNAL(prevButtonClicked()),this,SLOT(navBtnPrevClicked()));
	connect(m_navPanel,SIGNAL(nextButtonClicked()),this,SLOT(navBtnNextClicked()));
	connect(m_navPanel,SIGNAL(lastButtonClicked()),this,SLOT(navBtnLastClicked()));
	connect(m_navPanel,SIGNAL(firstButtonClicked()),this,SLOT(navBtnFirstClicked()));
	connect(m_navPanel,SIGNAL(newButtonClicked()),this,SLOT(navBtnNewClicked()));
	connect(verticalScrollBar(),SIGNAL(valueChanged(int)),
		this,SLOT(vScrollBarValueChanged(int)));
*/
//	m_navPanel->updateGeometry(leftMargin());
}

#if 0
void KexiTableView::setNavRowNumber(int newrow)
{
	QString n;
	if (newrow>=0)
		n = QString::number(newrow+1);
	else
		n = " ";
//	if (d->navRowNumber->text().length() != n.length()) {//resize
//		d->navRowNumber->setFixedWidth(
//			d->nav1DigitWidth*QMAX( QMAX(n.length(),2)+1,d->navRowCount->text().length()+1)+6 
//		);
//	}
	d->navRowNumber->setText(n);
	d->navRowCount->deselect();
	d->navBtnPrev->setEnabled(newrow>0);
	d->navBtnFirst->setEnabled(newrow>0);
	d->navBtnNext->setEnabled(newrow<(rows()-1+(isInsertingEnabled()?1:0)));
	d->navBtnLast->setEnabled(newrow!=(rows()-1) && (isInsertingEnabled() || rows()>0));
}

void KexiTableView::setNavRowCount(int newrows)
{
	const QString & n = QString::number(newrows);
	if (d->navRowCount->text().length() != n.length()) {//resize
		d->navRowCount->setFixedWidth(d->nav1DigitWidth*n.length()+6);
		
		if (horizontalScrollBar()->isVisible()) {
			//+width of the delta
			m_navPanel->resize(
				m_navPanel->width()+(n.length()-d->navRowCount->text().length())*d->nav1DigitWidth,
				m_navPanel->height());
//			horizontalScrollBar()->move(m_navPanel->x()+m_navPanel->width()+20,horizontalScrollBar()->y());
		}
	}
	//update row number widget's width
	const int w = d->nav1DigitWidth*QMAX( QMAX(n.length(),2)+1,d->navRowNumber->text().length()+1)+6;
	if (d->navRowNumber->width()!=w) //resize
		d->navRowNumber->setFixedWidth(w);

	d->navRowCount->setText(n);
	d->navRowCount->deselect();

//	if (horizontalScrollBar()->isVisible()) {
//	}
//	updateNavPanelGeometry();
	updateScrollBars();
}
#endif

#if 0 //moved
void KexiTableView::setData( KexiTableViewData *data, bool owner )
{
	const bool theSameData = m_data && m_data==data;
	if (m_owner && m_data && m_data!=data/*don't destroy if it's the same*/) {
		kdDebug(44021) << "KexiTableView::setData(): destroying old data (owned)" << endl;
		delete m_data; //destroy old data
		m_data = 0;
	}
	m_owner = owner;
	if(!data) {
		m_data = new KexiTableViewData();
		m_owner = true;
	}
	else {
		m_data = data;
		m_owner = owner;
		kdDebug(44021) << "KexiTableView::setData(): using shared data" << endl;
		//add columns
//		d->pTopHeader->setUpdatesEnabled(false);
		while(d->pTopHeader->count()>0)
			d->pTopHeader->removeLabel(0);

		{
			int i=0;
			for (KexiTableViewColumn::ListIterator it(m_data->columns);
				it.current(); ++it, i++) 
			{
				KexiDB::Field *f = it.current()->field();
//				if (!it.current()->fieldinfo || it.current()->fieldinfo->visible) {
				if (it.current()->visible()) {
					int wid = f->width();
					if (wid==0)
						wid=KEXITV_DEFAULT_COLUMN_WIDTH;//default col width in pixels
//js: TODO - add col width configuration and storage
//					d->pTopHeader->addLabel(f->captionOrName(), wid);
					d->pTopHeader->addLabel(it.current()->captionAliasOrName(), wid);
					if (!f->description().isEmpty())
						d->pTopHeader->setToolTip( i, f->description() );
				}
			}
		}

//		d->pTopHeader->setUpdatesEnabled(true);
		//add rows
//		triggerUpdate();
		m_verticalHeader->clear();
		m_verticalHeader->addLabels(m_data->count());
		if (m_data->count()==0)
			m_navPanel->setCurrentRecordNumber(1);
//			setNavRowNumber(0);
	}
	
	if (!theSameData) {
//! @todo: store sorting?
		setSorting(-1);
//		connect(m_data, SIGNAL(refreshRequested()), this, SLOT(slotRefreshRequested()));
		connect(m_data, SIGNAL(reloadRequested()), this, SLOT(reloadData()));
		connect(m_data, SIGNAL(destroying()), this, SLOT(slotDataDestroying()));
		connect(m_data, SIGNAL(rowsDeleted( const QValueList<int> & )), 
			this, SLOT(slotRowsDeleted( const QValueList<int> & )));
		connect(m_data, SIGNAL(aboutToDeleteRow(KexiTableItem&,KexiDB::ResultInfo*,bool)),
			this, SLOT(slotAboutToDeleteRow(KexiTableItem&,KexiDB::ResultInfo*,bool)));
		connect(m_data, SIGNAL(rowDeleted()), this, SLOT(slotRowDeleted()));
		connect(m_data, SIGNAL(rowInserted(KexiTableItem*,bool)), 
			this, SLOT(slotRowInserted(KexiTableItem*,bool)));
		connect(m_data, SIGNAL(rowInserted(KexiTableItem*,uint,bool)), 
			this, SLOT(slotRowInserted(KexiTableItem*,uint,bool))); //not db-aware
		connect(m_data, SIGNAL(rowRepaintRequested(KexiTableItem&)), 
			this, SLOT(slotRowRepaintRequested(KexiTableItem&)));
	}

	if (!data) {
//		clearData();
		cancelRowEdit();
		m_data->clearInternal();
	}

	if (!m_insertItem) {//first setData() call - add 'insert' item
		m_insertItem = new KexiTableItem(m_data->columns.count());
	}
	else {//just reinit
		m_insertItem->init(m_data->columns.count());
	}

	//update gui mode
//	d->navBtnNew->setEnabled(isInsertingEnabled());
	m_navPanel->setInsertingEnabled(isInsertingEnabled());
	m_verticalHeader->showInsertRow(isInsertingEnabled());

	initDataContents();

	emit dataSet( m_data );

//	QSize s(tableSize());
//	resizeContents(s.width(),s.height());
}
#endif

void KexiTableView::initDataContents()
{
	updateWidgetContentsSize();

	KexiDataAwareObjectInterface::initDataContents();

/*moved
	if (!d->cursorPositionSetExplicityBeforeShow) {
		//set current row:
		m_currentItem = 0;
		int curRow = -1, curCol = -1;
		if (m_data->columnsCount()>0) {
			if (rows()>0) {
				m_currentItem = m_data->first();
				curRow = 0;
				curCol = 0;
			}
			else {//no data
				if (isInsertingEnabled()) {
					m_currentItem = m_insertItem;
					curRow = 0;
					curCol = 0;
				}
			}
		}

		setCursorPosition(curRow, curCol);
	}
	ensureVisible(m_curRow,m_curCol);
//	updateRowCountInfo();
//	setNavRowCount(rows());
	updateContents();

	d->cursorPositionSetExplicityBeforeShow = false;

	emit dataRefreshed();*/
}

void KexiTableView::addHeaderColumn(const QString& caption, const QString& description, int width)
{
	const int nr = d->pTopHeader->count();
	d->pTopHeader->addLabel(caption, width);
	if (!description.isEmpty())
		d->pTopHeader->setToolTip(nr, description);
}

void KexiTableView::updateWidgetContentsSize()
{
	QSize s(tableSize());
	resizeContents(s.width(), s.height());
}

/* moved
void KexiTableView::slotDataDestroying()
{
	m_data = 0;
}

*/
void KexiTableView::slotRowsDeleted( const QValueList<int> &rows )
{
	viewport()->repaint();
	updateWidgetContentsSize();
	setCursorPosition(QMAX(0, (int)m_curRow - (int)rows.count()), -1, true);
}


/*void KexiTableView::addDropFilter(const QString &filter)
{
	d->dropFilters.append(filter);
	viewport()->setAcceptDrops(true);
}*/

void KexiTableView::setFont( const QFont &font )
{
	QScrollView::setFont(font);
	updateFonts(true);
}

void KexiTableView::updateFonts(bool repaint)
{
#ifdef Q_WS_WIN
	d->rowHeight = fontMetrics().lineSpacing() + 4;
#else
	d->rowHeight = fontMetrics().lineSpacing() + 1;
#endif
	if (d->appearance.fullRowSelection) {
		d->rowHeight -= 1;
	}
	if(d->rowHeight < 17)
		d->rowHeight = 17;
//	if(d->rowHeight < 22)
//		d->rowHeight = 22;
	setMargins(
		QMIN(d->pTopHeader->sizeHint().height(), d->rowHeight),
		d->pTopHeader->sizeHint().height(), 0, 0);
//	setMargins(14, d->rowHeight, 0, 0);
	m_verticalHeader->setCellHeight(d->rowHeight);

	QFont f = font();
	f.setItalic(true);
	d->autonumberFont = f;

	QFontMetrics fm(d->autonumberFont);
	d->autonumberTextWidth = fm.width(i18n("(autonumber)"));

	if (repaint)
		updateContents();
}

#if 0 //moved
bool KexiTableView::beforeDeleteItem(KexiTableItem *)
{
	//always return
	return true;
}

bool KexiTableView::deleteItem(KexiTableItem *item)/*, bool moveCursor)*/
{
	if (!item || !beforeDeleteItem(item))
		return false;

	QString msg, desc;
//	bool current = (item == m_currentItem);
	if (!m_data->deleteRow(*item, true /*repaint*/)) {
		//error
		if (m_data->result()->desc.isEmpty())
			KMessageBox::sorry(this, m_data->result()->msg);
		else
			KMessageBox::detailedSorry(this, m_data->result()->msg, m_data->result()->desc);
		return false;
	}
	else {
//setCursorPosition() wil lset this!		if (current)
			//m_currentItem = m_data->current();
	}

//	repaintAfterDelete();
	if (d->spreadSheetMode) { //append empty row for spreadsheet mode
			m_data->append(new KexiTableItem(m_data->columns.count()));
			m_verticalHeader->addLabels(1);
	}

	return true;
}
/*
void KexiTableView::repaintAfterDelete()
{
	int row = m_curRow;
	if (!isInsertingEnabled() && row>=rows())
		row--; //move up

	QSize s(tableSize());
	resizeContents(s.width(),s.height());

	setCursorPosition(row, m_curCol, true);

	m_verticalHeader->removeLabel();
//		if(moveCursor)
//		selectPrev();
//		d->pUpdateTimer->start(1,true);
	//get last visible row
	int r = rowAt(clipper()->height());
	if (r==-1) {
		r = rows()+1+(isInsertingEnabled()?1:0);
	}
	//update all visible rows below 
	updateContents( contentsX(), rowPos(m_curRow), clipper()->width(), d->rowHeight*(r-m_curRow));

	//update navigator's data
	setNavRowCount(rows());
}*/

void KexiTableView::deleteCurrentRow()
{
	if (m_newRowEditing) {//we're editing fresh new row: just cancel this!
		cancelRowEdit();
		return;
	}

	if (!acceptRowEdit())
		return;

	if (!isDeleteEnabled() || !m_currentItem || m_currentItem == m_insertItem)
		return;
	switch (d->deletionPolicy) {
	case NoDelete:
		return;
	case ImmediateDelete:
		break;
	case AskDelete:
		if (KMessageBox::Cancel == KMessageBox::warningContinueCancel(this, i18n("Do you want to delete selected row?"), 0, 
			KGuiItem(i18n("&Delete Row"),"editdelete"), KStdGuiItem::no(), "dontAskBeforeDeleteRow"/*config entry*/))
			return;
		break;
	case SignalDelete:
		emit itemDeleteRequest(m_currentItem, m_curRow, m_curCol);
		emit currentItemDeleteRequest();
		return;
	default:
		return;
	}

	if (!deleteItem(m_currentItem)) {//nothing
	}
}
#endif //moved

#if 0 //moved
void KexiTableView::slotAboutToDeleteRow(KexiTableItem& item, 
	KexiDB::ResultInfo* /*result*/, bool repaint)
{
	if (repaint) {
		d->rowWillBeDeleted = m_data->findRef(&item);
	}
}

void KexiTableView::slotRowDeleted()
{
	if (d->rowWillBeDeleted >= 0) {
		if (d->rowWillBeDeleted > 0 && d->rowWillBeDeleted >= rows())
			d->rowWillBeDeleted--; //move up
		updateWidgetContentsSize();

		setCursorPosition(d->rowWillBeDeleted, m_curCol, true/*forceSet*/);
		m_verticalHeader->removeLabel();

		//get last visible row
		int r = rowAt(clipper()->height()+contentsY());
		if (r==-1) {
			r = rows()+1+(isInsertingEnabled()?1:0);
		}
		//update all visible rows below 
		int leftcol = d->pTopHeader->sectionAt( d->pTopHeader->offset() );
		int row = m_curRow;
		updateContents( columnPos( leftcol ), rowPos(row), 
			clipper()->width(), clipper()->height() - (rowPos(row) - contentsY()) );

		//update navigator's data
		m_navPanel->setRecordCount(rows());

		d->rowWillBeDeleted = -1;
	}
}
#endif

void KexiTableView::updateAllVisibleRowsBelow(int row)
{
	//get last visible row
	int r = rowAt(clipper()->height()+contentsY());
	if (r==-1) {
		r = rows()+1+(isInsertingEnabled()?1:0);
	}
	//update all visible rows below 
	int leftcol = d->pTopHeader->sectionAt( d->pTopHeader->offset() );
//	int row = m_curRow;
	updateContents( columnPos( leftcol ), rowPos(row), 
		clipper()->width(), clipper()->height() - (rowPos(row) - contentsY()) );
}

void KexiTableView::slotRowInserted(KexiTableItem *item, bool repaint)
{
	int row = m_data->findRef(item);
	slotRowInserted( item, row, repaint );
}

void KexiTableView::slotRowInserted(KexiTableItem * /*item*/, uint row, bool repaint)
{
	if (repaint && (int)row<rows()) {
		updateWidgetContentsSize();

		//redraw only this row and below:
		int leftcol = d->pTopHeader->sectionAt( d->pTopHeader->offset() );
	//	updateContents( columnPos( leftcol ), rowPos(m_curRow), 
	//		clipper()->width(), clipper()->height() - (rowPos(m_curRow) - contentsY()) );
		updateContents( columnPos( leftcol ), rowPos(row), 
			clipper()->width(), clipper()->height() - (rowPos(row) - contentsY()) );

		if (!d->verticalHeaderAlreadyAdded)
			m_verticalHeader->addLabel();
		else //it was added because this inserting was interactive
			d->verticalHeaderAlreadyAdded = false;

		//update navigator's data
		m_navPanel->setRecordCount(rows());

		if (m_curRow >= (int)row) {
			//update
			editorShowFocus( m_curRow, m_curCol );
		}
	}
}

#if 0 //moved
KexiTableItem *KexiTableView::insertEmptyRow(int row)
{
	if ( !acceptRowEdit() || !isEmptyRowInsertingEnabled() 
		|| (row!=-1 && row >= (rows()+isInsertingEnabled()?1:0) ) )
		return 0;

	KexiTableItem *newItem = new KexiTableItem(m_data->columns.count());
	insertItem(newItem, row);
	return newItem;
}

void KexiTableView::insertItem(KexiTableItem *newItem, int row)
{
	bool changeCurrent = (row==-1 || row==m_curRow);
	if (changeCurrent) {
		row = (m_curRow >= 0 ? m_curRow : 0);
		m_currentItem = newItem;
		m_curRow = row;
	}
	else if (m_curRow >= row) {
		m_curRow++;
	}
	m_data->insertRow(*newItem, row, true /*repaint*/);

/*
	QSize s(tableSize());
	resizeContents(s.width(),s.height());

	//redraw only this row and below:
	int leftcol = d->pTopHeader->sectionAt( d->pTopHeader->offset() );
//	updateContents( columnPos( leftcol ), rowPos(m_curRow), 
//		clipper()->width(), clipper()->height() - (rowPos(m_curRow) - contentsY()) );
	updateContents( columnPos( leftcol ), rowPos(row), 
		clipper()->width(), clipper()->height() - (rowPos(row) - contentsY()) );

	m_verticalHeader->addLabel();

	//update navigator's data
	setNavRowCount(rows());

	if (m_curRow >= row) {
		//update
		editorShowFocus( m_curRow, m_curCol );
	}
	*/
}

tristate KexiTableView::deleteAllRows(bool ask, bool repaint)
{
	if (!hasData())
		return true;
	if (m_data->count()<1)
		return true;

	if (ask) {
		QString tableName = m_data->dbTableName();
		if (!tableName.isEmpty()) {
			tableName.prepend(" \"");
			tableName.append("\"");
		}
		if (KMessageBox::Cancel == KMessageBox::warningContinueCancel(this, 
				i18n("Do you want to clear the contents of table %1?").arg(tableName),
				0, KGuiItem(i18n("&Clear Contents")), KStdGuiItem::no()))
			return cancelled;
	}

	cancelRowEdit();
//	acceptRowEdit();
//	m_verticalHeader->clear();
	const bool repaintLater = repaint && d->spreadSheetMode;
	const int oldRows = rows();

	bool res = m_data->deleteAllRows(repaint && !repaintLater);

	if (res) {
		if (d->spreadSheetMode) {
			const uint columns = m_data->columns.count();
			for (int i=0; i<oldRows; i++) {
				m_data->append(new KexiTableItem(columns));
			}
		}
	}
	if (repaintLater)
		m_data->reload();

//	d->clearVariables();
//	m_verticalHeader->setCurrentRow(-1);

//	d->pUpdateTimer->start(1,true);
//	if (repaint)
//		viewport()->repaint();
	return res;
}

void KexiTableView::clearColumns(bool repaint)
{
	cancelRowEdit();
	m_data->clearInternal();

	while(d->pTopHeader->count()>0)
		d->pTopHeader->removeLabel(0);

	if (repaint)
		viewport()->repaint();

/*	for(int i=0; i < rows(); i++)
	{
		m_verticalHeader->removeLabel();
	}

	editorCancel();
	m_contents->clear();

	d->clearVariables();
	d->numCols = 0;

	while(d->pTopHeader->count()>0)
		d->pTopHeader->removeLabel(0);

	m_verticalHeader->setCurrentRow(-1);

	viewport()->repaint();

//	d->pColumnTypes.resize(0);
//	d->pColumnModes.resize(0);
//	d->pColumnDefaults.clear();*/
}

void KexiTableView::slotRefreshRequested()
{
//	cancelRowEdit();
	acceptRowEdit();
	m_verticalHeader->clear();

	if (m_curCol>=0 && m_curCol<columns()) {
		//find the editor for this column
		KexiTableEdit *edit = editor( m_curCol );
		if (edit) {
			edit->hideFocus();
		}
	}
//	setCursorPosition(-1, -1, true);
	clearVariables();
	m_verticalHeader->setCurrentRow(-1);
	if (isVisible())
		initDataContents();
	else
		m_initDataContentsOnShow = true;
	m_verticalHeader->addLabels(m_data->count());

	updateScrollBars();
}
#endif //moved

void KexiTableView::clearColumnsInternal(bool /*repaint*/)
{
	while(d->pTopHeader->count()>0)
		d->pTopHeader->removeLabel(0);
}

#if 0 //todo
int KexiTableView::findString(const QString &string)
{
	int row = 0;
	int col = sorting();
	if(col == -1)
		return -1;
	if(string.isEmpty())
	{
		setCursorPosition(0, col);
		return 0;
	}

	QPtrListIterator<KexiTableItem> it(*m_contents);

	if(string.at(0) != QChar('*'))
	{
		switch(columnType(col))
		{
			case QVariant::String:
			{
				QString str2 = string.lower();
				for(; it.current(); ++it)
				{
					if(it.current()->at(col).toString().left(string.length()).lower().compare(str2)==0)
					{
						center(columnPos(col), rowPos(row));
						setCursorPosition(row, col);
						return row;
					}
					row++;
				}
				break;
			}
			case QVariant::Int:
			case QVariant::Bool:
				for(; it.current(); ++it)
				{
					if(QString::number(it.current()->at(col).toInt()).left(string.length()).compare(string)==0)
					{
						center(columnPos(col), rowPos(row));
						setCursorPosition(row, col);
						return row;
					}
					row++;
				}
				break;

			default:
				break;
		}
	}
	else
	{
		QString str2 = string.mid(1);
		switch(columnType(col))
		{
			case QVariant::String:
				for(; it.current(); ++it)
				{
					if(it.current()->at(col).toString().find(str2,0,false) >= 0)
					{
						center(columnPos(col), rowPos(row));
						setCursorPosition(row, col);
						return row;
					}
					row++;
				}
				break;
			case QVariant::Int:
			case QVariant::Bool:
				for(; it.current(); ++it)
				{
					if(QString::number(it.current()->at(col).toInt()).find(str2,0,true) >= 0)
					{
						center(columnPos(col), rowPos(row));
						setCursorPosition(row, col);
						return row;
					}
					row++;
				}
				break;

			default:
				break;
		}
	}
	return -1;
}
#endif

void KexiTableView::slotUpdate()
{
//	kdDebug(44021) << " KexiTableView::slotUpdate() -- " << endl;
//	QSize s(tableSize());
//	viewport()->setUpdatesEnabled(false);
///	resizeContents(s.width(), s.height());
//	viewport()->setUpdatesEnabled(true);

	updateContents();
	updateScrollBars();
	if (m_navPanel)
		m_navPanel->updateGeometry(leftMargin());
//	updateNavPanelGeometry();

	updateWidgetContentsSize();
//	updateContents(0, contentsY()+clipper()->height()-2*d->rowHeight, clipper()->width(), d->rowHeight*3);
	
	//updateGeometries();
//	updateContents(0, 0, viewport()->width(), contentsHeight());
//	updateGeometries();
}

/*
moved
bool KexiTableView::isSortingEnabled() const
{
	return d->isSortingEnabled;
}

void KexiTableView::setSortingEnabled(bool set)
{
	if (d->isSortingEnabled && !set)
		setSorting(-1);
	d->isSortingEnabled = set;
	emit reloadActions();
}

int KexiTableView::sortedColumn()
{
	if (m_data && d->isSortingEnabled)
		return m_data->sortedColumn();
	return -1;
}

bool KexiTableView::sortingAscending() const
{ 
	return m_data && m_data->sortingAscending();
}

void KexiTableView::setSorting(int col, bool ascending)
{
	if (!m_data || !d->isSortingEnabled)
		return;
	d->pTopHeader->setSortIndicator(col, ascending ? Ascending : Descending);
	m_data->setSorting(col, ascending);
}

bool KexiTableView::sort()
{
	if (!m_data || !d->isSortingEnabled)
		return false;

	if (rows() < 2)
		return true;

	if (!acceptRowEdit())
		return false;
			
	if (m_data->sortedColumn()!=-1)
		m_data->sort();

	//locate current record
	if (!m_currentItem) {
		m_currentItem = m_data->first();
		m_curRow = 0;
		if (!m_currentItem)
			return true;
	}
	if (m_currentItem != m_insertItem) {
		m_curRow = m_data->findRef(m_currentItem);
	}

//	m_currentItem = m_data->at(m_curRow);

	int cw = columnWidth(m_curCol);
	int rh = rowHeight();

//	m_verticalHeader->setCurrentRow(m_curRow);
	center(columnPos(m_curCol) + cw / 2, rowPos(m_curRow) + rh / 2);
//	updateCell(oldRow, m_curCol);
//	updateCell(m_curRow, m_curCol);
	m_verticalHeader->setCurrentRow(m_curRow);
//	slotUpdate();

	updateContents();
//	d->pUpdateTimer->start(1,true);
	return true;
}

void KexiTableView::sortAscending()
{
	if (currentColumn()<0)
		return;
	sortColumnInternal( currentColumn(), 1 );
}

void KexiTableView::sortDescending()
{
	if (currentColumn()<0)
		return;
	sortColumnInternal( currentColumn(), -1 );
}

void KexiTableView::sortColumnInternal(int col, int order)
{
	//-select sorting 
	bool asc;
	if (order == 0) {// invert
		if (col==sortedColumn())
			asc = !sortingAscending(); //inverse sorting for this column
		else
			asc = true;
	}
	else
		asc = (order==1);
	
	const QHeader::SortOrder prevSortOrder = d->pTopHeader->sortIndicatorOrder();
	const int prevSortColumn = d->pTopHeader->sortIndicatorSection();
	setSorting( col, asc );
	//-perform sorting 
	if (!sort())
		d->pTopHeader->setSortIndicator(prevSortColumn, prevSortOrder);
	
	if (col != prevSortColumn)
		emit sortedColumnChanged(col);
}
*/

int KexiTableView::currentLocalSortingOrder() const
{
	if (d->pTopHeader->sortIndicatorSection()==-1)
		return 0;
	return (d->pTopHeader->sortIndicatorOrder() == Qt::Ascending) ? 1 : -1;
}

void KexiTableView::setLocalSortingOrder(int col, int order)
{
	if (order == 0)
		col = -1;
	if (col>=0)
		d->pTopHeader->setSortIndicator(col, (order==1) ? Qt::Ascending : Qt::Descending);
}

int KexiTableView::currentLocalSortColumn() const
{
	return d->pTopHeader->sortIndicatorSection();
}

void KexiTableView::updateGUIAfterSorting()
{
	int cw = columnWidth(m_curCol);
	int rh = rowHeight();

//	m_verticalHeader->setCurrentRow(m_curRow);
	center(columnPos(m_curCol) + cw / 2, rowPos(m_curRow) + rh / 2);
//	updateCell(oldRow, m_curCol);
//	updateCell(m_curRow, m_curCol);
//	slotUpdate();

	updateContents();
//	d->pUpdateTimer->start(1,true);
}

QSizePolicy KexiTableView::sizePolicy() const
{
	// this widget is expandable
	return QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize KexiTableView::sizeHint() const
{
	const QSize &ts = tableSize();
	int w = QMAX( ts.width() + leftMargin()+ verticalScrollBar()->sizeHint().width() + 2*2, 
		(m_navPanel->isVisible() ? m_navPanel->width() : 0) );
	int h = QMAX( ts.height()+topMargin()+horizontalScrollBar()->sizeHint().height(), 
		minimumSizeHint().height() );
	w = QMIN( w, qApp->desktop()->width()*3/4 ); //stretch
	h = QMIN( h, qApp->desktop()->height()*3/4 ); //stretch

//	kdDebug() << "KexiTableView::sizeHint()= " <<w <<", " <<h << endl;

	return QSize(w, h);
		/*QSize(
		QMAX( ts.width() + leftMargin() + 2*2, (m_navPanel ? m_navPanel->width() : 0) ),
		//+ QMIN(m_verticalHeader->width(),d->rowHeight) + margin()*2,
		QMAX( ts.height()+topMargin()+horizontalScrollBar()->sizeHint().height(), 
			minimumSizeHint().height() )
	);*/
//		QMAX(ts.height() + topMargin(), minimumSizeHint().height()) );
}

QSize KexiTableView::minimumSizeHint() const
{
	return QSize(
		leftMargin() + ((columns()>0)?columnWidth(0):KEXI_DEFAULT_DATA_COLUMN_WIDTH) + 2*2, 
		d->rowHeight*5/2 + topMargin() + (m_navPanel && m_navPanel->isVisible() ? m_navPanel->height() : 0)
	);
}

void KexiTableView::createBuffer(int width, int height)
{
	if(!d->pBufferPm)
		d->pBufferPm = new QPixmap(width, height);
	else
		if(d->pBufferPm->width() < width || d->pBufferPm->height() < height)
			d->pBufferPm->resize(width, height);
//	d->pBufferPm->fill();
}

//internal
inline void KexiTableView::paintRow(KexiTableItem *item,
	QPainter *pb, int r, int rowp, int cx, int cy, 
	int colfirst, int collast, int maxwc)
{
	if (!item)
		return;
	// Go through the columns in the row r
	// if we know from where to where, go through [colfirst, collast],
	// else go through all of them
	if (colfirst==-1)
		colfirst=0;
	if (collast==-1)
		collast=columns()-1;

	int transly = rowp-cy;

	if (d->appearance.rowHighlightingEnabled && r == d->highlightedRow)
		pb->fillRect(0, transly, maxwc, d->rowHeight, d->appearance.rowHighlightingColor);
	else if(d->appearance.backgroundAltering && (r%2 != 0))
		pb->fillRect(0, transly, maxwc, d->rowHeight, d->appearance.alternateBackgroundColor);
	else
		pb->fillRect(0, transly, maxwc, d->rowHeight, d->appearance.baseColor);

	for(int c = colfirst; c <= collast; c++)
	{
		// get position and width of column c
		int colp = columnPos(c);
		if (colp==-1)
			continue; //invisible column?
		int colw = columnWidth(c);
		int translx = colp-cx;

		// Translate painter and draw the cell
		pb->saveWorldMatrix();
		pb->translate(translx, transly);
			paintCell( pb, item, c, r, QRect(colp, rowp, colw, d->rowHeight));
		pb->restoreWorldMatrix();
	}

	if (m_dragIndicatorLine>=0) {
		int y_line = -1;
		if (r==(rows()-1) && m_dragIndicatorLine==rows()) {
			y_line = transly+d->rowHeight-3; //draw at last line
		}
		if (m_dragIndicatorLine==r) {
			y_line = transly+1;
		}
		if (y_line>=0) {
			RasterOp op = pb->rasterOp();
			pb->setRasterOp(XorROP);
			pb->setPen( QPen(white, 3) );
			pb->drawLine(0, y_line, maxwc, y_line);
			pb->setRasterOp(op);
		}
	}
}

void KexiTableView::drawContents( QPainter *p, int cx, int cy, int cw, int ch)
{
	if (d->disableDrawContents)
		return;
	int colfirst = columnAt(cx);
	int rowfirst = rowAt(cy);
	int collast = columnAt(cx + cw-1);
	int rowlast = rowAt(cy + ch-1);
	bool inserting = isInsertingEnabled();
	bool plus1row = false; //true if we should show 'inserting' row at the end
	bool paintOnlyInsertRow = false;

/*	kdDebug(44021) << QString(" KexiTableView::drawContents(cx:%1 cy:%2 cw:%3 ch:%4)")
			.arg(cx).arg(cy).arg(cw).arg(ch) << endl;*/

	if (rowlast == -1) {
		rowlast = rows() - 1;
		plus1row = inserting;
		if (rowfirst == -1) {
			if (rowAt(cy - d->rowHeight) != -1) {
				paintOnlyInsertRow = true;
//				kdDebug(44021) << "-- paintOnlyInsertRow --" << endl;
			}
		}
	}
//	kdDebug(44021) << "rowfirst="<<rowfirst<<" rowlast="<<rowlast<<" rows()="<<rows()<<endl;
//	kdDebug(44021)<<" plus1row=" << plus1row<<endl;
	
	if ( collast == -1 )
		collast = columns() - 1;

	if (colfirst>collast) {
		int tmp = colfirst;
		colfirst = collast;
		collast = tmp;
	}
	if (rowfirst>rowlast) {
		int tmp = rowfirst;
		rowfirst = rowlast;
		rowlast = tmp;
	}

// 	qDebug("cx:%3d cy:%3d w:%3d h:%3d col:%2d..%2d row:%2d..%2d tsize:%4d,%4d", 
//	cx, cy, cw, ch, colfirst, collast, rowfirst, rowlast, tableSize().width(), tableSize().height());
//	triggerUpdate();

	if (rowfirst == -1 || colfirst == -1) {
		if (!paintOnlyInsertRow && !plus1row) {
			paintEmptyArea(p, cx, cy, cw, ch);
			return;
		}
	}

	createBuffer(cw, ch);
	if(d->pBufferPm->isNull())
		return;
	QPainter *pb = new QPainter(d->pBufferPm, this);
//	pb->fillRect(0, 0, cw, ch, colorGroup().base());

//	int maxwc = QMIN(cw, (columnPos(d->numCols - 1) + columnWidth(d->numCols - 1)));
	int maxwc = columnPos(columns() - 1) + columnWidth(columns() - 1);
//	kdDebug(44021) << "KexiTableView::drawContents(): maxwc: " << maxwc << endl;

	pb->fillRect(cx, cy, cw, ch, d->appearance.baseColor);

	int rowp;
	int r;
	if (paintOnlyInsertRow) {
		r = rows();
		rowp = rowPos(r); // 'insert' row's position
	}
	else {
		QPtrListIterator<KexiTableItem> it = m_data->iterator();
		it += rowfirst;//move to 1st row
		rowp = rowPos(rowfirst); // row position 
		for (r = rowfirst;r <= rowlast; r++, ++it, rowp+=d->rowHeight) {
			paintRow(it.current(), pb, r, rowp, cx, cy, colfirst, collast, maxwc);
		}
	}

	if (plus1row) { //additional - 'insert' row
		paintRow(m_insertItem, pb, r, rowp, cx, cy, colfirst, collast, maxwc);
	}

	delete pb;

	p->drawPixmap(cx,cy,*d->pBufferPm, 0,0,cw,ch);

  //(js)
	paintEmptyArea(p, cx, cy, cw, ch);
}

void KexiTableView::paintCell(QPainter* p, KexiTableItem *item, int col, int row, const QRect &cr, bool print)
{
	p->save();
//	kdDebug() <<"KexiTableView::paintCell(col=" << col <<"row="<<row<<")"<<endl;
	Q_UNUSED(print);
	int w = cr.width();
	int h = cr.height();
	int x2 = w - 1;
	int y2 = h - 1;

	//	Draw our lines
	QPen pen(p->pen());

	if (!d->appearance.fullRowSelection) {
		p->setPen(d->appearance.borderColor);
		p->drawLine( x2, 0, x2, y2 );	// right
		p->drawLine( 0, y2, x2, y2 );	// bottom
	}
	p->setPen(pen);

	if (m_editor && row == m_curRow && col == m_curCol //don't paint contents of edited cell
		&& m_editor->hasFocusableWidget() //..if it's visible
	   ) {
		p->restore();
		return;
	}

	KexiTableEdit *edit = dynamic_cast<KexiTableEdit*>( editor( col, /*ignoreMissingEditor=*/true ) );
//	if (!edit)
//		return;

/*
#ifdef Q_WS_WIN
	int x = 1;
	int y_offset = -1;
#else
	int x = 1;
	int y_offset = 0;
#endif

//	const int ctype = columnType(col);*/
//	int x=1;
	int x = edit ? edit->leftMargin() : 0;
	int y_offset=0;

	int align = SingleLine | AlignVCenter;
	QString txt; //text to draw

	QVariant cell_value;
	if ((uint)col < item->count()) {
		if (m_currentItem == item) {
			if (m_editor && row == m_curRow && col == m_curCol 
				&& !m_editor->hasFocusableWidget())
			{
				//we're over editing cell and the editor has no widget
				// - we're displaying internal values, not buffered
//				bool ok;
				cell_value = m_editor->value();
			}
			else {
				//we're displaying values from edit buffer, if available
				cell_value = *bufferedValueAt(col);
			}
		}
		else {
			cell_value = item->at(col);
		}
	}

	if (edit)
		edit->setupContents( p, m_currentItem == item && col == m_curCol, 
			cell_value, txt, align, x, y_offset, w, h );

	if (d->appearance.fullRowSelection)
		y_offset++; //correction because we're not drawing cell borders

/*
	if (KexiDB::Field::isFPNumericType( ctype )) {
#ifdef Q_WS_WIN
#else
			x = 0;
#endif
//js TODO: ADD OPTION to desplaying NULL VALUES as e.g. "(null)"
		if (!cell_value.isNull())
			txt = KGlobal::locale()->formatNumber(cell_value.toDouble());
		w -= 6;
		align |= AlignRight;
	}
	else if (ctype == KexiDB::Field::Enum)
	{
		txt = m_data->column(col)->field->enumHints().at(cell_value.toInt());
		align |= AlignLeft;
	}
	else if (KexiDB::Field::isIntegerType( ctype )) {
		int num = cell_value.toInt();
#ifdef Q_WS_WIN
		x = 1;
#else
		x = 0;
#endif
		w -= 6;
		align |= AlignRight;
		if (!cell_value.isNull())
			txt = QString::number(num);
	}
	else if (ctype == KexiDB::Field::Boolean) {
		int s = QMAX(h - 5, 12);
		QRect r(w/2 - s/2 + x, h/2 - s/2 - 1, s, s);
		p->setPen(QPen(colorGroup().text(), 1));
		p->drawRect(r);
		if (cell_value.asBool())
		{
			p->drawLine(r.x() + 2, r.y() + 2, r.right() - 1, r.bottom() - 1);
			p->drawLine(r.x() + 2, r.bottom() - 2, r.right() - 1, r.y() + 1);
		}
	}
	else if (ctype == KexiDB::Field::Date) { //todo: datetime & time
#ifdef Q_WS_WIN
		x = 5;
#else
		x = 5;
#endif
		if(cell_value.toDate().isValid())
		{
#ifdef USE_KDE
			txt = KGlobal::locale()->formatDate(cell_value.toDate(), true);
#else
			if (!cell_value.isNull())
				txt = cell_value.toDate().toString(Qt::LocalDate);
#endif
		}
		align |= AlignLeft;
	}
	else {//default:
#ifdef Q_WS_WIN
		x = 5;
//		y_offset = -1;
#else
		x = 5;
//		y_offset = 0;
#endif
		if (!cell_value.isNull())
			txt = cell_value.toString();
		align |= AlignLeft;
	}*/
	
	// draw selection background
//	const bool has_focus = hasFocus() || viewport()->hasFocus() || m_popup->hasFocus();

	const bool columnReadOnly = m_data->column(col)->readOnly();

	if (m_currentItem == item && col == m_curCol) {
/*		edit->paintSelectionBackground( p, isEnabled(), txt, align, x, y_offset, w, h,
			has_focus ? colorGroup().highlight() : gray,
			columnReadOnly, d->fullRowSelectionEnabled );*/
		if (edit)
			edit->paintSelectionBackground( p, isEnabled(), txt, align, x, y_offset, w, h,
				isEnabled() ? colorGroup().highlight() : QColor(200,200,200),//d->grayColor,
				columnReadOnly, d->appearance.fullRowSelection );
	}

/*
	if (!txt.isEmpty() && m_currentItem == item 
		&& col == m_curCol && !columnReadOnly) //js: && !d->recordIndicator)
	{
		QRect bound=fontMetrics().boundingRect(x, y_offset, w - (x+x), h, align, txt);
		bound.setX(bound.x()-1);
		bound.setY(0);
		bound.setWidth( QMIN( bound.width()+2, w - (x+x)+1 ) );
		bound.setHeight(d->rowHeight-1);
		if (has_focus)
			p->fillRect(bound, colorGroup().highlight());
		else
			p->fillRect(bound, gray);
	}
*/	
	if (!edit){
		p->fillRect(0, 0, x2, y2, d->diagonalGrayPattern);
	}

//	If we are in the focus cell, draw indication
	if(m_currentItem == item && col == m_curCol //js: && !d->recordIndicator)
		&& !d->appearance.fullRowSelection) 
	{
//		kdDebug() << ">>> CURRENT CELL ("<<m_curCol<<"," << m_curRow<<") focus="<<has_focus<<endl;
//		if (has_focus) {
		if (isEnabled()) {
			p->setPen(d->appearance.textColor);
		}
		else {
			QPen gray_pen(p->pen());
			gray_pen.setColor(d->appearance.borderColor);
			p->setPen(gray_pen);
		}
		if (edit)
			edit->paintFocusBorders( p, cell_value, 0, 0, x2, y2 );
		else
			p->drawRect(0, 0, x2, y2);
	}

	bool autonumber = false;
	if ((!m_newRowEditing &&item == m_insertItem) 
		|| (m_newRowEditing && item == m_currentItem && cell_value.isNull())) {
		//we're in "insert row"
		if (m_data->column(col)->field()->isAutoIncrement()) {
			//"autonumber" column
			txt = i18n("(autonumber)");
			autonumber = true;
		}
	}

	// draw text
	if (!txt.isEmpty()) {
		if (autonumber) {
			p->setPen(blue);
			p->setFont(d->autonumberFont);
			p->drawPixmap( w - x - x - 9 - d->autonumberTextWidth - d->autonumberIcon.width(),
				(h-d->autonumberIcon.height())/2, d->autonumberIcon );
		}
		else if (m_currentItem == item && col == m_curCol && !columnReadOnly)
			p->setPen(colorGroup().highlightedText());
		else if (d->appearance.rowHighlightingEnabled && row == d->highlightedRow)
			p->setPen(d->appearance.rowHighlightingTextColor);
		else
			p->setPen(d->appearance.textColor);
		p->drawText(x, y_offset, w - (x + x)- ((align & AlignLeft)?2:0)/*right space*/, h, align, txt);
	}
	p->restore();
}

QPoint KexiTableView::contentsToViewport2( const QPoint &p )
{
	return QPoint( p.x() - contentsX(), p.y() - contentsY() );
}

void KexiTableView::contentsToViewport2( int x, int y, int& vx, int& vy )
{
	const QPoint v = contentsToViewport2( QPoint( x, y ) );
	vx = v.x();
	vy = v.y();
}

QPoint KexiTableView::viewportToContents2( const QPoint& vp )
{
	return QPoint( vp.x() + contentsX(),
		   vp.y() + contentsY() );
}

void KexiTableView::paintEmptyArea( QPainter *p, int cx, int cy, int cw, int ch )
{
//  qDebug("%s: paintEmptyArea(x:%d y:%d w:%d h:%d)", (const char*)parentWidget()->caption(),cx,cy,cw,ch);

	// Regions work with shorts, so avoid an overflow and adjust the
	// table size to the visible size
	QSize ts( tableSize() );
//	ts.setWidth( QMIN( ts.width(), visibleWidth() ) );
//	ts.setHeight( QMIN( ts.height() - (m_navPanel ? m_navPanel->height() : 0), visibleHeight()) );
/*	kdDebug(44021) << QString(" (cx:%1 cy:%2 cw:%3 ch:%4)")
			.arg(cx).arg(cy).arg(cw).arg(ch) << endl;
	kdDebug(44021) << QString(" (w:%3 h:%4)")
			.arg(ts.width()).arg(ts.height()) << endl;*/
	
	// Region of the rect we should draw, calculated in viewport
	// coordinates, as a region can't handle bigger coordinates
	contentsToViewport2( cx, cy, cx, cy );
	QRegion reg( QRect( cx, cy, cw, ch ) );

//kdDebug() << "---cy-- " << contentsY() << endl;

	// Subtract the table from it
//	reg = reg.subtract( QRect( QPoint( 0, 0 ), ts-QSize(0,m_navPanel->isVisible() ? m_navPanel->height() : 0) ) );
	reg = reg.subtract( QRect( QPoint( 0, 0 ), ts
		-QSize(0,QMAX((m_navPanel ? m_navPanel->height() : 0), horizontalScrollBar()->sizeHint().height())
			- (horizontalScrollBar()->isVisible() ? horizontalScrollBar()->sizeHint().height()/2 : 0)
			+ (horizontalScrollBar()->isVisible() ? 0 : 
				d->internal_bottomMargin
//	horizontalScrollBar()->sizeHint().height()/2
		)
//- /*d->bottomMargin */ horizontalScrollBar()->sizeHint().height()*3/2
			+ contentsY()
//			- (verticalScrollBar()->isVisible() ? horizontalScrollBar()->sizeHint().height()/2 : 0)
			)
		) );
//	reg = reg.subtract( QRect( QPoint( 0, 0 ), ts ) );

	// And draw the rectangles (transformed inc contents coordinates as needed)
	QMemArray<QRect> r = reg.rects();
	for ( int i = 0; i < (int)r.count(); i++ ) {
		QRect rect( viewportToContents2(r[i].topLeft()), r[i].size() );
/*		kdDebug(44021) << QString("- pEA: p->fillRect(x:%1 y:%2 w:%3 h:%4)")
			.arg(rect.x()).arg(rect.y())
			.arg(rect.width()).arg(rect.height()) << endl;*/
//		p->fillRect( QRect(viewportToContents2(r[i].topLeft()),r[i].size()), d->emptyAreaColor );
		p->fillRect( rect, d->appearance.emptyAreaColor );
//		p->fillRect( QRect(viewportToContents2(r[i].topLeft()),r[i].size()), viewport()->backgroundBrush() );
	}
}

void KexiTableView::contentsMouseDoubleClickEvent(QMouseEvent *e)
{
//	kdDebug(44021) << "KexiTableView::contentsMouseDoubleClickEvent()" << endl;
	m_contentsMousePressEvent_dblClick = true;
	contentsMousePressEvent(e);
	m_contentsMousePressEvent_dblClick = false;

	if(m_currentItem)
	{
		if(d->editOnDoubleClick && columnEditable(m_curCol) 
			&& columnType(m_curCol) != KexiDB::Field::Boolean)
		{
			startEditCurrentCell();
//			createEditor(m_curRow, m_curCol, QString::null);
		}

		emit itemDblClicked(m_currentItem, m_curRow, m_curCol);
	}
}

void KexiTableView::contentsMousePressEvent( QMouseEvent* e )
{
//	kdDebug(44021) << "KexiTableView::contentsMousePressEvent() ??" << endl;
	setFocus();
	if(m_data->count()==0 && !isInsertingEnabled()) {
		QScrollView::contentsMousePressEvent( e );
		return;
	}

	if (columnAt(e->pos().x())==-1) { //outside a colums
		QScrollView::contentsMousePressEvent( e );
		return;
	}
//	d->contentsMousePressEvent_ev = *e;
//	d->contentsMousePressEvent_enabled = true;
//	QTimer::singleShot(2000, this, SLOT( contentsMousePressEvent_Internal() ));
//	d->contentsMousePressEvent_timer.start(100,true);
	
//	if (!d->contentsMousePressEvent_enabled)
//		return;
//	d->contentsMousePressEvent_enabled=false;

	if (!d->moveCursorOnMouseRelease) {
		if (!handleContentsMousePressOrRelease(e, false))
			return;
	}

//	kdDebug(44021)<<"void KexiTableView::contentsMousePressEvent( QMouseEvent* e ) by now the current items should be set, if not -> error + crash"<<endl;
	if(e->button() == RightButton)
	{
		showContextMenu(e->globalPos());
	}
	else if(e->button() == LeftButton)
	{
		if(columnType(m_curCol) == KexiDB::Field::Boolean && columnEditable(m_curCol))
		{
			boolToggled();
		}
#if 0 //js: TODO
		else if(columnType(m_curCol) == QVariant::StringList && columnEditable(m_curCol))
		{
			createEditor(m_curRow, m_curCol);
		}
#endif
	}
//ScrollView::contentsMousePressEvent( e );
}

void KexiTableView::contentsMouseReleaseEvent( QMouseEvent* e )
{
//	kdDebug(44021) << "KexiTableView::contentsMousePressEvent() ??" << endl;
	if(m_data->count()==0 && !isInsertingEnabled())
		return;

	if (d->moveCursorOnMouseRelease)
		handleContentsMousePressOrRelease(e, true);

	int col = columnAt(e->pos().x());
	int row = rowAt(e->pos().y());

	if (!m_currentItem || col==-1 || row==-1 || col!=m_curCol || row!=m_curRow)//outside a current cell
		return;

	QScrollView::contentsMouseReleaseEvent( e );

	emit itemMouseReleased(m_currentItem, m_curRow, m_curCol);
}

//! @internal called by contentsMouseOrEvent() contentsMouseReleaseEvent() to move cursor
bool KexiTableView::handleContentsMousePressOrRelease(QMouseEvent* e, bool release)
{
	// remember old focus cell
	int oldRow = m_curRow;
	int oldCol = m_curCol;
	kdDebug(44021) << "oldRow=" << oldRow <<" oldCol=" << oldCol <<endl;
	bool onInsertItem = false;

	int newrow, newcol;
	//compute clicked row nr
	if (isInsertingEnabled()) {
		if (rowAt(e->pos().y())==-1) {
			newrow = rowAt(e->pos().y() - d->rowHeight);
			if (newrow==-1 && m_data->count()>0) {
				if (release)
					QScrollView::contentsMouseReleaseEvent( e );
				else
					QScrollView::contentsMousePressEvent( e );
				return false;
			}
			newrow++;
			kdDebug(44021) << "Clicked just on 'insert' row." << endl;
			onInsertItem=true;
		}
		else {
			// get new focus cell
			newrow = rowAt(e->pos().y());
		}
	}
	else {
		if (rowAt(e->pos().y())==-1 || columnAt(e->pos().x())==-1) {
			if (release)
				QScrollView::contentsMouseReleaseEvent( e );
			else
				QScrollView::contentsMousePressEvent( e );
			return false; //clicked outside a grid
		}
		// get new focus cell
		newrow = rowAt(e->pos().y());
	}
	newcol = columnAt(e->pos().x());

	if(e->button() != NoButton) {
		setCursorPosition(newrow,newcol);
	}
	return true;
}

/*moved
KPopupMenu* KexiTableView::popup() const
{
	return m_popup;
}*/

void KexiTableView::showContextMenu(QPoint pos)
{
	if (!d->contextMenuEnabled || m_popup->count()<1)
		return;
	if (pos==QPoint(-1,-1)) {
		pos = viewport()->mapToGlobal( QPoint( columnPos(m_curCol), rowPos(m_curRow) + d->rowHeight ) );
	}
	//show own context menu if configured
//	if (updateContextMenu()) {
		selectRow(m_curRow);
		m_popup->exec(pos);
/*	}
	else {
		//request other context menu
		emit contextMenuRequested(m_currentItem, m_curCol, pos);
	}*/
}

void KexiTableView::contentsMouseMoveEvent( QMouseEvent *e )
{
	if (d->appearance.rowHighlightingEnabled) {
		int row;
		if (columnAt(e->x())<0)
			row = -1;
		else
			row = rowAt( e->y() );

//	const col = columnAt(e->x());
//	columnPos(col) + columnWidth(col)
//	columnPos(d->numCols - 1) + columnWidth(d->numCols - 1)));

		if (row != d->highlightedRow) {
			updateRow(d->highlightedRow);
				d->highlightedRow = row;
			updateRow(d->highlightedRow);
		}
	}

#if 0//(js) doesn't work!

	// do the same as in mouse press
	int x,y;
	contentsToViewport(e->x(), e->y(), x, y);

	if(y > visibleHeight())
	{
		d->needAutoScroll = true;
		d->scrollTimer->start(70, false);
		d->scrollDirection = ScrollDown;
	}
	else if(y < 0)
	{
		d->needAutoScroll = true;
		d->scrollTimer->start(70, false);
		d->scrollDirection = ScrollUp;
	}
	else if(x > visibleWidth())
	{
		d->needAutoScroll = true;
		d->scrollTimer->start(70, false);
		d->scrollDirection = ScrollRight;
	}
	else if(x < 0)
	{
		d->needAutoScroll = true;
		d->scrollTimer->start(70, false);
		d->scrollDirection = ScrollLeft;
	}
	else
	{
		d->needAutoScroll = false;
		d->scrollTimer->stop();
		contentsMousePressEvent(e);
	}
#endif
	QScrollView::contentsMouseMoveEvent(e);
}

#if 0 //moved
void KexiTableView::startEditCurrentCell(const QString &setText)
{
//	if (columnType(m_curCol) == KexiDB::Field::Boolean)
//		return;
	if (isReadOnly() || !columnEditable(m_curCol))
		return;
	if (m_editor) {
		if (m_editor->hasFocusableWidget()) {
			m_editor->show();
			m_editor->setFocus();
		}
	}
	ensureVisible(columnPos(m_curCol), rowPos(m_curRow)+rowHeight(), columnWidth(m_curCol), rowHeight());
	createEditor(m_curRow, m_curCol, setText, !setText.isEmpty());
}

void KexiTableView::deleteAndStartEditCurrentCell()
{
	if (isReadOnly() || !columnEditable(m_curCol))
		return;
	if (m_editor) {//if we've editor - just clear it
		m_editor->clear();
		return;
	}
	if (columnType(m_curCol) == KexiDB::Field::Boolean)
		return;
	ensureVisible(columnPos(m_curCol), rowPos(m_curRow)+rowHeight(), columnWidth(m_curCol), rowHeight());
	createEditor(m_curRow, m_curCol, QString::null, false/*removeOld*/);
	if (!m_editor)
		return;
	m_editor->clear();
	if (m_editor->acceptEditorAfterDeleteContents())
		acceptEditor();
}
#endif //moved

#if 0//(js) doesn't work!
void KexiTableView::contentsMouseReleaseEvent(QMouseEvent *)
{
	if(d->needAutoScroll)
	{
		d->scrollTimer->stop();
	}
}
#endif

/*
moved to KexiSharedActionClient
void KexiTableView::plugSharedAction(KAction* a)
{
	if (!a)
		return;
	d->sharedActions.insert(a->name(), a);
}
*/

static bool overrideEditorShortcutNeeded(QKeyEvent *e)
{
	//perhaps more to come...
	return e->key() == Qt::Key_Delete && e->state()==Qt::ControlButton;
}

bool KexiTableView::shortCutPressed( QKeyEvent *e, const QCString &action_name )
{
	KAction *action = m_sharedActions[action_name];
	if (action) {
		if (!action->isEnabled())//this action is disabled - don't process it!
			return false; 
		if (action->shortcut() == KShortcut( KKey(e) )) {
			//special cases when we need to override editor's shortcut
			if (overrideEditorShortcutNeeded(e)) {
				return true;
			}
			return false;//this shortcut is owned by shared action - don't process it!
		}
	}

	//check default shortcut (when user app has no action shortcuts defined
	// but we want these shortcuts to still work)
	if (action_name=="data_save_row")
		return (e->key() == Key_Return || e->key() == Key_Enter) && e->state()==ShiftButton;
	if (action_name=="edit_delete_row")
		return e->key() == Key_Delete && e->state()==ControlButton;
	if (action_name=="edit_delete")
		return e->key() == Key_Delete && e->state()==NoButton;
	if (action_name=="edit_edititem")
		return e->key() == Key_F2 && e->state()==NoButton;
	if (action_name=="edit_insert_empty_row")
		return e->key() == Key_Insert && e->state()==(ShiftButton | ControlButton);

	return false;
}

void KexiTableView::keyPressEvent(QKeyEvent* e)
{
	if (!hasData())
		return;
//	kdDebug() << "KexiTableView::keyPressEvent: key=" <<e->key() << " txt=" <<e->text()<<endl;

	const bool ro = isReadOnly();
	QWidget *w = focusWidget();
//	if (!w || w!=viewport() && w!=this && (!m_editor || w!=m_editor->view() && w!=m_editor)) {
//	if (!w || w!=viewport() && w!=this && (!m_editor || w!=m_editor->view())) {
	if (!w || w!=viewport() && w!=this && (!m_editor || !Kexi::hasParent(dynamic_cast<QObject*>(m_editor), w))) {
		//don't process stranger's events
		e->ignore();
		return;
	}
	if (d->skipKeyPress) {
		d->skipKeyPress=false;
		e->ignore();
		return;
	}
	
	if(m_currentItem == 0 && (m_data->count() > 0 || isInsertingEnabled()))
	{
		setCursorPosition(0,0);
	}
	else if(m_data->count() == 0 && !isInsertingEnabled())
	{
		e->accept();
		return;
	}

	if(m_editor) {// if a cell is edited, do some special stuff
		if (e->key() == Key_Escape) {
			cancelEditor();
			e->accept();
			return;
		} else if (e->key() == Key_Return || e->key() == Key_Enter) {
			if (columnType(m_curCol) == KexiDB::Field::Boolean) {
				boolToggled();
			}
			else {
				acceptEditor();
			}
			e->accept();
			return;
		}
	}
	else if (m_rowEditing) {// if a row is in edit mode, do some special stuff
		if (shortCutPressed( e, "data_save_row")) {
			kdDebug() << "shortCutPressed!!!" <<endl;
			acceptRowEdit();
			return;
		}
	}

	if(e->key() == Key_Return || e->key() == Key_Enter)
	{
		emit itemReturnPressed(m_currentItem, m_curRow, m_curCol);
	}

	int curRow = m_curRow;
	int curCol = m_curCol;

	const bool nobtn = e->state()==NoButton;
	bool printable = false;

	//check shared shortcuts
	if (!ro) {
		if (shortCutPressed(e, "edit_delete_row")) {
			deleteCurrentRow();
			e->accept();
			return;
		} else if (shortCutPressed(e, "edit_delete")) {
			deleteAndStartEditCurrentCell();
			e->accept();
			return;
		}
		else if (shortCutPressed(e, "edit_insert_empty_row")) {
			insertEmptyRow();
			e->accept();
			return;
		}
	}

	switch (e->key())
	{
/*	case Key_Delete:
		if (e->state()==Qt::ControlButton) {//remove current row
			deleteCurrentRow();
		}
		else if (nobtn) {//remove contents of the current cell
			deleteAndStartEditCurrentCell();
		}
		break;*/

	case Key_Shift:
	case Key_Alt:
	case Key_Control:
	case Key_Meta:
		e->ignore();
		break;
	case Key_Up:
		if (nobtn) {
			selectPrevRow();
			e->accept();
			return;
		}
		break;
	case Key_Down:
		if (nobtn) {
//			curRow = QMIN(rows() - 1 + (isInsertingEnabled()?1:0), curRow + 1);
			selectNextRow();
			e->accept();
			return;
		}
		break;
	case Key_PageUp:
		if (nobtn) {
//			curRow -= visibleHeight() / d->rowHeight;
//			curRow = QMAX(0, curRow);
			selectPrevPage();
			e->accept();
			return;
		}
		break;
	case Key_PageDown:
		if (nobtn) {
//			curRow += visibleHeight() / d->rowHeight;
//			curRow = QMIN(rows() - 1 + (isInsertingEnabled()?1:0), curRow);
			selectNextPage();
			e->accept();
			return;
		}
		break;
	case Key_Home:
		if (d->appearance.fullRowSelection) {
			//we're in row-selection mode: home key always moves to 1st row
			curRow = 0;//to 1st row
		}
		else {//cell selection mode: different actions depending on ctrl and shift keys state
			if (nobtn) {
				curCol = 0;//to 1st col
			}
			else if (e->state()==ControlButton) {
				curRow = 0;//to 1st row
			}
			else if (e->state()==(ControlButton|ShiftButton)) {
				curRow = 0;//to 1st row and col
				curCol = 0;
			}
		}
		break;
	case Key_End:
		if (d->appearance.fullRowSelection) {
			//we're in row-selection mode: home key always moves to last row
			curRow = m_data->count()-1+(isInsertingEnabled()?1:0);//to last row
		}
		else {//cell selection mode: different actions depending on ctrl and shift keys state
			if (nobtn) {
				curCol = columns()-1;//to last col
			}
			else if (e->state()==ControlButton) {
				curRow = m_data->count()-1+(isInsertingEnabled()?1:0);//to last row
			}
			else if (e->state()==(ControlButton|ShiftButton)) {
				curRow = m_data->count()-1+(isInsertingEnabled()?1:0);//to last row and col
				curCol = columns()-1;//to last col
			}
		}
		break;
	case Key_Backspace:
		if (nobtn && !ro && columnType(curCol) != KexiDB::Field::Boolean && columnEditable(curCol))
			createEditor(curRow, curCol, QString::null, true);
		break;
	case Key_Space:
		if (nobtn && !ro && columnEditable(curCol)) {
			if (columnType(curCol) == KexiDB::Field::Boolean) {
				boolToggled();
				break;
			}
			else
				printable = true; //just space key
		}
	case Key_Escape:
		if (nobtn && m_rowEditing) {
			cancelRowEdit();
			return;
		}
	default:
		//others:
		if (nobtn && (e->key()==Key_Tab || e->key()==Key_Right)) {
//! \todo add option for stopping at 1st column for Key_left
			//tab
			if (acceptEditor()) {
				if (curCol == (columns() - 1)) {
					if (curRow < (rows()-1+(isInsertingEnabled()?1:0))) {//skip to next row
						curRow++;
						curCol = 0;
					}
				}
				else
					curCol++;
			}
		}
		else if ((e->state()==ShiftButton && e->key()==Key_Tab)
		 || (nobtn && e->key()==Key_Backtab)
		 || (e->state()==ShiftButton && e->key()==Key_Backtab)
		 || (nobtn && e->key()==Key_Left)
			) {
//! \todo add option for stopping at last column
			//backward tab
			if (acceptEditor()) {
				if (curCol == 0) {
					if (curRow>0) {//skip to previous row
						curRow--;
						curCol = columns() - 1;
					}
				}
				else
					curCol--;
			}
		}
		else if ( nobtn && (e->key()==Key_Enter || e->key()==Key_Return || shortCutPressed(e, "edit_edititem")) ) {
			startEditOrToggleValue();
		}
		else if (nobtn && e->key()==d->contextMenuKey) { //Key_Menu:
			showContextMenu();
		}
		else {
			KexiTableEdit *edit = dynamic_cast<KexiTableEdit*>( editor( m_curCol ) );
			if (edit && edit->handleKeyPress(e, m_editor==edit)) {
				//try to handle the event @ editor's level
				e->accept();
				return;
			}

			qDebug("KexiTableView::KeyPressEvent(): default");
			if (e->text().isEmpty() || !e->text().isEmpty() && !e->text()[0].isPrint() ) {
				kdDebug(44021) << "NOT PRINTABLE: 0x0" << QString("%1").arg(e->key(),0,16) <<endl;
//				e->ignore();
				QScrollView::keyPressEvent(e);
				return;
			}

			printable = true;
		}
	}
	//finally: we've printable char:
	if (printable && !ro) {
		KexiTableViewColumn *colinfo = m_data->column(curCol);
		if (colinfo->acceptsFirstChar(e->text()[0])) {
			kdDebug(44021) << "KexiTableView::KeyPressEvent(): ev pressed: acceptsFirstChar()==true";
	//			if (e->text()[0].isPrint())
			createEditor(curRow, curCol, e->text(), true);
		}
		else {
//TODO show message "key not allowed eg. on a statusbar"
			kdDebug(44021) << "KexiTableView::KeyPressEvent(): ev pressed: acceptsFirstChar()==false";
		}
	}

	d->vScrollBarValueChanged_enabled=false;

	// if focus cell changes, repaint
	setCursorPosition(curRow, curCol);

	d->vScrollBarValueChanged_enabled=true;

	e->accept();
}

void KexiTableView::emitSelected()
{
	if(m_currentItem)
		emit itemSelected(m_currentItem);
}

/*moved
void KexiTableView::startEditOrToggleValue()
{
	if ( !isReadOnly() && columnEditable(m_curCol) ) {
		if (columnType(m_curCol) == KexiDB::Field::Boolean) {
			boolToggled();
		}
		else {
			startEditCurrentCell();
			return;
		}
	}
}

void KexiTableView::boolToggled()
{
	startEditCurrentCell();
	if (m_editor) {
		m_editor->clickedOnContents();
	}
	acceptEditor();
	updateCell(m_curRow, m_curCol);

#if 0
	int s = m_currentItem->at(m_curCol).toInt();
	QVariant oldValue=m_currentItem->at(m_curCol);
	(*m_currentItem)[m_curCol] = QVariant(s ? 0 : 1);
	updateCell(m_curRow, m_curCol);
//	emit itemChanged(m_currentItem, m_curRow, m_curCol, oldValue);
//	emit itemChanged(m_currentItem, m_curRow, m_curCol);
#endif
}

void KexiTableView::clearSelection()
{
//	selectRow( -1 );
	int oldRow = m_curRow;
//	int oldCol = m_curCol;
	m_curRow = -1;
	m_curCol = -1;
	m_currentItem = 0;
	updateRow( oldRow );
	m_navPanel->setCurrentRecordNumber(0);
//	setNavRowNumber(-1);
}

void KexiTableView::selectNextRow()
{
	selectRow( QMIN( rows() - 1 +(isInsertingEnabled()?1:0), m_curRow + 1 ) );
}
*/

int KexiTableView::rowsPerPage() const
{
	return visibleHeight() / d->rowHeight;
}

/*moved
void KexiTableView::selectPrevPage()
{
	selectRow( 
		QMAX( 0, m_curRow - rowsPerPage() )
	);
}

void KexiTableView::selectNextPage()
{
	selectRow( 
		QMIN( 
			rows() - 1 + (isInsertingEnabled()?1:0),
			m_curRow + rowsPerPage()
		)
	);
}

void KexiTableView::selectFirstRow()
{
	selectRow(0);
}

void KexiTableView::selectLastRow()
{
	selectRow(rows() - 1 + (isInsertingEnabled()?1:0));
}

void KexiTableView::selectRow(int row)
{
	setCursorPosition(row, -1);
}

void KexiTableView::selectPrevRow()
{
	selectRow( QMAX( 0, m_curRow - 1 ) );
}
*/

KexiDataItemInterface *KexiTableView::editor( int col, bool ignoreMissingEditor )
{
	if (!m_data || col<0 || col>=columns())
		return 0;
	KexiTableViewColumn *tvcol = m_data->column(col);
//	int t = tvcol->field->type();

	//find the editor for this column
	KexiTableEdit *editor = d->editors[ tvcol ];
	if (editor)
		return editor;

	//not found: create
//	editor = KexiCellEditorFactory::createEditor(*m_data->column(col)->field, this);
	editor = KexiCellEditorFactory::createEditor(*m_data->column(col), this);
	if (!editor) {//create error!
		if (!ignoreMissingEditor) {
			//js TODO: show error???
			cancelRowEdit();
		}
		return 0;
	}
	editor->hide();
	connect(editor,SIGNAL(editRequested()),this,SLOT(slotEditRequested()));
	connect(editor,SIGNAL(cancelRequested()),this,SLOT(cancelEditor()));
	connect(editor,SIGNAL(acceptRequested()),this,SLOT(acceptEditor()));

	editor->resize(columnWidth(col)-1, rowHeight()-1);
	editor->installEventFilter(this);
	if (editor->widget())
		editor->widget()->installEventFilter(this);
	//store
	d->editors.insert( tvcol, editor );
	return editor;
}

void KexiTableView::editorShowFocus( int /*row*/, int col )
{
	KexiDataItemInterface *edit = editor( col );
	/*nt p = rowPos(row);
	 (!edit || (p < contentsY()) || (p > (contentsY()+clipper()->height()))) {
		kdDebug()<< "KexiTableView::editorShowFocus() : OUT" << endl;
		return;
	}*/
	if (edit) {
		kdDebug()<< "KexiTableView::editorShowFocus() : IN" << endl;
		QRect rect = cellGeometry( m_curRow, m_curCol );
//		rect.moveBy( -contentsX(), -contentsY() );
		edit->showFocus( rect );
	}
}

void KexiTableView::slotEditRequested()
{
//	KexiTableEdit *edit = editor( m_curCol );
//	if (edit) {

	createEditor(m_curRow, m_curCol);
}

void KexiTableView::createEditor(int row, int col, const QString& addText, bool removeOld)
{
	kdDebug(44021) << "KexiTableView::createEditor('"<<addText<<"',"<<removeOld<<")"<<endl;
	if (isReadOnly()) {
		kdDebug(44021) << "KexiTableView::createEditor(): DATA IS READ ONLY!"<<endl;
		return;
	}

	if (m_data->column(col)->readOnly()) {//d->pColumnModes.at(d->numCols-1) & ColumnReadOnly)
		kdDebug(44021) << "KexiTableView::createEditor(): COL IS READ ONLY!"<<endl;
		return;
	}

	const bool startRowEdit = !m_rowEditing; //remember if we're starting row edit

	if (!m_rowEditing) {
		//we're starting row editing session
		m_data->clearRowEditBuffer();
		
		m_rowEditing = true;
		//indicate on the vheader that we are editing:
		m_verticalHeader->setEditRow(m_curRow);
		if (isInsertingEnabled() && m_currentItem==m_insertItem) {
			//we should know that we are in state "new row editing"
			m_newRowEditing = true;
			//'insert' row editing: show another row after that:
			m_data->append( m_insertItem );
			//new empty insert item
			m_insertItem = new KexiTableItem(columns());
//			updateContents();
			m_verticalHeader->addLabel();
			d->verticalHeaderAlreadyAdded = true;
			updateWidgetContentsSize();
			//refr. current and next row
			updateContents(columnPos(0), rowPos(row), viewport()->width(), d->rowHeight*2);
//			updateContents(columnPos(0), rowPos(row+1), viewport()->width(), d->rowHeight);
//js: warning this breaks behaviour (cursor is skipping, etc.): qApp->processEvents(500);
			ensureVisible(columnPos(m_curCol), rowPos(row+1)+d->rowHeight-1, columnWidth(m_curCol), d->rowHeight);

			m_verticalHeader->setOffset(contentsY());
		}
	}	

	m_editor = editor( col );
	QWidget *m_editorWidget = dynamic_cast<QWidget*>(m_editor);
	if (!m_editorWidget)
		return;

	m_editor->setValue(*bufferedValueAt(col), addText, removeOld);
	if (m_editor->hasFocusableWidget()) {
		moveChild(m_editorWidget, columnPos(m_curCol), rowPos(m_curRow));

		m_editorWidget->resize(columnWidth(m_curCol)-1, rowHeight()-1);
		m_editorWidget->show();

		m_editor->setFocus();
	}

	if (startRowEdit)
		emit rowEditStarted(m_curRow);
}

void KexiTableView::focusInEvent(QFocusEvent*)
{
	updateCell(m_curRow, m_curCol);
}


void KexiTableView::focusOutEvent(QFocusEvent*)
{
	d->scrollBarTipTimer.stop();
	d->scrollBarTip->hide();
	
	updateCell(m_curRow, m_curCol);
}

bool KexiTableView::focusNextPrevChild(bool /*next*/)
{
	return false; //special Tab/BackTab meaning
/*	if (m_editor)
		return true;
	return QScrollView::focusNextPrevChild(next);*/
}

void KexiTableView::resizeEvent(QResizeEvent *e)
{
	QScrollView::resizeEvent(e);
	//updateGeometries();
	
	if (m_navPanel)
		m_navPanel->updateGeometry(leftMargin());
//	updateNavPanelGeometry();

	if ((contentsHeight() - e->size().height()) <= d->rowHeight) {
		slotUpdate();
		triggerUpdate();
	}
//	d->pTopHeader->repaint();


/*		m_navPanel->setGeometry(
			frameWidth(),
			viewport()->height() +d->pTopHeader->height() 
			-(horizontalScrollBar()->isVisible() ? 0 : horizontalScrollBar()->sizeHint().height())
			+frameWidth(),
			m_navPanel->sizeHint().width(), // - verticalScrollBar()->sizeHint().width() - horizontalScrollBar()->sizeHint().width(),
			horizontalScrollBar()->sizeHint().height()
		);*/
//		updateContents();
//		m_navPanel->setGeometry(1,horizontalScrollBar()->pos().y(),
	//		m_navPanel->width(), horizontalScrollBar()->height());
//	updateContents(0,0,2000,2000);//js
//	erase(); repaint();
}

#if 0//moved
void KexiTableView::updateNavPanelGeometry()
{
	if (!m_navPanel)
		return;
	m_navPanel->updateGeometry();
//	QRect g = m_navPanel->geometry();
//		kdDebug(44021) << "**********"<< g.top() << " " << g.left() <<endl;
	int navWidth;
	if (horizontalScrollBar()->isVisible()) {
		navWidth = m_navPanel->sizeHint().width();
	}
	else {
		navWidth = leftMargin() + clipper()->width();
	}
	
	m_navPanel->setGeometry(
		frameWidth(),
		height() - horizontalScrollBar()->sizeHint().height()-frameWidth(),
		navWidth,
		horizontalScrollBar()->sizeHint().height()
	);

//	horizontalScrollBar()->move(m_navPanel->x()+m_navPanel->width()+20,horizontalScrollBar()->y());

//	horizontalScrollBar()->hide();
//	updateGeometry();
	updateScrollBars();
//	horizontalScrollBar()->move(m_navPanel->x()+m_navPanel->width()+20,horizontalScrollBar()->y());
#if 0 //prev impl.
	QRect g = m_navPanel->geometry();
//		kdDebug(44021) << "**********"<< g.top() << " " << g.left() <<endl;
	m_navPanel->setGeometry(
		frameWidth(),
		height() - horizontalScrollBar()->sizeHint().height()-frameWidth(),
		m_navPanel->sizeHint().width(), // - verticalScrollBar()->sizeHint().width() - horizontalScrollBar()->sizeHint().width(),
		horizontalScrollBar()->sizeHint().height()
	);
#endif
}
#endif

void KexiTableView::viewportResizeEvent( QResizeEvent *e )
{
	QScrollView::viewportResizeEvent( e );
	updateGeometries();
//	erase(); repaint();
}

void KexiTableView::showEvent(QShowEvent *e)
{
	QScrollView::showEvent(e);
	if (!d->maximizeColumnsWidthOnShow.isEmpty()) {
		maximizeColumnsWidth(d->maximizeColumnsWidthOnShow);
		d->maximizeColumnsWidthOnShow.clear();
	}

	if (m_initDataContentsOnShow) {
		//full init
		m_initDataContentsOnShow = false;
		initDataContents();
	}
	else {
		//just update size
		QSize s(tableSize());
//	QRect r(cellGeometry(rows() - 1 + (isInsertingEnabled()?1:0), columns() - 1 ));
//	resizeContents(r.right() + 1, r.bottom() + 1);
		resizeContents(s.width(),s.height());
	}
	updateGeometries();

	//now we can ensure cell's visibility ( if there was such a call before show() )
	if (d->ensureCellVisibleOnShow!=QPoint(-1,-1)) {
		ensureCellVisible( d->ensureCellVisibleOnShow.x(), d->ensureCellVisibleOnShow.y() );
		d->ensureCellVisibleOnShow = QPoint(-1,-1); //reset the flag
	}
	if (m_navPanel)
		m_navPanel->updateGeometry(leftMargin());
//	updateNavPanelGeometry();
}

/*moved
bool KexiTableView::dropsAtRowEnabled() const
{
	return m_dropsAtRowEnabled;
}

void KexiTableView::setDropsAtRowEnabled(bool set)
{
//	const bool old = m_dropsAtRowEnabled;
	if (!set)
		d->dragIndicatorLine = -1;
	if (m_dropsAtRowEnabled && !set) {
		m_dropsAtRowEnabled = false;
		update();
	}
	else {
		m_dropsAtRowEnabled = set;
	}
}
*/

void KexiTableView::contentsDragMoveEvent(QDragMoveEvent *e)
{
	if (!hasData())
		return;
	if (m_dropsAtRowEnabled) {
		QPoint p = e->pos();
		int row = rowAt(p.y());
		KexiTableItem *item = 0;
//		if (row==(rows()-1) && (p.y() % d->rowHeight) > (d->rowHeight*2/3) ) {
		if ((p.y() % d->rowHeight) > (d->rowHeight*2/3) ) {
			row++;
		}
		item = m_data->at(row);
		emit dragOverRow(item, row, e);
		if (e->isAccepted()) {
			if (m_dragIndicatorLine>=0 && m_dragIndicatorLine != row) {
				//erase old indicator
				updateRow(m_dragIndicatorLine);
			}
			if (m_dragIndicatorLine != row) {
				m_dragIndicatorLine = row;
				updateRow(m_dragIndicatorLine);
			}
		}
		else {
			if (m_dragIndicatorLine>=0) {
				//erase old indicator
				updateRow(m_dragIndicatorLine);
			}
			m_dragIndicatorLine = -1;
		}
	}
	else
		e->acceptAction(false);
/*	for(QStringList::Iterator it = d->dropFilters.begin(); it != d->dropFilters.end(); it++)
	{
		if(e->provides((*it).latin1()))
		{
			e->acceptAction(true);
			return;
		}
	}*/
//	e->acceptAction(false);
}

void KexiTableView::contentsDropEvent(QDropEvent *ev)
{
	if (!hasData())
		return;
	if (m_dropsAtRowEnabled) {
		//we're no longer dragging over the table
		if (m_dragIndicatorLine>=0) {
			int row2update = m_dragIndicatorLine;
			m_dragIndicatorLine = -1;
			updateRow(row2update);
		}
		QPoint p = ev->pos();
		int row = rowAt(p.y());
		if ((p.y() % d->rowHeight) > (d->rowHeight*2/3) ) {
			row++;
		}
		KexiTableItem *item = m_data->at(row);
		KexiTableItem *newItem = 0;
		emit droppedAtRow(item, row, ev, newItem);
		if (newItem) {
			const int realRow = (row==m_curRow ? -1 : row);
			insertItem(newItem, realRow);
			setCursorPosition(row, 0);
//			m_currentItem = newItem;
		}
	}
}

void KexiTableView::viewportDragLeaveEvent( QDragLeaveEvent * )
{
	if (!hasData())
		return;
	if (m_dropsAtRowEnabled) {
		//we're no longer dragging over the table
		if (m_dragIndicatorLine>=0) {
			int row2update = m_dragIndicatorLine;
			m_dragIndicatorLine = -1;
			updateRow(row2update);
		}
	}
}

void KexiTableView::updateCell(int row, int col)
{
//	kdDebug(44021) << "updateCell("<<row<<", "<<col<<")"<<endl;
	updateContents(cellGeometry(row, col));
/*	QRect r = cellGeometry(row, col);
	r.setHeight(r.height()+6);
	r.setTop(r.top()-3);
	updateContents();*/
}

void KexiTableView::updateRow(int row)
{
//	kdDebug(44021) << "updateRow("<<row<<")"<<endl;
	if (row < 0 || row >= (rows() + 2/* sometimes we want to refresh the row after last*/ ))
		return;
	int leftcol = d->pTopHeader->sectionAt( d->pTopHeader->offset() );
//	int rightcol = d->pTopHeader->sectionAt( clipper()->width() );
	updateContents( QRect( columnPos( leftcol ), rowPos(row), clipper()->width(), rowHeight() ) ); //columnPos(rightcol)+columnWidth(rightcol), rowHeight() ) );
}

void KexiTableView::slotColumnWidthChanged( int, int, int )
{
	QSize s(tableSize());
	int w = contentsWidth();
	viewport()->setUpdatesEnabled(false);
	resizeContents( s.width(), s.height() );
	viewport()->setUpdatesEnabled(true);
	if (contentsWidth() < w)
		updateContents(contentsX(), 0, viewport()->width(), contentsHeight());
//		repaintContents( s.width(), 0, w - s.width() + 1, contentsHeight(), TRUE );
	else
	//	updateContents( columnPos(col), 0, contentsWidth(), contentsHeight() );
		updateContents(contentsX(), 0, viewport()->width(), contentsHeight());
	//	viewport()->repaint();

//	updateContents(0, 0, d->pBufferPm->width(), d->pBufferPm->height());
	QWidget *m_editorWidget = dynamic_cast<QWidget*>(m_editor);
	if (m_editorWidget)
	{
		m_editorWidget->resize(columnWidth(m_curCol)-1, rowHeight()-1);
		moveChild(m_editorWidget, columnPos(m_curCol), rowPos(m_curRow));
	}
	updateGeometries();
	updateScrollBars();
	if (m_navPanel)
		m_navPanel->updateGeometry(leftMargin());
//	updateNavPanelGeometry();
}

void KexiTableView::slotSectionHandleDoubleClicked( int section )
{
	adjustColumnWidthToContents(section);
	slotColumnWidthChanged(0,0,0); //to update contents and redraw
}


void KexiTableView::updateGeometries()
{
	QSize ts = tableSize();
	if (d->pTopHeader->offset() && ts.width() < (d->pTopHeader->offset() + d->pTopHeader->width()))
		horizontalScrollBar()->setValue(ts.width() - d->pTopHeader->width());

//	m_verticalHeader->setGeometry(1, topMargin() + 1, leftMargin(), visibleHeight());
	d->pTopHeader->setGeometry(leftMargin() + 1, 1, visibleWidth(), topMargin());
	m_verticalHeader->setGeometry(1, topMargin() + 1, leftMargin(), visibleHeight());
}

int KexiTableView::columnWidth(int col) const
{
	if (!hasData())
		return 0;
	int vcID = m_data->visibleColumnID( col );
	return vcID==-1 ? 0 : d->pTopHeader->sectionSize( vcID );
}

int KexiTableView::rowHeight() const
{
	return d->rowHeight;
}

int KexiTableView::columnPos(int col) const
{
	if (!hasData())
		return 0;
	//if this column is hidden, find first column before that is visible
	int c = QMIN(col, (int)m_data->columnsCount()-1), vcID = 0;
	while (c>=0 && (vcID=m_data->visibleColumnID( c ))==-1)
		c--;
	if (c<0)
		return 0;
	if (c==col)
		return d->pTopHeader->sectionPos(vcID);
	return d->pTopHeader->sectionPos(vcID)+d->pTopHeader->sectionSize(vcID);
}

int KexiTableView::rowPos(int row) const
{
	return d->rowHeight*row;
}

int KexiTableView::columnAt(int pos) const
{
	if (!hasData())
		return -1;
	int r = d->pTopHeader->sectionAt(pos);
	if (r<0)
		return r;
	return m_data->globalColumnID( r );

//	if (r==-1)
//		kdDebug() << "columnAt("<<pos<<")==-1 !!!" << endl;
//	return r;
}

int KexiTableView::rowAt(int pos, bool ignoreEnd) const
{
	if (!hasData())
		return -1;
	pos /=d->rowHeight;
	if (pos < 0)
		return 0;
	if ((pos >= (int)m_data->count()) && !ignoreEnd)
		return -1;
	return pos;
}

QRect KexiTableView::cellGeometry(int row, int col) const
{
	return QRect(columnPos(col), rowPos(row),
		columnWidth(col), rowHeight());
}

QSize KexiTableView::tableSize() const
{
	if ((rows()+ (isInsertingEnabled()?1:0) ) > 0 && columns() > 0) {
/*		kdDebug() << "tableSize()= " << columnPos( columns() - 1 ) + columnWidth( columns() - 1 ) 
			<< ", " << rowPos( rows()-1+(isInsertingEnabled()?1:0)) + d->rowHeight
//			+ QMAX(m_navPanel ? m_navPanel->height() : 0, horizontalScrollBar()->sizeHint().height())
			+ (m_navPanel->isVisible() ? QMAX( m_navPanel->height(), horizontalScrollBar()->sizeHint().height() ) :0 )
			+ margin() << endl;
*/
//		kdDebug()<< m_navPanel->isVisible() <<" "<<m_navPanel->height()<<" "
//		<<horizontalScrollBar()->sizeHint().height()<<" "<<rowPos( rows()-1+(isInsertingEnabled()?1:0))<<endl;

		//int xx = horizontalScrollBar()->sizeHint().height()/2;

		QSize s( 
			columnPos( columns() - 1 ) + columnWidth( columns() - 1 ),
//			+ verticalScrollBar()->sizeHint().width(),
			rowPos( rows()-1+(isInsertingEnabled()?1:0) ) + d->rowHeight
			+ (horizontalScrollBar()->isVisible() ? 0 : horizontalScrollBar()->sizeHint().height())
			+ d->internal_bottomMargin
//				horizontalScrollBar()->sizeHint().height()/2
//			- /*d->bottomMargin */ horizontalScrollBar()->sizeHint().height()*3/2

//			+ ( (m_navPanel && m_navPanel->isVisible() && verticalScrollBar()->isVisible()
	//			&& !horizontalScrollBar()->isVisible()) 
		//		? horizontalScrollBar()->sizeHint().height() : 0)

//			+ QMAX( (m_navPanel && m_navPanel->isVisible()) ? m_navPanel->height() : 0, 
//				horizontalScrollBar()->isVisible() ? horizontalScrollBar()->sizeHint().height() : 0)

//			+ (m_navPanel->isVisible() 
//				? QMAX( m_navPanel->height(), horizontalScrollBar()->sizeHint().height() ) :0 )

//			- (horizontalScrollBar()->isVisible() ? horizontalScrollBar()->sizeHint().height() :0 )
			+ margin() 
//-2*d->rowHeight
		);

//		kdDebug() << rows()-1 <<" "<< (isInsertingEnabled()?1:0) <<" "<< (m_rowEditing?1:0) << " " <<  s << endl;
		return s;
//			+horizontalScrollBar()->sizeHint().height() + margin() );
	}
	return QSize(0,0);
}

/*moved
int KexiTableView::rows() const
{
	if (!hasData())
		return 0;
	return m_data->count();
}

int KexiTableView::columns() const
{
	if (!hasData())
		return 0;
	return m_data->columns.count();
}
*/

void KexiTableView::ensureCellVisible(int row, int col/*=-1*/)
{
	if (!isVisible()) {
		//the table is invisible: we can't ensure visibility now
		d->ensureCellVisibleOnShow = QPoint(row,col);
		return;
	}

	//quite clever: ensure the cell is visible:
	QRect r( columnPos(col==-1 ? m_curCol : col), rowPos(row) +(d->appearance.fullRowSelection?1:0), 
		columnWidth(col==-1 ? m_curCol : col), rowHeight());

/*	if (m_navPanel && horizontalScrollBar()->isHidden() && row == rows()-1) {
		//when cursor is moved down and navigator covers the cursor's area,
		//area is scrolled up
		if ((viewport()->height() - m_navPanel->height()) < r.bottom()) {
			scrollBy(0,r.bottom() - (viewport()->height() - m_navPanel->height()));
		}
	}*/

	if (m_navPanel && m_navPanel->isVisible() && horizontalScrollBar()->isHidden()) {
		//a hack: for visible navigator: increase height of the visible rect 'r'
		r.setBottom(r.bottom()+m_navPanel->height());
	}

	QPoint pcenter = r.center();
	ensureVisible(pcenter.x(), pcenter.y(), r.width()/2, r.height()/2);
//	updateContents();
//	updateNavPanelGeometry();
//	slotUpdate();
}

#if 0 //moved
void KexiTableView::setCursorPosition(int row, int col/*=-1*/, bool forceSet)
{
	int newrow = row;
	int newcol = col;

	if(rows() <= 0) {
		m_verticalHeader->setCurrentRow(-1);
		if (isInsertingEnabled()) {
			m_currentItem=m_insertItem;
			newrow=0;
			if (col>=0)
				newcol=col;
			else
				newcol=0;
		}
		else {
			m_currentItem=0;
			m_curRow=-1;
			m_curCol=-1;
			return;
		}
	}

	if(col>=0)
	{
		newcol = QMAX(0, col);
		newcol = QMIN(columns() - 1, newcol);
	}
	else {
		newcol = m_curCol; //no changes
		newcol = QMAX(0, newcol); //may not be < 0 !
	}
	newrow = QMAX( 0, row);
	newrow = QMIN( rows() - 1 + (isInsertingEnabled()?1:0), newrow);

//	m_currentItem = itemAt(m_curRow);
//	kdDebug(44021) << "setCursorPosition(): m_curRow=" << m_curRow << " oldRow=" << oldRow << " m_curCol=" << m_curCol << " oldCol=" << oldCol << endl;

	if ( forceSet || m_curRow != newrow || m_curCol != newcol )
	{
		kdDebug(44021) << "setCursorPosition(): " <<QString("old:%1,%2 new:%3,%4").arg(m_curCol).arg(m_curRow).arg(newcol).arg(newrow) << endl;
		
		// cursor moved: get rid of editor
		if (m_editor) {
			if (!m_contentsMousePressEvent_dblClick) {
				if (!acceptEditor()) {
					return;
				}
				//update row num. again
				newrow = QMIN( rows() - 1 + (isInsertingEnabled()?1:0), newrow);
			}
		}

		if (m_curRow != newrow) {//update current row info
			m_navPanel->setCurrentRecordNumber(newrow+1);
//			setNavRowNumber(newrow);
//			d->navBtnPrev->setEnabled(newrow>0);
//			d->navBtnFirst->setEnabled(newrow>0);
//			d->navBtnNext->setEnabled(newrow<(rows()-1+(isInsertingEnabled()?1:0)));
//			d->navBtnLast->setEnabled(newrow!=(rows()-1));
		}

		// cursor moved to other row: end of row editing
		if (m_rowEditing && m_curRow != newrow) {
			if (!acceptRowEdit()) {
				//accepting failed: cancel setting the cursor
				return;
			}
			//update row number, because number of rows changed
			newrow = QMIN( rows() - 1 + (isInsertingEnabled()?1:0), newrow);
		}

		//change position
		int oldRow = m_curRow;
		int oldCol = m_curCol;
		m_curRow = newrow;
		m_curCol = newcol;

//		int cw = columnWidth( m_curCol );
//		int rh = rowHeight();
//		ensureVisible( columnPos( m_curCol ) + cw / 2, rowPos( m_curRow ) + rh / 2, cw / 2, rh / 2 );
//		center(columnPos(m_curCol) + cw / 2, rowPos(m_curRow) + rh / 2, cw / 2, rh / 2);
//	kdDebug(44021) << " contentsY() = "<< contentsY() << endl;

//js		if (oldRow > m_curRow)
//js			ensureVisible(columnPos(m_curCol), rowPos(m_curRow) + rh, columnWidth(m_curCol), rh);
//js		else// if (oldRow <= m_curRow)
//js		ensureVisible(columnPos(m_curCol), rowPos(m_curRow), columnWidth(m_curCol), rh);


		//show editor-dependent focus, if we're changing the current column
		if (oldCol>=0 && oldCol<columns() && m_curCol!=oldCol) {
			//find the editor for this column
			KexiTableEdit *edit = editor( oldCol );
			if (edit) {
				edit->hideFocus();
			}
		}

		//show editor-dependent focus, if needed
		//find the editor for this column
		editorShowFocus( m_curRow, m_curCol );

		updateCell( oldRow, oldCol );

		//quite clever: ensure the cell is visible:
		ensureCellVisible(m_curRow, m_curCol);

//		QPoint pcenter = QRect( columnPos(m_curCol), rowPos(m_curRow), columnWidth(m_curCol), rh).center();
//		ensureVisible(pcenter.x(), pcenter.y(), columnWidth(m_curCol)/2, rh/2);

//		ensureVisible(columnPos(m_curCol), rowPos(m_curRow) - contentsY(), columnWidth(m_curCol), rh);
		m_verticalHeader->setCurrentRow(m_curRow);
		updateCell( m_curRow, m_curCol );
		if (m_curCol != oldCol || m_curRow != oldRow ) //ensure this is also refreshed
			updateCell( oldRow, m_curCol );
		if (isInsertingEnabled() && m_curRow == rows()) {
			kdDebug(44021) << "NOW insert item is current" << endl;
			m_currentItem = m_insertItem;
		}
		else {
			kdDebug(44021) << QString("NOW item at %1 (%2) is current").arg(m_curRow).arg((ulong)itemAt(m_curRow)) << endl;
//NOT EFFECTIVE!!!!!!!!!!!
			m_currentItem = itemAt(m_curRow);
		}

		emit itemSelected(m_currentItem);
		emit cellSelected(m_curCol, m_curRow);
	}

	if(m_initDataContentsOnShow) {
		d->cursorPositionSetExplicityBeforeShow = true;
	}
}

bool KexiTableView::acceptEditor()
{
	if (!hasData())
		return true;
	if (!m_editor || d->inside_acceptEditor)
		return true;

	d->inside_acceptEditor = true;//avoid recursion

	QVariant newval;
	KexiValidator::Result res = KexiValidator::Ok;
	QString msg, desc;
	bool setNull = false;
//	bool allow = true;
//	static const QString msg_NOT_NULL = i18n("\"%1\" column requires a value to be entered.");

	//autoincremented field can be omitted (left as null or empty) if we're inserting a new row
	const bool autoIncColumnCanBeOmitted = m_newRowEditing && m_editor->field()->isAutoIncrement();

	bool valueChanged = m_editor->valueChanged();
	bool editCurrentCellAgain = false;

	if (valueChanged) {
		if (m_editor->valueIsNull()) {//null value entered
			if (m_editor->field()->isNotNull() && !autoIncColumnCanBeOmitted) {
				kdDebug() << "KexiTableView::acceptEditor(): NULL NOT ALLOWED!" << endl;
				res = KexiValidator::Error;
				msg = KexiValidator::msgColumnNotEmpty().arg(m_editor->field()->captionOrName())
					+ "\n\n" + KexiValidator::msgYouCanImproveData();
				desc = i18n("The column's constraint is declared as NOT NULL.");
				editCurrentCellAgain = true;
	//			allow = false;
	//			removeEditor();
	//			return true;
			}
			else {
				kdDebug() << "KexiTableView::acceptEditor(): NULL VALUE WILL BE SET" << endl;
				//ok, just leave newval as NULL
				setNull = true;
			}
		}
		else if (m_editor->valueIsEmpty()) {//empty value entered
			if (m_editor->field()->hasEmptyProperty()) {
				if (m_editor->field()->isNotEmpty() && !autoIncColumnCanBeOmitted) {
					kdDebug() << "KexiTableView::acceptEditor(): EMPTY NOT ALLOWED!" << endl;
					res = KexiValidator::Error;
					msg = KexiValidator::msgColumnNotEmpty().arg(m_editor->field()->captionOrName())
						+ "\n\n" + KexiValidator::msgYouCanImproveData();
					desc = i18n("The column's constraint is declared as NOT EMPTY.");
					editCurrentCellAgain = true;
	//				allow = false;
	//				removeEditor();
	//				return true;
				}
				else {
					kdDebug() << "KexiTableView::acceptEditor(): EMPTY VALUE WILL BE SET" << endl;
				}
			}
			else {
				if (m_editor->field()->isNotNull() && !autoIncColumnCanBeOmitted) {
					kdDebug() << "KexiTableView::acceptEditor(): NEITHER NULL NOR EMPTY VALUE CAN BE SET!" << endl;
					res = KexiValidator::Error;
					msg = KexiValidator::msgColumnNotEmpty().arg(m_editor->field()->captionOrName())
						+ "\n\n" + KexiValidator::msgYouCanImproveData();
					desc = i18n("The column's constraint is declared as NOT EMPTY and NOT NULL.");
					editCurrentCellAgain = true;
//				allow = false;
	//				removeEditor();
	//				return true;
				}
				else {
					kdDebug() << "KexiTableView::acceptEditor(): NULL VALUE WILL BE SET BECAUSE EMPTY IS NOT ALLOWED" << endl;
					//ok, just leave newval as NULL
					setNull = true;
				}
			}
		}
	}//changed

	//try to get the value entered:
	if (res == KexiValidator::Ok) {
		if (!setNull && !valueChanged
			|| setNull && m_currentItem->at(m_curCol).isNull()) {
			kdDebug() << "KexiTableView::acceptEditor(): VALUE NOT CHANGED." << endl;
			removeEditor();
			d->inside_acceptEditor = false;
			if (d->acceptsRowEditAfterCellAccepting || d->internal_acceptsRowEditAfterCellAccepting)
				acceptRowEdit();
			return true;
		}
		if (!setNull) {//get the new value 
			bool ok;
			newval = m_editor->value(ok);
			if (!ok) {
				kdDebug() << "KexiTableView::acceptEditor(): INVALID VALUE - NOT CHANGED." << endl;
				res = KexiValidator::Error;
//js: TODO get detailed info on why m_editor->value() failed
				msg = i18n("Entered value is invalid.")
					+ "\n\n" + KexiValidator::msgYouCanImproveData();
				editCurrentCellAgain = true;
//				removeEditor();
//				return true;
			}
		}

		//Check other validation rules:
		//1. check using validator
		KexiValidator *validator = m_data->column(m_curCol)->validator();
		if (validator) {
			res = validator->check(m_data->column(m_curCol)->field()->captionOrName(), 
				newval, msg, desc);
		}
	}

	//show the validation result if not OK:
	if (res == KexiValidator::Error) {
		if (desc.isEmpty())
			KMessageBox::sorry(this, msg);
		else
			KMessageBox::detailedSorry(this, msg, desc);
		editCurrentCellAgain = true;
//		allow = false;
	}
	else if (res == KexiValidator::Warning) {
		//js: todo: message!!!
		KMessageBox::messageBox(this, KMessageBox::Sorry, msg + "\n" + desc);
		editCurrentCellAgain = true;
	}

	if (res == KexiValidator::Ok) {
		//2. check using signal
		//bool allow = true;
//		emit aboutToChangeCell(m_currentItem, newval, allow);
//		if (allow) {
		//send changes to the backend
		if (m_data->updateRowEditBufferRef(m_currentItem,m_curCol,newval)) {
			kdDebug() << "KexiTableView::acceptEditor(): ------ EDIT BUFFER CHANGED TO:" << endl;
			m_data->rowEditBuffer()->debug();
		} else {
			kdDebug() << "KexiTableView::acceptEditor(): ------ CHANGE FAILED in KexiTableViewData::updateRowEditBuffer()" << endl;
			res = KexiValidator::Error;

			//now: there might be called cancelEditor() in updateRowEditBuffer() handler,
			//if this is true, m_editor is NULL.

			if (m_editor && m_data->result()->column>=0 && m_data->result()->column<columns()) {
				//move to faulty column (if m_editor is not cleared)
				setCursorPosition(m_curRow, m_data->result()->column);
			}
			if (!m_data->result()->msg.isEmpty()) {
				if (m_data->result()->desc.isEmpty())
					KMessageBox::sorry(this, m_data->result()->msg);
				else
					KMessageBox::detailedSorry(this, m_data->result()->msg, m_data->result()->desc);
			}
		}
	}

	if (res == KexiValidator::Ok) {
		removeEditor();
		emit itemChanged(m_currentItem, m_curRow, m_curCol, m_currentItem->at(m_curCol));
		emit itemChanged(m_currentItem, m_curRow, m_curCol);
	}
	d->inside_acceptEditor = false;
	if (res == KexiValidator::Ok) {
		if (d->acceptsRowEditAfterCellAccepting || d->internal_acceptsRowEditAfterCellAccepting)
			acceptRowEdit();
		return true;
	}
	if (m_editor) {
		//allow to edit the cell again, (if m_editor is not cleared)
		startEditCurrentCell(newval.type()==QVariant::String ? newval.toString() : QString::null);
		m_editor->setFocus();
	}
	return false;
}

void KexiTableView::cancelEditor()
{
	if (!m_editor)
		return;

	removeEditor();
}

bool KexiTableView::acceptRowEdit()
{
	if (!m_rowEditing)
		return true;
	if (d->inside_acceptEditor) {
		d->internal_acceptsRowEditAfterCellAccepting = true;
		return true;
	}
	d->internal_acceptsRowEditAfterCellAccepting = false;
	if (!acceptEditor())
		return false;
	kdDebug() << "EDIT ROW ACCEPTING..." << endl;

	bool success = true;
//	bool allow = true;
//	int faultyColumn = -1; // will be !=-1 if cursor has to be moved to that column
	const bool inserting = m_newRowEditing;
//	QString msg, desc;
//	bool inserting = m_insertItem && m_insertItem==m_currentItem;

	if (m_data->rowEditBuffer()->isEmpty() && !m_newRowEditing) {
/*		if (m_newRowEditing) {
			cancelRowEdit();
			kdDebug() << "-- NOTHING TO INSERT!!!" << endl;
			return true;
		}
		else {*/
			kdDebug() << "-- NOTHING TO ACCEPT!!!" << endl;
//		}
	}
	else {//not empty edit buffer or new row to insert:
		if (m_newRowEditing) {
//			emit aboutToInsertRow(m_currentItem, m_data->rowEditBuffer(), success, &faultyColumn);
//			if (success) {
			kdDebug() << "-- INSERTING: " << endl;
			m_data->rowEditBuffer()->debug();
			success = m_data->saveNewRow(*m_currentItem);
//				if (!success) {
//				}
//			}
		}
		else {
//			emit aboutToUpdateRow(m_currentItem, m_data->rowEditBuffer(), success, &faultyColumn);
			if (success) {
				//accept changes for this row:
				kdDebug() << "-- UPDATING: " << endl;
				m_data->rowEditBuffer()->debug();
				success = m_data->saveRowChanges(*m_currentItem);//, &msg, &desc, &faultyColumn);
//				if (!success) {
//				}
			}
		}
	}

	if (success) {
		//editing is finished:
		m_rowEditing = false;
		m_newRowEditing = false;
		//indicate on the vheader that we are not editing
		m_verticalHeader->setEditRow(-1);
		//redraw
		updateRow(m_curRow);

		kdDebug() << "EDIT ROW ACCEPTED:" << endl;
		/*debug*/itemAt(m_curRow);

		if (inserting) {
//			emit rowInserted(m_currentItem);
			//update navigator's data
			m_navPanel->setRecordCount(rows());
		}
		else {
//			emit rowUpdated(m_currentItem);
		}

		emit rowEditTerminated(m_curRow);
	}
	else {
//		if (!allow) {
//			kdDebug() << "INSERT/EDIT ROW - DISALLOWED by signal!" << endl;
//		}
//		else {
//			kdDebug() << "EDIT ROW - ERROR!" << endl;
//		}
		if (m_data->result()->column>=0 && m_data->result()->column<columns()) {
			//move to faulty column
			setCursorPosition(m_curRow, m_data->result()->column);
		}
		if (m_data->result()->desc.isEmpty())
			KMessageBox::sorry(this, m_data->result()->msg);
		else
			KMessageBox::detailedSorry(this, m_data->result()->msg, m_data->result()->desc);

		//edit this cell
		startEditCurrentCell();
	}

	return success;
}
#endif //moved

//reimpl.
void KexiTableView::removeEditor()
{
	if (!m_editor)
		return;
	KexiDataAwareObjectInterface::removeEditor();

//moved	m_editor->hide();
//moved	m_editor = 0;
	viewport()->setFocus();
}

void KexiTableView::slotRowRepaintRequested(KexiTableItem& item)
{
	updateRow( m_data->findRef(&item) );
}

/*moved
void KexiTableView::cancelRowEdit()
{
	if (!hasData())
		return;
	if (!m_rowEditing)
		return;
	cancelEditor();
	m_rowEditing = false;
	//indicate on the vheader that we are not editing
	m_verticalHeader->setEditRow(-1);
	if (m_newRowEditing) {
		m_newRowEditing = false;
		//remove current edited row (it is @ the end of list)
		m_data->removeLast();
		//current item is now empty, last row
		m_currentItem = m_insertItem;
		//update visibility
		m_verticalHeader->removeLabel(false); //-1 label
//		updateContents(columnPos(0), rowPos(rows()), 
//			viewport()->width(), d->rowHeight*3 + (m_navPanel ? m_navPanel->height() : 0)*3 );
		updateContents(); //js: above didnt work well so we do that dirty
//TODO: still doesn't repaint properly!!
		QSize s(tableSize());
		resizeContents(s.width(), s.height());
		m_verticalHeader->update();
		//--no cancel action is needed for datasource, 
		//  because the row was not yet stored.
	}

	m_data->clearRowEditBuffer();
	updateRow(m_curRow);
	
//! \todo (js): cancel changes for this row!
	kdDebug(44021) << "EDIT ROW CANCELLED." << endl;

	emit rowEditTerminated(m_curRow);
}

bool KexiTableView::acceptsRowEditAfterCellAccepting() const
{
	return d->acceptsRowEditAfterCellAccepting;
}

void KexiTableView::setAcceptsRowEditAfterCellAccepting(bool set)
{
	d->acceptsRowEditAfterCellAccepting = set;
}

void KexiTableView::setDeletionPolicy(DeletionPolicy policy)
{
	d->deletionPolicy = policy;
//	updateContextMenu();
}

KexiTableView::DeletionPolicy KexiTableView::deletionPolicy() const
{
	return d->deletionPolicy;
}*/

#if 0
bool KexiTableView::updateContextMenu()
{
  // delete m_popup;
  //  m_popup = 0L;
//  m_popup->clear();
//	if(m_currentItem && m_currentItem->isInsertItem())
//	return;

//	if(d->additionPolicy != NoAdd || d->deletionPolicy != NoDelete)
//	{
//		m_popup = new QPopupMenu(this);
  m_popup->setItemVisible(d->menu_id_addRecord, d->additionPolicy != NoAdd);
#if 0 //todo(js)
  m_popup->setItemVisible(d->menu_id_removeRecord, d->deletionPolicy != NoDelete
	&& m_currentItem && !m_currentItem->isInsertItem());
#else
  m_popup->setItemVisible(d->menu_id_removeRecord, d->deletionPolicy != NoDelete
	&& m_currentItem);
#endif
  for (int i=0; i<(int)m_popup->count(); i++) {
	if (m_popup->isItemVisible( m_popup->idAt(i) ))
	  return true;
  }
  return false;
}
#endif

//(js) unused
void KexiTableView::slotAutoScroll()
{
	kdDebug(44021) << "KexiTableView::slotAutoScroll()" <<endl;
	if (!d->needAutoScroll)
		return;

	switch(d->scrollDirection)
	{
		case ScrollDown:
			setCursorPosition(m_curRow + 1, m_curCol);
			break;

		case ScrollUp:
			setCursorPosition(m_curRow - 1, m_curCol);
			break;
		case ScrollLeft:
			setCursorPosition(m_curRow, m_curCol - 1);
			break;

		case ScrollRight:
			setCursorPosition(m_curRow, m_curCol + 1);
			break;
	}
}

#ifndef KEXI_NO_PRINT
void
KexiTableView::print(KPrinter &/*printer*/)
{
//	printer.setFullPage(true);
#if 0
	int leftMargin = printer.margins().width() + 2 + d->rowHeight;
	int topMargin = printer.margins().height() + 2;
//	int bottomMargin = topMargin + ( printer.realPageSize()->height() * printer.resolution() + 36 ) / 72;
	int bottomMargin = 0;
	kdDebug(44021) << "KexiTableView::print: bottom = " << bottomMargin << endl;

	QPainter p(&printer);

	KexiTableItem *i;
	int width = leftMargin;
	for(int col=0; col < columns(); col++)
	{
		p.fillRect(width, topMargin - d->rowHeight, columnWidth(col), d->rowHeight, QBrush(gray));
		p.drawRect(width, topMargin - d->rowHeight, columnWidth(col), d->rowHeight);
		p.drawText(width, topMargin - d->rowHeight, columnWidth(col), d->rowHeight, AlignLeft | AlignVCenter, d->pTopHeader->label(col));
		width = width + columnWidth(col);
	}

	int yOffset = topMargin;
	int row = 0;
	int right = 0;
	for(i = m_data->first(); i; i = m_data->next())
	{
		if(!i->isInsertItem())
		{	kdDebug(44021) << "KexiTableView::print: row = " << row << " y = " << yOffset << endl;
			int xOffset = leftMargin;
			for(int col=0; col < columns(); col++)
			{
				kdDebug(44021) << "KexiTableView::print: col = " << col << " x = " << xOffset << endl;
				p.saveWorldMatrix();
				p.translate(xOffset, yOffset);
				paintCell(&p, i, col, QRect(0, 0, columnWidth(col) + 1, d->rowHeight), true);
				p.restoreWorldMatrix();
//			p.drawRect(xOffset, yOffset, columnWidth(col), d->rowHeight);
				xOffset = xOffset + columnWidth(col);
				right = xOffset;
			}

			row++;
			yOffset = topMargin  + row * d->rowHeight;
		}

		if(yOffset > 900)
		{
			p.drawLine(leftMargin, topMargin, leftMargin, yOffset);
			p.drawLine(leftMargin, topMargin, right - 1, topMargin);
			printer.newPage();
			yOffset = topMargin;
			row = 0;
		}
	}
	p.drawLine(leftMargin, topMargin, leftMargin, yOffset);
	p.drawLine(leftMargin, topMargin, right - 1, topMargin);

//	p.drawLine(60,60,120,150);
	p.end();
#endif
}
#endif

QString KexiTableView::columnCaption(int colNum) const
{
	return d->pTopHeader->label(colNum);
}

KexiDB::Field* KexiTableView::field(int colNum) const
{
	if (!m_data || !m_data->column(colNum))
		return 0;
	return m_data->column(colNum)->field();
}

void KexiTableView::adjustColumnWidthToContents(int colNum)
{
	if (!hasData())
		return;
	if (columns()<=colNum || colNum < -1)
		return;

	if (colNum==-1) {
//		const int cols = columns();
		for (int i=0; i<columns(); i++)
			adjustColumnWidthToContents(i);
		return;
	}

	KexiCellEditorFactoryItem *item = KexiCellEditorFactory::item( columnType(colNum) );
	if (!item)
		return;
	QFontMetrics fm(font());
	int maxw = fm.width( d->pTopHeader->label( colNum ) );
//	int start = rowAt(contentsY());
//	int end = QMAX( start, rowAt( contentsY() + viewport()->height() - 1 ) );
//	for (int i=start; i<=end; i++) {

//! \todo js: this is NOT EFFECTIVE for big data sets!!!!

	KexiTableEdit *ed = dynamic_cast<KexiTableEdit*>( editor( colNum ) );
//	KexiDB::Field *f = m_data->column( colNum )->field;
	if (ed) {
//		KexiDB::Field *f = m_data->column(colNum)->field;
		for (QPtrListIterator<KexiTableItem> it = m_data->iterator(); it.current(); ++it) {
			maxw = QMAX( maxw, ed->widthForValue( it.current()->at( colNum ), fm ) );
//			maxw = QMAX( maxw, item->widthForValue( *f, it.current()->at( colNum ), fm ) );
		}
		maxw += (fm.width("  ") + ed->leftMargin() + ed->rightMargin());
	}
	if (maxw < KEXITV_MINIMUM_COLUMN_WIDTH )
		maxw = KEXITV_MINIMUM_COLUMN_WIDTH; //not too small
	setColumnWidth( colNum, maxw );
}

void KexiTableView::setColumnWidth(int colNum, int width)
{
	if (columns()<=colNum || colNum < 0)
		return;
	const int oldWidth = d->pTopHeader->sectionSize( colNum );
	d->pTopHeader->resizeSection( colNum, width );
	slotTopHeaderSizeChange( colNum, oldWidth, d->pTopHeader->sectionSize( colNum ) );
}

void KexiTableView::maximizeColumnsWidth( const QValueList<int> &columnList )
{
	if (!isVisible()) {
		d->maximizeColumnsWidthOnShow += columnList;
		return;
	}
	if (width() <= d->pTopHeader->headerWidth())
		return;
	//sort the list and make it unique
	QValueList<int>::const_iterator it;
	QValueList<int> cl, sortedList = columnList;
	qHeapSort(sortedList);
	int i=-999;

	for (it=sortedList.constBegin(); it!=sortedList.end(); ++it) {
		if (i!=(*it)) {
			cl += (*it);
			i = (*it);
		}
	}
	//resize
	int sizeToAdd = (width() - d->pTopHeader->headerWidth()) / cl.count() - verticalHeader()->width();
	if (sizeToAdd<=0)
		return;
	for (it=cl.constBegin(); it!=cl.end(); ++it) {
		int w = d->pTopHeader->sectionSize(*it);
		if (w>0) {
			d->pTopHeader->resizeSection(*it, w+sizeToAdd);
		}
	}
	updateContents();
	editorShowFocus( m_curRow, m_curCol );
}

void KexiTableView::adjustHorizontalHeaderSize()
{
	d->pTopHeader->adjustHeaderSize();
}

void KexiTableView::setColumnStretchEnabled( bool set, int colNum )
{
	d->pTopHeader->setStretchEnabled( set, colNum );
}

/*moved
int KexiTableView::currentColumn() const
{
	return m_curCol;
}

int KexiTableView::currentRow() const
{
	return m_curRow;
}

KexiTableItem *KexiTableView::selectedItem() const
{
	return m_currentItem;
}*/

//void KexiTableView::setBackgroundAltering(bool altering) { d->bgAltering = altering; }
//bool KexiTableView::backgroundAltering()  const { return d->bgAltering; }

void KexiTableView::setEditableOnDoubleClick(bool set) { d->editOnDoubleClick = set; }
bool KexiTableView::editableOnDoubleClick() const { return d->editOnDoubleClick; }

/*void KexiTableView::setEmptyAreaColor(const QColor& c)
{
	d->emptyAreaColor = c;
}

QColor KexiTableView::emptyAreaColor() const
{
	return d->emptyAreaColor;
}

bool KexiTableView::fullRowSelectionEnabled() const
{
	return d->fullRowSelectionEnabled;
}*/
/*
void KexiTableView::setFullRowSelectionEnabled(bool set)
{
	if (d->fullRowSelectionEnabled == set)
		return;
	if (set) {
		d->rowHeight -= 1;
	}
	else {
		d->rowHeight += 1;
	}
	d->fullRowSelectionEnabled = set;
	if (m_verticalHeader)
		m_verticalHeader->setCellHeight(d->rowHeight);
	if (d->pTopHeader) {
		setMargins(
			QMIN(d->pTopHeader->sizeHint().height(), d->rowHeight),
			d->pTopHeader->sizeHint().height(), 0, 0);
	}
	setFont(font());//update
}
*/
bool KexiTableView::verticalHeaderVisible() const
{
	return m_verticalHeader->isVisible();
}

void KexiTableView::setVerticalHeaderVisible(bool set)
{
	int left_width;
	if (set) {
		m_verticalHeader->show();
		left_width = QMIN(d->pTopHeader->sizeHint().height(), d->rowHeight);
	}
	else {
		m_verticalHeader->hide();
		left_width = 0;
	}
	setMargins( left_width, horizontalHeaderVisible() ? d->pTopHeader->sizeHint().height() : 0, 0, 0);
}

bool KexiTableView::horizontalHeaderVisible() const
{
	return d->pTopHeader->isVisible();
}

void KexiTableView::setHorizontalHeaderVisible(bool set)
{
	int top_height;
	if (set) {
		d->pTopHeader->show();
		top_height = d->pTopHeader->sizeHint().height();
	}
	else {
		d->pTopHeader->hide();
		top_height = 0;
	}
	setMargins( verticalHeaderVisible() ? m_verticalHeader->width() : 0, top_height, 0, 0);
}

void KexiTableView::triggerUpdate()
{
//	kdDebug(44021) << "KexiTableView::triggerUpdate()" << endl;
//	if (!d->pUpdateTimer->isActive())
		d->pUpdateTimer->start(20, true);
//		d->pUpdateTimer->start(200, true);
}

/*moved
int KexiTableView::columnType(int col) const
{
	return (m_data && col>=0 && col<columns()) ? m_data->column(col)->field()->type() : KexiDB::Field::InvalidType;
}

bool KexiTableView::columnEditable(int col) const
{
	return (m_data && col>=0 && col<columns()) ? !m_data->column(col)->readOnly() : false;
}

QVariant KexiTableView::columnDefaultValue(int col) const
{
	return QVariant(0);
//TODO(js)	
//	return m_data->columns[col].defaultValue;
}

void KexiTableView::setReadOnly(bool set)
{
	if (isReadOnly() == set || (m_data && m_data->isReadOnly() && !set))
		return; //not allowed!
	d->readOnly = (set ? 1 : 0);
	if (set)
		setInsertingEnabled(false);
	update();
	emit reloadActions();
}

bool KexiTableView::isReadOnly() const
{
	if (d->readOnly == 1 || d->readOnly == 0)
		return (bool)d->readOnly;
	if (!hasData())
		return true;
	return m_data->isReadOnly();
}

void KexiTableView::setInsertingEnabled(bool set)
{
	if (isInsertingEnabled() == set || (m_data && !m_data->isInsertingEnabled() && set))
		return; //not allowed!
	d->insertingEnabled = (set ? 1 : 0);
	m_navPanel->setInsertingEnabled(set);
	m_verticalHeader->showInsertRow(set);
	if (set)
		setReadOnly(false);
	update();
	emit reloadActions();
}

bool KexiTableView::isInsertingEnabled() const
{
	if (d->insertingEnabled == 1 || d->insertingEnabled == 0)
		return (bool)d->insertingEnabled;
	CHECK_DATA(true);
	return m_data->isInsertingEnabled();
}

bool KexiTableView::isEmptyRowInsertingEnabled() const
{
	return d->emptyRowInsertingEnabled;//js && isInsertingEnabled();
}

void KexiTableView::setEmptyRowInsertingEnabled(bool set)
{
	d->emptyRowInsertingEnabled = set;
	emit reloadActions();
}

bool KexiTableView::isDeleteEnabled() const
{
	return d->deletionPolicy != NoDelete && !isReadOnly();
}

bool KexiTableView::rowEditing() const
{
	return m_rowEditing;
}
bool KexiTableView::contextMenuEnabled() const
{
	return d->contextMenuEnabled;
}

void KexiTableView::setContextMenuEnabled(bool set)
{
	d->contextMenuEnabled = set;
}
*/

void KexiTableView::setHBarGeometry( QScrollBar & hbar, int x, int y, int w, int h )
{
/*todo*/
	kdDebug(44021)<<"KexiTableView::setHBarGeometry"<<endl;
	if (d->appearance.navigatorEnabled) {
		m_navPanel->setHBarGeometry( hbar, x, y, w, h );
	}
	else {
		hbar.setGeometry( x , y, w, h );
	}
}

/*moved
void KexiTableView::setFilteringEnabled(bool set)
{
	d->isFilteringEnabled = set;
}

bool KexiTableView::filteringEnabled() const
{
	return d->isFilteringEnabled;
}

void KexiTableView::setSpreadSheetMode()
{
	d->spreadSheetMode = true;
	Appearance a = d->appearance;
	setSortingEnabled( false );
	setInsertingEnabled( false );
	setAcceptsRowEditAfterCellAccepting( true );
	setFilteringEnabled( false );
	setEmptyRowInsertingEnabled( true );
	a.navigatorEnabled = false;
	setAppearance( a );
}

bool KexiTableView::spreadSheetMode() const
{
	return d->spreadSheetMode;
}
*/

void KexiTableView::setSpreadSheetMode()
{
	KexiDataAwareObjectInterface::setSpreadSheetMode();
	//copy m_navPanelEnabled flag
	Appearance a = d->appearance;
	a.navigatorEnabled = m_navPanelEnabled;
	setAppearance( a );
}

bool KexiTableView::scrollbarToolTipsEnabled() const
{
	return d->scrollbarToolTipsEnabled;
}

void KexiTableView::setScrollbarToolTipsEnabled(bool set)
{
	d->scrollbarToolTipsEnabled=set;
}

int KexiTableView::validRowNumber(const QString& text)
{
	bool ok=true;
	int r = text.toInt(&ok);
	if (!ok || r<1)
		r = 1;
	else if (r > (rows()+(isInsertingEnabled()?1:0)))
		r = rows()+(isInsertingEnabled()?1:0);
	return r-1;
}

void KexiTableView::moveToRecordRequested( uint r )
{
	r--;
	if (r > uint(rows()+(isInsertingEnabled()?1:0)))
		r = rows()+(isInsertingEnabled()?1:0);
	setFocus();
	selectRow( r );
}

void KexiTableView::moveToLastRecordRequested()
{
	setFocus();
	selectRow(rows()>0 ? (rows()-1) : 0);
}

void KexiTableView::moveToPreviousRecordRequested()
{
	setFocus();
	selectPrevRow();
}

void KexiTableView::moveToNextRecordRequested()
{
	setFocus();
	selectNextRow();
}

void KexiTableView::moveToFirstRecordRequested()
{
	setFocus();
	selectFirstRow();
}

void KexiTableView::addNewRecordRequested()
{
	if (!isInsertingEnabled())
		return;
	if (m_rowEditing) {
		if (!acceptRowEdit())
			return;
	}
/*	if (m_editor) {
		//already editing!
		m_editor->setFocus();
		return;
	}*/
	setFocus();
	selectRow(rows());
	startEditCurrentCell();
}

bool KexiTableView::eventFilter( QObject *o, QEvent *e )
{
	//don't allow to stole key my events by others:
//	kdDebug() << "spontaneous " << e->spontaneous() << " type=" << e->type() << endl;

	if (e->type()==QEvent::KeyPress) {
		if (e->spontaneous() /*|| e->type()==QEvent::AccelOverride*/) {
			QKeyEvent *ke = static_cast<QKeyEvent*>(e);
			const int k = ke->key();
			int s = ke->state();
			//cell editor's events:
			//try to handle the event @ editor's level
			KexiTableEdit *edit = dynamic_cast<KexiTableEdit*>( editor( m_curCol ) );
			if (edit && edit->handleKeyPress(ke, m_editor==edit)) {
				ke->accept();
				return true;
			}
			else if (m_editor && (o==dynamic_cast<QObject*>(m_editor) || o==m_editor->widget())) {
				if ( (k==Key_Tab && (s==NoButton || s==ShiftButton))
					|| (overrideEditorShortcutNeeded(ke))
					|| (k==Key_Enter || k==Key_Return || k==Key_Up || k==Key_Down) 
					|| (k==Key_Left && m_editor->cursorAtStart())
					|| (k==Key_Right && m_editor->cursorAtEnd())
					)
				{
					//try to steal the key press from editor or it's internal widget...
					keyPressEvent(ke);
					if (ke->isAccepted())
						return true;
				}
			}
			/*
			else if (e->type()==QEvent::KeyPress && (o==this || (m_editor && o==m_editor->widget()))){//|| o==viewport())
				keyPressEvent(ke);
				if (ke->isAccepted())
					return true;
			}*/
/*todo			else if ((k==Key_Tab || k==(SHIFT|Key_Tab)) && o==d->navRowNumber) {
				//tab key focuses tv
				ke->accept();
				setFocus();
				return true;
			}*/
		}
	}
	else if (o==horizontalScrollBar()) {
		if ((e->type()==QEvent::Show && !horizontalScrollBar()->isVisible()) 
			|| (e->type()==QEvent::Hide && horizontalScrollBar()->isVisible())) {
			updateWidgetContentsSize();
		}
	}
	else if (e->type()==QEvent::Leave) {
		if (o==viewport() && d->appearance.rowHighlightingEnabled) {
			if (d->highlightedRow>=0)
				updateRow(d->highlightedRow);
			d->highlightedRow = -1;
		}
	}
/*	else if (e->type()==QEvent::FocusOut && o->inherits("QWidget")) {
		//hp==true if currently focused widget is a child of this table view
		const bool hp = Kexi::hasParent( static_cast<QWidget*>(o), focusWidget());
		if (!hp && Kexi::hasParent( this, static_cast<QWidget*>(o))) {
			//accept row editing if focus is moved to foreign widget 
			//(not a child, like eg. editor) from one of our table view's children
			//or from table view itself
			if (!acceptRowEdit()) {
				static_cast<QWidget*>(o)->setFocus();
				return true;
			}
		}
	}*/
	return QScrollView::eventFilter(o,e);
}

void KexiTableView::vScrollBarValueChanged(int v)
{
	if (!d->vScrollBarValueChanged_enabled)
		return;
	kdDebug(44021) << "VCHANGED: " << v << " / " << horizontalScrollBar()->maxValue() <<  endl;
	
//	updateContents();
	m_verticalHeader->update(); //<-- dirty but needed

	if (d->scrollbarToolTipsEnabled) {
		QRect r = verticalScrollBar()->sliderRect();
		int row = rowAt(contentsY())+1;
		if (row<=0) {
			d->scrollBarTipTimer.stop();
			d->scrollBarTip->hide();
			return;
		}
		d->scrollBarTip->setText( i18n("Row: ") + QString::number(row) );
		d->scrollBarTip->adjustSize();
		d->scrollBarTip->move( 
		 mapToGlobal( r.topLeft() + verticalScrollBar()->pos() ) + QPoint( - d->scrollBarTip->width()-5, r.height()/2 - d->scrollBarTip->height()/2) );
		if (verticalScrollBar()->draggingSlider()) {
			kdDebug(44021) << "  draggingSlider()  " << endl;
			d->scrollBarTipTimer.stop();
			d->scrollBarTip->show();
			d->scrollBarTip->raise();
		}
		else {
			d->scrollBarTipTimerCnt++;
			if (d->scrollBarTipTimerCnt>4) {
				d->scrollBarTipTimerCnt=0;
				d->scrollBarTip->show();
				d->scrollBarTip->raise();
				d->scrollBarTipTimer.start(500, true);
			}
		}
	}
	//update bottom view region
	if (m_navPanel && (contentsHeight() - contentsY() - clipper()->height()) <= QMAX(d->rowHeight,m_navPanel->height())) {
		slotUpdate();
		triggerUpdate();
	}
}

void KexiTableView::vScrollBarSliderReleased()
{
	kdDebug(44021) << "vScrollBarSliderReleased()" << endl;
	d->scrollBarTip->hide();
}

void KexiTableView::scrollBarTipTimeout()
{
	if (d->scrollBarTip->isVisible()) {
		kdDebug(44021) << "TIMEOUT! - hide" << endl;
		if (d->scrollBarTipTimerCnt>0) {
			d->scrollBarTipTimerCnt=0;
			d->scrollBarTipTimer.start(500, true);
			return;
		}
		d->scrollBarTip->hide();
	}
	d->scrollBarTipTimerCnt=0;
}

void KexiTableView::slotTopHeaderSizeChange( 
	int /*section*/, int /*oldSize*/, int /*newSize*/ )
{
	editorShowFocus( m_curRow, m_curCol );
}

/*moved
const QVariant* KexiTableView::bufferedValueAt(int col)
{
	if (m_rowEditing && m_data->rowEditBuffer())
	{
		KexiTableViewColumn* tvcol = m_data->column(col);
		if (tvcol->isDBAware) {
//			QVariant *cv = m_data->rowEditBuffer()->at( *static_cast<KexiDBTableViewColumn*>(tvcol)->field );
			const QVariant *cv = m_data->rowEditBuffer()->at( *tvcol->fieldinfo );
			if (cv)
				return cv;

			return &m_currentItem->at(col);
		}
		const QVariant *cv = m_data->rowEditBuffer()->at( tvcol->field()->name() );
		if (cv)
			return cv;
	}
	return &m_currentItem->at(col);
}
*/

void KexiTableView::setBottomMarginInternal(int pixels)
{
	d->internal_bottomMargin = pixels;
}

void KexiTableView::paletteChange( const QPalette & )
{
}

KexiTableView::Appearance KexiTableView::appearance() const
{
	return d->appearance;
}

void KexiTableView::setAppearance(const Appearance& a)
{
//	if (d->appearance.fullRowSelection != a.fullRowSelection) {
	if (a.fullRowSelection) {
		d->rowHeight -= 1;
	}
	else {
		d->rowHeight += 1;
	}
	if (m_verticalHeader)
		m_verticalHeader->setCellHeight(d->rowHeight);
	if (d->pTopHeader) {
		setMargins(
			QMIN(d->pTopHeader->sizeHint().height(), d->rowHeight),
			d->pTopHeader->sizeHint().height(), 0, 0);
	}
//	}

	if(!a.navigatorEnabled)
		m_navPanel->hide();
	else
		m_navPanel->show();
//	}

	d->highlightedRow = -1;
//TODO is setMouseTracking useful for other purposes?
	viewport()->setMouseTracking(a.rowHighlightingEnabled);

	d->appearance = a;

	setFont(font()); //this also updates contents
}

int KexiTableView::highlightedRow() const
{
	return d->highlightedRow;
}

void KexiTableView::setHighlightedRow(int row)
{
	if (row!=-1) {
		row = QMAX( 0, QMIN(rows()-1, row) );
		ensureCellVisible(row, -1);
	}
	updateRow(d->highlightedRow);
	if (d->highlightedRow == row)
		return;
	d->highlightedRow = row;
	if (d->highlightedRow!=-1)
		updateRow(d->highlightedRow);
}

KexiTableItem *KexiTableView::highlightedItem() const
{
	return m_data->at(d->highlightedRow);
}

void KexiTableView::slotSettingsChanged(int category)
{
	if (category==KApplication::SETTINGS_SHORTCUTS) {
		d->contextMenuKey = KGlobalSettings::contextMenuKey();
	}
}


#include "kexitableview.moc"

