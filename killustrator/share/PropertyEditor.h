/* -*- C++ -*-

  $Id$

  This file is part of KIllustrator.
  Copyright (C) 1998 Kai-Uwe Sattler (kus@iti.cs.uni-magdeburg.de)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Library General Public License as
  published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef PropertyEditor_h_
#define PropertyEditor_h_

#include <qdialog.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qwidgetstack.h>
#include <qslider.h>

#include <ktabctl.h>
#include <kcolordlg.h>
#include <kcolorbtn.h>

class QComboBox;
class GDocument;
class CommandHistory;
class UnitBox;
class Gradient;
class BrushCells;
class FontSelector;

class QRadioButton;

class PropertyEditor : public QDialog {
  Q_OBJECT
public:
  PropertyEditor (CommandHistory* history, GDocument* doc,
		  QWidget* parent = 0L, const char* name = 0L);

  static int edit (CommandHistory* history, GDocument* doc);

protected:
  QWidget* createInfoWidget (QWidget* parent);
  QWidget* createOutlineWidget (QWidget* parent);
  QWidget* createFillWidget (QWidget* parent);
  QWidget* createFontWidget (QWidget* parent);

private slots:
  void applyPressed ();
  void helpPressed ();
  void fillStyleChanged ();
  void gradientColorChanged (const QColor&);
  void gradientStyleChanged (int);

private:
  void readProperties ();
  void updateGradient ();

  GDocument* document;
  //  GObject* object;
  CommandHistory* cmdHistory;
  bool haveObjects, haveTextObjects, haveLineObjects,
    haveEllipseObjects, haveRectangleObjects;
  QString text;

  KTabCtl* tabctl;

  // Info Tab
  QLabel* infoLabel[5];

  // OutlinePen Tab
  UnitBox *widthField;
  //  ColorComboBox* penColorField;
  KColorButton* penColorBttn;
  QComboBox* penStyleField;
  QComboBox *leftArrows, *rightArrows;
  QPushButton *ellipseKind[3];
  QPushButton *textAlign[3];
  QSlider* roundnessSlider;

  // Fill Tab
  QRadioButton *fillStyleBttn[5];
  QComboBox *gradStyleCombo;
  KColorButton *solidColorBttn, *patternColorBttn,
    *gradColor1Bttn, *gradColor2Bttn;
  QLabel *gradPreview;
  QWidgetStack *wstack;
  Gradient *gradient;
  BrushCells *brushCells;

  // Font Tab
  FontSelector* fontSelector;
};

#endif
