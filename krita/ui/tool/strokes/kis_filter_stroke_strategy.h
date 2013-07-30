/*
 *  Copyright (c) 2013 Dmitry Kazakov <dimula73@gmail.com>
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

#ifndef __KIS_FILTER_STROKE_STRATEGY_H
#define __KIS_FILTER_STROKE_STRATEGY_H

#include "kis_types.h"
#include "kis_painter_based_stroke_strategy.h"


class KRITAUI_EXPORT KisFilterStrokeStrategy : public KisPainterBasedStrokeStrategy
{
public:
    class Data : public KisStrokeJobData {
    public:
        Data(const QRect &_processRect, bool concurrent)
            : KisStrokeJobData(concurrent ? CONCURRENT : SEQUENTIAL),
              processRect(_processRect) {}
        QRect processRect;
    };

    class CancelSilentlyMarker : public KisStrokeJobData {
    public:
        CancelSilentlyMarker()
            : KisStrokeJobData(SEQUENTIAL)
        {}
    };

public:
    KisFilterStrokeStrategy(KisFilterSP filter,
                            KisSafeFilterConfigurationSP filterConfig,
                            KisResourcesSnapshotSP resources);
    ~KisFilterStrokeStrategy();

    void initStrokeCallback();
    void doStrokeCallback(KisStrokeJobData *data);
    void cancelStrokeCallback();
    void finishStrokeCallback();


private:
    struct Private;
    Private* const m_d;
};

#endif /* __KIS_FILTER_STROKE_STRATEGY_H */
