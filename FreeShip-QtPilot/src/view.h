#pragma once
#include "scene.h"
#include "gpu_fill.h"
#include <QWidget>
#include <QPainter>
#include <memory>
#include <functional>

class LinesView:public QWidget {
public:
    explicit LinesView(QWidget*parent=nullptr);
    void setScene(std::shared_ptr<const Scene> scene);
    void fit();
    void panBy(QPointF delta);
    void zoomAt(QPointF screen,double factor);
    void setFill(bool value){fill_=value;update();}
    void renderScene(QPainter &painter,bool raw=false,bool cull=true,bool labels=true)const;
    QImage image(bool raw=false,bool cull=true,bool labels=true,qreal dpr=1)const;
    QTransform camera()const;
    double lastPaintMs=0;
    quint64 paintCount=0;
    std::function<void()> onFirstFrame;
    double scale()const{return scale_;}
    QPointF offset()const{return offset_;}
    QString gpuDevice()const{return gpu_&&gpu_->ready()?gpu_->device:QString("CPU fallback");}
    double gpuInitializationMs()const{return gpu_?gpu_->initializationMs:0;}
    void setGpuEnabled(bool enabled){gpuEnabled_=enabled;update();}
    QVector<QRectF> visibleLabelRects()const;
protected:
    void paintEvent(QPaintEvent*)override;
    void mousePressEvent(QMouseEvent*)override;
    void mouseMoveEvent(QMouseEvent*)override;
    void mouseReleaseEvent(QMouseEvent*)override;
    void wheelEvent(QWheelEvent*)override;
    void resizeEvent(QResizeEvent*)override;
private:
    std::shared_ptr<const Scene> scene_;
    mutable std::unique_ptr<GpuFill> gpu_;
    bool gpuEnabled_=true;
    struct LabelPlacement{int index;QRectF rect;};
    mutable QVector<LabelPlacement> labelPlacements_;
    mutable double labelScale_=-1;
    void layoutLabels()const;
    double scale_=1;
    QPointF offset_,lastMouse_;
    bool dragging_=false,fill_=true,fitted_=true,firstFramePending_=false;
};
