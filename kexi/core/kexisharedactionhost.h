/* This file is part of the KDE project
   Copyright (C) 2004 Jarosław Staniek <staniek@kde.org>

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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#ifndef KEXISHAREDACTIONHOST_H
#define KEXISHAREDACTIONHOST_H

#include <KStandardAction>

#include <QAction>

#include "kexicore_export.h"

class QKeySequence;
class KGuiItem;
class KActionCollection;
class KexiMainWindowIface;
class KexiActionProxy;
class KexiSharedActionHostPrivate;

namespace KexiPart
{
class Part;
}

//! Acts as application-wide host that offers shared actions.
/*!
 You can inherit this class together with KexiMainWindow.
 Call setAsDefaultHost() to make the host default for all shared actions that have
 not explicitly specified host.

 For example how all this is done, see KexiMainWindow implementation.

 \sa KexiActionProxy, KexiMainWindow */
class KEXICORE_EXPORT KexiSharedActionHost
{
public:

    /*! Constructs host for main window \a mainWin. */
    explicit KexiSharedActionHost(KexiMainWindowIface* mainWin);

    virtual ~KexiSharedActionHost();

    /*! Performs lookup like in KexiSharedActionHost::focusWindow()
     but starting from \a w instead of a widget returned by QWidget::focusWidget().
     \return NULL if no widget matches acceptsSharedActions() or if \a w is NULL. */
    virtual QWidget* findWindow(QWidget *w);

    /*! \return window widget that is currently focused (using QWidget::focusWidget())
     and matches acceptsSharedActions(). If focused widget does not match,
     it's parent, grandparent, and so on is checked. If all this fails,
     or no widget has focus, 0 is returned. */
    QWidget* focusWindow();

    /*! Sets this host as default shared actions host. */
    void setAsDefaultHost();

    /*! \return default shared actions host, used when no host
     is explicitly specified for shared actions.
     There can be exactly one deault shared actions host. */
    static KexiSharedActionHost* defaultHost();

    /*! \return shared actions list. */
    QList<QAction *> sharedActions() const;

    /*! PROTOTYPE, DO NOT USE YET */
    void setActionVolatile(QAction *a, bool set);

protected:
    /*! Invalidates all shared actions declared using createSharedAction().
     Any shared action will be enabled if \a o (typically: a child window or a dock window)
     has this action plugged _and_ it is available (i.e. enabled).
     Otherwise the action is disabled.

     If \a o is not KexiWindow or its child,
     actions are only invalidated if these come from mainwindow's KActionCollection
     (thus part-actions are untouched when the focus is e.g. in the Property Editor.

     Call this method when it is known that some actions need invalidation
     (e.g. when new window is activated). See how it is used in KexiMainWindow. */
    virtual void invalidateSharedActions(QObject *o);

    void setActionAvailable(const QString& action_name, bool avail);

    /*! Plugs shared actions proxy \a proxy for this host. */
    void plugActionProxy(KexiActionProxy *proxy);

    /*! Updates availability of action \a action_name for object \a obj.
     Object is mainly the window. */
    void updateActionAvailable(const QString& action_name, bool avail, QObject *obj);

    /*! \return main window for which this host is defined. */
    KexiMainWindowIface* mainWindow() const;

    /*! Creates shared action using \a text, \a pix_name pixmap, shortcut \a cut,
     optional \a name. You can pass your own action collection as \a col.
     If \a col action collection is null, main window's action will be used.
     Pass desired QAction subclass with \a subclassName (e.g. "KToggleAction") to have
     that subclass allocated instead just QAction (what is the default).
     Created action's data is owned by the main window. */
    QAction * createSharedAction(const QString &text, const QString &iconName,
                                const QKeySequence &cut, const char *name, KActionCollection* col = 0,
                                const char *subclassName = 0);

    /*! Like above - creates shared action, but from standard action identified by \a id.
     Action's data is owned by the main window. */
    QAction * createSharedAction(KStandardAction::StandardAction id, const char *name = 0,
                                KActionCollection* col = 0);

    /*! Creates shared action with name \a name and shortcut \a cut
     by copying all properties of \a guiItem.
     If \a col action collection is null, main window's action will be used. */
    QAction * createSharedAction(const KGuiItem& guiItem, const QKeySequence &cut, const char *name,
                                KActionCollection* col = 0);

    /*! \return action proxy for object \a o, or NULL if this object has
     no plugged shared actions. */
    KexiActionProxy* actionProxyFor(QObject *o) const;

    /*! Like actionProxyFor(), but takes the proxy from the host completely.
     This is called by KExiActionProxy on its destruction. */
    KexiActionProxy* takeActionProxyFor(QObject *o);

private:
    /*! Helper function for createSharedAction(). */
    QAction * createSharedActionInternal(QAction *action);

    KexiSharedActionHostPrivate *d;

    friend class KexiActionProxy;
    friend class KexiPart::Part;
    friend class KexiView;
    friend class KexiWindow;
};

#endif
