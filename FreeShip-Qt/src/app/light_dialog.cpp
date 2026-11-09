// SPDX-License-Identifier: GPL-2.0-or-later
// Field groups and ranges follow Forms/lightdialog.lfm. The current view is
// authoritative; opening this window must not silently replace its light.
#include "light_dialog.h"
#include "studio_ui.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

LightDialog::LightDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("光源");setProperty("view_operation","light_dialog");resize(470,360);
    auto* layout=new QVBoxLayout(this);layout->setSpacing(12);
    enabled_=new QCheckBox("启用光照");enabled_->setProperty("light_field","enabled");layout->addWidget(enabled_);
    auto* position=new QGroupBox("位置");auto* positionLayout=new QFormLayout(position);positionLayout->setVerticalSpacing(14);layout->addWidget(position);
    auto* strength=new QGroupBox("亮度");auto* strengthLayout=new QFormLayout(strength);strengthLayout->setVerticalSpacing(14);layout->addWidget(strength);
    auto field=[&](QFormLayout* form,const QString& name,const QString& label,int low,int high){
        auto* row=new QWidget;auto* line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);
        auto* slider=new QSlider(Qt::Horizontal);slider->setRange(low,high);slider->setPageStep(10);slider->setMinimumWidth(230);slider->setProperty("light_slider",name);slider->setAccessibleName(label);
        auto* number=new QSpinBox;number->setRange(low,high);number->setMinimumWidth(68);number->setProperty("light_field",name);fields_[name]=number;
        line->addWidget(slider,1);line->addWidget(number);form->addRow(label,row);
        connect(slider,&QSlider::valueChanged,number,&QSpinBox::setValue);connect(number,&QSpinBox::valueChanged,slider,&QSlider::setValue);
        connect(number,&QSpinBox::valueChanged,this,[this]{publish();});
    };
    field(positionLayout,"x","X",-180,180);field(positionLayout,"y","Y",-180,180);field(positionLayout,"z","Z",-180,180);
    field(strengthLayout,"ambient","环境光",0,255);field(strengthLayout,"intensity","强度",0,255);
    auto* hint=new QLabel("X 朝向观察者，Y 向右，Z 向上。调整即时作用于当前视图。");hint->setWordWrap(true);layout->addWidget(hint);
    auto* close=new QPushButton(studioIcon("Confirm"),"关闭");close->setProperty("light_action","close");layout->addWidget(close,0,Qt::AlignRight);connect(close,&QPushButton::clicked,this,&QDialog::accept);
    connect(enabled_,&QCheckBox::toggled,this,[this]{publish();});setLight(HullLight{});
}
void LightDialog::setLight(const HullLight& light){
    loading_=true;enabled_->setChecked(light.enabled);fields_["x"]->setValue(qRound(light.direction.x));fields_["y"]->setValue(qRound(light.direction.y));fields_["z"]->setValue(qRound(light.direction.z));fields_["ambient"]->setValue(light.ambient);fields_["intensity"]->setValue(light.intensity);loading_=false;
}
void LightDialog::publish(){
    if(loading_||!onLightChanged)return;
    HullLight light;light.enabled=enabled_->isChecked();light.direction={double(fields_["x"]->value()),double(fields_["y"]->value()),double(fields_["z"]->value())};light.ambient=fields_["ambient"]->value();light.intensity=fields_["intensity"]->value();onLightChanged(light);
}
