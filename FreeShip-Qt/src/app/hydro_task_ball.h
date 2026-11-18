#pragma once
#include <QAbstractButton>
#include <QPainter>
#include <QTimer>
#include <QVariantAnimation>
#include <QApplication>
#include <QMouseEvent>
#include <QIcon>
#include <cmath>
#include <algorithm>

// A retained task entry. Dragging changes its position without activating it.
class HydroTaskBall final:public QAbstractButton {
public:
    explicit HydroTaskBall(QWidget* parent):QAbstractButton(parent){
        setObjectName("HydroTaskBall");setFixedSize(48,48);setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);
        timer_.setInterval(16);connect(&timer_,&QTimer::timeout,this,[this]{angle_=(angle_+6)%360;update();});
        flight_.setDuration(520);flight_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&flight_,&QVariantAnimation::valueChanged,this,[this](const QVariant& v){
            const double t=v.toDouble();const QPoint p=start_+(target_-start_)*t;
            move(p+QPoint(0,int(-38*std::sin(t*3.141592653589793))));roll_=t*720;update();});
        connect(&flight_,&QVariantAnimation::finished,this,[this]{setProperty("task_docking",false);roll_=0;update();});hide();
    }
    void setState(bool running,bool unread,bool failed=false,bool queued=false,int position=0){
        running_=running;unread_=unread;failed_=failed;queued_=queued;queuePosition_=position;
        setProperty("task_running",running);setProperty("task_unread",unread);setProperty("task_failed",failed);
        setProperty("task_queued",queued);setProperty("task_queue_position",position);
        if(running&&isVisible())timer_.start();else timer_.stop();update();
    }
    void setTaskIcon(const QIcon& icon){
        icon_=icon;iconPixmap_=icon.pixmap(QSize(24,24),devicePixelRatioF());
        if(!iconPixmap_.isNull()){QPainter paint(&iconPixmap_);paint.setCompositionMode(QPainter::CompositionMode_SourceIn);paint.fillRect(iconPixmap_.rect(),QColor("#d8eee6"));}update();
    }
    void dock(QPoint target,QPoint start,bool animate){
        target_=bounded(userPosition_?pos():target);
        if(dragging_)return;
        // Progress reports must not interrupt the flight to the task gutter.
        if(!animate&&flight_.state()==QAbstractAnimation::Running)return;
        flight_.stop();start_=start;show();raise();setProperty("task_docking",animate);
        if(running_)timer_.start();
        if(animate){flight_.setStartValue(0.);flight_.setEndValue(1.);flight_.start();}else {roll_=0;move(target_);}
    }
    void dismiss(){flight_.stop();timer_.stop();setProperty("task_docking",false);hide();}
protected:
    void mousePressEvent(QMouseEvent* event)override{
        if(event->button()!=Qt::LeftButton){QAbstractButton::mousePressEvent(event);return;}
        flight_.stop();setProperty("task_docking",false);roll_=0;
        pressGlobal_=event->globalPosition().toPoint();pressPosition_=pos();dragging_=false;
        QAbstractButton::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event)override{
        if(!(event->buttons()&Qt::LeftButton)){QAbstractButton::mouseMoveEvent(event);return;}
        const auto delta=event->globalPosition().toPoint()-pressGlobal_;
        if(!dragging_&&delta.manhattanLength()<QApplication::startDragDistance()){QAbstractButton::mouseMoveEvent(event);return;}
        dragging_=true;userPosition_=true;setDown(false);setProperty("task_dragged",true);
        move(bounded(pressPosition_+delta));target_=pos();event->accept();
    }
    void mouseReleaseEvent(QMouseEvent* event)override{
        if(event->button()==Qt::LeftButton&&dragging_){dragging_=false;setDown(false);event->accept();return;}
        QAbstractButton::mouseReleaseEvent(event);
    }
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#79a39a"),1.5));p.setBrush(QColor("#214c46"));p.drawEllipse(QRectF(3,3,42,42));
        p.save();p.translate(24,24);p.rotate(roll_);p.setPen(QPen(QColor("#d8eee6"),1.6));
        if(icon_.isNull()){p.drawLine(QPointF(-10,4),QPointF(-5,10));p.drawLine(QPointF(-5,10),QPointF(8,10));p.drawLine(QPointF(8,10),QPointF(12,2));p.drawLine(QPointF(12,2),QPointF(-10,4));p.drawLine(QPointF(0,2),QPointF(0,-10));p.drawLine(QPointF(0,-10),QPointF(8,-2));}
        else p.drawPixmap(QRect(-12,-12,24,24),iconPixmap_);p.restore();
        if(running_){p.setPen(QPen(QColor("#63d2b6"),2.5,Qt::SolidLine,Qt::RoundCap));p.setBrush(Qt::NoBrush);p.drawArc(QRectF(1.5,1.5,45,45),-angle_*16,250*16);}
        else if(queued_){p.setPen(QPen(QColor("#e7be72"),2.3,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(18,35),QPointF(30,35));p.drawLine(QPointF(18,39),QPointF(30,39));}
        else if(unread_){p.setPen(QPen(Qt::white,1.5));p.setBrush(QColor(failed_?"#d58a39":"#39ac80"));p.drawEllipse(QRectF(35,2,10,10));}
        if(hasFocus()){p.setPen(QPen(QColor("#2b8674"),1,Qt::DotLine));p.setBrush(Qt::NoBrush);p.drawEllipse(QRectF(.5,.5,47,47));}
    }
private:
    QPoint bounded(QPoint p)const{const auto* host=parentWidget();if(!host)return p;return {std::clamp(p.x(),0,std::max(0,host->width()-width())),std::clamp(p.y(),0,std::max(0,host->height()-height()))};}
    QTimer timer_;QVariantAnimation flight_;QPoint start_,target_,pressGlobal_,pressPosition_;QIcon icon_;QPixmap iconPixmap_;int angle_=0,queuePosition_=0;double roll_=0;
    bool running_=false,unread_=false,failed_=false,queued_=false,dragging_=false,userPosition_=false;
};
