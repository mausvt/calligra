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

#ifndef BACKDIA_H
#define BACKDIA_H

#include <qdialog.h>
#include <qcolor.h>
#include <qstring.h>
#include <qframe.h>

#include "global.h"

class QLabel;
class QGroupBox;
class QComboBox;
class QPushButton;
class KColorButton;
class QButtonGroup;
class QSlider;
class KPBackGround;
class KPresenterDoc;
class QResizeEvent;
class QPainter;
class QRadioButton;
class QCheckBox;

/*******************************************************************
 *
 * Class: BackPreview
 *
 *******************************************************************/

class BackPreview : public QFrame
{
    Q_OBJECT

public:
    BackPreview( QWidget *parent, KPresenterDoc *doc );

    KPBackGround *backGround() const {
	return back;
    }

protected:
    void resizeEvent( QResizeEvent *e );
    void drawContents( QPainter *p );

private:
    KPBackGround *back;

};

/******************************************************************/
/* class BackDia						  */
/******************************************************************/
class BackDia : public QDialog
{
    Q_OBJECT

public:
    BackDia( QWidget* parent, const char* name,
	     BackType backType, QColor backColor1,
	     QColor backColor2, BCType _bcType,
	     QString backPic, QString backClip,
	     BackView backPicView, bool _unbalanced,
	     int _xfactor, int _yfactor, KPresenterDoc *doc );

    QColor getBackColor1();
    QColor getBackColor2();
    BCType getBackColorType();
    BackType getBackType();
    QString getBackPixFilename();
    QString getBackClipFilename();
    BackView getBackView();
    bool getBackUnbalanced();
    int getBackXFactor();
    int getBackYFactor();

private:
    QLabel *lPicName, *picPreview, *lClipName;
    QCheckBox *unbalanced;
    QComboBox *cType, *backCombo, *picView;
    QPushButton *okBut, *applyBut, *applyGlobalBut, *cancelBut;
    QPushButton *picChoose, *clipChoose;
    KColorButton *color1Choose, *color2Choose;
    QSlider *xfactor, *yfactor;
    QString chosenPic;
    QString chosenClip;
    BackPreview *preview;
    bool picChanged, clipChanged, lockUpdate;

private slots:
    void selectPic();
    void selectClip();
    void updateConfiguration();

    void Ok() { emit backOk( FALSE ); }
    void Apply() { emit backOk( FALSE ); }
    void ApplyGlobal() { emit backOk( TRUE ); }

signals:
    void backOk( bool );

};
#endif //BACKDIA_H
