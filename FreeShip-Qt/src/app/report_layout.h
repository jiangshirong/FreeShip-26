#pragma once

#include <QList>
#include <QStringList>
#include <algorithm>
#include "message_style.h"
#include "math_notation.h"
#include "numeric_range_text.h"
#include "text_style.h"
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTextBlock>
#include <QTextImageFormat>
#include <QTextFragment>
#include <QImage>
#include <QUrl>

namespace report {
struct Table {
    QString title;
    QStringList headers;
    QList<QStringList> rows;
    QList<int> leftColumns;
    QList<int> widthPercent{};
    int htmlWidth=0;
};

inline int textWidth(const QString& text) {
    int width=0;
    for(const auto code:text.toUcs4()) {
        const auto category=QChar::category(code);
        if(category==QChar::Mark_NonSpacing||category==QChar::Mark_Enclosing)continue;
        const bool wide=(code>=0x1100&&code<=0x115f)||(code>=0x2e80&&code<=0xa4cf)||
            (code>=0xac00&&code<=0xd7a3)||(code>=0xf900&&code<=0xfaff)||
            (code>=0xfe10&&code<=0xfe6f)||(code>=0xff01&&code<=0xff60)||
            (code>=0xffe0&&code<=0xffe6)||(code>=0x20000&&code<=0x3fffd);
        width+=wide?2:1;
    }
    return width;
}

inline QString heading(const QString& text, bool html, bool title=false) {
    if(text.isEmpty())return {};
    if(!html)return text+"\n\n";
    if(title)return "<h2 style='color:#174944'>"+text.toHtmlEscaped()+"</h2>";
    return "<h3 style='color:#174944; margin-top:14px; margin-bottom:6px'>"+text.toHtmlEscaped()+"</h3>";
}

inline QString metadata(QString text, bool html) {
    text=formatNumericRanges(text);
    if(text.trimmed().isEmpty())return {};
    if(!html)return text.trimmed()+"\n\n";
    return "<p>"+text.trimmed().toHtmlEscaped().replace("\r\n","\n").replace('\r','\n').replace('\n',"<br>")+"</p>";
}

// Hints require an explicit role; plain metadata is intentionally separate.
inline QString message(QString text,bool html,StudioMessageRole role,const QString& title={}){
    text=formatNumericRanges(text);
    if(text.trimmed().isEmpty())return {};
    if(!html)return heading(title,false)+text.trimmed()+"\n\n";
    const auto style=studioMessageStyle(role);
    const auto body=(title.isEmpty()?QString{}:"<b>"+title.toHtmlEscaped()+"</b><br>")+
        text.trimmed().toHtmlEscaped().replace("\r\n","\n").replace('\r','\n').replace('\n',"<br>");
    return QString("<table width='100%' cellspacing='0' cellpadding='10' bgcolor='%1' style='border:1px solid %2;margin-top:10px;margin-bottom:10px'>"
        "<tr><td width='24' valign='top'><img src='studio-message:%3' width='16' height='16'></td>"
        "<td valign='top' style='color:%4;white-space:pre-wrap'>%5</td></tr></table><p style='margin:4px'></p>")
        .arg(style.background.name(),style.border.name(),style.name,style.foreground.name(),body);
}
inline void setHtml(QTextDocument* document,const QString& html){
    document->setDefaultFont(studioTextFont(document->defaultFont()));
    const auto resources=[&]{for(auto role:{StudioMessageRole::Explanation,StudioMessageRole::Feedback,StudioMessageRole::Warning}){
        QImage icon(64,64,QImage::Format_ARGB32_Premultiplied);icon.fill(Qt::transparent);QPainter p(&icon);p.scale(4,4);paintStudioMessageMark(p,role);p.end();
        document->addResource(QTextDocument::ImageResource,QUrl(QString("studio-message:")+studioMessageStyle(role).name),icon);
    }};
    resources();document->setHtml(notation::htmlDocument(html,document->defaultFont().pointSizeF()>0?document->defaultFont().pointSizeF():10.5));resources();
    // Rich-text headings and <th>/<b> create explicit formats. Normalize those
    // too; changing only the document default leaves explicit font formats.
    QTextCursor all(document);all.select(QTextCursor::Document);QTextCharFormat smooth;
    smooth.setFontHintingPreference(QFont::PreferNoHinting);all.mergeCharFormat(smooth);
    const auto base=document->defaultFont().pointSizeF();
    if(base>0)for(auto block=document->begin();block.isValid();block=block.next())if(block.blockFormat().headingLevel()==3){
        for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();auto section=fragment.charFormat();
            if(section.isImageFormat()||section.verticalAlignment()!=QTextCharFormat::AlignNormal)continue;
            // Qt's <h3> relative-size property otherwise overrides an explicit
            // point size. Preserve each fragment's remaining math/link styles.
            section.clearProperty(QTextFormat::FontSizeAdjustment);section.setFontPointSize(base*1.30);
            section.setFontWeight(QFont::Bold);section.setFontHintingPreference(QFont::PreferNoHinting);
            QTextCursor heading(document);heading.setPosition(fragment.position());heading.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);heading.setCharFormat(section);
        }
    }
}
inline QString messageRole(const QTextTable* table){
    if(!table||table->rows()!=1||table->columns()!=2)return {};
    const auto cell=table->cellAt(0,0);
    for(auto block=cell.firstCursorPosition().block();block.isValid()&&block.position()<=cell.lastCursorPosition().position();block=block.next())
        for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();
            if(!fragment.charFormat().isImageFormat())continue;
            const auto name=fragment.charFormat().toImageFormat().name();
            for(const auto* role:{"explanation","feedback","warning"})if(name==QString("studio-message:")+role)return role;
        }
    return {};
}

inline QString render(const Table& table, bool html) {
    if(table.rows.isEmpty())return {};
    QString result=html?heading(table.title,true):table.title.isEmpty()?QString{}:table.title+'\n';
    const int columns=table.headers.size();
    if(!columns)return result;
    if(html) {
        result+="<table width='"+(table.htmlWidth>0?QString::number(table.htmlWidth):QString("100%"))+"' cellspacing='0' cellpadding='5'><thead>";
        const auto append=[&](const QStringList& row, const QString& tag, const QString& color) {
            result+="<tr style='background-color:"+color+"'>";
            for(int i=0;i<columns;++i) {
                auto cell=row.value(i).toHtmlEscaped();
                if(tag=="th"&&row.value(i).endsWith(')')) {
                    const int unit=cell.lastIndexOf(" (");
                    if(unit>=0)cell=cell.left(unit)+"<br>"+cell.mid(unit+2,cell.size()-unit-3);
                }
                const QString alignment=table.leftColumns.contains(i)?"left":"right";
                const int width=table.widthPercent.value(i);
                const auto size=tag=="th"&&width>0?QString(" width='%1%'").arg(width):QString{};
                result+='<'+tag+" align='"+alignment+"' valign='middle'"+size+" style='border-bottom:1px solid #e1e8e4'>"+cell+"</"+tag+'>';
            }
            result+="</tr>";
        };
        append(table.headers,"th","#e0eee8");
        result+="</thead>";
        for(int i=0;i<table.rows.size();++i)
            append(table.rows[i],"td","#ffffff");
        return result+"</table>";
    }
    QList<int> widths;
    for(const auto& header:table.headers)widths.append(textWidth(header));
    for(const auto& row:table.rows)
        for(int i=0;i<columns;++i)widths[i]=std::max(widths[i],textWidth(row.value(i)));
    const auto append=[&](const QStringList& row) {
        QStringList cells;
        for(int i=0;i<columns;++i) {
            const auto cell=row.value(i);
            const auto padding=QString(widths[i]-textWidth(cell),' ');
            cells.append(table.leftColumns.contains(i)?cell+padding:padding+cell);
        }
        result+=cells.join("  |  ")+"\n";
    };
    QStringList rules;
    for(int width:widths)rules.append(QString(width,'-'));
    append(table.headers);
    result+=rules.join("--+--")+'\n';
    for(const auto& row:table.rows)append(row);
    return result+'\n';
}
}
