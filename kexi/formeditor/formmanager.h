/* This file is part of the KDE project
   Copyright (C) 2004 Cedric Pasteur <cedric.pasteur@free.fr>

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

#ifndef FORMMANAGER_H
#define FORMMANAGER_H

#include <qobject.h>
#include <qdom.h>

template<class type> class QPtrList;
class QWidget;
class QWorkspace;
class KPopupMenu;
class KexiPropertyEditor;
class KActionCollection;
class KAction;

namespace KFormDesigner {

class ObjectPropertyBuffer;
class Form;
class WidgetLibrary;
class ObjectTreeView;
typedef QPtrList<KAction> Actions;

//! A class to manage (create/load/save) Forms
/** This is Form Designer's top class, which is used by external APIs to access FormDesigner. This is the class you have to use
   to integrate FormDesigner into another program.\n
   It deals with creating, saving and loading Form, as well as widget insertion and copying.\n
   It also ensures all the components (ObjectTreeView, Form and PropertyEditor) are synced, and link them.\n
   It holds the WidgetLibrary, the ObjectPropertyBuffer, links to ObjectTreeView and PropertyEditor, as well as the copied widget
   and the insert state.
 **/
class KFORMEDITOR_EXPORT FormManager : public QObject
{
	Q_OBJECT

	public:
		FormManager(QWorkspace *workspace, QObject *parent, const char *name);
		~FormManager(){;}

		/*! Creates all the KAction related to widget insertion, and plug them into the KActionCollection \a parent.
		  These actions are automatically connected to insertWidget() slot.
		  \return a QPtrList of the created actions.
		 */
		Actions createActions(KActionCollection *parent);
		//void createForm(QWidget *toplevel);
		//void loadForm(const QString &filename);

		/*! Sets the external editors used by FormDesigner (as they may be docked). This function also connects
		  appropriate signals and slots to ensure sync with the current Form.
		 */
		void setEditors(KexiPropertyEditor *editor, ObjectTreeView *treeview);

		//! \return A pointer to the WidgetLibrary owned by this Manager.
		WidgetLibrary*    lib() { return m_lib; }
		/*! \return true if one of the insert buttons was pressed and the forms are ready to create a widget. 
		 \return false otherwise.
		 */
		bool              inserting() { return m_inserting; }
		/*! \return The name of the class being inserted, corresponding to the menu item or the toolbar button clicked.
		 */
		QString           insertClass() { return m_insertClass; }

		/*! \return The popup menu to be shown when right-clicking on the form. Each container adds a widget-specific part
		  to this one before showing it. This menu contains Copy/cut/paste/remove.
		 */
		KPopupMenu*       popupMenu() { return m_popup; }
		/*! The Container use this function to indicate the exec point of the contextual menu, which is used to position the 
		  pasted widgets.
		 */
		void              setInsertPoint(const QPoint &p);

		/*! \return The Form actually active and focused.
		 */
		Form*             activeForm();
		/*! \return true if \a w is a toplevel widget, ie it is the main widget of a Form (so it should have a caption ,
		 an icon ...)
		*/
		bool              isTopLevel(QWidget *w);

		//! \return A pointer to the KexiPropertyEditor we use. 
		KexiPropertyEditor* editor() { return m_editor; }

		/*! Creates a new blank Form, whose toplevel widget inherits \a classname. The Form is automatically shown. */
		void createBlankForm(const QString &classname, const char *name);

	public slots:
		/*! Creates a new blank Form with default class top widget (ie QWidget). The new Form is shown and becomes
		   the active Form.
		  */
		void createBlankForm();
		/*! Loads a Form from a UI file. A "Open File" dialog is shown to select the file. The loaded Form is shown and becomes
		   the active Form.
		  */
		void loadForm();
		/*! Save the active Form into a UI file. A "Save File" dialog is shown to choose a name for the file, and the former name
		  is used if there is one (not yet).
		 */
		void saveForm();

		/*! Deletes the selected widget in active Form and all of its children. */
		void deleteWidget();
		/*! Copies the slected widget and all its children of the active Form using an XML representation. */
		void copyWidget();
		/*! Cuts (ie Copies and deletes) the slected widget and all its children of the active Form using an XML representation. */
		void cutWidget();
		/*! Pastes the XML representation of the copied or cut widget. The widget is pasted when the user clicks the Form to
		  indicate the new position of the widget, or at the position of the contextual menu if there is one.
		 */
		void pasteWidget();

		/*! This slot is called when the user presses a "Widget" toolbar button or a "Widget" menu item. Prepares all Forms for 
		  creation of a new widget (ie changes cursor ...).
		 */
		void insertWidget(const QString &classname);
		/*! Stopts the current widget insertion (ie unset the cursor ...). */
		void stopInsert();

		/*! Print to the command line the ObjectTree of the active Form (ie a line for each widget, with parent and name). */
		void debugTree();
		/*! This slot is called when the selected Form changes. It updates the ObjectTreeView to show this Form. */
		void updateTreeView(QWidget *);

	private:
		ObjectPropertyBuffer	*m_buffer;
		WidgetLibrary		*m_lib;
		KexiPropertyEditor	*m_editor;
		ObjectTreeView		*m_treeview;
		QPtrList<Form>		m_forms;
		QWorkspace		*m_workspace;

		QDomDocument		m_domDoc;
		KPopupMenu		*m_popup;
		QPoint			m_insertPoint;

		bool			m_inserting;
		QString			m_insertClass;
};

}

#endif

