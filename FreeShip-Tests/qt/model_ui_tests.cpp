// Independent geometry, compositing and interaction expectations. No desktop input.
#include "app/layer_panel.h"
#include "render/hull_view.h"
#include "render/label_layout.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSpinBox>
#include <cmath>
using namespace fs::presentation;
static void mouse(HullView& v,QEvent::Type type,QPointF p,Qt::MouseButton b,Qt::MouseButtons held){QMouseEvent e(type,p,p,b,held,Qt::NoModifier);QApplication::sendEvent(&v,&e);}
static std::shared_ptr<fs::ModelSnapshot> fixture(){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="model-ui-independent";m->revision=1;m->units="metric";m->bothSides=false;m->shadeUnderwater=false;
    m->minimum={0,-2,0};m->maximum={10,2,10};m->length=10;m->beam=4;m->draft=2;
    m->layers={{1,"Red",0x0000ff,true,false},{2,"Blue",0xff0000,true,false}};
    m->points={{1,{0,0,0}},{2,{10,0,0}},{3,{10,0,10}},{4,{0,0,10}}};m->edges={{1,1,2},{2,2,3},{3,3,4},{4,4,1}};m->faces={{1,1,{1,2,3,4}}};
    m->triangles={{1,{{{0,0,0},{10,0,0},{10,0,10}}}},{1,{{{0,0,0},{10,0,10},{0,0,10}}}}};return m;
}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;QString out=QString::fromLocal8Bit(argv[2]);QDir().mkpath(out);QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](QString id,bool pass){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"}});failed|=!pass;};
    try{
        auto m=fixture();HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(600,600);view.setAnimationsEnabled(false);view.setSnapshot(m);view.setProjection(HullProjection::Profile);view.setGridVisible(false);view.setHydroVisible(false);view.setControlNetVisible(false);view.fit();
        auto colorAt=[&](const QImage& image,fs::Vec3 p){const auto pixel=view.project(p);return image.pixelColor(qRound(pixel.x()),qRound(pixel.y()));};
        auto opaque=view.image();check("opaque-fill-is-unchanged",colorAt(opaque,{4,0,5})==QColor(255,0,0));
        auto translucent=std::make_shared<fs::ModelSnapshot>(*m);translucent->revision++;translucent->layers[0].alphaBlend=128;view.setSnapshot(translucent);auto alpha=view.image();
        const QColor blended(252,125,125);auto center=colorAt(alpha,{5,0,5});report["alpha_pixel"]=center.name();check("half-opacity-source-over-background",center==blended);
        bool seamless=true;for(int i=2;i<9;++i)seamless&=colorAt(alpha,{double(i),0,double(i)})==center&&colorAt(alpha,{double(i)+.15,0,double(i)})==center;
        check("shared-triangle-edge-has-no-opacity-seam",seamless);alpha.save(out+"/transparent.png");
        auto zero=std::make_shared<fs::ModelSnapshot>(*translucent);zero->layers[0].alphaBlend=0;zero->revision++;view.setSnapshot(zero);check("zero-opacity-does-not-occlude",colorAt(view.image(),{5,0,5})==QColor(249,250,251));
        auto crossed=std::make_shared<fs::ModelSnapshot>(*translucent);crossed->revision+=2;crossed->layers[1].alphaBlend=128;
        crossed->triangles.append({2,{{{0,-2,0},{10,2,0},{10,2,10}}}});crossed->triangles.append({2,{{{0,-2,0},{10,2,10},{0,-2,10}}}});
        view.setSnapshot(crossed);auto image=view.image();check("intersecting-transparency-uses-per-pixel-depth",colorAt(image,{2,0,4}).red()>colorAt(image,{2,0,4}).blue()&&colorAt(image,{8,0,4}).blue()>colorAt(image,{8,0,4}).red());image.save(out+"/intersecting.png");
        auto opaqueFront=std::make_shared<fs::ModelSnapshot>(*crossed);opaqueFront->layers[1].alphaBlend=255;opaqueFront->revision++;view.setSnapshot(opaqueFront);auto occluded=view.image();check("opaque-front-occludes-transparent-back",colorAt(occluded,{8,0,4})==QColor(0,0,255));
        view.setSnapshot(m);view.setControlNetVisible(true);view.setSelectionKind(HullEntity::Point);const auto p=view.project(m->points[0].position);check("control-point-is-pickable",view.entityAt(p)==1);view.setControlNetVisible(false);auto fill=view.image();view.setControlNetVisible(true);view.setHiddenControlLayers({1});check("hidden-layer-control-point-not-pickable",view.entityAt(p)==0);view.setControlNetVisible(false);check("control-net-hiding-keeps-surface-fill",view.image()==fill);
        auto shared=std::make_shared<fs::ModelSnapshot>(*m);shared->revision++;shared->faces.append({2,2,{1,2,3}});view.setSnapshot(shared);view.setControlNetVisible(true);view.setHiddenControlLayers({1});check("shared-control-point-remains-for-visible-layer",view.entityAt(p)==1);
        view.setHiddenControlLayers({});view.setSnapshot(m);int requests=0;fs::Vec3 created;view.onPointCreationRequested=[&](fs::Vec3 value){++requests;created=value;};
        for(auto projection:{HullProjection::Profile,HullProjection::Plan,HullProjection::Body}){
            view.setProjection(projection);view.fit();view.panBy({23,17});view.zoomAt({250,240},1.4);fs::Vec3 target{3,1,4};auto pixel=view.project(target);const auto count=requests;view.setPointPlacement(true);mouse(view,QEvent::MouseButtonPress,pixel,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,pixel,Qt::LeftButton,Qt::NoButton);
            auto expected=target;if(projection==HullProjection::Profile)expected.y=0;if(projection==HullProjection::Plan)expected.z=0;if(projection==HullProjection::Body)expected.x=10;
            check("point-placement-projection-"+QString::number(int(projection)),requests==count+1&&!view.pointPlacement()&&std::hypot(created.x-expected.x,created.y-expected.y,created.z-expected.z)<1e-8);
        }
        const int count=requests;view.setPointPlacement(true);mouse(view,QEvent::MouseButtonPress,{50,50},Qt::RightButton,Qt::RightButton);check("point-right-cancel-without-edit",!view.pointPlacement()&&requests==count);
        view.setPointPlacement(true);QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&view,&escape);check("point-escape-cancel-without-edit",!view.pointPlacement()&&requests==count);
        view.setProjection(HullProjection::Perspective);view.setPointPlacement(true);check("perspective-rejects-ambiguous-point-placement",!view.pointPlacement());
        LayerPanel panel;panel.resize(800,650);m->layers[0].thickness=10;m->layers[0].materialDensity=2;panel.setSnapshot(m);panel.setSelectedLayerId(1);
        auto measurements=panel.property("layer_measurements").toJsonObject();auto cg=measurements["center"].toArray();check("independent-area-mass-centroid",std::abs(measurements["area"].toDouble()-100)<1e-10&&std::abs(measurements["weight"].toDouble()-2)<1e-10&&cg==QJsonArray{5,0,5});
        auto* symmetric=qobject_cast<QCheckBox*>(panel.fieldEditor("symmetric"));symmetric->setChecked(true);measurements=panel.property("layer_measurements").toJsonObject();check("symmetric-area-and-weight-double",measurements["area"]==200&&measurements["weight"]==4);
        auto* alphaField=qobject_cast<QSpinBox*>(panel.fieldEditor("alpha_blend"));alphaField->setValue(75);check("percent-transparency-to-alpha",panel.changes()["alpha_blend"]==64);panel.discardDrafts();check("cancel-discards-draft-and-keeps-layer",panel.changes().isEmpty()&&panel.selectedLayerId()==1);
        auto exact=std::make_shared<fs::ModelSnapshot>(*m);exact->revision++;exact->layers[0].alphaBlend=137;panel.setSnapshot(exact);check("display-rounding-does-not-rewrite-alpha",panel.changes().isEmpty());
        LabelLayout layout;layout.begin({0,0,600,400},0,false);auto named=layout.place("dwl","DWL 1000",{100,100},{110,100},{90,16},LabelKind::Named);
        check("nearby-named-value-suppresses-numeric-tick",named.visible&&!layout.place("tick","1000",{110,100},{110,130},{40,16},LabelKind::VerticalTick).visible);
        check("distant-numeric-tick-is-not-suppressed",layout.place("far","1000",{400,300},{400,320},{40,16},LabelKind::VerticalTick).visible);
        LabelLayout cache;cache.begin({0,0,600,400},0,false);auto first=cache.place("p","Value",{200,200},{210,210},{40,16},LabelKind::Named);cache.begin({0,0,600,400},16,false);auto changed=cache.place("p","Value",{200,200},{250,210},{40,16},LabelKind::Named);check("changed-preferred-position-invalidates-cache",changed.position.x()>first.position.x()+20);
        LabelLayout covered;covered.begin({0,0,200,100},0,true);covered.reserve({-100,-100,400,400});check("covered-label-is-not-drawn",!covered.place("n","Named 12",{50,50},{60,50},{60,16},LabelKind::Named).visible);covered.begin({0,0,200,100},16,false);check("covered-label-recovers-complete-text",covered.place("n","Named 12",{50,50},{60,50},{60,16},LabelKind::Named).clip.width()==60);
        auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(QString::fromLocal8Bit(argv[1])));panel.setSnapshot(actual);panel.grab().save(out+"/layer-panel.png");
    }catch(const std::exception& e){report["error"]=e.what();}
    report["status"]=report.contains("error")?"error":failed?"fail":"pass";report["cases"]=cases;QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(report).toJson());return report.contains("error")?3:failed?1:0;
}
