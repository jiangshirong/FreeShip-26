#include "view.h"
#include "lines_reference.h"
#include "text_style.h"
#include "app/math_notation.h"
#include "software_scene.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QImage>
#include <algorithm>
#include <cmath>

LinesView::LinesView(QWidget*p):QWidget(p){setObjectName("linesplanView");setMinimumSize(160,100);setAttribute(Qt::WA_OpaquePaintEvent);labelClock_.start();labelTimer_.setInterval(16);connect(&labelTimer_,&QTimer::timeout,this,[this]{if(isVisible())update();else labelTimer_.stop();});}
void LinesView::setReferenceCaptions(const QString& baseline,const QString& centerline){
    if(baselineCaption_==baseline&&centerlineCaption_==centerline)return;
    baselineCaption_=baseline;centerlineCaption_=centerline;
    rebuildLabelMetrics();
    screenLayout_.clear();screenDirty_=true;labelScale_=-1;
    update();
}
void LinesView::rebuildLabelMetrics(){
    labelMetrics_.clear();
    if(!scene_)return;
    for(int i:scene_->labels){
        const auto& p=scene_->primitives[i];
        const QString text=linesReferenceText(p.text,baselineCaption_,centerlineCaption_);
        QFont font=studioTextFont(QFont("Arial"));font.setPixelSize(p.pixels);QFontMetricsF metrics(font);
        labelMetrics_.insert(i,{text,font,notation::size(text,font),metrics.ascent()});
    }
}
void LinesView::setScene(std::shared_ptr<const Scene>s,bool resetCamera){
    if(scene_==s&&!resetCamera)return;
    resetCamera=resetCamera||!scene_;
    scene_=std::move(s);gpu_.reset();
    softwareScene_.reset();
    screenLayout_.clear();screenLabels_.clear();labelMetrics_.clear();screenDirty_=true;labelTimer_.stop();
    if(!scene_){labelKinds_.clear();update();return;}
    using fs::presentation::LabelKind;
    labelKinds_.fill(LabelKind::Named,scene_->primitives.size());
    QHash<QString,QVector<int>> repeated;
    rebuildLabelMetrics();
    for(int i:scene_->labels)repeated[scene_->primitives[i].text].append(i);
    for(int i:scene_->labels){const auto& v=scene_->primitives[i];
        if(v.text.isEmpty()||!(v.text[0].isDigit()||v.text[0]=='-'||v.text[0]=='+'))continue;
        labelKinds_[i]=LabelKind::HorizontalTick;
        // Opposite-end copies of the same tick identify the grid direction.
        for(int j:repeated[v.text])if(i!=j){auto d=scene_->primitives[j].points[0]-v.points[0];
            if(std::abs(d.x())>1e-8&&std::abs(d.y())<1e-8){labelKinds_[i]=LabelKind::VerticalTick;break;}
        }
    }
    // Tiny scenes cost less to rasterize than a GPU readback and context startup.
    // The first filled draw creates the GPU resources. Hidden lines windows need none.
    labelScale_=-1;firstFramePending_=true;
    if(resetCamera)fit();else update();
}
QTransform LinesView::camera()const{return QTransform(scale_,0,0,-scale_,offset_.x(),offset_.y());}
void LinesView::fit(){
    if(!scene_)return;
    auto b=scene_->bounds;
    scale_=std::min(std::max(1,width()-180)/b.width(),std::max(1,height()-100)/b.height());
    auto c=b.center();offset_=QPointF(width()/2.0-scale_*c.x(),height()/2.0+scale_*c.y());fitted_=true;screenDirty_=true;update();
}
void LinesView::panBy(QPointF d){offset_+=d;screenLayout_.translate(d);screenDirty_=true;fitted_=false;update();}
void LinesView::zoomAt(QPointF at,double factor){
    if(!scene_||!std::isfinite(factor)||factor<=0)return;
    double next=std::clamp(scale_*factor,1e-7,1e8);
    offset_=at+(offset_-at)*(next/scale_);scale_=next;screenDirty_=true;fitted_=false;update();
}
void LinesView::renderScene(QPainter&p,bool raw,bool cull,bool showLabels,bool interactive)const{
    p.fillRect(QRect(0,0,width(),height()),Qt::white);if(!scene_)return;
    bool gpuFilled=false;
    if(!raw&&fill_&&gpuEnabled_&&!gpu_&&scene_->triangles>=2048)gpu_=std::make_unique<GpuFill>(*scene_);
    if(!raw&&fill_&&gpuEnabled_&&gpu_){auto pixels=gpu_->render(size(),p.device()->devicePixelRatioF(),camera());if(!pixels.isNull()){p.drawImage(QPointF(0,0),pixels);gpuFilled=true;}}
    bool softwareRendered=false;
    if(interactive&&!raw&&fill_&&!gpuFilled){
        if(!softwareScene_)softwareScene_=std::make_shared<SoftwareScene>(*scene_);
        const auto& pixels=softwareScene_->render(size(),p.device()->devicePixelRatioF(),camera());
        if(!pixels.isNull()){p.drawImage(QPointF{},pixels);softwareRendered=true;}
    }
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
    }else if(!softwareRendered){
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
        if(interactive&&!raw){
            layoutScreenLabels();
            for(const auto& placed:screenLabels_){const auto& v=scene_->primitives[placed.index];
                const auto& metrics=labelMetrics_.constFind(placed.index).value();p.setFont(metrics.font);p.setPen(legacyColor(v.color));
                p.save();p.setClipRect(placed.clip,Qt::IntersectClip);
                notation::drawBaseline(p,placed.position+QPointF(0,metrics.ascent),metrics.text);p.restore();
            }
            return;
        }
        layoutLabels();
        for(const auto&placement:labelPlacements_){const auto&v=scene_->primitives[placement.index];
            const auto& metrics=labelMetrics_.constFind(placement.index).value();
            p.setFont(metrics.font);p.setPen(legacyColor(v.color));
            auto r=placement.rect.translated(offset_);
            if(!cull||r.intersects(QRectF(rect())))notation::drawBaseline(p,r.topLeft()+QPointF(0,metrics.ascent),metrics.text);
        }
    }
}
void LinesView::layoutScreenLabels()const{
    if(!scene_||(!screenDirty_&&!screenLayout_.animating()))return;
    screenDirty_=false;screenLabels_.clear();
    using fs::presentation::LabelKind;
    const auto bounds=QRectF(rect()).adjusted(3,3,-3,-3);
    screenLayout_.begin(bounds,labelClock_.elapsed(),true);
    for(int pass=0;pass<2;++pass)for(int i:scene_->labels){
        const auto kind=labelKinds_[i];if((kind==LabelKind::Named)!=(pass==0))continue;
        const auto& v=scene_->primitives[i];
        auto anchor=camera().map(v.points[0]);const auto& metrics=labelMetrics_.constFind(i).value();
        if(!bounds.adjusted(-192,-192,192,192).intersects(QRectF(anchor,metrics.size)))continue;
        auto placed=screenLayout_.place(QString::number(i),metrics.text,anchor,anchor,metrics.size,kind);
        if(placed.visible)screenLabels_.append({i,placed.position,placed.clip});
    }
}
QVector<QRectF> LinesView::screenLabelRects()const{layoutScreenLabels();QVector<QRectF> out;for(const auto& p:screenLabels_)out.append(p.clip);return out;}
QStringList LinesView::screenLabelTexts()const{
    layoutScreenLabels();
    QStringList result;for(const auto& placed:screenLabels_)result.append(labelMetrics_.constFind(placed.index).value().text);
    return result;
}
void LinesView::renderScreen(QPainter& painter)const{renderScene(painter,false,true,true,true);}
void LinesView::layoutLabels()const{
    if(!scene_||labelScale_==scale_)return;
    labelPlacements_.clear();labelScale_=scale_;
    // Pan changes only offset_. Label choices are recomputed only on zoom.
    for(int pass=0;pass<2;++pass)for(int idx:scene_->labels){const auto&v=scene_->primitives[idx];
        bool numeric=!v.text.isEmpty()&&(v.text[0].isDigit()||v.text[0]=='-');if(numeric!=(pass==1))continue;
        QPointF anchor(v.points[0].x()*scale_,-v.points[0].y()*scale_);
        const auto size=labelMetrics_.constFind(idx).value().size;
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
    QElapsedTimer t;t.start();{QPainter p(this);renderScreen(p);}
    if(screenLayout_.animating())labelTimer_.start();else labelTimer_.stop();
    lastPaintMs=t.nsecsElapsed()/1e6;++paintCount;
    if(firstFramePending_){firstFramePending_=false;if(onFirstFrame)onFirstFrame();}
}
void LinesView::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::RightButton){dragging_=true;lastMouse_=e->position();setCursor(Qt::ClosedHandCursor);e->accept();}else QWidget::mousePressEvent(e);}
void LinesView::mouseMoveEvent(QMouseEvent*e){if(dragging_&&(e->buttons()&Qt::RightButton)){panBy(e->position()-lastMouse_);lastMouse_=e->position();e->accept();}else QWidget::mouseMoveEvent(e);}
void LinesView::mouseReleaseEvent(QMouseEvent*e){if(e->button()==Qt::RightButton){dragging_=false;unsetCursor();e->accept();}else QWidget::mouseReleaseEvent(e);}
void LinesView::wheelEvent(QWheelEvent*e){double delta=e->pixelDelta().isNull()?e->angleDelta().y()/120.0:e->pixelDelta().y()/40.0;zoomAt(e->position(),std::pow(1.2,delta));e->accept();}
void LinesView::resizeEvent(QResizeEvent*e){QWidget::resizeEvent(e);screenDirty_=true;if(fitted_)fit();else update();}
