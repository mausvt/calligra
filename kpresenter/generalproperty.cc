// -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4; -*-
/* This file is part of the KDE project
   Copyright (C) 2005 Thorsten Zachmann <zachmann@kde.org>

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
#include "generalproperty.h"

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>

#include <knuminput.h>

#include "generalpropertyui.h"

GeneralProperty::GeneralProperty( QWidget *parent, const char *name, GeneralValue &generalValue, KoUnit::Unit unit )
: QWidget( parent, name )
, m_ratio( 1.0 )
, m_generalValue( generalValue )
, m_unit( unit )
{
    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->addWidget( m_ui = new GeneralPropertyUI( this ) );

    if ( m_generalValue.m_name.isNull() )
    {
        m_ui->nameLabel->setEnabled( false );
        m_ui->nameInput->setEnabled( false );
    }
    else
    {
        m_ui->nameInput->setText( m_generalValue.m_name );
    }

    connect( m_ui->protect, SIGNAL( toggled( bool ) ), this, SLOT( slotProtectToggled( bool ) ) );
    connect( m_ui->keepRatio, SIGNAL( toggled( bool ) ), this, SLOT( slotKeepRatioToggled( bool ) ) );

    m_ui->widthInput->setRange( 0, 9999, 1, false );
    connect( m_ui->widthInput, SIGNAL( valueChanged( double ) ), this, SLOT( slotWidthChanged( double ) ) );
    m_ui->heightInput->setRange( 0, 9999, 1, false );
    connect( m_ui->heightInput, SIGNAL( valueChanged( double ) ), this, SLOT( slotHeightChanged( double ) ) );

    slotReset();
}


GeneralProperty::~GeneralProperty()
{
}


int GeneralProperty::getGeneralPropertyChange() const
{
    int flags = 0;

    if ( !m_generalValue.m_name.isNull() && m_generalValue.m_name != m_ui->nameInput->text() )
        flags |= Name;

    if ( m_ui->protect->state() != QButton::NoChange )
    {
        if ( ( m_ui->protect->isOn() ? STATE_ON : STATE_OFF ) != m_generalValue.m_protect )
            flags |= Protect;

        if ( !m_ui->protect->isOn() )
        {
            KoRect rect = getRect();
            if ( m_generalValue.m_rect.left() != rect.left() )
                flags |= Left;
            if ( m_generalValue.m_rect.top() != rect.top() )
                flags |= Top;
            if ( m_generalValue.m_rect.width() != rect.width() )
                flags |= Width;
            if ( m_generalValue.m_rect.height() != rect.height() )
                flags |= Height;
        }
    }

    if ( m_ui->keepRatio->state() != QButton::NoChange
         && ( m_ui->keepRatio->isOn() ? STATE_ON : STATE_OFF ) != m_generalValue.m_keepRatio )
    {
        flags |= KeepRatio;
    }

    return flags;
}


GeneralProperty::GeneralValue GeneralProperty::getGeneralValue() const
{
    GeneralValue generalValue;
    generalValue.m_name = m_ui->nameInput->isEnabled() ? m_ui->nameInput->text() : QString();
    generalValue.m_protect = m_ui->protect->isOn() ? STATE_ON : STATE_OFF;
    generalValue.m_keepRatio = m_ui->keepRatio->isOn() ? STATE_ON : STATE_OFF;
    generalValue.m_rect = getRect();
    return generalValue;
}


void GeneralProperty::apply()
{
    int flags = getGeneralPropertyChange();

    if ( flags & Name )
        m_generalValue.m_name = m_ui->nameInput->text();

    if ( flags & Protect )
        m_generalValue.m_protect = m_ui->protect->isOn() ? STATE_ON : STATE_OFF;

    if ( flags & KeepRatio )
        m_generalValue.m_keepRatio = m_ui->keepRatio->isOn() ? STATE_ON : STATE_OFF;

    KoRect rect = getRect();
    if ( flags & Left )
        m_generalValue.m_rect.setLeft( rect.left() );

    if ( flags & Top )
        m_generalValue.m_rect.setTop( rect.top() );

    if ( flags & Width )
        m_generalValue.m_rect.setWidth( rect.width() );

    if ( flags & Height )
        m_generalValue.m_rect.setHeight( rect.height() );
}


KoRect GeneralProperty::getRect() const
{
    double x = QMAX( 0, KoUnit::fromUserValue( m_ui->xInput->value(), m_unit ) );
    double y = QMAX( 0, KoUnit::fromUserValue( m_ui->yInput->value(), m_unit ) );
    double w = QMAX( 0, KoUnit::fromUserValue( m_ui->widthInput->value(), m_unit ) );
    double h = QMAX( 0, KoUnit::fromUserValue( m_ui->heightInput->value(), m_unit ) );

    KoRect rect( x, y, w, h );
    return rect;
}


void GeneralProperty::setRect( KoRect &rect )
{
    m_ui->xInput->setValue( KoUnit::toUserValue( QMAX( 0.00, rect.left() ), m_unit ) );
    m_ui->yInput->setValue( KoUnit::toUserValue( QMAX( 0.00, rect.top() ), m_unit ) );
    m_ui->widthInput->setValue( KoUnit::toUserValue( QMAX( 0.00, rect.width() ), m_unit ) );
    m_ui->heightInput->setValue( KoUnit::toUserValue( QMAX( 0.00, rect.height() ), m_unit ) );
}


void GeneralProperty::slotReset()
{
    switch ( m_generalValue.m_protect )
    {
        case STATE_ON:
            m_ui->protect->setChecked( true );
            break;
        case STATE_OFF:
            m_ui->protect->setChecked( false );
            break;
        case STATE_UNDEF:
            m_ui->protect->setTristate( true );
            m_ui->protect->setNoChange();
            break;
        default:
            m_ui->protect->setChecked( false );
            break;
    }

    switch ( m_generalValue.m_keepRatio )
    {
        case STATE_ON:
            m_ui->keepRatio->setChecked( true );
            break;
        case STATE_OFF:
            m_ui->keepRatio->setChecked( false );
            break;
        case STATE_UNDEF:
            m_ui->keepRatio->setTristate( true );
            m_ui->keepRatio->setNoChange();
            break;
        default:
            m_ui->keepRatio->setChecked( false );
            break;
    }

    setRect( m_generalValue.m_rect );
    // this is done due to the rounding so we can detect a change
    m_generalValue.m_rect = getRect();
}


void GeneralProperty::slotProtectToggled( bool state )
{
    m_ui->positionGroup->setEnabled( !state );
    m_ui->sizeGroup->setEnabled( !state );
}


void GeneralProperty::slotKeepRatioToggled( bool state )
{
    if ( state )
    {
        if ( m_ui->widthInput->value() == 0 )
        {
            m_ratio = 1.0;
        }
        else
        {
            m_ratio = m_ui->heightInput->value() / m_ui->widthInput->value();
        }
    }
}


void GeneralProperty::slotWidthChanged( double value )
{
    if ( m_ui->keepRatio->isChecked() )
    {
        m_ui->heightInput->setValue( value * m_ratio );
    }
}


void GeneralProperty::slotHeightChanged( double value )
{
    if ( m_ui->keepRatio->isChecked() && m_ratio != 0 )
    {
        m_ui->widthInput->setValue( value / m_ratio );
    }
}


#include "generalproperty.moc"
