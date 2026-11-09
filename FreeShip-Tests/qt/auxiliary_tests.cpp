// Curves/flowlines: analytic geometry expectations and real HullView events.
// Hidden widgets only; no desktop automation or pixel-style acceptance.
#include "render/hull_view.h"
#include "render/auxiliary_geometry.h"
#include "native/geometry.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <cmath>
#include <algorithm>
using namespace fs::presentation;
namespace {
void mouse(HullView& v,QEvent::Type type,QPointF p,Qt::MouseButton b,Qt::MouseButtons held){QMouseEvent e(type,p,p,b,held,Qt::NoModifier);QApplication::sendEvent(&v,&e);}
double error(fs::Vec3 a,fs::Vec3 b){return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)});}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 3;
    const auto out=args[2];QDir().mkpath(out);QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try{
        QVector<fs::SectionSpline> samples{
            {{{0,0,0},{1,2,3},{2,-1,4},{3,2,1}},{false,false,false,false}},
            {{{0,0,0},{1,2,3},{2,-1,4},{3,2,1}},{true,true,true,true}},
            {{{0,0,0},{0,0,0},{1,2,3},{2,-1,4}},{false,false,false,false}},
            {{{1,1,1},{1,1,1},{1,1,1}},{false,false,false}}
        };
        double maxError=0;
        for(int n=0;n<samples.size();++n){
            const auto& s=samples[n];std::vector<fs::native_geometry::Vec3> points;std::vector<bool> flags;
            for(auto p:s.points)points.push_back({float(p.x),float(p.y),float(p.z)});for(bool k:s.knuckles)flags.push_back(k);
            fs::native_geometry::Spline reference(points,flags);auto curves=splineCubics(s);auto t=reference.parameters();int index=0;double local=0;
            for(size_t i=1;i<t.size();++i){if(std::abs(t[i]-t[i-1])<1e-6)continue;
                for(int j=0;j<=100;++j){double u=j/100.;auto expected=reference.value(double(t[i-1])+(double(t[i])-t[i-1])*u);auto actual=cubicValue(curves[index],u);local=std::max(local,error(actual,{expected.x,expected.y,expected.z}));}++index;
            }
            maxError=std::max(maxError,local);check("cubic-matches-single-spline-"+QString::number(n),local<8e-6);check("retains-every-nonzero-span-"+QString::number(n),index==curves.size());
        }
        report["maximum_single_rounding_difference"]=maxError;
        Cubic triple{{{-.08,1,0},{.14,1,1},{-.14,1,2},{.08,1,3}}};auto split=splitBodyCubics({triple},0);
        check("three-crossings-produce-four-pieces",split.size()==4);
        bool mirror=split.size()==4;for(int i=0;i<split.size();++i)for(auto p:split[i])mirror&=p.y==(i%2?1:-1);
        check("no-body-center-connector",mirror);
        auto path=projectCubics({triple},[](fs::Vec3 p){return QPointF(p.x,p.z);},false);
        check("orthographic-keeps-exact-cubic",path.elementCount()==4&&path.elementAt(1).isCurveTo());
        auto perspective=[](fs::Vec3 p){return QPointF(400*p.y/(5+p.x),400*p.z/(5+p.x));};
        auto projected=projectCubics({triple},perspective,true);double pixelError=0;
        for(int i=0;i<=1000;++i){auto p=perspective(cubicValue(triple,i/1000.));double nearest=1e30;
            for(int k=1;k<projected.elementCount();++k){auto e=projected.elementAt(k-1),f=projected.elementAt(k);QPointF a(e.x,e.y),b(f.x,f.y),v=b-a;double len=QPointF::dotProduct(v,v),t=len?std::clamp(QPointF::dotProduct(p-a,v)/len,0.,1.):0.;nearest=std::min(nearest,QLineF(p,a+t*v).length());}pixelError=std::max(pixelError,nearest);
        }
        report["perspective_pixel_error"]=pixelError;check("perspective-subpixel-projection",pixelError<=.2);
        auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="auxiliary-fixture";m->revision=1;m->length=2;m->beam=6;m->draft=2;m->units="metric";m->bothSides=false;m->useDefaultSplitSectionLocation=false;m->splitSectionLocation=0;
        m->minimum={-1,0,0};m->maximum={1,3,2};m->points={{1,{-1,1,0}},{2,{1,3,2}}};m->edges={{1,1,2,false}};
        fs::AuxiliaryCurve curve;curve.id=7;curve.color=0xff0000;curve.controls={1,2};curve.spline={{{-1,1,0},{1,3,2}},{true,true}};m->controlCurves={curve};
        auto flow=curve;flow.id=9;flow.sourceView=1;flow.source={-.5,.5};flow.spline={{{-.5,1,.5},{.5,2,.5}},{true,true}};m->flowlines={flow};
        HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.setAnimationsEnabled(false);view.resize(800,520);view.setProjection(HullProjection::Body);view.setSnapshot(m);view.setFillVisible(false);view.setControlNetVisible(false);view.fit();view.setSelectionKind(HullEntity::Curve);
        check("curve-hit-on-both-body-pieces",view.entityAt(view.project({-.5,-1.5,.5}))==7&&view.entityAt(view.project({.5,2.5,1.5}))==7);
        check("curve-does-not-hit-center-gap",view.entityAt(view.project({0,0,1}))==0);
        view.setSelectedEntities({7,99});check("curve-selection-filters-unknown-ids",view.selectedEntities()==QSet<fs::EntityId>{7});
        view.image().save(out+"/half-curves.png");int builds=view.overlayEvidence()["auxiliary_path_builds"].toInt();view.image();check("repaint-reuses-projected-paths",view.overlayEvidence()["auxiliary_path_builds"].toInt()==builds);
        view.setProjection(HullProjection::Profile);view.fit();view.setSelectionKind(HullEntity::Flowline);check("flow-hit-is-independent-from-control-net",view.entityAt(view.project({0,1.5,.5}))==9);
        view.setFillVisible(true);check("flow-hidden-in-shaded-view",view.entityAt(view.project({0,1.5,.5}))==0);view.setFillVisible(false);
        auto hidden=std::make_shared<fs::ModelSnapshot>(*m);hidden->revision=2;hidden->showFlowlines=false;hidden->showControlCurves=false;view.setSnapshot(hidden);check("hidden-flow-cannot-be-selected",view.entityAt(view.project({0,1.5,.5}))==0);
        view.setSelectionKind(HullEntity::Curve);check("hidden-curve-cannot-be-selected",view.entityAt(view.project({0,2,1}))==0);view.setSnapshot(m);
        int requests=0,ended=0,sourceView=-1;QPointF source;view.onFlowRequested=[&](int v,QPointF p){++requests;sourceView=v;source=p;};view.onFlowPlacementEnded=[&]{++ended;};
        for(auto projection:{HullProjection::Profile,HullProjection::Plan,HullProjection::Body}){
            view.setProjection(projection);view.fit();fs::Vec3 p{.3,1.2,.7};QPointF expected=projection==HullProjection::Profile?QPointF(p.x,p.z):projection==HullProjection::Plan?QPointF(p.x,p.y):QPointF(p.y,p.z);
            int count=requests;view.setFlowPlacement(true);auto pixel=view.project(p);mouse(view,QEvent::MouseButtonPress,pixel,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,pixel,Qt::LeftButton,Qt::NoButton);
            check("flow-click-projection-"+QString::number(int(projection)),requests==count+1&&sourceView==view.backgroundView()&&QLineF(source,expected).length()<1e-10&&view.flowPlacement());
            // Original DoAddFlowLine leaves the tool active for the next source.
            mouse(view,QEvent::MouseButtonPress,pixel,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,pixel,Qt::LeftButton,Qt::NoButton);
            check("flow-second-click-without-reactivation-"+QString::number(int(projection)),requests==count+2&&view.flowPlacement());
        }
        auto both=std::make_shared<fs::ModelSnapshot>(*m);both->revision=3;both->bothSides=true;view.setSnapshot(both);view.setProjection(HullProjection::Body);view.fit();
        check("both-body-flow-source-reflects-negative-half",std::abs(view.flowSource(view.project({0,-1,.7})).x()-1)<1e-10);
        int count=requests;view.setFlowPlacement(true);mouse(view,QEvent::MouseButtonPress,{200,200},Qt::RightButton,Qt::RightButton);mouse(view,QEvent::MouseButtonRelease,{200,200},Qt::RightButton,Qt::NoButton);
        check("right-click-cancels-without-edit",!view.flowPlacement()&&requests==count);view.setFlowPlacement(true);QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&view,&escape);check("escape-cancels-without-edit",!view.flowPlacement()&&requests==count);
        view.setProjection(HullProjection::Perspective);view.setFlowPlacement(true);check("perspective-does-not-accept-flow-source",!view.flowPlacement());
        view.setProjection(HullProjection::Profile);view.fit();QElapsedTimer timer;timer.start();for(int i=0;i<30;++i){view.panBy({i%2?1.:-1.,0});view.zoomAt({400,260},i%2?1/1.01:1.01);view.image();}report["navigation_mean_ms"]=timer.nsecsElapsed()/1e6/30;check("navigation-keeps-immutable-document",view.snapshot()==both&&both->revision==3&&requests==count);
        QFile input(args[1]);if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Missing snapshot fixture");auto root=QJsonDocument::fromJson(input.readAll()).object();
        QJsonObject item{{"id",1},{"name","flow"},{"color",255},{"view",1},{"source",QJsonArray{0,0}},{"spline",QJsonObject{{"points",QJsonArray{QJsonArray{0,0,0},QJsonArray{1,1,1}}},{"knuckles",QJsonArray{false,false}}}}};
        auto load=[&]{QFile f(out+"/snapshot.json");if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot save snapshot");f.write(QJsonDocument(root).toJson());f.close();return fs::ModelSnapshot::load(f.fileName());};
        auto original=load();QJsonArray bindings;
        for(auto value:root["control_curves"].toArray()){auto c=value.toObject();bindings.append(QJsonObject{{"id",c["id"]},{"spline",c["spline"]}});}
        QJsonObject surface{{"format","FreeShipSurfaceDisplay"},{"version",1},{"triangles",root["triangles"]},
            {"surface_normals",root["surface_normals"]},{"control_curves",bindings}};
        original.verifySurfaceDisplay(QJsonDocument(surface).toJson());check("surface-artifact-matches-snapshot",true);
        for(const char* field:{"triangles","surface_normals","control_curves"}){
            auto altered=surface;auto values=altered[field].toArray();values.append(QJsonObject{{"tampered",true}});altered[field]=values;
            bool rejected=false;try{original.verifySurfaceDisplay(QJsonDocument(altered).toJson());}catch(const std::exception&){rejected=true;}
            check("surface-rejects-mismatched-"+QString(field),rejected);
        }
        bool malformed=false;try{original.verifySurfaceDisplay("{}");}catch(const std::exception&){malformed=true;}check("surface-rejects-missing-format",malformed);
        root["flowlines"]=QJsonArray{item};check("valid-auxiliary-snapshot-loads",load().flowlines.size()==1);
        auto reject=[&](QString id,QJsonObject bad){root["flowlines"]=QJsonArray{bad};bool rejected=false;try{load();}catch(const std::exception&){rejected=true;}check(id,rejected);};
        auto bad=item;bad["view"]=3;reject("rejects-perspective-flow-source",bad);bad=item;bad["spline"]=QJsonObject{{"points",QJsonArray{QJsonArray{0,0,0}}},{"knuckles",QJsonArray{}}};reject("rejects-inconsistent-spline-flags",bad);
        cases.append(QJsonObject{{"id","curves-desktop-parity"},{"status","review"},{"message","Hidden view events and images; desktop acceptance remains separate"}});report["status"]=failed?"fail":"partial";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;QFile f(out+"/report.json");if(!f.open(QIODevice::WriteOnly))return 3;f.write(QJsonDocument(report).toJson());return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
