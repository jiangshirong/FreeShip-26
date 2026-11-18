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

void MainWindow::auditViewGridResize(bool frameTiming){
    const auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    auto* canvas=viewGrid_->parentWidget();const auto original=session_.model();const bool modified=session_.modified();
    QJsonArray samples;QDir().mkpath(workflowOutput_+"/views");
    const auto ratio=[&]{const auto cell=viewGrid_->cellRect(0,0);return QPointF(
        double(cell.width())/std::max(1,canvas->width()-viewGrid_->spacing()),
        double(cell.height())/std::max(1,canvas->height()-viewGrid_->spacing()));};
    const auto sample=[&](QString phase){
        QJsonArray frames;bool aligned=true;QRect occupied;
        for(auto* frame:viewFrames_)if(frame->isVisible()){
            const auto index=viewGrid_->indexOf(frame);int row,col,rs,cs;viewGrid_->getItemPosition(index,&row,&col,&rs,&cs);
            const auto r=frame->geometry();aligned&=r==viewGrid_->cellRect(row,col);occupied=occupied.united(r);
            frames.append(QJsonArray{r.x(),r.y(),r.width(),r.height()});
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
        samples.append(QJsonObject{{"phase",phase},{"frames",frames},{"left",at.x()},{"top",at.y()}});
        grab().save(workflowOutput_+"/views/resize-"+phase+".png");
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
        send(QEvent::MouseButtonRelease,target,Qt::LeftButton,Qt::NoButton);wait(40);
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
