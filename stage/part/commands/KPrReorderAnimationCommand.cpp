/* This file is part of the KDE project
 * Copyright (C) 2012 Paul Mendez <paulestebanms@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (  at your option ) any later version.
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


#include "KPrReorderAnimationCommand.h"

#include "KPrShapeAnimations.h"
#include "KPrAnimationsTreeModel.h"
#include "animations/KPrAnimationStep.h"
#include "animations/KPrAnimationSubStep.h"
#include "animations/KPrShapeAnimation.h"
#include "KLocale"

KPrReorderAnimationCommand::KPrReorderAnimationCommand(KPrShapeAnimations *shapeAnimationsModel, KPrAnimationStep *step,
                                                       KPrAnimationStep *newStep, KUndo2Command *parent)
    : KUndo2Command(parent)
    , m_shapeAnimationsModel(shapeAnimationsModel)
    , m_step(step)
    , m_newStep(newStep)
{
    setText(i18nc("(qtundo-format)", "Reorder animations"));
    m_oldRow = m_shapeAnimationsModel->steps().indexOf(m_step);
    m_newRow = m_shapeAnimationsModel->steps().indexOf(m_newStep);
}

KPrReorderAnimationCommand::~KPrReorderAnimationCommand()
{
}

void KPrReorderAnimationCommand::redo()
{
    m_shapeAnimationsModel->swap(m_oldRow, m_newRow);
}

void KPrReorderAnimationCommand::undo()
{
    m_shapeAnimationsModel->swap(m_newRow, m_oldRow);
}
