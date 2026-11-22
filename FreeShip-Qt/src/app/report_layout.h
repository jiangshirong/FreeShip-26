#pragma once

#include <QList>
#include <QStringList>
#include <algorithm>
#include "message_style.h"
#include "math_notation.h"
#include "numeric_range_text.h"
#include "text_style.h"
#include "studio_table.h"
#include <QFontMetricsF>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTextBlock>
#include <QTextImageFormat>
#include <QTextFragment>
#include <QRegularExpression>
#include <QImage>
#include <QUrl>
#include <QtMath>
#include <numeric>

namespace report {
struct Table {
    QString title;
    QStringList headers;
    QList<QStringList> rows;
    QList<int> leftColumns;
    QList<int> widthPercent{};
    int htmlWidth=0;
    QList<StudioHeaderGroup> groups{};
};

struct PdfTableLayout {
    QFont font;
    int availableWidth;
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

inline QString pdfTable(const Table& table,const PdfTableLayout& layout){
    constexpr int padding=5;
    const auto font=layout.font;
    auto headingFont=font;headingFont.setBold(true);
    const auto width=[](const QString& text,const QFont& font){
        const QFontMetricsF metrics(font);
        qreal longest=0;
        for(const auto& line:text.split('\n'))
            longest=std::max(longest,notation::contains(line)?notation::size(line,font).width():metrics.horizontalAdvance(line));
        return qCeil(longest)+2*padding+4;
    };
    const auto wordWidth=[&](const QString& text,const QFont& font){
        static const QRegularExpression spaces("\\s+");
        int longest=width({},font);
        for(const auto& word:text.split(spaces,Qt::SkipEmptyParts))longest=std::max(longest,width(word,font));
        return longest;
    };
    auto captions=table.headers;
    for(const auto& group:table.groups)
        for(int i=0;i<group.labels.size();++i)captions[group.first+i]=group.labels[i];
    const int columns=captions.size();
    QList<int> widths,withoutBodyWrap,minimum;
    for(int col=0;col<columns;++col){
        const int headingWidth=width(captions[col],headingFont);
        int bodyWidth=0,bodyWordWidth=0;
        for(const auto& row:table.rows){
            const auto text=row.value(col);
            bodyWidth=std::max(bodyWidth,width(text,font));
            bodyWordWidth=std::max(bodyWordWidth,wordWidth(text,font));
        }
        const int headingWordWidth=wordWidth(captions[col],headingFont);
        widths.append(std::max(headingWidth,bodyWidth));
        withoutBodyWrap.append(std::max(headingWordWidth,bodyWidth));
        minimum.append(std::max(headingWordWidth,bodyWordWidth));
    }
    const auto fitGroup=[](QList<int>& columns,const StudioHeaderGroup& group,int titleWidth){
        const auto first=columns.begin()+group.first;
        const int count=group.labels.size();
        const int extra=titleWidth-std::accumulate(first,first+count,0);
        for(int i=0;i<count&&extra>0;++i)first[i]+=extra/count+(i<extra%count);
    };
    for(const auto& group:table.groups){
        const int natural=width(group.title,headingFont),word=wordWidth(group.title,headingFont);
        fitGroup(widths,group,natural);
        fitGroup(withoutBodyWrap,group,word);
        fitGroup(minimum,group,word);
    }
    const auto sum=[](const QList<int>& sizes){return std::accumulate(sizes.cbegin(),sizes.cend(),0);};
    int total=sum(widths);
    if(total>layout.availableWidth){
        const auto& lower=sum(withoutBodyWrap)<=layout.availableWidth?withoutBodyWrap:minimum;
        const int spare=layout.availableWidth-sum(lower);
        if(spare>=0){
            int adaptable=0;
            for(int col=0;col<columns;++col)adaptable+=std::max(0,widths[col]-lower[col]);
            for(int col=0;col<columns;++col)
                widths[col]=lower[col]+(adaptable?int(qint64(std::max(0,widths[col]-lower[col]))*spare/adaptable):0);
            total=sum(widths);
        }
    }
    const bool fits=total<=layout.availableWidth;
    const auto escape=[](const QString& text){return text.toHtmlEscaped().replace('\n',"<br>");};
    const auto columnWidth=[&](int col){
        return " width='"+(fits?QString::number(widths[col]):QString::number(100.*widths[col]/total,'f',3)+"%")+"'";
    };
    const auto header=[&](const QString& text,const QString& attributes){
        return "<th align='center' valign='middle'"+attributes+" style='border:1px solid #d6e3dc'>"+escape(text)+"</th>";
    };
    const QString headerRow="<tr style='background-color:#e0eee8'>";
    QString result="<table width='"+(fits?QString::number(total):QString("100%"))+
        "' cellspacing='0' cellpadding='"+QString::number(padding)+"'><thead>"+headerRow;
    QString lowerRow;
    for(int col=0;col<columns;){
        const auto group=std::find_if(table.groups.begin(),table.groups.end(),[col](const auto& g){return g.first==col;});
        if(group==table.groups.end()){
            result+=header(captions[col],columnWidth(col)+(table.groups.isEmpty()?"":" rowspan='2'"));
            ++col;
            continue;
        }
        result+=header(group->title,QString(" colspan='%1'").arg(group->labels.size()));
        for(int i=0;i<group->labels.size();++i,++col)lowerRow+=header(captions[col],columnWidth(col));
    }
    result+="</tr>";
    if(!lowerRow.isEmpty())result+=headerRow+lowerRow+"</tr>";
    result+="</thead>";
    for(const auto& row:table.rows){
        result+="<tr>";
        for(int col=0;col<columns;++col){
            const auto alignment=table.leftColumns.contains(col)?"left":"right";
            result+=QString("<td align='%1' valign='middle' style='border:1px solid #e1e8e4'>%2</td>")
                .arg(alignment,escape(row.value(col)));
        }
        result+="</tr>";
    }
    return result+"</table>";
}

inline QString render(const Table& table, bool html,const PdfTableLayout* pdf=nullptr) {
    if(table.rows.isEmpty())return {};
    QString result=html?heading(table.title,true):table.title.isEmpty()?QString{}:table.title+'\n';
    const int columns=table.headers.size();
    if(!columns)return result;
    if(html&&pdf)return result+pdfTable(table,*pdf);
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
