/*
 *
 *  Copyright (C) 2011 Torio Mlshi <mlshi@lavabit.com>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ANIMATOR_IMPORTER_H
#define ANIMATOR_IMPORTER_H

#include <QObject>
#include <KoCanvasObserverBase.h>

#include <kis_image_manager.h>

#include "animator_manager.h"

class AnimatorImporter : public QObject, public KoCanvasObserverBase
{
    Q_OBJECT
    
public:
    AnimatorImporter(AnimatorManager *manager);
    virtual ~AnimatorImporter();

public:
    virtual void unsetCanvas();
    virtual void setCanvas(KoCanvasBase *canvas);
    
public:
    virtual void importUser();
    
    virtual void importBetween(KisNodeSP from, KisNodeSP to);
    
private:
    AnimatorManager *m_manager;
    KisImageManager *m_imageManager;
};

#endif // ANIMATOR_IMPORTER_H
