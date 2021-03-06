/* This file is part of the KDE project
 * Copyright (C) 2006 Jan Hambrecht <jaham@gmx.net>
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

#ifndef CALLOUTTOOLFACTORY_H
#define CALLOUTTOOLFACTORY_H

#include <KoToolFactoryBase.h>
#include <KoPathTool.h>


/// Factory for the KoPathTool
class CalloutToolFactory : public KoToolFactoryBase
{
public:
    CalloutToolFactory();
    ~CalloutToolFactory();

    KoToolBase *createTool(KoCanvasBase *canvas);
};

class CalloutPathTool : public KoPathTool
{
    Q_OBJECT
public:
    CalloutPathTool(KoCanvasBase *canvas);

    QList<QPointer<QWidget> > createOptionWidgets();

public Q_SLOTS:
    void activate(ToolActivation toolActivation, const QSet<KoShape*> &shapes);
};

#endif
