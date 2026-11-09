// SPDX-License-Identifier: GPL-2.0-or-later
#include "calculation_plot.h"
#include "calculation_charts.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QApplication>
#include <QCache>
#include <QPointer>
#include <QThread>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLFunctions>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QElapsedTimer>
#include <QtMath>
#include <QMap>
#include <QCryptographicHash>
#include <QDataStream>
#include <QtEndian>
#include <QIODevice>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
namespace {
constexpr double NaN=std::numeric_limits<double>::quiet_NaN();
bool finite(QPointF p){return std::isfinite(p.x())&&std::isfinite(p.y());}
QString tickText(double value){return QString::number(value==0?0:value,'g',7);}
QJsonArray rectJson(const QRectF& r){return {r.x(),r.y(),r.width(),r.height()};}
struct Group {QString title,xTitle;QVector<int> curves;double lo=INFINITY,hi=-INFINITY;};
// Use Qt's own stroker, joins, dashes and clipping on an offscreen GPU target.
// No vertex reduction or custom approximation of the curve is involved.
// The GUI owns the surface. A worker owns the context and precompiles the
// painter while the user fills in parameters / the solver runs. Render calls
// serialize with that preparation and return only a detached image to the GUI.
class ChartGpuPainter final:public QObject {
    std::unique_ptr<QOpenGLContext> context_;QOffscreenSurface surface_;
    QThread thread_;QObject worker_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;bool ready_=false,surfacePrepared_=false;QElapsedTimer initializationTimer_;
public:
    QString device;double initializationMs=0,prepareCallMs=0;
    explicit ChartGpuPainter(QObject* parent):QObject(parent){
        setProperty("calculation_gpu_renderer",true);
        // Stop before QApplication starts tearing down its platform/driver.
        // QObject-child destruction is too late if driver creation is pending.
        connect(qApp,&QCoreApplication::aboutToQuit,this,[]{shutdown();});
        initializationTimer_.start();QElapsedTimer timer;timer.start();QSurfaceFormat format;format.setVersion(2,0);format.setRenderableType(QSurfaceFormat::OpenGL);
        worker_.moveToThread(&thread_);thread_.start();
        QMetaObject::invokeMethod(&worker_,[this,format]{
            context_=std::make_unique<QOpenGLContext>();context_->setFormat(format);
            const bool valid=context_->create();const auto actualFormat=context_->format();
            // Load the driver on the worker before creating the GUI-owned
            // surface, and use the actual context format (Qt portability rule).
            QMetaObject::invokeMethod(this,[this,actualFormat,valid]{prepareSurface(actualFormat,valid);},Qt::QueuedConnection);
        },Qt::QueuedConnection);prepareCallMs=timer.nsecsElapsed()/1e6;
    }
    ~ChartGpuPainter(){QMetaObject::invokeMethod(&worker_,[this]{
        if(ready_&&context_->makeCurrent(&surface_)){fbo_.reset();context_->doneCurrent();}
        // Keep creation, use AND native-driver destruction on this thread.
        // Moving an unused Intel context back to the GUI before deleting it can
        // crash inside wglDeleteContext during an immediate application exit.
        context_.reset();worker_.moveToThread(qApp->thread());
    },Qt::BlockingQueuedConnection);thread_.quit();thread_.wait();}
    QImage render(QSize pixels,qreal ratio,const std::function<void(QPainter&)>& draw){
        // A direct render may precede delivery of the queued surface callback.
        // Drain only worker initialization, never run a nested GUI event loop.
        if(!surfacePrepared_){QSurfaceFormat format;bool valid=false;QMetaObject::invokeMethod(&worker_,[&]{format=context_->format();valid=context_->isValid();},Qt::BlockingQueuedConnection);prepareSurface(format,valid);}
        QImage image;QMetaObject::invokeMethod(&worker_,[&]{image=renderOnWorker(pixels,ratio,draw);},Qt::BlockingQueuedConnection);return image;
    }
    static ChartGpuPainter* instance(){auto& painter=storage();if(!painter)painter=new ChartGpuPainter(qApp);return painter;}
    static void shutdown(){delete storage().data();}
private:
    static QPointer<ChartGpuPainter>& storage(){static QPointer<ChartGpuPainter> painter;return painter;}
    void prepareSurface(const QSurfaceFormat& format,bool valid){
        if(surfacePrepared_)return;surfacePrepared_=true;QElapsedTimer timer;timer.start();
        if(valid){surface_.setFormat(format);surface_.create();}
        prepareCallMs+=timer.nsecsElapsed()/1e6;
        QMetaObject::invokeMethod(&worker_,[this]{
            if(surface_.isValid()&&context_->makeCurrent(&surface_)){auto* f=context_->functions();f->initializeOpenGLFunctions();device=QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_RENDERER)));ready_=true;context_->doneCurrent();
                renderOnWorker(QSize(64,64),1,[](QPainter& p){p.setPen(QPen(Qt::blue,2.6));p.drawPolyline(QPolygonF{QPointF(2,2),QPointF(20,30),QPointF(60,60)});});}
            initializationMs=initializationTimer_.nsecsElapsed()/1e6;
        },Qt::QueuedConnection);
    }
    QImage renderOnWorker(QSize pixels,qreal ratio,const std::function<void(QPainter&)>& draw){
        if(!ready_)return {};
        if(!context_->makeCurrent(&surface_))return {};
        auto restore=[&]{context_->doneCurrent();};
        if(!fbo_||fbo_->size()!=pixels){QOpenGLFramebufferObjectFormat format;format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);format.setSamples(8);fbo_=std::make_unique<QOpenGLFramebufferObject>(pixels,format);}
        // Without MSAA, Qt's thick-line GPU coverage is not the required quality.
        if(!fbo_->isValid()||fbo_->format().samples()<4||!fbo_->bind()){restore();return {};}
        auto* f=context_->functions();f->glViewport(0,0,pixels.width(),pixels.height());f->glDisable(GL_SCISSOR_TEST);f->glClearColor(0,0,0,0);f->glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        QOpenGLPaintDevice target(pixels);target.setDevicePixelRatio(ratio);
        {QPainter p(&target);p.setRenderHint(QPainter::Antialiasing);draw(p);}
        QImage result=fbo_->toImage();result.setDevicePixelRatio(ratio);fbo_->release();restore();return result;
    }
};
}
std::shared_ptr<const CalculationChartData> CalculationChartData::read(const QString& method,const QJsonObject& result){
    QElapsedTimer timer;timer.start();auto data=std::make_shared<CalculationChartData>();data->method=method;
    static const auto styles=QJsonDocument::fromJson(CalculationChartStyles).object();const auto style=styles[method].toObject();
    data->legendRight=style["legend_right"].toBool();const auto definitions=style["series"].toArray();int index=0;
    for(auto value:result["series"].toArray()){
        const auto s=value.toObject();CalculationCurve c;c.source=s["source"].toString();c.name=s["name"].toString().trimmed();
        c.xTitle=s["x_title"].toString();c.yTitle=s["y_title"].toString();c.axis=c.yTitle;
        if(method=="propeller1"&&c.source=="Series3")c.axis=c.yTitle="螺旋桨直径 (m)";
        if(method=="wind")c.axis="Cx；Cy；10 × Cmz（无量纲）";
        if(method=="propeller4"||method=="propeller5")c.axis="KT；10 × KQ；η₀（无量纲）";
        QJsonObject definition;
        for(auto d:definitions)if(d.toObject()["source"].toString()==c.source&&!c.source.isEmpty())definition=d.toObject();
        if(c.source.isEmpty()&&index<definitions.size())definition=definitions[index].toObject();
        c.color=QColor(definition["color"].toString("#000000"));c.width=std::max(1,definition["width"].toInt(1));
        const auto pen=definition["style"].toString();c.style=pen=="psDash"?Qt::DashLine:pen=="psDot"?Qt::DotLine:pen=="psDashDot"?Qt::DashDotLine:Qt::SolidLine;c.markers=definition["points"].toBool();
        const auto points=s["points"].toArray();c.points.reserve(points.size());
        QElapsedTimer decode;decode.start();double xmin=INFINITY,xmax=-INFINITY,ymin=INFINITY,ymax=-INFINITY;bool previous=false;
        for(auto p:points){const auto xy=p.toArray();const auto a=xy.at(0),b=xy.at(1);const auto point=a.isDouble()&&b.isDouble()?QPointF(a.toDouble(),b.toDouble()):QPointF(NaN,NaN);c.points.append(point);
            if(!finite(point)){previous=false;continue;}xmin=std::min(xmin,point.x());xmax=std::max(xmax,point.x());ymin=std::min(ymin,point.y());ymax=std::max(ymax,point.y());++c.finitePoints;if(previous)++c.segments;previous=true;}
        if(c.finitePoints)c.bounds=QRectF(QPointF(xmin,ymin),QPointF(xmax,ymax));
        data->decodeMs+=decode.nsecsElapsed()/1e6;
        data->curves.append(std::move(c));++index;
    }
    data->preparationMs=timer.nsecsElapsed()/1e6;return data;
}
QString CalculationChartData::numericFingerprint()const{
    if(!numericSha256_.isEmpty())return numericSha256_;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto& c:curves){
        // The fingerprint is the same little-endian doubles as the old stream,
        // without millions of virtual stream writes and buffer growths.
        char coordinates[64*1024];qToLittleEndian<quint64>(c.points.size(),coordinates);hash.addData(QByteArrayView(coordinates,8));qsizetype used=0;
        for(auto p:c.points){for(double value:{p.x(),p.y()}){quint64 bits;std::memcpy(&bits,&value,sizeof(bits));qToLittleEndian(bits,coordinates+used);used+=8;}
            if(used==sizeof(coordinates)){hash.addData(QByteArrayView(coordinates,used));used=0;}}
        if(used)hash.addData(QByteArrayView(coordinates,used));
    }
    numericSha256_=QString::fromLatin1(hash.result().toHex());return numericSha256_;
}
CalculationAxis CalculationAxis::fit(double lo,double hi,int intervals){
    CalculationAxis axis;if(!std::isfinite(lo)||!std::isfinite(hi)||hi<lo)return axis;
    if(lo==hi){const double delta=std::max(1.,std::abs(lo)*.05);lo-=delta;hi+=delta;}
    const double rough=(hi-lo)/std::max(2,intervals),unit=std::pow(10.,std::floor(std::log10(rough)));
    if(!std::isfinite(rough)||rough<=0||!std::isfinite(unit)||unit<=0){axis.minimum=lo;axis.maximum=hi;return axis;}
    const double fraction=rough/unit,nice=fraction<=1?1:fraction<=2?2:fraction<=2.5?2.5:fraction<=5?5:10,step=unit*nice;
    axis.minimum=std::floor(lo/step)*step;axis.maximum=std::ceil(hi/step)*step;
    for(int i=0;i<100;++i){const double value=axis.minimum+i*step;if(value>axis.maximum+step*1e-8)break;axis.ticks.append(std::abs(value)<step*1e-12?0:value);}
    return axis;
}
class CalculationCanvas final:public QWidget {
public:
    std::shared_ptr<CalculationChartState> state;bool dirty=true;QPixmap cached;QJsonObject observation;QJsonArray history;quint64 builds=0,paints=0;
    QCache<QByteArray,QImage> layers{64*1024};quint64 layerBuilds=0,layerHits=0; // KiB, bounded per page.
    explicit CalculationCanvas(QWidget* parent):QWidget(parent){setMinimumSize(180,160);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);}
    void invalidate(){extent();dirty=true;update();}
    bool paired()const{return width()>=std::max(450,fontMetrics().averageCharWidth()*34);}
    void extent(){
        QStringList groups;if(state&&state->data)for(int i=0;i<state->data->curves.size();++i){if(state->selectedOnly?i!=state->selected:!state->visible.value(i))continue;
            const auto& c=state->data->curves[i];const auto key=c.xTitle+"\n"+c.axis;if(!groups.contains(key))groups.append(key);}
        const int panels=std::max(1,(int(groups.size())+(paired()?1:0))/(paired()?2:1));
        setMinimumHeight(panels*std::max(150,fontMetrics().height()*7));
    }
protected:
    void resizeEvent(QResizeEvent* event)override{QWidget::resizeEvent(event);invalidate();}
    void changeEvent(QEvent* event)override{QWidget::changeEvent(event);if(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange||event->type()==QEvent::PaletteChange)invalidate();}
    void paintEvent(QPaintEvent*)override{
        ++paints;const auto ratio=devicePixelRatioF();const QSize pixels(std::max(1,qCeil(width()*ratio)),std::max(1,qCeil(height()*ratio)));
        if(dirty||cached.size()!=pixels||cached.devicePixelRatioF()!=ratio){
            QElapsedTimer timer;timer.start();cached=QPixmap(pixels);cached.setDevicePixelRatio(ratio);cached.fill(Qt::white);QPainter painter(&cached);painter.setFont(font());draw(painter);dirty=false;++builds;observation["render_ms"]=timer.nsecsElapsed()/1e6;
            auto record=observation;record.remove("panels");record["width"]=width();record["height"]=height();record["layer_builds"]=double(layerBuilds);record["layer_hits"]=double(layerHits);if(history.size()==32)history.removeFirst();history.append(record);
        }
        QPainter painter(this);painter.drawPixmap(0,0,cached);
    }
private:
    void draw(QPainter& painter){
        observation={};QElapsedTimer stageTimer;stageTimer.start();double mappingMs=0,strokeMs=0;QVector<Group> groups;double xmin=INFINITY,xmax=-INFINITY;qint64 count=0,segments=0;
        if(state&&state->data)for(int i=0;i<state->data->curves.size();++i){
            if(state->selectedOnly?i!=state->selected:!state->visible.value(i))continue;
            const auto& curve=state->data->curves[i];int group=-1;
            for(int j=0;j<groups.size();++j)if(groups[j].title==curve.axis&&groups[j].xTitle==curve.xTitle)group=j;
            if(group<0){group=groups.size();groups.append(Group{curve.axis,curve.xTitle,{}});}
            groups[group].curves.append(i);
            if(curve.finitePoints){xmin=std::min(xmin,curve.bounds.left());xmax=std::max(xmax,curve.bounds.right());groups[group].lo=std::min(groups[group].lo,curve.bounds.top());groups[group].hi=std::max(groups[group].hi,curve.bounds.bottom());count+=curve.finitePoints;segments+=curve.segments;}
        }
        observation["points"]=double(count);observation["segments"]=double(segments);
        observation["range_ms"]=stageTimer.nsecsElapsed()/1e6;
        if(!count){painter.setPen(QColor(65,75,85));painter.drawText(rect().adjusted(10,10,-10,-10),Qt::AlignCenter|Qt::TextWordWrap,state&&state->data&&!state->data->curves.isEmpty()?"没有显示的曲线，请在图例中勾选。":"计算完成后显示曲线");return;}
        groups.erase(std::remove_if(groups.begin(),groups.end(),[](const Group& g){return !std::isfinite(g.lo);}),groups.end());
        QVector<QVector<int>> panels;for(int i=0;i<groups.size();++i){if(panels.isEmpty()||panels.last().size()==(paired()?2:1)||groups[panels.last()[0]].xTitle!=groups[i].xTitle)panels.append(QVector<int>{});panels.last().append(i);}
        const auto fm=fontMetrics();const int h=fm.height();const auto x=CalculationAxis::fit(xmin,xmax,std::max(2,width()/125));QJsonArray evidence;
        const double panelHeight=double(height())/panels.size();painter.setRenderHint(QPainter::Antialiasing);
        // All panels share the same plot width and X range, so time histories
        // remain aligned even when the quantities have different dimensions.
        double left=0,right=10;QVector<CalculationAxis> axes;
        for(const auto& group:groups)axes.append(CalculationAxis::fit(group.lo,group.hi,std::max(2,int(panelHeight)/(3*h))));
        for(const auto& panel:panels)for(int side=0;side<panel.size();++side){double width=0;for(double t:axes[panel[side]].ticks)width=std::max(width,double(fm.horizontalAdvance(tickText(t))));const double margin=width+2*h+14;if(side==0)left=std::max(left,margin);else right=std::max(right,margin);}
        for(int panelIndex=0;panelIndex<panels.size();++panelIndex){
            const auto panel=panels[panelIndex];const double top=panelIndex*panelHeight+h*.5;QRectF plot(left,top,std::max(1.,width()-left-right),std::max(1.,panelHeight-3.5*h-8));
            auto px=[&](double value){return plot.left()+(value-x.minimum)/(x.maximum-x.minimum)*plot.width();};
            QJsonArray ys,drawn;
            painter.setPen(QColor(210,220,228));painter.drawRect(plot);
            for(double tick:x.ticks){const double xx=px(tick);painter.setPen(QColor(231,235,239));painter.drawLine(QPointF(xx,plot.top()),QPointF(xx,plot.bottom()));painter.setPen(QColor(55,65,75));painter.drawText(QRectF(xx-55,plot.bottom()+3,110,h+2),Qt::AlignHCenter|Qt::AlignTop,tickText(tick));}
            painter.drawText(QRectF(plot.left(),plot.bottom()+h+6,plot.width(),h*1.5),Qt::AlignCenter,groups[panel[0]].xTitle);
            for(int side=0;side<panel.size();++side){
                const auto& group=groups[panel[side]];const auto& y=axes[panel[side]];QJsonArray ticks;
                auto py=[&](double value){return plot.bottom()-(value-y.minimum)/(y.maximum-y.minimum)*plot.height();};
                for(double tick:y.ticks){const auto yy=py(tick);ticks.append(tick);if(side==0){painter.setPen(QColor(231,235,239));painter.drawLine(QPointF(plot.left(),yy),QPointF(plot.right(),yy));}painter.setPen(QColor(55,65,75));
                    const QRectF label=side==0?QRectF(2*h,yy-h/2.,left-2*h-7,h+2):QRectF(plot.right()+7,yy-h/2.,right-2*h-7,h+2);
                    painter.drawText(label,(side==0?Qt::AlignRight:Qt::AlignLeft)|Qt::AlignVCenter,tickText(tick));}
                painter.save();painter.translate(side==0?h*.8:width()-h*.8,plot.center().y());painter.rotate(side==0?-90:90);
                painter.drawText(QRectF(-plot.height()/2,-h/2.,plot.height(),h+2),Qt::AlignCenter,fm.elidedText(group.title,Qt::ElideMiddle,int(plot.height())));painter.restore();
                ys.append(QJsonObject{{"title",group.title},{"minimum",y.minimum},{"maximum",y.maximum},{"ticks",ticks},{"side",side==0?"left":"right"}});
                painter.save();painter.setClipRect(plot.adjusted(-1,-1,1,1));
                for(int id:group.curves){const auto& curve=state->data->curves[id];const auto box=plot.adjusted(-1,-1,1,1).toAlignedRect();const auto ratio=devicePixelRatioF();
                    const QSize pixels(qCeil(box.width()*ratio),qCeil(box.height()*ratio));const double lineWidth=curve.width+(id==state->selected?.6:0);
                    QByteArray key;QDataStream keys(&key,QIODevice::WriteOnly);keys<<id<<pixels<<ratio<<plot.translated(-box.topLeft())<<x.minimum<<x.maximum<<y.minimum<<y.maximum<<lineWidth;
                    const QImage* layer=layers.object(key);QImage fresh;
                    if(layer)++layerHits;else{
                        auto paintCurve=[&](QPainter& target){QPolygonF line;line.reserve(curve.points.size());QVector<QPointF> markers;
                            target.translate(-box.topLeft());target.setClipRect(plot.adjusted(-1,-1,1,1));target.setPen(QPen(curve.color,lineWidth,curve.style));
                            auto stroke=[&]{QElapsedTimer timer;timer.start();if(line.size()>1)target.drawPolyline(line);else if(line.size()==1)markers.append(line[0]);strokeMs+=timer.nsecsElapsed()/1e6;line.clear();};
                            stageTimer.restart();const auto previousStrokeMs=strokeMs;
                            for(auto point:curve.points){if(!finite(point)){stroke();continue;}const QPointF mapped(px(point.x()),py(point.y()));line.append(mapped);if(curve.markers)markers.append(mapped);}stroke();
                            mappingMs+=stageTimer.nsecsElapsed()/1e6-(strokeMs-previousStrokeMs);
                            target.setBrush(curve.color);for(auto point:markers)target.drawEllipse(point,2.5,2.5);
                        };
                        if(curve.finitePoints>=8192&&!qEnvironmentVariableIntValue("FREESHIP_CHART_SOFTWARE")){
                            auto* gpu=ChartGpuPainter::instance();fresh=gpu->render(pixels,ratio,paintCurve);observation["gpu_device"]=gpu->device;observation["gpu_initialization_ms"]=gpu->initializationMs;observation["gpu_prepare_call_ms"]=gpu->prepareCallMs;
                            if(!fresh.isNull())observation["gpu_layers"]=observation["gpu_layers"].toInt()+1;
                        }
                        if(fresh.isNull()){fresh=QImage(pixels,QImage::Format_ARGB32_Premultiplied);fresh.setDevicePixelRatio(ratio);fresh.fill(Qt::transparent);QPainter target(&fresh);target.setRenderHint(QPainter::Antialiasing);paintCurve(target);observation["software_layers"]=observation["software_layers"].toInt()+1;}
                        ++layerBuilds;const auto cost=std::max<qint64>(1,(fresh.sizeInBytes()+1023)/1024);
                        if(cost<=layers.maxCost()){layers.insert(key,new QImage(fresh),int(cost));layer=layers.object(key);}else layer=&fresh;
                    }
                    painter.drawImage(box.topLeft(),*layer);drawn.append(id);
                }
                painter.restore();
            }
            evidence.append(QJsonObject{{"plot",rectJson(plot)},{"x_title",groups[panel[0]].xTitle},{"x_minimum",x.minimum},{"x_maximum",x.maximum},{"axes",ys},{"curves",drawn}});
        }
        observation["panels"]=evidence;
        observation["mapping_ms"]=mappingMs;observation["stroke_ms"]=strokeMs;
    }
};
CalculationPlot::CalculationPlot(QWidget* parent):QWidget(parent){
    setProperty("performance_chart",true);auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);
    single_=new QCheckBox("仅显示选中曲线");single_->setProperty("chart_selected_only",true);root->addWidget(single_);
    content_=new QBoxLayout(QBoxLayout::TopToBottom);root->addLayout(content_,1);
    legend_=new QScrollArea;legend_->setFrameShape(QFrame::NoFrame);legend_->setWidgetResizable(true);legend_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);content_->addWidget(legend_);
    legend_->setStyleSheet("QToolButton {color:#7b8582;background:transparent;border:1px solid transparent;border-radius:2px;padding:3px;} QToolButton:checked {color:#173b37;background:#eff6f3;border-color:#d6e3de;} QToolButton:hover {background:#e7f2fc;border-color:#abd5f5;}");
    auto* plotScroll=new QScrollArea;plotScroll->setFrameShape(QFrame::NoFrame);plotScroll->setWidgetResizable(true);plotScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    canvas_=new CalculationCanvas(this);plotScroll->setWidget(canvas_);content_->addWidget(plotScroll,1);
    connect(single_,&QCheckBox::toggled,this,[this](bool value){if(!state_)return;state_->selectedOnly=value;synchronize();if(onDisplayChanged)onDisplayChanged();});
}
void CalculationPlot::prepareRenderer(){if(!qEnvironmentVariableIntValue("FREESHIP_CHART_SOFTWARE"))ChartGpuPainter::instance();}
void CalculationPlot::shutdownRenderer(){ChartGpuPainter::shutdown();}
void CalculationPlot::setState(std::shared_ptr<CalculationChartState> state){
    state_=std::move(state);canvas_->state=state_;canvas_->layers.clear();canvas_->history={};buttons_.clear();legendLayout_=nullptr;auto* body=new QWidget;auto* grid=new QGridLayout(body);grid->setContentsMargins(2,2,2,2);grid->setSpacing(3);
    QMap<QString,int> duplicates;if(state_&&state_->data)for(const auto& c:state_->data->curves)++duplicates[c.name];
    QMap<QString,int> ordinals;
    if(state_&&state_->data)for(int i=0;i<state_->data->curves.size();++i){const auto& c=state_->data->curves[i];auto* button=new QToolButton;auto label=c.name;if(duplicates[c.name]>1)label+=" · "+QString::number(++ordinals[c.name]);button->setText(label);button->setToolTip(c.name+"\n"+c.xTitle+" / "+c.yTitle);button->setCheckable(true);button->setProperty("chart_curve",i);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);button->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        QPixmap icon(28,16);icon.fill(Qt::transparent);QPainter p(&icon);p.setPen(QPen(c.color,std::max(2.,c.width),c.style));p.drawLine(2,8,26,8);button->setIcon(QIcon(icon));buttons_.append(button);grid->addWidget(button,i,0);
        connect(button,&QToolButton::toggled,this,[this,i](bool value){if(!state_)return;state_->visible[i]=value;synchronize();if(onDisplayChanged)onDisplayChanged();});
    }
    legend_->setWidget(body);legendLayout_=grid;legendColumns_=0;arrangeLegend();synchronize();
}
void CalculationPlot::synchronize(){
    QSignalBlocker guard(single_);single_->setChecked(state_&&state_->selectedOnly);single_->setEnabled(state_&&state_->data&&!state_->data->curves.isEmpty());
    for(int i=0;i<buttons_.size();++i){QSignalBlocker guard(buttons_[i]);buttons_[i]->setChecked(state_&&state_->visible.value(i));buttons_[i]->setEnabled(!state_||!state_->selectedOnly);}
    canvas_->invalidate();
}
void CalculationPlot::setSelected(int index){if(!state_)return;state_->selected=index;canvas_->invalidate();}
void CalculationPlot::arrangeLegend(){
    if(!legendLayout_)return;const bool right=state_&&state_->data&&state_->data->legendRight&&width()>=680;
    const int available=right?std::max(140,fontMetrics().height()*9):std::max(140,width()-22);int itemWidth=100;
    for(auto* button:buttons_)itemWidth=std::max(itemWidth,std::min(available,button->sizeHint().width()));
    const int columns=right?1:std::max(1,available/(itemWidth+3));
    if(columns!=legendColumns_||right!=right_){for(auto* button:buttons_)legendLayout_->removeWidget(button);for(int c=0;c<legendColumns_;++c)legendLayout_->setColumnStretch(c,0);
        for(int i=0;i<buttons_.size();++i)legendLayout_->addWidget(buttons_[i],i/columns,i%columns);legendColumns_=columns;right_=right;}
    const int rows=(buttons_.size()+columns-1)/columns;const int rowHeight=buttons_.isEmpty()?0:buttons_[0]->sizeHint().height()+3;
    content_->setDirection(right?QBoxLayout::RightToLeft:QBoxLayout::TopToBottom);
    legend_->setMinimumWidth(right?available+18:0);legend_->setMaximumWidth(right?available+18:QWIDGETSIZE_MAX);
    legend_->setMinimumHeight(right?0:std::min(140,rows*rowHeight+6));legend_->setMaximumHeight(right?QWIDGETSIZE_MAX:std::min(140,rows*rowHeight+6));
    legend_->setVisible(!buttons_.isEmpty());
}
void CalculationPlot::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);arrangeLegend();}
void CalculationPlot::changeEvent(QEvent* event){QWidget::changeEvent(event);if(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange){legendColumns_=0;arrangeLegend();}}
QJsonObject CalculationPlot::evidence()const{
    auto out=canvas_->observation;out["cache_builds"]=double(canvas_->builds);out["paints"]=double(canvas_->paints);out["selected_only"]=state_&&state_->selectedOnly;out["selected"]=state_?state_->selected:-1;
    out["layer_builds"]=double(canvas_->layerBuilds);out["layer_hits"]=double(canvas_->layerHits);out["layer_cache_kib"]=canvas_->layers.totalCost();out["render_history"]=canvas_->history;
    out["preparation_ms"]=state_&&state_->data?state_->data->preparationMs:0;out["decode_ms"]=state_&&state_->data?state_->data->decodeMs:0;
    QJsonArray curves;int i=0;if(state_&&state_->data)for(const auto& c:state_->data->curves)curves.append(QJsonObject{{"index",i},{"source",c.source},{"name",c.name},{"color",c.color.name()},{"axis",c.axis},{"points",double(c.points.size())},{"visible",state_->visible.value(i++)}});
    out["curves"]=curves;out["numeric_sha256"]=state_&&state_->data?state_->data->numericFingerprint():QString{};return out;
}
