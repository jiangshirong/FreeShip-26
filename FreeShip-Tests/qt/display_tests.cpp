// Independent body-plane expectations, real view events and original-core
// curvature comparison input/output. No desktop automation.
#include "render/hull_view.h"
#include "render/body_projection.h"
#include "render/curvature_comb.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <cmath>
#include <stdexcept>
using namespace fs::presentation;
namespace {
void mouse(HullView& v,QEvent::Type type,QPointF p,Qt::MouseButton b,Qt::MouseButtons held){QMouseEvent e(type,p,p,b,held,Qt::NoModifier);QApplication::sendEvent(&v,&e);}
QJsonArray xyz(fs::Vec3 p){return {p.x,p.y,p.z};}
void write(const QString& path,const QJsonDocument& json){QFile f(path);if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot save test evidence");f.write(json.toJson());}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 3;
    const auto out=args[2];QDir().mkpath(out);QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](QString name,bool pass){cases.append(QJsonObject{{"id",name},{"status",pass?"pass":"fail"}});failed|=!pass;};
    try{
        QFile requestFile(args[1]);if(!requestFile.open(QIODevice::ReadOnly))throw std::runtime_error("Missing curvature request");
        auto requests=QJsonDocument::fromJson(requestFile.readAll()).array();QJsonArray observations;
        for(auto request:requests){
            auto r=request.toObject();fs::SectionSpline s;for(auto value:r["points"].toArray()){auto p=value.toArray();s.points.append({p[0].toDouble(),p[1].toDouble(),p[2].toDouble()});}
            for(auto flag:r["knuckles"].toArray())s.knuckles.append(flag.toBool());
            auto samples=curvatureComb(s,r["precision"].toInt());QJsonArray rows;
            for(const auto& p:samples)rows.append(QJsonObject{{"base",xyz(p.base)},{"tip",xyz(combTip(p,float(r["scale"].toDouble())))},{"rib",p.rib}});
            observations.append(QJsonObject{{"name",r["name"]},{"samples",rows}});
        }
        write(out+"/curvature.json",QJsonDocument(observations));
        check("aft-point-mirrored",bodyPoint({-1,2,3},0).y==-2);check("fore-point-unmodified",bodyPoint({1,2,3},0).y==2);
        auto spans=bodySegments({-1,1,0},{1,3,4},0);
        check("seam-splits-with-interpolated-height",spans.size()==2&&spans[0][1].y==-2&&spans[1][0].y==2&&spans[0][1].z==2&&spans[1][0].z==2);
        check("seam-endpoint-stays-on-fore-segment",bodySegments({0,2,2},{1,3,4},0)[0][0].y==2);
        auto polygons=bodyPolygons({{-1,1,0},{1,1,0},{1,2,2},{-1,2,2}},0);
        check("polygon-clipped-into-two-disconnected-halves",polygons.size()==2&&polygons[0].size()==4&&polygons[1].size()==4);
        bool sides=true;for(auto p:polygons[0])sides&=p.y<=-1;for(auto p:polygons[1])sides&=p.y>=1;check("polygon-does-not-bridge-center",sides);
        auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="display-fixture";m->revision=1;m->length=2;m->beam=4;m->draft=2;m->units="metric";m->bothSides=false;m->useDefaultSplitSectionLocation=false;m->splitSectionLocation=0;
        m->minimum={-1,0,0};m->maximum={1,2,2};m->points={{1,{-1,1,0}},{2,{1,2,2}},{3,{1,1,.5}}};m->edges={{1,1,2,false}};
        auto aft=sectionSides(*m,2,1,{{-1,1,0},{-1,2,1}}),fore=sectionSides(*m,2,1,{{1,1,0},{1,2,1}}),middle=sectionSides(*m,2,1,{{0,1,0},{0,2,1}});
        check("station-front-back-and-midship-rules",!aft.first&&aft.second&&fore.first&&!fore.second&&middle.first&&middle.second);
        HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.setAnimationsEnabled(false);view.resize(800,520);view.setProjection(HullProjection::Body);view.setSnapshot(m);view.setFillVisible(false);view.fit();
        auto at=view.projectControlPoint(1);check("control-projection-uses-visible-side",QLineF(at,view.project({-1,-1,0})).length()<1e-9);
        check("body-point-hit-matches-display",view.entityAt(at)==1);view.setSelectionKind(HullEntity::Edge);
        check("crossing-edge-hit-on-each-side",view.entityAt(view.project({-.5,-1.25,.5}))==1&&view.entityAt(view.project({.5,1.75,1.5}))==1);
        check("crossing-edge-does-not-hit-false-center-connector",view.entityAt(view.project({0,0,1}))==0);
        view.setSelectionKind(HullEntity::Point);view.setSelectedPoints({1,2});QVector<fs::PointMove> moved;
        view.onPointsMoved=[&](auto p){moved=p;};auto end=at+QPointF(24,-12);
        mouse(view,QEvent::MouseButtonPress,at,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
        check("aft-body-drag-flips-world-y-only",moved.size()==2&&moved[0].position.y<1&&moved[0].position.z>0&&moved[0].position.x==-1);
        if(moved.size()==2)check("mixed-half-selection-keeps-rigid-world-delta",std::abs((moved[0].position.y-1)-(moved[1].position.y-2))<1e-12);
        view.setSelectedPoints({});auto first=at-QPointF(12,12),last=at+QPointF(12,12);
        mouse(view,QEvent::MouseButtonPress,first,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseMove,last,Qt::NoButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,last,Qt::LeftButton,Qt::NoButton);
        check("box-selects-displayed-half-body-point",view.selectedPoints()==QSet<fs::EntityId>{1});
        auto painted=std::make_shared<fs::ModelSnapshot>(*m);painted->revision=10;painted->shadeUnderwater=false;
        painted->points={{1,{-1,1,0}},{2,{-1,3,0}},{3,{-1,2,2}}};painted->edges.clear();painted->layers={{1,"green",0x008000}};
        painted->faces={{1,1,{1,2,3}}};painted->triangles={{1,{fs::Vec3{-1,1,0},fs::Vec3{-1,3,0},fs::Vec3{-1,2,2}}}};
        view.setSnapshot(painted);view.setControlNetVisible(false);view.setFillVisible(true);view.fit();view.zoomAt({400,260},.35);auto halfImage=view.image();
        check("half-body-fill-is-on-aft-side",halfImage.pixelColor(view.project({-1,-2,.5}).toPoint())==QColor(0,128,0));
        check("half-body-fill-does-not-duplicate-fore-side",halfImage.pixelColor(view.project({-1,2,.5}).toPoint())!=QColor(0,128,0));
        view.setSelectionKind(HullEntity::Face);check("half-body-face-hit-matches-fill",view.entityAt(view.project({-1,-2,.5}))==1&&view.entityAt(view.project({-1,2,.5}))==0);
        halfImage.save(out+"/half-fill.png");view.setProjection(HullProjection::Plan);view.fit();view.setProjection(HullProjection::Body);view.fit();view.zoomAt({400,260},.35);
        check("projection-switch-rebuilds-correct-body-mesh",view.image().pixelColor(view.project({-1,-2,.5}).toPoint())==QColor(0,128,0));
        auto bothPainted=std::make_shared<fs::ModelSnapshot>(*painted);bothPainted->revision=11;bothPainted->bothSides=true;view.setSnapshot(bothPainted);view.fit();auto bothImage=view.image();
        check("both-body-fill-includes-both-halves",bothImage.pixelColor(view.project({-1,-2,.5}).toPoint())==QColor(0,128,0)&&bothImage.pixelColor(view.project({-1,2,.5}).toPoint())==QColor(0,128,0));
        bothImage.save(out+"/both-fill.png");view.setSelectionKind(HullEntity::Point);view.setFillVisible(false);
        fs::Section section;section.kind=1;section.index=0;section.showCurvature=true;section.color=0x003300;section.paths={{{1,0,0},{1,1,1},{1,2,0}}};section.splines={{{{1,0,0},{1,1,1},{1,2,0}},{false,false,false}}};
        auto combModel=std::make_shared<fs::ModelSnapshot>(*m);combModel->revision=2;combModel->sections={section};combModel->showCurvature=false;
        view.setSnapshot(combModel);view.setControlNetVisible(false);view.image();check("hidden-curvature-is-lazy",view.overlayEvidence()["curvature_builds"].toInt()==0);
        auto visible=std::make_shared<fs::ModelSnapshot>(*combModel);visible->showCurvature=true;visible->revision=3;view.setSnapshot(visible);view.image().save(out+"/body-curvature.png");
        check("curvature-built-on-demand-once",view.overlayEvidence()["curvature_builds"].toInt()==1&&view.overlayEvidence()["curvature_errors"].toInt()==0);
        QElapsedTimer timer;timer.start();for(int i=0;i<20;++i){view.panBy({i%2?1.:-1.,0});view.zoomAt({400,260},i%2?1/1.01:1.01);view.image();}
        report["navigation_mean_ms"]=timer.nsecsElapsed()/1e6/20;check("navigation-reuses-curvature",view.overlayEvidence()["curvature_builds"].toInt()==1);
        auto full=std::make_shared<fs::ModelSnapshot>(*visible);full->revision=4;full->bothSides=true;view.setSnapshot(full);view.fit();
        check("both-sides-restores-source-point-position",view.projectControlPoint(1)==view.project({-1,1,0}));view.image().save(out+"/both-curvature.png");
        cases.append(QJsonObject{{"id","display-desktop-parity"},{"status","review"},{"message","Hidden object and input events, not desktop acceptance"}});report["status"]=failed?"fail":"partial";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;write(out+"/report.json",QJsonDocument(report));return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
