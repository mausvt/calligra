/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Reginald Stadlbauer <reggie@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef gotopage_h
#define gotopage_h

#include <qframe.h>
#include <qvaluelist.h>

class QWidget;
class QObject;
class QEvent;
class QResizeEvent;
class QComboBox;
class QLabel;

/******************************************************************/
/* class KPGotoPage                                               */
/******************************************************************/

class KPGotoPage : public QFrame
{
    Q_OBJECT

public:
    KPGotoPage( const QValueList<int> &slides, int start,
                QWidget *parent = 0L, const char *name = 0L, WFlags f = 0 );

    bool eventFilter( QObject *obj, QEvent *e );

    int getPage() { return page; }

    static int gotoPage( const QValueList<int> &slides, int start, QWidget *parent = 0L );

protected:
    void resizeEvent( QResizeEvent *e );

    QComboBox *spinbox;
    QLabel *label;

    int _default, page;

};


#endif
