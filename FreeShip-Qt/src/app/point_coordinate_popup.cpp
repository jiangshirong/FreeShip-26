// SPDX-License-Identifier: GPL-2.0-or-later
#include "point_coordinate_popup.h"
#include "studio_numeric.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {
class CoordinateField final:public QDoubleSpinBox {
protected:
    QString textFromValue(double value)const override{
        auto text=QDoubleSpinBox::textFromValue(value);
        if(text.contains(locale().decimalPoint())){
            while(text.endsWith('0'))text.chop(1);
            if(text.endsWith(locale().decimalPoint()))text.chop(locale().decimalPoint().size());
        }return text;
    }
};
}
PointCoordinatePopup::PointCoordinatePopup(QWidget* parent):QFrame(parent){
    setObjectName("PointCoordinatePopup");setProperty("studio_content_role","point_coordinates");setProperty("studio_overlay",true);
    setStyleSheet("QFrame#PointCoordinatePopup{background:#f8fbf8;border:1px solid #adc7bc;border-radius:8px;}"
                  "QFrame#PointCoordinatePopup QLabel{color:#24463d;border:0;background:transparent;}");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(12,10,12,10);layout->setSpacing(8);
    auto* heading=new QHBoxLayout;auto* icon=new QLabel;icon->setPixmap(studioIcon("Point").pixmap(18,18));
    heading->addWidget(icon);heading->addWidget(new QLabel(uiText("控制点")));heading->addStretch();layout->addLayout(heading);
    auto* form=new QFormLayout;form->setHorizontalSpacing(10);form->setVerticalSpacing(6);layout->addLayout(form);
    for(int axis=0;axis<3;++axis){
        auto* field=new CoordinateField;fields_[axis]=field;field->setRange(-1e9,1e9);field->setDecimals(12);
        field->setKeyboardTracking(false);field->setProperty("point_popup_axis",axis);field->setSingleStep(.01);
        form->addRow(QString(QChar('X'+axis)),field);
        connect(field,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,axis]{if(!loading_)changed_[axis]=true;});
    }
    confirm_=new QPushButton(studioIcon("Apply"),uiText("确定"));confirm_->setProperty("point_popup_action","confirm");
    confirm_->setAutoDefault(false);confirm_->setDefault(false);connect(confirm_,&QPushButton::clicked,this,[this]{acceptCoordinates();});
    auto* controls=new QHBoxLayout;controls->addStretch();controls->addWidget(confirm_);layout->addLayout(controls);
    qApp->installEventFilter(this);parent->installEventFilter(this);hide();
}
void PointCoordinatePopup::present(fs::EntityId id,fs::Vec3 position,const QString& units,const QPoint& anchor){
    id_=id;original_=position;anchor_=anchor;loading_=true;
    const double values[]{position.x,position.y,position.z};
    for(int axis=0;axis<3;++axis){fields_[axis]->setSuffix(units=="imperial"?" ft":" m");fields_[axis]->setValue(values[axis]);}
    changed_.fill(false);loading_=false;setProperty("point_id",qulonglong(id));
    configureStudioNumericForms(this);layout()->activate();adjustSize();place();show();raise();
    fields_[0]->setFocus(Qt::OtherFocusReason);fields_[0]->selectAll();
}
void PointCoordinatePopup::setEditingEnabled(bool enabled){
    const bool wasEnabled=confirm_->isEnabled();
    for(auto* field:fields_)field->setEnabled(enabled);confirm_->setEnabled(enabled);
    if(enabled&&!wasEnabled&&isVisible()){fields_[0]->setFocus(Qt::OtherFocusReason);fields_[0]->selectAll();}
}
void PointCoordinatePopup::acceptCoordinates(){
    if(!confirm_->isEnabled())return;
    for(auto* field:fields_)field->interpretText();
    auto next=original_;double* coordinates[]{&next.x,&next.y,&next.z};
    for(int axis=0;axis<3;++axis)if(changed_[axis])*coordinates[axis]=fields_[axis]->value();
    if(onApply)onApply(id_,next);
}
void PointCoordinatePopup::place(){
    const auto available=parentWidget()->rect().adjusted(8,8,-8,-8);const int w=width(),h=height();
    const QList<QPoint> candidates{anchor_+QPoint(16,12),anchor_+QPoint(-w-16,12),anchor_+QPoint(16,-h-12),anchor_+QPoint(-w-16,-h-12)};
    QPoint best;double bestScore=std::numeric_limits<double>::max();
    for(auto at:candidates){at.setX(std::clamp(at.x(),available.left(),std::max(available.left(),available.right()-w+1)));
        at.setY(std::clamp(at.y(),available.top(),std::max(available.top(),available.bottom()-h+1)));
        const QRect rectangle(at,size());const auto center=rectangle.center()-anchor_;
        const double score=(rectangle.intersects(QRect(anchor_-QPoint(8,8),QSize(16,16)))?1e9:0)+center.manhattanLength();
        if(score<bestScore){bestScore=score;best=at;}
    }move(best);
    setProperty("anchor",anchor_);setProperty("avoids_anchor",!geometry().intersects(QRect(anchor_-QPoint(8,8),QSize(16,16))));
}
bool PointCoordinatePopup::eventFilter(QObject* object,QEvent* event){
    if(!isVisible())return QFrame::eventFilter(object,event);
    auto* widget=qobject_cast<QWidget*>(object);const bool inside=widget&&(widget==this||isAncestorOf(widget));
    if(inside&&event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Escape){hide();if(onCancelPlacement)onCancelPlacement();return true;}
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){acceptCoordinates();return true;}
    }
    if(widget&&!inside&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::Wheel||event->type()==QEvent::KeyPress))hide();
    if(event->type()==QEvent::ApplicationDeactivate)hide();
    if(object==parentWidget()&&event->type()==QEvent::Resize)hide();
    if(object==this&&event->type()==QEvent::LayoutRequest)QTimer::singleShot(0,this,[this]{if(isVisible()){adjustSize();place();}});
    return QFrame::eventFilter(object,event);
}
