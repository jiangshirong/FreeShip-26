// SPDX-License-Identifier: GPL-2.0-or-later
// Run only via the unified private-desktop runner; no system mouse input.
#include "render/hull_view.h"
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QLineF>
#include <cmath>

namespace {
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
std::shared_ptr<fs::ModelSnapshot> fixture(){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="hull-motion-independent";m->revision=1;m->units="metric";m->bothSides=false;m->shadeUnderwater=false;
    m->minimum={0,0,0};m->maximum={10,0,10};m->length=10;m->beam=2;m->draft=2;
    m->layers={{1,"Motion fixture",0x00ff00,true,false}};
    m->points={{1,{0,0,0}},{2,{10,0,0}},{3,{10,0,10}},{4,{0,0,10}}};
    m->edges={{1,1,2},{2,2,3},{3,3,4},{4,4,1}};m->faces={{1,1,{1,2,3,4}}};
    m->triangles={{1,{{{0,0,0},{10,0,0},{10,0,10}}}},{1,{{{0,0,0},{10,0,10},{0,0,10}}}}};return m;
}
void hover(HullView& view,QPointF at){QMouseEvent move(QEvent::MouseMove,at,view.mapToGlobal(at.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&view,&move);}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setProperty("studio_motion_enabled",true);
    const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;QJsonObject evidence;
    const auto check=[&](QString id,bool ok,QJsonObject observed=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"},{"observed",observed}});};
    const auto model=fixture();HullView view;view.resize(640,520);view.setAnimationsEnabled(false);view.setSnapshot(model);view.setFillVisible(false);view.setProjection(HullProjection::Profile);view.show();wait(40);view.setAnimationsEnabled(true);
    const auto point=view.projectControlPoint(1);hover(view,point);wait(35);auto state=view.interactionMotionEvidence();
    const auto hoverRows=state["hover"].toArray();const double opacity=hoverRows.isEmpty()?0:hoverRows[0].toObject()["opacity"].toDouble();
    check("point-hover-fades-in",opacity>0&&opacity<1,state);view.grab().save(out+"/hover-point-mid.png");
    QEvent leave(QEvent::Leave);QApplication::sendEvent(&view,&leave);wait(100);check("point-hover-leave-retires",view.interactionMotionEvidence()["hover"].toArray().isEmpty());
    for(auto kind:{HullEntity::Edge,HullEntity::Face}){
        view.setSelectionKind(kind);const auto at=kind==HullEntity::Edge?(view.projectControlPoint(1)+view.projectControlPoint(2))/2:(view.projectControlPoint(1)+view.projectControlPoint(3))/2;
        hover(view,at);wait(35);state=view.interactionMotionEvidence();const auto rows=state["hover"].toArray();
        check("entity-hover-"+QString::number(int(kind)),!rows.isEmpty()&&rows[0].toObject()["kind"].toInt()==int(kind)&&rows[0].toObject()["opacity"].toDouble()>0,state);
        QApplication::sendEvent(&view,&leave);wait(100);
    }
    view.setSelectionKind(HullEntity::Point);view.setSelectedPoints({1});wait(30);state=view.interactionMotionEvidence();
    check("selection-emphasizes-real-entity-id",state["selected_ids"].toArray()==QJsonArray{1.0}&&view.selectedPoints()==QSet<fs::EntityId>{1},state);
    wait(250);check("selection-emphasis-is-single-shot",view.interactionMotionEvidence()["selected_ids"].toArray().isEmpty());
    const double original=view.interactionMotionEvidence()["camera_scale"].toDouble();
    view.zoomAt(view.rect().center(),1.7);state=view.interactionMotionEvidence();check("zoom-starts-from-displayed-camera",state["camera_active"].toBool()&&std::abs(state["camera_scale"].toDouble()-original)<1e-6,state);
    wait(35);const double middle=view.interactionMotionEvidence()["camera_scale"].toDouble();check("zoom-has-intermediate-camera",middle>original&&middle<original*1.7);
    view.zoomAt(view.rect().center(),1.2);state=view.interactionMotionEvidence();const double retarget=state["camera_target_scale"].toDouble();
    check("retarget-inherits-nonzero-camera-velocity",state["camera_initial_log_velocity"].toDouble()>0,state);wait(170);
    check("consecutive-wheel-retargets-without-losing-input",std::abs(retarget-original*1.7*1.2)<1e-6&&!view.interactionMotionEvidence()["camera_active"].toBool());
    view.zoomAt(view.rect().center(),1.5);wait(35);view.zoomAt(view.rect().center(),.5);state=view.interactionMotionEvidence();
    check("reverse-input-preserves-incoming-velocity",state["camera_initial_log_velocity"].toDouble()>0&&state["camera_target_scale"].toDouble()<state["camera_scale"].toDouble(),state);wait(170);
    view.fit();wait(50);state=view.interactionMotionEvidence();check("fit-has-intermediate-camera",state["camera_active"].toBool(),state);
    view.panBy({0,0});const auto before=view.projectControlPoint(1);view.panBy({13,9});const auto after=view.projectControlPoint(1);check("direct-pan-interrupts-and-follows-input",!view.interactionMotionEvidence()["camera_active"].toBool()&&QLineF(after-before,QPointF(13,9)).length()<.001);
    view.zoomAt(view.rect().center(),1.2);view.image();check("export-settles-at-final-camera",!view.interactionMotionEvidence()["camera_active"].toBool());
    view.setAnimationsEnabled(false);view.zoomAt(view.rect().center(),1.2);check("disabled-motion-has-immediate-final-camera",!view.interactionMotionEvidence()["camera_active"].toBool());
    check("model-snapshot-is-unchanged",model->revision==1&&model->points[0].position.x==0&&view.snapshot()==model);
    view.hide();check("hidden-view-stops-interaction-driver",!view.interactionMotionEvidence()["driver_running"].toBool());
    int failures=0;for(const auto& c:cases)failures+=c.toObject()["status"]=="fail";
    QFile report(out+"/report.json");report.open(QIODevice::WriteOnly);report.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases},{"artifacts",evidence}}).toJson());return failures?1:0;
}
