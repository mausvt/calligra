/*
 *  Copyright (c) 2015 Dmitry Kazakov <dimula73@gmail.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "timeline_model_test.h"

#include <qtest_kde.h>

#include "kis_image.h"
#include "kis_node.h"
#include "kis_paint_device.h"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include "KisDocument.h"
#include "KisPart.h"
#include "kis_name_server.h"
#include "flake/kis_shape_controller.h"

#include "frames_table_view.h"
#include "timeline_frames_model.h"

#include "kis_node_dummies_graph.h"


void TimelineModelTest::init()
{
    m_doc = KisPart::instance()->createDocument();

    m_nameServer = new KisNameServer();
    m_shapeController = new KisShapeController(m_doc, m_nameServer);
    //m_nodeModel = new KisNodeModel(0);

    initBase();
}

void TimelineModelTest::cleanup()
{
    cleanupBase();

    //delete m_nodeModel;
    delete m_shapeController;
    delete m_nameServer;
    delete m_doc;
}

#include "timeline_frames_index_converter.h"

void TimelineModelTest::testConverter()
{
    constructImage();
    addSelectionMasks();
    m_shapeController->setImage(m_image);

    m_layer1->enableAnimation();

    m_layer1->setUseInTimeline(true);
    m_layer2->setUseInTimeline(true);
    m_sel3->setUseInTimeline(true);

    TimelineFramesIndexConverter converter(m_shapeController);

    QCOMPARE(converter.rowCount(), 3);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_layer1)), 2);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_layer2)), 1);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_sel3)), 0);

    QCOMPARE(converter.dummyFromRow(2), m_shapeController->dummyForNode(m_layer1));
    QCOMPARE(converter.dummyFromRow(1), m_shapeController->dummyForNode(m_layer2));
    QCOMPARE(converter.dummyFromRow(0), m_shapeController->dummyForNode(m_sel3));

    TimelineNodeListKeeper keeper(0, m_shapeController);

    QCOMPARE(keeper.rowCount(), 3);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_layer1)), 2);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_layer2)), 1);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_sel3)), 0);

    QCOMPARE(keeper.dummyFromRow(2), m_shapeController->dummyForNode(m_layer1));
    QCOMPARE(keeper.dummyFromRow(1), m_shapeController->dummyForNode(m_layer2));
    QCOMPARE(keeper.dummyFromRow(0), m_shapeController->dummyForNode(m_sel3));

    TimelineNodeListKeeper::OtherLayersList list = keeper.otherLayersList();

    foreach (const TimelineNodeListKeeper::OtherLayer &l, list) {
        qDebug() << ppVar(l.name) << ppVar(l.dummy->node()->name());
    }

}

void TimelineModelTest::testModel()
{
    QScopedPointer<TimelineFramesModel> model(new TimelineFramesModel(0));
}

void TimelineModelTest::testView()
{
    QDialog dlg;

    QFont font;
    font.setPointSizeF(9);
    dlg.setFont(font);

    QDoubleSpinBox *dblZoom = new QDoubleSpinBox(&dlg);
    dblZoom->setValue(1.0);
    dblZoom->setSingleStep(0.1);

    QSpinBox *intFps = new QSpinBox(&dlg);
    intFps->setValue(12);

    FramesTableView *framesTable = new FramesTableView(&dlg);

    TimelineFramesModel *model = new TimelineFramesModel(&dlg);

    constructImage();
    addSelectionMasks();
    m_shapeController->setImage(m_image);

    m_layer1->enableAnimation();
    m_layer1->setUseInTimeline(true);

    model->setDummiesFacade(m_shapeController, m_image);

    framesTable->setModel(model);

    connect(dblZoom, SIGNAL(valueChanged(double)), framesTable, SLOT(setZoomDouble(double)));
    connect(intFps, SIGNAL(valueChanged(int)), framesTable, SLOT(setFramesPerSecond(int)));

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    layout->addWidget(dblZoom);
    layout->addWidget(intFps);
    layout->addWidget(framesTable);

    layout->setStretch(0, 0);
    layout->setStretch(1, 0);
    layout->setStretch(2, 1);

    dlg.resize(600, 400);

    dlg.exec();
}

QTEST_KDEMAIN(TimelineModelTest, GUI)
