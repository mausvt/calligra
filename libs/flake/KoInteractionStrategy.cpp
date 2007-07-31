/* This file is part of the KDE project
 * Copyright (C) 2006 Thomas Zander <zander@kde.org>
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

#include "KoInteractionStrategy.h"
#include "KoSelection.h"
#include "KoShapeManager.h"
#include "KoPointerEvent.h"
#include "KoShapeRubberSelectStrategy.h"
#include "KoShapeMoveStrategy.h"
#include "KoShapeRotateStrategy.h"
#include "KoShapeShearStrategy.h"
#include "KoShapeResizeStrategy.h"
#include "KoCreateShapeStrategy.h"
#include "KoCreateShapesTool.h"
#include "KoInteractionTool.h"
#include "KoCanvasBase.h"
#include "KoTool.h"
#include "KoShapeContainer.h"

#include <QUndoCommand>

#include <QMouseEvent>

void KoInteractionStrategy::cancelInteraction() {
    QUndoCommand *cmd = createCommand();
    if(cmd) {
        cmd->undo();
        delete cmd;
    }
}

KoInteractionStrategy::KoInteractionStrategy(KoTool *parent, KoCanvasBase *canvas)
: m_parent(parent)
, m_canvas(canvas)
{
}


void KoInteractionStrategy::applyGrid(QPointF &point) {
    // The 1e-10 here is a workaround for some weird division problem.
    // 360.00062366 / 2.83465058 gives 127 'exactly' when shown as a double,
    // but when casting into an int, we get 126. In fact it's 127 - 5.64e-15 !
    double gridX, gridY;
    m_canvas->gridSize(&gridX, &gridY);

    // This is a problem when calling applyGrid twice, we get 1 less than the time before.
    point.setX( static_cast<int>( point.x() / gridX + 1e-10 ) * gridX );
    point.setY( static_cast<int>( point.y() / gridY + 1e-10 ) * gridY );
}

QPointF KoInteractionStrategy::snapToGrid( const QPointF &point, Qt::KeyboardModifiers modifiers ) {
    if( ! m_canvas->snapToGrid() || (modifiers & Qt::ShiftModifier) )
        return point;
    QPointF p = point;
    applyGrid(p);
    return p;
}

// static
KoInteractionStrategy* KoInteractionStrategy::createStrategy(KoPointerEvent *event, KoInteractionTool *parent, KoCanvasBase *canvas) {
    if((event->buttons() & Qt::LeftButton) == 0)
        return 0;  // Nothing to do for middle/right mouse button

    KoCreateShapesTool *crs = dynamic_cast<KoCreateShapesTool*>(parent);
    if(crs)
        return new KoCreateShapeStrategy(crs, canvas, event->point);

    KoShapeManager *shapeManager = canvas->shapeManager();
    KoSelection *select = shapeManager->selection();
    bool insideSelection, editableShape=false;
    KoFlake::SelectionHandle handle = parent->handleAt(event->point, &insideSelection);

    foreach (KoShape* shape, select->selectedShapes()) {
        if( isEditable( shape ) ) {
            editableShape = true;
            break;
        }
    }

    if(editableShape && (event->modifiers() == Qt::NoModifier )) {
        // manipulation of selected shapes goes first
        if(handle != KoFlake::NoHandle) {
            if(insideSelection)
                return new KoShapeResizeStrategy(parent, canvas, event->point, handle);
            if(handle == KoFlake::TopMiddleHandle || handle == KoFlake::RightMiddleHandle ||
                        handle == KoFlake::BottomMiddleHandle || handle == KoFlake::LeftMiddleHandle)
                return new KoShapeShearStrategy(parent, canvas, event->point, handle);
            return new KoShapeRotateStrategy(parent, canvas, event->point);
        }
        // This is wrong now when there is a single rotated object as you get it also when pressing outside of the object
        if(select->boundingRect().contains(event->point))
            return new KoShapeMoveStrategy(parent, canvas, event->point);
    }

    KoShape * object( shapeManager->shapeAt( event->point, (event->modifiers() & Qt::ShiftModifier) ? KoFlake::NextUnselected : KoFlake::ShapeOnTop ) );
    if( !object && handle == KoFlake::NoHandle) {
        if ( ( event->modifiers() & Qt::ControlModifier ) == 0 )
        {
            parent->repaintDecorations();
            select->deselectAll();
        }
        return new KoShapeRubberSelectStrategy(parent, canvas, event->point);
    }

    if(select->isSelected(object)) {
        if ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier ) {
            parent->repaintDecorations();
            select->deselect(object);
        }
    }
    else if(handle == KoFlake::NoHandle) { // clicked on object which is not selected
        parent->repaintDecorations();
        if ( ( event->modifiers() & Qt::ControlModifier ) == 0 )
            shapeManager->selection()->deselectAll();
        select->select(object);
        parent->repaintDecorations();
        return new KoShapeMoveStrategy(parent, canvas, event->point);
    }
    return 0;
}

// static
bool KoInteractionStrategy::isEditable( KoShape * shape ) {
    Q_ASSERT(shape);

    if( !shape || !shape->isVisible() || shape->isLocked() )
        return false;

    KoShapeContainer * parent = shape->parent();

    if(parent && parent->isChildLocked(shape))
        return false;

    while( parent )
    {
        if( ! parent->isVisible() )
            return false;
        parent = parent->parent();
    }

    return true;
}
