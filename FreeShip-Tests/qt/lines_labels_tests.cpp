// SPDX-License-Identifier: GPL-2.0-or-later
#include "view.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <algorithm>

int main(int argc,char**argv){
    QApplication app(argc,argv);if(argc!=2&&argc!=3)return 3;
    const QString out=app.arguments().last();QDir().mkpath(out);QJsonArray cases;bool failed=false;
    auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    auto scene=std::make_shared<Scene>();scene->bounds={0,0,100,60};
    auto add=[&](QString text,QPointF at){Primitive p;p.kind=3;p.text=text;p.pixels=14;p.points={at};scene->labels.append(scene->primitives.size());scene->primitives.append(p);};
    add("DWL 10",{50,30});add("10",{50,30});add("20",{51,30});add("30",{52,30});add("BASE 0",{50,29});
    LinesView view;view.resize(640,400);view.setGpuEnabled(false);view.setScene(scene);
    auto screen=[&](){QImage im(view.size(),QImage::Format_ARGB32_Premultiplied);QPainter p(&im);view.renderScreen(p);return im;};
    auto disjoint=[&](){auto boxes=view.screenLabelRects();if(boxes.isEmpty())return false;for(int i=0;i<boxes.size();++i){if(!QRectF(view.rect()).contains(boxes[i]))return false;for(int j=0;j<i;++j)if(boxes[i].intersects(boxes[j]))return false;}return true;};
    const auto original=view.image();check("screen.crowded-labels-disjoint",disjoint());screen().save(out+"/crowded.png");
    for(int i=0;i<20;++i){view.panBy({i%2?-2.:2.,1.});screen();check("pan.frame-"+QString::number(i),disjoint());}
    view.fit();check("export.unchanged-by-screen-history",view.image()==original);
    for(int i=0;i<12;++i){view.zoomAt({320,200},i%2?1/1.1:1.1);screen();check("zoom.frame-"+QString::number(i),disjoint());}
    // Independent one-label cases distinguish edge force from plain clipping.
    auto single=std::make_shared<Scene>();single->bounds=scene->bounds;single->primitives={scene->primitives[0]};single->labels={0};
    QFont font("Arial");font.setPixelSize(14);QFontMetricsF fm(font);const QSizeF size(fm.horizontalAdvance("DWL 10"),fm.height());
    for(const QString edge:{"left","right","top","bottom"}){
        view.setScene(single);auto anchor=view.camera().map(QPointF(50,30));QPointF target(200,150);
        if(edge=="left")target.setX(15);if(edge=="right")target.setX(640-15-size.width());
        if(edge=="top")target.setY(15);if(edge=="bottom")target.setY(400-15-size.height());
        view.panBy(target-anchor);screen();
        for(int i=0;i<20;++i){QThread::msleep(16);screen();}
        auto boxes=view.screenLabelRects();bool moved=false;
        if(boxes.size()==1){auto p=boxes[0].topLeft();moved=edge=="left"?p.x()<target.x()-1:edge=="right"?p.x()>target.x()+1:edge=="top"?p.y()<target.y()-1:p.y()>target.y()+1;}
        check("edge."+edge+"-attraction",moved);screen().save(out+"/edge-"+edge+".png");
        view.panBy({-1000,0});screen();view.panBy({1000,0});screen();
        check("edge."+edge+"-returns",!view.screenLabelRects().isEmpty());
    }
    view.setScene(scene);auto before=view.image();view.resize(480,320);screen();check("resize.disjoint",disjoint());view.resize(640,400);view.fit();check("export.resize-roundtrip",view.image()==before);
    check("scene.labels-unmodified",scene->primitives[0].text=="DWL 10"&&scene->primitives[0].points[0]==QPointF(50,30));
    auto dense=std::make_shared<Scene>();dense->bounds={0,0,100,60};
    for(int y=0;y<12;++y)for(int x=0;x<30;++x){auto p=scene->primitives[1];p.text=QString::number(x);p.points={QPointF(x*3,y*5)};dense->labels.append(dense->primitives.size());dense->primitives.append(p);}
    view.setScene(dense);QVector<double> times;
    for(int i=0;i<30;++i){QElapsedTimer t;t.start();view.panBy({i%2?2.:-2.,0});screen();times.append(t.nsecsElapsed()/1e6);check("dense.frame-"+QString::number(i),disjoint());}
    std::sort(times.begin(),times.end());
    // Report measured cost; do not turn this machine's timing into a universal FPS claim.
    QJsonObject report{{"status",failed?"fail":"pass"},{"cases",cases},{"dense_labels",360},{"screen_p50_ms",times[15]},{"screen_p95_ms",times[28]}};
    if(argc==3){
        auto real=std::make_shared<Scene>(Scene::load(app.arguments()[1]));view.resize(1280,800);view.setScene(real);
        auto exported=view.image();screen().save(out+"/real-screen.png");
        QVector<double> elapsed;
        for(int i=0;i<30;++i){QElapsedTimer clock;clock.start();view.panBy({i%2?4.:-4.,0});screen();elapsed.append(clock.nsecsElapsed()/1e6);check("real.pan-"+QString::number(i),disjoint());}
        check("real.export-unchanged",view.image()==exported);
        std::sort(elapsed.begin(),elapsed.end());report["real_screen_p95_ms"]=elapsed[28];report["real_labels"]=real->labels.size();
        report["status"]=failed?"fail":"pass";report["cases"]=cases;
    }
    QFile f(out+"/report.json");if(!f.open(QIODevice::WriteOnly))return 3;f.write(QJsonDocument(report).toJson());return failed?1:0;
}
