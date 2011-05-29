/* This file is part of the KDE project
 * Copyright (C) 2011 Aakriti Gupta <aakriti.a.gupta@gmail.com>
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
#define FrameId "Frame"

#include <QString>

class Frame {
  public:
    Frame();
   ~Frame();
   
   void setTitle(QString *title);
   void setRefId(QString *refId); //Data type for ref id of groups/shapes?
   void setSequence(int seq);
   void setZoomPercent(qreal zoomFactor);
   void setTransitionStyle(QString *transitionStyle); //-2,-1,0,1,2 ?
   void setTransitionDuration(qreal timeMs);
   
   QString* getTitle();
   QString* getRefId(); 
   int getSequence();
   qreal getZoomPercent();
   QString* getTransitionStyle(); 
   qreal getTransitionDuration();
   
private:
  QString *title;
  QString *refId;
  QString *transitionStyle;
  
  int sequence;
  
  qreal zoomFactor;
  qreal timeMs;
};