// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_ui.h"
#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolTip>
#include <algorithm>

class SolveStatusDelegate final:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        QStyleOptionViewItem cell(option);initStyleOption(&cell,index);
        const auto icon=cell.icon;cell.icon={};cell.features&=~QStyleOptionViewItem::HasDecoration;
        const auto* style=cell.widget?cell.widget->style():QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&cell,painter,cell.widget);
        const int side=std::min({cell.rect.width(),cell.rect.height(),std::max(12,cell.fontMetrics.height())});
        icon.paint(painter,QStyle::alignedRect(cell.direction,Qt::AlignCenter,QSize(side,side),cell.rect));
    }
};
class SolveStatusTable final:public QTableWidget {
public:
    SolveStatusTable(){
        setMouseTracking(true);viewport()->setMouseTracking(true);
        setItemDelegateForColumn(0,new SolveStatusDelegate(this));
        for(auto* scroll:{horizontalScrollBar(),verticalScrollBar()})
            connect(scroll,&QScrollBar::valueChanged,this,[this]{clearStatusTip();});
        connect(model(),&QAbstractItemModel::modelAboutToBeReset,this,[this]{clearStatusTip();});
        connect(model(),&QAbstractItemModel::rowsAboutToBeRemoved,this,[this]{clearStatusTip();});
        connect(model(),&QAbstractItemModel::dataChanged,this,[this](const QModelIndex& first,const QModelIndex& last){
            if(hovered_.isValid()&&first.column()==0&&first.row()<=hovered_.row()&&last.row()>=hovered_.row())clearStatusTip();
        });
    }
    void setSolveStatus(int row,bool success,const QString& accessibleText,const QString& tooltip={}){
        auto* item=new QTableWidgetItem;
        item->setIcon(studioIcon(success?"MessageSuccess":"MessageCritical"));
        item->setData(Qt::UserRole,success);
        item->setData(Qt::AccessibleTextRole,accessibleText);
        if(!tooltip.isEmpty())item->setToolTip(tooltip);
        setItem(row,0,item);
    }
protected:
    bool viewportEvent(QEvent* event)override{
        if(event->type()==QEvent::MouseMove){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const auto index=indexAt(mouse->position().toPoint());
            if(index!=hovered_){
                clearStatusTip();
                if(index.isValid()&&index.column()==0){
                    hovered_=index;
                    shown_=index.data(Qt::ToolTipRole).toString();
                    if(!shown_.isEmpty())QToolTip::showText(mouse->globalPosition().toPoint(),shown_,viewport(),visualRect(index));
                }
            }
        }else if(event->type()==QEvent::Leave||event->type()==QEvent::Hide)clearStatusTip();
        return QTableWidget::viewportEvent(event);
    }
private:
    QPersistentModelIndex hovered_;
    QString shown_;
    void clearStatusTip(){
        if(!shown_.isEmpty()&&QToolTip::text()==shown_)QToolTip::hideText();
        hovered_=QPersistentModelIndex{};shown_.clear();
    }
};
