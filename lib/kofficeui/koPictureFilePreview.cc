/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Reginald Stadlbauer <reggie@kde.org>
   Copyright (C) 2002 Nicolas GOUTTE <nicog@snafu.de>

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

#include "koPictureFilePreview.h"
#include "koPictureFilePreview.moc"
#include <kdialog.h>
#include <klocale.h>
#include <kurl.h>
#include <qbitmap.h>
#include <qlayout.h>
#include <qfileinfo.h>
#include <qpainter.h>
#include <qscrollview.h>

#include <kdebug.h>

#include <koPicture.h>

/**
 * This class implements the actual widget that shows the image.
 * It is a scrollview, to have scrollbars if the image is big,
 * and it supports both pixmaps and cliparts
 */
class KoPictureFilePreviewWidget : public QScrollView
{
public:
    KoPictureFilePreviewWidget( QWidget *parent )
        : QScrollView( parent ) { viewport()->setBackgroundMode( PaletteBase ); }

    bool setPicture( const QString & filename )
    {
        KoPicture picture;
        if (picture.loadFromFile( filename ))
        {
            m_size = picture.getOriginalSize();
            m_picture = picture;
            resizeContents( m_size.width(), m_size.height() );
            repaintContents();
            return true;
        }
        else
            return false;
    }

    void setNullPicture(void)
    {
        m_picture=KoPicture();
        m_size=QSize();
    }

    void drawContents( QPainter *p, int, int, int, int )
    {
        p->setBackgroundColor( Qt::white );
        // Be sure that the background is white (for transparency)
        p->fillRect(0, 0, m_size.width(), m_size.height(), QBrush( Qt::white ));
        m_picture.draw( *p, 0 ,0, m_size.width(), m_size.height());
    }

private:
    KoPicture m_picture;
    QSize m_size;
};

KoPictureFilePreview::KoPictureFilePreview( QWidget *parent )
    : KPreviewWidgetBase( parent )
{
    QVBoxLayout *vb = new QVBoxLayout( this, KDialog::marginHint() );
    m_widget = new KoPictureFilePreviewWidget( this );
    vb->addWidget( m_widget, 1 );
}

void KoPictureFilePreview::showPreview( const KURL &u )
{
    if ( u.isLocalFile() ) {
        QString path = u.path();
        m_widget->setPicture( path );
    } else {
        // ## TODO support for remote URLs
        m_widget->setNullPicture();
    }
}

void KoPictureFilePreview::clearPreview()
{
    m_widget->setNullPicture();
}

QString KoPictureFilePreview::clipartPattern()
{
    return i18n( "*.svg *.wmf|Clipart (*.svg *.wmf)" );
}

QStringList KoPictureFilePreview::clipartMimeTypes()
{
    QStringList lst;
    lst << "image/svg+xml";
    lst << "image/x-wmf";
    // TODO; image/x-qpicture
    return lst;
}
