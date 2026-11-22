// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_table.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QComboBox>
#include <QHeaderView>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QWheelEvent>

class RowActionsTable final : public QTableWidget {
public:
    static constexpr int ButtonSize=26;
    static constexpr int ButtonInset=3;
    static constexpr int ControlWidth=ButtonSize+ButtonInset;

    RowActionsTable(int columns,bool rowActions):QTableWidget(0,columns){
        setStudioHeaderGroups(this,{});
        setItemDelegate(new CellDelegate(this,rowActions));
        if(!rowActions)return;
        setProperty("studio_first_column_control_width",ControlWidth);
        const auto place=[this]{placeButtons();};
        connect(horizontalHeader(),&QHeaderView::sectionResized,this,place);
        connect(verticalHeader(),&QHeaderView::sectionResized,this,place);
        connect(horizontalHeader(),&QHeaderView::geometriesChanged,this,place);
        connect(verticalHeader(),&QHeaderView::geometriesChanged,this,place);
    }

    void useCanvas(QScrollArea* canvas){canvas_=canvas;}
    void clearRowButtons(){
        for(auto* button:buttons_){button->hide();button->deleteLater();}
        buttons_.clear();
    }
    void appendRowButton(QToolButton* button){buttons_.append(button);}
    QToolButton* addHeaderButton(const char* caption){
        auto* header=horizontalHeader();
        header->setProperty("studio_first_header_control_width",ControlWidth);
        auto* button=actionButton("AddOne",caption,header->viewport());
        const auto place=[this,header,button]{
            const qreal scale=tableScale();
            const int right=header->sectionViewportPosition(0)+header->sectionSize(0);
            const int size=qRound(ButtonSize*scale),gap=qRound(ButtonInset*scale),icon=qRound(16*scale);
            button->setGeometry(right-size-gap,(header->viewport()->height()-size)/2,size,size);
            button->setIconSize({icon,icon});
        };
        connect(header,&QHeaderView::geometriesChanged,button,place);
        connect(header,&QHeaderView::sectionResized,button,place);
        connect(horizontalScrollBar(),&QScrollBar::valueChanged,button,place);
        place();
        return button;
    }
    static QToolButton* actionButton(const char* icon,const char* caption,QWidget* parent){
        auto* button=new QToolButton(parent);
        button->setIcon(studioIcon(icon));button->setIconSize({16,16});button->setAutoRaise(true);
        bindUiText(button,"accessibleName",caption);bindUiText(button,"toolTip",caption);
        return button;
    }
    void placeButtons(){
        const qreal scale=tableScale();
        const int size=qRound(ButtonSize*scale),gap=qRound(ButtonInset*scale);
        for(int row=0;row<buttons_.size();++row){
            auto* button=buttons_[row];
            const QRect cell=visualRect(model()->index(row,0));
            const QRect area(cell.right()+1-size-gap,cell.center().y()-size/2,size,size);
            const bool visible=cell.contains(area)&&viewport()->rect().intersects(area);
            if(visible){
                button->setGeometry(area);
                const int icon=qRound(16*scale);
                button->setIconSize({icon,icon});
            }
            button->setVisible(visible);
        }
    }
    qreal tableScale()const{
        const auto scale=horizontalHeader()->property("studio_table_scale");
        return scale.isValid()?scale.toReal():1.0;
    }

protected:
    void changeEvent(QEvent* event)override{
        QTableWidget::changeEvent(event);
        if(event->type()==QEvent::FontChange)
            for(auto* combo:viewport()->findChildren<QComboBox*>(QString{},Qt::FindDirectChildrenOnly))
                combo->setFont(font());
    }
    void wheelEvent(QWheelEvent* event)override{
        if(!canvas_||event->modifiers().testFlag(Qt::ControlModifier)){
            QTableWidget::wheelEvent(event);
            return;
        }
        auto* viewport=canvas_->viewport();
        QWheelEvent forwarded(viewport->mapFromGlobal(event->globalPosition()),event->globalPosition(),
                              event->pixelDelta(),event->angleDelta(),event->buttons(),event->modifiers(),
                              event->phase(),event->inverted(),event->source(),event->pointingDevice());
        QApplication::sendEvent(viewport,&forwarded);
        event->setAccepted(forwarded.isAccepted());
    }
    void scrollContentsBy(int dx,int dy)override{
        QTableWidget::scrollContentsBy(dx,dy);
        placeButtons();
    }
    void updateGeometries()override{
        QTableWidget::updateGeometries();
        placeButtons();
    }

private:
    class CellDelegate final : public QStyledItemDelegate {
    public:
        CellDelegate(RowActionsTable* table,bool rowActions):QStyledItemDelegate(table),table_(table),rowActions_(rowActions){}
        void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
            auto text=option;
            text.state&=~(QStyle::State_Selected|QStyle::State_HasFocus);
            if(!rowActions_||index.column()!=0){QStyledItemDelegate::paint(painter,text,index);return;}
            initStyleOption(&text,index);
            auto background=text;
            background.text.clear();
            const auto* style=table_->style();
            style->drawControl(QStyle::CE_ItemViewItem,&background,painter,table_);

            // The button shares the cell background. Reserve space only in
            // the text and editor, so the cell keeps a single border.
            text.rect=textRect(text.rect);
            const int padding=qRound(3*table_->tableScale());
            text.rect.adjust(padding,0,-padding,0);
            if(text.rect.isEmpty())return;
            painter->save();
            // Clip the second style pass to the text's interior. The first
            // pass already drew the actual cell edge and selection.
            painter->setClipRect(text.rect.adjusted(0,2,0,-2),Qt::IntersectClip);
            style->drawControl(QStyle::CE_ItemViewItem,&text,painter,table_);
            painter->restore();
        }
        void updateEditorGeometry(QWidget* editor,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
            auto text=option;
            if(rowActions_&&index.column()==0){
                text.rect=textRect(option.rect);
                editor->setMaximumWidth(text.rect.width());
            }
            QStyledItemDelegate::updateEditorGeometry(editor,text,index);
        }
    private:
        QRect textRect(QRect rect)const{
            rect.setWidth(qMax(0,rect.width()-qRound(ControlWidth*table_->tableScale())));
            return rect;
        }
        RowActionsTable* table_;
        bool rowActions_;
    };
    QList<QToolButton*> buttons_;
    QScrollArea* canvas_=nullptr;
};
