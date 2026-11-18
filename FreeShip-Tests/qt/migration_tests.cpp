// Production model reader and four-view behavior. No desktop input is sent.
#include "core/model_snapshot.h"
#include "core/model_checks.h"
#include "render/hull_view.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFocusEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

static QByteArray read(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read model fixture");
    return f.readAll();
}
static void write(const QString& path, const QJsonObject& value) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(value).toJson()) < 0)
        throw std::runtime_error("Cannot write test evidence");
}
static void mouse(QWidget& view, QEvent::Type type, QPointF p,
                  Qt::MouseButton button, Qt::MouseButtons buttons,
                  Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    QMouseEvent event(type,p,p,button,buttons,modifiers);
    QApplication::sendEvent(&view,&event);
}
static void drag(QWidget& view, QPointF start, QPointF delta, Qt::MouseButton button) {
    mouse(view,QEvent::MouseButtonPress,start,button,button);
    mouse(view,QEvent::MouseMove,start+delta,Qt::NoButton,button);
    mouse(view,QEvent::MouseButtonRelease,start+delta,button,Qt::NoButton);
}
static bool near(double a,double b) {return std::abs(a-b)<1e-8;}
static bool near(QPointF a,QPointF b) {return QLineF(a,b).length()<1e-7;}
static bool equal(fs::Vec3 a,fs::Vec3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static QJsonObject statistics(QVector<double> times) {
    std::sort(times.begin(),times.end());
    return {{"samples",times.size()},{"p50_ms",times[times.size()/2]},
            {"p95_ms",times[qMin(times.size()-1,qsizetype(times.size()*.95))]},
            {"max_ms",times.last()}};
}
static std::shared_ptr<fs::ModelSnapshot> fixture(bool locked=false) {
    auto m=std::make_shared<fs::ModelSnapshot>();
    m->documentId="7228234c-aa81-4916-99d2-072eff3f0511";m->revision=1;
    m->name="Asymmetric tetrahedron";m->units="metric";
    m->minimum={0,0,0};m->maximum={6,7,7};
    m->length=6;m->beam=14;m->draft=3;m->waterlineZ=3;m->shadeUnderwater=false;
    m->points={{1,{2,3,4},locked},{2,{6,1,0},false},{3,{0,7,2},false},{4,{1,0,7},false}};
    m->edges={{1,1,2,true},{2,2,3,false},{3,3,4,false},{4,4,1,false}};
    m->faces={{1,1,{1,2,3,4}}};m->layers={{1,"Hull",0x0000ff,true,false}};
    m->triangles={{1,{{m->points[0].position,m->points[1].position,m->points[2].position}}},
                  {1,{{m->points[0].position,m->points[2].position,m->points[3].position}}}};
    return m;
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    if (argc!=3) return 3;
    const QString input=app.arguments()[1],out=app.arguments()[2];
    QDir().mkpath(out);
    QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](const QString& id,bool ok) {
        cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;
    };
    try {
        const auto markers=fs::parseMarkerText("FreeShip markers\n0 0 0\n1 2 3\n\n4 5 6\n7 8 9\nEOF\n","metric");
        check("marker-text-two-paths",markers.size()==2&&markers[0].toArray().size()==2&&markers[1].toArray()[1].toArray()==QJsonArray{7,8,9});
        for(const QByteArray bad:{QByteArray("Header\n0 0 0\nwrong row\n"),QByteArray("Header\n0 0 0\nEOF\n"),QByteArray("Header\n0 0 0\n1e999 2 3\n"),QByteArray("Header\n0 0 0\n1 2 3\nEOF\n4 5 6")}){
            bool rejected=false;try{fs::parseMarkerText(bad,"metric");}catch(const std::exception&){rejected=true;}check("marker-text-invalid-atomic-"+QString::number(bad.size()),rejected);
        }
        fs::ModelSnapshot diagnosis;
        diagnosis.points={{1,{0,0,0}},{2,{1,0,0}},{3,{0,1,0}},{4,{0,-1,0}},{5,{0,0,1}}};
        diagnosis.edges={{9,1,2,false}};diagnosis.faces={{1,1,{1,2,3}},{2,1,{1,2,4}}};
        auto issues=fs::inspectModel(diagnosis);bool winding=false;for(const auto& issue:issues)if(issue.code=="winding"&&issue.ids==QVector<fs::EntityId>{9})winding=true;
        check("model-check-independent-winding",winding);diagnosis.faces.append({3,1,{2,1,5}});issues=fs::inspectModel(diagnosis);bool nonmanifold=false;
        for(const auto& issue:issues)if(issue.code=="nonmanifold"&&issue.ids==QVector<fs::EntityId>{9})nonmanifold=true;
        check("model-check-independent-nonmanifold",nonmanifold);
        diagnosis.points[0].anchor=2;diagnosis.points[1].anchor=3;diagnosis.points[2].anchor=1;diagnosis.points[3].lineA=1;diagnosis.points[3].lineB=5;
        issues=fs::inspectModel(diagnosis);bool cyclic=false;for(const auto& issue:issues)if(issue.code=="constraint"&&issue.ids==QVector<fs::EntityId>{1,2,3,4})cyclic=true;
        check("model-check-transitive-cycle-and-followers",cyclic);
        const QByteArray original=read(input);
        auto model=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(input));
        check("reader-nonempty-control-mesh",!model->points.isEmpty()&&!model->edges.isEmpty()&&!model->faces.isEmpty());
        check("reader-nonempty-derived-triangles",!model->triangles.isEmpty());
        report["control_points"]=model->points.size();report["control_edges"]=model->edges.size();
        report["control_faces"]=model->faces.size();report["triangles"]=model->triangles.size();
        bool pointLookup=true,layerLookup=true;
        for(const auto& p:model->points)pointLookup&=model->point(p.id)&&equal(model->point(p.id)->position,p.position);
        for(const auto& l:model->layers)layerLookup&=model->layer(l.id)&&model->layer(l.id)->name==l.name;
        check("reader-stable-point-lookup",pointLookup);check("reader-stable-layer-lookup",layerLookup);
        check("reader-unknown-identifiers",model->point(0)==nullptr&&model->layer(0)==nullptr);

        const auto json=QJsonDocument::fromJson(original).object();
        auto reject=[&](const QString& id,QJsonObject bad) {
            const QString path=out+"/invalid-"+id+".json";write(path,bad);bool rejected=false;
            try {fs::ModelSnapshot::load(path);} catch(const std::exception&) {rejected=true;}
            check("reader-rejects-"+id,rejected);
        };
        auto bad=json;bad["version"]=999;reject("unknown-version",bad);
        bad=json;bad["units"]="unknown";reject("unknown-units",bad);
        bad=json;auto points=bad["control_points"].toArray();points.append(points[0]);bad["control_points"]=points;reject("duplicate-point-id",bad);
        bad=json;auto edges=bad["control_edges"].toArray();auto edge=edges[0].toObject();edge["a"]=0;edges[0]=edge;bad["control_edges"]=edges;reject("unknown-edge-reference",bad);
        bad=json;auto triangles=bad["triangles"].toArray();auto triangle=triangles[0].toObject();triangle["layer"]=0;triangles[0]=triangle;bad["triangles"]=triangles;reject("unknown-triangle-layer",bad);
        bad=json;points=bad["control_points"].toArray();auto point=points[0].toObject();point["p"]=QJsonArray{0,QJsonValue(),0};points[0]=point;bad["control_points"]=points;reject("null-coordinate",bad);
        bad=json;auto bounds=bad["bounds"].toObject();bounds["min"]=QJsonArray{1e9,1e9,1e9};bad["bounds"]=bounds;reject("inverted-bounds",bad);

        const HullProjection projections[]={HullProjection::Profile,HullProjection::Plan,HullProjection::Body,HullProjection::Perspective};
        const QString names[]={"profile","plan","body","perspective"};
        QJsonObject timings;
        for(int n=0;n<4;++n) {
            HullView view;view.resize(640,420);view.setSnapshot(model);view.setProjection(projections[n]);
            const auto before=view.project(model->points[0].position);const auto base=view.image();
            check(names[n]+"-image-size",base.size()==QSize(640,420));
            check(names[n]+"-image-write",base.save(out+"/"+names[n]+".png"));
            QImage dpi=view.image(1.5);check(names[n]+"-dpi150-size",dpi.size()==QSize(960,630));
            view.panBy({37,21});check(names[n]+"-pan-displacement",near(view.project(model->points[0].position)-before,{37,21}));
            view.panBy({-37,-21});view.zoomAt(before,1.7);check(names[n]+"-cursor-anchor",near(view.project(model->points[0].position),before));
            view.zoomAt(before,1.0/1.7);check(names[n]+"-camera-return",near(view.project(model->points[0].position),before));
            check(names[n]+"-zoom-pan-restores-image",view.image()==base);
            drag(view,{100,100},{37,21},Qt::RightButton);check(names[n]+"-right-button-pan",near(view.project(model->points[0].position)-before,{37,21}));
            const auto anchor=view.project(model->points[0].position);
            const auto second=view.project(model->points[1].position);
            QWheelEvent wheel(anchor,anchor,QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(&view,&wheel);
            check(names[n]+"-wheel-anchor",near(view.project(model->points[0].position),anchor));
            check(names[n]+"-wheel-magnifies",QLineF(view.project(model->points[1].position),anchor).length()>QLineF(second,anchor).length());
            QVector<double> samples;QElapsedTimer timer;
            for(int i=0;i<20;++i) {timer.start();view.panBy({i%2?-3.25:3.25,0});view.image();samples.append(timer.nsecsElapsed()/1e6);}
            timings[names[n]]=statistics(samples);
        }
        report["offscreen_pan_with_readback"]=timings;

        for(int n=0;n<3;++n) {
            HullView view;view.resize(640,420);auto m=fixture();view.setSnapshot(m);view.setProjection(projections[n]);
            const fs::Vec3 source=m->points[0].position;
            auto x=source,y=source,hidden=source;
            if(n==0){x.x+=1;y.z+=1;hidden.y+=1;}
            if(n==1){x.x+=1;y.y+=1;hidden.z+=1;}
            if(n==2){x.y+=1;y.z+=1;hidden.x+=1;}
            const auto p=view.project(source),dx=view.project(x)-p,dy=view.project(y)-p;
            check(names[n]+"-independent-axis-mapping",dx.x()>0&&near(dx.y(),0)&&dy.y()<0&&near(dy.x(),0)&&near(view.project(hidden),p));
            int commits=0;fs::EntityId selected=0,committed=0;fs::Vec3 result;
            view.onPointSelected=[&](fs::EntityId id){selected=id;};
            view.onPointMoved=[&](fs::EntityId id,fs::Vec3 position){++commits;committed=id;result=position;};
            const QPointF delta(20,-15);drag(view,p,delta,Qt::LeftButton);
            fs::Vec3 expected=source;
            if(n==0){expected.x+=delta.x()/dx.x();expected.z+=delta.y()/dy.y();}
            if(n==1){expected.x+=delta.x()/dx.x();expected.y+=delta.y()/dy.y();}
            if(n==2){expected.y+=delta.x()/dx.x();expected.z+=delta.y()/dy.y();}
            check(names[n]+"-pick-and-single-release-command",selected==1&&committed==1&&commits==1);
            check(names[n]+"-drag-model-coordinates",near(result.x,expected.x)&&near(result.y,expected.y)&&near(result.z,expected.z));
            check(names[n]+"-preserves-hidden-coordinate",n==0?result.y==source.y:n==1?result.z==source.z:result.x==source.x);
            check(names[n]+"-view-never-mutates-model",equal(m->points[0].position,source));
            auto locked=fixture(true);view.setSnapshot(locked);view.fit();commits=0;selected=0;
            drag(view,view.project(source),delta,Qt::LeftButton);
            check(names[n]+"-locked-select-without-edit",selected==1&&commits==0);
            view.setSnapshot(m);view.fit();commits=0;
            drag(view,view.project(source),{0,0},Qt::LeftButton);check(names[n]+"-click-without-edit",commits==0);
            mouse(view,QEvent::MouseButtonPress,view.project(source),Qt::LeftButton,Qt::LeftButton);
            mouse(view,QEvent::MouseMove,view.project(source)+delta,Qt::NoButton,Qt::LeftButton);
            QFocusEvent focusLost(QEvent::FocusOut,Qt::OtherFocusReason);QApplication::sendEvent(&view,&focusLost);
            mouse(view,QEvent::MouseButtonRelease,view.project(source)+delta,Qt::LeftButton,Qt::NoButton);
            check(names[n]+"-focus-loss-cancels-edit",commits==0);
            view.setSelectedPoint(1);view.panBy({37,21});view.zoomAt({180,140},1.4);
            const auto fixedAnchor=view.project(m->points[1].position);
            auto revised=std::make_shared<fs::ModelSnapshot>(*m);revised->revision=2;
            revised->maximum.x+=10;revised->points[0].position.x+=10;
            view.setSnapshot(revised);
            check(names[n]+"-revision-preserves-camera",near(view.project(m->points[1].position),fixedAnchor));
            check(names[n]+"-revision-preserves-selection",view.selectedPoint()==1);
            auto other=std::make_shared<fs::ModelSnapshot>(*m);other->documentId="5db37f24-5d47-4c6a-8938-e63a225c0913";
            view.setSnapshot(other);
            HullView fitted;fitted.resize(640,420);fitted.setSnapshot(other);fitted.setProjection(projections[n]);
            check(names[n]+"-new-document-fits",near(view.project(m->points[1].position),fitted.project(m->points[1].position)));
            commits=0;selected=0;view.setEditingEnabled(false);
            drag(view,view.project(source),delta,Qt::LeftButton);
            check(names[n]+"-busy-view-selects-without-edit",!view.editingEnabled()&&selected==1&&commits==0);
            const auto beforePan=view.project(source);
            drag(view,{100,100},{37,21},Qt::RightButton);
            check(names[n]+"-busy-view-still-pans",near(view.project(source)-beforePan,{37,21}));
            view.setEditingEnabled(true);
            mouse(view,QEvent::MouseButtonPress,view.project(source),Qt::LeftButton,Qt::LeftButton);
            mouse(view,QEvent::MouseMove,view.project(source)+delta,Qt::NoButton,Qt::LeftButton);
            view.setEditingEnabled(false);
            mouse(view,QEvent::MouseButtonRelease,view.project(source)+delta,Qt::LeftButton,Qt::NoButton);
            check(names[n]+"-becoming-busy-cancels-preview",commits==0);
        }
        HullView perspective;perspective.resize(640,420);auto m=fixture();perspective.setSnapshot(m);perspective.setProjection(HullProjection::Perspective);
        int edits=0;perspective.onPointMoved=[&](fs::EntityId,fs::Vec3){++edits;};
        drag(perspective,perspective.project(m->points[0].position),{30,20},Qt::LeftButton);
        check("perspective-cannot-accidentally-move-control-point",edits==0);
        // Interior samples are away from edges, point markers and labels.
        auto symmetric=std::make_shared<fs::ModelSnapshot>();
        symmetric->documentId="cab2378a-d5a6-4ddf-af7d-362f2ba634d8";symmetric->revision=1;
        symmetric->name="One starboard triangle";symmetric->units="metric";
        symmetric->minimum={0,0,0};symmetric->maximum={1,5,3};
        symmetric->length=1;symmetric->beam=10;symmetric->draft=1;symmetric->waterlineZ=1;
        symmetric->shadeUnderwater=false;
        symmetric->points={{1,{0,2,0},false},{2,{0,5,0},false},{3,{0,2,3},false}};
        symmetric->edges={{1,1,2,true},{2,2,3,true},{3,3,1,true}};
        symmetric->faces={{1,1,{1,2,3}}};symmetric->layers={{1,"Hull",0x0000ff,true,true}};
        symmetric->triangles={{1,{{symmetric->points[0].position,symmetric->points[1].position,symmetric->points[2].position}}}};
        HullView symmetry;symmetry.resize(640,420);symmetry.setSnapshot(symmetric);symmetry.setProjection(HullProjection::Body);
        const QPoint sourcePixel=symmetry.project({0,3,1}).toPoint(),mirrorPixel=symmetry.project({0,-3,1}).toPoint();
        const auto both=symmetry.image();const auto background=both.pixelColor(3,3);
        const bool visiblePixels=both.rect().contains(sourcePixel)&&both.rect().contains(mirrorPixel);
        check("symmetric-layer-two-interiors",visiblePixels&&both.pixelColor(sourcePixel)!=background&&both.pixelColor(sourcePixel)==both.pixelColor(mirrorPixel));
        auto single=std::make_shared<fs::ModelSnapshot>(*symmetric);single->revision=2;single->layers[0].symmetric=false;
        symmetry.setSnapshot(single);const auto one=symmetry.image();
        check("asymmetric-layer-no-invented-mirror",visiblePixels&&one.pixelColor(sourcePixel)==both.pixelColor(sourcePixel)&&one.pixelColor(mirrorPixel)==background);
        auto hidden=std::make_shared<fs::ModelSnapshot>(*single);hidden->revision=3;hidden->layers[0].visible=false;
        symmetry.setSnapshot(hidden);const auto none=symmetry.image();
        check("hidden-layer-no-filled-face",visiblePixels&&none.pixelColor(sourcePixel)==background&&none.pixelColor(mirrorPixel)==background);
        both.save(out+"/symmetric-layer.png");one.save(out+"/asymmetric-layer.png");none.save(out+"/hidden-layer.png");
        auto grouped=fixture();grouped->pointGroups={{0,"Deck group",false,{1,2}}};grouped->overlay.present=true;
        HullView groupView;groupView.resize(640,420);groupView.setAnimationsEnabled(false);groupView.setProjection(HullProjection::Profile);
        groupView.setSnapshot(grouped);groupView.setFillVisible(false);groupView.setEditingEnabled(true);groupView.fit();
        auto groupImage=groupView.image();groupImage.save(out+"/group-handle.png");bool groupLabel=false;
        for(auto item:groupView.overlayEvidence()["labels"].toArray())if(item.toObject()["key"]=="group:0")groupLabel=true;
        check("point-group-centroid-label",groupLabel);
        QVector<fs::PointMove> groupMoves;groupView.onPointsMoved=[&](QVector<fs::PointMove> moves){groupMoves=std::move(moves);};
        drag(groupView,groupView.project({4,2,2}),{20,10},Qt::LeftButton);
        check("point-group-handle-moves-all-members",groupMoves.size()==2&&groupMoves[0].id==1&&groupMoves[1].id==2&&
            near(groupMoves[0].position.x-2,groupMoves[1].position.x-6)&&near(groupMoves[0].position.z-4,groupMoves[1].position.z));
        groupView.setSelectedPoints({});drag(groupView,groupView.project(grouped->points[0].position),{},Qt::LeftButton);
        check("point-group-member-can-be-selected-individually",groupView.selectedPoints()==QSet<fs::EntityId>{1});
        auto marked=std::make_shared<fs::ModelSnapshot>(*grouped);marked->revision++;
        marked->markers={{0,true,0x0000ff,{{{1,1,1},{4,1,5},{6,1,6}},{false,false,false}}}};marked->showMarkers=true;
        groupView.setSnapshot(marked);auto markerImage=groupView.image();
        auto unmarked=std::make_shared<fs::ModelSnapshot>(*marked);unmarked->revision++;unmarked->showMarkers=false;
        groupView.setSnapshot(unmarked);check("marker-display-toggle",markerImage!=groupView.image());markerImage.save(out+"/model-markers.png");
        auto normalModel=std::make_shared<fs::ModelSnapshot>(*unmarked);normalModel->revision++;normalModel->showNormals=false;
        normalModel->surfaceNormals={{1,{4,2,2},{0,0,1}}};groupView.setSnapshot(normalModel);groupView.setSelectionKind(HullEntity::Face);groupView.setSelectedEntities({1});
        auto withoutNormals=groupView.image();auto withNormals=std::make_shared<fs::ModelSnapshot>(*normalModel);withNormals->revision++;withNormals->showNormals=true;
        groupView.setSnapshot(withNormals);check("selected-face-surface-normals",withoutNormals!=groupView.image());
        check("input-snapshot-file-unchanged",read(input)==original);
        cases.append(QJsonObject{{"id","native-input-focus-and-desktop-presentation"},{"status","not_run"}});
        cases.append(QJsonObject{{"id","four-view-visual-and-performance-acceptance"},{"status","review"}});
        report["status"]=failed?"fail":"partial";
    } catch(const std::exception& e) {report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;
    report["scope"]="Production snapshot reader and four-view renderer; synthetic events and offscreen images only. Timings include readback, not display frame rate.";
    write(out+"/report.json",report);
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
