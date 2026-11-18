// SPDX-License-Identifier: GPL-2.0-or-later
#include "studio_view_grid.h"
#include <QEvent>
#include <QDynamicPropertyChangeEvent>
#include <QGridLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
class Divider final:public QWidget {
public:
    Divider(QWidget* owner,int axes):QWidget(owner),axes_(axes){
        setProperty("studio_view_divider_axis",axes==1?"x":axes==2?"y":"xy");
        setCursor(axes==1?Qt::SplitHCursor:axes==2?Qt::SplitVCursor:Qt::SizeAllCursor);
        setMouseTracking(true);setAttribute(Qt::WA_NoSystemBackground);hide();
    }
    std::function<void(QPoint)> onDrag;
protected:
    void mousePressEvent(QMouseEvent* e)override{
        if(e->button()!=Qt::LeftButton){e->ignore();return;}
        dragging_=true;update();e->accept();
    }
    void mouseMoveEvent(QMouseEvent* e)override{
        if(dragging_&&(e->buttons()&Qt::LeftButton)){
            if(onDrag)onDrag(parentWidget()->mapFromGlobal(e->globalPosition().toPoint()));
            e->accept();return;
        }QWidget::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e)override{
        if(e->button()==Qt::LeftButton){dragging_=false;update();e->accept();return;}e->ignore();
    }
    void enterEvent(QEnterEvent* e)override{hover_=true;update();QWidget::enterEvent(e);}
    void leaveEvent(QEvent* e)override{hover_=false;update();QWidget::leaveEvent(e);}
    void hideEvent(QHideEvent* e)override{dragging_=false;hover_=false;QWidget::hideEvent(e);}
    void paintEvent(QPaintEvent*)override{
        // The layout gap already marks the seam. Paint feedback only while
        // interacting with it, so the hit areas don't add a second cross.
        if(!hover_&&!dragging_)return;
        QPainter p(this);
        p.fillRect(rect(),QColor("#dcece5"));
        p.setPen(QPen(QColor("#548675"),1));
        if(axes_&1)p.drawLine(width()/2,0,width()/2,height());
        if(axes_&2)p.drawLine(0,height()/2,width(),height()/2);
    }
private:
    int axes_;bool dragging_=false,hover_=false;
};
class GridController final:public QObject {
public:
    GridController(QGridLayout* grid,const QList<QWidget*>& frames):QObject(grid),grid_(grid),owner_(grid->parentWidget()){
        owner_->installEventFilter(this);grid->setSpacing(6);
        for(auto* frame:frames)frame->installEventFilter(this);
        for(int i=0;i<3;++i){
            const int axes=i+1;auto* handle=new Divider(owner_,axes);handles_[i]=handle;
            handle->onDrag=[this,axes](QPoint point){drag(axes,point);};
        }
        shape(4);
    }
    void shape(int count){
        rows_=count>2?2:1;columns_=count>1?2:1;
        apply();queue();
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event)override{
        // The owner can resize from its normal geometry to maximized before
        // the grid's cells are updated. Retire the old hit areas immediately.
        if(watched==owner_&&(event->type()==QEvent::Resize||event->type()==QEvent::Hide))
            for(auto* handle:handles_)if(handle)handle->hide();
        if(watched==owner_&&event->type()==QEvent::DynamicPropertyChange &&
           static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="studio_view_layout_active")queue();
        if(!grid_->isEnabled())return false;
        if(event->type()==QEvent::Resize||event->type()==QEvent::Move||event->type()==QEvent::Show||
           event->type()==QEvent::Hide||event->type()==QEvent::LayoutRequest)queue();
        return false;
    }
private:
    void queue(){if(queued_)return;queued_=true;QTimer::singleShot(0,this,[this]{queued_=false;sync();});}
    void apply(){
        const int x=std::clamp(int(std::lround(left_*10000)),1,9999),y=std::clamp(int(std::lround(top_*10000)),1,9999);
        grid_->setColumnStretch(0,columns_==2?x:10000);grid_->setColumnStretch(1,columns_==2?10000-x:0);
        grid_->setRowStretch(0,rows_==2?y:10000);grid_->setRowStretch(1,rows_==2?10000-y:0);
        owner_->setProperty("studio_view_left_ratio",left_);owner_->setProperty("studio_view_top_ratio",top_);
        grid_->activate();
    }
    void drag(int axes,QPoint point){
        if(!grid_->isEnabled())return;
        const auto bounds=owner_->contentsRect();const int spacing=grid_->spacing();
        if((axes&1)&&columns_==2){
            const int span=bounds.width()-spacing;
            if(span>0)left_=std::clamp((point.x()-bounds.left()-spacing*.5)/span,.01,.99);
        }
        if((axes&2)&&rows_==2){
            const int span=bounds.height()-spacing;
            if(span>0)top_=std::clamp((point.y()-bounds.top()-spacing*.5)/span,.01,.99);
        }
        // QGridLayout enforces every visible view's existing minimum size.
        // Store the resulting feasible ratio, so reversing a clamped drag or
        // resizing a small window does not produce a hidden overshoot.
        apply();const auto first=grid_->cellRect(0,0);
        const auto requested=QPointF(left_,top_);
        if((axes&1)&&columns_==2)left_=double(first.width())/std::max(1,bounds.width()-spacing);
        if((axes&2)&&rows_==2)top_=double(first.height())/std::max(1,bounds.height()-spacing);
        if(std::abs(left_-requested.x())*(bounds.width()-spacing)>1 ||
           std::abs(top_-requested.y())*(bounds.height()-spacing)>1)apply();
        else{
            owner_->setProperty("studio_view_left_ratio",left_);
            owner_->setProperty("studio_view_top_ratio",top_);
        }
        sync();
    }
    void sync(){
        if(!owner_)return;
        const bool enabled=owner_->isVisible()&&grid_->isEnabled()&&!owner_->property("studio_view_layout_active").toBool();
        if(enabled)grid_->activate();
        const auto a=grid_->cellRect(0,0),bounds=owner_->contentsRect();QRect x,y;
        if(columns_==2){const auto b=grid_->cellRect(0,1);x=QRect(a.right()+1,bounds.top(),std::max(0,b.left()-a.right()-1),bounds.height());}
        if(rows_==2){const auto b=grid_->cellRect(1,0);y=QRect(bounds.left(),a.bottom()+1,bounds.width(),std::max(0,b.top()-a.bottom()-1));}
        const QRect zones[]{x,y,x.intersected(y)};
        // The last row can contain only one view. Use the layout's last cell
        // to verify coverage, regardless of how many frames occupy that row.
        const auto occupied=a.united(grid_->cellRect(rows_-1,columns_-1));
        const bool aligned=occupied==bounds;
        for(int i=0;i<3;++i){auto* handle=handles_[i];const auto zone=zones[i];
            handle->setGeometry(zone);handle->setVisible(enabled&&aligned&&!zone.isEmpty());if(handle->isVisible())handle->raise();
        }
        // A floating coordinate editor can cross a seam. Keep its inputs
        // above handles when a resize/layout request restacks the canvas.
        for(auto* overlay:owner_->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
            if(overlay->isVisible()&&overlay->property("studio_overlay").toBool())overlay->raise();
    }
    QGridLayout* grid_;QPointer<QWidget> owner_;Divider* handles_[3]{};
    double left_=.5,top_=.5;int rows_=2,columns_=2;bool queued_=false;
};
GridController* controller(QGridLayout* grid){for(auto* child:grid->children())if(auto* c=dynamic_cast<GridController*>(child))return c;return nullptr;}
}
namespace studio_view_grid {
void install(QGridLayout* grid,const QList<QWidget*>& frames){if(!controller(grid))new GridController(grid,frames);}
void setVisibleCount(QGridLayout* grid,int count){if(auto* c=controller(grid))c->shape(count);}
}
