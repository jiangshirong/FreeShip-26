// Production layout lifecycle observations on the unified non-input desktop.
#include "main_window.h"
#include "studio_caption.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QMap>
#include <QMouseEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

void MainWindow::auditViewReactivation(){
    const auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    const auto rect=[](QRect r){return QJsonArray{r.x(),r.y(),r.width(),r.height()};};
    const auto original=session_.model();const bool modified=session_.modified();
    auto* container=viewGrid_->parentWidget();QJsonArray samples;const auto identities=viewFrames_;
    QDir().mkpath(workflowOutput_+"/views");
    const auto capture=[&](QString phase){
        QJsonArray frames;QRect occupied;bool correct=true;
        for(int i=0;i<4;++i){auto* frame=viewFrames_[i];int row=0,col=0,rs=0,cs=0;
            viewGrid_->getItemPosition(viewGrid_->indexOf(frame),&row,&col,&rs,&cs);
            const auto expected=viewGrid_->cellRect(row,col),actual=frame->geometry();
            correct&=frame->isVisible()&&actual==expected&&frame==identities[i];occupied=occupied.united(actual);
            frames.append(QJsonObject{{"projection",i},{"actual",rect(actual)},{"expected",rect(expected)}});
        }
        workflowCase("view-layout."+phase,correct&&occupied==container->contentsRect()&&viewGrid_->isEnabled());
        samples.append(QJsonObject{{"phase",phase},{"container",rect(container->contentsRect())},{"occupied",rect(occupied)},{"frames",frames},{"layout_enabled",viewGrid_->isEnabled()}});
        grab().save(workflowOutput_+"/views/"+phase+".png");
    };
    showStudioMainWindow(this);wait(300);capture("initial-maximized");
    QDialog peer(nullptr,Qt::Window);peer.setWindowTitle("Private activation peer");peer.resize(450,240);
    for(int i=0;i<3;++i){
        peer.show();peer.raise();peer.activateWindow();
#ifdef Q_OS_WIN
        DWORD ourPid=0,peerPid=0;const auto ownHwnd=reinterpret_cast<HWND>(winId()),peerHwnd=reinterpret_cast<HWND>(peer.winId());
        GetWindowThreadProcessId(ownHwnd,&ourPid);GetWindowThreadProcessId(peerHwnd,&peerPid);
        workflowCase("view-layout.activation-owned-handles-"+QString::number(i),ourPid==GetCurrentProcessId()&&peerPid==ourPid);
        SetActiveWindow(peerHwnd);
#endif
        wait(45);raise();activateWindow();
#ifdef Q_OS_WIN
        SetActiveWindow(ownHwnd);
#endif
        peer.hide();wait(90);capture("reactivated-"+QString::number(i));
    }
    showMinimized();wait(60);showMaximized();wait(180);capture("minimize-restore");
    showNormal();resize(1300,820);wait(100);capture("normal-resize");
    hide();wait(30);show();wait(120);capture("hide-show");
    viewActions_[3]->trigger();wait(45);resize(1400,880);wait(270);viewActions_[3]->trigger();wait(270);capture("toggle-resize-return");
    showMaximized();wait(150);capture("maximized-again");
    workflowCase("view-layout.document-unmodified",session_.model()==original&&session_.modified()==modified);
    workflowArtifacts_["view_layout_lifecycle"]=samples;
}

void MainWindow::auditViewGridResize(bool frameTiming,bool shaded){
    const auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    auto* canvas=viewGrid_->parentWidget();const auto original=session_.model();const bool modified=session_.modified();
    QJsonArray samples;QDir().mkpath(workflowOutput_+"/views");
    if(shaded){
        if(!original)throw std::runtime_error("Shaded divider replay requires a model");
        showStudioMainWindow(this);
        tabs_->setCurrentIndex(0);
        for(auto* view:views_)view->finishPresentation();
        wait(280);
        QWidget* handle=nullptr;
        for(auto* child:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
            if(child->property("studio_view_divider_axis")=="xy"&&child->isVisible())handle=child;
        if(!handle)throw std::runtime_error("Shaded divider replay requires visible four-view layout");
        const auto send=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,handle->mapFrom(canvas,at),canvas->mapToGlobal(at),button,buttons,Qt::NoModifier);
            QApplication::sendEvent(handle,&event);
        };
        const auto renders=[&]{
            int total=0;for(auto* view:views_)total+=view->viewSettingsEvidence()["fill_renders"].toInt();
            return total;
        };
        const auto anchors=[&]{
            QVector<QPointF> pixels;for(auto* view:views_)pixels<<view->project(original->minimum)<<view->project(original->maximum);
            return pixels;
        };
        const std::array<QString,4> modes{"fill","shade_1","shade_2","shade_3"};
        QJsonObject modesEvidence;
        for(const auto& mode:modes){
            for(auto* view:views_){
                if(!view->selectDisplayMode(mode))throw std::runtime_error("Display mode unavailable");
                view->fit();
            }
            wait(280);
            const QPoint start=handle->geometry().center();
            const int shadeBefore=renders();
            const QPoint excursion(std::max(12,canvas->width()/13),std::max(8,canvas->height()/16));
            send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            QJsonArray timings;
            bool complete=true,tracks=true;
            QPoint at=start;
            for(int step=1;step<=24;++step){
                at=start+(step<=12?excursion*step/12:excursion-excursion*(step-12)/18);
                std::array<quint64,4> paints;
                for(int j=0;j<4;++j)paints[j]=views_[j]->paintCount;
                QElapsedTimer elapsed;elapsed.start();
                send(QEvent::MouseMove,at,Qt::NoButton,Qt::LeftButton);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                timings.append(elapsed.nsecsElapsed()/1e6);
                for(int j=0;j<4;++j)complete&=paints[j]!=views_[j]->paintCount;
                tracks&=(handle->geometry().center()-at).manhattanLength()<=4;
            }
            const auto beforeRelease=anchors();
            const int duringRenders=renders()-shadeBefore;
            QElapsedTimer release;release.start();
            send(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const double releaseMs=release.nsecsElapsed()/1e6;
            const bool sameProjection=beforeRelease==anchors();
            QJsonArray finalImages,legends;
            for(int j=0;j<4;++j){
                auto* view=views_[j];
                const auto path=workflowOutput_+"/views/shaded-divider-"+mode+"-"+QString::number(j)+".png";
                legends.append(view->viewSettingsEvidence()["shade_legend"]);
                if(!view->image(view->devicePixelRatioF()).save(path))throw std::runtime_error("Cannot capture settled shade");
                finalImages.append(path);
            }
            modesEvidence[mode]=QJsonObject{{"frame_ms",timings},{"fill_renders",duringRenders},
                {"release_ms",releaseMs},{"final_images",finalImages},{"legends",legends}};
            workflowCase("shaded-divider."+mode+".layout",complete&&tracks&&sameProjection);
        }
        workflowCase("shaded-divider.document-unmodified",session_.model()==original&&session_.modified()==modified);
        workflowArtifacts_["shaded_divider"]=QJsonObject{{"modes",modesEvidence},
            {"control_faces",int(original->faces.size())},{"triangles",int(original->triangles.size())},
            {"dpr",views_[0]->devicePixelRatioF()}};
        return;
    }
    if(exchangeWorkflow_){
        if(!original)throw std::runtime_error("The viewport measurement requires an imported model");
        showStudioMainWindow(this);
        tabs_->setCurrentIndex(0);
        for(auto* view:views_)view->finishPresentation();
        wait(250);
        QWidget* handle=nullptr;
        for(auto* child:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
            if(child->property("studio_view_divider_axis")=="xy"&&child->isVisible())handle=child;
        workflowCase("dense-viewport.center-handle",handle!=nullptr);
        if(!handle)return;
        const auto send=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,handle->mapFrom(canvas,at),canvas->mapToGlobal(at),button,buttons,Qt::NoModifier);
            QApplication::sendEvent(handle,&event);
        };
        QJsonObject metrics{{"faces",int(original->faces.size())},{"edges",int(original->edges.size())},
                            {"points",int(original->points.size())},{"render_triangles",int(original->triangles.size())},
                            {"show_normals",original->showNormals},{"surface_normals",int(original->surfaceNormals.size())},
                            {"dpr",views_[0]->devicePixelRatioF()}};
        QJsonArray modes;
        for(auto* view:views_)modes.append(view->displayModeKey());
        metrics["initial_display_modes"]=modes;
        const QPoint start=handle->geometry().center();
        const auto anchors=[&]{QVector<QPointF> points;for(auto* view:views_)points<<view->project(original->minimum)<<view->project(original->maximum);return points;};
        send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        QJsonArray frames;
        bool layoutFollows=true,allPaint=true;
        QPoint at=start;
        for(int i=1;i<=20;++i){
            at=start+QPoint(canvas->width()*i/200,canvas->height()*i/220);
            std::array<quint64,4> counts;
            for(int j=0;j<4;++j)counts[j]=views_[j]->paintCount;
            QElapsedTimer elapsed;elapsed.start();
            send(QEvent::MouseMove,at,Qt::NoButton,Qt::LeftButton);
            const double eventMs=elapsed.nsecsElapsed()/1e6;
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const double frameMs=elapsed.nsecsElapsed()/1e6;
            QJsonArray paints;
            for(int j=0;j<4;++j){
                auto* view=views_[j];const bool painted=counts[j]!=view->paintCount;
                allPaint&=painted;paints.append(painted?view->lastPaintMs:-1.);
            }
            layoutFollows&=(handle->geometry().center()-at).manhattanLength()<=4;
            frames.append(QJsonObject{{"event_ms",eventMs},{"frame_ms",frameMs},{"view_ms",paints}});
        }
        const auto beforeRelease=anchors();
        QElapsedTimer release;release.start();
        send(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        metrics["divider_release_ms"]=release.nsecsElapsed()/1e6;
        workflowCase("dense-viewport.divider-follows-cursor",layoutFollows);
        workflowCase("dense-viewport.all-four-views-paint",allPaint);
        workflowCase("dense-viewport.release-keeps-projection",anchors()==beforeRelease);
        metrics["divider_frames"]=frames;
        grab().save(workflowOutput_+"/views/dense-divider-final.png");

        setSelectionKind(HullEntity::Edge);
        auto* view=views_[1];
        view->selectDisplayMode("inspect_lines");
        QSet<fs::EntityId> edges;
        for(const auto& edge:original->edges)if(edge.id%8!=0)edges.insert(edge.id);
        const auto paint=[&]{
            QElapsedTimer elapsed;elapsed.start();view->repaint();
            return elapsed.nsecsElapsed()/1e6;
        };
        QJsonArray unselected, selected, selectedAll;
        for(int i=0;i<12;++i)unselected.append(paint());
        QElapsedTimer select;select.start();
        selectEntities(HullEntity::Edge,edges);
        metrics["selection_event_ms"]=select.nsecsElapsed()/1e6;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        metrics["selection_commit_ms"]=select.nsecsElapsed()/1e6;
        metrics["selected_edges"]=view->selectedEntities().size();
        workflowCase("dense-viewport.complete-edge-selection",edges.size()>1000&&view->selectedEntities()==edges);
        for(int i=0;i<12;++i)selected.append(paint());
        for(int i=0;i<12;++i){
            QElapsedTimer elapsed;elapsed.start();
            for(auto* viewport:views_)viewport->update();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            selectedAll.append(elapsed.nsecsElapsed()/1e6);
        }
        metrics["no_selection_paint_ms"]=unselected;
        metrics["selected_paint_ms"]=selected;
        metrics["selected_four_view_ms"]=selectedAll;
        view->grab().save(workflowOutput_+"/views/dense-selected-edges.png");
        auto* plan=views_[2];
        plan->selectDisplayMode("inspect_lines");
        selectEntities(HullEntity::Edge,edges);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        workflowCase("dense-viewport.plan-keeps-selection",plan->selectedEntities()==edges);
        plan->grab().save(workflowOutput_+"/views/dense-selected-plan.png");

        setSelectionKind(HullEntity::Face);
        QSet<fs::EntityId> faces;
        for(const auto& face:original->faces)if(face.id%8!=0)faces.insert(face.id);
        const auto facePan=[&](HullView* viewport,const QString& mode){
            viewport->selectDisplayMode(mode);
            viewport->fit();
            wait(280);
            selectEntities(HullEntity::Face,faces);
            const auto sendToView=[&](QEvent::Type type,QPoint p,Qt::MouseButton button,Qt::MouseButtons buttons){
                QMouseEvent event(type,p,viewport->mapToGlobal(p),button,buttons,Qt::NoModifier);
                QApplication::sendEvent(viewport,&event);
            };
            const auto hasSelectionInk=[](const QImage& image){
                const auto ink=QColor("#1455b0").rgb();
                for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
                    if(image.pixel(x,y)==ink)return true;
                return false;
            };
            const auto before=viewport->grab().toImage();
            const auto fillRenders=viewport->overlayEvidence()["fill_renders"].toInt();
            const auto anchor=viewport->project(original->minimum);
            const auto start=viewport->rect().center();
            sendToView(QEvent::MouseButtonPress,start,Qt::RightButton,Qt::RightButton);
            QJsonArray times;
            bool follows=true,paints=true;
            for(int i=1;i<=48;++i){
                const auto displacement=QPoint(3*i,i/2);
                const auto count=viewport->paintCount;
                QElapsedTimer elapsed;elapsed.start();
                sendToView(QEvent::MouseMove,start+displacement,Qt::NoButton,Qt::RightButton);
                viewport->repaint();
                times.append(elapsed.nsecsElapsed()/1e6);
                paints&=viewport->paintCount!=count;
                follows&=QLineF(viewport->project(original->minimum),anchor+displacement).length()<.01;
            }
            sendToView(QEvent::MouseButtonRelease,start+QPoint(144,24),Qt::RightButton,Qt::NoButton);
            const auto after=viewport->grab().toImage();
            metrics[mode+"_fill_renders_during_pan"]=viewport->overlayEvidence()["fill_renders"].toInt()-fillRenders;
            before.save(workflowOutput_+"/views/dense-face-"+mode+"-before.png");
            after.save(workflowOutput_+"/views/dense-face-"+mode+"-after.png");
            workflowCase("dense-viewport."+mode+".face-pan",paints&&follows&&faces.size()>1000&&
                         viewport->selectedEntities()==faces&&hasSelectionInk(before)&&hasSelectionInk(after));
            return times;
        };
        metrics["selected_faces"]=faces.size();
        metrics["selected_face_pan_inspection_ms"]=facePan(views_[0],"inspect_faces");
        metrics["selected_face_pan_fill_ms"]=facePan(views_[3],"fill");
        // Reproduce the mixed view modes and large existing selection from the user's screenshot.
        for(auto* viewport:views_)viewport->selectDisplayMode("wireframe");
        setSelectionKind(HullEntity::Point);
        auto* faceView=views_[0];
        faceView->selectDisplayMode("inspect_faces");
        for(auto* viewport:views_)viewport->fit();
        wait(300);
        selectEntities(HullEntity::Face,faces);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        const auto sendStroke=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,at,faceView->mapToGlobal(at),button,buttons,Qt::ControlModifier);
            QApplication::sendEvent(faceView,&event);
        };
        const QPoint origin(30,35),lineEnd(130,35);
        sendStroke(QEvent::MouseButtonPress,origin,Qt::LeftButton,Qt::LeftButton);
        QJsonArray strokeFrames;
        QPoint last=lineEnd;
        bool strokePainted=true,selectionHeld=true;
        for(int i=0;i<24;++i){
            if(i>0)last=QPoint(faceView->width()*(5+i*3)/78,
                               faceView->height()*(i%2?2:5)/7);
            const auto count=faceView->paintCount;
            QElapsedTimer elapsed;elapsed.start();
            sendStroke(QEvent::MouseMove,last,Qt::NoButton,Qt::LeftButton);
            const auto callback=elapsed.nsecsElapsed()/1e6;
            faceView->repaint();
            strokeFrames.append(QJsonObject{{"event_ms",callback},{"frame_ms",elapsed.nsecsElapsed()/1e6}});
            strokePainted&=count!=faceView->paintCount;
            selectionHeld&=faceView->selectedEntities()==faces;
            if(i==0){
                const auto image=faceView->grab().toImage();
                const auto dpr=image.devicePixelRatio();
                int blue=0;
                for(int y=qRound(33*dpr);y<=qRound(37*dpr);++y)
                    for(int x=qRound(35*dpr);x<qRound(125*dpr);++x){
                        const auto rgb=image.pixel(x,y);blue+=qBlue(rgb)>qRed(rgb)+45&&qGreen(rgb)<150;
                    }
                workflowCase("dense-viewport.stroke-line-drawn-in-first-frame",blue>20);
            }
        }
        faceView->grab().save(workflowOutput_+"/views/dense-face-stroke.png");
        sendStroke(QEvent::MouseButtonRelease,last,Qt::LeftButton,Qt::NoButton);
        const auto afterStroke=faceView->selectedEntities();
        workflowCase("dense-viewport.stroke-preview-and-commit",strokePainted&&selectionHeld&&afterStroke.size()>faces.size()&&
                     (faces-afterStroke).isEmpty());
        metrics["face_stroke_frames"]=strokeFrames;
        metrics["face_stroke_added"]=afterStroke.size()-faces.size();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        const auto dividerStart=handle->geometry().center();
        auto dividerEnd=dividerStart;
        send(QEvent::MouseButtonPress,dividerStart,Qt::LeftButton,Qt::LeftButton);
        QJsonArray faceDivider;
        bool selectionResizeTracks=true,selectionResizePaints=true;
        for(int i=1;i<=24;++i){
            dividerEnd=dividerStart-QPoint(i*3,i*2);
            std::array<quint64,4> counts;
            for(int j=0;j<4;++j)counts[j]=views_[j]->paintCount;
            QElapsedTimer elapsed;elapsed.start();
            send(QEvent::MouseMove,dividerEnd,Qt::NoButton,Qt::LeftButton);
            const auto callback=elapsed.nsecsElapsed()/1e6;
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            faceDivider.append(QJsonObject{{"event_ms",callback},{"frame_ms",elapsed.nsecsElapsed()/1e6}});
            selectionResizeTracks&=(handle->geometry().center()-dividerEnd).manhattanLength()<=4;
            for(int j=0;j<4;++j)selectionResizePaints&=views_[j]->paintCount!=counts[j];
        }
        const auto selectedAnchors=anchors();
        QElapsedTimer settle;settle.start();
        send(QEvent::MouseButtonRelease,dividerEnd,Qt::LeftButton,Qt::NoButton);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        metrics["selected_faces_divider_release_ms"]=settle.nsecsElapsed()/1e6;
        metrics["selected_faces_divider_frames"]=faceDivider;
        workflowCase("dense-viewport.selected-faces-divider-tracks-and-keeps-selection",
                     selectionResizeTracks&&selectionResizePaints&&anchors()==selectedAnchors&&faceView->selectedEntities()==afterStroke);
        workflowCase("dense-viewport.document-unmodified",session_.model()==original&&session_.modified()==modified);
        workflowArtifacts_["dense_viewport"]=metrics;
        return;
    }
    const auto ratio=[&]{const auto cell=viewGrid_->cellRect(0,0);return QPointF(
        double(cell.width())/std::max(1,canvas->width()-viewGrid_->spacing()),
        double(cell.height())/std::max(1,canvas->height()-viewGrid_->spacing()));};
    const auto sample=[&](QString phase){
        const auto screenshot=grab();
        QJsonArray frames,titles;bool aligned=true;QRect occupied;
        for(int i=0;i<int(viewFrames_.size());++i)if(auto* frame=viewFrames_[i];frame->isVisible()){
            const auto index=viewGrid_->indexOf(frame);int row,col,rs,cs;viewGrid_->getItemPosition(index,&row,&col,&rs,&cs);
            const auto r=frame->geometry();aligned&=r==viewGrid_->cellRect(row,col);occupied=occupied.united(r);
            frames.append(QJsonArray{r.x(),r.y(),r.width(),r.height()});
            auto* view=views_[i];const auto painted=view->overlayEvidence();
            titles.append(QJsonObject{{"projection",int(view->projection())},
                {"text",painted["projection_title_text"]},{"rect",painted["projection_title_rect"]}});
        }
        const auto at=ratio();workflowCase("view-resize."+phase+".fills-canvas",aligned&&occupied==canvas->contentsRect()&&viewGrid_->isEnabled());
        if(frames.size()==4){
            const auto frameAt=[&](int row,int column){
                return viewGrid_->itemAtPosition(row,column)->widget()->geometry();
            };
            const QRect first=frameAt(0,0),right=frameAt(0,1),below=frameAt(1,0);
            const QRect bounds=canvas->contentsRect();
            const QRect column(first.right()+1,bounds.top(),right.left()-first.right()-1,bounds.height());
            const QRect row(bounds.left(),first.bottom()+1,bounds.width(),below.top()-first.bottom()-1);
            const QMap<QString,QRect> seams{{"x",column},{"y",row},{"xy",column.intersected(row)}};
            bool handlesAligned=true;
            int visibleHandles=0;
            for(auto* child:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly)){
                const auto axis=child->property("studio_view_divider_axis").toString();
                if(!seams.contains(axis)||!child->isVisible())continue;
                ++visibleHandles;
                handlesAligned&=child->geometry()==seams.value(axis);
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(child,&leave);
                QImage ink(child->size(),QImage::Format_ARGB32_Premultiplied);
                ink.fill(Qt::transparent);
                child->render(&ink,QPoint(),QRegion(),QWidget::DrawChildren);
                QImage empty(ink.size(),ink.format());
                empty.fill(Qt::transparent);
                workflowCase("view-resize."+phase+".idle-handle-unpainted-"+axis,ink==empty);
            }
            // During first show, the deferred sync may still be pending.
            // In that case all handles must stay hidden until the next sample.
            const bool expectedCount=visibleHandles==3||(phase=="first-show"&&visibleHandles==0);
            workflowCase("view-resize."+phase+".handles-aligned",handlesAligned&&expectedCount);
        }
        samples.append(QJsonObject{{"phase",phase},{"frames",frames},{"titles",titles},{"left",at.x()},{"top",at.y()}});
        screenshot.save(workflowOutput_+"/views/resize-"+phase+".png");
    };
    const auto divider=[&](const QString& axis)->QWidget*{
        QList<QWidget*> found;for(auto* widget:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
            if(widget->property("studio_view_divider_axis")==axis&&widget->isVisible())found.append(widget);
        if(found.size()!=1)throw std::runtime_error("Visible view divider missing or ambiguous");return found[0];
    };
    const auto drag=[&](const QString& axis,QPoint target){
        auto* handle=divider(axis);auto local=handle->rect().center();
        // Test a plain seam away from the separately draggable intersection.
        if(axis=="x")local.setY(handle->height()/4);
        if(axis=="y")local.setX(handle->width()/4);
        const auto start=handle->mapTo(canvas,local);
        const auto* hit=canvas->childAt(start);
        workflowCase("view-resize."+axis+".input-surface",hit&&hit->property("studio_view_divider_axis")==axis);
        const auto send=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent e(type,handle->mapFrom(canvas,at),canvas->mapToGlobal(at),button,buttons,Qt::NoModifier);QApplication::sendEvent(handle,&e);
        };
        send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        QJsonArray costs;
        for(int i=1;i<=12;++i){QElapsedTimer cost;cost.start();send(QEvent::MouseMove,start+(target-start)*i/12,Qt::NoButton,Qt::LeftButton);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);costs.append(cost.nsecsElapsed()/1e6);}
        if(axis=="xy"&&!workflowArtifacts_.contains("center_drag_ms"))workflowArtifacts_["center_drag_ms"]=costs;
        const auto anchors=[&]{QVector<QPointF> values;if(original)for(auto* view:views_){
            values<<view->project(original->minimum)<<view->project(original->maximum);
        }return values;};
        const auto before=anchors();
        send(QEvent::MouseButtonRelease,target,Qt::LeftButton,Qt::NoButton);wait(40);
        if(frameTiming&&axis=="xy"){
            workflowCase("motion.release-keeps-projections",before==anchors());
            int used=0;bool exact=true;
            for(auto* view:views_){const int result=view->verifyResizeSectionPixels();if(result>=0){++used;exact&=result==1;}}
            workflowArtifacts_["parallel_section_layers_checked"]=used;
            workflowCase("motion.parallel-section-pixels-match-inline",exact);
        }
    };
    showStudioMainWindow(this);sample("first-show");wait(300);sample("initial");
    if(frameTiming && original){
        tabs_->setCurrentIndex(0);
        for(auto* view:views_)view->setAnimationsEnabled(true);
        qApp->setProperty("studio_motion_enabled",true);
        wait(500);
        QWidget* center=nullptr;
        for(auto* child:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
            if(child->property("studio_view_divider_axis")=="xy")center=child;
        workflowCase("motion.center-available",center && center->isVisible());
        if(center && center->isVisible())drag("xy",QPoint(qRound(canvas->width()*.60),qRound(canvas->height()*.57)));
        const auto measure=[&](const QString& name,int duration,int skip){
            QElapsedTimer clock;clock.start();QJsonArray gaps;auto previous=views_[3]->paintCount;
            auto lastGeometry=viewFrames_[3]->geometry();
            double last=0;
            QTimer sampleTimer;sampleTimer.setTimerType(Qt::PreciseTimer);
            connect(&sampleTimer,&QTimer::timeout,this,[&]{
                const auto geometry=viewFrames_[3]->geometry();
                if(previous==views_[3]->paintCount && geometry==lastGeometry)return;
                lastGeometry=geometry;
                previous=views_[3]->paintCount;const double now=clock.nsecsElapsed()/1e6;
                if(last>=skip)gaps.append(now-last);
                last=now;
            });
            sampleTimer.start(2);wait(duration);workflowArtifacts_[name]=gaps;
        };
        for(auto* v:views_)v->restartPresentation();
        measure("entrance_late_frame_intervals_ms",2250,600);
        for(int index:{2,1,0}){
            viewActions_[index]->trigger();
            measure(QString("hide_%1_frame_intervals_ms").arg(index),280,0);
            viewActions_[index]->trigger();wait(450);
        }
        const auto model=session_.model();
        for(auto* v:views_){
            const auto image=v->image();v->restartPresentation();
            workflowCase("motion.export-during-reveal-"+QString::number(int(v->projection())),!image.isNull()&&image==v->image());
            v->finishPresentation();
        }
        workflowCase("motion.document-unchanged",model==original&&session_.modified()==modified);
        workflowArtifacts_["view_grid_resize"]=samples;
        return;
    }
    drag("x",QPoint(qRound(canvas->width()*.65),canvas->height()/4));
    workflowCase("view-resize.vertical-divider-changes-columns",std::abs(ratio().x()-.65)<.015&&std::abs(ratio().y()-.5)<.015);
    // 55% is feasible even on the small 200%-DPI private desktop, while
    // retaining the production 160 px minimum for the bottom view.
    drag("y",QPoint(canvas->width()/4,qRound(canvas->height()*.55)));
    workflowCase("view-resize.horizontal-divider-changes-rows",std::abs(ratio().y()-.55)<.015&&std::abs(ratio().x()-.65)<.015);
    sample("dragged");
    drag("xy",QPoint(qRound(canvas->width()*.56),qRound(canvas->height()*.57)));sample("cross");
    workflowCase("view-resize.cross-changes-both-axes",std::abs(ratio().x()-.56)<.015&&std::abs(ratio().y()-.57)<.015);
    if(original){
        const auto center=[](const HullView* view){return QPointF(view->width()/2.,view->height()/2.);};
        const auto scale=[](const HullView* view){return view->interactionMotionEvidence()["camera_scale"].toDouble();};
        const QPoint pan(24,-18);
        std::array<QPointF,4> pannedAnchors,resizedAnchors;
        std::array<double,4> originalScales,resizedScales;
        const auto projected=[&](HullView* view){return view->project(original->minimum)-center(view);};
        bool moved=true;
        for(auto* view:views_)view->fit();
        wait(280);
        for(int i=0;i<4;++i){
            auto* view=views_[i];
            const auto before=projected(view);
            originalScales[i]=scale(view);
            const QPoint start=view->rect().center();
            const auto send=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
                QMouseEvent event(type,at,view->mapToGlobal(at),button,buttons,Qt::NoModifier);
                QApplication::sendEvent(view,&event);
            };
            send(QEvent::MouseButtonPress,start,Qt::RightButton,Qt::RightButton);
            send(QEvent::MouseMove,start+pan,Qt::NoButton,Qt::RightButton);
            send(QEvent::MouseButtonRelease,start+pan,Qt::RightButton,Qt::NoButton);
            pannedAnchors[i]=projected(view);
            moved&=(pannedAnchors[i]-before-QPointF(pan)).manhattanLength()<.001;
        }
        const QPoint restore(qRound(canvas->width()*.56),qRound(canvas->height()*.57));
        drag("xy",QPoint(qRound(canvas->width()*.44),qRound(canvas->height()*.45)));
        for(int i=0;i<4;++i){resizedAnchors[i]=projected(views_[i]);resizedScales[i]=scale(views_[i]);}
        grab().save(workflowOutput_+"/views/resize-after-pan.png");
        bool shrank=false,correct=true;
        for(auto* view:views_)view->fit();
        wait(280);
        for(int i=0;i<4;++i){
            auto* view=views_[i];
            const double factor=scale(view)/originalScales[i];
            shrank|=factor<.98;
            correct&=std::abs(resizedScales[i]/scale(view)-1)<1e-8&&
                (resizedAnchors[i]-pannedAnchors[i]*factor).manhattanLength()<.001;
        }
        workflowCase("view-resize.panned-views-scale-around-retained-center",moved&&shrank&&correct);
        drag("xy",restore);
    }
    const auto retained=ratio();
    showNormal();resize(1360,900);wait(160);sample("window-resize");
    workflowCase("view-resize.window-resize-retains-ratios",std::abs(ratio().x()-retained.x())<.015&&std::abs(ratio().y()-retained.y())<.015);
    hide();wait(30);show();wait(120);sample("hide-show");
    workflowCase("view-resize.hide-show-retains-ratios",std::abs(ratio().x()-retained.x())<.015&&std::abs(ratio().y()-retained.y())<.015);
    // Reuse the owned-window activation and minimize/restore regression with
    // the user's unequal proportions, rather than only testing a 50/50 grid.
    auditViewReactivation();wait(60);sample("reactivated");
    workflowCase("view-resize.activation-retains-ratios",std::abs(ratio().x()-retained.x())<.015&&std::abs(ratio().y()-retained.y())<.015);
    for(int i:{0,2,3})if(viewActions_[i]->isChecked())viewActions_[i]->trigger();wait(270);
    int visible=0;for(auto* handle:canvas->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))if(handle->property("studio_view_divider_axis").isValid()&&handle->isVisible())++visible;
    workflowCase("view-resize.single-view-has-no-dividers",visible==0);sample("single-view");
    for(int i:{0,2,3})if(!viewActions_[i]->isChecked())viewActions_[i]->trigger();wait(270);sample("four-views-restored");
    workflowCase("view-resize.visibility-toggle-retains-ratios",std::abs(ratio().x()-retained.x())<.015&&std::abs(ratio().y()-retained.y())<.015);
    drag("xy",QPoint(-100,-100));sample("minimum-size");
    workflowCase("view-resize.minimum-size-protected",std::all_of(views_.begin(),views_.end(),[](auto* v){return v->width()>=v->minimumWidth()&&v->height()>=v->minimumHeight();}));
    drag("xy",QPoint(canvas->width()*2,canvas->height()*2));sample("opposite-limit");
    workflowCase("view-resize.opposite-minimum-size-protected",std::all_of(views_.begin(),views_.end(),[](auto* v){return v->width()>=v->minimumWidth()&&v->height()>=v->minimumHeight();}));
    workflowCase("view-resize.document-unmodified",session_.model()==original&&session_.modified()==modified);
    workflowArtifacts_["view_grid_resize"]=samples;
}
