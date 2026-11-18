// SPDX-License-Identifier: GPL-2.0-or-later
#include "message_motion.h"
#include "studio_ui.h"
#include "studio_motion.h"
#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QVariantAnimation>
#include <QTimer>
#include <algorithm>
namespace {
class MessageMotion final:public QObject{
public:
 MessageMotion(StudioMessage* label):QObject(label),label_(label),height_(this),busy_(this),confirmation_(this){
  setObjectName("StudioMessageMotion");minimum_=label->minimumHeight();maximum_=label->maximumHeight();label->installEventFilter(this);
  height_.setEasingCurve(QEasingCurve::OutCubic);
  connect(&height_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){
   label_->setMaximumHeight(std::max(0,value.toInt()));label_->setProperty("studio_message_reveal_height",value);label_->updateGeometry();
  });
  connect(&height_,&QVariantAnimation::finished,this,[this]{
   if(!target_)label_->QLabel::setVisible(false);
   label_->setMaximumHeight(maximum_);label_->setMinimumHeight(minimum_);
  });
  busy_.setStartValue(0.);busy_.setEndValue(1.);busy_.setDuration(800);busy_.setLoopCount(-1);
  connect(&busy_,&QVariantAnimation::valueChanged,this,[this](const QVariant& v){label_->setProperty("studio_message_busy_phase",v);label_->update(QRect(10,10,22,22));});
  confirmation_.setStartValue(0.);confirmation_.setEndValue(1.);confirmation_.setDuration(120);
  connect(&confirmation_,&QVariantAnimation::valueChanged,this,[this](const QVariant& v){label_->setProperty("studio_message_confirmation_phase",v);label_->update(QRect(10,10,22,22));});
 }
 bool visible(bool value){
  if(!studio_motion::enabled()||!label_->parentWidget()||!label_->parentWidget()->isVisible()){
   height_.stop();label_->setMaximumHeight(maximum_);label_->setMinimumHeight(minimum_);return false;
  }
  if(value==target_&&((height_.state()==QAbstractAnimation::Running)||value==label_->isVisible()))return false;
  const int from=height_.state()==QAbstractAnimation::Running?height_.currentValue().toInt():(label_->isVisible()?label_->height():0);
  height_.stop();target_=value;label_->setProperty("studio_message_target_visible",value);
  const int width=std::max(100,label_->width());
  const int to=value?std::max(minimum_,label_->heightForWidth(width)):0;
  label_->setMinimumHeight(0);label_->setMaximumHeight(from);label_->QLabel::setVisible(true);
  height_.setStartValue(from);height_.setEndValue(to);height_.setDuration(value?160:120);height_.start();syncBusy();return true;
 }
 void working(bool value){
  if(!value&&label_->property("studio_message_role")=="feedback"&&!label_->text().isEmpty()&&label_->text()!=lastConfirmation_){
   lastConfirmation_=label_->text();confirmation_.stop();
   if(studio_motion::enabled()&&label_->isVisible())confirmation_.start();else label_->setProperty("studio_message_confirmation_phase",1.);
  }
  working_=value;syncBusy();
 }
protected:
 bool eventFilter(QObject*,QEvent* e)override{
  if(e->type()==QEvent::Hide){busy_.stop();confirmation_.stop();height_.stop();label_->setMaximumHeight(maximum_);label_->setMinimumHeight(minimum_);}
  if(e->type()==QEvent::Show)syncBusy();return false;
 }
private:
 void syncBusy(){if(working_&&label_->isVisible()&&studio_motion::enabled()){if(busy_.state()!=QAbstractAnimation::Running)busy_.start();}else{busy_.stop();label_->setProperty("studio_message_busy_phase",0.);}}
 StudioMessage* label_;QVariantAnimation height_,busy_,confirmation_;QString lastConfirmation_;int minimum_=40,maximum_=QWIDGETSIZE_MAX;bool target_=false,working_=false;
};
MessageMotion* controller(StudioMessage* label){
 for(auto* child:label->children())if(auto* c=dynamic_cast<MessageMotion*>(child))return c;
 return new MessageMotion(label);
}
class Confirmation final:public QWidget{
public:
 Confirmation(QWidget* owner):QWidget(owner),phase_(this){
  setObjectName("StudioSuccessMotion");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_TranslucentBackground);setFocusPolicy(Qt::NoFocus);
  setGeometry(std::max(0,owner->width()-34),std::max(0,(owner->height()-28)/2),28,28);
  phase_.setStartValue(0.);phase_.setEndValue(1.);phase_.setDuration(120);
  connect(&phase_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){progress_=value.toDouble();setProperty("studio_confirmation_progress",progress_);update();});
  show();raise();phase_.start();QTimer::singleShot(1200,this,&QObject::deleteLater);
 }
protected:
 void paintEvent(QPaintEvent*)override{
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(QColor("#e7f3ed"));p.drawEllipse(QRectF(1,1,26,26));
  p.setClipRect(QRectF(0,0,width()*progress_,height()));p.setPen(QPen(QColor("#007e74"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPolyline(QPolygonF{{7,14},{12,19},{21,9}});
 }
private:QVariantAnimation phase_;double progress_=0;
};
}
bool message_motion::visibility(StudioMessage* message,bool visible){return controller(message)->visible(visible);}
void message_motion::updated(StudioMessage* message,bool working){if(message->parentWidget())controller(message)->working(working);}
void message_motion::confirmation(QWidget* owner){if(!owner||!owner->isVisible()||!studio_motion::enabled())return;for(auto* old:owner->findChildren<QWidget*>("StudioSuccessMotion",Qt::FindDirectChildrenOnly))delete old;new Confirmation(owner);}
