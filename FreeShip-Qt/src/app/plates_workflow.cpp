// Persistent hidden production workflow; native desktop acceptance is separate.
#include "studio_ui.h"
#include "main_window.h"
#include "plates_dialog.h"
#include "pdf_preview_probe.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include "plate_assembly.h"
#include "plate_depth_buffer.h"
#include <QPainter>
#include "layer_panel.h"
#include "ui_language.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QMouseEvent>
#include <QPushButton>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsSimpleTextItem>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLayout>
#include <QThread>
#include <QLabel>
#include <QTableWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QEventLoop>
#include <algorithm>
void MainWindow::verifyPlates(const QString& request,const QString& output){
    workflow_=true;platesWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read plate request");return;}
    platesRequest_=QJsonDocument::fromJson(f.readAll()).object();QDir().mkpath(workflowOutput_+"/views");
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();openModel(platesRequest_["model"].toString());
}
void MainWindow::platesStep(const QString& operation){
    if(operation=="closed"){captureWorkflow("after");workflowCase("plates.readonly",!session_.modified()&&!session_.canUndo()&&double(session_.model()->revision)==workflowArtifacts_["before_revision"].toDouble());finishWorkflow();return;}
    if(operation!="load")return;
    captureWorkflow("original");workflowArtifacts_["before_revision"]=double(session_.model()->revision);
    if(platesRequest_.contains("render_fixture")){
        QFile fixture(platesRequest_["render_fixture"].toString());if(!fixture.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read private plate fixture");return;}
        PlatesDialog dialog(session_,true,this);dialog.resize(1500,1000);dialog.show();dialog.receive(QJsonDocument::fromJson(fixture.readAll()).object());
        if(!dialog.hasResult()){finishWorkflow(dialog.failure());return;}
        workflowArtifacts_["drag_measurement"]=dialog.measureDrag();
        for(auto* canvas:dialog.findChildren<QGraphicsView*>()){
            canvas->scale(8,8);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const auto image=workflowOutput_+"/views/plate-canvas-zoomed.png";
            workflowCase("drag.zoomed-canvas-picture",dialog.grab().save(image));workflowArtifacts_["canvas_zoom_image"]=image;
        }
        for(const auto* format:{"json","dxf","txt","png","pdf"})workflowCase("drag.export."+QString(format),dialog.exportFile(workflowOutput_+"/drag."+format,format).isEmpty());
        dialog.reject();captureWorkflow("after");workflowCase("drag.document-unchanged",!session_.modified());finishWorkflow();return;
    }

    QList<QAction*> candidates;for(auto*a:findChildren<QAction*>())if(a->property("operation")=="develop_plates")candidates<<a;
    if(candidates.size()!=1||!candidates[0]->isEnabled()){workflowCases_.append(QJsonObject{{"name","plate-entry"},{"status","not_run"}});finishWorkflow();return;}
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!platesDialog_||session_.busy())return;
        auto* task=calculationSession("plates:develop");
        auto end=[this,timer]{timer->stop();timer->deleteLater();platesDialog_->reject();};
        if(*stage==0){
            QStringList actions;
            for(auto* button:platesDialog_->findChildren<QPushButton*>()){
                const auto action=button->property("plates_action").toString();
                if(!action.isEmpty())actions.append(action);
            }
            workflowCase("plates.toolbar-only-export",actions==QStringList{"dxf","txt","json","png","pdf"},actions.join(", "));
            *stage=1;
        }
        if(task&&task->busy())return;
        if(*stage==2){workflowCase("plates.cancel-preserves-result",!platesDialog_->failure().isEmpty()&&platesDialog_->result()==workflowArtifacts_["plate_data"].toObject());end();return;}
        if(!platesDialog_->failure().isEmpty()){
            const auto failure=platesDialog_->failure();workflowCase("plates.expected-failure",platesRequest_["expect_failure"].toBool(),failure);
            auto* label=platesDialog_->findChild<QLabel*>("PlateStatus");workflowCase("plates.failure-warning-style",label&&label->property("studio_message_role")=="warning");
            end();return;
        }
        if(!platesDialog_->hasResult())return;
        workflowCase("plates.generated-on-open",task&&platesDialog_->hasResult());
        if(platesRequest_["assembly"].toBool()){
            timer->stop();
            const auto before=platesDialog_->result();
            const auto beforeFile=workflowOutput_+"/before.dxf",afterFile=workflowOutput_+"/after.dxf";
            workflowCase("assembly.before-export",platesDialog_->exportFile(beforeFile,"dxf").isEmpty());
            const auto flatPng=workflowOutput_+"/unselected.png";
            workflowCase("assembly.unselected-png",platesDialog_->exportFile(flatPng,"png").isEmpty());
            PlateDepthBuffer depth;
            for(bool reverse:{false,true}){
                depth.clear(QSize(32,32),Qt::white);
                auto triangle=[&](bool slope){
                    depth.triangle({{1,1},slope?.6f:1.f,{}},{{30,1},slope?1.5f:1.f,{}},
                                   {{1,30},slope?.6f:1.f,{}},slope?qRgb(220,30,20):qRgb(10,60,200));
                };
                triangle(reverse);triangle(!reverse);
                workflowCase(reverse?"depth.reversed-order":"depth.crossing-surfaces",
                    depth.image.pixel(4,4)==qRgb(10,60,200)&&depth.image.pixel(23,3)==qRgb(220,30,20)&&
                    !depth.visible({4,4},.7f)&&depth.visible({23,3},1.4f));
            }
            QTimer::singleShot(0,this,[this]{
                auto* preview=qobject_cast<QDialog*>(frontStudioDialog());
                if(!preview){workflowCase("assembly.modal",false);return;}
                auto* canvas=preview->findChild<QWidget*>("PlateAssemblyCanvas");
                auto* combo=preview->findChild<QComboBox*>("PlateAssemblySeams");
                auto* info=preview->findChild<QLabel*>("PlateAssemblyMeasurement");
                auto* slider=preview->findChild<QSlider*>();auto* all=preview->findChild<QCheckBox*>();
                auto* play=preview->findChild<QPushButton*>();
                if(!canvas||!combo||!slider||!info||!all||!play){workflowCase("assembly.controls",false);preview->reject();return;}
                const int seam=combo->findText("Bottom (SB) ↔ Sides (SB)");
                workflowCase("assembly.starboard-chine",seam>0);
                workflowCase("assembly.port-chine",combo->findText("Bottom (P) ↔ Sides (P)")>0);
                workflowCase("assembly.no-cross-side-chine",combo->findText("Bottom (SB) ↔ Sides (P)")<0);
                // Sample the same projected vertices that are passed to the painter.
                // One new panel enters per step; every previously seated vertex stays put.
                bool sequential=true,anchored=true,rewind=true;
                QVector<QPolygonF> priorEnd,firstFrame;
                QImage strip(540*5,270*9,QImage::Format_RGB32);strip.fill(Qt::white);
                QPainter stripPainter(&strip);
                for(int stage=0;stage<5;++stage) {
                    slider->setValue(stage*200+18);
                    auto early=plateAssemblyScreenGeometry(canvas);
                    slider->setValue(stage*200+144);
                    auto late=plateAssemblyScreenGeometry(canvas);
                    int moving=0,visible=0;
                    for(int p=0;p<early.size();++p){
                        if(early[p].isEmpty())continue;
                        ++visible;if(early[p]!=late[p])++moving;
                        if(!priorEnd.isEmpty()&&!priorEnd[p].isEmpty())
                            anchored &= early[p]==priorEnd[p]&&late[p]==priorEnd[p];
                    }
                    sequential &= visible==stage+2&&moving==1;
                    slider->setValue(stage*200+180);
                    priorEnd=plateAssemblyScreenGeometry(canvas);
                    if(stage==0)firstFrame=early;
                    for(int sub=0;sub<9;++sub){
                        slider->setValue(stage==4&&sub==8?slider->maximum():stage*200+sub*24);
                        auto frame=canvas->grab().scaled(540,270,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                        frame.setDevicePixelRatio(1);
                        const int i=stage*9+sub;
                        stripPainter.drawPixmap((i%5)*540+(540-frame.width())/2,(i/5)*270+(270-frame.height())/2,frame);
                    }
                }
                stripPainter.end();strip.save(workflowOutput_+"/views/sequence-frames.png");
                slider->setValue(18);rewind=plateAssemblyScreenGeometry(canvas)==firstFrame;
                workflowCase("assembly.one-panel-per-step",sequential);
                workflowCase("assembly.installed-vertices-screen-stationary",anchored);
                workflowCase("assembly.scrub-restores-stage",rewind);
                bool hit=false,hoverVisible=false;
                // Include the entire hull when searching its selectable boundary.
                for(int y=4;y<canvas->height() && !hit;y+=4) {
                    slider->setValue(270); // interacting while the starboard chine is exposed
                    QPointF pt(canvas->width()/2.,y);
                    const int previousChoice=combo->currentIndex();
                    QMouseEvent move(QEvent::MouseMove,pt,QPointF(canvas->mapToGlobal(pt.toPoint())),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(canvas,&move);
                    const auto region=canvas->grab(QRect(pt.toPoint()-QPoint(9,9),QSize(19,19))).toImage();
                    bool localHover=false;
                    for(int row=0;row<region.height();++row)for(int col=0;col<region.width();++col)
                        localHover|=region.pixelColor(col,row)==plateHoverColor;
                    const bool hoverOnly=localHover&&combo->currentIndex()==previousChoice;
                    for(auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
                        QMouseEvent ev(type,pt,QPointF(canvas->mapToGlobal(pt.toPoint())),Qt::LeftButton,
                            type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
                        QApplication::sendEvent(canvas,&ev);
                    }
                    hit=seam>0&&combo->currentIndex()==seam;
                    if(hit)hoverVisible=hoverOnly;else combo->setCurrentIndex(0);
                }
                workflowCase("assembly.click-pairs-two-plates",hit&&!all->isChecked());
                workflowCase("assembly.preview-hover-before-click",hoverVisible);
                const auto measurement=info->text();workflowArtifacts_["measurement"]=measurement;
                workflowCase("assembly.millimetres",measurement.contains("4363.19")&&measurement.contains("4362.17")&&measurement.contains("1.02"));
                preview->grab().save(workflowOutput_+"/views/pair-flat.png");
                play->click();QEventLoop tick;QTimer::singleShot(240,&tick,&QEventLoop::quit);tick.exec();play->click();
                workflowCase("assembly.play-pause",slider->value()>0&&slider->value()<1000);
                const auto paused=slider->value();QEventLoop wait;QTimer::singleShot(65,&wait,&QEventLoop::quit);wait.exec();
                workflowCase("assembly.paused-position",slider->value()==paused);
                slider->setValue(770);preview->grab().save(workflowOutput_+"/views/pair-closing.png");
                const auto pairFrame=canvas->grab().toImage();
                slider->setValue(1000);
                workflowCase("assembly.measurement-retained",info->text()==measurement);
                all->setChecked(true);preview->grab().save(workflowOutput_+"/views/whole-hull.png");
                const auto wholeFrame=canvas->grab().toImage();
                workflowCase("assembly.scrub-and-whole-hull",wholeFrame!=pairFrame);
                const QPointF start=canvas->rect().center(),finish=start+QPointF(95,30);
                QMouseEvent press(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QMouseEvent drag(QEvent::MouseMove,finish,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease,finish,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                for(auto* event:{&press,&drag,&release})QApplication::sendEvent(canvas,event);
                workflowCase("assembly.rotate-keeps-seam",canvas->grab().toImage()!=wholeFrame&&combo->currentIndex()==seam);
                preview->reject();
            });
            platesDialog_->previewAssembly();
            auto* sheet=platesDialog_->findChild<QGraphicsView*>();
            auto* message=platesDialog_->findChild<QLabel*>("PlateStatus");
            auto hasPair=[&]{return message&&message->text().contains("Bottom (SB)")&&message->text().contains("Sides (SB)")&&message->text().contains("1.02");};
            workflowCase("assembly.preview-selection-to-sheet",hasPair());
            if(sheet&&message){
                auto sendMouse=[&](QEvent::Type type,QPoint pos,Qt::MouseButtons buttons,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
                    QMouseEvent event(type,QPointF(pos),QPointF(sheet->viewport()->mapToGlobal(pos)),
                        type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,buttons,modifiers);
                    QApplication::sendEvent(sheet->viewport(),&event);
                };
                auto tap=[&](QPoint pos){
                    sendMouse(QEvent::MouseButtonPress,pos,Qt::LeftButton);
                    sendMouse(QEvent::MouseButtonRelease,pos,Qt::NoButton);
                };
                auto* layers=platesDialog_->list_;
                auto cleared=[&]{return layers->currentRow()<0&&sheet->scene()->selectedItems().isEmpty();};
                auto locatePair=[&]{
                    for(int y=2;y<sheet->viewport()->height()&&!hasPair();y+=2)tap({sheet->viewport()->width()/2,y});
                };
                tap({2,2});
                workflowCase("assembly.sheet-clears-selection",!hasPair());
                locatePair();
                workflowCase("assembly.sheet-clicks-paired-edge",hasPair()&&cleared());
                if(hasPair()){
                    const int index=platesDialog_->seam_;
                    const auto& pair=platesDialog_->seams_[index];
                    const auto& plates=platesDialog_->data_->plates;
                    bool corresponding=pair.longitudinal&&pair.ends.size()==2;
                    for(const auto [a,b]:pair.ends)
                        corresponding &= plates[pair.left].original[a]==plates[pair.right].original[b];
                    workflowCase("assembly.corresponding-ends",corresponding&&
                        plates[pair.left].original[pair.ends.first().first].x()<plates[pair.left].original[pair.ends.last().first].x());
                    auto sample=[&](int plate,bool right){
                        const auto& model=platesDialog_->data_->plates[plate];
                        const auto indices=pair.segments[pair.segments.size()/2];
                        const auto& edge=model.edges[right?indices.second:indices.first];
                        const QPointF mid=(model.points[edge.a]+model.points[edge.b])/2;
                        return sheet->mapFromScene(platesDialog_->items_[plate]->mapToScene({mid.x(),-mid.y()}));
                    };
                    const auto left=sample(pair.left,false),right=sample(pair.right,true);
                    auto nearColor=[](const QImage& img,QPoint logical,QColor color){
                        const QPoint pt=(logical*img.devicePixelRatio());
                        for(int y=pt.y()-2;y<=pt.y()+2;++y)for(int x=pt.x()-2;x<=pt.x()+2;++x)
                            if(img.valid(x,y)&&img.pixelColor(x,y)==color)return true;
                        return false;
                    };
                    auto image=sheet->viewport()->grab().toImage();
                    workflowCase("assembly.sheet-both-strokes",left!=right&&nearColor(image,left,plateSeamColor)&&nearColor(image,right,plateSeamColor));
                    image.save(workflowOutput_+"/views/sheet-seam.png");
                    tap({2,2});sendMouse(QEvent::MouseMove,left,Qt::NoButton);
                    image=sheet->viewport()->grab().toImage();
                    workflowCase("assembly.hover-local-only",platesDialog_->hovered_.seam==index&&platesDialog_->seam_<0&&cleared()&&
                        nearColor(image,left,plateHoverColor)&&!nearColor(image,right,plateHoverColor));
                    sendMouse(QEvent::MouseMove,{2,2},Qt::NoButton);
                    workflowCase("assembly.leave-edge",!platesDialog_->hovered_.valid());
                    // Probe both sides of the visible outline, then a point in the body.
                    auto* item=platesDialog_->items_[pair.left];
                    QPoint inside,outside,body;bool foundIn=false,foundOut=false,foundBody=false;
                    for(int d=1;d<35;++d)for(int sign:{-1,1}){
                        const QPoint pos=left+QPoint(0,sign*d);
                        const bool within=item->contains(item->mapFromScene(sheet->mapToScene(pos)));
                        const auto hit=platesDialog_->edgeAt(pos);
                        if(within&&!foundBody&&!hit.valid()){body=pos;foundBody=true;}
                        if(hit.plate!=pair.left||hit.seam!=index)continue;
                        if(within&&!foundIn){inside=pos;foundIn=true;}
                        if(!within&&!foundOut){outside=pos;foundOut=true;}
                    }
                    if(foundBody)tap(body);
                    workflowCase("assembly.body-selects-only-plate",foundBody&&layers->currentRow()==pair.left&&
                        item->isSelected()&&platesDialog_->seam_<0);
                    if(foundIn)tap(inside);
                    workflowCase("assembly.inside-rim-exclusive",foundIn&&cleared()&&hasPair());
                    tap({2,2});
                    if(foundOut)tap(outside);
                    workflowCase("assembly.outside-rim-exclusive",foundOut&&cleared()&&hasPair());
                    tap({2,2});const auto original=item->pos();
                    sendMouse(QEvent::MouseButtonPress,left,Qt::LeftButton);
                    const auto displaced=left+QPoint(2*QApplication::startDragDistance(),5);
                    sendMouse(QEvent::MouseMove,displaced,Qt::LeftButton);
                    sendMouse(QEvent::MouseButtonRelease,displaced,Qt::NoButton);
                    workflowCase("assembly.edge-drag-never-moves-plate",cleared()&&platesDialog_->seam_<0&&item->pos()==original);
                }
                const auto bottoms=layers->findItems("Bottom (SB)",Qt::MatchExactly);
                if(!bottoms.isEmpty()){
                    layers->setCurrentItem(bottoms.front());
                    const int row=layers->currentRow();
                    auto* item=platesDialog_->items_[row];
                    auto& plate=platesDialog_->data_->plates[row];
                    const double angle=plate.rotation;const bool flipped=plate.mirrored;
                    const QPointF localCenter(plate.bounds.center().x(),-plate.bounds.center().y());
                    const QPointF center=item->mapToScene(localCenter);
                    const auto offset=plate.offset;
                    auto* presets=platesDialog_->rotationPresets_;
                    workflowCase("assembly.presets-initially-hidden",presets->isHidden());
                    auto dragSide=[&](int side){
                        const QPoint start=sheet->mapFromScene(item->mapToScene(localCenter+QPointF(side*plate.bounds.width()*.20,0)));
                        const QPoint finish=start+QPoint(0,18);
                        sendMouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::ControlModifier);
                        sendMouse(QEvent::MouseMove,finish,Qt::LeftButton,Qt::ControlModifier);
                        sendMouse(QEvent::MouseButtonRelease,finish,Qt::NoButton,Qt::ControlModifier);
                        return plate.rotation-angle;
                    };
                    const auto leftAngle=dragSide(-1);
                    plate.rotation=angle;platesDialog_->placePlate(row);
                    const auto rightAngle=dragSide(1);
                    workflowCase("assembly.ctrl-rotate-opposite-sides",leftAngle>0&&rightAngle<0);
                    workflowCase("assembly.ctrl-rotate-fixed-center",QLineF(item->mapToScene(localCenter),center).length()<1e-8&&plate.offset==offset);
                    const auto rotated=plate.rotation;
                    bool exact=presets->isVisibleTo(platesDialog_);
                    const auto buttons=presets->findChildren<QPushButton*>();
                    for(auto* button:buttons){
                        const auto value=button->property("plate_angle").toInt();
                        button->click();button->click();
                        exact &= plate.rotation==value&&plate.offset==offset&&
                            QLineF(item->mapToScene(localCenter),center).length()<1e-8;
                    }
                    workflowCase("assembly.presets-set-absolute-angle",buttons.size()==6&&exact);
                    platesDialog_->grab().save(workflowOutput_+"/views/rotation-presets.png");
                    plate.rotation=rotated;platesDialog_->placePlate(row);
                    const auto rightMouse=[&](QEvent::Type type,QPoint pos,Qt::MouseButtons held){
                        QMouseEvent event(type,pos,sheet->viewport()->mapToGlobal(pos),type==QEvent::MouseMove?Qt::NoButton:Qt::RightButton,held,Qt::NoModifier);
                        QApplication::sendEvent(sheet->viewport(),&event);
                    };
                    const auto activeMenu=[&]()->QMenu*{
                        auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
                        return menu&&menu->objectName()=="PlateContextMenu"?menu:nullptr;
                    };
                    const auto flipAt=[&](QPoint pos,bool capture=false){
                        rightMouse(QEvent::MouseButtonPress,pos,Qt::RightButton);
                        rightMouse(QEvent::MouseButtonRelease,pos,Qt::NoButton);
                        auto* menu=activeMenu();
                        if(!menu)return false;
                        const auto actions=menu->actions();
                        const bool ok=actions.size()==1&&actions.front()->property("plate_operation")=="mirror";
                        if(ok){
                            if(capture)menu->grab().save(workflowOutput_+"/views/plate-mirror-menu.png");
                            actions.front()->trigger();
                        }
                        menu->hide();return ok;
                    };
                    const auto transformBefore=item->transform();
                    bool noOldControl=true;for(auto* field:platesDialog_->findChildren<QCheckBox*>())noOldControl&=field->property("plate_field")!="mirror";
                    const bool selectedFlip=flipAt({2,2})&&plate.mirrored!=flipped&&item->transform()!=transformBefore;
                    workflowCase("assembly.mirror-selected-plate",noOldControl&&selectedFlip&&plate.offset==offset&&plate.rotation==rotated);
                    const bool restored=flipAt({2,2})&&plate.mirrored==flipped&&item->transform()==transformBefore;
                    platesDialog_->fit();
                    // Edge inspection clears the plate selection, unlike a blank
                    // canvas click which may preserve the sidebar's current row.
                    for(int y=2;y<sheet->viewport()->height()&&!cleared();y+=2){
                        const QPoint at(sheet->viewport()->width()/2,y);
                        if(platesDialog_->edgeAt(at).valid())tap(at);
                    }
                    const bool unselected=cleared();
                    QPoint mirrorPoint(-1,-1);
                    const auto area=sheet->mapFromScene(item->sceneBoundingRect()).boundingRect().intersected(sheet->viewport()->rect());
                    for(int y=area.top();y<=area.bottom()&&mirrorPoint.x()<0;y+=3)
                        for(int x=area.left();x<=area.right();x+=3)
                            if(platesDialog_->plateAt({x,y})==row&&item->contains(item->mapFromScene(sheet->mapToScene(QPoint(x,y))))){mirrorPoint={x,y};break;}
                    const bool directMenu=mirrorPoint.x()>=0&&flipAt(mirrorPoint,true);
                    const bool directFlip=unselected&&directMenu&&plate.mirrored!=flipped&&layers->currentRow()==row;
                    workflowCase("assembly.mirror-direct-hit",restored&&directFlip,
                        QStringLiteral("restored=%1 unselected=%2 point=(%3,%4) menu=%5 row=%6 target=%7")
                            .arg(restored).arg(unselected).arg(mirrorPoint.x()).arg(mirrorPoint.y()).arg(directMenu).arg(layers->currentRow()).arg(row));
                    const auto cameraOrigin=sheet->mapToScene(QPoint());
                    const bool mirrorBeforePan=plate.mirrored;
                    rightMouse(QEvent::MouseButtonPress,{2,2},Qt::RightButton);
                    rightMouse(QEvent::MouseMove,{44,35},Qt::RightButton);
                    rightMouse(QEvent::MouseButtonRelease,{44,35},Qt::NoButton);
                    const bool panned=sheet->mapToScene(QPoint())!=cameraOrigin;
                    workflowCase("assembly.right-drag-pans-without-menu",panned&&!activeMenu()&&plate.mirrored==mirrorBeforePan);
                    rightMouse(QEvent::MouseButtonPress,{44,35},Qt::RightButton);
                    rightMouse(QEvent::MouseMove,{2,2},Qt::RightButton);
                    rightMouse(QEvent::MouseButtonRelease,{2,2},Qt::NoButton);
                    tap({2,2});
                    workflowCase("assembly.presets-dismissed",presets->isHidden());
                    bool foundTransformed=false;
                    for(int i=0;i<platesDialog_->seams_.size();++i){
                        const auto& pair=platesDialog_->seams_[i];
                        const auto& plates=platesDialog_->data_->plates;
                        if(plates[pair.left].name!="Bottom (SB)"||plates[pair.right].name!="Sides (SB)")continue;
                        const auto& bottom=plates[pair.left];
                        const auto& e=bottom.edges[pair.segments[pair.segments.size()/2].first];
                        const auto mid=(bottom.points[e.a]+bottom.points[e.b])/2;
                        tap(sheet->mapFromScene(platesDialog_->items_[pair.left]->mapToScene({mid.x(),-mid.y()})));
                        foundTransformed=platesDialog_->seam_==i&&hasPair();
                        break;
                    }
                    workflowCase("assembly.transformed-sheet-pick",foundTransformed);
                    if(!bottoms.isEmpty())layers->setCurrentItem(bottoms.front());
                    plate.rotation=angle;plate.mirrored=flipped;platesDialog_->placePlate(row);locatePair();
                }
                QTimer::singleShot(0,this,[this]{
                    auto* d=qobject_cast<QDialog*>(frontStudioDialog());
                    if(!d){workflowCase("assembly.sheet-selection-to-preview",false);return;}
                    // Refreshing the sheet can write its implicitly shared plate array
                    // while the preview snapshot is still alive.
                    platesDialog_->data_->plates.detach();
                    auto* choice=d->findChild<QComboBox*>("PlateAssemblySeams");
                    workflowCase("assembly.sheet-selection-to-preview",choice&&choice->currentText()=="Bottom (SB) ↔ Sides (SB)");
                    d->reject();
                });platesDialog_->previewAssembly();
                auto highlighted=workflowOutput_+"/selected.png";
                workflowCase("assembly.print-excludes-highlights",platesDialog_->exportFile(highlighted,"png").isEmpty()&&QImage(flatPng)==QImage(highlighted));
            }
            workflowCase("assembly.layout-preserved",before==platesDialog_->result());
            workflowCase("assembly.after-export",platesDialog_->exportFile(afterFile,"dxf").isEmpty());
            QFile a(beforeFile),b(afterFile);
            workflowCase("assembly.dxf-unchanged",a.open(QIODevice::ReadOnly)&&b.open(QIODevice::ReadOnly)&&a.readAll()==b.readAll());
            end();return;
        }
        if(platesRequest_["verify_names"].toBool()){
            const auto originalLanguage=uiLanguage();
            QJsonObject observed;
            for(const auto& language:QStringList{"en","zh","ru","ja","de","fr","es"}){
                if(!setUiLanguage(language).isEmpty()){finishWorkflow("Plate language catalog unavailable");return;}
                QJsonArray rows,canvas;
                if(auto* list=platesDialog_->findChild<QListWidget*>())
                    for(int i=0;i<list->count();++i)rows.append(list->item(i)->text());
                if(auto* view=platesDialog_->findChild<QGraphicsView*>())
                    for(auto* item:view->scene()->items())
                        if(auto* label=dynamic_cast<QGraphicsSimpleTextItem*>(item);label&&label->isVisible())canvas.append(label->text());
                observed[language]=QJsonObject{{"list",rows},{"canvas",canvas}};
            }
            setUiLanguage(originalLanguage);
            workflowArtifacts_["plate_names_by_language"]=observed;
            end();return;
        }
        if(!workflowArtifacts_.contains("plate_message_styles")){
            QJsonArray messages;for(auto* label:platesDialog_->findChildren<QLabel*>())if(label->property("studio_message_role").isValid())messages.append(QJsonObject{{"role",label->property("studio_message_role").toString()},{"text",label->text()}});
            auto* status=platesDialog_->findChild<QLabel*>("PlateStatus");auto* table=platesDialog_->findChild<QTableWidget*>("PlateMetrics");QJsonArray cells;
            if(table)for(int row=0;row<table->rowCount();++row){QJsonArray values;for(int col=0;col<table->columnCount();++col)values.append(table->item(row,col)->text());cells.append(values);}
            workflowArtifacts_["plate_message_styles"]=QJsonObject{{"messages",messages},{"metrics",cells}};
            workflowCase("plates.success-feedback-style",status&&status->property("studio_message_role")=="feedback");
            workflowCase("plates.metrics-are-result-table",table&&table->columnCount()==3&&table->property("studio_content_role")=="result_data");
            const auto& metric=platesDialog_->data_->plates[platesDialog_->list_->currentRow()].metrics;
            const double expected=100*metric["signed_area_error"].toDouble()/metric["reference_area"].toDouble();
            bool percentMatches=false;
            if(table)for(int row=0;row<table->rowCount();++row)if(table->item(row,2)->text()=="%"){
                bool numeric=false;
                const double fullValue=table->item(row,1)->toolTip().toDouble(&numeric);
                percentMatches=numeric&&std::abs(fullValue-expected)<1e-12;
            }
            workflowCase("plates.relative-area-matches-developed-minus-original",percentMatches);
            workflowCase("plates.explanation-separate-from-feedback",std::any_of(messages.begin(),messages.end(),[](const QJsonValue& value){return value.toObject()["role"]=="explanation";}));
        }
        if(platesRequest_["drag_benchmark"].toBool()){
            workflowArtifacts_["drag_measurement"]=platesDialog_->measureDrag();
            for(const auto* format:{"json","dxf","txt","png","pdf"})workflowCase("drag.export."+QString(format),platesDialog_->exportFile(workflowOutput_+"/drag."+format,format).isEmpty());
            end();return;
        }
        workflowCase("plates.generated",!platesRequest_["expect_failure"].toBool());workflowArtifacts_["raw_plate_data"]=platesDialog_->result();
        // Exercise the export fixture at its historical placement; pointer rotation
        // and ordinary dragging are exercised by the interaction workflows.
        if(!platesRequest_["pdf_default_layout"].toBool()){
            auto& firstPlate=platesDialog_->data_->plates[0];
            firstPlate.rotation=37;firstPlate.offset={1.25,-2.5};firstPlate.mirrored=true;platesDialog_->placePlate(0);
            const auto layout=platesDialog_->result()["layout"].toArray()[0].toObject();
            workflowCase("plates.transform",layout["rotation"]==37&&layout["mirrored"].toBool());
        }
        workflowArtifacts_["plate_data"]=platesDialog_->result();
        auto before=platesDialog_->result();for(auto*v:platesDialog_->findChildren<QGraphicsView*>()){v->scale(15,15);v->translate(350,-450);}
        workflowCase("plates.camera-readonly",platesDialog_->result()==before);
        platesDialog_->fit();platesDialog_->grab().save(workflowOutput_+"/views/plates.png");
        // Show the result/notice area, not just the controls at the top of the
        // sidebar. Only this private dialog's scroll position is changed.
        if(auto* table=platesDialog_->findChild<QTableWidget*>("PlateMetrics"))for(auto* scroll:platesDialog_->findChildren<QScrollArea*>())
            if(scroll->widget()&&scroll->widget()->isAncestorOf(table)){
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                const int previous=scroll->verticalScrollBar()->value();scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                platesDialog_->grab().save(workflowOutput_+"/views/plate-messages.png");
                QJsonArray widths;for(int col=0;col<table->columnCount();++col)widths.append(table->columnWidth(col));
                workflowArtifacts_["plate_metric_layout"]=QJsonObject{{"width",table->width()},{"height",table->height()},{"columns",widths},
                    {"horizontal_scroll",table->horizontalScrollBar()->maximum()},{"vertical_scroll",table->verticalScrollBar()->maximum()}};
                scroll->verticalScrollBar()->setValue(previous);break;
            }
        bool outsideReadable=true;
        for(auto*v:platesDialog_->findChildren<QGraphicsView*>())for(auto*i:v->scene()->items())if(auto*label=dynamic_cast<QGraphicsSimpleTextItem*>(i))
            if(label->parentItem()&&!label->parentItem()->shape().contains(label->pos()))outsideReadable&=label->brush().color().lightness()<128;
        workflowCase("plates.labels-outside-fill-readable",outsideReadable);
        for(const auto* format:{"dxf","txt","json","png","pdf"}){auto path=workflowOutput_+"/plates."+format;auto error=platesDialog_->exportFile(path,format);workflowCase("plates.export."+QString(format),error.isEmpty()&&QFileInfo(path).size()>0,error);}
        // Printing at a declared physical scale is checked independently by the
        // PDF reader, including feet-to-mm conversion and page tiling.
        QTimer::singleShot(0,this,[this]{
            auto* dialog=frontStudioDialog();
            if(!dialog||dialog->objectName()!="PlatePdfSettings"){
                workflowCase("plates.pdf-scale-fields",false);return;
            }
            auto* fit=dialog->findChild<QCheckBox*>("PdfFitSinglePage");
            auto* a=dialog->findChild<QDoubleSpinBox*>("PlatePdfNumerator");
            auto* b=dialog->findChild<QDoubleSpinBox*>("PlatePdfDenominator");
            const bool fitted=fit&&fit->isChecked()&&a&&b&&!a->isVisible()&&!b->isVisible();
            auto* preview=dialog->findChild<QWidget*>("PdfFirstPagePreview");
            if(fitted&&preview){
                auto* buttons=dialog->findChild<QDialogButtonBox*>();
                auto* warning=dialog->findChild<QLabel*>("PlatePdfWarning");
                auto* ratio=dialog->findChild<QLabel*>("PlatePdfComputedRatio");
                const bool preparing=buttons&&!buttons->button(QDialogButtonBox::Ok)->isEnabled();
                fit->setChecked(false);
                const auto step=[&](QStyle::SubControl arrow){
                    QStyleOptionSpinBox option;option.initFrom(b);option.frame=b->hasFrame();
                    option.buttonSymbols=b->buttonSymbols();
                    const auto rect=b->style()->subControlRect(QStyle::CC_SpinBox,&option,arrow,b);
                    if(rect.isEmpty()||!b->rect().contains(rect.center()))return false;
                    const auto pos=rect.center();
                    for(const auto eventType:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
                        QMouseEvent event(eventType,pos,b->mapToGlobal(pos),Qt::LeftButton,
                            eventType==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
                        QApplication::sendEvent(b,&event);
                    }
                    return true;
                };
                const double saved=b->value();b->setValue(2.5);
                const bool editWhilePreparing=preparing&&a->isVisible()&&a->isEnabled()&&b->isVisible()&&b->isEnabled()&&
                    step(QStyle::SC_SpinBoxUp)&&b->value()==3.5&&step(QStyle::SC_SpinBoxDown)&&b->value()==2.5;
                b->setValue(saved);
                fit->setChecked(true);
                workflowCase("plates.pdf-settings-responsive-before-preview",editWhilePreparing);
                workflowCase("plates.pdf-warning-outside-preview",warning&&warning->isVisible()&&!preview->isAncestorOf(warning));
                workflowCase("plates.pdf-fit-exposes-computed-ratio",ratio&&ratio->isVisible()&&!ratio->text().isEmpty());
                if(ratio)workflowArtifacts_["plate_pdf_computed_ratio"]=ratio->locale().toDouble(ratio->text().section(':',1).trimmed());
                QElapsedTimer wait;wait.start();
                while(buttons&&!buttons->button(QDialogButtonBox::Ok)->isEnabled()&&wait.elapsed()<3000)waitForPdfPreview(10);
                dialog->grab().save(workflowOutput_+"/plate-pdf-fit-settings.png");
                b->setValue(150.5);a->setValue(1.5);
                workflowCase("plates.pdf-scale-preview-animates",observePdfPreviewMotion(dialog,[&]{fit->setChecked(false);}));
                dialog->grab().save(workflowOutput_+"/plate-pdf-fixed-settings.png");
            }
            const bool fixed=a&&b&&a->isVisible()&&b->isVisible()&&a->buttonSymbols()==QAbstractSpinBox::UpDownArrows&&b->buttonSymbols()==QAbstractSpinBox::UpDownArrows;
            workflowCase("plates.pdf-scale-fields",fitted&&fixed&&preview&&preview->isVisible());
            if(auto* box=dialog->findChild<QDialogButtonBox*>())box->button(QDialogButtonBox::Ok)->click();
            if(dialog->isVisible())dialog->reject();
        });
        std::shared_ptr<PlatesDialog::PdfSheet> settings;
        const bool accepted=platesDialog_->selectPdfSettings(settings);
        workflowCase("plates.scaled-pdf",accepted&&platesDialog_->exportFile(workflowOutput_+"/scaled.pdf","pdf").isEmpty());
        platesDialog_->pdfNumerator_=1;platesDialog_->pdfDenominator_=10;
        workflowCase("plates.tiled-pdf",platesDialog_->exportFile(workflowOutput_+"/tiled.pdf","pdf").isEmpty());
        platesDialog_->pdfNumerator_=1000;platesDialog_->pdfDenominator_=1;
        auto path=workflowOutput_+"/failed-print.pdf";
        QFile keep(path);keep.open(QIODevice::WriteOnly);keep.write("previous");keep.close();
        const auto oversizeError=platesDialog_->exportFile(path,"pdf");keep.open(QIODevice::ReadOnly);
        workflowCase("plates.excess-pages-preserve-file",!oversizeError.isEmpty()&&keep.readAll()=="previous");
        keep.close();
        platesDialog_->pdfNumerator_=1;platesDialog_->pdfDenominator_=0;
        auto lists=platesDialog_->findChildren<QListWidget*>();if(lists.size()==1){
            auto*list=lists[0];list->item(0)->setCheckState(Qt::Unchecked);workflowArtifacts_["hidden_plate_data"]=platesDialog_->result();
            const auto error=platesDialog_->exportFile(workflowOutput_+"/hidden.dxf","dxf");
            workflowCase("plates.hidden-export",list->count()==1?!error.isEmpty():error.isEmpty());
            list->item(0)->setCheckState(Qt::Checked);
        }
        auto protectedPath=workflowOutput_+"/protected.txt";QFile f(protectedPath);f.open(QIODevice::WriteOnly);f.write("keep");f.close();auto error=platesDialog_->exportFile(protectedPath,"bad");f.open(QIODevice::ReadOnly);workflowCase("plates.failed-export-preserves-file",!error.isEmpty()&&f.readAll()=="keep");f.close();
        if(platesRequest_["cancel"].toBool()){
            if(task){
                auto* opened=platesDialog_;
                const auto before=opened->result();
                platesWorkflow_=false;
                opened->reject();
                platesWorkflow_=true;
                openPlates();
                workflowCase("plates.reopen-keeps-current-layout",platesDialog_==opened&&!task->busy()&&opened->result()==before);
                // Diagnostic workflow only: hold Qt dispatch until the real
                // worker publishes its result, then cancel the pending UI
                // request. This makes the late-success race reproducible.
                const QDir workspace(QFileInfo(QFileInfo(task->modelPath()).absolutePath()).absolutePath());
                const auto previous=workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot);
                if(!task->developPlates()){workflowCase("plates.diagnostic-restart",false);end();return;}
                *stage=2;
                if(platesRequest_["late_cancel"].toBool()){
                    QElapsedTimer deadline;deadline.start();bool produced=false;
                    while(!produced && deadline.elapsed()<3000){
                        for(const auto &name:workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot))
                            if(!previous.contains(name) && QFileInfo::exists(workspace.filePath(name+"/plates.json")))produced=true;
                        if(!produced)QThread::msleep(1);
                    }
                    workflowCase("plates.late-result-produced-before-cancel",produced&&task->busy());
                }
                task->cancelPlates();return;
            }
        }
        end();
    });timer->start();candidates[0]->trigger();
}
