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

#include "kis_file_layer_test.h"

#include <qtest_kde.h>

#include <KoColor.h>

#include <kis_file_layer.h>
#include <kis_transform_mask.h>
#include <kis_transform_mask_params_interface.h>

#include <testutil.h>

bool checkDeviceImpl(KisPaintDeviceSP device, KisImageSP image, const QString &testName, const QString &prefix) {
    return TestUtil::checkQImageExternal(device->convertToQImage(0, image->bounds()),
                                         "file_layer",
                                         prefix,
                                         testName, 1, 1, 100);
}

bool checkImage(KisImageSP image, const QString &testName, const QString &prefix) {
    return checkDeviceImpl(image->projection(), image, testName, prefix);
}

void KisFileLayerTest::testFileLayerPlusTransformMaskOffImage()
{
    QRect refRect(0,0,640,441);
    QRect fillRect(400,400,100,100);
    TestUtil::MaskParent p(refRect);

    QString refName(TestUtil::fetchDataFileLazy("hakonepa.png"));
    KisLayerSP flayer = new KisFileLayer(p.image, "", refName, KisFileLayer::None, "flayer", OPACITY_OPAQUE_U8);

    p.image->addNode(flayer, p.image->root(), KisNodeSP());

    QTest::qWait(2000);
    p.image->waitForDone();

    KisTransformMaskSP mask1 = new KisTransformMask();
    p.image->addNode(mask1, flayer);

    mask1->setName("mask1");

    flayer->setDirty(refRect);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "00_initial_layer_update", "flayer_tmask_offimage"));

    QTest::qWait(4000);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "00X_initial_layer_update", "flayer_tmask_offimage"));


    flayer->setX(580);
    flayer->setY(400);

    flayer->setDirty(refRect);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "01_file_layer_moved", "flayer_tmask_offimage"));

    QTest::qWait(4000);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "01X_file_layer_moved", "flayer_tmask_offimage"));


    QTransform transform = QTransform::fromTranslate(-580, -400);
    mask1->setTransformParams(KisTransformMaskParamsInterfaceSP(
                                  new KisDumbTransformMaskParams(transform)));


    /**
     * NOTE: here we see our image cropped by 1.5 image size rect!
     *       That is expected and controlled by
     *       KisImageConfig::transformMaskOffBoundsReadArea()
     *       parameter
     */

    mask1->setDirty(refRect);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "02_mask1_moved_mask_update", "flayer_tmask_offimage"));

    QTest::qWait(4000);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "02X_mask1_moved_mask_update", "flayer_tmask_offimage"));
}

void KisFileLayerTest::testFileLayerPlusTransformMaskSmallFileBigOffset()
{
    QRect refRect(0,0,2000,1500);
    QRect fillRect(400,400,100,100);
    TestUtil::MaskParent p(refRect);

    QString refName(TestUtil::fetchDataFileLazy("file_layer_source.png"));
    KisLayerSP flayer = new KisFileLayer(p.image, "", refName, KisFileLayer::None, "flayer", OPACITY_OPAQUE_U8);

    p.image->addNode(flayer, p.image->root(), KisNodeSP());

    QTest::qWait(2000);
    p.image->waitForDone();

    KisTransformMaskSP mask1 = new KisTransformMask();
    p.image->addNode(mask1, flayer);

    mask1->setName("mask1");

    flayer->setDirty(refRect);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "00_initial_layer_update", "flayer_tmask_huge_offset"));

    QTest::qWait(4000);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "00X_initial_layer_update", "flayer_tmask_huge_offset"));

    QTransform transform;

    transform = QTransform::fromTranslate(1200, 300);
    mask1->setTransformParams(KisTransformMaskParamsInterfaceSP(
                                  new KisDumbTransformMaskParams(transform)));

    mask1->setDirty(refRect);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "01_mask1_moved_mask_update", "flayer_tmask_huge_offset"));

    QTest::qWait(4000);
    p.image->waitForDone();
    QVERIFY(checkImage(p.image, "01X_mask1_moved_mask_update", "flayer_tmask_huge_offset"));
}

QTEST_KDEMAIN(KisFileLayerTest, GUI)