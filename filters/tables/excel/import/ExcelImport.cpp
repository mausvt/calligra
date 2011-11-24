/* This file is part of the KDE project
   Copyright (C) 2003-2006 Ariya Hidayat <ariya@kde.org>
   Copyright (C) 2006 Marijn Kruisselbrink <mkruisselbrink@kde.org>
   Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
   Contact: Manikandaprasad Chandrasekar <manikandaprasad.chandrasekar@nokia.com>
   Copyright (c) 2010 Carlos Licea <carlos@kdab.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "ExcelImport.h"
#include <ExcelImport.moc>

#include <QString>
#include <QDate>
#include <QBuffer>
#include <QFontMetricsF>
#include <QPair>
#include <QTextCursor>

#include <kdebug.h>
#include <KoFilterChain.h>
#include <KoGlobal.h>
#include <KoUnit.h>
#include <kpluginfactory.h>

#include <KoXmlWriter.h>
#include <KoOdfWriteStore.h>
#include <KoGenStyles.h>
#include <KoGenStyle.h>
#include <KoOdfNumberStyles.h>
#include <KoXmlNS.h>
#include <KoShapeLoadingContext.h>
#include <KoShapeRegistry.h>
#include <KoOdfStylesReader.h>
#include <KoOdfLoadingContext.h>
#include <KoShape.h>
#include <KoDocumentInfo.h>
#include <KoTextDocument.h>

#include <DocBase.h>
#include <tables/Sheet.h>
#include <CalculationSettings.h>
#include <CellStorage.h>
#include <HeaderFooter.h>
#include <LoadingInfo.h>
#include <Map.h>
#include <NamedAreaManager.h>
#include <RowColumnFormat.h>
#include <RowFormatStorage.h>
#include <Sheet.h>
#include <SheetPrint.h>
#include <Style.h>
#include <StyleManager.h>
#include <StyleStorage.h>
#include <ValueConverter.h>
#include <ShapeApplicationData.h>
#include <Util.h>

#include <Charting.h>
#include <ChartExport.h>
#include <NumberFormatParser.h>

#include <iostream>

#include "swinder.h"
#include "objects.h"
#include "ODrawClient.h"
#include "ImportUtils.h"
#include "conditionals.h"

// Enable this definition to make the filter output to an ods file instead of
// using m_chain.outputDocument() to write the spreadsheet to.
//#define OUTPUT_AS_ODS_FILE

K_PLUGIN_FACTORY(ExcelImportFactory, registerPlugin<ExcelImport>();)
K_EXPORT_PLUGIN(ExcelImportFactory("calligrafilters"))

static const qreal SIDEWINDERPROGRESS = 40.0;
static const qreal ODFPROGRESS = 40.0;
static const qreal EMBEDDEDPROGRESS = 15.0;

using namespace Swinder;
using namespace XlsUtils;

static qreal offset( unsigned long dimension, unsigned long offset, qreal factor ) {
    return (float)dimension * (float)offset / factor;
}

static qreal columnWidth(Sheet* sheet, unsigned long col) {
    if( sheet->column(col, false) )
        return sheet->column(col)->width();

    return sheet->defaultColWidth();
}

static qreal rowHeight(Sheet* sheet, unsigned long row) {
    if( sheet->row(row, false) )
        return sheet->row(row)->height();

    return sheet->defaultRowHeight();
}

class ExcelImport::Private
{
public:
    Private(ExcelImport *q)
    : q(q)
    {
    }

    QString inputFile;
    Calligra::Tables::DocBase* outputDoc;

    Workbook *workbook;

    // for embedded shapes
    KoStore* storeout;
    KoGenStyles *shapeStyles;
    KoGenStyles *dataStyles;
    KoXmlWriter *shapesXml;

    void processMetaData();
    void processSheet(Sheet* isheet, Calligra::Tables::Sheet* osheet);
    void processSheetForHeaderFooter(Sheet* isheet, Calligra::Tables::Sheet* osheet);
    void processSheetForFilters(Sheet* isheet, Calligra::Tables::Sheet* osheet);
    void processSheetForConditionals(Sheet* isheet, Calligra::Tables::Sheet* osheet);
    void processColumn(Sheet* isheet, unsigned column, Calligra::Tables::Sheet* osheet);
    void processRow(Sheet* isheet, unsigned row, Calligra::Tables::Sheet* osheet);
    void processCell(Cell* icell, Calligra::Tables::Cell ocell);
    void processCellObjects(Cell* icell, Calligra::Tables::Cell ocell);
    void processEmbeddedObjects(const KoXmlElement& rootElement, KoStore* store);
    void processNumberFormats();

    QString convertHeaderFooter(const QString& xlsHeader);

    int convertStyle(const Format* format, const QString& formula = QString());
    QHash<CellFormatKey, int> styleCache;
    QList<Calligra::Tables::Style> styleList;
    QHash<QString, Calligra::Tables::Style> dataStyleCache;
    QHash<QString, Calligra::Tables::Conditions> dataStyleConditions;

    void processFontFormat(const FormatFont& font, Calligra::Tables::Style& style);
    QTextCharFormat convertFontToCharFormat(const FormatFont& font);
    QPen convertBorder(const Pen& pen);

    int rowsCountTotal, rowsCountDone;
    void addProgress(int addValue);

    QHash<int, QRegion> cellStyles;
    QHash<int, QRegion> rowStyles;
    QHash<int, QRegion> columnStyles;
    QList<QPair<QRegion, Calligra::Tables::Conditions> > cellConditions;

    QList<ChartExport*> charts;
    void processCharts(KoXmlWriter* manifestWriter);

    void addManifestEntries(KoXmlWriter* ManifestWriter);
    void insertPictureManifest(const QString& fileName);
    QMap<QString,QString> manifestEntries;

    KoXmlWriter* beginMemoryXmlWriter(const char* docElement);
    KoXmlDocument endMemoryXmlWriter(KoXmlWriter* writer);

    ExcelImport *q;

};

ExcelImport::ExcelImport(QObject* parent, const QVariantList&)
        : KoFilter(parent)
{
    d = new Private(this);
    d->storeout = 0;
}

ExcelImport::~ExcelImport()
{
    delete d->storeout;
    delete d;
}

KoFilter::ConversionStatus ExcelImport::convert(const QByteArray& from, const QByteArray& to)
{
    if (from != "application/vnd.ms-excel")
        return KoFilter::NotImplemented;

    if (to != "application/vnd.oasis.opendocument.spreadsheet")
        return KoFilter::NotImplemented;

    d->inputFile = m_chain->inputFile();

#ifndef OUTPUT_AS_ODS_FILE
    KoDocument* document = m_chain->outputDocument();
    if (!document)
        return KoFilter::StupidError;

    d->outputDoc = qobject_cast<Calligra::Tables::DocBase*>(document);
    if (!d->outputDoc) {
        kWarning() << "document isn't a Calligra::Tables::Doc but a " << document->metaObject()->className();
        return KoFilter::WrongFormat;
    }
#else
    d->outputDoc = new Calligra::Tables::DocBase();
#endif
    d->outputDoc->setOutputMimeType(to);

    emit sigProgress(0);


    QBuffer storeBuffer; // TODO: use temporary file instead
    delete d->storeout;
    d->storeout = KoStore::createStore(&storeBuffer, KoStore::Write);
    d->storeout->disallowNameExpansion();

    // open inputFile
    d->workbook = new Swinder::Workbook(d->storeout);
    connect(d->workbook, SIGNAL(sigProgress(int)), this, SLOT(slotSigProgress(int)));
    if (!d->workbook->load(d->inputFile.toLocal8Bit())) {
        delete d->workbook;
        d->workbook = 0;
        delete d->storeout;
        d->storeout = 0;
        return KoFilter::InvalidFormat;
    }

    if (d->workbook->isPasswordProtected()) {
        delete d->workbook;
        d->workbook = 0;
        delete d->storeout;
        d->storeout = 0;
        return KoFilter::PasswordProtected;
    }

    emit sigProgress(-1);
    emit sigProgress(0);

    // count the number of rows in total to provide a good progress value
    d->rowsCountTotal = d->rowsCountDone = 0;
    for (unsigned i = 0; i < d->workbook->sheetCount(); i++) {
        Sheet* sheet = d->workbook->sheet(i);
        d->rowsCountTotal += qMin(maximalRowCount, sheet->maxRow());
    }

    d->shapeStyles = new KoGenStyles();
    d->dataStyles = new KoGenStyles();

    // convert number formats
    d->processNumberFormats();

    d->processMetaData();

    d->shapesXml = d->beginMemoryXmlWriter("table:shapes");

    Calligra::Tables::Map* map = d->outputDoc->map();
    for (unsigned i = 0; i < d->workbook->sheetCount(); i++) {
        d->shapesXml->startElement("table:table");
        d->shapesXml->addAttribute("table:id", i);
        Sheet* sheet = d->workbook->sheet(i);
        if (i == 0) {
            map->setDefaultColumnWidth(sheet->defaultColWidth());
            map->setDefaultRowHeight(sheet->defaultRowHeight());
        }
        Calligra::Tables::Sheet* ksheet = map->addNewSheet(sheet->name());
        d->processSheet(sheet, ksheet);
        d->shapesXml->endElement();
    }

    // named expressions
    const std::map<std::pair<unsigned, QString>, QString>& namedAreas = d->workbook->namedAreas();
    for (std::map<std::pair<unsigned, QString>, QString>::const_iterator it = namedAreas.begin(); it != namedAreas.end(); ++it) {
        QString range = it->second;
        if(range.startsWith('[') && range.endsWith(']'))
            range = range.mid(1, range.length() - 2);
        Calligra::Tables::Region region(Calligra::Tables::Region::loadOdf(range), d->outputDoc->map());
        if (!region.isValid() || !region.lastSheet()) {
            kDebug() << "invalid area" << range;
            continue;
        }
        d->outputDoc->map()->namedAreaManager()->insert(region, it->first.second);
    }

    QBuffer manifestBuffer;
    KoXmlWriter manifestWriter(&manifestBuffer);
    manifestWriter.startDocument("manifest:manifest");
    manifestWriter.startElement("manifest:manifest");
    manifestWriter.addAttribute("xmlns:manifest", KoXmlNS::manifest);
    manifestWriter.addManifestEntry("/", "application/vnd.oasis.opendocument.spreadsheet");

    d->processCharts(&manifestWriter);
    d->addManifestEntries(&manifestWriter);

    manifestWriter.endElement();
    manifestWriter.endDocument();
    if (d->storeout->open("META-INF/manifest.xml")) {
        d->storeout->write(manifestBuffer.buffer());
        d->storeout->close();
    }

    delete d->storeout;
    d->storeout = 0;
    storeBuffer.close();

    KoStore *store = KoStore::createStore(&storeBuffer, KoStore::Read);
    store->disallowNameExpansion();

    // Debug odf for shapes
#if 0
    d->shapesXml->endElement();
    d->shapesXml->endDocument();

    d->shapesXml->device()->seek(0);

    QTextStream input(d->shapesXml->device());
    qDebug() << "-- START SHAPES_XML -- size : " << d->shapesXml->device()->size();
    qDebug() << input.readAll();
    qDebug() << "-- SHAPES_XML --";
#endif

    KoXmlDocument xmlDoc = d->endMemoryXmlWriter(d->shapesXml);

    d->processEmbeddedObjects(xmlDoc.documentElement(), store);

    // sheet background images
    for (unsigned i = 0; i < d->workbook->sheetCount(); i++) {
        Sheet* sheet = d->workbook->sheet(i);
        Calligra::Tables::Sheet* ksheet = map->sheet(i);
        kDebug() << i << sheet->backgroundImage();
        if (sheet->backgroundImage().isEmpty()) continue;

        QByteArray data;
        store->extractFile(sheet->backgroundImage(), data);
        QImage image = QImage::fromData(data);
        if (image.isNull()) continue;

        ksheet->setBackgroundImage(image);
        ksheet->setBackgroundImageProperties(Calligra::Tables::Sheet::BackgroundImageProperties());
    }

#ifndef OUTPUT_AS_ODS_FILE
    d->outputDoc->map()->completeLoading(store);
#endif

    delete store;

    // ensure at least one sheet
    if (d->outputDoc->map()->count() == 0) {
        d->outputDoc->map()->addNewSheet();
    }

    // active sheet
    kDebug() << "ACTIVE " << d->workbook->activeTab();
    d->outputDoc->map()->loadingInfo()->setInitialActiveSheet(d->outputDoc->map()->sheet(d->workbook->activeTab()));

#ifdef OUTPUT_AS_ODS_FILE
    d->outputDoc->saveNativeFormat(m_chain->outputFile());
    delete d->outputDoc;
#endif

    delete d->workbook;
    delete d->shapeStyles;
    delete d->dataStyles;
    d->inputFile.clear();
    d->outputDoc = 0;
    d->shapesXml = 0;


    emit sigProgress(100);
    return KoFilter::OK;
}

void ExcelImport::Private::processMetaData()
{
    KoDocumentInfo* info = outputDoc->documentInfo();

    if (workbook->hasProperty(Workbook::PIDSI_TITLE)) {
        info->setAboutInfo("title", workbook->property(Workbook::PIDSI_TITLE).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_SUBJECT)) {
        info->setAboutInfo("subject", workbook->property(Workbook::PIDSI_SUBJECT).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_AUTHOR)) {
        info->setAuthorInfo("creator", workbook->property(Workbook::PIDSI_AUTHOR).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_KEYWORDS)) {
        info->setAboutInfo("keyword", workbook->property(Workbook::PIDSI_KEYWORDS).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_COMMENTS)) {
        info->setAboutInfo("comments", workbook->property(Workbook::PIDSI_COMMENTS).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_REVNUMBER)) {
        info->setAboutInfo("editing-cycles", workbook->property(Workbook::PIDSI_REVNUMBER).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_LASTPRINTED_DTM)) {
        info->setAboutInfo("print-date", workbook->property(Workbook::PIDSI_LASTPRINTED_DTM).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_CREATE_DTM)) {
        info->setAboutInfo("creation-date", workbook->property(Workbook::PIDSI_CREATE_DTM).toString());
    }
    if (workbook->hasProperty(Workbook::PIDSI_LASTSAVED_DTM)) {
        info->setAboutInfo("date", workbook->property(Workbook::PIDSI_LASTSAVED_DTM).toString());
    }
    // template
    // lastauthor
    // edittime

    switch (workbook->version()) {
    case Workbook::Excel95:
        info->setOriginalGenerator("Calligra xls Filter/Excel 95");
        break;
    case Workbook::Excel97:
        info->setOriginalGenerator("Calligra xls Filter/Excel 97");
        break;
    case Workbook::Excel2000:
        info->setOriginalGenerator("Calligra xls Filter/Excel 2000");
        break;
    case Workbook::Excel2002:
        info->setOriginalGenerator("Calligra xls Filter/Excel 2002");
        break;
    case Workbook::Excel2003:
        info->setOriginalGenerator("Calligra xls Filter/Excel 2003");
        break;
    case Workbook::Excel2007:
        info->setOriginalGenerator("Calligra xls Filter/Excel 2007");
        break;
    case Workbook::Excel2010:
        info->setOriginalGenerator("Calligra xls Filter/Excel 2010");
        break;
    default:
        info->setOriginalGenerator("Calligra xls Filter/Unknown");
    }
}

void ExcelImport::Private::processEmbeddedObjects(const KoXmlElement& rootElement, KoStore* store)
{
    // save styles to xml
    KoXmlWriter *stylesXml = beginMemoryXmlWriter("office:styles");
    shapeStyles->saveOdfStyles(KoGenStyles::DocumentAutomaticStyles, stylesXml);

    KoXmlDocument stylesDoc = endMemoryXmlWriter(stylesXml);

    // Register additional attributes, that identify shapes anchored in cells.
    // Their dimensions need adjustment after all rows are loaded,
    // because the position of the end cell is not always known yet.
    KoShapeLoadingContext::addAdditionalAttributeData(KoShapeLoadingContext::AdditionalAttributeData(
                KoXmlNS::table, "end-cell-address",
                "table:end-cell-address"));
    KoShapeLoadingContext::addAdditionalAttributeData(KoShapeLoadingContext::AdditionalAttributeData(
                KoXmlNS::table, "end-x",
                "table:end-x"));
    KoShapeLoadingContext::addAdditionalAttributeData(KoShapeLoadingContext::AdditionalAttributeData(
                KoXmlNS::table, "end-y",
                "table:end-y"));


    KoOdfStylesReader odfStyles;
    odfStyles.createStyleMap(stylesDoc, false);
    KoOdfLoadingContext odfContext(odfStyles, store);
    KoShapeLoadingContext shapeContext(odfContext, outputDoc->resourceManager());

    int numSheetTotal = rootElement.childNodesCount();
    int currentSheet = 0;
    KoXmlElement sheetElement;
    forEachElement(sheetElement, rootElement) {
        Q_ASSERT(sheetElement.namespaceURI() == KoXmlNS::table && sheetElement.localName() == "table");
        int sheetId = sheetElement.attributeNS(KoXmlNS::table, "id").toInt();
        Calligra::Tables::Sheet* sheet = outputDoc->map()->sheet(sheetId);

        KoXmlElement cellElement;
        int numCellElements = sheetElement.childNodesCount();
        int currentCell = 0;
        forEachElement(cellElement, sheetElement) {
            Q_ASSERT(cellElement.namespaceURI() == KoXmlNS::table);
            if (cellElement.localName() == "shapes") {
                KoXmlElement element;
                forEachElement(element, cellElement) {
                    sheet->loadOdfObject(element, shapeContext);
                }
            } else {
                Q_ASSERT(cellElement.localName() == "table-cell");
                int row = cellElement.attributeNS(KoXmlNS::table, "row").toInt();
                int col = cellElement.attributeNS(KoXmlNS::table, "column").toInt();
                Calligra::Tables::Cell cell(sheet, col, row);

                KoXmlElement element;
                forEachElement(element, cellElement) {
                    cell.loadOdfObject(element, shapeContext);
                }
            }
            ++currentCell;
            const int progress = int(currentSheet / qreal(numSheetTotal) * EMBEDDEDPROGRESS
                               + (EMBEDDEDPROGRESS / qreal(numSheetTotal) * currentCell/numCellElements)
                               + SIDEWINDERPROGRESS + ODFPROGRESS) + 0.5;
            emit q->sigProgress(progress);
        }

        ++currentSheet;
        const int progress = int(currentSheet / qreal(numSheetTotal) * EMBEDDEDPROGRESS + SIDEWINDERPROGRESS + ODFPROGRESS + 0.5);
        emit q->sigProgress(progress);
    }
}

static QRectF getRect(const MSO::OfficeArtFSPGR &r)
{
    return QRect(r.xLeft, r.yTop, r.xRight - r.xLeft, r.yBottom - r.yTop);
}

void ExcelImport::Private::processSheet(Sheet* is, Calligra::Tables::Sheet* os)
{
    os->setHidden(!is->visible());
    //os->setProtected(is->protect());
    os->setAutoCalculationEnabled(is->autoCalc());
    os->setHideZero(!is->showZeroValues());
    os->setShowGrid(is->showGrid());
    os->setFirstLetterUpper(false);
    os->map()->loadingInfo()->setCursorPosition(os, is->firstVisibleCell() + QPoint(1, 1));
    os->setShowFormulaIndicator(false);
    os->setShowCommentIndicator(true);
    os->setShowPageBorders(is->isPageBreakViewEnabled());
    os->setLcMode(false);
    os->setShowColumnNumber(false);
    os->setLayoutDirection(is->isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);

    // TODO: page layout
    processSheetForHeaderFooter(is, os);

    if(is->password() != 0) {
        //TODO
    }


    const unsigned columnCount = qMin(maximalColumnCount, is->maxColumn());
    for (unsigned i = 0; i <= columnCount; ++i) {
        processColumn(is, i, os);
    }

    cellStyles.clear();
    rowStyles.clear();
    columnStyles.clear();
    cellConditions.clear();
    const unsigned rowCount = qMin(maximalRowCount, is->maxRow());
    for (unsigned i = 0; i <= rowCount && i < KS_rowMax; ++i) {
        processRow(is, i, os);
    }

    QList<QPair<QRegion, Calligra::Tables::Style> > styles;
    for (QHash<int, QRegion>::const_iterator it = columnStyles.constBegin(); it != columnStyles.constEnd(); ++it) {
        styles.append(qMakePair(it.value(), styleList[it.key()]));
    }
    for (QHash<int, QRegion>::const_iterator it = rowStyles.constBegin(); it != rowStyles.constEnd(); ++it) {
        styles.append(qMakePair(it.value(), styleList[it.key()]));
    }
    for (QHash<int, QRegion>::const_iterator it = cellStyles.constBegin(); it != cellStyles.constEnd(); ++it) {
        styles.append(qMakePair(it.value(), styleList[it.key()]));
    }
    os->cellStorage()->loadStyles(styles);

    // sheet shapes
    if (!is->drawObjects().isEmpty() || is->drawObjectsGroupCount()) {
        shapesXml->startElement("table:shapes");

        ODrawClient client = ODrawClient(is);
        ODrawToOdf odraw(client);
        Writer writer(*shapesXml, *shapeStyles, false);

        const QList<OfficeArtObject*> objs = is->drawObjects();
        for (int i = 0; i < objs.size(); ++i) {
            OfficeArtObject* o = objs[i];
            client.setShapeText(o->text());
            client.setZIndex(o->index());
            odraw.processDrawingObject(o->object(), writer);
        }

        for (int i = is->drawObjectsGroupCount()-1; i >= 0; --i) {
            shapesXml->startElement("draw:g");

            const MSO::OfficeArtSpgrContainer& group = is->drawObjectsGroup(i);
            const MSO::OfficeArtSpContainer* first = group.rgfb.first().anon.get<MSO::OfficeArtSpContainer>();
            if (first && first->clientAnchor && first->shapeGroup) {
                QRectF oldCoords = client.getGlobalRect(*first->clientAnchor);
                QRectF newCoords = getRect(*first->shapeGroup);
                Writer transw = writer.transform(oldCoords, newCoords);
                const QList<OfficeArtObject*> gobjs = is->drawObjects(i);
                for (int j = 0; j < gobjs.size(); ++j) {
                    OfficeArtObject* o = gobjs[j];
                    client.setShapeText(o->text());
                    client.setZIndex(o->index());
                    odraw.processDrawingObject(o->object(), transw);
                }
            } else {
                const QList<OfficeArtObject*> gobjs = is->drawObjects(i);
                for (int j = 0; j < gobjs.size(); ++j) {
                    OfficeArtObject* o = gobjs[j];
                    client.setShapeText(o->text());
                    client.setZIndex(o->index());
                    odraw.processDrawingObject(o->object(), writer);
                }
            }
            shapesXml->endElement(); // draw:g
        }

        shapesXml->endElement();
    }


    processSheetForFilters(is, os);
    processSheetForConditionals(is, os);

    os->cellStorage()->loadConditions(cellConditions);
}

void ExcelImport::Private::processSheetForHeaderFooter(Sheet* is, Calligra::Tables::Sheet* os)
{
    os->print()->headerFooter()->setHeadFootLine(
            convertHeaderFooter(is->leftHeader()), convertHeaderFooter(is->centerHeader()),
            convertHeaderFooter(is->rightHeader()), convertHeaderFooter(is->leftFooter()),
            convertHeaderFooter(is->centerFooter()), convertHeaderFooter(is->rightFooter()));
}

void ExcelImport::Private::processSheetForFilters(Sheet* is, Calligra::Tables::Sheet* os)
{
    static int rangeId = 0; // not very nice to do this this way, but I only care about sort of unique names
    QList<QRect> filters = workbook->filterRanges(is);
    foreach (const QRect& filter, filters) {
        Calligra::Tables::Database db;
        db.setName(QString("excel-database-%1").arg(++rangeId));
        db.setDisplayFilterButtons(true);
        QRect r = filter.adjusted(1, 1, 1, 1);
        r.setBottom(is->maxRow()+1);
        Calligra::Tables::Region range(r, os);
        db.setRange(range);
        db.setFilter(is->autoFilters());
        os->cellStorage()->setDatabase(range, db);

        // xls files don't seem to make a difference between hidden and filtered rows, so
        // assume all rows in a database range are filtered, not explicitly hidden
        int row = r.top() + 1;
        while (row <= r.bottom()) {
            int lastRow;
            bool isHidden = os->rowFormats()->isHidden(row, &lastRow);
            if (isHidden) {
                os->rowFormats()->setHidden(row, lastRow, false);
                os->rowFormats()->setFiltered(row, lastRow, true);
            }
            row = lastRow + 1;
        }
    }
}

static Calligra::Tables::Value convertValue(const Value& v)
{
    if (v.isBoolean()) {
        return Calligra::Tables::Value(v.asBoolean());
    } else if (v.isFloat()) {
        return Calligra::Tables::Value(v.asFloat());
    } else if (v.isInteger()) {
        return Calligra::Tables::Value(v.asInteger());
    } else if (v.isText()) {
        return Calligra::Tables::Value(v.asString());
    } else if (v.isError()) {
        Calligra::Tables::Value kv(Calligra::Tables::Value::Error);
        kv.setError(v.asString());
        return kv;
    } else {
        return Calligra::Tables::Value();
    }
}

void ExcelImport::Private::processSheetForConditionals(Sheet* is, Calligra::Tables::Sheet* os)
{
    static int styleNameId = 0;
    const QList<ConditionalFormat*> conditionals = is->conditionalFormats();
    Calligra::Tables::StyleManager* styleManager = os->map()->styleManager();
    foreach (ConditionalFormat* cf, conditionals) {
        QRegion r = cf->region().translated(1, 1);
        QLinkedList<Calligra::Tables::Conditional> conds;
        foreach (const Conditional& c, cf->conditionals()) {
            Calligra::Tables::Conditional kc;
            switch (c.cond) {
            case Conditional::None:
                kc.cond = Calligra::Tables::Conditional::None;
                break;
            case Conditional::Formula:
                kc.cond = Calligra::Tables::Conditional::IsTrueFormula;
                break;
            case Conditional::Between:
                kc.cond = Calligra::Tables::Conditional::Between;
                break;
            case Conditional::Outside:
                kc.cond = Calligra::Tables::Conditional::Different;
                break;
            case Conditional::Equal:
                kc.cond = Calligra::Tables::Conditional::Equal;
                break;
            case Conditional::NotEqual:
                kc.cond = Calligra::Tables::Conditional::DifferentTo;
                break;
            case Conditional::Greater:
                kc.cond = Calligra::Tables::Conditional::Superior;
                break;
            case Conditional::Less:
                kc.cond = Calligra::Tables::Conditional::Inferior;
                break;
            case Conditional::GreaterOrEqual:
                kc.cond = Calligra::Tables::Conditional::SuperiorEqual;
                break;
            case Conditional::LessOrEqual:
                kc.cond = Calligra::Tables::Conditional::InferiorEqual;
                break;
            }
            qDebug() << "FRM:" << c.cond << kc.cond;
            kc.value1 = convertValue(c.value1);
            kc.value2 = convertValue(c.value2);
            kc.baseCellAddress = Swinder::encodeAddress(is->name(), cf->region().boundingRect().left(), cf->region().boundingRect().top());

            Calligra::Tables::CustomStyle* style = new Calligra::Tables::CustomStyle(QString("Excel-Condition-Style-%1").arg(styleNameId++));
            kc.styleName = style->name();

            // TODO: valueFormat
            if (c.hasFontItalic()) {
                style->setFontItalic(c.font().italic());
            }
            if (c.hasFontStrikeout()) {
                style->setFontStrikeOut(c.font().strikeout());
            }
            if (c.hasFontBold()) {
                style->setFontBold(c.font().bold());
            }
            // TODO: sub/superscript
            if (c.hasFontUnderline()) {
                style->setFontUnderline(c.font().underline());
            }
            if (c.hasFontColor()) {
                style->setFontColor(c.font().color());
            }
            // TODO: other properties

            styleManager->insertStyle(style);
            conds.append(kc);
        }
        Calligra::Tables::Conditions kcs;
        kcs.setConditionList(conds);
        cellConditions.append(qMakePair(r, kcs));
    }
}

QString ExcelImport::Private::convertHeaderFooter(const QString& text)
{
    QString result;
    bool skipUnsupported = false;
    int lastPos;
    int pos = text.indexOf('&');
    int len = text.length();
    if ((pos < 0) && (text.length() > 0))   // If ther is no &
        result += text;
    else if (pos > 0) // Some text and '&'
        result += text.mid(0,  pos - 1);

    while (pos >= 0) {
        switch (text[pos + 1].unicode()) {
        case 'D':
            result += "<date>";
            break;
        case 'T':
            result += "<time>";
            break;
        case 'P':
            result += "<page>";
            break;
        case 'N':
            result += "<pages>";
            break;
        case 'F':
            result += "<name>";
            break;
        case 'A':
            result += "<sheet>";
            break;
        case '\"':
        default:
            skipUnsupported = true;
            break;
        }
        lastPos = pos;
        pos = text.indexOf('&', lastPos + 1);
        if (!skipUnsupported && (pos > (lastPos + 1)))
            result += text.mid(lastPos + 2, (pos - lastPos - 2));
        else if (!skipUnsupported && (pos < 0))  //Remaining text
            result += text.mid(lastPos + 2, len - (lastPos + 2));
        else
            skipUnsupported = false;
    }
    return result;
}

void ExcelImport::Private::processColumn(Sheet* is, unsigned columnIndex, Calligra::Tables::Sheet* os)
{
    Column* column = is->column(columnIndex, false);

    if (!column) return;

    Calligra::Tables::ColumnFormat* oc = os->nonDefaultColumnFormat(columnIndex+1);
    oc->setWidth(column->width());
    oc->setHidden(!column->visible());

    int styleId = convertStyle(&column->format());
    columnStyles[styleId] += QRect(columnIndex+1, 1, 1, KS_rowMax);
}

void ExcelImport::Private::processRow(Sheet* is, unsigned rowIndex, Calligra::Tables::Sheet* os)
{
    Row *row = is->row(rowIndex, false);

    if (!row) {
        if (is->defaultRowHeight() != os->map()->defaultRowFormat()->height()) {
            os->rowFormats()->setRowHeight(rowIndex+1, rowIndex+1, is->defaultRowHeight());
        }
        return;
    }

    os->rowFormats()->setRowHeight(rowIndex+1, rowIndex+1, row->height());
    os->rowFormats()->setHidden(rowIndex+1, rowIndex+1, !row->visible());
    // TODO default cell style

    // find the column of the rightmost cell (if any)
    const int lastCol = row->sheet()->maxCellsInRow(rowIndex);
    for (int i = 0; i <= lastCol; ++i) {
        Cell* cell = is->cell(i, rowIndex, false);
        if (!cell) continue;
        processCell(cell, Calligra::Tables::Cell(os, i+1, rowIndex+1));
    }

    addProgress(1);
}

static QString cellFormulaNamespace(const QString& formula)
{
    if (!formula.isEmpty()) {
        if(formula.startsWith("ROUNDUP(") || formula.startsWith("ROUNDDOWN(") || formula.startsWith("ROUND(") || formula.startsWith("RAND(")) {
            // Special case Excel formulas that differ from OpenFormula
            return "msoxl:";
        } else if (!formula.isEmpty()) {
            return "of:";
        }
    }
    return QString();
}

static QDateTime convertDate(double timestamp)
{
    QDateTime dt(QDate(1899, 12, 30));
    dt = dt.addMSecs((qint64)(timestamp * 86400 * 1000));
    return dt;
}

static QTime convertTime(double timestamp)
{
    QTime tt;
    tt = tt.addMSecs(qRound((timestamp - (qint64)timestamp) * 86400 * 1000));
    return tt;
}

void ExcelImport::Private::processCell(Cell* ic, Calligra::Tables::Cell oc)
{
    int colSpan = ic->columnSpan();
    int rowSpan = ic->rowSpan();
    if (colSpan > 1 || rowSpan > 1) {
        oc.mergeCells(oc.column(), oc.row(), colSpan - 1, rowSpan - 1);
    }

    const QString formula = ic->formula();
    const bool isFormula = !formula.isEmpty();
    if (isFormula) {
        const QString nsPrefix = cellFormulaNamespace(formula);
        const QString decodedFormula = Calligra::Tables::Odf::decodeFormula('=' + formula, oc.locale(), nsPrefix);
        oc.setRawUserInput(decodedFormula);
    }

    int styleId = convertStyle(&ic->format(), formula);

    Value value = ic->value();
    if (value.isBoolean()) {
        oc.setValue(Calligra::Tables::Value(value.asBoolean()));
        if (!isFormula)
            oc.setRawUserInput(oc.sheet()->map()->converter()->asString(oc.value()).asString());
    } else if (value.isNumber()) {
        const QString valueFormat = ic->format().valueFormat();

        if (isPercentageFormat(valueFormat)) {
            Calligra::Tables::Value v(value.asFloat());
            v.setFormat(Calligra::Tables::Value::fmt_Percent);
            oc.setValue(v);
        } else if (Calligra::Tables::Format::isDate(styleList[styleId].formatType())) {
            QDateTime date = convertDate(value.asFloat());
            oc.setValue(Calligra::Tables::Value(date, outputDoc->map()->calculationSettings()));
            KLocale* locale = outputDoc->map()->calculationSettings()->locale();
            if (!isFormula) {
                if (true /* TODO somehow determine if time should be included */) {
                    oc.setRawUserInput(locale->formatDate(date.date()));
                } else {
                    oc.setRawUserInput(locale->formatDateTime(date));
                }
            }
        } else if (Calligra::Tables::Format::isTime(styleList[styleId].formatType())) {
            QTime time = convertTime(value.asFloat());
            oc.setValue(Calligra::Tables::Value(time, outputDoc->map()->calculationSettings()));
            KLocale* locale = outputDoc->map()->calculationSettings()->locale();
            if (!isFormula)
                oc.setRawUserInput(locale->formatTime(time, true));
        } else /* fraction or normal */ {
            oc.setValue(Calligra::Tables::Value(value.asFloat()));
            if (!isFormula)
                oc.setRawUserInput(oc.sheet()->map()->converter()->asString(oc.value()).asString());
        }
    } else if (value.isText()) {
        QString txt = value.asString();

        Hyperlink link = ic->hyperlink();
        if (link.isValid) {
            if (!link.location.isEmpty()) {
                if (link.location[0] == '#') {
                    oc.setLink(link.location.mid(1));
                } else {
                    oc.setLink(link.location);
                }

                if (!link.displayName.trimmed().isEmpty())
                    txt = link.displayName.trimmed();
            }
        }

        oc.setValue(Calligra::Tables::Value(txt));
        if (!isFormula) {
            if (txt.startsWith('='))
                oc.setRawUserInput('\'' + txt);
            else
                oc.setRawUserInput(txt);
        }
        if (value.isRichText() || ic->format().font().subscript() || ic->format().font().superscript()) {
            std::map<unsigned, FormatFont> formatRuns = value.formatRuns();
            // add sentinel to list of format runs
            if (!formatRuns.count(0))
                formatRuns[0] = ic->format().font();
            formatRuns[txt.length()] = ic->format().font();

            QSharedPointer<QTextDocument> doc(new QTextDocument(txt));
            KoTextDocument(doc.data()).setStyleManager(oc.sheet()->map()->textStyleManager());
            QTextCursor c(doc.data());
            for (std::map<unsigned, FormatFont>::iterator it = formatRuns.begin(); it != formatRuns.end(); ++it) {
                std::map<unsigned, FormatFont>::iterator it2 = it; ++it2;
                if (it2 != formatRuns.end()) {
                    // select block
                    c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, it2->first - it->first);
                    c.setCharFormat(convertFontToCharFormat(it->second));
                    c.clearSelection();
                }
            }
            oc.setRichText(doc);
        }
    } else if (value.isError()) {
        Calligra::Tables::Value v(Calligra::Tables::Value::Error);
        v.setError(value.asString());
        oc.setValue(v);
    }

    QString note = ic->note();
    if (!note.isEmpty())
        oc.setComment(note);

    cellStyles[styleId] += QRect(oc.column(), oc.row(), 1, 1);
    QHash<QString, Calligra::Tables::Conditions>::iterator conds = dataStyleConditions.find(ic->format().valueFormat());
    if (conds != dataStyleConditions.end()) {
        cellConditions.append(qMakePair(QRegion(oc.column(), oc.row(), 1, 1), conds.value()));
    }

    processCellObjects(ic, oc);
}

void ExcelImport::Private::processCellObjects(Cell* ic, Calligra::Tables::Cell oc)
{
    bool hasObjects = false;

    // handle charts
    foreach(ChartObject *chart, ic->charts()) {
        Sheet* const sheet = ic->sheet();
        if(chart->m_chart->m_impl==0) {
            kDebug() << "Invalid chart to be created, no implementation.";
            continue;
        }

        if (!hasObjects) {
            shapesXml->startElement("table:table-cell");
            shapesXml->addAttribute("table:row", oc.row());
            shapesXml->addAttribute("table:column", oc.column());
            hasObjects = true;
        }

        ChartExport *c = new ChartExport(chart->m_chart);
        c->setSheetReplacement( false );
        c->m_href = QString("Chart%1").arg(this->charts.count()+1);
        c->m_endCellAddress = Swinder::encodeAddress(sheet->name(), chart->m_colR, chart->m_rwB);
        c->m_notifyOnUpdateOfRanges = "Sheet1.D2:Sheet1.F2";

        const unsigned long colL = chart->m_colL;
        const unsigned long dxL = chart->m_dxL;
        const unsigned long dyT = chart->m_dyT;
        const unsigned long rwT = chart->m_rwT;

        c->m_x = offset(columnWidth(sheet, colL), dxL, 1024);
        c->m_y = offset(rowHeight(sheet, rwT), dyT, 256);

        if (!chart->m_chart->m_cellRangeAddress.isNull() )
            c->m_cellRangeAddress = Swinder::encodeAddress(sheet->name(), chart->m_chart->m_cellRangeAddress.left(), chart->m_chart->m_cellRangeAddress.top()) + ":" +
                                    Swinder::encodeAddress(sheet->name(), chart->m_chart->m_cellRangeAddress.right(), chart->m_chart->m_cellRangeAddress.bottom());

        this->charts << c;

        c->saveIndex(shapesXml);
    }



    // handle ODraw objects
    QList<OfficeArtObject*> objects = ic->drawObjects();
    if (!objects.empty()) {
        if (!hasObjects) {
            shapesXml->startElement("table:table-cell");
            shapesXml->addAttribute("table:row", oc.row());
            shapesXml->addAttribute("table:column", oc.column());
            hasObjects = true;
        }
        ODrawClient client = ODrawClient(ic->sheet());

        ODrawToOdf odraw(client);
        Writer writer(*shapesXml, *shapeStyles, false);
        for (int i = 0; i < objects.size(); ++i) {
            OfficeArtObject* o = objects[i];
            client.setShapeText(o->text());
            client.setZIndex(o->index());
            odraw.processDrawingObject(o->object(), writer);
        }
    }

    if (hasObjects) {
        shapesXml->endElement();
    }
}

void ExcelImport::Private::processCharts(KoXmlWriter* manifestWriter)
{
    foreach(ChartExport *c, this->charts) {
        c->set2003ColorPalette( workbook->colorTable() );
        c->saveContent(this->storeout, manifestWriter);
    }
}

int ExcelImport::Private::convertStyle(const Format* format, const QString& formula)
{
    CellFormatKey key(format, formula);
    int& styleId = styleCache[key];
    if (!styleId) {
        Calligra::Tables::Style style;
        style.setDefault();

        if (!key.isGeneral) {
            style.merge(dataStyleCache.value(format->valueFormat(), Calligra::Tables::Style()));
        } else {
            if (key.decimalCount >= 0) {
                style.setFormatType(Calligra::Tables::Format::Number);
                style.setPrecision(key.decimalCount);
                QString format = ".";
                for (int i = 0; i < key.decimalCount; i++) {
                    format += '0';
                }
                style.setCustomFormat(format);
            }
        }

        processFontFormat(format->font(), style);

        FormatAlignment align = format->alignment();
        if (!align.isNull()) {
            switch (align.alignY()) {
            case Format::Top:
                style.setVAlign(Calligra::Tables::Style::Top);
                break;
            case Format::Middle:
                style.setVAlign(Calligra::Tables::Style::Middle);
                break;
            case Format::Bottom:
                style.setVAlign(Calligra::Tables::Style::Bottom);
                break;
            case Format::VJustify:
                style.setVAlign(Calligra::Tables::Style::VJustified);
                break;
            case Format::VDistributed:
                style.setVAlign(Calligra::Tables::Style::VDistributed);
                break;
            }

            style.setWrapText(align.wrap());

            if (align.rotationAngle()) {
                style.setAngle(align.rotationAngle());
            }

            if (align.stackedLetters()) {
                style.setVerticalText(true);
            }

            if (align.shrinkToFit()) {
                style.setShrinkToFit(true);
            }

            switch (align.alignX()) {
            case Format::Left:
                style.setHAlign(Calligra::Tables::Style::Left);
                break;
            case Format::Center:
                style.setHAlign(Calligra::Tables::Style::Center);
                break;
            case Format::Right:
                style.setHAlign(Calligra::Tables::Style::Right);
                break;
            case Format::Justify:
            case Format::Distributed:
                style.setHAlign(Calligra::Tables::Style::Justified);
                break;
            }

            if (align.indentLevel() != 0) {
                style.setIndentation(align.indentLevel() * 10);
            }
        }

        FormatBorders borders = format->borders();
        if (!borders.isNull()) {
            style.setLeftBorderPen(convertBorder(borders.leftBorder()));
            style.setRightBorderPen(convertBorder(borders.rightBorder()));
            style.setTopBorderPen(convertBorder(borders.topBorder()));
            style.setBottomBorderPen(convertBorder(borders.bottomBorder()));
            style.setFallDiagonalPen(convertBorder(borders.topLeftBorder()));
            style.setGoUpDiagonalPen(convertBorder(borders.bottomLeftBorder()));
        }

        FormatBackground back = format->background();
        if (!back.isNull() && back.pattern() != FormatBackground::EmptyPattern) {
            QColor backColor = back.backgroundColor();
            if (back.pattern() == FormatBackground::SolidPattern)
                backColor = back.foregroundColor();
            style.setBackgroundColor(backColor);

            QBrush brush;
            switch (back.pattern()) {
            case FormatBackground::SolidPattern:
                brush.setStyle(Qt::SolidPattern);
                brush.setColor(backColor);
                break;
            case FormatBackground::Dense3Pattern: // 88% gray
                brush.setStyle(Qt::Dense2Pattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::Dense4Pattern: // 50% gray
                brush.setStyle(Qt::Dense4Pattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::Dense5Pattern: // 37% gray
                brush.setStyle(Qt::Dense5Pattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::Dense6Pattern: // 12% gray
                brush.setStyle(Qt::Dense6Pattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::Dense7Pattern: // 6% gray
                brush.setStyle(Qt::Dense7Pattern);
                brush.setColor(Qt::black);
                break;

            case FormatBackground::Dense1Pattern:
            case FormatBackground::HorPattern:
                brush.setStyle(Qt::HorPattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::VerPattern:
                brush.setStyle(Qt::VerPattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::Dense2Pattern:
            case FormatBackground::BDiagPattern:
                brush.setStyle(Qt::BDiagPattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::FDiagPattern:
                brush.setStyle(Qt::FDiagPattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::CrossPattern:
                brush.setStyle(Qt::CrossPattern);
                brush.setColor(Qt::black);
                break;
            case FormatBackground::DiagCrossPattern:
                brush.setStyle(Qt::DiagCrossPattern);
                brush.setColor(Qt::black);
                break;
            }
            style.setBackgroundBrush(brush);
        }

        styleId = styleList.size();
        styleList.append(style);
    }
    return styleId;
}

void ExcelImport::Private::processFontFormat(const FormatFont& font, Calligra::Tables::Style& style)
{
    if (font.isNull()) return;

    QFont f;
    f.setBold(font.bold());
    f.setItalic(font.italic());
    f.setUnderline(font.underline());
    f.setStrikeOut(font.strikeout());
    f.setFamily(font.fontFamily());
    f.setPointSizeF(font.fontSize());
    style.setFont(f);
    style.setFontColor(font.color());
}

QTextCharFormat ExcelImport::Private::convertFontToCharFormat(const FormatFont& font)
{
    QTextCharFormat frm;
    QFont f;
    f.setBold(font.bold());
    f.setItalic(font.italic());
    f.setUnderline(font.underline());
    f.setStrikeOut(font.strikeout());
    f.setFamily(font.fontFamily());
    f.setPointSizeF(font.fontSize());
    frm.setFont(f);
    frm.setForeground(font.color());
    if (font.subscript())
        frm.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    if (font.superscript())
        frm.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    return frm;
}

QPen ExcelImport::Private::convertBorder(const Pen& pen)
{
    if (pen.style == Pen::NoLine || pen.width == 0) {
        return QPen(Qt::NoPen);
    } else {
        QPen op;
        if (pen.style == Pen::DoubleLine) {
            op.setWidthF(pen.width * 3);
        } else {
            op.setWidthF(pen.width);
        }

        switch (pen.style) {
        case Pen::SolidLine: op.setStyle(Qt::SolidLine); break;
        case Pen::DashLine: op.setStyle(Qt::DashLine); break;
        case Pen::DotLine: op.setStyle(Qt::DotLine); break;
        case Pen::DashDotLine: op.setStyle(Qt::DashDotLine); break;
        case Pen::DashDotDotLine: op.setStyle(Qt::DashDotDotLine); break;
        case Pen::DoubleLine: op.setStyle(Qt::SolidLine); break; // TODO
        }

        op.setColor(pen.color);

        return op;
    }
}

void ExcelImport::Private::insertPictureManifest(const QString& fileName)
{
    QString mimeType;
    const QString extension = fileName.right(fileName.size() - fileName.lastIndexOf('.') - 1);

    if( extension == "gif" ) {
        mimeType = "image/gif";
    }
    else if( extension == "jpg" || extension == "jpeg"
            || extension == "jpe" || extension == "jfif" ) {
        mimeType = "image/jpeg";
    }
    else if( extension == "tif" || extension == "tiff" ) {
        mimeType = "image/tiff";
    }
    else if( extension == "png" ) {
        mimeType = "image/png";
    }
    else if( extension == "emf" ) {
        mimeType = "application/x-openoffice-wmf;windows_formatname=\"Image EMF\"";
    }
    else if( extension == "wmf" ) {
        mimeType = "application/x-openoffice-wmf;windows_formatname=\"Image WMF\"";
    }
    else if( extension == "bmp" ) {
        mimeType = "image/bmp";
    }

    manifestEntries.insert("Pictures/" + fileName, mimeType);
}

void ExcelImport::Private::addManifestEntries(KoXmlWriter* manifestWriter)
{
    QMap<QString, QString>::const_iterator iterator = manifestEntries.constBegin();
    QMap<QString, QString>::const_iterator end = manifestEntries.constEnd();
    while( iterator != end ) {
        manifestWriter->addManifestEntry(iterator.key(), iterator.value());
        ++iterator;
    }
}


// Updates the displayed progress information
void ExcelImport::Private::addProgress(int addValue)
{
    rowsCountDone += addValue;
    const int progress = int(rowsCountDone / qreal(rowsCountTotal) * ODFPROGRESS + 0.5 + SIDEWINDERPROGRESS);
    emit q->sigProgress(progress);
}

KoXmlWriter* ExcelImport::Private::beginMemoryXmlWriter(const char* docElement)
{
    QIODevice* d = new QBuffer;
    d->open(QIODevice::ReadWrite);
    KoXmlWriter* xml = new KoXmlWriter(d);
    xml->startDocument(docElement);
    xml->startElement(docElement);
    xml->addAttribute("xmlns:office", KoXmlNS::office);
    xml->addAttribute("xmlns:meta", KoXmlNS::meta);
    xml->addAttribute("xmlns:config", KoXmlNS::config);
    xml->addAttribute("xmlns:text", KoXmlNS::text);
    xml->addAttribute("xmlns:table", KoXmlNS::table);
    xml->addAttribute("xmlns:draw", KoXmlNS::draw);
    xml->addAttribute("xmlns:presentation", KoXmlNS::presentation);
    xml->addAttribute("xmlns:dr3d", KoXmlNS::dr3d);
    xml->addAttribute("xmlns:chart", KoXmlNS::chart);
    xml->addAttribute("xmlns:form", KoXmlNS::form);
    xml->addAttribute("xmlns:script", KoXmlNS::script);
    xml->addAttribute("xmlns:style", KoXmlNS::style);
    xml->addAttribute("xmlns:number", KoXmlNS::number);
    xml->addAttribute("xmlns:math", KoXmlNS::math);
    xml->addAttribute("xmlns:svg", KoXmlNS::svg);
    xml->addAttribute("xmlns:fo", KoXmlNS::fo);
    xml->addAttribute("xmlns:anim", KoXmlNS::anim);
    xml->addAttribute("xmlns:smil", KoXmlNS::smil);
    xml->addAttribute("xmlns:calligra", KoXmlNS::calligra);
    xml->addAttribute("xmlns:officeooo", KoXmlNS::officeooo);
    xml->addAttribute("xmlns:dc", KoXmlNS::dc);
    xml->addAttribute("xmlns:xlink", KoXmlNS::xlink);
    return xml;
}

KoXmlDocument ExcelImport::Private::endMemoryXmlWriter(KoXmlWriter* writer)
{
    writer->endElement();
    writer->endDocument();
    QBuffer* b = static_cast<QBuffer*>(writer->device());


    b->seek(0);
    KoXmlDocument doc;
    QString errorMsg; int errorLine, errorColumn;
    if (!doc.setContent(b, true, &errorMsg, &errorLine, &errorColumn)) {
        kDebug() << errorMsg << errorLine << errorColumn;
    }
    delete b;
    delete writer;
    return doc;
}

void ExcelImport::Private::processNumberFormats()
{
    static const QString sNoStyle = QString::fromLatin1("NOSTYLE");
    QHash<QString, QString> dataStyleMap;

    NumberFormatParser::setStyles(dataStyles);
    for (int i = 0; i < workbook->formatCount(); i++) {
        Format* f = workbook->format(i);
        QString& styleName = dataStyleMap[f->valueFormat()];
        if (styleName.isEmpty()) {
            KoGenStyle s = NumberFormatParser::parse(f->valueFormat());
            if (s.type() != KoGenStyle::ParagraphAutoStyle) {
                styleName = dataStyles->insert(s, "N");
            } else {
                styleName = sNoStyle; // assign it a name anyway to prevent converting it twice
            }
        }
    }

    KoXmlWriter *stylesXml = beginMemoryXmlWriter("office:styles");
    dataStyles->saveOdfStyles(KoGenStyles::DocumentAutomaticStyles, stylesXml);

    KoXmlDocument stylesDoc = endMemoryXmlWriter(stylesXml);

    KoOdfStylesReader odfStyles;
    odfStyles.createStyleMap(stylesDoc, false);

    for (int i = 0; i < workbook->formatCount(); i++) {
        Format* f = workbook->format(i);
        const QString& styleName = dataStyleMap[f->valueFormat()];
        if (styleName != sNoStyle) {
            Calligra::Tables::Style& style = dataStyleCache[f->valueFormat()];
            if (style.isEmpty()) {
                Calligra::Tables::Conditions conditions;
                style.loadOdfDataStyle(odfStyles, styleName, conditions, outputDoc->map()->styleManager(), outputDoc->map()->parser());

                if (!conditions.isEmpty())
                    dataStyleConditions[f->valueFormat()] = conditions;
            }
        }
    }
}

void ExcelImport::slotSigProgress(int progress)
{
    emit sigProgress(int(SIDEWINDERPROGRESS/100.0 * progress + 0.5));
}
