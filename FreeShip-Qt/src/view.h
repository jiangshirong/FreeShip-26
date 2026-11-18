#pragma once
#include "scene.h"
#include "gpu_fill.h"
#include "render/label_layout.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include <QPainter>
#include <memory>
#include <functional>

class SoftwareScene;
class LinesView:public QWidget {
public:
    explicit LinesView(QWidget*parent=nullptr);
    void setScene(std::shared_ptr<const Scene> scene, bool resetCamera=true);
    void setReferenceCaptions(const QString& baseline, const QString& centerline);
    void fit();
    void panBy(QPointF delta);
    void zoomAt(QPointF screen,double factor);
    void setFill(bool value){fill_=value;update();}
    void renderScene(QPainter &painter,bool raw=false,bool cull=true,bool labels=true,bool interactive=false)const;
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
    QVector<QRectF> screenLabelRects()const;
    QStringList screenLabelTexts()const;
    // Shared with paintEvent; exporting continues to use the fixed layout.
    void renderScreen(QPainter& painter)const;
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
    mutable std::shared_ptr<SoftwareScene> softwareScene_;
    bool gpuEnabled_=true;
    struct LabelPlacement{int index;QRectF rect;};
    mutable QVector<LabelPlacement> labelPlacements_;
    mutable double labelScale_=-1;
    void layoutLabels()const;
    void layoutScreenLabels()const;
    struct ScreenLabel {int index;QPointF position;QRectF clip;};
    mutable fs::presentation::LabelLayout screenLayout_;
    mutable QVector<ScreenLabel> screenLabels_;
    QVector<fs::presentation::LabelKind> labelKinds_;
    void rebuildLabelMetrics();
    QString baselineCaption_,centerlineCaption_;
    struct LabelMetrics {QString text;QFont font;QSizeF size;qreal ascent;};
    QHash<int,LabelMetrics> labelMetrics_;
    mutable bool screenDirty_=true;
    QElapsedTimer labelClock_;
    QTimer labelTimer_;
    double scale_=1;
    QPointF offset_,lastMouse_;
    bool dragging_=false,fill_=true,fitted_=true,firstFramePending_=false;
};
