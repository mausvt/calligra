/*
    kostep -- handles changetracking using operational transformation for calligra
    Copyright (C) 2013  Luke Wolf <Lukewolf101010@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "StepAddCommand.h"
#include "../StepSteps.h"

StepAddCommand::StepAddCommand (QTextCursor caret, QString text, StepStepStack *changeStack) : StepCommand (caret, changeStack)
{
    StepAddTextStep step (text);
    finalize (step);

}
StepAddCommand::StepAddCommand (QTextCursor caret, StepStepStack *changestack) : StepCommand (caret, changestack)
{
    StepAddTextBlockStep step = new StepAddTextBlockStep ();
    finalize (step);

}

void StepAddCommand::finalize (StepStepBase &step)
{
    StepStepLocation location (caret);
    step.setLocation (location);

    changeStack->push (step);
    step =0;
    changeStack = 0;
}
