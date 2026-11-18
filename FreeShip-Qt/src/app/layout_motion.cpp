// SPDX-License-Identifier: GPL-2.0-or-later
#include "layout_motion.h"
#include "studio_motion.h"
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QEvent>
#include <QLayout>
#include <QGraphicsEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

namespace layout_motion {
namespace {
// The unified animation driver can bootstrap its first local tick after the
// initial native Show/paint work. Keep presentation time anchored to Show,
// rather than pausing at phase zero until that bootstrap has completed.
class RevealClock final:public QAbstractAnimation {
public:
 RevealClock(int milliseconds,QObject* parent,std::function<void(double)> update):QAbstractAnimation(parent),duration_(milliseconds),update_(std::move(update)){}
 int duration()const override{return duration_;}
 void begin(){stop();clock_.start();start();}
protected:
 void updateCurrentTime(int localTime)override{
  const double p=localTime>=duration_?1.:std::clamp(double(clock_.elapsed())/duration_,0.,1.);
  update_(QEasingCurve(QEasingCurve::OutCubic).valueForProgress(p));
 }
private:
 int duration_;QElapsedTimer clock_;std::function<void(double)> update_;
};
// An input-transparent tint leaves one live, native-resolution content layer.
// No screenshot is scaled, and no focus or editing operation waits for it.
class Veil final:public QWidget {
public:
 explicit Veil(QWidget* parent,int duration):QWidget(parent),animation_(duration,this,[this](double phase){
  phase_=phase;parentWidget()->setProperty("studio_window_motion_phase",phase_);update();
 }){
  setObjectName("StudioFeatureContentEntrance");setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::NoFocus);
  QObject::connect(&animation_,&QAbstractAnimation::finished,this,[this]{finish();});
 }
 void begin(){setGeometry(parentWidget()->rect());phase_=0;show();raise();animation_.begin();parentWidget()->setProperty("studio_window_motion_active",true);}
 void finish(){animation_.stop();phase_=1;hide();if(parentWidget()){parentWidget()->setProperty("studio_window_motion_phase",1.);parentWidget()->setProperty("studio_window_motion_active",false);}}
protected:
 void hideEvent(QHideEvent*)override{animation_.stop();}
 void paintEvent(QPaintEvent*)override{
  QPainter painter(this);QColor paper=parentWidget()->palette().color(QPalette::Window);
  // The first visible frame already contains the complete, correctly sized
  // content. This short wash only settles its contrast, never its geometry.
  paper.setAlphaF(.32*(1-phase_));painter.fillRect(rect(),paper);
 }
private:
 RevealClock animation_;double phase_=1;
};

class ExitPresentation final:public QWidget {
public:
 ExitPresentation(QPixmap pixels,const QRect& geometry):QWidget(nullptr,Qt::ToolTip|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint|Qt::WindowTransparentForInput|Qt::WindowDoesNotAcceptFocus),pixels_(std::move(pixels)){
  setObjectName("StudioFeatureWindowExitPresentation");setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_TranslucentBackground);setFocusPolicy(Qt::NoFocus);
  setGeometry(geometry);animation_.setParent(this);animation_.setDuration(120);animation_.setStartValue(1.);animation_.setEndValue(0.);
  animation_.setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(&animation_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){opacity_=value.toDouble();setProperty("studio_window_exit_progress",opacity_);update();});
  QObject::connect(&animation_,&QVariantAnimation::finished,this,[this]{hide();deleteLater();});
  show();animation_.start();
 }
protected:
 void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.setCompositionMode(QPainter::CompositionMode_Source);painter.fillRect(rect(),Qt::transparent);painter.setCompositionMode(QPainter::CompositionMode_SourceOver);painter.setOpacity(opacity_);painter.drawPixmap(0,0,pixels_);}
private:
 QPixmap pixels_;QVariantAnimation animation_;double opacity_=1;
};

class FeatureWindow final:public QObject {
public:
 explicit FeatureWindow(QWidget* window):QObject(window),window_(window){
  setObjectName("StudioFeatureWindowMotion");window->setProperty("studio_motion_feature_window",true);
  qApp->installEventFilter(this);
 }
 ~FeatureWindow()override{if(qApp)qApp->removeEventFilter(this);}
protected:
 bool eventFilter(QObject* watched,QEvent* event)override{
  if(!window_)return false;
  if(watched==window_){
   if(event->type()==QEvent::Show){
    if(exit_){delete exit_;exit_=nullptr;}if(!veil_)veil_=new Veil(window_,160);
    if(studio_motion::enabled()&&!window_->isMinimized())veil_->begin();else veil_->finish();
   }else if(event->type()==QEvent::Resize){if(veil_)veil_->finish();}
   else if(event->type()==QEvent::Hide){
    if(veil_)veil_->finish();
    // Hide occurs after semantic closure. The passive presentation cannot
    // retain modal input, delay done()/finished(), or access a deleted dialog.
    if(studio_motion::enabled()&&!capturing_&&!window_->isMinimized()&&!QCoreApplication::closingDown()&&
       (qApp->applicationState()==Qt::ApplicationActive||qApp->property("studio_motion_probe").toBool())){
     bool gpu=false;for(auto* child:window_->findChildren<QWidget*>())gpu|=child->inherits("QOpenGLWidget");
     if(!gpu){capturing_=true;const auto pixels=window_->grab();capturing_=false;
      if(!pixels.isNull()){if(exit_)delete exit_;exit_=new ExitPresentation(pixels,QRect(window_->mapToGlobal(QPoint{}),window_->size()));}
     }
    }
   }
  }
  if(veil_&&veil_->isVisible()&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress||event->type()==QEvent::InputMethod||event->type()==QEvent::Wheel))
   if(auto* widget=qobject_cast<QWidget*>(watched);widget&&(widget==window_||window_->isAncestorOf(widget)))veil_->finish();
  return false;
 }
private:
 QPointer<QWidget> window_;QPointer<Veil> veil_;QPointer<ExitPresentation> exit_;bool capturing_=false;
};

// A translated widget has identical local pixels. Qt invalidates this
// device-resolution pixmap itself if the view paints new content mid-slide.
class MovingFrameEffect final:public QGraphicsEffect {
protected:
 void draw(QPainter* painter)override{
  QPoint offset;
  const auto pixels=sourcePixmap(Qt::LogicalCoordinates,&offset,NoPad);
  painter->drawPixmap(offset,pixels);
 }
};
class ViewLayout final:public QObject {
public:
 explicit ViewLayout(QWidget* container):QObject(container),container_(container){
  setObjectName("StudioViewLayoutMotion");qApp->installEventFilter(this);animation_.setParent(this);animation_.setDuration(220);animation_.setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(&animation_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){
   const double p=value.toDouble();container_->setProperty("studio_view_layout_progress",p);
   for(int i=0;i<frames_.size();++i)if(frames_[i]&&frames_[i]->isVisible()){
    const auto a=from_[i],b=to_[i];frames_[i]->setGeometry(QRect(qRound(a.x()+(b.x()-a.x())*p),qRound(a.y()+(b.y()-a.y())*p),qRound(a.width()+(b.width()-a.width())*p),qRound(a.height()+(b.height()-a.height())*p)));
   }
  });
  QObject::connect(&animation_,&QVariantAnimation::finished,this,[this]{finish();});
 }
 ~ViewLayout()override{if(qApp)qApp->removeEventFilter(this);}
 void change(const QList<QWidget*>& frames,const std::function<void()>& apply){
  QList<QRect> old;QList<bool> wasVisible;for(auto* frame:frames){old<<frame->geometry();wasVisible<<frame->isVisible();}
  clearMovingEffects();
  animation_.stop();active_=false;container_->setProperty("studio_view_layout_active",false);if(container_->layout())container_->layout()->setEnabled(true);
  apply();if(container_->layout())container_->layout()->activate();
  frames_.clear();from_.clear();to_.clear();bool different=false;
  for(int i=0;i<frames.size();++i){auto* frame=frames[i];const QRect target=frame->geometry();
   frames_<<frame;to_<<target;from_<<(wasVisible[i]&&frame->isVisible()?old[i]:target);
   different|=frame->isVisible()&&from_.last()!=target;
   if(!wasVisible[i]&&frame->isVisible()&&studio_motion::enabled()&&container_->isVisible()){
    auto* veil=dynamic_cast<Veil*>(frame->findChild<QWidget*>("StudioFeatureContentEntrance",Qt::FindDirectChildrenOnly));
    if(!veil)veil=new Veil(frame,160);veil->begin();
   }
  }
  if(!studio_motion::enabled()||!container_->isVisible()||!different){finish();return;}
  if(container_->layout())container_->layout()->setEnabled(false);
  for(int i=0;i<frames_.size();++i)if(frames_[i]&&frames_[i]->isVisible())frames_[i]->setGeometry(from_[i]);
  for(int i=0;i<frames_.size();++i){
   auto* frame=frames_[i].data();
   if(frame&&frame->isVisible()&&wasVisible[i]&&from_[i].size()==to_[i].size()&&from_[i]!=to_[i]&&!frame->graphicsEffect()){
    frame->setGraphicsEffect(new MovingFrameEffect);movingFrames_.append(frame);
   }
  }
  active_=true;container_->setProperty("studio_view_layout_active",true);animation_.setStartValue(0.);animation_.setEndValue(1.);animation_.start();
 }
 void finish(bool resized=false){
  // Captured rectangles belong only to the active transition. Replaying them
  // after a later restore/hide would overwrite a perfectly valid live layout.
  if(!active_)return;active_=false;animation_.stop();if(container_){
  if(!resized)for(int i=0;i<frames_.size();++i)if(frames_[i]&&frames_[i]->isVisible())frames_[i]->setGeometry(to_[i]);
  if(container_->layout()){container_->layout()->setEnabled(true);container_->layout()->invalidate();container_->layout()->activate();}
  container_->setProperty("studio_view_layout_active",false);container_->setProperty("studio_view_layout_progress",1.);
  clearMovingEffects();frames_.clear();from_.clear();to_.clear();
 }}
 void clearMovingEffects(){
  for(auto& frame:movingFrames_)if(frame&&dynamic_cast<MovingFrameEffect*>(frame->graphicsEffect()))frame->setGraphicsEffect(nullptr);
  movingFrames_.clear();
 }
 QList<QPointer<QWidget>> movingFrames_;
protected:
 bool eventFilter(QObject* watched,QEvent* event)override{
  if(!container_)return false;
  if(watched==container_&&(event->type()==QEvent::Resize||event->type()==QEvent::Hide))finish(event->type()==QEvent::Resize);
  if(animation_.state()==QAbstractAnimation::Running&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress||event->type()==QEvent::Wheel))
   if(auto* widget=qobject_cast<QWidget*>(watched);widget&&(widget==container_||container_->isAncestorOf(widget)))finish();
  return false;
 }
private:
 QPointer<QWidget> container_;QVariantAnimation animation_;QList<QPointer<QWidget>> frames_;QList<QRect> from_,to_;bool active_=false;
};

class HandleMark final:public QWidget {
public:
 explicit HandleMark(QSplitterHandle* handle):QWidget(handle),handle_(handle){
  setObjectName("StudioSplitterMotionMark");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::NoFocus);show();
 }
 void setPhase(double phase){phase_=phase;setGeometry(parentWidget()->rect());raise();update();}
protected:
 void paintEvent(QPaintEvent*)override{
  QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);QColor ink("#007e74");ink.setAlphaF(.18+.82*phase_);painter.setPen(QPen(ink,2,Qt::SolidLine,Qt::RoundCap));
  if(handle_->orientation()==Qt::Horizontal)painter.drawLine(QPointF(width()/2.,4),QPointF(width()/2.,height()-4));
  else painter.drawLine(QPointF(4,height()/2.),QPointF(width()-4,height()/2.));
 }
private:
 QSplitterHandle* handle_;double phase_=0;
};

class Splitter final:public QObject {
public:
 explicit Splitter(QSplitter* splitter):QObject(splitter),splitter_(splitter){
  setObjectName("StudioSplitterMotion");splitter->setProperty("studio_splitter_motion",true);splitter->setHandleWidth(16);splitter->installEventFilter(this);
  snap_.setParent(this);snap_.setDuration(80);snap_.setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(&snap_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){
   const double p=value.toDouble();QList<int> sizes=from_;for(int i=0;i<sizes.size();++i)sizes[i]=qRound(from_[i]+(to_[i]-from_[i])*p);splitter_->setSizes(sizes);splitter_->setProperty("studio_splitter_snap_progress",p);
  });
  refresh();
 }
protected:
 bool eventFilter(QObject* watched,QEvent* event)override{
  if(watched==splitter_&&(event->type()==QEvent::ChildAdded||event->type()==QEvent::LayoutRequest)){
   if(!queued_){queued_=true;QTimer::singleShot(0,this,[this]{queued_=false;refresh();});}return false;
  }
  auto* handle=qobject_cast<QSplitterHandle*>(watched);if(!handle)return false;
  auto* mark=dynamic_cast<HandleMark*>(handle->findChild<QWidget*>("StudioSplitterMotionMark",Qt::FindDirectChildrenOnly));
  if(event->type()==QEvent::Resize&&mark)mark->setGeometry(handle->rect());
  if(event->type()==QEvent::Enter||event->type()==QEvent::HoverEnter)hover(handle,1);
  if(event->type()==QEvent::Leave)hover(handle,0);
  if(event->type()==QEvent::MouseButtonPress){snap_.stop();hover(handle,1);}
  if(event->type()==QEvent::MouseButtonRelease)QTimer::singleShot(0,this,[this,handle]{snap(handle);});
  return false;
 }
private:
 void refresh(){
  for(int i=1;i<splitter_->count();++i){auto* h=splitter_->handle(i);if(h->property("studio_splitter_motion_handle").toBool())continue;
   h->setProperty("studio_splitter_motion_handle",true);h->setAttribute(Qt::WA_Hover);h->setMouseTracking(true);h->installEventFilter(this);
   auto* mark=new HandleMark(h);mark->setPhase(0);
   auto* animation=new QVariantAnimation(h);animation->setObjectName("StudioSplitterHoverMotion");animation->setDuration(100);animation->setEasingCurve(QEasingCurve::OutCubic);
   QObject::connect(animation,&QVariantAnimation::valueChanged,h,[h,mark](const QVariant& value){h->setProperty("studio_splitter_motion_progress",value);mark->setPhase(value.toDouble());});
  }
 }
 void hover(QSplitterHandle* handle,double target){
  auto* animation=handle->findChild<QVariantAnimation*>("StudioSplitterHoverMotion",Qt::FindDirectChildrenOnly);if(!animation)return;
  const double current=handle->property("studio_splitter_motion_progress").toDouble();animation->stop();animation->setStartValue(current);animation->setEndValue(target);
  if(studio_motion::enabled())animation->start();else{handle->setProperty("studio_splitter_motion_progress",target);if(auto* mark=dynamic_cast<HandleMark*>(handle->findChild<QWidget*>("StudioSplitterMotionMark")))mark->setPhase(target);}
 }
 void snap(QSplitterHandle* handle){
  int at=-1;for(int i=1;i<splitter_->count();++i)if(splitter_->handle(i)==handle){at=i;break;}if(at<1)return;
  from_=splitter_->sizes();to_=from_;const int total=from_[at-1]+from_[at],half=total/2;
  if(std::abs(from_[at-1]-half)>8||from_[at-1]==half)return;
  to_[at-1]=half;to_[at]=total-half;
  splitter_->setProperty("studio_splitter_snap_count",splitter_->property("studio_splitter_snap_count").toInt()+1);
  if(!studio_motion::enabled()){splitter_->setSizes(to_);return;}snap_.setStartValue(0.);snap_.setEndValue(1.);snap_.start();
 }
 QPointer<QSplitter> splitter_;QVariantAnimation snap_;QList<int> from_,to_;bool queued_=false;
};
}

void installFeatureWindow(QWidget* window){if(window&&!window->findChild<QObject*>("StudioFeatureWindowMotion",Qt::FindDirectChildrenOnly))new FeatureWindow(window);}
void animateViewLayout(QWidget* container,const QList<QWidget*>& frames,const std::function<void()>& applyTargetLayout){
 if(!container){applyTargetLayout();return;}
 auto* motion=dynamic_cast<ViewLayout*>(container->findChild<QObject*>("StudioViewLayoutMotion",Qt::FindDirectChildrenOnly));if(!motion)motion=new ViewLayout(container);motion->change(frames,applyTargetLayout);
}
void installSplitter(QSplitter* splitter){if(splitter&&!splitter->findChild<QObject*>("StudioSplitterMotion",Qt::FindDirectChildrenOnly))new Splitter(splitter);}
}
