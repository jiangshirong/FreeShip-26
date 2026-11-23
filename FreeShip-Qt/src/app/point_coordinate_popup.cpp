// SPDX-License-Identifier: GPL-2.0-or-later
#include "point_coordinate_popup.h"
#include "studio_numeric.h"
#include "render/hull_view.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QStackedWidget>
#include <QComboBox>
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
    auto* heading=new QHBoxLayout;auto* icon=new QLabel;icon->setObjectName("MeshEntityIcon");icon->setPixmap(studioIcon("Point").pixmap(18,18));
    heading->addWidget(icon);heading_=new QLabel;heading->addWidget(heading_);heading->addStretch();layout->addLayout(heading);
    pages_=new QStackedWidget;layout->addWidget(pages_);
    auto* coordinates=new QWidget;pages_->addWidget(coordinates);auto* form=new QFormLayout(coordinates);form->setContentsMargins(0,0,0,0);form->setHorizontalSpacing(10);form->setVerticalSpacing(6);
    for(int axis=0;axis<3;++axis){
        auto* field=new CoordinateField;fields_[axis]=field;field->setRange(-1e9,1e9);field->setDecimals(12);
        field->setKeyboardTracking(false);field->setProperty("point_popup_axis",axis);field->setSingleStep(.01);
        form->addRow(QString(QChar('X'+axis)),field);
        connect(field,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,axis]{if(!loading_)changed_[axis]=true;});
    }
    auto* properties=new QWidget;pages_->addWidget(properties);auto* attributes=new QFormLayout(properties);attributes->setContentsMargins(0,0,0,0);
    name_=new QLineEdit;name_->setObjectName("PointName");type_=new QComboBox;
    for(auto key:{"普通点","折痕点","尖点","角点"})type_->addItem(uiText(key));
    attributes->addRow(uiText("名称"),name_);attributes->addRow(uiText("点类型"),type_);
    auto value=[](const char* object){auto* label=new QLabel;label->setObjectName(object);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setMaximumWidth(270);return label;};
    pointLayers_=value("PointLayers");attributes->addRow(uiText("所属图层"),pointLayers_);
    auto* details=new QWidget;pages_->addWidget(details);auto* info=new QFormLayout(details);info->setContentsMargins(0,0,0,0);
    entityLayers_=value("EntityLayers");info->addRow(uiText("所属图层"),entityLayers_);
    countTitle_=new QLabel;countValue_=value("EntityCount");info->addRow(countTitle_,countValue_);
    creaseValue_=value("EdgeCrease");info->addRow(uiText("折痕"),creaseValue_);
    confirm_=new QPushButton(studioIcon("Apply"),uiText("确定"));confirm_->setProperty("point_popup_action","confirm");
    confirm_->setAutoDefault(false);confirm_->setDefault(false);connect(confirm_,&QPushButton::clicked,this,[this]{acceptCoordinates();});
    auto* controls=new QHBoxLayout;controls->addStretch();controls->addWidget(confirm_);layout->addLayout(controls);
    configureStudioNumericForms(this);qApp->installEventFilter(this);hide();
}
void PointCoordinatePopup::present(fs::EntityId id,fs::Vec3 position,const QString& units,HullProjection projection,const QPoint& anchor,int clearance,bool focusField,bool properties,const QString& name,int type,const QString& layers){
    findChild<QLabel*>("MeshEntityIcon")->setPixmap(studioIcon("Point").pixmap(18,18));confirm_->show();pointLayers_->setText(layers);
    pages_->setCurrentIndex(properties?1:0);name_->setText(name);type_->setCurrentIndex(type);
    heading_->setText(uiText(properties?"控制点属性":"控制点"));
    clearance_=clearance;
    id_=id;original_=position;anchor_=anchor;loading_=true;
    const double values[]{position.x,position.y,position.z};
    for(int axis=0;axis<3;++axis){fields_[axis]->setSuffix(units=="imperial"?" ft":" m");fields_[axis]->setValue(values[axis]);}
    const int hidden=projection==HullProjection::Body?0:projection==HullProjection::Profile?1:projection==HullProjection::Plan?2:-1;
    auto* form=qobject_cast<QFormLayout*>(pages_->widget(0)->layout());
    for(int axis=0;axis<3;++axis)form->setRowVisible(fields_[axis],axis!=hidden);
    changed_.fill(false);loading_=false;setProperty("point_id",qulonglong(id));
    layout()->activate();adjustSize();place();show();raise();
    if(focusField&&!properties)for(auto* field:fields_)if(field->isVisible()){field->setFocus(Qt::OtherFocusReason);field->selectAll();break;}
}
void PointCoordinatePopup::presentEntity(bool edge,const QPoint& anchor,const QString& layers,int count,bool crease){
    findChild<QLabel*>("MeshEntityIcon")->setPixmap(studioIcon(edge?"Edge":"Face").pixmap(18,18));pages_->setCurrentIndex(2);confirm_->hide();id_=0;anchor_=anchor;clearance_=84;
    heading_->setText(uiText(edge?"边属性":"面属性"));entityLayers_->setText(layers);
    countTitle_->setText(uiText(edge?"相邻面数":"顶点数"));countValue_->setText(QString::number(count));
    creaseValue_->setText(uiText(crease?"是":"否"));qobject_cast<QFormLayout*>(pages_->currentWidget()->layout())->setRowVisible(creaseValue_,edge);
    layout()->activate();adjustSize();place();show();raise();
}
void PointCoordinatePopup::follow(fs::Vec3 position,const QPoint& anchor){
    if(position.x!=original_.x||position.y!=original_.y||position.z!=original_.z){
        original_=position;loading_=true;
        const double values[]{position.x,position.y,position.z};
        for(int axis=0;axis<3;++axis)fields_[axis]->setValue(values[axis]);
        changed_.fill(false);loading_=false;
    }
    if(anchor!=anchor_){anchor_=anchor;place();}
}
void PointCoordinatePopup::setEditingEnabled(bool enabled,bool movable,bool typeEditable){
    for(auto* field:fields_)field->setEnabled(enabled&&movable);
    name_->setEnabled(enabled);type_->setEnabled(enabled&&typeEditable);
    confirm_->setEnabled(enabled&&(pages_->currentIndex()==1||movable));
}
void PointCoordinatePopup::acceptCoordinates(){
    if(pages_->currentIndex()==2||!confirm_->isEnabled())return;
    if(pages_->currentIndex()==1){if(onProperties)onProperties(id_,name_->text(),type_->currentIndex());return;}
    for(auto* field:fields_)field->interpretText();
    auto next=original_;double* coordinates[]{&next.x,&next.y,&next.z};
    for(int axis=0;axis<3;++axis)if(!fields_[axis]->isHidden()&&changed_[axis])*coordinates[axis]=fields_[axis]->value();
    if(onApply)onApply(id_,next);
}
void PointCoordinatePopup::place(){
    const auto available=parentWidget()->rect().adjusted(8,8,-8,-8);const int w=width(),h=height();
    const int gap=clearance_+4;const QRect keep(anchor_-QPoint(clearance_,clearance_),QSize(2*clearance_+1,2*clearance_+1));
    const QList<QPoint> candidates{anchor_+QPoint(gap,-h/2),anchor_+QPoint(-w-gap,-h/2),anchor_+QPoint(-w/2,gap),anchor_+QPoint(-w/2,-h-gap)};
    QPoint best;double bestScore=std::numeric_limits<double>::max();
    for(auto at:candidates){at.setX(std::clamp(at.x(),available.left(),std::max(available.left(),available.right()-w+1)));
        at.setY(std::clamp(at.y(),available.top(),std::max(available.top(),available.bottom()-h+1)));
        const QRect rectangle(at,size());const auto center=rectangle.center()-anchor_;
        const double score=(double(rectangle.intersected(keep).width())*rectangle.intersected(keep).height()*1e5)+center.manhattanLength();
        if(score<bestScore){bestScore=score;best=at;}
    }move(best);
    setProperty("avoids_handles",!geometry().intersects(keep));setProperty("anchor",anchor_);setProperty("avoids_anchor",!geometry().intersects(QRect(anchor_-QPoint(8,8),QSize(16,16))));
}
bool PointCoordinatePopup::eventFilter(QObject* object,QEvent* event){
    if(!isVisible())return QFrame::eventFilter(object,event);
    auto* widget=qobject_cast<QWidget*>(object);const bool inside=widget&&(widget==this||isAncestorOf(widget));
    if(QApplication::activePopupWidget())return QFrame::eventFilter(object,event);
    if(inside&&pages_->currentIndex()==0&&(event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress)){
        auto* key=static_cast<QKeyEvent*>(event);
        if(key->matches(QKeySequence::Undo)&&canUndoPlacement&&canUndoPlacement()){
            // A typed draft belongs to its editor. An untouched XYZ field must not swallow document undo.
            bool draft=false;for(auto* field:fields_){auto* text=field->findChild<QLineEdit*>();draft|=text&&(text->isModified()||text->isUndoAvailable());}
            if(!draft&&std::none_of(changed_.begin(),changed_.end(),[](bool value){return value;})){
                key->accept();if(event->type()==QEvent::KeyPress&&undoPlacement)undoPlacement();return true;
            }
        }
    }
    if(inside&&event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Escape){hide();if(onCancelPlacement)onCancelPlacement();return true;}
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){acceptCoordinates();return true;}
    }
    if(widget&&!inside){
        // The view owns point selection and Escape cancellation. Ordinary keys
        // (including axis shortcuts and modifiers) are not dismissal gestures.
        if(event->type()==QEvent::MouseButtonPress){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const bool pointGesture=dynamic_cast<HullView*>(widget)&&mouse->button()==Qt::LeftButton&&!mouse->modifiers();
            if(!pointGesture||pages_->currentIndex()==2)hide();
        }else if(event->type()==QEvent::Wheel)hide();
    }
    if(event->type()==QEvent::ApplicationDeactivate)hide();
    if(object==parentWidget()&&event->type()==QEvent::Resize)hide();
    if(object==this&&event->type()==QEvent::LayoutRequest)QTimer::singleShot(0,this,[this]{if(isVisible()){adjustSize();place();}});
    return QFrame::eventFilter(object,event);
}
