// Independent mesh and actual widget-event checks. No desktop automation.
#include "render/grid_preview.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <iostream>
int main(int argc,char**argv){
    QApplication app(argc,argv);if(argc!=2)return 2;QString out=app.arguments()[1];QDir().mkpath(out);QJsonArray cases;auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"name",id},{"status",ok?"pass":"fail"}});};
    QJsonObject data{{"format","FreeShipOffsetPreview"},{"version",1},{"stations",5},{"waterlines",5},{"hulls",2},{"separation_m",10},{"length_m",8},{"draft_m",2}};QJsonArray table;
    for(int i=0;i<5;++i){QJsonArray row;for(int j=0;j<5;++j)row.append(i*j/8.);table.append(row);}data["half_breadths_m"]=table;
    auto mesh=fs::OffsetGrid::read(data);check("full-twin-counts",mesh.points.size()==100&&mesh.stations.size()==20&&mesh.waterlines.size()==20);
    check("independent-extents",mesh.minimum==fs::OffsetGrid::Point{0,-7,0}&&mesh.maximum==fs::OffsetGrid::Point{8,7,2});
    check("bow-first-row-and-upward-waterlines",mesh.points[0]==fs::OffsetGrid::Point{8,-5,0}&&mesh.points[24]==fs::OffsetGrid::Point{0,-3,2});
    bool symmetry=true;for(int i=0;i<25;++i){symmetry &= mesh.points[i][1]+mesh.points[25+i][1]==-10;for(int k:{0,2})symmetry &= mesh.points[i][k]==mesh.points[25+i][k];symmetry &= mesh.points[i][1]+mesh.points[75+i][1]==0;}
    check("mirrored-hulls-and-sides",symmetry);
    size_t segments=0;for(const auto& p:mesh.stations)segments+=p.size()-1;for(const auto& p:mesh.waterlines)segments+=p.size()-1;check("all-grid-segments",segments==160);
    std::vector<QJsonObject> invalid;auto add=[&](QString key,QJsonValue value){auto d=data;d[key]=value;invalid.push_back(d);};add("stations",6);add("stations",1000000);add("waterlines",5.5);add("hulls",3);add("separation_m",0);add("length_m",0);add("draft_m","2");add("half_breadths_m",QJsonArray{});
    auto badRows=table;auto row=badRows[0].toArray();row[0]=-1;badRows[0]=row;add("half_breadths_m",badRows);
    GridPreviewView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(800,500);view.show();view.setGrid(data);app.processEvents();auto before=view.evidence();auto image=view.grab().toImage();image.save(out+"/grid.png");
    for(size_t i=0;i<invalid.size();++i){bool rejected=false;try{view.setGrid(invalid[i]);}catch(const std::exception&){rejected=true;}check("reject-"+QString::number(i),rejected&&view.evidence()["grid_sha256"]==before["grid_sha256"]);}
    auto mouse=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,p,view.mapToGlobal(p.toPoint()),button,buttons,Qt::NoModifier);QApplication::sendEvent(&view,&event);};
    mouse(QEvent::MouseButtonPress,{200,200},Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseMove,{260,225},Qt::NoButton,Qt::LeftButton);mouse(QEvent::MouseButtonRelease,{260,225},Qt::LeftButton,Qt::NoButton);
    auto rotated=view.grab().toImage();rotated.save(out+"/rotated.png");check("rotate-changes-camera-and-image",view.evidence()["yaw"]!=before["yaw"]&&rotated!=image);
    auto panBefore=view.evidence()["pan"].toArray();mouse(QEvent::MouseButtonPress,{300,250},Qt::RightButton,Qt::RightButton);mouse(QEvent::MouseMove,{317,241},Qt::NoButton,Qt::RightButton);mouse(QEvent::MouseButtonRelease,{317,241},Qt::RightButton,Qt::NoButton);
    auto pan=view.evidence()["pan"].toArray();check("pan-follows-pixels",pan[0].toDouble()-panBefore[0].toDouble()==17&&pan[1].toDouble()-panBefore[1].toDouble()==-9);
    QWheelEvent wheel({400,250},view.mapToGlobal(QPoint(400,250)),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&view,&wheel);check("wheel-zoom",view.evidence()["zoom"].toDouble()>1);
    for(int i=0;i<4;++i){view.setProjection(i);auto picture=view.grab();check("projection-"+QString::number(i),!picture.isNull());picture.save(out+"/projection-"+QString::number(i)+".png");}
    check("camera-keeps-exact-grid",view.evidence()["grid_sha256"]==before["grid_sha256"]&&view.evidence()["grid_revision"]==before["grid_revision"]);
    view.fit();check("fit-clears-pan-and-zoom",view.evidence()["zoom"]==1&&view.evidence()["pan"].toArray()==QJsonArray{0,0});
    // All 26,244 vertices retained at the largest supported two-hull sampling.
    auto dense=data;dense["stations"]=81;dense["waterlines"]=81;QJsonArray rows;for(int i=0;i<81;++i){QJsonArray row;for(int j=0;j<81;++j)row.append(2.*std::sin(std::acos(-1.)*i/80)*j/80);rows.append(row);}dense["half_breadths_m"]=rows;view.setGrid(dense);std::vector<double> timings;
    for(int i=0;i<8;++i){QElapsedTimer timer;timer.start();view.grab();timings.push_back(timer.nsecsElapsed()/1e6);}std::sort(timings.begin(),timings.end());
    check("maximum-grid-retained",view.evidence()["points"]==26244);
    bool passed=true;for(auto c:cases)passed &= c.toObject()["status"]=="pass";
    QJsonObject report{{"status",passed?"pass":"fail"},{"cases",cases},{"view",view.evidence()},{"hidden_grab_median_ms",timings[timings.size()/2]},{"timing_scope","Hidden widget render/readback, not desktop frame rate"}};QFile f(out+"/report.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(report).toJson());std::cout<<QJsonDocument(report).toJson(QJsonDocument::Compact).constData();return passed?0:1;
}
