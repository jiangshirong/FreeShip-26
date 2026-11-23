// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "app/ui_language.h"
#include "core/model_snapshot.h"
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QWheelEvent>
#include <algorithm>
#include <charconv>
#include <functional>

// Editor-only context. The chosen X does not add or modify a model section.
class StationPlaneEditor final : public QFrame {
    class Field final : public QDoubleSpinBox {
    public:
        using QDoubleSpinBox::QDoubleSpinBox;
    protected:
        QString textFromValue(double v)const override{
            auto text=QDoubleSpinBox::textFromValue(v);
            if(text.contains(locale().decimalPoint())){
                const auto zero=locale().zeroDigit();while(text.endsWith(zero))text.chop(zero.size());
                if(text.endsWith(locale().decimalPoint()))text.chop(locale().decimalPoint().size());
            }return text;
        }
        void wheelEvent(QWheelEvent* event)override{event->accept();}
    };
    Field* field_;
    QToolButton* choices_;
public:
    using Source=std::function<std::shared_ptr<const fs::ModelSnapshot>()>;
    StationPlaneEditor(QWidget* parent,Source source):QFrame(parent){
        setObjectName("StationPlaneEditor");setCursor(Qt::ArrowCursor);
        setStyleSheet("QFrame#StationPlaneEditor{background:#f8fbf8;border:1px solid #adc7bc;border-radius:5px;}QFrame#StationPlaneEditor QLabel{color:#24463d;border:0;}");
        auto* row=new QHBoxLayout(this);row->setContentsMargins(5,2,4,2);row->setSpacing(4);
        auto* label=new QLabel;row->addWidget(label);bindUiText(label,"text","站面 X");
        field_=new Field;field_->setObjectName("StationPlaneX");bindUiText(field_,"accessibleName","站面 X");
        label->setBuddy(field_);field_->setDecimals(12);field_->setRange(-1e9,1e9);field_->setSingleStep(.1);
        field_->setButtonSymbols(QAbstractSpinBox::NoButtons);field_->setKeyboardTracking(false);
        field_->setFixedWidth(field_->fontMetrics().horizontalAdvance("−0000.0000 ft")+12);row->addWidget(field_);
        choices_=new QToolButton;choices_->setObjectName("StationPlanePresets");choices_->setArrowType(Qt::DownArrow);choices_->setAutoRaise(true);
        bindUiText(choices_,"toolTip","选择已有横剖站");bindUiText(choices_,"accessibleName","选择已有横剖站");
        auto* menu=new QMenu(choices_);choices_->setMenu(menu);choices_->setPopupMode(QToolButton::InstantPopup);choices_->setStyleSheet("QToolButton::menu-indicator{image:none;}");row->addWidget(choices_);
        connect(menu,&QMenu::aboutToShow,this,[this,menu,source]{
            menu->clear();const auto model=source();if(!model)return;
            auto add=[&](QString name,double x){auto* a=menu->addAction(name);a->setData(x);connect(a,&QAction::triggered,this,[this,x]{field_->setValue(x);parentWidget()->setFocus();});};
            add(QStringLiteral("X = 0%1").arg(field_->suffix()),0.);
            QVector<const fs::Section*> stations;for(const auto& s:model->sections)if(s.kind==1)stations.append(&s);
            std::stable_sort(stations.begin(),stations.end(),[](const auto* a,const auto* b){return a->distance<b->distance;});
            if(!stations.isEmpty())menu->addSeparator();
            for(const auto* s:stations){char digits[48];auto value=std::to_chars(digits,digits+sizeof digits,float(s->distance));
                auto coordinate=QStringLiteral("X = %1%2").arg(QString::fromLatin1(digits,value.ptr-digits),field_->suffix());
                add(QString(uiText("站 %1：%2")).arg(s->index+1).arg(coordinate),s->distance);}
        });
        move(6,6);
    }
    double station()const{return field_->value();}
    void setUnits(const QString& units){field_->setSuffix(units=="imperial"?" ft":" m");}
    void reset(){field_->setValue(0.);}
};
