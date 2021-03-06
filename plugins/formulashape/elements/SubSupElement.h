/* This file is part of the KDE project
   Copyright (C) 2006 Martin Pfeiffer <hubipete@gmx.net>
   Copyright (C) 2006 Alfredo Beaumont Sainz <alfredo.beaumont@gmail.com>

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
   Boston, MA 02110-1301, USA.
*/

#ifndef SUBSUPELEMENT_H
#define SUBSUPELEMENT_H

#include "FixedElement.h"
#include "koformula_export.h"

/**
 * @short Implementation of the msub, msup, msubsup elements
 */
class KOFORMULA_EXPORT SubSupElement : public FixedElement {
public:
    /// The standard constructor
    explicit SubSupElement( BasicElement* parent = 0, ElementType elementType = SubSupScript);

    /// The destructor
    ~SubSupElement();

    /**
     * Obtain a list of all child elements of this element
     * @return a QList with pointers to all child elements
     */
    const QList<BasicElement*> childElements() const;

    /// inherited from BasicElement
    virtual bool replaceChild ( BasicElement* oldElement, BasicElement* newElement );

    /**
     * Render the element to the given QPainter
     * @param painter The QPainter to paint the element to
     * @param am AttributeManager containing style info
     */
    void paint( QPainter& painter, AttributeManager* am );

    /**
     * Calculate the size of the element and the positions of its children
     * @param am The AttributeManager providing information about attributes values
     */
    void layout( const AttributeManager* am );

    /// @return The default value of the attribute for this element
    QString attributesDefaultValue( const QString& attribute ) const; 

    /// @return The element's ElementType
    ElementType elementType() const;

    virtual int endPosition() const;

    virtual bool moveCursor ( FormulaCursor& newcursor, FormulaCursor& oldcursor );

    virtual bool setCursorTo ( FormulaCursor& cursor, QPointF point );
protected:
    /// Read all content from the node
    bool readMathMLContent( const KoXmlElement& element );

    /// Write all content to the KoXmlWriter
    void writeMathMLContent( KoXmlWriter* writer, const QString& ns ) const;

private:
    /// The base element 
    RowElement* m_baseElement;

    /// The subscript right to the m_baseElement
    RowElement* m_subScript;

    /// The superscript right to the m_baseElement
    RowElement* m_superScript;

    /// Whether this is a SubScript, SupScript or SubSupScript
    ElementType m_elementType;
};

#endif // SUBSUPELEMENT_H
