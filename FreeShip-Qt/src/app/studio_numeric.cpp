// SPDX-License-Identifier: GPL-2.0-or-later
#include "studio_numeric.h"
#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QEvent>
#include <QFontMetricsF>
#include <QLayout>
#include <QLineEdit>
#include <QPointer>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
bool numericWidget(QWidget* widget){return qobject_cast<QDoubleSpinBox*>(widget)||qobject_cast<QSpinBox*>(widget)||
    (qobject_cast<QLineEdit*>(widget)&&widget->property("exchange_number").isValid());}
class NumericGroup final:public QObject {
public:
    explicit NumericGroup(QLayout* layout):QObject(layout),layout_(layout){setObjectName("StudioNumericGroup");}
    void discover(){
        for(int i=0;i<layout_->count();++i){
            auto* widget=layout_->itemAt(i)->widget();if(!numericWidget(widget))continue;
            if(fields_.contains(widget))continue;
            fields_.append(widget);widget->installEventFilter(this);
            if(auto* edit=qobject_cast<QLineEdit*>(widget))connect(edit,&QLineEdit::textChanged,this,[this]{queue();});
            auto* spin=qobject_cast<QAbstractSpinBox*>(widget);if(!spin)continue;
            if(auto* edit=spin->findChild<QLineEdit*>())connect(edit,&QLineEdit::textChanged,this,[this]{queue();});
            // Qt may block its inner QLineEdit signals for setValue(). Restored
            // parameters and unit conversion must resize just like typed input.
            if(auto* number=qobject_cast<QDoubleSpinBox*>(spin))connect(number,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{queue();});
            if(auto* number=qobject_cast<QSpinBox*>(spin))connect(number,qOverload<int>(&QSpinBox::valueChanged),this,[this]{queue();});
        }
        measure();
    }
protected:
    bool eventFilter(QObject*,QEvent* event)override{
        if(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange||event->type()==QEvent::Show||event->type()==QEvent::FocusOut)queue();
        return false;
    }
private:
    void queue(){if(queued_)return;queued_=true;QTimer::singleShot(0,this,[this]{queued_=false;measure();});}
    void measure(){
        int required=0;bool editing=false;
        for(auto field:fields_)if(field){
            const QFontMetricsF metrics(field->font());
            auto* edit=qobject_cast<QLineEdit*>(field.data());if(!edit)edit=field->findChild<QLineEdit*>();
            const QString text=edit?edit->text():QString{};
            int chrome=12;
            if(auto* spin=qobject_cast<QAbstractSpinBox*>(field.data())){
                QStyleOptionSpinBox option;option.initFrom(spin);option.frame=spin->hasFrame();
                option.buttonSymbols=spin->buttonSymbols();option.rect=QRect(0,0,1000,std::max(30,spin->height()));
                const QRect contents=spin->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxEditField,spin);
                chrome=1000-contents.width();
            }
            if(edit){const auto margins=edit->textMargins();chrome+=margins.left()+margins.right();}
            // Two digits of editing room also cover a sign and decimal separator.
            const int room=int(std::ceil(metrics.horizontalAdvance("00")))+8;
            required=std::max(required,int(std::ceil(metrics.horizontalAdvance(text)))+chrome+room);
            editing|=field->hasFocus()||(edit&&edit->hasFocus());
        }
        if(editing)required=std::max(required,width_); // No shrinking under the caret.
        width_=required;
        for(auto field:fields_)if(field){
            field->setProperty("studio_numeric_content_width",required);
            if(field->minimumWidth()!=required||field->maximumWidth()!=required)field->setFixedWidth(required);
        }
    }
    QLayout* layout_;QList<QPointer<QWidget>> fields_;int width_=0;bool queued_=false;
};
void configure(QLayout* layout){
    if(!layout)return;
    auto* group=dynamic_cast<NumericGroup*>(layout->findChild<QObject*>("StudioNumericGroup",Qt::FindDirectChildrenOnly));
    bool numeric=false;
    for(int i=0;i<layout->count();++i)numeric|=numericWidget(layout->itemAt(i)->widget());
    if(numeric){if(!group)group=new NumericGroup(layout);group->discover();}
    for(int i=0;i<layout->count();++i)if(auto* nested=layout->itemAt(i)->layout())configure(nested);
}
}
void configureStudioNumericForms(QWidget* root){
    if(!root)return;configure(root->layout());
    for(auto* child:root->findChildren<QWidget*>())if(child->layout())configure(child->layout());
}
