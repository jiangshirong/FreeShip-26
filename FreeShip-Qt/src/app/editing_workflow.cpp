#include "main_window.h"
#include "project_dialog.h"
#include "point_coordinate_popup.h"
#include "layer_panel.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QMenu>
#include <QEventLoop>
#include <algorithm>
#include <stdexcept>

namespace {
template<class T> T* editor(QWidget* field) {
    auto* value = qobject_cast<T*>(field);
    if (!value) throw std::runtime_error("Missing production property editor");
    return value;
}
QByteArray bytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read editing evidence");
    return file.readAll();
}
bool same(fs::Vec3 a, fs::Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool differentSection(const fs::Section& a,const fs::Section& b){
    if(a.paths.size()!=b.paths.size())return true;
    for(qsizetype i=0;i<a.paths.size();++i){const auto& p=a.paths[i];const auto& q=b.paths[i];if(p.size()!=q.size())return true;
        for(qsizetype j=0;j<p.size();++j)if(!same(p[j],q[j]))return true;}
    return false;
}
void waitForDragSections(HullView* view){
    if(view->dragSectionComparison())return;
    QEventLoop loop;QTimer poll;QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(view->dragSectionComparison())loop.quit();});
    QTimer::singleShot(6000,&loop,&QEventLoop::quit);poll.start(20);loop.exec(QEventLoop::ExcludeUserInputEvents);
}
class PopupHideCounter final:public QObject {
public:
    explicit PopupHideCounter(QWidget* popup):QObject(popup){popup->installEventFilter(this);}
    bool eventFilter(QObject* watched,QEvent* event)override{
        if(event->type()==QEvent::Hide)watched->setProperty("verifyHideCount",watched->property("verifyHideCount").toInt()+1);
        return false;
    }
};
void workflowPointMouse(HullView* view,QEvent::Type event,QPointF at){
    QMouseEvent e(event,at,view->mapToGlobal(at),event==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,
                  event==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(view,&e);
}
void workflowPointKey(HullView* view,int key,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    QKeyEvent e(QEvent::KeyPress,key,modifiers);QApplication::sendEvent(view,&e);
}
QPointF workflowAxisDirection(HullView* view,fs::Vec3 position,int axis){
    auto end=position;if(axis==1)end.y+=.001;else end.z+=.001;
    auto d=view->project(end)-view->project(position);const double length=QLineF(QPointF(),d).length();
    return length>0?d/length:QPointF();
}
bool workflowAxisInk(HullView* view,fs::EntityId id,int axis){
    const auto position=view->displayedControlPoint(id);
    if(!position)return false;
    const auto frame=view->grab();const auto pixels=frame.toImage();
    const auto offset=workflowAxisDirection(view,*position,axis)*31.;
    const auto center=((view->projectControlPoint(id)+offset)*frame.devicePixelRatio()).toPoint();
    const QColor normal(axis==1?"#16734b":"#285fa9"),hot(axis==1?"#12a75d":"#297df5");
    const auto patch=QRect(center-QPoint(3,3),QSize(7,7)).intersected(pixels.rect());
    for(int y=patch.top();y<=patch.bottom();++y)for(int x=patch.left();x<=patch.right();++x){
        const auto pixel=pixels.pixelColor(x,y);if(pixel==normal||pixel==hot)return true;
    }
    return false;
}
}

// Hidden verification uses the production widgets and command path, with all
// outputs confined to the caller's private evidence directory.
void MainWindow::verifyEditing(const QString& model, const QString& output) {
    editingWorkflow_ = true;
    if(qApp->arguments().contains("--verify-point-transform"))workflowStage_=90;
    verifyWorkflow(model, output);
}

void MainWindow::editingStep(const QString& method) {
    if (workflowFinished_) return;
    try {
        const auto model = session_.model();
        const auto popupFollows=[this](HullView* view,fs::EntityId id,fs::Vec3 expected){
            if(!pointPopup_||!pointPopup_->isVisible()||pointPopup_->pointId()!=id||
               pointPopup_->property("verifyHideCount").toInt()!=0||
               pointPopup_->property("anchor").toPoint()!=view->mapTo(pointPopup_->parentWidget(),view->projectControlPoint(id).toPoint()))return false;
            const auto displayed=view->displayedControlPoint(id);if(!displayed||!same(*displayed,expected))return false;
            const double values[]{expected.x,expected.y,expected.z};int count=0;
            for(auto* f:pointPopup_->findChildren<QDoubleSpinBox*>()){
                const int index=f->property("point_popup_axis").toInt();
                if(index<0||index>2||std::abs(values[index]-f->value())>1e-9||f->isEnabled()==session_.busy())return false;
                ++count;
            }
            return count==3;
        };
        switch (workflowStage_++) {
        case 90: {
            if(method!="load")throw std::runtime_error("Expected a private hull");
            editingOriginal_=model;workflowOriginalDigest_=bytes(session_.scenePath());
            // Render the normal control net and both selection states on the real Kayak.
            {
                HullView preview;preview.resize(700,380);preview.setProjection(HullProjection::Perspective);preview.setStudioAppearance(true);
                preview.setAnimationsEnabled(false);preview.setSnapshot(model);preview.setFillVisible(false);
                preview.setGridVisible(false);preview.setHydroVisible(false);preview.setControlNetVisible(true);
                preview.fit();const auto net=preview.grab();
                fs::EntityId edgeId=0,faceId=0;
                double bestEdge=0,bestFace=0;
                preview.setSelectionKind(HullEntity::Edge);
                for(const auto& edge:model->edges){
                    if(!edge.crease)continue;
                    const QLineF line(preview.projectControlPoint(edge.a),preview.projectControlPoint(edge.b));
                    const auto mid=line.center();
                    if(line.length()>bestEdge&&preview.rect().contains(mid.toPoint())&&preview.entityAt(mid)==edge.id){
                        edgeId=edge.id;bestEdge=line.length();
                    }
                }
                preview.setSelectedEntities({edgeId});const auto edge=preview.grab();
                preview.setSelectionKind(HullEntity::Face);
                for(const auto& face:model->faces){
                    if(face.points.size()<3)continue;
                    QPolygonF polygon;QPointF center;
                    for(auto id:face.points){const auto point=preview.projectControlPoint(id);polygon.append(point);center+=point;}
                    center/=face.points.size();const auto bounds=polygon.boundingRect();
                    const auto area=bounds.width()*bounds.height();
                    if(area>bestFace&&preview.rect().contains(center.toPoint())&&preview.entityAt(center)==face.id){
                        faceId=face.id;bestFace=area;
                    }
                }
                preview.setSelectedEntities({faceId});const auto face=preview.grab();
                const auto bluePixels=[](const QPixmap& frame){
                    const auto image=frame.toImage();int count=0;
                    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
                        const auto c=image.pixelColor(x,y);
                        if(c.blue()>130&&c.blue()>c.red()+90&&c.blue()>c.green()+65)++count;
                    }
                    return count;
                };
                const auto saved=net.save(workflowOutput_+"/views/control-selection-net.png")&&
                    edge.save(workflowOutput_+"/views/control-selection-edge.png")&&face.save(workflowOutput_+"/views/control-selection-face.png");
                const auto baseBlue=bluePixels(net);
                workflowCase("real-hull-control-edge-and-face-use-distinct-selection-ink",saved&&edgeId&&faceId&&
                    bluePixels(edge)>baseBlue+8&&bluePixels(face)>baseBlue+8,QString("edge=%1 face=%2").arg(edgeId).arg(faceId));
            }
            auto *view=views_[3];view->setControlNetVisible(true);view->setFillVisible(false);
            const fs::ControlPoint *seed=nullptr;
            for(const auto &p:model->points)if(!p.locked&&p.batchMovable&&p.position.y>0 && (!seed || p.position.y>seed->position.y))seed=&p;
            if(!seed)throw std::runtime_error("No starboard control point");editingMoves_={{seed->id,seed->position}};
            selectPoints({seed->id});const auto key=[&](int k){QKeyEvent e(QEvent::KeyPress,k,Qt::NoModifier);QApplication::sendEvent(view,&e);};
            auto enterDistance=[&]{for(int k:{Qt::Key_Y,Qt::Key_1,Qt::Key_0,Qt::Key_M,Qt::Key_M})key(k);};
            for(auto *v:views_)v->finishPresentation();
            const auto idleFrame=view->grab(); // Deliver the initial resize before projecting the handle.
            const auto center=view->project(seed->position);auto y=seed->position;y.y+=.001;
            auto d=view->project(y)-center;d/=QLineF(QPointF(),d).length();auto at=center+d*36;
            QRect patch(center.toPoint()-QPoint(99,99),QSize(198,198));
            const auto idle=idleFrame.copy(QRectF(patch.x()*idleFrame.devicePixelRatio(),patch.y()*idleFrame.devicePixelRatio(),patch.width()*idleFrame.devicePixelRatio(),patch.height()*idleFrame.devicePixelRatio()).toAlignedRect());QMouseEvent hover(QEvent::MouseMove,at,at,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(view,&hover);const auto hot=view->grab(patch);
            const bool saved=idle.save(workflowOutput_+"/views/axis-idle.png") && hot.save(workflowOutput_+"/views/axis-hover.png");
            workflowCase("perspective_axis_hover_visible",saved && idle.size()==hot.size() && idle.toImage()!=hot.toImage(),QString("%1,%2").arg(center.x()).arg(center.y()));
            enterDistance();key(Qt::Key_Escape);workflowCase("cancel_leaves_hull_clean",!session_.busy()&&!session_.modified());
            enterDistance();key(Qt::Key_O);
            QWheelEvent grow(QPointF(250,180),QPointF(250,180),{},QPoint(0,240),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(view,&grow);
            auto *poll=new QTimer(this);poll->setInterval(25);connect(poll,&QTimer::timeout,this,[this,view,poll,deadline=workflowTimer_.elapsed()+10000]{
                bool ready=std::all_of(views_.begin(),views_.end(),[](auto *v){return bool(v->moveSectionPreview());});
                if(!ready&&workflowTimer_.elapsed()<deadline)return;poll->stop();poll->deleteLater();
                workflowCase("live_sections_in_four_views",ready);
                auto pairs=view->dragSectionComparison();workflowCase("view-menu-has-no-drag-comparison-switch",!findChild<QAction*>("StudioCompareDragSections"));
                bool matches=pairs&&!pairs->isEmpty()&&pairs->size()<=2;bool changed=false;
                if(matches)for(const auto& pair:*pairs){const auto& src=editingOriginal_->sections;
                    auto it=std::find_if(src.begin(),src.end(),[&](const auto& sec){return sec.kind==1&&sec.index==pair.before.index;});
                    matches&=it!=src.end()&&!differentSection(*it,pair.before);changed|=differentSection(pair.before,pair.after);}
                workflowCase("comparison-uses-original-and-changed-stations",matches&&changed);
                auto* body=views_[1];auto sections=body->dragSectionComparison();auto shell=body->dragShellComparison();
                auto with=body->grab();body->setDragSectionComparison({});auto without=body->grab();body->setDragSectionComparison(sections,shell);
                workflowCase("drag-comparison-visible-without-menu-action",with.toImage()!=without.toImage());
                with.save(workflowOutput_+"/views/station-drag-overlay.png");without.save(workflowOutput_+"/views/station-drag-no-overlay.png");
                if(ready){auto copy=std::make_shared<fs::ModelSnapshot>(*editingOriginal_);copy->sections=*view->moveSectionPreview();editingChanged_=copy;}workflowCase("perspective_preview_saved",view->grab().save(workflowOutput_+"/views/move.png"));
                QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(view,&enter);
                if(!session_.busy())finishWorkflow("Viewport did not submit the move");});poll->start();break;
        }
        case 91: {
            auto seed=editingMoves_[0];double dy=.01/(model->units=="imperial"?.3048:1.);int followers=0;bool fixed=true;
            for(const auto &p:model->points){auto b=editingOriginal_->point(p.id);if(!b)throw std::runtime_error("Changed topology");fixed&=p.position.x==b->position.x&&p.position.z==b->position.z;
                if(p.id==seed.id)continue;double d=p.position.y-b->position.y;fixed&=d>=0 && d<=dy+1e-6;
                if(b->locked||!b->batchMovable||std::abs(b->position.y)<1e-8)fixed&=d==0;if(d>0)++followers;}
            auto match=[](const auto &lhs,const auto &rhs){if(lhs.size()!=rhs.size())return false;
                for(int i=0;i<lhs.size();++i){const auto &a=lhs[i].paths,&b=rhs[i].paths;if(a.size()!=b.size())return false;
                    for(int j=0;j<a.size();++j){if(a[j].size()!=b[j].size())return false;for(int k=0;k<a[j].size();++k)if(!same(a[j][k],b[j][k]))return false;}}return true;};
            workflowCase("live_sections_differ_from_original",editingChanged_&&!match(editingChanged_->sections,editingOriginal_->sections));
            workflowCase("live_sections_match_committed_points",editingChanged_&&match(editingChanged_->sections,model->sections));
            workflowCase("ten_mm_outboard",std::abs(model->point(seed.id)->position.y-seed.position.y-dy)<1e-6);
            workflowCase("model_units",model->units=="imperial");workflowCase("neighbors_follow",followers>0,QString::number(followers));
            workflowCase("longitudinal_height_and_pins_preserved",fixed);
            workflowCase("one_undo_record",method=="move_points"&&model->revision==editingOriginal_->revision+1&&session_.canUndo());
            undo_->trigger();break;
        }
        case 92:{bool restored=method=="undo";for(const auto &p:editingOriginal_->points){const auto *q=model->point(p.id);restored&=q&&same(p.position,q->position);}
            workflowCase("one_undo_restores_every_point",restored);workflowCase("undo_restores_shape",bytes(session_.scenePath())==workflowOriginalDigest_);
            setAttribute(Qt::WA_DontShowOnScreen);show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            selectPoints({});auto *v=views_[3];v->setFillVisible(true);auto at=v->projectControlPoint(editingMoves_.first().id);
            QMouseEvent press(QEvent::MouseButtonPress,at,at,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(v,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,at,at,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(v,&release);
            workflowCase("click-opens-coordinate-editor-clear-of-axes",pointPopup_&&pointPopup_->isVisible()&&pointPopup_->property("avoids_handles").toBool()&&selection_.contains(pointPopup_->pointId()),QString("selected=%1 hit=%2 popup=%3 visible=%4 clear=%5").arg(selected_).arg(v->entityAt(at)).arg(pointPopup_?pointPopup_->pointId():0).arg(pointPopup_&&pointPopup_->isVisible()).arg(pointPopup_&&pointPopup_->property("avoids_handles").toBool()));
            auto* name=pointPopup_?pointPopup_->findChild<QLineEdit*>("PointName"):nullptr;
            workflowCase("first-click-shows-coordinates",name&&!name->isVisible()&&pointPopup_->isVisible());
            auto click=[&]{QApplication::sendEvent(v,&press);QApplication::sendEvent(v,&release);};
            click();workflowCase("second-click-shows-properties",name&&name->isVisible()&&pointPopup_->property("avoids_handles").toBool());
            click();workflowCase("third-click-returns-to-coordinates",name&&!name->isVisible()&&pointPopup_->isVisible());
            const auto id=editingMoves_.first().id;const auto original=model->point(id)->position;
            new PopupHideCounter(pointPopup_);
            workflowPointKey(v,Qt::Key_Shift,Qt::ShiftModifier);workflowPointKey(v,Qt::Key_F8);
            workflowCase("ordinary-keys-keep-coordinate-popup",popupFollows(v,id,original));
            workflowPointKey(v,Qt::Key_Y);workflowPointKey(v,Qt::Key_1);
            const auto preview=v->displayedControlPoint(id);
            workflowCase("axis-key-keeps-popup-and-previews-coordinate",preview&&!same(*preview,original)&&popupFollows(v,id,*preview));
            workflowPointKey(v,Qt::Key_Escape);
            workflowCase("escape-cancels-move-and-restores-coordinate-popup",!session_.busy()&&popupFollows(v,id,original));
            const auto direction=workflowAxisDirection(v,original,1);
            const auto handle=at+direction*36.;const auto target=handle+direction*18.;
            workflowPointMouse(v,QEvent::MouseButtonPress,handle);workflowPointMouse(v,QEvent::MouseMove,target);
            const auto moved=v->displayedControlPoint(id);
            workflowCase("axis-drag-keeps-popup-at-preview",moved&&!same(*moved,original)&&popupFollows(v,id,*moved));
            workflowStage_=97;workflowPointMouse(v,QEvent::MouseButtonRelease,target);
            workflowCase("axis-release-keeps-popup-and-rendered-axis-while-busy",session_.busy()&&moved&&popupFollows(v,id,*moved)&&workflowAxisInk(v,id,1));
            if(!session_.busy())finishWorkflow("Axis drag did not submit a point move");
            break;}
        case 97:{
            const auto id=editingMoves_.first().id;const auto position=model->point(id)->position;auto* perspective=views_[3];
            workflowCase("axis-commit-keeps-popup-and-rendered-axis",popupFollows(perspective,id,position)&&workflowAxisInk(perspective,id,1));
            HullView* view=nullptr;
            for(auto* candidate:views_)if(candidate!=perspective){
                candidate->setControlNetVisible(true);candidate->setFillVisible(false);candidate->finishPresentation();
                const auto at=candidate->projectControlPoint(id);
                if(candidate->rect().adjusted(20,20,-20,-20).contains(at.toPoint())&&candidate->entityAt(at)==id){view=candidate;break;}
            }
            if(!view)throw std::runtime_error("No orthographic view can pick the moved point");
            const auto at=view->projectControlPoint(id),target=at+QPointF(12,24);
            workflowPointMouse(view,QEvent::MouseButtonPress,at);workflowPointMouse(view,QEvent::MouseMove,target);
            const auto moved=view->displayedControlPoint(id);
            workflowCase("direct-drag-keeps-popup-at-preview",moved&&!same(*moved,position)&&popupFollows(view,id,*moved));
            waitForDragSections(view);auto pairs=view->dragSectionComparison();
            workflowCase("direct-mouse-drag-shows-nearby-cuts",pairs&&!pairs->isEmpty()&&pairs->size()<=2&&
                std::any_of(pairs->begin(),pairs->end(),[](const auto& pair){return differentSection(pair.before,pair.after);}),[&]{QStringList details;
                    details<<QString("point %1,%2,%3").arg(position.x).arg(position.y).arg(position.z);
                    for(const auto& s:model->sections)details<<QString("s%1:%2 (%3)").arg(s.kind).arg(s.distance).arg(s.paths.size());
                    if(pairs)for(const auto& p:*pairs)details<<QString("cut %1@%2 %3->%4").arg(p.before.kind).arg(p.before.distance).arg(p.before.paths.size()).arg(p.after.paths.size());else details<<"no cuts";
                    return details.join("; ");}());
            const auto shell=view->dragShellComparison();bool onSurface=shell&&!shell->isEmpty();double bow=0,travel=0;
            if(shell)for(const auto& e:*shell){
                if(e.context)continue;
                onSurface&=(e.a==id||e.b==id)&&e.before.points.size()==e.after.points.size();
                if(e.before.points.size()!=e.after.points.size()||e.before.points.isEmpty())continue;
                auto start=view->project(e.before.points.front()),span=view->project(e.before.points.back())-start;
                for(int i=0;i<e.before.points.size();++i){auto p=e.before.points[i];
                    onSurface&=std::any_of(model->triangles.begin(),model->triangles.end(),[&](const auto& t){return std::any_of(t.vertices.begin(),t.vertices.end(),[&](auto v){return same(v,p);});});
                    const auto u=view->project(p)-start;
                    bow=std::max(bow,std::abs(u.x()*span.y()-u.y()*span.x())/std::max(.001,std::hypot(span.x(),span.y())));
                    travel=std::max(travel,QLineF(view->project(p),view->project(e.after.points[i])).length());
                }
            }
            if(shell){auto local=std::make_shared<QVector<DragShellEdge>>();double lo=model->maximum.x,hi=model->minimum.x;
                for(const auto& edge:*shell){if(!edge.context){local->append(edge);continue;}
                    for(auto p:edge.after.points){lo=std::min(lo,p.x);hi=std::max(hi,p.x);}}
                auto* perspective=views_[3];auto full=perspective->grab().toImage();perspective->setDragSectionComparison(pairs,local);
                auto localOnly=perspective->grab().toImage();perspective->setDragSectionComparison(pairs,shell);
                int remotePixels=0;const auto anchor=perspective->projectControlPoint(id)*full.devicePixelRatio();
                for(int y=0;y<full.height();++y)for(int x=0;x<full.width();++x)if(QLineF(anchor,QPointF(x,y)).length()>70*full.devicePixelRatio()&&full.pixelColor(x,y)==QColor(79,100,118)&&full.pixel(x,y)!=localOnly.pixel(x,y))++remotePixels;
                workflowCase("drag-shows-distant-faired-hull-in-filled-perspective",hi-lo>.7*(model->maximum.x-model->minimum.x)&&remotePixels>20,QString("%1 remote pixels").arg(remotePixels));
                full.save(workflowOutput_+"/views/whole-shell-perspective.png");}
            auto withShell=view->grab();view->setDragSectionComparison(pairs);auto oldDisplay=view->grab();view->setDragSectionComparison(pairs,shell);
            oldDisplay.save(workflowOutput_+"/views/direct-drag-section-only.png");
            workflowCase("ordinary-drag-traces-real-shell-with-no-authored-curves",model->controlCurves.isEmpty()&&onSurface&&travel>2&&withShell.toImage()!=oldDisplay.toImage(),QString("%1 edges, bow %2 px, travel %3 px").arg(shell?shell->size():0).arg(bow).arg(travel));
            withShell.save(workflowOutput_+"/views/direct-drag-overlay.png");
            workflowStage_=98;workflowPointMouse(view,QEvent::MouseButtonRelease,target);
            const auto releasedImage=view->grab().toImage();
            const auto tealPixels=[](const QImage& image){int n=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y)==QColor(19,115,107))++n;return n;};
            workflowCase("release-clears-teal-ink",tealPixels(withShell.toImage())>0&&tealPixels(releasedImage)==0);
            workflowCase("mouse-release-clears-comparison-in-all-views",std::none_of(views_.begin(),views_.end(),[](auto* v){return v->dragSectionComparison()||v->dragShellComparison();}));
            const int axis=view->projection()==HullProjection::Plan?1:2;
            workflowCase("direct-release-keeps-popup-and-rendered-axis-while-busy",session_.busy()&&moved&&popupFollows(view,id,*moved)&&workflowAxisInk(view,id,axis));
            if(!session_.busy())finishWorkflow("Direct drag did not submit a point move");
            break;}
        case 98:{
            const auto id=editingMoves_.first().id;auto* v=selectedPointView_.data();
            if(!v)throw std::runtime_error("Missing popup view after point drag");
            const int axis=v->projection()==HullProjection::Plan?1:2;
            workflowCase("commit-does-not-restore-drag-overlay",std::none_of(views_.begin(),views_.end(),[](auto* v){return v->dragSectionComparison()||v->dragShellComparison();}));
            workflowCase("direct-commit-keeps-popup-and-rendered-axis",popupFollows(v,id,model->point(id)->position)&&workflowAxisInk(v,id,axis));
            workflowPointKey(v,Qt::Key_Escape);
            workflowCase("escape-without-preview-clears-point-selection",!pointPopup_->isVisible()&&selection_.isEmpty());
            selectPoints({id},v);const auto at=v->projectControlPoint(id);
            workflowPointMouse(v,QEvent::MouseButtonPress,at);workflowPointMouse(v,QEvent::MouseButtonRelease,at);
            auto* name=pointPopup_->findChild<QLineEdit*>("PointName");
            if(!name||!name->isVisible())throw std::runtime_error("Cannot edit point name inline");
            auto* type=pointPopup_->findChild<QComboBox*>();if(!type||!type->isEnabled())throw std::runtime_error("Point type not editable");
            type->setCurrentIndex((type->currentIndex()+1)%4);name->setText(QStringLiteral("左舷艏部・甲板 🚢"));workflowStage_=95;
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(name,&enter);break;}
        case 95:{workflowCase("inline-unicode-rename",model->point(editingMoves_.first().id)->name==QStringLiteral("左舷艏部・甲板 🚢"));
            const auto id=editingMoves_.first().id;workflowCase("inline-type-change",model->point(id)->vertexType==(editingOriginal_->point(id)->vertexType+1)%4);
            workflowStage_=93;
            beginPointPlacement();HullView *body=nullptr;for(auto *canvas:views_)if(canvas->projection()==HullProjection::Body)body=canvas;
            for(auto pos:{fs::Vec3{0,-.64,1.25},fs::Vec3{0,.83,1.55}}){auto px=body->project(pos);
                QMouseEvent p(QEvent::MouseButtonPress,px,px,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(body,&p);
                QMouseEvent r(QEvent::MouseButtonRelease,px,px,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(body,&r);}
            workflowCase("rapid-placement-feedback-before-commit",submittingPlacement_.has_value()&&waitingPlacements_.size()==1&&session_.model()->points.size()==editingOriginal_->points.size());break;}
        case 93:workflowCase("first-rapid-click-committed",model->points.size()==editingOriginal_->points.size()+1 && model->points.last().position.x==0);break;
        case 94:{bool exact=model->points.size()==editingOriginal_->points.size()+2;
            if(exact){auto a=model->points[model->points.size()-2].position,b=model->points.last().position;
                auto sign=!model->bothSides && 0<=(model->useDefaultSplitSectionLocation?model->length*.5:model->splitSectionLocation)?-1.:1.;
                exact=a.x==0&&b.x==0&&std::abs(a.y+sign*.64)<1e-5&&std::abs(b.y-sign*.83)<1e-5&&std::abs(a.z-1.25)<1e-5&&std::abs(b.z-1.55)<1e-5;}
            workflowCase("both-body-clicks-keep-screen-side-and-zero-x",exact);
            bool clean=true;for(auto* action:findChildren<QAction*>()){
                const auto op=action->property("operation").toString();if(op=="add_point"||op=="extrude_points")clean=false;
            }
            workflowCase("redundant-point-actions-removed",clean&&placePoint_&&placePoint_->isEnabled());
            if(!pointPopup_||!pointPopup_->isVisible()||pointPopup_->pointId()!=model->points.last().id)throw std::runtime_error("Missing placed-point coordinate editor");
            for(auto* spin:pointPopup_->findChildren<QDoubleSpinBox*>())if(spin->isVisible()&&spin->property("point_popup_axis").toInt()==1)spin->setValue(2.25);
            pointPopup_->findChild<QPushButton*>()->click();workflowStage_=96;break;}
        case 96:{
            workflowCase("placed-point-accepts-exact-inline-coordinate",model->points.last().position.y==2.25&&model->points.last().position.x==0&&model->points.size()==editingOriginal_->points.size()+2);
            beginPointPlacement();auto* body=views_[1];
            auto* station=body->findChild<QDoubleSpinBox*>("StationPlaneX");
            auto* presets=body->findChild<QToolButton*>("StationPlanePresets");
            if(!station||!station->isVisible()||!presets||!presets->menu())throw std::runtime_error("Missing body station controls");
            const auto originalRevision=model->revision;auto* menu=presets->menu();menu->popup(presets->mapToGlobal(presets->rect().bottomLeft()));
            const auto found=std::find_if(model->sections.begin(),model->sections.end(),[](const auto& s){return s.kind==1&&s.distance!=0;});
            QAction* pick=nullptr;if(found!=model->sections.end())for(auto* a:menu->actions())if(a->data().isValid()&&a->data().toDouble()==found->distance){pick=a;break;}
            if(pick)pick->trigger();
            menu->hide();
            workflowCase("station-preset-matches-existing-section",pick&&std::abs(station->value()-found->distance)<1e-9&&station->suffix().trimmed()=="ft");
            auto* text=station->findChild<QLineEdit*>();if(!text)throw std::runtime_error("Missing station coordinate field");
            text->setText(station->locale().toString(9.25,'f',2)+station->suffix());station->interpretText();
            workflowCase("setting-work-plane-does-not-edit-hull",station->value()==9.25&&!session_.busy()&&model->revision==originalRevision);
            body->panBy(QPointF(body->rect().center())-body->project({9.25,1.13,1.625}));
            body->grab().save(workflowOutput_+"/views/station-placement.png");
            workflowStage_=100;
            int n=0;
            for(auto pos:{fs::Vec3{9.25,.47,1.0625},fs::Vec3{9.25,1.13,1.625}}){const auto px=body->project(pos);
                workflowPointMouse(body,QEvent::MouseButtonPress,px);workflowPointMouse(body,QEvent::MouseButtonRelease,px);
                ++n;if(!submittingPlacement_||(n==2&&waitingPlacements_.size()!=1))throw std::runtime_error(QString("Station input %1 not submitted: placement=%2 enabled=%3 busy=%4 local=%5,%6").arg(n).arg(body->pointPlacement()).arg(body->editingEnabled()).arg(session_.busy()).arg(px.x()).arg(px.y()).toStdString());}
            break;}
        case 100:workflowCase("station-first-click-commits-specified-x",model->points.last().position.x==9.25);break;
        case 101:{bool exact=model->points.size()==editingOriginal_->points.size()+4;
            if(exact){const auto a=model->points.at(model->points.size()-2).position,b=model->points.last().position;
                const double sign=!model->bothSides&&9.25<=(model->useDefaultSplitSectionLocation?model->length*.5:model->splitSectionLocation)?-1.:1.;
                exact=a.x==9.25&&b.x==9.25&&std::abs(a.y-sign*.47)<1e-5&&std::abs(b.y-sign*1.13)<1e-5&&std::abs(a.z-1.0625)<1e-5&&std::abs(b.z-1.625)<1e-5;}
            workflowCase("two-station-points-have-correct-x-and-screen-yz",exact);
            auto* body=views_[1];const auto pt=model->points.last();const auto origin=body->projectControlPoint(pt.id);
            workflowCase("placed-point-draws-Z-axis",workflowAxisInk(body,pt.id,2),QString("center %1,%2 view %3,%4").arg(origin.x()).arg(origin.y()).arg(body->width()).arg(body->height()));
            workflowCase("placed-point-popup-clears-handles",pointPopup_->isVisible()&&pointPopup_->property("avoids_handles").toBool(),QString("visible %1 clearance %2").arg(pointPopup_->isVisible()).arg(pointPopup_->property("avoids_handles").toBool()));
            grab().save(workflowOutput_+"/views/placed-point-axes.png");
            const auto arrow=origin-QPointF(0,35);workflowPointMouse(body,QEvent::MouseMove,arrow);
            workflowCase("placed-point-axis-hover",body->cursor().shape()==Qt::PointingHandCursor);
            workflowPointMouse(body,QEvent::MouseButtonPress,arrow);workflowPointMouse(body,QEvent::MouseMove,arrow-QPointF(0,18));
            workflowPointMouse(body,QEvent::MouseButtonRelease,arrow-QPointF(0,18));
            if(!session_.busy())throw std::runtime_error("Placed-point axis drag did not submit");
            break;}
        case 102:{
            const auto pt=model->points.last();workflowCase("placed-point-axis-drag-moves-only-Z-and-keeps-placement",model->points.size()==editingOriginal_->points.size()+4&&pt.position.x==9.25&&std::abs(std::abs(pt.position.y)-1.13)<1e-5&&pt.position.z>1.625&&views_[1]->pointPlacement()&&selection_.contains(pt.id));
            workflowPointKey(views_[1],Qt::Key_Escape);auto* field=views_[1]->findChild<QDoubleSpinBox*>("StationPlaneX");
            workflowCase("leaving-placement-hides-station-control",field&&!field->isVisible()&&!views_[1]->pointPlacement());
            // A side-on midship edge on this kayak can be nearly flat. Also pick a
            // real point in plan and pull it across the beam to inspect the curved hull.
            auto* plan=views_[2];plan->setControlNetVisible(true);plan->finishPresentation();
            bool planChanged=false;double planBow=0;int planEdges=0;
            for(const auto& pt:model->points){const auto at=plan->projectControlPoint(pt.id);
                if(pt.locked||!pt.batchMovable||pt.position.y<.3||plan->entityAt(at)!=pt.id||!plan->rect().adjusted(25,40,-25,-40).contains(at.toPoint()))continue;
                selectPoints({pt.id},plan);workflowPointMouse(plan,QEvent::MouseButtonPress,at);workflowPointMouse(plan,QEvent::MouseMove,at+QPointF(14,38));
                waitForDragSections(plan);auto traces=plan->dragShellComparison();if(traces)for(const auto& e:*traces){++planEdges;
                    for(const auto& shape:{e.before,e.after}){if(shape.points.size()<3)continue;
                        const auto start=plan->project(shape.points.front()),span=plan->project(shape.points.back())-start;
                        for(auto v:shape.points){auto u=plan->project(v)-start;planBow=std::max(planBow,std::abs(u.x()*span.y()-u.y()*span.x())/std::max(.001,std::hypot(span.x(),span.y())));}}}
                auto active=plan->grab();auto sections=plan->dragSectionComparison();plan->setDragSectionComparison(sections);auto old=plan->grab();
                planChanged=active.toImage()!=old.toImage();active.save(workflowOutput_+"/views/plan-shell-drag.png");old.save(workflowOutput_+"/views/plan-section-only.png");
                workflowPointKey(plan,Qt::Key_Escape);workflowPointMouse(plan,QEvent::MouseButtonRelease,at+QPointF(14,38));break;}
            workflowCase("plan-drag-shows-visible-shell-arc-and-cancels",planChanged&&planBow>1&&planEdges>0&&!plan->dragSectionComparison()&&!session_.busy(),QString("%1 edges, bow %2 px").arg(planEdges).arg(planBow));
            beginPointPlacement();auto* body=views_[1];body->onPointCreationRequested({9.25,.77,1.34});break;}
        case 103:{
            auto* body=views_[1];const auto pt=model->points.last();workflowOriginalPoint_=pt.position;workflowPoint_=pt.id;
            auto* spin=pointPopup_->findChild<QDoubleSpinBox*>();auto* text=spin?spin->findChild<QLineEdit*>():nullptr;
            if(!text||!pointPopup_->isVisible())throw std::runtime_error("No XYZ field after placement");
            const auto initial=text->text();spin->selectAll();
            QKeyEvent typeKey(QEvent::KeyPress,Qt::Key_7,Qt::NoModifier,"7");QApplication::sendEvent(text,&typeKey);
            const bool edited=text->text()!=initial;QKeyEvent revert(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier);QApplication::sendEvent(text,&revert);
            workflowCase("placement-undo-in-edited-XYZ-reverts-only-text",edited&&text->text()==initial&&!session_.busy()&&pointPopup_->isVisible());
            const auto at=body->projectControlPoint(pt.id);auto crop=QRect(at.toPoint()-QPoint(6,6),QSize(13,13));auto before=body->grab(crop).toImage();
            QElapsedTimer elapsed;elapsed.start();QKeyEvent key(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier);QApplication::sendEvent(text,&key);
            auto after=body->grab(crop).toImage();workflowTimings_["placement_undo_first_frame_ms"]=elapsed.nsecsElapsed()/1e6;
            workflowCase("placement-undo-removes-marker-popup-and-hit-before-backend-reply",session_.busy()&&session_.model()->point(pt.id)&&before!=after&&!pointPopup_->isVisible()&&body->entityAt(at)!=pt.id);
            if(!session_.busy())throw std::runtime_error("Undo from untouched XYZ field not submitted");break;}
        case 104:workflowTimings_["placement_undo_backend_ms"]=session_.lastOperationMs();workflowCase("placement-undo-committed-exact-id",method=="undo"&&!model->point(workflowPoint_));redo_->trigger();break;
        case 105:{const auto* restored=model->point(workflowPoint_);workflowCase("placement-redo-restores-original-point",restored&&same(restored->position,workflowOriginalPoint_));
            auto* body=views_[1];body->onPointCreationRequested({9.25,.51,1.24});body->onPointCreationRequested({9.25,.59,1.43});
            undo_->trigger();undo_->trigger();workflowCase("undo-accepts-both-outstanding-placement-clicks",placementUndos_==2&&waitingPlacements_.size()==1);break;}
        case 106:case 107:case 108:break;
        case 109:workflowCase("queued-undo-preserves-earlier-geometry-and-redo",method=="undo"&&model->points.size()==editingOriginal_->points.size()+5&&model->point(workflowPoint_)&&session_.canRedo()&&placementUndos_==0);redo_->trigger();break;
        case 110:workflowCase("queued-redo-first-placement",same(model->points.last().position,{9.25,double(float(.51)),double(float(1.24))}));redo_->trigger();break;
        case 111:workflowCase("queued-redo-second-placement",same(model->points.last().position,{9.25,double(float(.59)),double(float(1.43))}));finishWorkflow();break;
        case 0: {
            if (method != "load") throw std::runtime_error("Expected editing model load");
            captureWorkflow("original"); editingOriginal_ = model;
            const auto revision = model->revision;
            ProjectDialog cancelled(*model, this);
            editor<QLineEdit>(cancelled.fieldEditor("name"))->setText("Cancelled project");
            cancelled.reject();
            workflowCase("project_cancel_unchanged", cancelled.result() == QDialog::Rejected &&
                session_.model()->revision == revision && !session_.busy() && !session_.modified());
            ProjectDialog dialog(*model, this);
            editor<QLineEdit>(dialog.fieldEditor("name"))->setText("Qt M2 project");
            editor<QLineEdit>(dialog.fieldEditor("designer"))->setText("Qt M2 designer");
            editor<QPlainTextEdit>(dialog.fieldEditor("comment"))->setPlainText("Qt M2 comment");
            editor<QDoubleSpinBox>(dialog.fieldEditor("draft"))->setValue(model->draft + 0.125);
            if (!dialog.validationError().isEmpty()) throw std::runtime_error(dialog.validationError().toUtf8().constData());
            workflowCase("project_dialog_validated", dialog.validationError().isEmpty());
            if (!session_.updateProject(dialog.changes())) throw std::runtime_error("Project command was not submitted");
            break;
        }
        case 1: {
            if (method != "update_project") throw std::runtime_error("Expected project update");
            captureWorkflow("project"); editingChanged_ = model; workflowOriginalDigest_ = bytes(session_.scenePath());
            workflowCase("project_fields_applied", model->name == "Qt M2 project" && model->designer == "Qt M2 designer" &&
                model->comment == "Qt M2 comment" && model->draft == double(float(editingOriginal_->draft + 0.125)));
            workflowCase("project_one_revision", model->revision == editingOriginal_->revision + 1 && session_.canUndo() && session_.modified());
            ProjectDialog dialog(*model, this);
            auto* units = editor<QComboBox>(dialog.fieldEditor("units"));
            const int next = units->findData(model->units == "metric" ? "imperial" : "metric");
            if (next < 0) throw std::runtime_error("Missing alternate unit choice");
            units->setCurrentIndex(next);
            workflowCase("units_only_patch", dialog.changes().size() == 1 && dialog.changes().contains("units"));
            if (!session_.updateProject(dialog.changes())) throw std::runtime_error("Unit command was not submitted");
            break;
        }
        case 2:
            if (method != "update_project") throw std::runtime_error("Expected unit update");
            captureWorkflow("units");
            workflowCase("units_applied", model->units != editingChanged_->units && model->revision == editingChanged_->revision + 1);
            undo_->trigger(); break;
        case 3: {
            if (method != "undo") throw std::runtime_error("Expected unit undo");
            captureWorkflow("units_undo");
            workflowCase("units_undo_exact_scene", bytes(session_.scenePath()) == workflowOriginalDigest_ && model->units == editingChanged_->units);
            if (model->layers.isEmpty()) throw std::runtime_error("Fixture has no layer");
            editingLayer_ = model->layers.first().id;
            openLayers();
            layers_->editLayerName(editingLayer_);
            auto* name=editor<QLineEdit>(layers_->findChild<QLineEdit*>("layer_inline_name"));
            name->setText("Qt M2 layer");
            workflowStage_=30;
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(name,&enter);
            if(!session_.busy())throw std::runtime_error("Layer rename did not submit a command");
            break;
        }
        case 30: {
            if(method!="update_layer")throw std::runtime_error("Expected inline layer rename");
            auto* color = layers_->fieldEditor("color");
            // The color editor exposes its stored Windows RGB value for hidden
            // verification; its ordinary button still opens the color chooser.
            color->setProperty("colorValue", 0x112233);
            editor<QDoubleSpinBox>(layers_->fieldEditor("thickness"))->setValue(12.5);
            editor<QDoubleSpinBox>(layers_->fieldEditor("material_density"))->setValue(7.5);
            if (!layers_->grab().save(workflowOutput_ + "/views/layers.png")) throw std::runtime_error("Cannot save layer evidence");
            workflowStage_=4;
            if (!session_.busy()) throw std::runtime_error("Layer edits did not submit automatically");
            break;
        }
        case 4: {
            if (method != "update_layer") throw std::runtime_error("Expected layer update");
            layerDialog_->close();captureWorkflow("layer");
            const auto* layer = model->layer(editingLayer_);
            workflowCase("layer_fields_applied", layer && layer->name == "Qt M2 layer" && layer->color == 0x112233 &&
                layer->thickness == 12.5 && layer->materialDensity == 7.5);
            editingChanged_ = model; workflowOriginalDigest_ = bytes(session_.scenePath());
            QSet<fs::EntityId> ids;
            for (const auto& point : model->points) {
                if (point.locked || !point.batchMovable) continue;
                views_.front()->setSelectedPoint(point.id);
                if (views_.front()->selectedPoint() != point.id) continue;
                ids.insert(point.id); editingMoves_.append({point.id, point.position});
                if (ids.size() == 2) break;
            }
            if (ids.size() != 2) throw std::runtime_error("Fixture has fewer than two visible editable points");
            selectPoints(ids); coordinates_[2]->setValue(0.01); apply_->click();
            if (!session_.busy()) throw std::runtime_error("Batch apply did not submit a command");
            break;
        }
        case 5: {
            if (method != "move_points") throw std::runtime_error("Expected batch move");
            captureWorkflow("batch"); workflowMovedDigest_ = bytes(session_.scenePath());
            bool valid = model->revision == editingChanged_->revision + 1;
            for (const auto& original : editingMoves_) {
                const auto* p = model->point(original.id);
                valid &= p && p->position.x == original.position.x && p->position.y == original.position.y &&
                    p->position.z == double(float(original.position.z + 0.01));
            }
            workflowCase("batch_one_command_coordinates", valid); undo_->trigger(); break;
        }
        case 6: {
            if (method != "undo") throw std::runtime_error("Expected batch undo");
            captureWorkflow("batch_undo"); bool valid = true;
            for (const auto& original : editingMoves_) {
                const auto* p = model->point(original.id); valid &= p && same(p->position, original.position);
            }
            workflowCase("batch_undo_exact", valid && bytes(session_.scenePath()) == workflowOriginalDigest_);
            redo_->trigger(); break;
        }
        case 7: {
            if (method != "redo") throw std::runtime_error("Expected batch redo");
            captureWorkflow("batch_redo");
            workflowCase("batch_redo_exact_scene", bytes(session_.scenePath()) == workflowMovedDigest_);
            editingChanged_ = model;
            const QString saved = workflowOutput_ + "/saved.fbm"; workflowArtifacts_.insert("saved_file", saved);
            session_.save(saved); break;
        }
        case 8:
            if (method != "save") throw std::runtime_error("Expected editing save");
            workflowCase("save_clean", !session_.modified());
            session_.load(workflowArtifacts_.value("saved_file").toString()); break;
        case 9: {
            if (method != "load") throw std::runtime_error("Expected editing reopen");
            captureWorkflow("reopened");
            workflowCase("reopen_project_fields", model->name == editingChanged_->name && model->designer == editingChanged_->designer &&
                model->comment == editingChanged_->comment && model->draft == editingChanged_->draft && model->units == editingChanged_->units);
            const auto layer = std::find_if(model->layers.begin(), model->layers.end(), [](const auto& item) { return item.name == "Qt M2 layer"; });
            workflowCase("reopen_layer_material_fields", layer != model->layers.end() && layer->color == 0x112233 &&
                layer->thickness == 12.5 && layer->materialDensity == 7.5);
            workflowCase("reopen_exact_scene", bytes(session_.scenePath()) == workflowMovedDigest_);
            workflowCase("reopen_clean", !session_.modified() && !session_.canUndo() && !session_.canRedo());
            workflowCases_.append(QJsonObject{{"name", "desktop_acceptance"}, {"status", "not_run"},
                {"message", "Hidden production widgets only; new editing gestures need user desktop acceptance."}});
            finishWorkflow(); break;
        }
        default: throw std::runtime_error("Unexpected editing callback");
        }
    } catch (const std::exception& ex) { finishWorkflow(QString::fromUtf8(ex.what())); }
}
