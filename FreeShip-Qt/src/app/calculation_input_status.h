// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_ui.h"
#include "math_notation.h"
#include "ui_language.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSet>
#include <QToolTip>
#include <cmath>

namespace calculationInput {
class StatusMarker final : public QLabel {
public:
    explicit StatusMarker(QWidget* parent):QLabel(parent){setAlignment(Qt::AlignCenter);}

protected:
    void enterEvent(QEnterEvent* event) override{
        QLabel::enterEvent(event);
        hovered_=true;
        showStatusTip();
    }
    void leaveEvent(QEvent* event) override{
        dismissStatusTip();
        QLabel::leaveEvent(event);
    }
    bool event(QEvent* event) override{
        // The marker already opened its tooltip when the pointer entered.
        if(event->type()==QEvent::ToolTip)return true;
        if(event->type()==QEvent::Hide&&hovered_)dismissStatusTip();
        const bool handled=QLabel::event(event);
        if(event->type()==QEvent::ToolTipChange&&hovered_)showStatusTip();
        return handled;
    }

private:
    bool hovered_=false;
    void showStatusTip(){
        const bool effects=QApplication::isEffectEnabled(Qt::UI_General);
        if(effects)QApplication::setEffectEnabled(Qt::UI_General,false);
        QToolTip::showText(mapToGlobal(rect().bottomRight()),toolTip(),this,rect());
        if(effects)QApplication::setEffectEnabled(Qt::UI_General,true);
    }
    void dismissStatusTip(){
        if(hovered_&&QToolTip::text()==toolTip())QToolTip::hideText();
        hovered_=false;
    }
};

inline QWidget* caption(QWidget* label,QWidget* field,const QString& key){
    auto* row=new QWidget;
    auto* layout=new QHBoxLayout(row);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(6);
    auto* marker=new StatusMarker(row);
    marker->setProperty("calculation_input_status",key);
    marker->setFixedSize(22,22);
    layout->addWidget(label);
    layout->addWidget(marker,0,Qt::AlignVCenter);
    layout->addStretch();
    field->setProperty("calculation_input_marker",QVariant::fromValue(static_cast<QObject*>(marker)));
    return row;
}

template<class Callback>
void watch(QWidget* field,QObject* owner,Callback changed){
    if(auto* number=qobject_cast<QDoubleSpinBox*>(field)){
        QObject::connect(number,qOverload<double>(&QDoubleSpinBox::valueChanged),owner,changed);
        QObject::connect(number,&QDoubleSpinBox::editingFinished,owner,changed);
        QObject::connect(number->findChild<QLineEdit*>(),&QLineEdit::textEdited,owner,changed);
    }else if(auto* option=qobject_cast<QCheckBox*>(field))QObject::connect(option,&QCheckBox::toggled,owner,changed);
    else if(auto* choice=qobject_cast<QComboBox*>(field))QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),owner,changed);
    else if(auto* text=qobject_cast<QLineEdit*>(field))QObject::connect(text,&QLineEdit::textChanged,owner,changed);
}

inline double number(const QDoubleSpinBox& field,bool& valid){
    const double value=field.locale().toDouble(field.cleanText(),&valid);
    valid=valid&&field.hasAcceptableInput()&&std::isfinite(value);
    return value;
}

inline bool update(const QMap<QString,QWidget*>& fields,const QMap<QString,QString>& errors,
                   const QMap<QString,QString>& hints={},const QSet<QString>& pending={}){
    for(auto it=fields.cbegin();it!=fields.cend();++it){
        auto* marker=qobject_cast<QLabel*>(it.value()->property("calculation_input_marker").value<QObject*>());
        if(!marker)continue;
        const bool invalid=errors.contains(it.key());
        const bool waiting=!invalid&&pending.contains(it.key());
        const bool valid=!invalid&&!waiting;
        const auto description=invalid?errors.value(it.key()):hints.value(it.key(),uiText("参数有效"));
        if(marker->accessibleName()==description&&marker->property("performance_field_valid").toBool()==valid&&
           marker->property("performance_field_pending").toBool()==waiting)continue;
        marker->setPixmap(studioIcon(waiting?"MessageInformation":valid?"MessageSuccess":"MessageCritical")
            .pixmap(QSize(18,18),marker->devicePixelRatioF()));
        if(notation::contains(description)){
            const auto formatted=notation::html(description,marker->font().pointSizeF()).replace('\n',"<br>");
            marker->setToolTip(QStringLiteral("<p>%1</p>").arg(formatted));
        }else marker->setToolTip(description);
        marker->setAccessibleName(description);
        marker->setProperty("performance_field_valid",valid);
        marker->setProperty("performance_field_pending",waiting);
        it.value()->setAccessibleDescription(description);
    }
    return errors.isEmpty()&&pending.isEmpty();
}
}
