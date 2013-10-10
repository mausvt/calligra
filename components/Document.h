/*
 * This file is part of the KDE project
 * Copyright (C) 2013 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef CALLIGRA_COMPONENTS_DOCUMENT_H
#define CALLIGRA_COMPONENTS_DOCUMENT_H

#include <QtCore/QObject>

#include "Global.h"

class QGraphicsWidget;
class KoFindBase;

namespace Calligra {
namespace Components {

/**
 * \brief The Document object provides a loader for Calligra Documents.
 *
 */
class Document : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Global::DocumentType documentType READ documentType NOTIFY sourceChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum State {
        UnloadedState,
        LoadingState,
        LoadedState,
        FailedState,
    };
    Q_ENUMS(State);

    Document(QObject* parent = 0);
    ~Document();

    /**
     * @{
     *
     * \property source
     */
    QUrl source() const;
    void setSource(const QUrl& value);
    /**
     * @}
     */

    Global::DocumentType documentType() const;

    State state() const;

    /**
     * \defgroup internal Internal Methods
     * The methods are used internally by the components and not exposed
     * to QML.
     * @{
     */
    KoFindBase* finder() const;
    QGraphicsWidget* canvas() const;

    /**
     * @}
     */
Q_SIGNALS:
    void sourceChanged();
    void stateChanged();

private:
    class Private;
    Private* const d;
};

} // Namespace Components
} // Namespace Calligra

Q_DECLARE_METATYPE(Calligra::Components::Document*)

#endif // CALLIGRA_COMPONENTS_DOCUMENT_H
