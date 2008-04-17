/* This file is part of the KDE project
 * Copyright (C) 2008 Girish Ramakrishnan <girish@forwardbias.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KOTEXTDEBUG_H
#define KOTEXTDEBUG_H

#include "kotext_export.h"

class QTextDocument;
class QTextFrame;
class QTextBlock;
class QTextTable;
class QTextFragment;

class KOTEXT_EXPORT KoTextDebug
{
public:
    static void dumpDocument(QTextDocument *document);
    static void dumpFrame(QTextFrame *frame);
    static void dumpBlock(const QTextBlock &block);
    static void dumpTable(QTextTable *);
    static void dumpFragment(const QTextFragment &fragment);

private:
    KoTextDebug();
    KoTextDebug( const KoTextDebug& );
    KoTextDebug operator=( const KoTextDebug& );

    static int depth;
    static const int INDENT;
};

#endif /* KOTEXTDEBUG_H */
