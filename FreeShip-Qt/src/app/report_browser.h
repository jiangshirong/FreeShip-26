// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "report_layout.h"
#include "studio_ui.h"
#include <QScrollBar>
#include <QTextBrowser>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace report {
class Browser final:public QTextBrowser {
public:
    Browser(){
        applyStudioScrollbars(this);
        setUndoRedoEnabled(false);
        setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    }

    void setReport(const QString& html){
        auto* doc=document();
        if(hasReport_){
            doc->setDefaultFont(baseFont_);
            doc->setDocumentMargin(baseMargin_);
        }
        report::setHtml(doc,html);
        hasReport_=true;
        baseFont_=doc->defaultFont();
        baseMargin_=doc->documentMargin();
        tables_.clear();
        fragments_.clear();
        for(auto* frame:doc->rootFrame()->childFrames())
            if(auto* table=qobject_cast<QTextTable*>(frame))tables_.append({table,table->format()});
        for(auto block=doc->begin();block.isValid();block=block.next()){
            for(auto it=block.begin();!it.atEnd();++it){
                const auto fragment=it.fragment();
                const auto format=fragment.charFormat();
                if(format.isImageFormat()||format.fontPointSize()>0)
                    fragments_.append({fragment.position(),fragment.length(),format});
            }
        }
        applyZoom();
    }

protected:
    void wheelEvent(QWheelEvent* event)override{
        if(!event->modifiers().testFlag(Qt::ControlModifier)){
            QTextBrowser::wheelEvent(event);
            return;
        }
        const qreal steps=event->pixelDelta().isNull()?event->angleDelta().y()/120.:event->pixelDelta().y()/80.;
        if(!document()->isEmpty()&&steps!=0){
            const bool fits=horizontalScrollBar()->maximum()==0&&verticalScrollBar()->maximum()==0;
            const auto next=std::clamp(scale_*std::pow(1.12,steps),.25,2.5);
            if(next!=scale_&&(steps>0||!fits)){
                const auto anchor=cursorForPosition(event->position().toPoint());
                const auto at=cursorRect(anchor).topLeft();
                scale_=next;
                applyZoom();
                const auto movement=cursorRect(anchor).topLeft()-at;
                auto* x=horizontalScrollBar();auto* y=verticalScrollBar();
                x->setValue(x->value()+movement.x());y->setValue(y->value()+movement.y());
            }
        }
        event->accept();
    }

    bool viewportEvent(QEvent* event)override{
        const bool handled=QTextBrowser::viewportEvent(event);
        if(event->type()==QEvent::Resize&&!reflowPending_&&widthUsed_!=viewport()->width()){
            reflowPending_=true;
            QTimer::singleShot(0,this,[this]{reflowPending_=false;fitTableWidths();});
        }
        return handled;
    }

private:
    void fitTableWidths(){
        widthUsed_=viewport()->width();
        // The painted frame can extend beyond its requested width at the
        // last cell. Budget for the table edge as well as the document margins.
        const qreal available=std::max(1.,widthUsed_-2*document()->documentMargin()-2-std::ceil(8*scale_));
        for(const auto& entry:tables_){
            auto layout=entry.original;
            const auto originalWidth=layout.width();
            if(originalWidth.type()==QTextLength::FixedLength)
                layout.setWidth(QTextLength(QTextLength::FixedLength,std::min(available,originalWidth.rawValue()*scale_)));
            else if(originalWidth.type()==QTextLength::PercentageLength)
                layout.setWidth(QTextLength(QTextLength::FixedLength,available*originalWidth.rawValue()/100));
            layout.setCellPadding(entry.original.cellPadding()*scale_);
            layout.setCellSpacing(entry.original.cellSpacing()*scale_);
            auto columns=layout.columnWidthConstraints();
            for(auto& column:columns)if(column.type()==QTextLength::FixedLength)
                column=QTextLength(QTextLength::FixedLength,column.rawValue()*scale_);
            layout.setColumnWidthConstraints(columns);
            if(entry.table->format()!=layout)entry.table->setFormat(layout);
        }
    }

    void applyZoom(){
        auto* doc=document();
        auto font=baseFont_;
        if(font.pointSizeF()>0)font.setPointSizeF(font.pointSizeF()*scale_);
        QTextCursor edit(doc);
        edit.beginEditBlock();
        doc->setDefaultFont(font);
        doc->setDocumentMargin(baseMargin_*scale_);
        for(const auto& fragment:fragments_){
            edit.setPosition(fragment.position);
            edit.setPosition(fragment.position+fragment.length,QTextCursor::KeepAnchor);
            auto style=fragment.format;
            if(style.isImageFormat()){
                auto image=style.toImageFormat();
                if(image.width()>0)image.setWidth(image.width()*scale_);
                if(image.height()>0)image.setHeight(image.height()*scale_);
                style=image;
            }else style.setFontPointSize(style.fontPointSize()*scale_);
            edit.setCharFormat(style);
        }
        fitTableWidths();
        edit.endEditBlock();
    }

    struct TableStyle {QTextTable* table;QTextTableFormat original;};
    struct FragmentStyle {int position,length;QTextCharFormat format;};
    QList<TableStyle> tables_;
    QList<FragmentStyle> fragments_;
    QFont baseFont_;
    qreal baseMargin_=4,scale_=1;
    int widthUsed_=-1;
    bool hasReport_=false,reflowPending_=false;
};
}
