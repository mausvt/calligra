/* This file is part of the KDE project
   Copyright (C)  2001 Montel Laurent <lmontel@mandrakesoft.com>

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

#ifndef __KoChangeCaseDia__
#define __KoChangeCaseDia__

#include <kdialogbase.h>

class QRadioButton;
class QPushButton;

class KoChangeCaseDia : public KDialogBase
{
    Q_OBJECT
public:
    KoChangeCaseDia( QWidget *parent, const char *name );
    enum TypeOfCase { UpperCase =0, LowerCase=1, TitleCase=2, ToggleCase=3};
    TypeOfCase getTypeOfCase();

protected:
    QRadioButton *m_upperCase;
    QRadioButton *m_titleCase;
    QRadioButton *m_lowerCase;
    QRadioButton *m_toggleCase;
};

#endif
