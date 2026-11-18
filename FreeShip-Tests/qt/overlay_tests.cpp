// Deterministic, independent expectations for the production Qt presentation.
// Retained regression source; no desktop automation or user-file writes.
#include "render/hull_view.h"
#include "render/view_overlay.h"
#include <QApplication>
#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <cmath>
#include <stdexcept>
using namespace fs::presentation;
static bool near(double a,double b,double eps=1e-6){return std::abs(a-b)<=eps;}
static double length(const QVector<Stroke>& lines){double total=0;for(const auto& s:lines)total+=s.line.length();return total;}
static void mouse(QWidget& w,QEvent::Type type,QPointF p,Qt::MouseButton b,Qt::MouseButtons held){QMouseEvent e(type,p,p,b,held,Qt::NoModifier);QApplication::sendEvent(&w,&e);}
static std::shared_ptr<fs::ModelSnapshot> fixture(){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="overlay-independent";m->revision=1;m->units="metric";
    m->minimum={0,0,0};m->maximum={10,2,3};m->length=10;m->beam=4;m->draft=m->waterlineZ=1;m->splitSectionLocation=5;
    m->points={{1,{0,0,0},false,true,"A"},{2,{10,0,0},false,true,"B"},{3,{10,2,3},false,true,"C"},{4,{0,2,3},false,true,"D"}};
    m->edges={{1,1,2,false},{2,2,3,true},{3,3,4,false},{4,4,1,true}};
    m->faces={{1,1,{1,2,3,4}}};m->layers={{1,"Hull",0,true,true}};
    for(int kind=1;kind<=3;++kind){fs::Section s;s.kind=kind;s.index=0;s.distance=kind==1?2:1;m->sections.append(s);}
    m->overlay.present=m->overlay.mainParticulars=m->overlay.hydroAvailable=true;
    m->overlay.markers={{"displacement","Displ=40.00",{5,0,.5}},{"lcf","LCF=5.00",{5,0,1}}};
    m->overlay.areaPath={{0,0,0},{5,0,2},{10,0,0}};m->overlay.areas={{{5,0,2},8}};
    return m;
}
int main(int argc,char** argv){
    QApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 3;
    const auto out=args[2];QDir().mkpath(out);QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](QString id,bool pass){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"}});failed|=!pass;};
    try{
        RevealGraph graph;QVector<Stroke> cross{{{{-100,0},{100,0}},Qt::blue,2},{{{0,-100},{0,100}},Qt::red,1}};
        graph.prepare(cross,{0,0});check("graph-start-empty",graph.visible(0).isEmpty());
        check("intersection-splits-four-propagating-arms",near(length(graph.visible(.5)),200));
        auto full=graph.visible(1);check("completion-exact-original-lines",full.size()==2&&full[0].line==cross[0].line&&full[1].color==Qt::red&&full[0].width==2);
        graph.prepare(cross,{0,0});check("static-geometry-reuses-graph",graph.buildCount()==1);
        graph.prepare(cross,{10,0});check("camera-center-invalidates-graph",graph.buildCount()==2);
        QVector<Stroke> disconnected{{{{0,0},{10,0}},Qt::blue,1},{{{100,0},{110,0}},Qt::red,1}};
        graph.prepare(disconnected,{0,0});check("disconnected-gap-has-no-fake-line",near(length(graph.visible(.5)),10));
        QVector<Stroke> square{{{{0,0},{20,0}},Qt::blue,1},{{{20,0},{20,20}},Qt::blue,1},{{{20,20},{0,20}},Qt::blue,1},{{{0,20},{0,0}},Qt::blue,1}};
        graph.prepare(square,{0,0});check("cycle-final-frame-does-not-pop",length(graph.visible(.99999))>79.99);
        Entrance e;e.restart();e.beginFrame(10000,100);check("delayed-first-paint-starts-at-zero",e.preview()&&e.previewFraction()==0&&!e.pointVisible(100));
        e.beginFrame(10550,100);check("preview-complete-frame-before-transition",e.preview()&&e.previewFraction()==1);e.endFrame();
        e.beginFrame(11050,100);check("point-sweep-right-to-left",e.pointVisible(75)&&!e.pointVisible(25));
        QColor base(49,93,85);auto bright=e.pointColor(1,base);check("first-visible-point-starts-bright",bright==QColor(255,155,20));
        check("same-frame-repeated-draw-does-not-age-point",e.pointColor(1,base)==bright);
        e.pause(11050);e.beginFrame(14000,100);check("pause-freezes-all-clocks",e.pointColor(1,base)==bright&&near(e.lineFraction(),500./1300));
        e.resume(14000);e.beginFrame(14650,100);check("resume-preserves-point-duration",e.pointColor(1,base)==base);
        check("numeric-label-is-not-width-wiped",e.textFraction(true)==1&&e.textFraction(false)>0&&e.textFraction(false)<1);
        auto late=e.pointColor(2,base);e.endFrame();e.beginFrame(15000,100);e.endFrame();check("late-point-pulse-keeps-animation-alive",e.active()&&late==bright);
        e.beginFrame(15300,100);e.pointColor(2,base);e.endFrame();check("final-visible-point-completes-before-stop",!e.active());
        e.restart();e.finish();check("user-interrupt-reveals-final-state",e.pointVisible(-100)&&e.lineFraction()==1&&e.textOpacity(false)==1);

        check("segment-crossing",LabelLayout::segmentHits({10,10,20,20},{{0,20},{40,20}}));
        check("segment-tangent",LabelLayout::segmentHits({10,10,20,20},{{0,10},{40,10}}));
        check("segment-outside",!LabelLayout::segmentHits({10,10,20,20},{{0,9},{40,9}}));
        LabelLayout indexed;indexed.begin({0,0,640,400},0,false);QVector<QLineF> lines;
        for(int i=-50;i<100;++i){lines.append({{double(i*11),-1000},{double(i*9+64),1000}});lines.append({{-100000,double(i*7)},{100000,double(i*7)}});}
        for(auto line:lines)indexed.segment(line);
        bool exactIndex=true;for(int x=0;x<600;x+=31)for(int y=0;y<360;y+=29){QRectF box(x,y,40,17);int brute=0;for(auto line:lines)brute+=LabelLayout::segmentHits(box,line);exactIndex&=indexed.segmentCrossings(box)==brute;}
        check("spatial-index-preserves-exact-crossing-counts",exactIndex);
        LabelLayout layout;layout.begin({0,0,400,300},0,false);layout.reserve({95,95,10,10});
        auto a=layout.place("a","123.456",{100,100},{108,103},{50,16},LabelKind::Point);
        auto b=layout.place("b","789.012",{100,100},{108,103},{50,16},LabelKind::Point);
        check("coincident-points-get-distinct-readable-labels",a.visible&&b.visible&&!a.clip.intersects(b.clip)&&a.clip.size()==QSizeF(50,16)&&b.clip.size()==QSizeF(50,16));
        check("coincident-coordinate-dedup",layout.claim({100,100},"0.000")&&!layout.claim({100,100},"0.000")&&layout.claim({100,100},"1.000"));
        LabelLayout panned;panned.begin({0,0,800,600},0,false);auto p=panned.place("p","Label",{200,200},{208,203},{60,16},LabelKind::Point);
        panned.translate({5,8});panned.begin({0,0,800,600},16,true);auto q=panned.place("p","Label",{205,208},{213,211},{60,16},LabelKind::Point);
        check("pan-preserves-label-offset",p.visible&&q.visible&&QLineF(q.position,p.position+QPointF(5,8)).length()<.01);
        LabelLayout tick;tick.begin({0,0,400,300},0,false);tick.reserve({100,100,40,18});
        auto t=tick.place("tick","1000",{100,100},{100,100},{35,16},LabelKind::VerticalTick);
        check("vertical-grid-label-stays-on-own-axis",t.visible&&near(t.position.x(),100));
        LabelLayout blocked;blocked.begin({0,0,300,200},0,false);blocked.reserve({-500,-500,1000,1000});
        check("fully-blocked-label-is-hidden",!blocked.place("p","1234",{100,100},{108,103},{50,16},LabelKind::Point).visible);
        check("metric-grid-is-millimetres",dimension(1.25,false)=="1250"&&dimension(-.125,false)=="-125");
        check("imperial-grid-is-feet-and-inches",dimension(1.5,true)=="1' 6.0''"&&dimension(-.5,true)=="-0' 6.0''");

        auto m=fixture();HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(800,520);view.setStudioAppearance(true);view.setFillVisible(false);view.setSnapshot(m);
        auto original=view.image();check("complete-image-during-entrance",view.presentationActive()&&!original.isNull());
        for(int ms:{0,275,550,1050,1700,2200})view.presentationImageAt(ms).save(out+"/entrance-"+QString::number(ms)+".png");
        check("animation-does-not-change-export",original==view.image());
        view.setControlNetVisible(false);view.image();check("grid-and-hydro-independent-of-control-net",view.overlayEvidence()["labels"].toArray().size()>0&&view.overlayEvidence()["wave_lines"].toInt()>0);
        view.setGridVisible(false);view.setHydroVisible(false);view.image();check("visibility-switches-hide-overlay",view.overlayEvidence()["labels"].toArray().isEmpty());
        view.setGridVisible(true);view.setControlNetVisible(true);view.setCoordinateAxis(1);view.image();
        auto gridBefore=view.overlayEvidence()["grid_lines"].toInt();view.setSectionVisible(1,false);view.image();
        check("section-toggle-also-hides-its-grid",view.overlayEvidence()["grid_lines"].toInt()<gridBefore);view.setSectionVisible(1,true);
        int coordinates=0;for(auto item:view.overlayEvidence()["labels"].toArray()){auto o=item.toObject();if(o["key"].toString().startsWith("point:")){++coordinates;check("coordinate-preserves-three-decimals-"+o["key"].toString(),o["text"].toString().endsWith(".000"));}}
        check("coordinate-labels-present",coordinates>0);
        view.restartPresentation();view.panBy({15,8});check("pan-ends-entrance-immediately",!view.presentationActive());
        view.restartPresentation();view.zoomAt({400,260},1.1);check("zoom-ends-entrance-immediately",!view.presentationActive());
        view.setAnimationsEnabled(false);view.restartPresentation();check("disabled-animation-does-not-restart",!view.presentationActive());
        check("hidpi-export-dimensions",view.image(2).size()==QSize(1600,1040));
        mouse(view,QEvent::MouseButtonPress,{350,200},Qt::RightButton,Qt::RightButton);mouse(view,QEvent::MouseMove,{390,220},Qt::NoButton,Qt::RightButton);mouse(view,QEvent::MouseButtonRelease,{390,220},Qt::RightButton,Qt::NoButton);
        QContextMenuEvent dragged(QContextMenuEvent::Mouse,{390,220},{390,220});QApplication::sendEvent(&view,&dragged);check("right-drag-does-not-open-menu",view.findChildren<QMenu*>().isEmpty());
        // A plain right click invokes the real context-menu callback; offscreen
        // platform is required by the runner, so this cannot show on the desktop.
        mouse(view,QEvent::MouseButtonPress,{350,200},Qt::RightButton,Qt::RightButton);mouse(view,QEvent::MouseButtonRelease,{350,200},Qt::RightButton,Qt::NoButton);
        QContextMenuEvent clicked(QContextMenuEvent::Mouse,{350,200},{350,200});QApplication::sendEvent(&view,&clicked);
        QList<QAction*> axes;QJsonArray candidates;
        for(auto* action:view.findChildren<QAction*>())if(action->property("coordinate_axis").isValid()&&action->property("coordinate_axis").toInt()==3){axes.append(action);candidates.append(QJsonObject{{"text",action->text()},{"enabled",action->isEnabled()}});}
        report["context_action_candidates"]=candidates;
        if(axes.size()==1&&axes.front()->isEnabled()){axes.front()->trigger();check("context-action-changes-current-view",view.coordinateAxis()==3);}
        else cases.append(QJsonObject{{"id","context-coordinate-action"},{"status","not_run"},{"message","Action missing, ambiguous or disabled; see context_action_candidates."}});
        for(auto* menu:view.findChildren<QMenu*>())menu->hide();
        check("presentation-keeps-source-snapshot",m->revision==1&&m->points[0].position.x==0&&m->points[0].name=="A");

        auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(args[1]));
        QFile input(args[1]);if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Missing input snapshot");
        const auto originalJson=QJsonDocument::fromJson(input.readAll()).object();
        for(const auto& fault:QStringList{"color","font","marker-position","duplicate-marker","unavailable-results","vertex-kind"}){
            auto bad=originalJson;auto overlay=bad["overlay"].toObject();auto hydro=overlay["hydro"].toObject();
            if(fault=="color")overlay["grid_color"]=-1;
            else if(fault=="font")overlay["font_size"]=0;
            else if(fault=="vertex-kind"){auto points=bad["control_points"].toArray();auto first=points[0].toObject();first["vertex_type"]=4;points[0]=first;bad["control_points"]=points;}
            else{
                QJsonObject marker{{"key","sample"},{"text","Valid text"},{"p",QJsonArray{1,2,3}}};QJsonArray markers{marker};hydro["available"]=true;
                if(fault=="marker-position"){marker["p"]=QJsonArray{1,"NaN",3};markers[0]=marker;}
                if(fault=="duplicate-marker")markers.append(marker);
                if(fault=="unavailable-results")hydro["available"]=false;
                hydro["markers"]=markers;overlay["hydro"]=hydro;
            }
            bad["overlay"]=overlay;const auto temp=out+"/invalid-snapshot.json";QFile file(temp);file.open(QIODevice::WriteOnly);file.write(QJsonDocument(bad).toJson(QJsonDocument::Compact));file.close();
            bool rejected=false;try{fs::ModelSnapshot::load(temp);}catch(const std::exception&){rejected=true;}QFile::remove(temp);
            check("invalid-overlay-rejected-"+fault,rejected);
        }
        auto sidecar=QFileInfo(args[1]).absolutePath()+"/sections.json";QFile sections(sidecar);if(sections.open(QIODevice::ReadOnly))actual->loadSectionDisplay(sections.readAll());
        check("actual-snapshot-has-overlay-metadata",actual->overlay.present);
        QJsonArray timings;
        for(int projection=0;projection<4;++projection){view.setProjection(HullProjection(projection));view.setSnapshot(actual);view.setGridVisible(true);view.setHydroVisible(true);view.setCoordinateAxis(0);view.fit();
            view.image().save(out+"/view-"+QString::number(projection)+".png");
            QElapsedTimer clock;clock.start();for(int frame=0;frame<20;++frame){view.panBy({frame%2?2.0:-2.0,0});view.presentationImageAt(frame*16);}
            double ms=clock.nsecsElapsed()/1e6/20;timings.append(QJsonObject{{"projection",projection},{"mean_frame_ms",ms}});
            check("finite-frame-time-"+QString::number(projection),std::isfinite(ms));
        }
        view.setProjection(HullProjection::Profile);view.setCoordinateAxis(1);view.fit();QElapsedTimer clock;
        QJsonArray coordinateTimes;double coordinateTotal=0,coordinateFirst=0;
        for(int frame=0;frame<12;++frame){clock.start();view.panBy({frame%2?1.0:-1.0,0});view.presentationImageAt(500+frame*16);double ms=clock.nsecsElapsed()/1e6;coordinateTimes.append(ms);coordinateTotal+=ms;if(frame==0)coordinateFirst=ms;}
        report["coordinate_pan_frame_ms"]=coordinateTimes;
        report["coordinate_pan_first_ms"]=coordinateFirst;
        report["coordinate_pan_warm_mean_ms"]=(coordinateTotal-coordinateFirst)/11;
        report["coordinate_pan_mean_ms"]=coordinateTotal/12;
        view.image().save(out+"/coordinate-labels.png");
        report["pan_timings"]=timings;
        cases.append(QJsonObject{{"id","desktop-and-visual-parity"},{"status","review"},{"message","Inspect saved images; hidden Qt events do not establish desktop acceptance."}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception& error){report["status"]="error";report["error"]=error.what();}
    report["cases"]=cases;QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(report).toJson());
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
