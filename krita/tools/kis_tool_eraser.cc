/*
 *  kis_tool_eraser.cc - part of KimageShop^WKrayon^WKrita
 *
 *  Copyright (c) 1999 Matthias Elter <me@kde.org>
 *  Copyright (c) 2002 Patrick Julien <freak@codepimps.org>
 *  Copyright (c) 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include <kaction.h>
#include <kdebug.h>
#include <kcommand.h>
#include <klocale.h>

#include "kis_cursor.h"
#include "kis_doc.h"
#include "kis_painter.h"

#include "kis_tool_eraser.h"
#include "kis_vec.h"
#include "kis_view.h"
#include "kis_button_press_event.h"
#include "kis_button_release_event.h"
#include "kis_move_event.h"

KisToolEraser::KisToolEraser()
	: super(),
	  m_mode ( HOVER),
	  m_dragDist ( 0 )
{
	setName("tool_eraser");
	setCursor(KisCursor::eraserCursor());

	m_painter = 0;
	m_currentImage = 0;
}

KisToolEraser::~KisToolEraser()
{
}


void KisToolEraser::update(KisCanvasSubject *subject)
{
	m_subject = subject;
	m_currentImage = subject -> currentImg();

	super::update(m_subject);
}


void KisToolEraser::buttonPress(KisButtonPressEvent *e)
{
        if (!m_subject) return;

        if (!m_subject->currentBrush()) return;

        if (e -> button() == QMouseEvent::LeftButton) {
		m_mode = ERASE;
		initErase(e -> pos());
		m_painter -> eraseAt(e->pos(), e -> pressure(), e -> xTilt(), e -> yTilt());
		// XXX: get the rect that should be notified
		m_currentImage -> notify( m_painter -> dirtyRect() );
         }
}

void KisToolEraser::move(KisMoveEvent *e) {
	if (m_mode == ERASE) {
		eraseLine(m_dragStart, e -> pos(), e -> pressure(), e -> xTilt(), e -> yTilt());
	}
}

void KisToolEraser::buttonRelease(KisButtonReleaseEvent *e) {
	if (e -> button() == QMouseEvent::LeftButton && m_mode == ERASE) {
		endErase();
        }

}
#if 0
void KisToolEraser::tabletEvent(QTabletEvent *e) {
         if (e->device() == QTabletEvent::Eraser) {
		 if (!m_subject) {
			 e -> accept();
			 return;
		 }

		 if (!m_subject -> currentBrush()) {
			 e->accept();
			 return;
		 }

		 double pressure = e -> pressure() / 255.0;

		 if (pressure < PRESSURE_THRESHOLD && m_mode == ERASE_STYLUS) {
			 endErase();
		 }
		 else if (pressure >= PRESSURE_THRESHOLD && m_mode == HOVER) {
			 m_mode = ERASE_STYLUS;
			 initErase(e -> pos());
			 m_painter -> eraseAt(e -> pos(), pressure, e->xTilt(), e->yTilt());
			 // XXX: Get the rect that should be updated
			 m_currentImage -> notify( m_painter -> dirtyRect() );

		 }
		 else if (pressure >= PRESSURE_THRESHOLD && m_mode == ERASE_STYLUS) {
			 eraseLine(m_dragStart, e -> pos(), pressure, e -> xTilt(), e -> yTilt());
		 }
         }
	 e -> accept();
}
#endif
void KisToolEraser::initErase(const KisPoint & pos) {
	m_dragStart = pos;
	m_dragDist = 0;

	// Create painter
	KisPaintDeviceSP device;
	if (m_currentImage && (device = m_currentImage -> activeDevice())) {
            delete m_painter;
		m_painter = new KisPainter( device );
		m_painter -> beginTransaction(i18n("erase"));
	}

	m_painter -> setPaintColor(m_subject -> fgColor());
	m_painter -> setBackgroundColor(m_subject -> bgColor());
	m_painter -> setBrush(m_subject -> currentBrush());

}

void KisToolEraser::endErase() {
	m_mode = HOVER;
	KisPaintDeviceSP device;
	if (m_currentImage && (device = m_currentImage -> activeDevice())) {
		KisUndoAdapter *adapter = m_currentImage -> undoAdapter();
		if (adapter && m_painter) {
			// If painting in mouse release, make sure painter
			// is destructed or end()ed
			adapter -> addCommand(m_painter->endTransaction());
		}
		delete m_painter;
		m_painter = 0;
	}
}

void KisToolEraser::eraseLine(const KisPoint & pos1,
			      const KisPoint & pos2,
			      const double pressure,
			      const double xtilt,
			      const double ytilt)
{
	m_dragDist = m_painter -> paintLine(PAINTOP_ERASE, pos1, pos2, pressure, xtilt, ytilt, m_dragDist);
	m_currentImage -> notify( m_painter -> dirtyRect() );
	m_dragStart = pos2;
}

void KisToolEraser::setup(KActionCollection *collection)
{
	m_action = static_cast<KRadioAction *>(collection -> action(name()));

	if (m_action == 0) {
		m_action = new KRadioAction(i18n("&Eraser Tool"),
					    "eraser", 0, this,
					    SLOT(activate()), collection,
					    name());
		m_action -> setExclusiveGroup("tools");
		m_ownAction = true;
	}
}

#include "kis_tool_eraser.moc"
