// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QLayout>
#include <QWidget>
#include <functional>

class StudioFlowLayout final : public QLayout {
public:
    StudioFlowLayout(){setContentsMargins(0,0,0,0);setSpacing(4);}
    ~StudioFlowLayout()override{while(auto* item=takeAt(0))delete item;}
    void addItem(QLayoutItem* item)override{items_.append(item);invalidate();}
    int count()const override{return items_.size();}
    QLayoutItem* itemAt(int index)const override{return items_.value(index);}
    QLayoutItem* takeAt(int index)override{
        if(index<0||index>=items_.size())return nullptr;
        auto* item=items_.takeAt(index);invalidate();return item;
    }
    bool hasHeightForWidth()const override{return true;}
    int heightForWidth(int width)const override{return arrange(QRect(0,0,width,0),false);}
    QSize minimumSize()const override{return items_.isEmpty()?QSize(0,0):QSize(24,30);}
    QSize sizeHint()const override{return {240,heightForWidth(240)};}
    void setGeometry(const QRect& rect)override{
        QLayout::setGeometry(rect);arrange(rect,true);
        if(onLayout)onLayout();
    }
    std::function<void()> onLayout;
private:
    int arrange(const QRect& rect,bool place)const{
        int x=0,y=0,rowHeight=0;
        const int available=qMax(1,rect.width());
        for(auto* item:items_){
            const auto size=item->widget()->sizeHint();
            const int width=qMin(available,size.width());
            const int height=size.height();
            if(x>0&&x+width>available){x=0;y+=rowHeight+spacing();rowHeight=0;}
            if(place)item->setGeometry(QRect(rect.x()+x,rect.y()+y,width,height));
            x+=width+spacing();rowHeight=qMax(rowHeight,height);
        }
        return y+rowHeight;
    }
    QList<QLayoutItem*> items_;
};

