/*
 *  Copyright (c) 2005 Gábor Lehel <illissius@gmail.com>
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

#ifndef KIS_POPULATE_VISITOR_H
#define KIS_POPULATE_VISITOR_H

#include <kiconloader.h>
#include "kis_types.h"
#include "kis_layer.h"
#include "kis_group_layer.h"
#include "kis_paint_layer.h"
#include "kis_part_layer.h"
#include "kis_adjustment_layer.h"
#include "kis_layerlist.h"


class KisPopulateVisitor: public KisLayerVisitor
{
    public:
        KisPopulateVisitor(KisLayerList* widget)
            : m_widget(widget)
            , m_parent(0)
            { }

        KisPopulateVisitor(KisLayerItem* parent)
            : m_widget(parent -> listView())
            , m_parent(parent)
            { }

        virtual bool visit(KisPaintLayer* layer)
        {
            add(layer);
            return true;
        }

        virtual bool visit(KisPartLayer* layer)
        {
            add(layer) -> setPixmap(0, SmallIcon("gear", 16));
            return true;
        }

        virtual bool visit(KisAdjustmentLayer* layer)
        {
            add(layer) -> setPixmap(0, SmallIcon("filter", 16));
            return true;
        }

        virtual bool visit(KisGroupLayer* layer)
        {
            KisLayerItem* item = add(layer);
            item -> makeFolder();
            KisPopulateVisitor visitor(item);
            for (KisLayerSP l = layer -> firstChild(); l; l = l -> nextSibling())
                l -> accept(visitor);
            return true;
        }

    private:
        LayerList* m_widget;
        KisLayerItem* m_parent;

        KisLayerItem* add(KisLayer* layer)
        {
            KisLayerItem *item;
            if (m_parent)
                item = new KisLayerItem(m_parent, layer);
            else
                item = new KisLayerItem(m_widget, layer);
            if (layer == layer -> image() -> activeLayer())
                item -> setActive();
            return item;
        }
};

#endif
