/* This file is part of the KOffice project

   Copyright (C) 2009 Benjamin Cail <cricketc@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the Library GNU General Public
   version 2 of the License, or (at your option) version 3 or,
   at the discretion of KDE e.V (which shall act as a proxy as in
   section 14 of the GPLv3), any later version..

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef PARAGRAPH_H
#define PARAGRAPH_H

#include <QBuffer>
#include <QList>
#include <KoGenStyle.h>
#include <KoGenStyles.h>
#include <KoXmlWriter.h>

#include <wv2/src/styles.h>
#include <wv2/src/paragraphproperties.h>
#include <wv2/src/parser.h>

class Paragraph
{
public:
    explicit Paragraph(KoGenStyles* mainStyles, bool inStylesDotXml = false, bool isHeading = false, bool inHeader = false, int outlineLevel = 0);
    ~Paragraph();

    /**
     * Write the paragraph content into the @writer.
     *
     * While applying the paragraph style, store the tab leader (leader-text)
     * chacter into @tabLeader if requested by the calling handler.
     *
     * @return the name of the last KoGenStyle inserted into the styles
     * collection.
     */
    QString writeToFile(KoXmlWriter* writer, QChar* tabLeader=0);

    void addRunOfText(QString text,  wvWare::SharedPtr<const wvWare::Word97::CHP> chp, QString fontName, const wvWare::StyleSheet& styles, bool addCompleteElement=false);
    void openInnerParagraph();
    void closeInnerParagraph();

    /**
     * Set the named style from the stylesheet that applies to this paragraph.
     */
    void setParagraphStyle(const wvWare::Style* paragraphStyle);

    /**
     * Set the paragraph properties (PAP) that apply to this paragraph.
     */
    void setParagraphProperties(wvWare::SharedPtr<const wvWare::ParagraphProperties> properties);

    KoGenStyle* getOdfParagraphStyle();
    bool containsPageNumberField() const {
        return m_containsPageNumberField;
    }
    void setContainsPageNumberField(bool containsPageNumberField) {
        m_containsPageNumberField = containsPageNumberField;
    }

    typedef enum { NoDropCap, IsDropCapPara, HasDropCapIntegrated }  DropCapStatus;
    DropCapStatus dropCapStatus() const;
    void getDropCapData(QString *string, int *type, int *lines, qreal *distance, QString *style) const;
    void addDropCap(QString &string, int type, int lines, qreal distance, QString style);

    // debug:
    int  strings() const;
    QString string(int index) const;

    void setCombinedCharacters(bool isCombined);

    // Static functions for parsing wvWare properties and applying
    // them onto a KoGenStyle.
    static void applyParagraphProperties(const wvWare::ParagraphProperties& properties,
                                         KoGenStyle* style, const wvWare::Style* parentStyle,
                                         bool setDefaultAlign, Paragraph *paragraph, QChar* tabLeader=0);
    static void applyCharacterProperties(const wvWare::Word97::CHP* chp,
                                         KoGenStyle* style, const wvWare::Style* parentStyle,
                                         QString bgColor,
                                         bool suppressFontSize=false, bool combineCharacters=false);

    /**
     * Set the actual background-color to val.
     */
    static void setBgColor(QString val) { m_bgColor = val; }

    /**
     * A special purpose method, which creates a KoGenStyle for a <text:span>
     * element and inserts it into the styles collection.  Use this function if
     * you have to create XML snippets.  In any other case use addRunOfText.
     * @return the style name.
     */
    QString createTextStyle(wvWare::SharedPtr<const wvWare::Word97::CHP> chp, const wvWare::StyleSheet& styles);

private:
    wvWare::SharedPtr<const wvWare::ParagraphProperties> m_paragraphProperties;
    wvWare::SharedPtr<const wvWare::ParagraphProperties> m_paragraphProperties2;

    // ODF styles.  The MS equivalents are below.
    KoGenStyle* m_odfParagraphStyle; //pointer to KOffice structure for paragraph formatting
    KoGenStyle* m_odfParagraphStyle2; //place to store original style when we have an inner paragraph
    KoGenStyles* m_mainStyles; //pointer to style collection for this document

    // MS Styles
    const wvWare::Style* m_paragraphStyle;  // style for the paragraph
    const wvWare::Style* m_paragraphStyle2; // style when in inner paragraph

    //std::vector<QString> m_textStrings; // list of text strings within a paragraph
    //std::vector<QString> m_textStrings2; // original list when in inner paragraph
    QList<QString> m_textStrings; // list of text strings within a paragraph
    QList<QString> m_textStrings2; // original list when in inner paragraph
    std::vector<const KoGenStyle*> m_textStyles; // list of styles for text within a paragraph
    std::vector<const KoGenStyle*> m_textStyles2; // original list when in inner paragraph
    std::vector<bool> m_addCompleteElement;         // list of flags if we should output the complete parahraph instead of processing it
    std::vector<bool> m_addCompleteElement2;        // original list when in inner paragraph

    bool m_inStylesDotXml; //let us know if we're in content.xml or styles.xml
    bool m_isHeading; //information for writing a heading instead of a paragraph
    // (odt looks formats them similarly)
    int m_outlineLevel;

    DropCapStatus  m_dropCapStatus; // True if this paragraph has a dropcap 
    int   m_dcs_fdct;
    int   m_dcs_lines;
    qreal m_dropCapDistance;
    QString m_dropCapStyleName;
    bool m_inHeaderFooter;
    bool m_containsPageNumberField;
    bool m_combinedCharacters;            // is true when the next characters are combined

    //background-color of the current paragraph or the parent element
    static QString m_bgColor;

}; //end class Paragraph
#endif //PARAGRAPH_H
