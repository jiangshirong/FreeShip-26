// M2 property controls and selection gestures, linked to the production widgets.
#include "core/model_snapshot.h"
#include "render/hull_view.h"
#include "app/project_dialog.h"
#include "app/layer_panel.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFocusEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <cmath>
#include <stdexcept>

static std::shared_ptr<fs::ModelSnapshot> fixture() {
    auto m=std::make_shared<fs::ModelSnapshot>();
    m->documentId="cf2b168f-a3e7-4486-b667-40236769714a";m->revision=1;m->name="Independent editing fixture";
    m->units="metric";m->minimum={0,0,0};m->maximum={6,7,7};
    m->length=6.123456789;m->beam=14;m->draft=3.123456789;m->waterlineZ=m->draft;
    m->waterDensity=1.025;m->appendageCoefficient=1.0005;m->splitSectionLocation=2.25;
    m->useDefaultSplitSectionLocation=false;m->shadeUnderwater=false;
    m->points={{1,{2,3,4},false},{2,{6,1,0},false},{3,{0,7,2},false},
               {4,{1,0,7},true},{5,{5,6,6},false},{6,{3,5,1},false}};
    m->edges={{1,1,2,true},{2,2,3,false},{3,3,4,false},{4,4,1,false},{5,4,5,false},{6,5,3,false}};
    m->faces={{1,1,{1,2,3,4}},{2,2,{3,4,5}}};
    m->layers={{1,"Hull A",0x0000ff,true,false},{2,"Hidden",0x00ff00,false,false}};
    m->layers[0].thickness=12.123456789;m->layers[0].materialDensity=7.85;
    m->layers[0].useInHydrostatics=false;
    m->triangles={{1,{{m->points[0].position,m->points[1].position,m->points[2].position}}},
                  {1,{{m->points[0].position,m->points[2].position,m->points[3].position}}},
                  {2,{{m->points[2].position,m->points[3].position,m->points[4].position}}}};
    return m;
}
static void mouse(QWidget& widget,QEvent::Type type,QPointF p,Qt::MouseButton button,
                  Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier) {
    QMouseEvent event(type,p,p,button,buttons,mods);QApplication::sendEvent(&widget,&event);
}
static void click(QWidget& widget,QPointF p,Qt::KeyboardModifiers mods=Qt::NoModifier) {
    mouse(widget,QEvent::MouseButtonPress,p,Qt::LeftButton,Qt::LeftButton,mods);
    mouse(widget,QEvent::MouseButtonRelease,p,Qt::LeftButton,Qt::NoButton,mods);
}
static void drag(QWidget& widget,QPointF from,QPointF to,Qt::KeyboardModifiers mods=Qt::NoModifier) {
    mouse(widget,QEvent::MouseButtonPress,from,Qt::LeftButton,Qt::LeftButton,mods);
    mouse(widget,QEvent::MouseMove,to,Qt::NoButton,Qt::LeftButton,mods);
    mouse(widget,QEvent::MouseButtonRelease,to,Qt::LeftButton,Qt::NoButton,mods);
}
static bool near(double a,double b){return std::abs(a-b)<1e-8;}
static bool near(QPointF a,QPointF b){return QLineF(a,b).length()<1e-7;}
static bool same(fs::Vec3 a,fs::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
template<class T> static T* editor(QWidget* widget) {
    auto result=qobject_cast<T*>(widget);if(!result)throw std::runtime_error("Production field editor has unsupported type");return result;
}
static bool displayed(QDoubleSpinBox* spin,double expected) {
    return std::abs(spin->value()-expected)<=0.500001*std::pow(10.0,-spin->decimals())+1e-10*std::abs(expected);
}
static void pendingText(QDoubleSpinBox* spin,const QString& text) {
    auto line=spin->findChild<QLineEdit*>();if(!line)throw std::runtime_error("Numeric editor has no line editor");
    line->selectAll();QKeyEvent event(QEvent::KeyPress,Qt::Key_unknown,Qt::NoModifier,text);QApplication::sendEvent(line,&event);
}
int main(int argc,char**argv) {
    QApplication app(argc,argv);if(argc!=3)return 3;
    const QString input=app.arguments()[1],out=app.arguments()[2];QDir().mkpath(out);
    QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try {
        auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(input));
        check("m2-reader-project-properties",std::isfinite(actual->waterDensity)&&actual->waterDensity>0&&actual->precision>=0&&actual->precision<=3);
        check("m2-reader-layer-properties",!actual->layers.isEmpty()&&actual->layers[0].thickness>=0&&actual->layers[0].materialDensity>=0);
        const HullProjection projections[]={HullProjection::Profile,HullProjection::Plan,HullProjection::Body};
        const QString names[]={"profile","plan","body"};
        for(int n=0;n<3;++n) {
            auto m=fixture();HullView view;view.resize(640,420);view.setSnapshot(m);view.setProjection(projections[n]);
            int selections=0,batches=0,legacy=0,rejections=0;QVector<fs::PointMove> submitted;
            view.onSelectionChanged=[&](QSet<fs::EntityId>){++selections;};
            view.onPointsMoved=[&](QVector<fs::PointMove> points){++batches;submitted=points;};
            view.onPointMoved=[&](fs::EntityId,fs::Vec3){++legacy;};
            view.onEditRejected=[&](QString){++rejections;};
            click(view,view.project(m->points[0].position));check(names[n]+"-single-selection",view.selectedPoints()==QSet<fs::EntityId>{1});
            click(view,view.project(m->points[1].position),Qt::ControlModifier);
            check(names[n]+"-control-adds-selection",view.selectedPoints()==QSet<fs::EntityId>({1,2}));
            click(view,view.project(m->points[0].position),Qt::ShiftModifier);
            check(names[n]+"-shift-toggles-selection",view.selectedPoints()==QSet<fs::EntityId>{2});
            view.setSelectedPoints({1,2});const auto original1=m->points[0].position,original2=m->points[1].position;
            const auto start=view.project(original1);fs::Vec3 x=original1,y=original1;
            if(n==0){x.x+=1;y.z+=1;}if(n==1){x.x+=1;y.y+=1;}if(n==2){x.y+=1;y.z+=1;}
            const double sx=(view.project(x)-start).x(),sy=(view.project(y)-start).y();
            drag(view,start,start+QPointF(20,-15));
            check(names[n]+"-multi-drag-single-command",batches==1&&legacy==0&&submitted.size()==2);
            bool positions=submitted.size()==2;
            for(const auto& move:submitted) {
                const auto* source=m->point(move.id);if(!source||(move.id!=1&&move.id!=2)){positions=false;continue;}
                auto expected=source->position;
                if(n==0){expected.x+=20/sx;expected.z+=-15/sy;}
                if(n==1){expected.x+=20/sx;expected.y+=-15/sy;}
                if(n==2){expected.y+=20/sx;expected.z+=-15/sy;}
                positions&=near(move.position.x,expected.x)&&near(move.position.y,expected.y)&&near(move.position.z,expected.z);
                positions&=n==0?move.position.y==source->position.y:n==1?move.position.z==source->position.z:move.position.x==source->position.x;
            }
            check(names[n]+"-batch-delta-and-hidden-axes",positions);
            check(names[n]+"-batch-view-keeps-snapshot-immutable",same(m->points[0].position,original1)&&same(m->points[1].position,original2));
            view.setSelectedPoints({1,4});batches=0;rejections=0;
            drag(view,view.project(original1),view.project(original1)+QPointF(25,-20));
            check(names[n]+"-mixed-lock-selection-rejects-whole-drag",batches==0&&rejections==1&&view.selectedPoints()==QSet<fs::EntityId>({1,4}));
            auto related=std::make_shared<fs::ModelSnapshot>(*m);related->revision=2;related->points[1].batchMovable=false;
            view.setSnapshot(related);view.setSelectedPoints({1,2});batches=0;rejections=0;
            drag(view,view.project(original1),view.project(original1)+QPointF(25,-20));
            check(names[n]+"-related-point-rejects-whole-batch",batches==0&&rejections==1);
            view.setSelectedPoints({2});batches=0;
            drag(view,view.project(original2),view.project(original2)+QPointF(25,-20));
            check(names[n]+"-related-point-single-edit-still-supported",batches==1&&submitted.size()==1&&submitted[0].id==2);
            view.setSnapshot(m);
            view.setSelectedPoints({1});batches=0;
            drag(view,{5,5},{635,415});
            check(names[n]+"-box-selects-visible-and-locked-points",view.selectedPoints()==QSet<fs::EntityId>({1,2,3,4,6}));
            check(names[n]+"-box-selection-does-not-edit",batches==0);
            view.setSelectedPoints({1});const auto p2=view.project(original2);
            drag(view,p2-QPointF(14,14),p2+QPointF(14,14),Qt::ControlModifier);
            check(names[n]+"-control-box-appends",view.selectedPoints()==QSet<fs::EntityId>({1,2}));
            view.setSelectedPoints({1});const int notificationsBefore=selections;
            mouse(view,QEvent::MouseButtonPress,{5,5},Qt::LeftButton,Qt::LeftButton);
            mouse(view,QEvent::MouseMove,{635,415},Qt::NoButton,Qt::LeftButton);
            QFocusEvent focusOut(QEvent::FocusOut,Qt::OtherFocusReason);QApplication::sendEvent(&view,&focusOut);
            mouse(view,QEvent::MouseButtonRelease,{635,415},Qt::LeftButton,Qt::NoButton);
            check(names[n]+"-cancelled-box-keeps-selection",view.selectedPoints()==QSet<fs::EntityId>{1}&&selections==notificationsBefore);
            click(view,view.project(m->points[4].position));check(names[n]+"-hidden-point-not-pickable",!view.selectedPoints().contains(5));
            const int beforeSet=selections;view.setSelectedPoints({1,5,999});
            check(names[n]+"-selection-setter-filters-hidden-and-missing",view.selectedPoints()==QSet<fs::EntityId>{1}&&selections==beforeSet);
            view.setSelectedPoints({1,2});view.setEditingEnabled(false);batches=0;
            drag(view,view.project(original1),view.project(original1)+QPointF(20,20));
            check(names[n]+"-busy-selection-cannot-submit",batches==0);view.setEditingEnabled(true);
            auto hidden=std::make_shared<fs::ModelSnapshot>(*m);hidden->revision=2;hidden->layers[0].visible=false;
            view.setSelectedPoints({1,6});view.setSnapshot(hidden);
            check(names[n]+"-hidden-layer-removes-selection-keeps-orphan",view.selectedPoints()==QSet<fs::EntityId>{6});
            view.setSnapshot(m);view.panBy({37,21});view.zoomAt({220,160},1.4);
            auto units=std::make_shared<fs::ModelSnapshot>(*m);units->revision=3;units->units="imperial";
            for(auto& p:units->points){p.position.x/=0.3048;p.position.y/=0.3048;p.position.z/=0.3048;}
            units->maximum={6/0.3048,7/0.3048,7/0.3048};
            view.setSnapshot(units);HullView fitted;fitted.resize(640,420);fitted.setSnapshot(units);fitted.setProjection(projections[n]);
            check(names[n]+"-units-change-refits-camera",near(view.project(units->points[0].position),fitted.project(units->points[0].position)));
            view.image().save(out+"/"+names[n]+"-editing.png");
        }
        {
            auto topology=fixture();topology->points={{1,{0,0,0},false},{5,{2,0,0},false},{9,{2,2,0},false},{12,{0,2,0},false}};
            topology->edges={{4,1,5,true},{17,5,9,true},{23,9,12,true},{42,12,1,true}};
            topology->faces={{45,1,{1,5,9,12}}};topology->triangles.clear();
            HullView view;view.resize(640,420);view.setSnapshot(topology);view.setProjection(HullProjection::Plan);
            view.setSelectionKind(HullEntity::Edge);int notifications=0,moves=0;
            view.onEntitySelectionChanged=[&](HullEntity,QSet<fs::EntityId>){++notifications;};
            view.onPointsMoved=[&](QVector<fs::PointMove>){++moves;};
            const auto bottom=view.project({1,0,0}),right=view.project({2,1,0});
            check("edge-picking-sparse-id",view.entityAt(bottom)==4);
            click(view,bottom);click(view,right,Qt::ControlModifier);
            check("edge-control-multiselect",view.selectedEntities()==QSet<fs::EntityId>({4,17})&&notifications==2&&moves==0);
            click(view,bottom,Qt::ShiftModifier);check("edge-toggle-selection",view.selectedEntities()==QSet<fs::EntityId>{17});
            view.setSelectionKind(HullEntity::Face);const auto center=view.project({1,1,0});
            check("face-picking-control-polygon",view.entityAt(center)==45);click(view,center);
            check("face-selection-does-not-move-points",view.selectedEntities()==QSet<fs::EntityId>{45}&&moves==0);
            auto hidden=std::make_shared<fs::ModelSnapshot>(*topology);hidden->revision=2;hidden->layers[0].visible=false;
            view.setSnapshot(hidden);check("hidden-face-prunes-selection",view.selectedEntities().isEmpty()&&view.entityAt(center)==0);
            view.setSnapshot(topology);view.setSelectionKind(HullEntity::Edge);view.setSelectedEntities({17});
            auto fresh=std::make_shared<fs::ModelSnapshot>(*topology);fresh->documentId="a5b50f32-e6a7-4970-919d-3e299de7a38c";
            view.setSnapshot(fresh);check("new-document-clears-edge-selection",view.selectedEntities().isEmpty());
        }
        {
            auto overlap=fixture();overlap->points.clear();overlap->edges.clear();overlap->faces.clear();overlap->triangles.clear();
            for(int side=0;side<2;++side) {
                const fs::EntityId base=100+side*10;
                overlap->points.append({base,{0,0,double(side)},false});overlap->points.append({base+1,{2,0,double(side)},false});
                overlap->edges.append({fs::EntityId(70+side),base,base+1,true});
            }
            HullView view;view.resize(640,420);view.setSnapshot(overlap);view.setProjection(HullProjection::Plan);view.setSelectionKind(HullEntity::Edge);
            check("overlapping-edges-pick-nearest-depth",view.entityAt(view.project({1,0,1}))==71);
            auto faces=fixture();faces->points.clear();faces->edges.clear();faces->faces.clear();faces->triangles.clear();
            const double yaw=-30*3.14159265358979323846/180,pitch=25*3.14159265358979323846/180;
            const fs::Vec3 right{std::cos(yaw),std::sin(yaw),0};
            const fs::Vec3 up{-std::sin(yaw)*std::sin(pitch),std::cos(yaw)*std::sin(pitch),std::cos(pitch)};
            const fs::Vec3 depth{std::sin(yaw)*std::cos(pitch),-std::cos(yaw)*std::cos(pitch),std::sin(pitch)};
            // Two parallel squares face the initial camera, far from the world
            // origin. The square closer to that camera must win, independent
            // of ID order and the GPU's normalized coordinate representation.
            for(int side=0;side<2;++side) {
                const double along=side?1.0:-1.0;QVector<fs::EntityId> ids;
                for(int i=0;i<4;++i) {
                    const double x=(i==1||i==2)?1:-1,y=i>=2?1:-1;
                    const auto id=fs::EntityId(200+side*10+i);ids.append(id);
                    faces->points.append({id,{10000+along*depth.x+x*right.x+y*up.x,
                        20000+along*depth.y+x*right.y+y*up.y,30000+along*depth.z+x*right.z+y*up.z},false});
                }
                faces->faces.append({fs::EntityId(80+side),1,ids});
            }
            view.setSnapshot(faces);view.setProjection(HullProjection::Perspective);view.setSelectionKind(HullEntity::Face);
            check("translated-perspective-faces-pick-nearest-depth",view.entityAt(view.project({10000,20000,30000}))==81);
        }
        auto m=fixture();
        {
            auto sample=*m;sample.sections={{1,0,2,false,0,{1,0,0,-2},{}}};
            QJsonObject definition{{"kind",1},{"index",0},{"plane",QJsonArray{1,0,0,-2}},
                {"paths",QJsonArray{QJsonArray{QJsonArray{2,0,0},QJsonArray{2,1,1}}}}};
            auto document=[&](QJsonArray entries){return QJsonDocument(QJsonObject{{"format","FreeShipSectionDisplay"},{"version",1},{"sections",entries}}).toJson();};
            sample.loadSectionDisplay(document({definition}));
            check("section-display-loads-world-points",sample.sections[0].paths[0][1].y==1&&sample.sections[0].paths[0][1].z==1);
            for(const QString mutation:{"plane","duplicate","missing","key","number","short"}){
                auto bad=definition;QJsonArray entries;
                if(mutation=="plane")bad["plane"]=QJsonArray{1,0,0,-3};
                if(mutation=="key")bad["kind"]=2;
                if(mutation=="number")bad["paths"]=QJsonArray{QJsonArray{QJsonArray{2,0,0},QJsonArray{"bad",1,1}}};
                if(mutation=="short")bad["paths"]=QJsonArray{QJsonArray{QJsonArray{2,0,0}}};
                if(mutation!="missing")entries.append(bad);if(mutation=="duplicate")entries.append(bad);
                bool rejected=false;try{sample.loadSectionDisplay(document(entries));}catch(const std::exception&){rejected=true;}
                check("section-display-rejects-"+mutation,rejected&&sample.sections[0].paths.size()==1&&sample.sections[0].paths[0][1].z==1);
            }
        }
        ProjectDialog untouched(*m);check("project-opening-does-not-round-model",untouched.changes().isEmpty()&&m->length==6.123456789);
        untouched.accept();check("project-unchanged-accept-keeps-empty-command",untouched.result()==QDialog::Accepted&&untouched.changes().isEmpty());
        ProjectDialog cancelled(*m);editor<QLineEdit>(cancelled.fieldEditor("name"))->setText("Cancelled change");cancelled.reject();
        check("project-cancel-keeps-model",cancelled.result()==QDialog::Rejected&&m->name=="Independent editing fixture");
        ProjectDialog units(*m);auto combo=editor<QComboBox>(units.fieldEditor("units"));
        const int imperial=combo->findData("imperial"),metric=combo->findData("metric");
        if(imperial<0||metric<0)throw std::runtime_error("Units editor lacks metric/imperial values");
        combo->setCurrentIndex(imperial);
        check("project-pure-units-command-has-no-rounded-overrides",units.changes().size()==1&&units.changes().value("units")=="imperial");
        const double densityFactor=1000*0.3048*0.3048*0.3048/0.45359237;
        check("project-units-display-length",displayed(editor<QDoubleSpinBox>(units.fieldEditor("length")),double(float(m->length/0.3048))));
        check("project-units-display-density",displayed(editor<QDoubleSpinBox>(units.fieldEditor("water_density")),double(float(m->waterDensity*densityFactor))));
        combo->setCurrentIndex(metric);check("project-unit-toggle-back-is-no-op",units.changes().isEmpty());
        editor<QDoubleSpinBox>(units.fieldEditor("length"))->setValue(8.5);
        editor<QLineEdit>(units.fieldEditor("name"))->setText("Changed project");units.accept();
        check("project-explicit-fields-one-draft",units.result()==QDialog::Accepted&&units.changes().size()==2&&units.changes()["length"].toDouble()==8.5&&units.changes()["name"]=="Changed project");
        const auto temperature=units.fieldEditor("water_temperature");
        check("project-unpersisted-temperature-not-editable",!temperature||!temperature->isEnabled()||(qobject_cast<QLineEdit*>(temperature)&&qobject_cast<QLineEdit*>(temperature)->isReadOnly()));
        auto historical=*m;historical.name=QString(5000,'A');historical.designer=QString(5000,'B');historical.comment="First\r\nSecond\r\n";
        ProjectDialog longText(historical);check("project-long-name-and-crlf-untouched",longText.changes().isEmpty());
        editor<QDoubleSpinBox>(longText.fieldEditor("water_density"))->setValue(1.1);
        check("project-unrelated-edit-does-not-rewrite-text",longText.changes().size()==1&&longText.changes().contains("water_density"));
        ProjectDialog typed(*m);pendingText(editor<QDoubleSpinBox>(typed.fieldEditor("length")),"9.25");
        auto buttons=typed.findChild<QDialogButtonBox*>();if(!buttons||!buttons->button(QDialogButtonBox::Ok))throw std::runtime_error("Project dialog lacks an OK button role");
        buttons->button(QDialogButtonBox::Ok)->click();
        check("project-ok-commits-pending-keyboard-text",typed.result()==QDialog::Accepted&&typed.changes()["length"].toDouble()==9.25);
        ProjectDialog typedUnits(*m);pendingText(editor<QDoubleSpinBox>(typedUnits.fieldEditor("length")),"9.25");
        auto typedCombo=editor<QComboBox>(typedUnits.fieldEditor("units"));typedCombo->setCurrentIndex(typedCombo->findData("imperial"));
        // An edited draft retains Double precision until the core receives it;
        // only untouched snapshot values preview the core's Single conversion.
        const double editedImperial=9.25/0.3048;
        check("project-unit-switch-keeps-pending-old-unit-input",typedUnits.changes().contains("units")&&typedUnits.changes().contains("length")&&displayed(editor<QDoubleSpinBox>(typedUnits.fieldEditor("length")),editedImperial));
        check("project-unit-switch-submits-unrounded-edited-draft",std::abs(typedUnits.changes()["length"].toDouble()-editedImperial)<1e-13);
        auto defaults=*m;defaults.useDefaultSplitSectionLocation=true;defaults.splitSectionLocation=9999;
        ProjectDialog split(defaults);editor<QCheckBox>(split.fieldEditor("use_default_split_section_location"))->setChecked(false);
        check("project-default-split-off-emits-current-midpoint",split.changes().contains("split_section_location")&&displayed(editor<QDoubleSpinBox>(split.fieldEditor("split_section_location")),m->length/2));

        LayerPanel panel;panel.setSnapshot(m);panel.setSelectedLayerId(1);int requests=0;fs::EntityId target=0;QJsonObject changes;
        panel.onLayerChanged=[&](fs::EntityId id,const QJsonObject& update){++requests;target=id;changes=update;};
        check("layer-opening-does-not-round-model",panel.changes().isEmpty()&&m->layers[0].thickness==12.123456789);
        panel.applyChanges();check("layer-no-op-apply-does-not-submit",requests==0);
        editor<QLineEdit>(panel.fieldEditor("name"))->setText("Renamed layer");
        editor<QDoubleSpinBox>(panel.fieldEditor("thickness"))->setValue(8.5);
        panel.applyChanges();check("layer-explicit-apply-one-request",requests==1&&target==1&&changes.size()==2&&changes["name"]=="Renamed layer"&&changes["thickness"].toDouble()==8.5);
        check("layer-draft-does-not-mutate-snapshot",m->layers[0].name=="Hull A"&&m->layers[0].thickness==12.123456789);
        auto updated=std::make_shared<fs::ModelSnapshot>(*m);updated->revision=2;updated->layers[0].name="Renamed layer";updated->layers[0].thickness=8.5;
        panel.setSnapshot(updated);check("layer-refresh-clears-draft-without-submit",requests==1&&panel.changes().isEmpty());
        panel.setSelectedLayerId(2);check("layer-selection-does-not-submit",requests==1&&panel.selectedLayerId()==2);
        editor<QLineEdit>(panel.fieldEditor("name"))->setText("Blocked change");panel.setEditingEnabled(false);panel.applyChanges();
        check("layer-busy-panel-cannot-submit",requests==1);
        LayerPanel hydro;hydro.setSnapshot(m);hydro.setSelectedLayerId(1);
        editor<QCheckBox>(hydro.fieldEditor("use_in_hydrostatics"))->setChecked(true);
        check("layer-hydro-displays-automatic-symmetry",editor<QCheckBox>(hydro.fieldEditor("symmetric"))->isChecked()&&hydro.changes().value("use_in_hydrostatics").toBool()&&!hydro.changes().contains("symmetric"));
        editor<QCheckBox>(hydro.fieldEditor("symmetric"))->setChecked(false);
        check("layer-explicit-symmetry-override-is-not-lost",hydro.changes().contains("symmetric")&&!hydro.changes().value("symmetric").toBool());
        LayerPanel typedLayer;typedLayer.setSnapshot(m);typedLayer.setSelectedLayerId(1);QJsonObject typedChange;
        typedLayer.onLayerChanged=[&](fs::EntityId,const QJsonObject& update){typedChange=update;};
        pendingText(editor<QDoubleSpinBox>(typedLayer.fieldEditor("thickness")),"4.75");typedLayer.applyChanges();
        check("layer-apply-commits-pending-keyboard-text",typedChange["thickness"].toDouble()==4.75);
        cases.append(QJsonObject{{"id","real-native-mouse-keyboard-focus-and-dialog-presentation"},{"status","not_run"}});
        cases.append(QJsonObject{{"id","property-dialog-and-selection-visual-acceptance"},{"status","review"}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception& error){report["status"]="error";report["error"]=error.what();}
    report["cases"]=cases;report["scope"]="Actual property and hull widgets, independent fixtures and synthetic Qt events; no desktop automation.";
    QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(report).toJson());
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
