/* This file is part of the KDE project
 * Copyright (C) 2007,2009 Jan Hambrecht <jaham@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "TestShapeReorderCommand.h"
#include <MockShapes.h>
#include <KoShapeReorderCommand.h>
#include <KoShapeManager.h>

#include <kcomponentdata.h>

TestShapeReorderCommand::TestShapeReorderCommand()
{
    KComponentData componentData("TestShapeReorderCommand");    // we need an instance for the canvas
}

TestShapeReorderCommand::~TestShapeReorderCommand()
{
}

void TestShapeReorderCommand::testZIndexSorting()
{
    MockShape shape1;
    MockShape shape2;
    MockShape shape3;
    MockShape shape4;
    MockShape shape5;

    shape1.setZIndex(-2);
    shape2.setZIndex(5);
    shape3.setZIndex(0);
    shape4.setZIndex(9999);
    shape5.setZIndex(-9999);

    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&shape4);
    shapes.append(&shape5);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    QCOMPARE(shapes.indexOf(&shape1), 1);
    QCOMPARE(shapes.indexOf(&shape2), 3);
    QCOMPARE(shapes.indexOf(&shape3), 2);
    QCOMPARE(shapes.indexOf(&shape4), 4);
    QCOMPARE(shapes.indexOf(&shape5), 0);
}

void TestShapeReorderCommand::testRunThroughSorting()
{
    MockShape shape1;
    MockShape shape2;
    MockShape shape3;
    MockShape shape4;
    MockShape shape5;

    shape1.setZIndex(-2);
    shape2.setZIndex(5);
    shape3.setZIndex(0);
    shape4.setZIndex(9999);
    shape5.setZIndex(-9999);

    shape2.setTextRunAroundSide(KoShape::RunThrough, KoShape::Background);
    shape3.setTextRunAroundSide(KoShape::RunThrough, KoShape::Foreground);

    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&shape4);
    shapes.append(&shape5);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    QCOMPARE(shapes.indexOf(&shape1), 2);
    QCOMPARE(shapes.indexOf(&shape2), 0);
    QCOMPARE(shapes.indexOf(&shape3), 4);
    QCOMPARE(shapes.indexOf(&shape4), 3);
    QCOMPARE(shapes.indexOf(&shape5), 1);
}

void TestShapeReorderCommand::testParentChildSorting()
{
    MockShape shape1;
    MockShape shape2;
    MockShape shape3;
    MockShape shape4;
    MockShape shape5;
    MockShape shape6;
    MockShape shape7;
    MockContainer container1;
    MockContainer container2;
    MockContainer container3;

    shape1.setZIndex(-2);
    shape2.setZIndex(5);
    shape3.setZIndex(0);
    shape4.setZIndex(9999);
    shape5.setZIndex(-9999);
    shape6.setZIndex(3);
    shape7.setZIndex(7);
    container1.setZIndex(-55);
    container2.setZIndex(57);

    shape2.setTextRunAroundSide(KoShape::RunThrough, KoShape::Background);
    shape3.setTextRunAroundSide(KoShape::RunThrough, KoShape::Foreground);
    container1.setTextRunAroundSide(KoShape::RunThrough, KoShape::Foreground);

    container1.addShape(&shape1);
    container1.addShape(&shape2);
    container2.addShape(&shape4);
    container2.addShape(&shape5);
    container1.addShape(&container2);
    container1.addShape(&container3);

    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&shape4);
    shapes.append(&shape5);
    shapes.append(&shape6);
    shapes.append(&shape7);
    shapes.append(&container1);
    shapes.append(&container2);
    shapes.append(&container3);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    QCOMPARE(shapes.indexOf(&shape1), 3);
    QCOMPARE(shapes.indexOf(&shape2), 2);
    QCOMPARE(shapes.indexOf(&shape3), 9);
    QCOMPARE(shapes.indexOf(&shape4), 7);
    QCOMPARE(shapes.indexOf(&shape5), 6);
    QCOMPARE(shapes.indexOf(&shape6), 0);
    QCOMPARE(shapes.indexOf(&shape7), 1);

    QCOMPARE(shapes.indexOf(&container1), 8);
    QCOMPARE(shapes.indexOf(&container2), 5);
    QCOMPARE(shapes.indexOf(&container3), 4);
}

void TestShapeReorderCommand::testBringToFront()
{
    MockShape shape1, shape2, shape3;

    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);

    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape1), 0);
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape1);

    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::BringToFront);
    cmd->redo();

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape2), 0);
    QCOMPARE(shapes.indexOf(&shape3), 1);
    QCOMPARE(shapes.indexOf(&shape1), 2);

    delete cmd;
}

void TestShapeReorderCommand::testSendToBack()
{
    MockShape shape1, shape2, shape3;

    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);

    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape1), 0);
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape3);

    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    cmd->redo();

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape3), 0);
    QCOMPARE(shapes.indexOf(&shape1), 1);
    QCOMPARE(shapes.indexOf(&shape2), 2);

    delete cmd;
}

void TestShapeReorderCommand::testMoveUp()
{
    MockShape shape1, shape2, shape3;

    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);

    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape1), 0);
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape1);

    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::RaiseShape);
    cmd->redo();

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape2), 0);
    QCOMPARE(shapes.indexOf(&shape1), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    delete cmd;
}

void TestShapeReorderCommand::testMoveDown()
{
    MockShape shape1, shape2, shape3;

    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);

    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape1), 0);
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape2);

    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::LowerShape);
    cmd->redo();

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape2), 0);
    QCOMPARE(shapes.indexOf(&shape1), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    delete cmd;
}

void TestShapeReorderCommand::testMoveUpOverlapping()
{
    MockShape shape1, shape2, shape3, shape4, shape5;
    
    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);

    shape3.setSize(QSizeF(300, 300));
    shape3.setZIndex(3);
    
    shape4.setSize(QSizeF(100, 100));
    shape4.setPosition(QPointF(200,200));
    shape4.setZIndex(4);
    shape5.setSize(QSizeF(100, 100));
    shape5.setPosition(QPointF(200,200));
    shape5.setZIndex(5);
    
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&shape4);
    shapes.append(&shape5);
    
    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);
    
    QVERIFY(shape1.zIndex() < shape2.zIndex());
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QVERIFY(shape3.zIndex() < shape4.zIndex());
    QVERIFY(shape4.zIndex() < shape5.zIndex());
    
    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape1);
    
    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::RaiseShape);
    cmd->redo();
    delete cmd;
    
    QVERIFY(shape1.zIndex() > shape2.zIndex());
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QVERIFY(shape1.zIndex() < shape3.zIndex());
    QVERIFY(shape3.zIndex() < shape4.zIndex());
    QVERIFY(shape4.zIndex() < shape5.zIndex());
}

void TestShapeReorderCommand::testMoveDownOverlapping()
{
#if 0 // disable a current alogrithm does not yet support this
    MockShape shape1, shape2, shape3, shape4, shape5;
    
    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);

    shape3.setSize(QSizeF(300, 300));
    shape3.setZIndex(3);
    
    shape4.setSize(QSizeF(100, 100));
    shape4.setPosition(QPointF(200,200));
    shape4.setZIndex(4);
    shape5.setSize(QSizeF(100, 100));
    shape5.setPosition(QPointF(200,200));
    shape5.setZIndex(5);
    
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&shape4);
    shapes.append(&shape5);
    
    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);
    
    QVERIFY(shape1.zIndex() < shape2.zIndex());
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QVERIFY(shape3.zIndex() < shape4.zIndex());
    QVERIFY(shape4.zIndex() < shape5.zIndex());
    
    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape5);
    
    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::LowerShape);
    cmd->redo();
    delete cmd;
    
    QVERIFY(shape1.zIndex() < shape2.zIndex());
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QVERIFY(shape3.zIndex() < shape4.zIndex());
    QVERIFY(shape4.zIndex() > shape5.zIndex());
    QVERIFY(shape3.zIndex() > shape5.zIndex());
#endif
}

void TestShapeReorderCommand::testSendToBackChildren()
{
    MockShape shape1, shape2, shape3;
    
    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    
    MockContainer container;
    container.addShape(&shape1);
    container.addShape(&shape2);
    container.addShape(&shape3);
    
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);
    shapes.append(&container);
    
    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);
    
    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&container), 0); // atm the parent is always lower than its children
    QCOMPARE(shapes.indexOf(&shape1), 1);
    QCOMPARE(shapes.indexOf(&shape2), 2);
    QCOMPARE(shapes.indexOf(&shape3), 3);
    
    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape3);
    
    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    cmd->redo();
    delete cmd;
    
    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&container), 0); // atm the parent is always lower than its children
    QCOMPARE(shapes.indexOf(&shape3), 1);
    QVERIFY(shape3.zIndex() < shape1.zIndex());
    QCOMPARE(shapes.indexOf(&shape1), 2);
    QVERIFY(shape1.zIndex() < shape2.zIndex());
    QCOMPARE(shapes.indexOf(&shape2), 3);
    
    selectedShapes.clear();
    selectedShapes.append(&shape2);
    
    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    cmd->redo();
    delete cmd;
    
    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&container), 0); // atm the parent is always lower than its children
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QCOMPARE(shapes.indexOf(&shape3), 2);
    QVERIFY(shape3.zIndex() < shape1.zIndex());
    QCOMPARE(shapes.indexOf(&shape1), 3);
    
    selectedShapes.clear();
    selectedShapes.append(&shape1);
    
    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    cmd->redo();
    delete cmd;
    
    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&container), 0); // atm the parent is always lower than its children
    QCOMPARE(shapes.indexOf(&shape1), 1);
    QVERIFY(shape1.zIndex() < shape2.zIndex());
    QCOMPARE(shapes.indexOf(&shape2), 2);
    QVERIFY(shape2.zIndex() < shape3.zIndex());
    QCOMPARE(shapes.indexOf(&shape3), 3);
}

void TestShapeReorderCommand::testNoCommand()
{
    MockShape shape1, shape2, shape3;

    shape1.setSize(QSizeF(100, 100));
    shape1.setZIndex(1);
    shape2.setSize(QSizeF(100, 100));
    shape2.setZIndex(2);
    shape3.setSize(QSizeF(100, 100));
    shape3.setZIndex(3);
    QList<KoShape*> shapes;
    shapes.append(&shape1);
    shapes.append(&shape2);
    shapes.append(&shape3);

    MockCanvas canvas;
    KoShapeManager manager(&canvas, shapes);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
    QCOMPARE(shapes.indexOf(&shape1), 0);
    QCOMPARE(shapes.indexOf(&shape2), 1);
    QCOMPARE(shapes.indexOf(&shape3), 2);

    QList<KoShape*> selectedShapes;
    selectedShapes.append(&shape3);

    KUndo2Command * cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::BringToFront);
    QVERIFY(cmd == 0);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::RaiseShape);
    QVERIFY(cmd == 0);

    selectedShapes.append(&shape1);
    selectedShapes.append(&shape2);
    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::BringToFront);
    QVERIFY(cmd == 0);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::RaiseShape);
    QVERIFY(cmd == 0);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::LowerShape);
    QVERIFY(cmd == 0);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    QVERIFY(cmd == 0);

    selectedShapes.clear();
    selectedShapes.append(&shape1);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::SendToBack);
    QVERIFY(cmd == 0);

    cmd = KoShapeReorderCommand::createCommand(selectedShapes, &manager, KoShapeReorderCommand::LowerShape);
    QVERIFY(cmd == 0);
}

QTEST_MAIN(TestShapeReorderCommand)
#include "TestShapeReorderCommand.moc"
