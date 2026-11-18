// Explicit report-message evidence; no classification inferred from prose.
#pragma once
#include "app/report_layout.h"
#include <QAbstractTextDocumentLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextEdit>

inline QJsonArray reportMessageProbe(const QTextEdit* browser){
    QJsonArray rows;if(!browser)return rows;
    auto* document=browser->document();
    for(auto* frame:document->rootFrame()->childFrames()){
        auto* table=qobject_cast<QTextTable*>(frame);const auto role=report::messageRole(table);
        if(role.isEmpty())continue;
        auto cursor=table->cellAt(0,1).firstCursorPosition();
        cursor.setPosition(table->cellAt(0,1).lastCursorPosition().position(),QTextCursor::KeepAnchor);
        auto text=cursor.selectedText();text.replace(QChar::ParagraphSeparator,'\n');text.replace(QChar::LineSeparator,'\n');
        const auto bounds=document->documentLayout()->frameBoundingRect(table);
        rows.append(QJsonObject{{"role",role},{"text",text},
            {"background",table->format().background().color().name()},
            {"foreground",table->cellAt(0,1).firstCursorPosition().charFormat().foreground().color().name()},
            {"icon_resource",!document->resource(QTextDocument::ImageResource,QUrl("studio-message:"+role)).isNull()},
            {"width",bounds.width()},{"height",bounds.height()},{"viewport_width",browser->viewport()->width()},
            {"visible",browser->isVisible()},{"renderer","text_document"}});
    }
    return rows;
}

inline QJsonArray reportSubscriptProbe(const QTextEdit* browser){
    QJsonArray rows;if(!browser)return rows;
    for(auto block=browser->document()->begin();block.isValid();block=block.next())
        for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();
            if(fragment.charFormat().verticalAlignment()!=QTextCharFormat::AlignSubScript)continue;
            rows.append(QJsonObject{{"block",block.text()},{"text",fragment.text()},{"italic",fragment.charFormat().fontItalic()},{"position",fragment.position()}});
        }
    return rows;
}
