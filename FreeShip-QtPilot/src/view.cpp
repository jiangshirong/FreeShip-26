#include "view.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QImage>
#include <algorithm>
#include <cmath>

LinesView::LinesView(QWidget*p):QWidget(p){setObjectName("linesplanView");setMinimumSize(160,100);setAttribute(Qt::WA_OpaquePaintEvent);}
void LinesView::setScene(std::shared_ptr<const Scene>s){
    scene_=std::move(s);gpu_.reset();
    // Tiny scenes cost less to rasterize than a GPU readback and context startup.
    if(scene_->triangles>=2048)gpu_=std::make_unique<GpuFill>(*scene_);
    labelScale_=-1;firstFramePending_=true;fit();
}
QTransform LinesView::camera()const{return QTransform(scale_,0,0,-scale_,offset_.x(),offset_.y());}
void LinesView::fit(){
    if(!scene_)return;
    auto b=scene_->bounds;
    scale_=std::min(std::max(1,width()-180)/b.width(),std::max(1,height()-100)/b.height());
    auto c=b.center();offset_=QPointF(width()/2.0-scale_*c.x(),height()/2.0+scale_*c.y());fitted_=true;update();
}
void LinesView::panBy(QPointF d){offset_+=d;fitted_=false;update();}
void LinesView::zoomAt(QPointF at,double factor){
    if(!scene_||!std::isfinite(factor)||factor<=0)return;
    double next=std::clamp(scale_*factor,1e-7,1e8);
    offset_=at+(offset_-at)*(next/scale_);scale_=next;fitted_=false;update();
}
void LinesView::renderScene(QPainter&p,bool raw,bool cull,bool showLabels)const{
    p.fillRect(QRect(0,0,width(),height()),Qt::white);if(!scene_)return;
    bool gpuFilled=false;
    if(!raw&&fill_&&gpuEnabled_&&gpu_){auto pixels=gpu_->render(size(),p.device()->devicePixelRatioF(),camera());if(!pixels.isNull()){p.drawImage(QPointF(0,0),pixels);gpuFilled=true;}}
    p.save();p.setTransform(camera());QPen pen;pen.setWidthF(1);pen.setCosmetic(true);
    const QRectF visible=camera().inverted().mapRect(QRectF(rect().adjusted(-4,-4,4,4)));
    if(raw){
        for(const auto&v:scene_->primitives){
            if(v.kind==3||(v.kind==1&&!fill_))continue;
            bool fill=v.kind==1;auto col=legacyColor(v.color);
            p.setRenderHint(QPainter::Antialiasing,!fill);
            if(fill){p.setPen(Qt::NoPen);p.setBrush(col);p.drawPolygon(v.points.constData(),v.points.size(),Qt::WindingFill);}
            else{p.setBrush(Qt::NoBrush);pen.setColor(col);p.setPen(pen);p.drawPolyline(v.points.constData(),v.points.size());}
        }
    }else{
        for(const auto&b:scene_->batches){
            if(b.fill&&(!fill_||gpuFilled))continue;
            // Full world-space paths are retained. Culling never modifies their data.
            if(cull&&!visible.intersects(b.bounds.adjusted(-1e-9,-1e-9,1e-9,1e-9)))continue;
            p.setRenderHint(QPainter::Antialiasing,!b.fill);
            if(b.fill){p.setPen(Qt::NoPen);p.setBrush(b.color);}
            else{p.setBrush(Qt::NoBrush);pen.setColor(b.color);p.setPen(pen);}
            p.drawPath(b.path);
        }
    }
    p.restore();
    if(showLabels){
        // Text is painted at device resolution every frame; no scaled screenshot cache.
        p.setRenderHint(QPainter::TextAntialiasing,true);
        QFont font("Arial");
        layoutLabels();
        for(const auto&placement:labelPlacements_){const auto&v=scene_->primitives[placement.index];
            font.setPixelSize(v.pixels);p.setFont(font);p.setPen(legacyColor(v.color));
            auto r=placement.rect.translated(offset_);auto at=r.topLeft();QFontMetricsF fm(font);
            if(!cull||r.intersects(QRectF(rect())))p.drawText(at+QPointF(0,fm.ascent()),v.text);
        }
    }
}
void LinesView::layoutLabels()const{
    if(!scene_||labelScale_==scale_)return;
    labelPlacements_.clear();labelScale_=scale_;
    // Pan changes only offset_. Label choices are recomputed only on zoom.
    for(int pass=0;pass<2;++pass)for(int idx:scene_->labels){const auto&v=scene_->primitives[idx];
        bool numeric=!v.text.isEmpty()&&(v.text[0].isDigit()||v.text[0]=='-');if(numeric!=(pass==1))continue;
        QFont font("Arial");font.setPixelSize(v.pixels);QFontMetricsF metrics(font);
        QPointF anchor(v.points[0].x()*scale_,-v.points[0].y()*scale_);
        QSizeF size(metrics.horizontalAdvance(v.text),metrics.height());
        // Never move a dimension away from the grid line it describes.
        // Crowded ticks are omitted until zoom provides enough space.
        QRectF candidate(anchor,size);bool occupied=false;
        for(const auto&old:labelPlacements_)if(candidate.adjusted(-2,-2,2,2).intersects(old.rect)){occupied=true;break;}
        if(!occupied)labelPlacements_.append({idx,candidate});
    }
}
QVector<QRectF> LinesView::visibleLabelRects()const{
    layoutLabels();QVector<QRectF> result;for(const auto&p:labelPlacements_){auto r=p.rect.translated(offset_);if(r.intersects(QRectF(rect())))result.append(r);}return result;
}
QImage LinesView::image(bool raw,bool cull,bool labels,qreal dpr)const{
    QImage result(QSize(qRound(width()*dpr),qRound(height()*dpr)),QImage::Format_ARGB32_Premultiplied);result.setDevicePixelRatio(dpr);
    QPainter p(&result);renderScene(p,raw,cull,labels);return result;
}
void LinesView::paintEvent(QPaintEvent*){
    QElapsedTimer t;t.start();{QPainter p(this);renderScene(p);}
    lastPaintMs=t.nsecsElapsed()/1e6;++paintCount;
    if(firstFramePending_){firstFramePending_=false;if(onFirstFrame)onFirstFrame();}
}
void LinesView::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::RightButton){dragging_=true;lastMouse_=e->position();setCursor(Qt::ClosedHandCursor);e->accept();}else QWidget::mousePressEvent(e);}
void LinesView::mouseMoveEvent(QMouseEvent*e){if(dragging_&&(e->buttons()&Qt::RightButton)){panBy(e->position()-lastMouse_);lastMouse_=e->position();e->accept();}else QWidget::mouseMoveEvent(e);}
void LinesView::mouseReleaseEvent(QMouseEvent*e){if(e->button()==Qt::RightButton){dragging_=false;unsetCursor();e->accept();}else QWidget::mouseReleaseEvent(e);}
void LinesView::wheelEvent(QWheelEvent*e){double delta=e->pixelDelta().isNull()?e->angleDelta().y()/120.0:e->pixelDelta().y()/40.0;zoomAt(e->position(),std::pow(1.2,delta));e->accept();}
void LinesView::resizeEvent(QResizeEvent*e){QWidget::resizeEvent(e);if(fitted_)fit();}
