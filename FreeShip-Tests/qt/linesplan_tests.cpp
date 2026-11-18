// Behavioral contract for the pilot. No OS mouse movement or desktop capture.
#include "view.h"
#include "software_scene_cases.h"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

static QByteArray bytes(const QString&p){QFile f(p);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read artifact");return f.readAll();}
static QJsonObject stats(QVector<double> v){std::sort(v.begin(),v.end());return {{"p50_ms",v[v.size()/2]},{"p95_ms",v[qMin(v.size()-1,qsizetype(v.size()*.95))]},{"max_ms",v.last()},{"samples",v.size()}};}
int main(int argc,char**argv){
    QApplication app(argc,argv);if(argc!=3)return 3;QString input=app.arguments()[1],out=app.arguments()[2];QDir().mkpath(out);
    QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try{
        softwareSceneCases(check);
        auto scene=std::make_shared<Scene>(Scene::load(input));report["scene_sha256"]=QString::fromLatin1(scene->digest);
        report["load_ms"]=scene->loadMs;report["compile_ms"]=scene->compileMs;
        report["triangles"]=scene->triangles;report["line_vertices"]=scene->lineVertices;report["labels"]=scene->labels.size();report["batches"]=scene->batches.size();
        scene->save(out+"/roundtrip.fslp");check("double-coordinate-and-text-roundtrip",bytes(input)==bytes(out+"/roundtrip.fslp"));
        check("nonempty-fill-lines-labels",scene->triangles>0&&scene->lineVertices>0&&!scene->labels.isEmpty());
        LinesView view;view.resize(1280,800);view.setScene(scene);report["gpu_device"]=view.gpuDevice();report["gpu_initialization_ms"]=view.gpuInitializationMs();
        QElapsedTimer timer;timer.start();auto base=view.image();report["first_render_ms"]=timer.nsecsElapsed()/1e6;
        base.save(out+"/qt-fit.png");check("fit-culling-equals-full-scene",base==view.image(false,false));
        auto labelRects=view.visibleLabelRects();bool overlaps=false;
        for(int i=0;i<labelRects.size();++i)for(int j=0;j<i;++j)overlaps|=labelRects[i].intersects(labelRects[j]);
        check("visible-labels-do-not-overlap",!overlaps&&!labelRects.isEmpty());report["visible_labels_fit"]=labelRects.size();
        // Pick an actual filled pixel, so a blank high-zoom view cannot pass vacuously.
        QPointF anchor;double best=1e100;
        for(int y=5;y<base.height()-5;++y)for(int x=5;x<base.width()-5;++x){auto c=base.pixelColor(x,y);
            if(c.saturation()<100||base.pixel(x-3,y)!=base.pixel(x,y)||base.pixel(x+3,y)!=base.pixel(x,y))continue;
            double distance=QLineF(QPointF(x,y),QPointF(base.width()/2,base.height()/2)).length();
            if(distance<best){best=distance;anchor=QPointF(x,y);}
        }
        check("zoom-anchor-has-filled-content",best<1e99);
        auto original=view.camera();auto world=original.inverted().map(anchor);
        view.zoomAt(anchor,30);check("zoom-anchor",QLineF(view.camera().map(world),anchor).length()<1e-8);
        view.image().save(out+"/qt-zoom.png");check("zoom-culling-equals-full-scene",view.image()==view.image(false,false));
        view.panBy(QPointF(-20,15));view.image().save(out+"/qt-pan.png");check("pan-culling-equals-full-scene",view.image()==view.image(false,false));
        view.panBy(QPointF(20,-15));view.zoomAt(anchor,1.0/30);check("zoom-pan-return-no-lost-fill",base==view.image());
        view.fit();auto before=view.offset();
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(100,100),QPointF(100,100),Qt::RightButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(&view,&press);
        QMouseEvent move(QEvent::MouseMove,QPointF(137,121),QPointF(137,121),Qt::NoButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(&view,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(137,121),QPointF(137,121),Qt::RightButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&view,&release);
        check("right-button-event-pan",QLineF(view.offset()-before,QPointF(37,21)).length()<1e-9);
        double scale=view.scale();QWheelEvent wheel(anchor,anchor,QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&view,&wheel);
        check("wheel-event-zoom",std::abs(view.scale()/scale-1.2)<1e-12);
        view.fit();view.setFill(false);check("fill-toggle",view.image()!=base);view.setFill(true);check("fill-toggle-restores",view.image()==base);
        view.image(false,true,true,1.5).save(out+"/qt-dpi150.png");
        check("dpi150-full-scene",view.image(false,true,true,1.5)==view.image(false,false,true,1.5));
        // Independent primitive renderer verifies batching away from rasterized edges.
        auto raw=view.image(true,false,false),batched=view.image(false,false,false);raw.save(out+"/raw-primitives.png");batched.save(out+"/batched.png");
        qint64 different=0,interior=0;
        for(int y=1;y<raw.height()-1;++y)for(int x=1;x<raw.width()-1;++x){auto c=raw.pixel(x,y);if(c==batched.pixel(x,y))continue;++different;
            bool solid=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)solid&=raw.pixel(x+dx,y+dy)==c;
            if(solid)++interior;
        }
        report["batch_edge_different_pixels"]=different;report["batch_interior_different_pixels"]=interior;check("batch-preserves-solid-interiors",interior==0);
        QVector<double> pan,wheelTimes;for(int i=0;i<120;++i){timer.restart();view.panBy(QPointF(i%2? -3.25:3.25,i%2?-1.5:1.5));auto im=view.image();pan.append(timer.nsecsElapsed()/1e6);}
        for(int i=0;i<120;++i){timer.restart();view.zoomAt(anchor,i%2?1/1.02:1.02);auto im=view.image();wheelTimes.append(timer.nsecsElapsed()/1e6);}
        report["pan_render_with_gpu_readback"]=stats(pan);report["zoom_render_with_gpu_readback"]=stats(wheelTimes);
        view.fit();view.setGpuEnabled(false);QVector<double> fallback;
        for(int i=0;i<30;++i){timer.restart();view.panBy(QPointF(i%2?-3.25:3.25,0));auto im=view.image();fallback.append(timer.nsecsElapsed()/1e6);}
        report["cpu_fallback_render"]=stats(fallback);view.setGpuEnabled(true);
        auto screen=[&](bool labels){QImage im(view.size(),QImage::Format_ARGB32_Premultiplied);QPainter painter(&im);view.renderScene(painter,false,true,labels,true);return im;};
        view.setGpuEnabled(false);qint64 screenInterior=0,screenMismatch=0;
        for(int i=0;i<4;++i){view.fit();view.zoomAt(anchor,i==0?.3:i==1?1:i==2?6:30);view.panBy({23,-11});
            const auto expected=view.image(false,true,false),actual=screen(false);
            for(int y=1;y+1<expected.height();++y)for(int x=1;x+1<expected.width();++x){const auto color=expected.pixel(x,y);bool interior=true;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)interior&=expected.pixel(x+dx,y+dy)==color;
                if(interior){++screenInterior;if(color!=actual.pixel(x,y))++screenMismatch;}
            }
        }
        report["screen_fill_interior_pixels"]=screenInterior;report["screen_fill_mismatches"]=screenMismatch;
        report["scene_compile_ms"]=scene->compileMs;check("software-screen-contours-preserve-fill",screenInterior>0&&screenMismatch==0);
        for(bool gpu:{false,true}){view.setGpuEnabled(gpu);view.fit();screen(true);QVector<double> times;
            for(int i=0;i<30;++i){timer.restart();view.panBy({i%2?-3.:3.,0});view.zoomAt(anchor,i%2?1/1.02:1.02);screen(true);times.append(timer.nsecsElapsed()/1e6);}
            report[gpu?"gpu_screen_navigation":"cpu_screen_navigation"]=stats(times);
        }
        check("interaction-scene-immutable",scene->digest==Scene::load(input).digest&&bytes(input)==bytes(out+"/roundtrip.fslp"));
        QJsonObject phases;QSet<quint32> colors;int runs=0;quint32 lastColor=0;
        for(const auto& p:scene->primitives)if(p.kind==1){colors.insert(p.color);if(!runs||lastColor!=p.color)++runs;lastColor=p.color;}
        report["fill_color_count"]=colors.size();report["fill_color_runs"]=runs;
        for(int kind:{1,2,3}){
            auto isolated=std::make_shared<Scene>();isolated->bounds=scene->bounds;
            for(const auto& p:scene->primitives)if(p.kind==quint32(kind))isolated->primitives.append(p);
            isolated->compile();LinesView phase;phase.resize(view.size());phase.setGpuEnabled(false);phase.setScene(isolated);QImage im(view.size(),QImage::Format_ARGB32_Premultiplied);
            check("disabled-gpu-no-context-"+QString::number(kind),phase.gpuDevice()=="CPU fallback");
            auto draw=[&]{QPainter painter(&im);phase.renderScreen(painter);};timer.restart();draw();const double cold=timer.nsecsElapsed()/1e6;QVector<double> times;
            for(int i=0;i<16;++i){timer.restart();phase.panBy({i%2?-3.:3.,0});phase.zoomAt(anchor,i%2?1/1.02:1.02);draw();times.append(timer.nsecsElapsed()/1e6);}
            auto values=stats(times);values["cold_ms"]=cold;values["primitives"]=isolated->primitives.size();phases[QString::number(kind)]=values;
        }
        report["cpu_phases"]=phases;
        auto data=bytes(input);for(int mode=0;mode<3;++mode){QByteArray bad=data;
            if(mode==0)bad.truncate(55);if(mode==1)bad[0]='X';if(mode==2){for(int i=48;i<52;++i)bad[i]=char(0xff);}
            QString path=out+QString("/invalid-%1.fslp").arg(mode);QFile f(path);f.open(QIODevice::WriteOnly);f.write(bad);f.close();bool rejected=false;
            try{Scene::load(path);}catch(const std::exception&){rejected=true;}check(QString("reject-invalid-%1").arg(mode),rejected);
        }
        cases.append(QJsonObject{{"id","native-desktop-input-and-presentation"},{"status","not_run"}});
        cases.append(QJsonObject{{"id","typography-and-label-layout"},{"status","review"}});
        cases.append(QJsonObject{{"id","60hz-target"},{"status", "review"},{"cpu_p95_under_16_7",stats(pan)["p95_ms"].toDouble()<16.7&&stats(wheelTimes)["p95_ms"].toDouble()<16.7}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception&e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;report["scope"]="Offscreen rendering (including GPU readback when available) and synthetic Qt events; not desktop frame rate. No geometry algorithm migration.";
    QFile f(out+"/report.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(report).toJson());return report["status"]=="partial"?2:1;
}
