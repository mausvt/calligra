/*
 *  Copyright (c) 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "presethistory_dock.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QImage>

#include <klocale.h>

#include <KoCanvasResourceManager.h>
#include <KoCanvasBase.h>

#include "kis_canvas2.h"
#include "KisViewManager.h"
#include "kis_paintop_box.h"
#include "kis_paintop_presets_chooser_popup.h"
#include "kis_canvas_resource_provider.h"
#include <kis_paintop_preset.h>
#include <kis_types.h>

#define ICON_SIZE 48

PresetHistoryDock::PresetHistoryDock( )
    : QDockWidget(i18n("Brush Preset History"))
    , m_canvas(0)
    , m_block(false)
{
    m_presetHistory = new QListWidget(this);
    m_presetHistory->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    m_presetHistory->setDragEnabled(false);
    m_presetHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_presetHistory->setSelectionMode(QAbstractItemView::SingleSelection);
    m_presetHistory->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setWidget(m_presetHistory);

    connect(m_presetHistory, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(presetSelected(QListWidgetItem*)));
}

void PresetHistoryDock::setCanvas(KoCanvasBase * canvas)
{
    setEnabled(canvas != 0);

    if (m_canvas) {
        m_canvas->disconnectCanvasObserver(this);
        disconnect(m_canvas->resourceManager());
    }

    m_canvas = dynamic_cast<KisCanvas2*>(canvas);

    if (!m_canvas || !m_canvas->viewManager() || !m_canvas->resourceManager()) return;

    connect(canvas->resourceManager(), SIGNAL(canvasResourceChanged(int,QVariant)), SLOT(canvasResourceChanged(int,QVariant)));
}

void PresetHistoryDock::presetSelected(QListWidgetItem *item)
{
    if (item) {
        QVariant v = item->data(Qt::UserRole);
        KisPaintOpPresetSP preset = v.value<KisPaintOpPresetSP>();
        m_block = true;
        m_canvas->viewManager()->paintOpBox()->resourceSelected(preset.data());
        m_block = false;
    }
}

void PresetHistoryDock::canvasResourceChanged(int key, const QVariant& /*v*/)
{
    if (m_block) return;

    if (m_canvas && key == KisCanvasResourceProvider::CurrentPaintOpPreset) {
        KisPaintOpPresetSP preset = m_canvas->resourceManager()->resource(KisCanvasResourceProvider::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();
        if (preset) {
            if (m_presetHistory->count() > 0) {
                if (preset->name() == m_presetHistory->item(0)->text()) {
                    return;
                }
            }
            QListWidgetItem *item = new QListWidgetItem(QPixmap::fromImage(preset->image()), preset->name());
            QVariant v = QVariant::fromValue<KisPaintOpPresetSP>(preset);
            item->setData(Qt::UserRole, v);
            m_presetHistory->insertItem(0, item);
            m_presetHistory->setCurrentRow(0);
            if (m_presetHistory->count() > 10) {
                m_presetHistory->takeItem(10);
            }
        }
    }
}

#include "presethistory_dock.moc"