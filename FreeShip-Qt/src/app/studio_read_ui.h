// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_ui.h"
#include "math_notation.h"
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QHeaderView>
#include <QStyleOptionViewItem>
#include <cmath>

inline void styleStudioReadDialog(QDialog* dialog){
    dialog->setStyleSheet(
        "QDialog{background:#f5f8f5;color:#213f37} QLabel{color:#213f37}"
        "QFrame#ReadPanel{background:white;border:1px solid #dce7e0;border-radius:8px}"
        "QTableWidget{background:white;alternate-background-color:white;color:#213f37;border:0;gridline-color:transparent}"
        "QTableWidget::item{padding:8px;border:0;border-bottom:1px solid #edf1ee} QTableWidget::item:selected{background:#e5f1eb;color:#213f37}"
        "QHeaderView::section{background:#e4eee7;color:#315d50;border:0;padding:12px 10px;font-weight:500}"
        "QToolButton[copy_email]{border:1px solid #cfddd4;border-radius:6px;background:#f6f9f5;padding:6px}"
        "QToolButton[copy_email]:hover{background:#e3eee6;border-color:#9ebaae}"
        "QToolButton[copy_email]:pressed{background:#cfdfd5}"
        "QToolButton[copy_email]:focus{border:2px solid #007e74}");
}
inline QWidget* studioReadHeading(const QString& text,const QString& icon,QWidget* parent,QLabel** titleLabel=nullptr){
    auto* header=new QWidget(parent);auto* row=new QHBoxLayout(header);row->setContentsMargins(0,0,0,0);row->setSpacing(14);
    auto* mark=new QLabel;mark->setFixedSize(40,40);mark->setAlignment(Qt::AlignCenter);
    mark->setStyleSheet("background:#e1eee6;border-radius:8px");mark->setPixmap(studioIcon(icon).pixmap(QSize(24,24),parent->devicePixelRatioF()));
    auto* title=new QLabel(text);auto font=parent->font();font.setPointSizeF(17);font.setWeight(QFont::DemiBold);title->setFont(font);
    if(titleLabel)*titleLabel=title;
    row->addWidget(mark);row->addWidget(title);row->addStretch();return header;
}

struct StudioKeyPart{QString text;QRectF rect;bool key=false;};
inline QVector<StudioKeyPart> studioKeyParts(const QString& text,const QFont& font,int width){
    QFontMetricsF metrics(font);const qreal space=6,line=std::ceil(metrics.height()+14),limit=qMax(80,width-24);
    QVector<StudioKeyPart> parts;qreal x=12,y=7,rowHeight=line;bool first=true;
    for(const auto& combination:text.split('/')){
        auto tokens=QRegularExpression("[^+]+|[+]").globalMatch(combination);QStringList group;
        qreal groupWidth=0;
        while(tokens.hasNext()){const auto token=tokens.next().captured().trimmed();if(token.isEmpty())continue;group.append(token);groupWidth+=qMin(limit,std::ceil(metrics.horizontalAdvance(token))+(token=="+"?8:20))+space;}
        if(x>12&&groupWidth<=limit&&x+groupWidth+(first?0:18)>limit+12){x=12;y+=rowHeight+6;rowHeight=line;}
        if(!first){parts.append({"/",{x,y,12,line},false});x+=18;}first=false;
        for(const auto& token:group){
        const bool key=token!="+"&&token!="/";const qreal w=qMin(limit,std::ceil(metrics.horizontalAdvance(token))+(key?20:8));
        if(x>12&&x+w>limit+12){x=12;y+=rowHeight+6;rowHeight=line;}
        const qreal h=key?qMax(line,metrics.boundingRect(QRectF(0,0,qMax(1.,w-16),1000),Qt::TextWordWrap,token).height()+12):line;
        rowHeight=qMax(rowHeight,h);parts.append({token,{x,y,w,h},key});x+=w+space;
        }
    }return parts;
}
inline void studioPaintKeyPart(QPainter& painter,const StudioKeyPart& part,const QRectF& rect){
    if(part.key){
        painter.setBrush(QColor("#b5cbbf"));painter.setPen(Qt::NoPen);painter.drawRoundedRect(rect,5,5);
        painter.setBrush(QColor("#ffffff"));painter.setPen(QPen(QColor("#cadbd0"),1));painter.drawRoundedRect(rect.adjusted(.5,.5,-.5,-2.5),5,5);
    }
    painter.setPen(QColor(part.key?"#254e42":"#688378"));
    painter.drawText(rect.adjusted(part.key?8:0,part.key?2:0,part.key?-8:0,part.key?-4:0),Qt::AlignCenter|Qt::TextWordWrap,part.text);
}
class StudioKeycap final:public QWidget{
public:
    explicit StudioKeycap(const QString& key,QWidget* parent=nullptr):QWidget(parent),key_(key){
        setObjectName("StudioKeycap");setAccessibleName(key);setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    }
    QSize sizeHint()const override{return studioKeyParts(key_,font(),1000).first().rect.size().toSize();}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setFont(font());
        studioPaintKeyPart(painter,{key_,{},true},QRectF(rect()).adjusted(0,0,-.5,-.5));
    }
private:
    QString key_;
};
class StudioKeycapDelegate final:public QStyledItemDelegate{
public:
    explicit StudioKeycapDelegate(QTableWidget* table):QStyledItemDelegate(table),table_(table){}
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        auto base=option;initStyleOption(&base,index);base.text.clear();base.state&=~QStyle::State_HasFocus;
        table_->style()->drawControl(QStyle::CE_ItemViewItem,&base,painter,table_);
        painter->save();painter->setRenderHint(QPainter::Antialiasing);painter->setFont(option.font);
        for(const auto& part:studioKeyParts(index.data().toString(),option.font,option.rect.width())){
            const auto rect=part.rect.translated(option.rect.topLeft());
            studioPaintKeyPart(*painter,part,rect);
        }painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        qreal height=40;for(const auto& part:studioKeyParts(index.data().toString(),option.font,table_->columnWidth(index.column())))height=qMax(height,part.rect.bottom()+7);
        return {100,int(std::ceil(height))};
    }
private:QTableWidget* table_;
};
class StudioShortcutTable final:public QTableWidget{
public:using QTableWidget::QTableWidget;
protected:void resizeEvent(QResizeEvent* event)override{
        QTableWidget::resizeEvent(event);const int width=viewport()->width();
        setColumnWidth(0,width*30/100);setColumnWidth(1,width*45/100);resizeRowsToContents();
    }
};
// Cell widgets present the same data with richer hierarchy. Keep the item's
// text for inspection/accessibility without painting it beneath that widget.
class StudioWidgetCellDelegate final:public QStyledItemDelegate{
public:explicit StudioWidgetCellDelegate(QTableWidget* table,const QColor& divider={},int wrapColumn=-1)
    :QStyledItemDelegate(table),table_(table),divider_(divider),wrapColumn_(wrapColumn){}
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        // These are read-only information cells, not actionable selections.
        // Keep table keyboard navigation and child controls' focus indicators;
        // suppress only the native current-cell focus rectangle during painting.
        auto base=option;base.state&=~QStyle::State_HasFocus;
        if(!table_->cellWidget(index.row(),index.column())){
            if(index.column()==wrapColumn_&&table_->columnSpan(index.row(),index.column())==1){
                initStyleOption(&base,index);
                const auto text=base.text;base.text.clear();
                table_->style()->drawControl(QStyle::CE_ItemViewItem,&base,painter,table_);
                painter->save();painter->setFont(base.font);
                painter->setPen(base.palette.color(base.state.testFlag(QStyle::State_Selected)?QPalette::HighlightedText:QPalette::Text));
                notation::draw(*painter,base.rect.adjusted(4,0,-4,0),Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap,text);
                painter->restore();
            }else QStyledItemDelegate::paint(painter,base,index);
            if(divider_.isValid()){
                painter->save();painter->setPen(divider_);painter->drawLine(option.rect.bottomLeft(),option.rect.bottomRight());painter->restore();
            }
            return;
        }
        initStyleOption(&base,index);base.text.clear();base.icon={};table_->style()->drawControl(QStyle::CE_ItemViewItem,&base,painter,table_);
    }
private:QTableWidget* table_;QColor divider_;int wrapColumn_;
};
