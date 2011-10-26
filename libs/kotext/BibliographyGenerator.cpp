/* This file is part of the KDE project
 * Copyright (C) 2011 Smit Patel <smitpatel24@gmail.com>
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

#include "BibliographyGenerator.h"
#include "KoInlineCite.h"

#include <klocale.h>
#include <kdebug.h>
#include <KDebug>

#include <KoInlineTextObjectManager.h>
#include <KoParagraphStyle.h>
#include <KoTextDocument.h>
#include <KoStyleManager.h>
#include <KoTextEditor.h>
#include <KoInlineCite.h>

#include <QTextFrame>

BibliographyGenerator::BibliographyGenerator(QTextDocument *bibDocument, QTextBlock block, KoBibliographyInfo *bibInfo)
    : QObject(bibDocument)
    , m_bibDocument(bibDocument)
    , m_bibInfo(bibInfo)
    , m_block(block)
{
    Q_ASSERT(bibDocument);
    Q_ASSERT(bibInfo);

    m_bibInfo->setGenerator(this);

    bibDocument->setUndoRedoEnabled(false);
    generate();
}

BibliographyGenerator::~BibliographyGenerator()
{
    delete m_bibInfo;
}

static KoParagraphStyle *generateTemplateStyle(KoStyleManager *styleManager,QString bibType) {
    KoParagraphStyle *style = new KoParagraphStyle();
    style->setName("Bibliography_"+bibType);
    style->setParent(styleManager->paragraphStyle("Standard"));
    styleManager->add(style);
    return style;
}

void BibliographyGenerator::generate()
{
    if (!m_bibInfo)
        return;

    QTextCursor cursor = m_bibDocument->rootFrame()->lastCursorPosition();
    cursor.setPosition(m_bibDocument->rootFrame()->firstPosition(), QTextCursor::KeepAnchor);
    cursor.beginEditBlock();

    KoStyleManager *styleManager = KoTextDocument(m_block.document()).styleManager();

    if (!m_bibInfo->m_indexTitleTemplate.text.isNull()) {
        KoParagraphStyle *titleStyle = styleManager->paragraphStyle(m_bibInfo->m_indexTitleTemplate.styleId);
        if (!titleStyle) {
            titleStyle = styleManager->defaultParagraphStyle();
        }

        QTextBlock titleTextBlock = cursor.block();
        titleStyle->applyStyle(titleTextBlock);

        cursor.insertText(m_bibInfo->m_indexTitleTemplate.text);
        cursor.insertBlock();
    }

    QTextCharFormat savedCharFormat = cursor.charFormat();

    QList<KoInlineCite*> citeList;
    if ( KoTextDocument(m_block.document()).styleManager()->bibliographyConfiguration()->sortByPosition() ) {
        citeList = KoTextDocument(m_block.document())
                .inlineTextObjectManager()->citationsSortedByPosition(false, m_block.document()->firstBlock());
    } else {
        citeList = KoTextDocument(m_block.document()).inlineTextObjectManager()->citations(false).values();
    }

    foreach (KoInlineCite *cite, citeList)
    {
        KoParagraphStyle *bibTemplateStyle = 0;
        BibliographyEntryTemplate bibEntryTemplate;
        if (m_bibInfo->m_entryTemplate.keys().contains(cite->bibliographyType())) {

            bibEntryTemplate = m_bibInfo->m_entryTemplate[cite->bibliographyType()];

            bibTemplateStyle = styleManager->paragraphStyle(bibEntryTemplate.styleId);
            if (bibTemplateStyle == 0) {
                bibTemplateStyle = generateTemplateStyle(styleManager, cite->bibliographyType());
            }
        } else {
            qDebug() << "Bibliography meta-data has not BibliographyEntryTemplate for " << cite->bibliographyType();
            continue;
        }

        cursor.insertBlock(QTextBlockFormat(),QTextCharFormat());

        QTextBlock bibEntryTextBlock = cursor.block();
        bibTemplateStyle->applyStyle(bibEntryTextBlock);
        bool spanEnabled = false;           //true if data field is not empty

        foreach (IndexEntry * entry, bibEntryTemplate.indexEntries) {
            switch(entry->name) {
                case IndexEntry::BIBLIOGRAPHY: {
                    IndexEntryBibliography *indexEntry = static_cast<IndexEntryBibliography *>(entry);
                    cursor.insertText(QString(" ").append(cite->dataField(indexEntry->dataField)));
                    spanEnabled = !cite->dataField(indexEntry->dataField).isEmpty();
                    break;
                }
                case IndexEntry::SPAN: {
                    if(spanEnabled) {
                        IndexEntrySpan *span = static_cast<IndexEntrySpan*>(entry);
                        cursor.insertText(span->text);
                    }
                    break;
                }
                case IndexEntry::TAB_STOP: {
                    IndexEntryTabStop *tabEntry = static_cast<IndexEntryTabStop*>(entry);

                    cursor.insertText("\t");

                    QTextBlockFormat blockFormat = cursor.blockFormat();
                    QList<QVariant> tabList;
                    if (tabEntry->m_position == "MAX") {
                        tabEntry->tab.position = m_maxTabPosition;
                    } else {
                        tabEntry->tab.position = tabEntry->m_position.toDouble();
                    }
                    tabList.append(QVariant::fromValue<KoText::Tab>(tabEntry->tab));
                    blockFormat.setProperty(KoParagraphStyle::TabPositions, QVariant::fromValue<QList<QVariant> >(tabList));
                    cursor.setBlockFormat(blockFormat);
                    break;
                }
                default:{
                    qDebug() << "New or unknown index entry";
                    break;
                }
            }
        }// foreach
    }
    cursor.setCharFormat(savedCharFormat);   // restore the cursor char format
}





