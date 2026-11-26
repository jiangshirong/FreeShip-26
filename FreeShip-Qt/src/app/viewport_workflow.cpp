// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production menus, modeless light controls and current-view export.
#include "studio_ui.h"
#include "main_window.h"
#include "point_coordinate_popup.h"
#include <QDoubleSpinBox>
#include <QLabel>
#include "layer_panel.h"
#include "ui_language.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <stdexcept>

void MainWindow::verifyViewport(const QString& path,const QString& output){if(property("viewport_test_settings").isValid())loadPreferences(property("viewport_test_settings").toString());viewportWorkflow_=true;QApplication::setQuitOnLastWindowClosed(false);verifyWorkflow(path,output);}
void MainWindow::viewportStep(){
    if(workflowFinished_ || property("testing_topbar_change").toBool())return;
    try{
        if(workflowStage_++){
            workflowArtifacts_["saved_file"]=workflowOutput_+"/unchanged.fbm";
            workflowCases_.append(QJsonObject{{"name","desktop-native-dialog-print-acceptance"},{"status","not_run"}});finishWorkflow();return;
        }
        for(auto* v:views_){v->setAttribute(Qt::WA_DontShowOnScreen);v->setAnimationsEnabled(false);}
        if(!property("viewport_test_phase").isValid() && !captureStudioMenus(true)){finishWorkflow("Cannot write menu observations");return;}
        captureWorkflow("before");const auto model=session_.model();const auto modified=session_.modified(),undo=session_.canUndo(),redo=session_.canRedo();
        HullView* view=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Perspective)view=v;
        if(!view){workflowCases_.append(QJsonObject{{"name","perspective-view-unavailable"},{"status","not_run"}});finishWorkflow();return;}
        auto missing=[&](const QString& scope){workflowCases_.append(QJsonObject{{"name",scope},{"status","not_run"},{"message","Missing, ambiguous or disabled production entry."}});finishWorkflow();};
        auto menu=[&]()->QMenu*{
            for(auto* old:view->findChildren<QMenu*>())old->hide();QContextMenuEvent event(QContextMenuEvent::Mouse,{100,100},{100,100});QApplication::sendEvent(view,&event);QApplication::processEvents();
            QList<QMenu*> found;for(auto* m:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(m->isVisible())found.append(m);
            if(found.size()!=1){missing("context-menu-unavailable");return nullptr;}return found[0];
        };
        auto action=[&](QMenu* root,const char* property,const QVariant& value)->QAction*{
            if(!root)return nullptr;QList<QAction*> found;QJsonArray observed;
            for(auto* a:root->findChildren<QAction*>())if(a->property(property)==value){found.append(a);observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});}
            workflowArtifacts_[QString(property)+':'+value.toString()]=observed;
            if(found.size()!=1||!found[0]->isEnabled()){missing("action-unavailable:"+value.toString());return nullptr;}return found[0];
        };
        const auto switchMode=[&](HullView* canvas,const QString& mode){
            auto* button=canvas->findChild<QToolButton*>("HullDisplayMode");
            auto* entry=action(button?button->menu():nullptr,"view_operation",mode);
            if(!entry){if(!workflowFinished_)missing("corner-display-mode:"+mode);return false;}
            entry->trigger();
            return canvas->displayModeKey()==mode;
        };
        const auto restartPhase=property("viewport_test_phase").toString();
        if(!restartPhase.isEmpty()){
            const QStringList modes{"inspect_lines","inspect_faces","shade_3","fill"};
            const QMenu* visibility=nullptr;
            for(const auto* button:studioBar_->buttons())
                if(button->defaultAction() && button->defaultAction()->objectName()=="StudioObserve")
                    visibility=button->defaultAction()->menu();
            for(auto* toggle:visibility?visibility->actions():QList<QAction*>{}){
                const auto key=toggle->property("operation").toString();
                const auto flag=key=="both_sides"?&fs::ModelSnapshot::bothSides:
                    key=="show_curvature"?&fs::ModelSnapshot::showCurvature:
                    key=="show_normals"?&fs::ModelSnapshot::showNormals:nullptr;
                if(!flag)continue;
                const bool fileValue=(*model).*flag;
                if(restartPhase=="write"){
                    setProperty("testing_topbar_change",true);
                    toggle->trigger();
                    QElapsedTimer timeout;timeout.start();
                    while(session_.busy() && timeout.elapsed()<15000)
                        QApplication::processEvents(QEventLoop::AllEvents,20);
                    setProperty("testing_topbar_change",false);
                }
                const auto shown=displayModel();
                const bool visible=(*shown).*flag;
                workflowCase("persist-topbar-"+key,toggle->isCheckable() && toggle->isEnabled() &&
                    toggle->isChecked()==!fileValue && visible==!fileValue);
            }
            if(restartPhase=="read")workflowCase("restore-keeps-model-clean",!session_.modified()&&!session_.canUndo());
            if(restartPhase=="write"){
                for(size_t i=0;i<views_.size();++i){
                    view=views_[i];
                    if(!switchMode(view,modes[i]))return;
                }
                auto* popup=menu();auto* lens=action(popup,"focal_mm",90);
                if(!lens)return;
                lens->trigger();popup->hide();
                popup=menu();auto* axis=action(popup,"coordinate_axis",3);
                if(!axis)return;
                axis->trigger();popup->hide();
                HullLight light;light.enabled=true;light.ambient=51;view->setLight(light);
                for(auto* a:findChildren<QAction*>())
                    if(QStringList{"visible_0","control_net","section_3","overlay_grid"}.contains(a->property("viewPreference").toString()) && a->isChecked())a->trigger();
            }
            for(size_t i=0;i<views_.size();++i){
                auto* button=views_[i]->findChild<QToolButton*>("HullDisplayMode");
                workflowCase("persist-mode-"+QString::number(i),views_[i]->displayModeKey()==modes[i]&&
                             button&&button->property("display_mode")==modes[i]&&!button->icon().isNull());
            }
            workflowCase("persist-camera-light-axis",views_.back()->focalLength()==90 && views_.back()->light().ambient==51 &&
                views_.back()->light().enabled && views_.back()->coordinateAxis()==3 && views_.front()->coordinateAxis()==0);
            for(auto* a:findChildren<QAction*>()){
                const auto key=a->property("viewPreference").toString();
                if(QStringList{"visible_0","control_net","section_3","overlay_grid"}.contains(key))
                    workflowCase("persist-"+key,!a->isChecked());
            }
            workflowCase("persist-grid-on-model-load",!views_.back()->gridVisible());
            finishWorkflow();return;
        }
        {
            setAttribute(Qt::WA_DontShowOnScreen);show();QApplication::processEvents();
            const QStringList targets{"inspect_lines","fill","shade_3","inspect_faces"};
            const std::array<bool,4> right{false,true,true,false},bottom{true,true,false,false};
            bool corners=true,localModes=true,opens=true;
            for(size_t i=0;i<views_.size();++i){
                auto* canvas=views_[i];
                auto* button=canvas->findChild<QToolButton*>("HullDisplayMode");
                auto* popup=button?button->menu():nullptr;
                if(!button||!popup){missing("corner-mode-button-unavailable");return;}
                const auto box=button->geometry();
                corners&=button->isVisible()&&canvas->childAt(box.center())==button&&
                    (right[i]?canvas->rect().right()-box.right():box.left())==6&&
                    (bottom[i]?canvas->rect().bottom()-box.bottom():box.top())==6;
                auto* entry=action(popup,"view_operation",targets[i]);
                if(!entry)return;
                const auto nextIcon=entry->icon().cacheKey();
                QStringList before;for(auto* other:views_)before.append(other->displayModeKey());
                bool opened=false;
                QTimer choose;choose.setSingleShot(true);
                connect(&choose,&QTimer::timeout,this,[&]{
                    opened=popup->isVisible();
                    if(opened){
                        if(i==3)popup->grab().save(workflowOutput_+"/display-mode-menu.png");
                        const auto point=popup->actionGeometry(entry).center();
                        for(const auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
                            QMouseEvent event(type,point,popup->mapToGlobal(point),Qt::LeftButton,
                                              type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
                            QApplication::sendEvent(popup,&event);
                        }
                    }
                    popup->hide();
                });
                choose.start(0);button->click();QApplication::processEvents();choose.stop();
                opens&=opened;
                localModes&=canvas->displayModeKey()==targets[i]&&button->icon().cacheKey()==nextIcon&&
                            !entry->icon().isNull()&&entry->isChecked()&&button->accessibleName().contains(entry->text());
                for(size_t j=0;j<views_.size();++j)if(i!=j)localModes&=views_[j]->displayModeKey()==before[j];
            }
            workflowCase("mode-buttons-anchor-at-four-inner-corners",corners);
            workflowCase("click-opens-each-view-mode-popup",opens);
            workflowCase("mode-picker-updates-only-its-view-and-icon",localModes);
            viewGrid_->parentWidget()->grab().save(workflowOutput_+"/view-mode-corners.png");
            for(size_t i=0;i<views_.size();++i)
                if(!switchMode(views_[i],right[i]?"inspect_faces":"inspect_lines"))return;
            QApplication::processEvents();
            const auto legendImage=workflowOutput_+"/view-corner-legends.png";
            if(viewGrid_->parentWidget()->grab().save(legendImage))workflowArtifacts_["corner_legends_image"]=legendImage;
            for(size_t i=0;i<views_.size();++i){
                auto* canvas=views_[i];
                const auto box=canvas->findChild<QToolButton*>("HullDisplayMode")->geometry();
                const auto pixels=canvas->grab().toImage().scaled(canvas->size());
                const auto inkNear=[&](QRect search,QColor target,int tolerance){
                    search=search.intersected(pixels.rect());int count=0;
                    for(int y=search.top();y<=search.bottom();++y)for(int x=search.left();x<=search.right();++x){
                        const auto c=pixels.pixelColor(x,y);
                        if(std::abs(c.red()-target.red())+std::abs(c.green()-target.green())+std::abs(c.blue()-target.blue())<=tolerance)++count;
                    }return count;
                };
                const int cy=box.center().y();
                const QRect beside=right[i]?QRect(0,cy-4,box.left()-4,9):
                    QRect(box.right()+5,cy-4,canvas->width()-box.right()-5,9);
                const bool legend=right[i]?inkNear(beside,QColor("#b5d7dd"),0)>30:
                    inkNear(beside,QColor("#14699e"),20)>15;
                const int oppositeY=bottom[i]?15:canvas->height()-16;
                const QRect oldRow(4,oppositeY-4,canvas->width()-8,9);
                const bool oldAbsent=bottom[i]||(right[i]?inkNear(oldRow,QColor("#b5d7dd"),0)<=30:
                    inkNear(oldRow,QColor("#14699e"),20)<=15);
                workflowCase("legend-along-inner-edge-"+QString::number(i),legend&&oldAbsent);
            }
            for(auto* canvas:views_)if(!switchMode(canvas,"wireframe"))return;
        }
        auto* root=menu();auto* axis=action(root,"coordinate_axis",2);if(!axis)return;axis->trigger();root->hide();
        const auto axisIsLocal=[&](int selected){
            for(auto* candidate:views_)if(candidate->coordinateAxis()!=(candidate==view?selected:0))return false;
            return true;
        };
        workflowCase("coordinate-menu-affects-only-invoked-view",axisIsLocal(2));
        root=menu();auto* off=action(root,"coordinate_axis",0);if(!off)return;off->trigger();root->hide();
        workflowCase("coordinate-menu-disables-only-invoked-view",axisIsLocal(0));
        if(!model->points.empty()){
            selectPoint(model->points.front().id);
            for(auto* canvas:views_){
                showPointPopup(canvas,model->points.front().id);QString axes;
                for(auto* field:pointPopup_->findChildren<QDoubleSpinBox*>())if(field->isVisible())axes+=QChar('X'+field->property("point_popup_axis").toInt());
                const auto p=canvas->projection();const QString expected=p==HullProjection::Body?"YZ":p==HullProjection::Profile?"XZ":p==HullProjection::Plan?"XY":"XYZ";
                workflowCase("point-popup-visible-axes-"+expected,axes==expected,axes);
            }
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(view,&escape);
            workflowCase("escape-clears-selection",selection_.isEmpty());
        }
        // A loose four-edge loop beside two real faces must stay empty, even
        // though all three loops look identical in wireframe.
        {
            auto* shipView=view;
            for(const auto& op:{"inspect_faces","inspect_lines"}){if(!switchMode(view,op))return;
                view->image().save(workflowOutput_+"/"+op+".png");
                QPixmap frame(view->size());
                double cameraMs=0,repeatMs=0;
                for(int i=0;i<10;++i){
                    view->panBy({i%2?-2.:2.,0});
                    view->render(&frame);
                    cameraMs+=view->lastPaintMs;
                    view->render(&frame);
                    repeatMs+=view->lastPaintMs;
                }
                workflowArtifacts_[QString(op)+"_mean_paint_ms"]=QJsonObject{
                    {"after_pan",cameraMs/10},{"same_view",repeatMs/10},{"control_faces",model->faces.size()},
                    {"control_edges",model->edges.size()}};
            }
            if(!switchMode(view,"wireframe"))return;
            HullView fixture;fixture.setAttribute(Qt::WA_DontShowOnScreen);fixture.resize(640,360);fixture.show();fixture.setAnimationsEnabled(false);
            auto sample=std::make_shared<fs::ModelSnapshot>();sample->documentId="three-bays";sample->bothSides=false;sample->layers.append(fs::Layer{1,"shell"});
            for(int z=0;z<2;++z)for(int x=0;x<4;++x)sample->points.append(fs::ControlPoint{fs::EntityId(1+x+4*z),{double(x),0,double(z)}});
            sample->faces={{101,1,{1,2,6,5}},{102,1,{2,3,7,6}}};
            sample->edges={{11,1,5},{12,2,6,true},{13,3,7},{14,4,8},{15,1,2},{16,2,3},{17,3,4},{18,5,6},{19,6,7},{20,7,8}};
            { // Isolate the pending frame: suppress all other ink and compare the old and new crease positions.
                HullView release;release.resize(500,320);release.setProjection(HullProjection::Profile);release.setAnimationsEnabled(false);
                auto staged=std::make_shared<fs::ModelSnapshot>(*sample);auto topology=std::make_shared<fs::SurfaceTopology>();
                topology->points={{1,0,0},{1,0,1}};topology->creases.append(fs::SurfaceCrease{{0,1},{1}});staged->surfaceTopology=topology;
                release.setSnapshot(staged);release.setFillVisible(false);release.setControlNetVisible(false);release.fit();release.finishPresentation();
                auto hasInk=[&](double x){const auto im=release.grab().toImage();auto p=release.project({x,0,.42}).toPoint();p*=im.devicePixelRatio();
                    for(int i=-2;i<=2;++i)if(im.pixelColor(p+QPoint(i,0)).lightness()<160)return true;return false;};
                const bool old=hasInk(1)&&!hasInk(1.5);auto traces=std::make_shared<QVector<DragShellEdge>>();DragShellEdge edge{};edge.a=2;edge.b=6;edge.after.points={{1.5,0,0},{1.5,0,1}};traces->append(edge);
                release.setDragSectionComparison({},traces);release.retainReleasedCreases();release.setDragSectionComparison({});release.setPendingPointMoves({{2,{1.5,0,0}}});
                const bool pending=hasInk(1.5)&&!hasInk(1);release.setPendingPointMoves({});
                workflowCase("crease-release-paints-new-location-without-old-ink",old&&pending&&hasInk(1)&&!hasInk(1.5));
            }
            fixture.setProjection(HullProjection::Profile);fixture.setSnapshot(sample);fixture.finishPresentation();fixture.fit();view=&fixture;
            if(!view->selectDisplayMode("inspect_faces"))return;
            const auto at=[&](double x,double z){return view->project({x,0,z});};const auto img=view->image();
            workflowCase("existing-face-filled-loose-loop-white",img.pixelColor(at(.5,.5).toPoint())==QColor("#b5d7dd")&&img.pixelColor(at(2.5,.5).toPoint())==Qt::white);
            workflowCase("coverage-outlines-cells",
                img.pixelColor(at(3,.5).toPoint()).lightness()+65<255 &&
                std::min(img.pixelColor(at(1,.5).toPoint()).lightness(),img.pixelColor(at(1,.5).toPoint()-QPoint(1,0)).lightness())+65 < img.pixelColor(at(.5,.5).toPoint()).lightness());
            img.save(workflowOutput_+"/three-bays.png");
            const auto coveredOpening=[&](double y,bool closed){
                auto overlap=std::make_shared<fs::ModelSnapshot>(*sample);
                for(int i:{2,3,7,6}){
                    auto p=sample->points[i];p.id+=20;p.position.y=y;overlap->points.append(p);
                }
                overlap->faces.append({103,1,{23,24,28,27}});
                if(!closed)overlap->edges.removeLast();
                view->setSnapshot(overlap);view->finishPresentation();view->fit();
                const QPoint middle=at(2.5,.5).toPoint();
                const auto exported=view->image();
                QImage screen(view->size(),QImage::Format_RGB32);view->render(&screen);
                if(y<0 && closed)screen.save(workflowOutput_+"/white-opening.png");
                const QColor expected=y<0 && closed?QColor(Qt::white):QColor("#b5d7dd");
                return screen.pixelColor(middle)==expected && exported.pixelColor(middle)==expected;
            };
            workflowCase("white-opening-occludes-rear-face",coveredOpening(-1,true));
            workflowCase("near-face-occludes-white-opening",coveredOpening(1,true));
            workflowCase("unclosed-wire-keeps-rear-face-visible",coveredOpening(-1,false));
            auto twoMissing=std::make_shared<fs::ModelSnapshot>(*sample);
            twoMissing->faces.removeLast();
            view->setSnapshot(twoMissing);view->finishPresentation();view->fit();
            const auto missingImage=view->image();
            workflowCase("adjacent-empty-cells-share-a-visible-edge",
                missingImage.pixelColor(at(1.5,.5).toPoint())==Qt::white &&
                missingImage.pixelColor(at(2.5,.5).toPoint())==Qt::white &&
                missingImage.pixelColor(at(2,.5).toPoint()).lightness()<190);
            view->setSnapshot(sample);view->finishPresentation();view->fit();
            workflowCase("restored-face-replaces-white-cell",view->image().pixelColor(at(1.5,.5).toPoint())==QColor("#b5d7dd"));
            {
                auto overlap=std::make_shared<fs::ModelSnapshot>(*sample);
                overlap->points.append({{21,{.25,1,.2}},{22,{1.75,1,.2}},
                                        {23,{1.75,1,.8}},{24,{.25,1,.8}}});
                overlap->faces.append({103,1,{21,22,23,24}});
                overlap->edges.append({{21,21,22},{22,22,23},{23,23,24},{24,24,21}});
                view->setSnapshot(overlap);view->finishPresentation();view->fit();
                const QPoint seam=at(1,.5).toPoint();
                view->setControlNetVisible(false);
                const auto withoutNet=view->image();
                view->setControlNetVisible(true);
                const auto withNet=view->image();
                const auto darkest=[&](const QImage& image){
                    return std::min(image.pixelColor(seam).lightness(),image.pixelColor(seam-QPoint(1,0)).lightness());
                };
                workflowCase("inspect-faces-net-shows-covered-control-edge",
                    withoutNet.pixelColor(seam)==QColor("#b5d7dd")&&darkest(withNet)+55<darkest(withoutNet));
                withNet.save(workflowOutput_+"/inspect_faces-covered-edge.png");
                view->setSnapshot(sample);view->finishPresentation();view->fit();
            }
            // Do not count legend swatches: inspect the middle of each actual edge.
            auto nearColor=[&](const QImage& image,double x,double z,QColor color){const QPoint pt=at(x,z).toPoint();
                for(int dx=-2;dx<=2;++dx)for(int dy=-9;dy<=9;++dy){const auto c=image.pixelColor(pt+QPoint(dx,dy));
                    if(abs(c.red()-color.red())+abs(c.green()-color.green())+abs(c.blue()-color.blue())<70)return true;}return false;};
            if(!view->selectDisplayMode("inspect_lines"))return;
            const auto linesImage=view->image();workflowCase("edge-types-visible-on-geometry",nearColor(linesImage,0,.5,QColor("#14699e"))&&nearColor(linesImage,3,.5,QColor("#a12677"))&&nearColor(linesImage,1,.5,QColor("#ac4a00")));
            for(const auto& mode:{QStringLiteral("inspect_faces"),QStringLiteral("inspect_lines")}){
                view->selectDisplayMode("wireframe");
                view->setSelectionKind(HullEntity::Point);
                view->setSelectedPoints({1});
                view->setControlNetVisible(false);
                view->selectDisplayMode(mode);
                const auto withoutNet=view->image();
                workflowCase(mode+"-keeps-selected-point-with-net-off",
                    view->selectedPoints()==QSet<fs::EntityId>{1}&&nearColor(withoutNet,0,0,QColor("#e06519")));
                view->setControlNetVisible(true);
                const auto withNet=view->image();
                const auto target=at(1,1).toPoint();
                const QRect area(target-QPoint(3,3),QSize(7,7));
                workflowCase(mode+"-shows-unselected-net-points",
                    withNet.copy(area)!=withoutNet.copy(area)&&nearColor(withNet,0,0,QColor("#e06519")));
                withNet.save(workflowOutput_+"/"+mode+"-control-points.png");
                view->setSelectedPoints({});
            }
            const auto mouse=[&](QEvent::Type type,QPointF point){QMouseEvent e(type,point,point,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&e);};
            const auto sweep=[&](HullEntity kind,QSet<fs::EntityId> expected){view->selectDisplayMode(kind==HullEntity::Face?"inspect_faces":"inspect_lines");
                if(view->selectionKind()!=kind)return false;
                mouse(QEvent::MouseButtonPress,at(-.1,.5));mouse(QEvent::MouseMove,at(1.5,.5));mouse(QEvent::MouseButtonRelease,at(2.7,.65));
                return view->selectedEntities()==expected;};
            workflowCase("stroke-selects-crossed-edges-only",sweep(HullEntity::Edge,{11,12,13}));
            workflowCase("stroke-selects-crossed-faces-only",sweep(HullEntity::Face,{101,102}));
            mouse(QEvent::MouseButtonPress,at(.1,.4));mouse(QEvent::MouseMove,at(.9,.4));
            QKeyEvent esc(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(view,&esc);mouse(QEvent::MouseButtonRelease,at(.9,.4));
            workflowCase("cancelled-stroke-preserves-selection",view->selectedEntities()==QSet<fs::EntityId>{101,102});
            // Both adjacent faces share vertices; an axis drag must submit each vertex once.
            auto dragAxis=[&](double x,double z,const QSet<fs::EntityId>& expected){
                QVector<fs::PointMove> moved;view->onPointsMoved=[&](auto v){moved=v;};
                const auto handle=at(x,z)+QPointF(45,0);mouse(QEvent::MouseButtonPress,handle);
                mouse(QEvent::MouseMove,handle+QPointF(24,0));mouse(QEvent::MouseButtonRelease,handle+QPointF(24,0));
                view->onPointsMoved={};QSet<fs::EntityId> ids;
                for(const auto& m:moved){ids.insert(m.id);const auto p=sample->points[int(m.id)-1].position;
                    if(m.position.x<=p.x||m.position.y!=p.y||m.position.z!=p.z)return false;}
                return moved.size()==expected.size()&&ids==expected;
            };
            workflowCase("selected-faces-axis-moves-unique-vertices",dragAxis(1,.5,{1,2,3,5,6,7}));
            view->selectDisplayMode("inspect_lines");view->setSelectedEntities({11});
            workflowCase("selected-edge-axis-moves-both-ends",dragAxis(0,.5,{1,5}));
            view->setSelectionKind(HullEntity::Curve);bool overrideHeld=view->selectionKind()==HullEntity::Edge;
            view->selectDisplayMode("wireframe");workflowCase("inspection-restores-latest-global-picker",overrideHeld&&view->selectionKind()==HullEntity::Curve);
            // Dense curved panel. Pan across tile boundaries and return along the same track.
            auto dense=std::make_shared<fs::ModelSnapshot>();
            dense->documentId="dense-pan";dense->layers.append(fs::Layer{1,"shell"});
            constexpr int cols=160,rows=64;
            for(int z=0;z<=rows;++z)for(int x=0;x<=cols;++x){
                fs::EntityId id=1+z*(cols+1)+x;
                dense->points.append(fs::ControlPoint{id,{double(x),double(z*z)/rows/5,double(z)}});
                if(z && x)dense->faces.append({id,1,{id-cols-2,id-cols-1,id,id-1}});
            }
            view->resize(960,540);view->setProjection(HullProjection::Perspective);
            view->setSnapshot(dense);view->fit();view->finishPresentation();
            if(!view->selectDisplayMode("inspect_faces"))return;
            QImage frame(view->size(),QImage::Format_RGB32);view->render(&frame);
            const QImage startFrame=frame.copy();
            const auto initialBuilds=view->viewSettingsEvidence()["face_projection_builds"].toInteger();
            const auto origin=view->project({0,0,0});
            auto pos=QPointF(200,180);
            auto rightDrag=[&](QEvent::Type type){
                QMouseEvent event(type,pos,pos,type==QEvent::MouseMove?Qt::NoButton:Qt::RightButton,
                    type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::RightButton,Qt::NoModifier);
                QApplication::sendEvent(view,&event);
            };
            QVector<double> times;
            rightDrag(QEvent::MouseButtonPress);
            bool shifted=false;
            double rasterDifference=0;
            for(int i=0;i<80;++i){
                pos+=(i<40?QPointF(9,3):QPointF(-9,-3));
                QElapsedTimer timer;timer.start();rightDrag(QEvent::MouseMove);view->render(&frame);
                times.append(timer.nsecsElapsed()/1e6);
                if(i==39){
                    shifted=QLineF(origin+QPointF(360,120),view->project({0,0,0})).length()<.01 && frame!=startFrame;
                    const auto dpr=view->devicePixelRatioF();
                    const auto reference=view->image(dpr);
                    QImage physical(reference.size(),QImage::Format_RGB32);physical.setDevicePixelRatio(dpr);
                    view->render(&physical);
                    int samples=0;
                    for(int y=qCeil(45*dpr);y<physical.height()-40*dpr;y+=2)for(int x=10;x<physical.width()-10;x+=2){
                        rasterDifference+=std::abs(qGray(reference.pixel(x,y))-qGray(physical.pixel(x,y)));
                        ++samples;
                    }
                    workflowArtifacts_["dense_pan_dpr"]=dpr;
                    physical.save(workflowOutput_+"/pan-cached.png");
                    reference.save(workflowOutput_+"/pan-reference.png");
                    rasterDifference/=samples;
                }
            }
            rightDrag(QEvent::MouseButtonRelease);
            workflowCase("dense-pan-reuses-face-projections",initialBuilds>0 &&
                initialBuilds==view->viewSettingsEvidence()["face_projection_builds"].toInteger());
            workflowCase("dense-pan-matches-full-render",rasterDifference<1.5,QString::number(rasterDifference));
            workflowCase("dense-coverage-pan-round-trip",shifted && frame==startFrame);
            std::sort(times.begin(),times.end());
            workflowArtifacts_["dense_coverage_pan_ms"]=QJsonObject{{"faces",dense->faces.size()},
                {"frames",times.size()},{"median",times[40]},{"p95",times[75]},{"max",times.last()}};
            view=shipView;
        }
        // Production picking receives local mouse events on this private view.
        // Check both point and non-point paths; Shift no longer means additive.
        HullView* picking=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Body)picking=v;
        if(picking){
            picking->setControlNetVisible(true);picking->setFillVisible(false);picking->finishPresentation();picking->fit();
            const auto click=[&](QPointF p,Qt::KeyboardModifiers mods){
                QMouseEvent press(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,mods);QApplication::sendEvent(picking,&press);
                QMouseEvent release(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,mods);QApplication::sendEvent(picking,&release);
                QKeyEvent up(QEvent::KeyRelease,mods.testFlag(Qt::ShiftModifier)?Qt::Key_Shift:Qt::Key_Control,Qt::NoModifier);QApplication::sendEvent(picking,&up);
            };
            for(auto kind:{HullEntity::Point,HullEntity::Edge,HullEntity::Face}){
                setSelectionKind(kind);selectPoints({});bool verified=false;
                for(int y=12;y<picking->height()-12&&!verified;y+=9)for(int x=12;x<picking->width()-12&&!verified;x+=9){
                    const QPoint at(x,y);auto id=picking->entityAt(at);if(!id)continue;
                    click(at,Qt::NoModifier);click(at,Qt::NoModifier);
                    if(!pointPopup_||!pointPopup_->isVisible())continue;
                    auto* layers=pointPopup_->findChild<QLabel*>(kind==HullEntity::Point?"PointLayers":"EntityLayers");
                    auto* count=pointPopup_->findChild<QLabel*>("EntityCount");auto* crease=pointPopup_->findChild<QLabel*>("EdgeCrease");
                    if(!layers||!layers->isVisible())continue;
                    QSet<fs::EntityId> ids;int n=0;bool sharp=false;fs::EntityId a=0,b=0;
                    if(kind==HullEntity::Edge)for(const auto& e:model->edges)if(e.id==id){a=e.a;b=e.b;sharp=e.crease;break;}
                    for(const auto& f:model->faces){bool match=kind==HullEntity::Point?f.points.contains(id):kind==HullEntity::Face&&f.id==id;
                        if(kind==HullEntity::Edge){int i=f.points.indexOf(a),j=f.points.indexOf(b);match=i>=0&&j>=0&&(abs(i-j)==1||abs(i-j)==f.points.size()-1);}
                        if(match){ids.insert(f.layer);n=kind==HullEntity::Face?int(f.points.size()):n+1;}}
                    QStringList names;for(const auto& l:model->layers)if(ids.contains(l.id))names<<layerDisplayName(l);
                    verified=layers->text()==(names.isEmpty()?uiText("无"):names.join("\n"))&&(kind==HullEntity::Point||(count->isVisible()&&count->text().toInt()==n&&crease->isVisible()==(kind==HullEntity::Edge)&&(kind!=HullEntity::Edge||crease->text()==uiText(sharp?"是":"否"))));
                }
                workflowCase(QString("repeat-click-properties-")+(kind==HullEntity::Point?"point":kind==HullEntity::Edge?"edge":"face"),verified);
                if(pointPopup_)pointPopup_->hide();
            }
            for(auto kind:{HullEntity::Point,HullEntity::Edge}){
                setSelectionKind(kind);QList<QPair<fs::EntityId,QPointF>> hits;
                const auto collect=[&](fs::Vec3 p){
                    const auto pixel=picking->project(p);if(!picking->rect().adjusted(8,8,-8,-8).contains(pixel.toPoint()))return;
                    const auto id=picking->entityAt(pixel);if(!id)return;
                    for(const auto& hit:hits)if(hit.first==id||QLineF(hit.second,pixel).length()<24)return;
                    hits.append({id,pixel});
                };
                if(kind==HullEntity::Point){for(const auto& point:model->points){collect(point.position);if(hits.size()==2)break;}}
                else for(const auto& edge:model->edges){const auto* a=model->point(edge.a);const auto* b=model->point(edge.b);if(a&&b)collect({(a->position.x+b->position.x)/2,(a->position.y+b->position.y)/2,(a->position.z+b->position.z)/2});if(hits.size()==2)break;}
                const QString prefix=kind==HullEntity::Point?"point":"edge";
                if(hits.size()!=2){workflowCases_.append(QJsonObject{{"name",prefix+"-modifier-picking"},{"status","not_run"},{"message","Fixture has no two distinct pickable targets"}});continue;}
                const auto selected=[&]{return kind==HullEntity::Point?selection_:entitySelection_;};
                const auto selectFirst=[&]{if(kind==HullEntity::Point)selectPoints({hits[0].first});else selectEntities(kind,{hits[0].first});};
                selectFirst();click(hits[1].second,Qt::ControlModifier);
                workflowCase(prefix+"-ctrl-click-adds",selected()==QSet<fs::EntityId>{hits[0].first,hits[1].first});
                click(hits[1].second,Qt::ControlModifier);
                workflowCase(prefix+"-ctrl-click-removes",selected()==QSet<fs::EntityId>{hits[0].first});
                selectFirst();click(hits[1].second,Qt::ShiftModifier);
                workflowCase(prefix+"-shift-click-is-not-additive",selected()==QSet<fs::EntityId>{hits[1].first});
                click(hits[1].second,Qt::ShiftModifier);
                workflowCase(prefix+"-shift-click-does-not-toggle",selected()==QSet<fs::EntityId>{hits[1].first});
            }
            setSelectionKind(HullEntity::Point);selectPoints({});
        }
        if(!switchMode(view,"fill"))return;
        QList<QImage> otherImages;for(auto* other:views_)if(other!=view)otherImages.append(other->image());
        const auto lightBefore=view->light();root=menu();auto* wide=action(root,"focal_mm",28);if(!wide)return;wide->trigger();root->hide();
        workflowCase("wide-menu-selects-28-not-legacy-20",view->focalLength()==28);
        for(int focal:{50,90,130,200}){root=menu();auto* a=action(root,"focal_mm",focal);if(!a)return;a->trigger();root->hide();workflowCase("lens-action-"+QString::number(focal),view->focalLength()==focal);}
        view->image().save(workflowOutput_+"/before-light.png");
        root=menu();auto* open=action(root,"view_operation","light_settings");if(!open)return;open->trigger();root->hide();QApplication::processEvents();
        QList<QDialog*> candidates;for(auto* dialog:findChildren<QDialog*>())if(dialog->property("view_operation")=="light_dialog"&&dialog->property("light_projection").toInt()==int(HullProjection::Perspective))candidates.append(dialog);
        if(candidates.size()!=1){missing("light-dialog-unavailable");return;}auto* dialog=candidates[0];
        auto field=[&](const QString& key)->QWidget*{QList<QWidget*> found;for(auto* w:dialog->findChildren<QWidget*>())if(w->property("light_field")==key)found.append(w);if(found.size()!=1){missing("light-field-unavailable:"+key);return nullptr;}return found[0];};
        auto* enabled=qobject_cast<QCheckBox*>(field("enabled"));QHash<QString,QSpinBox*> fields;
        for(const auto& key:QStringList{"x","y","z","ambient","intensity"}){auto* number=qobject_cast<QSpinBox*>(field(key));if(!number){if(!workflowFinished_)missing("light-number-unavailable:"+key);return;}fields[key]=number;}
        if(!enabled){if(!workflowFinished_)missing("light-checkbox-unavailable");return;}
        workflowCase("opening-light-does-not-change-view",view->light()==lightBefore);
        workflowCase("light-fields-show-actual-values",fields["x"]->value()==lightBefore.direction.x&&fields["y"]->value()==lightBefore.direction.y&&fields["z"]->value()==lightBefore.direction.z&&fields["ambient"]->value()==lightBefore.ambient&&fields["intensity"]->value()==lightBefore.intensity&&enabled->isChecked()==lightBefore.enabled);
        dialog->grab().save(workflowOutput_+"/light-dialog.png");const auto imageBefore=view->image();
        enabled->setChecked(true);fields["x"]->setValue(0);fields["y"]->setValue(0);fields["z"]->setValue(0);fields["ambient"]->setValue(51);fields["intensity"]->setValue(153);
        auto current=view->light();workflowCase("light-fields-update-current-view",current.enabled&&current.direction.x==0&&current.direction.y==0&&current.direction.z==0&&current.ambient==51&&current.intensity==153);
        workflowCase("light-changes-rendered-image",view->image()!=imageBefore);view->image().save(workflowOutput_+"/after-light.png");dialog->grab().save(workflowOutput_+"/light-edited.png");
        const auto lit=view->image();enabled->setChecked(false);
        workflowCase("light-dialog-checkbox-disables-light",!view->light().enabled&&view->image()!=lit);enabled->setChecked(true);
        workflowCase("light-title-close-available",bool(dialog->windowFlags()&Qt::WindowCloseButtonHint));
        dialog->close();workflowCase("closing-keeps-live-light",!dialog->isVisible()&&view->light().ambient==51&&view->light().enabled);
        root=menu();open=action(root,"view_operation","light_settings");if(!open)return;open->trigger();root->hide();QApplication::processEvents();workflowCase("reopening-restores-view-values",dialog->isVisible()&&fields["ambient"]->value()==51&&fields["x"]->value()==0);dialog->close();
        int index=0;bool local=true;for(auto* other:views_)if(other!=view)local&=other->image()==otherImages[index++];workflowCase("camera-and-light-are-view-local",local);
        root=menu();auto* save=action(root,"view_operation","save_image");if(!save)return;
        root->grab().save(workflowOutput_+"/view-menu.png");const auto path=workflowOutput_+"/export.png";const auto expected=view->image();
        bool handled=false;QTimer picker;picker.setInterval(10);QElapsedTimer deadline;deadline.start();
        connect(&picker,&QTimer::timeout,this,[&]{
            auto* file=qobject_cast<QFileDialog*>(frontStudioDialog());
            if(!file||file->property("view_operation")!="save_image_file")return;
            picker.stop();handled=true;file->selectFile(path);QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);
        });picker.start();save->trigger();root->hide();
        while(!handled&&deadline.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,20);
        if(!handled){finishWorkflow("Missing safe viewport file dialog");return;}
        workflowCase("image-export-includes-selected-lens-and-light",QImage(path)==expected);
        workflowCase("view-settings-do-not-edit-document",session_.model()==model&&session_.modified()==modified&&session_.canUndo()==undo&&session_.canRedo()==redo);
        workflowArtifacts_["view_settings"]=view->viewSettingsEvidence();captureWorkflow("after");session_.save(workflowOutput_+"/unchanged.fbm");
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
