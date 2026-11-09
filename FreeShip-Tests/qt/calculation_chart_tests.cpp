// Independent numeric/interaction checks of the production chart widget.
#include "app/calculation_plot.h"
#include <QApplication>
#include <QCheckBox>
#include <QToolButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QPainter>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLFunctions>
#include <QTimer>
#include <cmath>
// Diagnostic backend comparison, retaining every vertex and the same QPen.
// It is deliberately separate from numerical/visual acceptance.
static QJsonObject rendererProbe(const CalculationChartData& data,const QString& out){
    QVector<QPolygonF> lines;for(const auto& curve:data.curves){double xmin=INFINITY,xmax=-INFINITY,ymin=INFINITY,ymax=-INFINITY;
        for(auto p:curve.points)if(std::isfinite(p.x())&&std::isfinite(p.y())){xmin=qMin(xmin,p.x());xmax=qMax(xmax,p.x());ymin=qMin(ymin,p.y());ymax=qMax(ymax,p.y());}
        QPolygonF line;for(auto p:curve.points)if(std::isfinite(p.x())&&std::isfinite(p.y()))line.append(QPointF(10+(p.x()-xmin)/qMax(1e-20,xmax-xmin)*980,490-(p.y()-ymin)/qMax(1e-20,ymax-ymin)*480));lines.append(line);}
    QJsonArray measurements;const QSize size(1000,500);
    auto draw=[&](QPaintDevice* device,bool cosmetic){QPainter p(device);p.fillRect(QRect(QPoint{},size),Qt::white);p.setRenderHint(QPainter::Antialiasing);
        for(int i=0;i<lines.size();++i){QPen pen(data.curves[i].color,data.curves[i].width+(i==0?.6:0),data.curves[i].style);pen.setCosmetic(cosmetic);p.setPen(pen);p.drawPolyline(lines[i]);}};
    for(bool cosmetic:{false,true}){QImage im(size,QImage::Format_ARGB32_Premultiplied);QElapsedTimer timer;timer.start();draw(&im,cosmetic);measurements.append(QJsonObject{{"backend",cosmetic?"raster-cosmetic":"raster"},{"ms",timer.nsecsElapsed()/1e6}});im.save(out+(cosmetic?"/cosmetic.png":"/raster.png"));}
    QElapsedTimer init;init.start();QOpenGLContext context;QSurfaceFormat format;format.setVersion(2,0);format.setRenderableType(QSurfaceFormat::OpenGL);context.setFormat(format);
    bool ready=context.create();QOffscreenSurface surface;surface.setFormat(context.format());surface.create();ready &= surface.isValid()&&context.makeCurrent(&surface);const double initMs=init.nsecsElapsed()/1e6;
    QString device;if(ready){auto* f=context.functions();f->initializeOpenGLFunctions();device=QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_RENDERER)));
        for(int samples:{0,4,8}){QOpenGLFramebufferObjectFormat target;target.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);target.setSamples(samples);QOpenGLFramebufferObject fbo(size,target);
            if(!fbo.isValid()||!fbo.bind())continue;QOpenGLPaintDevice paint(size);QElapsedTimer timer;timer.start();draw(&paint,false);auto im=fbo.toImage();measurements.append(QJsonObject{{"backend",QString("opengl-%1").arg(samples)},{"ms",timer.nsecsElapsed()/1e6},{"samples",fbo.format().samples()}});im.save(out+QString("/opengl-%1.png").arg(samples));fbo.release();}
    }
    return {{"status","pass"},{"cases",QJsonArray{QJsonObject{{"name","renderer-profile-only"},{"status","review"}}}},{"measurements",measurements},{"gl_ready",ready},{"gl_init_ms",initMs},{"device",device}};
}
// Compare the accelerated stroke with Qt's full-resolution software reference.
// Only anti-alias coverage may differ: every strongly covered pixel must have
// matching coverage within one physical pixel, in both directions.
static QJsonObject coverage(const QImage& a,const QImage& b){
    auto blue=[](QRgb p){return qBlue(p)>qRed(p)+70&&qBlue(p)>qGreen(p)+70;};
    auto unmatched=[&](const QImage& from,const QImage& to){qint64 total=0,missing=0;
        for(int y=0;y<from.height();++y)for(int x=0;x<from.width();++x)if(blue(from.pixel(x,y))){++total;bool found=false;
            for(int yy=qMax(0,y-1);yy<=qMin(to.height()-1,y+1)&&!found;++yy)for(int xx=qMax(0,x-1);xx<=qMin(to.width()-1,x+1);++xx)if(blue(to.pixel(xx,yy))){found=true;break;}if(!found)++missing;}
        return QJsonObject{{"pixels",double(total)},{"missing",double(missing)}};};
    return {{"same_size",a.size()==b.size()},{"software",unmatched(a,b)},{"gpu",unmatched(b,a)}};
}
int main(int argc,char** argv){
    if(argc==4&&QByteArray(argv[2]).startsWith("--scale="))qputenv("QT_SCALE_FACTOR",QByteArray(argv[2]).mid(8));
    QApplication app(argc,argv);struct RendererLifetime{~RendererLifetime(){CalculationPlot::shutdownRenderer();}} rendererLifetime;
    if(argc!=3&&argc!=4)return 3;const QString out=argv[argc-1];QDir().mkpath(out);QJsonArray cases,observations;
    if(QString(argv[1])=="--warmup-exit"){
        CalculationPlot::prepareRenderer();
        // Exercise the application's actual aboutToQuit cleanup before a
        // context has finished loading. The child runner checks the exit too.
        QTimer::singleShot(0,&app,&QCoreApplication::quit);app.exec();bool stopped=true;
        for(auto* object:app.findChildren<QObject*>())if(object->property("calculation_gpu_renderer").toBool())stopped=false;
        // Check the product hook already ran, before the test host's RAII guard.
        QFile f(out+"/report.json");if(!f.open(QIODevice::WriteOnly))return 3;const auto status=stopped?"pass":"fail";
        f.write(QJsonDocument(QJsonObject{{"status",status},{"cases",QJsonArray{QJsonObject{{"name","shutdown-during-gpu-preparation"},{"status",status}}}}}).toJson());return stopped?0:1;
    }
    auto check=[&](QString name,bool ok){cases.append(QJsonObject{{"name",name},{"status",ok?"pass":"fail"}});};
    auto settle=[&]{for(int i=0;i<3;++i){app.sendPostedEvents();app.processEvents();}};
    QJsonObject result;QJsonArray renderComparisons;QString method="fixture";bool synthetic=QString(argv[1])=="-";
    if(synthetic){QJsonArray series;
        series.append(QJsonObject{{"name","resistance"},{"x_title","speed (kn)"},{"y_title","force (kN)"},{"points",QJsonArray{QJsonArray{0,-3},QJsonArray{1,1},QJsonArray{1,2},QJsonArray{2,7}}}});
        series.append(QJsonObject{{"name","power"},{"x_title","speed (kn)"},{"y_title","power (kW)"},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{1,100},QJsonArray{2,300}}}});
        series.append(QJsonObject{{"name","reference"},{"x_title","speed (kn)"},{"y_title","force (kN)"},{"points",QJsonArray{QJsonArray{1,-2},QJsonArray{1,7}}}});result["series"]=series;
        auto axis=CalculationAxis::fit(-3,7);check("independent-nice-axis",axis.minimum==-4&&axis.maximum==8&&axis.ticks==QVector<double>{-4,-2,0,2,4,6,8});
        for(double value:{0.,-2.,.00001,1000000.}){auto a=CalculationAxis::fit(value,value);check("constant-range-"+QString::number(value),a.minimum<value&&a.maximum>value&&std::isfinite(a.minimum)&&std::isfinite(a.maximum));}
    }else{if(argc==3)CalculationPlot::prepareRenderer();QFile f(argv[1]);if(!f.open(QIODevice::ReadOnly))return 3;result=QJsonDocument::fromJson(f.readAll()).object();method=result["calculator"].toString();}
    const auto original=QJsonDocument(result).toJson(QJsonDocument::Compact);auto data=CalculationChartData::read(method,result);
    if(argc==4&&QString(argv[2])=="--renderers"){QFile f(out+"/report.json");if(!f.open(QIODevice::WriteOnly))return 3;f.write(QJsonDocument(rendererProbe(*data,out)).toJson());return 0;}
    auto state=std::make_shared<CalculationChartState>();state->data=data;state->visible.fill(true,data->curves.size());
    CalculationPlot view,detail;view.setAttribute(Qt::WA_DontShowOnScreen);detail.setAttribute(Qt::WA_DontShowOnScreen);view.setState(state);detail.setState(state);int callbacks=0;
    view.onDisplayChanged=[&]{++callbacks;detail.synchronize();};view.resize(1000,650);view.show();settle();view.grab().save(out+"/all.png");const auto first=view.evidence();
    const auto firstImage=view.grab().toImage();check("repeat-paint-reuses-cache",view.evidence()["cache_builds"]==first["cache_builds"]);
    qint64 points=0;bool exact=true;
    for(int i=0;i<data->curves.size();++i){const auto raw=result["series"].toArray()[i].toObject()["points"].toArray();exact &= data->curves[i].points.size()==raw.size();points+=raw.size();
        for(int j=0;j<raw.size();++j)exact &= data->curves[i].points[j]==QPointF(raw[j].toArray()[0].toDouble(),raw[j].toArray()[1].toDouble());}
    check("every-double-and-input-order-retained",exact);check("overlay-draws-every-point",first["points"].toDouble()==points);observations.append(first);
    if(!data->curves.isEmpty()){
        QToolButton* toggle=nullptr;for(auto* b:view.findChildren<QToolButton*>())if(b->property("chart_curve")==0)toggle=b;
        check("legend-action-discovered",toggle!=nullptr);if(toggle){toggle->click();settle();view.grab().save(out+"/hidden-first.png");check("legend-hides-only-one-curve",view.evidence()["points"].toDouble()==points-data->curves[0].points.size());check("legend-state-shared",!detail.evidence()["curves"].toArray()[0].toObject()["visible"].toBool()&&callbacks==1);
            const auto beforeRestore=view.evidence();toggle->click();settle();view.grab();const auto restored=view.evidence();observations.append(restored);
            check("legend-restore-exact-image",view.grab().toImage()==firstImage);
            if(beforeRestore["layer_cache_kib"].toInt()<32*1024)check("legend-restore-reuses-curve-layers",restored["layer_builds"]==beforeRestore["layer_builds"]&&restored["layer_hits"].toDouble()>beforeRestore["layer_hits"].toDouble());
            const auto oldBuilds=restored["layer_builds"].toDouble();view.setSelected(data->curves.size()-1);settle();view.grab();check("selection-reuses-unchanged-curves",view.evidence()["layer_builds"].toDouble()-oldBuilds<=2);observations.append(view.evidence());view.setSelected(0);settle();view.grab();}
        auto* single=view.findChild<QCheckBox*>();single->click();const int last=data->curves.size()-1;view.setSelected(last);settle();view.grab().save(out+"/single.png");check("selected-only-exact-points",view.evidence()["points"].toDouble()==data->curves[last].points.size());single->click();
        for(auto* b:view.findChildren<QToolButton*>())if(b->isChecked())b->click();settle();view.grab();check("all-hidden-safe",view.evidence()["points"].toDouble()==0);for(auto* b:view.findChildren<QToolButton*>())if(!b->isChecked())b->click();
    }
    for(int size:synthetic?QList<int>{9,14,18}:QList<int>{14})for(int width:QList<int>{1000,520}){
        auto font=view.font();font.setPointSize(size);view.setFont(font);view.resize(width,650);settle();view.grab().save(out+QString("/font%1-width%2.png").arg(size).arg(width));const auto seen=view.evidence();observations.append(seen);
        check(QString("reflow-keeps-data-%1-%2").arg(size).arg(width),seen["numeric_sha256"]==first["numeric_sha256"]&&seen["points"].toDouble()==points);
        bool valid=true;for(auto value:seen["panels"].toArray()){auto p=value.toObject();auto r=p["plot"].toArray();valid &= r[2].toDouble()>40&&r[3].toDouble()>25;
            for(auto a:p["axes"].toArray()){auto axis=a.toObject();valid &= axis["maximum"].toDouble()>axis["minimum"].toDouble();}}
        check(QString("readable-plot-area-%1-%2").arg(size).arg(width),valid);
    }
    if(synthetic){
        QJsonObject gaps{{"series",QJsonArray{QJsonObject{{"name","gaps"},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{1,1},QJsonArray{QJsonValue(),QJsonValue()},QJsonArray{3,3},QJsonArray{4,4}}}}}}};
        auto broken=std::make_shared<CalculationChartState>();broken->data=CalculationChartData::read("",gaps);broken->visible={true};view.setState(broken);settle();view.grab();check("invalid-sample-breaks-line",view.evidence()["points"].toInt()==4&&view.evidence()["segments"].toInt()==2);
        CalculationPlot::prepareRenderer();QJsonArray densePoints;
        for(int i=0;i<12000;++i){double x=i/11999.,y=std::sin(x*17)*.6;
            if(i==3777)y=1.4; // A single-sample spike must survive.
            if(i==6000||i==6001){densePoints.append(QJsonArray{QJsonValue(),QJsonValue()});continue;}
            if(i>=8000&&i<8200)x=.65; // Duplicate X, vertical strokes and reversal.
            densePoints.append(QJsonArray{x,y});}
        QJsonObject raw{{"series",QJsonArray{QJsonObject{{"name","dense"},{"points",densePoints}}}}};
        for(auto style:{Qt::SolidLine,Qt::DashLine,Qt::DotLine,Qt::DashDotLine}){
            auto full=std::make_shared<CalculationChartData>(*CalculationChartData::read("",raw));full->curves[0].color=Qt::blue;full->curves[0].width=2;full->curves[0].style=style;
            auto display=std::make_shared<CalculationChartState>();display->data=full;display->visible={true};
            CalculationPlot software,accelerated;software.setAttribute(Qt::WA_DontShowOnScreen);accelerated.setAttribute(Qt::WA_DontShowOnScreen);
            software.resize(800,420);accelerated.resize(800,420);qputenv("FREESHIP_CHART_SOFTWARE","1");software.setState(display);software.show();settle();auto cpu=software.grab().toImage();software.hide();
            qunsetenv("FREESHIP_CHART_SOFTWARE");accelerated.setState(display);accelerated.show();settle();auto gpu=accelerated.grab().toImage();auto seen=accelerated.evidence();const auto compared=coverage(cpu,gpu);renderComparisons.append(QJsonObject{{"style",int(style)},{"dpr",accelerated.devicePixelRatioF()},{"coverage",compared},{"evidence",seen}});
            cpu.save(out+QString("/software-%1.png").arg(int(style)));gpu.save(out+QString("/gpu-%1.png").arg(int(style)));
            if(seen["render_history"].toArray().last().toObject()["gpu_layers"].toInt())check(QString("full-resolution-stroke-coverage-%1").arg(int(style)),compared["same_size"].toBool()&&compared["software"].toObject()["pixels"].toInt()>500&&compared["gpu"].toObject()["pixels"].toInt()>500&&compared["software"].toObject()["missing"].toInt()==0&&compared["gpu"].toObject()["missing"].toInt()==0);
            else cases.append(QJsonObject{{"name","gpu-coverage-unavailable"},{"status","not_run"}});
            check(QString("dense-gaps-exact-%1").arg(int(style)),seen["points"].toInt()==11998&&seen["segments"].toInt()==11996);
            auto changed=std::make_shared<CalculationChartState>();changed->data=CalculationChartData::read("",gaps);changed->visible={true};accelerated.setState(changed);settle();accelerated.grab();check("replacing-data-invalidates-layers",accelerated.evidence()["points"].toInt()==4);
        }
    }
    check("original-result-unmodified",QJsonDocument(result).toJson(QJsonDocument::Compact)==original);
    bool ok=true;for(auto c:cases)ok &= c.toObject()["status"]!="fail";
    QJsonObject report{{"status",ok?"pass":"fail"},{"cases",cases},{"observations",observations},{"render_comparisons",renderComparisons},{"numeric_sha256",data->numericFingerprint()}};
    QFile f(out+"/report.json");if(!f.open(QIODevice::WriteOnly))return 3;f.write(QJsonDocument(report).toJson());return ok?0:1;
}
