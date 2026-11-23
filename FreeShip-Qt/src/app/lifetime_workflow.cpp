// Isolated production callbacks. No global input or personal files.
#include "main_window.h"
#include "exchange_dialog.h"
#include "exchange_save_probe.h"
#include "plates_dialog.h"
#include "preferences_dialog.h"
#include "studio_ui.h"
#include <QApplication>
#include <QAction>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <stdexcept>

void MainWindow::auditObjectLifetime(){
    if(!startupProbe_||!session_.ready()||session_.busy()){finishWorkflow("Lifetime audit needs an idle private model");return;}
    const auto original=session_.model();const bool oldWorkflow=workflow_;workflow_=true;
    qApp->setQuitOnLastWindowClosed(false);
    try{
        const auto viewportMenu=[](HullView* view){
            view->setAttribute(Qt::WA_DontShowOnScreen);
            QContextMenuEvent event(QContextMenuEvent::Mouse,QPoint(20,20),view->mapToGlobal(QPoint(20,20)));
            QApplication::sendEvent(view,&event);
            QPointer<QMenu> menu;
            for(auto* m:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(m->isVisible())menu=m;
            return menu;
        };
        const QStringList transforms{"translate","scale","rotate","rotate_midship","lackenby"};
        const auto menuItems=[](QMenu* menu){
            QStringList found;
            if(menu)for(auto* entry:menu->actions())if(entry->isVisible()&&!entry->isSeparator())
                found.append(entry->property("operation").toString());
            return found;
        };
        const auto standardMenu=[&](QMenu* menu){
            if(!menu)return false;
            const auto items=menuItems(menu);
            const auto actions=menu->actions();
            return std::none_of(items.begin(),items.end(),[&](const auto& id){return transforms.contains(id);})&&
                std::any_of(actions.begin(),actions.end(),[](const auto* a){return a->property("view_operation")=="save_image";})&&
                std::none_of(actions.begin(),actions.end(),[](const auto* a){return a->menu()&&a->menu()->property("menu_role")=="render_mode";});
        };
        QJsonArray popups;
        for(auto* view:views_){
            const auto menu=viewportMenu(view);
            const bool absent=menu&&!menu->actions().contains(platesAction_);
            popups.append(QJsonObject{{"view",int(view->projection())},{"visible",!menu.isNull()},{"plates_absent",absent}});
            workflowCase("no-plates-in-view-menu-"+QString::number(int(view->projection())),absent);
            workflowCase("ordinary-menu-without-selection-"+QString::number(int(view->projection())),standardMenu(menu));
            if(menu)menu->hide();
        }
        workflowArtifacts_["viewport_menus"]=popups;
        studioBar_->setStartupLayoutWidth(1920);
        const auto buttons=studioBar_->buttons();
        workflowCase("no-transform-toolbar-button",std::none_of(buttons.begin(),buttons.end(),[](auto* button){
            auto* action=button->defaultAction();return action&&action->objectName()=="StudioTransform";
        }));
        QToolButton* platesButton=nullptr;
        for(auto* button:buttons)if(button->defaultAction()==platesAction_)platesButton=button;
        const auto index=buttons.indexOf(platesButton);
        workflowCase("plates-next-to-lines-in-top-bar",index>=2&&
            buttons[index-2]->defaultAction()==sectionsAction_&&buttons[index-1]->defaultAction()==linesAction_&&
            studioBar_->isAncestorOf(platesButton)&&platesButton->parentWidget()==buttons[index-1]->parentWidget()&&
            platesButton->x()-buttons[index-1]->geometry().right()<=3&&
            platesButton->x()>buttons[index-1]->geometry().right()&&!platesButton->menu());
        if(platesButton)platesButton->click();
        QElapsedTimer elapsed;elapsed.start();
        while(!platesDialog_&&elapsed.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,20);
        const bool opened=platesDialog_&&platesDialog_->isVisible();
        if(platesDialog_)platesDialog_->reject();
        workflowCase("plates-toolbar-opens-and-closes-dialog",opened&&!platesDialog_);
        studioBar_->setStartupLayoutWidth(0);

        // Exercise the real menu entries and dismiss their parameter windows.
        if(original->points.isEmpty()||original->edges.isEmpty()||original->faces.isEmpty()||original->controlCurves.isEmpty())
            throw std::runtime_error("Transform menu fixture requires a point, edge, face and control curve");
        const auto completed=session_.onCompleted;
        const auto restoreCompletion=qScopeGuard([this,completed]{session_.onCompleted=completed;});
        session_.onCompleted=[completed](const QString& operation){
            if(operation!="inspect_modeling"&&completed)completed(operation);
        };
        struct Selection {HullEntity kind;fs::EntityId id;QString name;};
        const QList<Selection> selections{
            {HullEntity::Point,original->points.front().id,"point"},
            {HullEntity::Edge,original->edges.front().id,"edge"},
            {HullEntity::Face,original->faces.front().id,"face"},
            {HullEntity::Curve,original->controlCurves.front().id,"curve"}};
        for(const auto& [kind,id,type]:selections){
            setSelectionKind(kind);
            if(kind==HullEntity::Point)selectPoints({id});else selectEntities(kind,{id});
            for(auto* view:views_){
                const auto menu=viewportMenu(view);
                const auto observed=menuItems(menu);
                workflowCase("transform-only-menu-"+type+"-"+QString::number(int(view->projection())),observed==transforms,
                    observed.join('|'));
                if(menu)menu->hide();
            }
            const auto clicks=kind==HullEntity::Point?QStringList{"translate"}:
                kind==HullEntity::Edge?QStringList{"scale"}:kind==HullEntity::Face?QStringList{"rotate","rotate_midship","lackenby"}:QStringList{};
            for(const auto& operation:clicks){
                const auto menu=viewportMenu(views_[0]);
                QAction* entry=nullptr;
                if(menu)for(auto* item:menu->actions())if(item->property("operation")==operation)entry=item;
                if(!entry||!entry->isEnabled())throw std::runtime_error("Transform action is missing or disabled");
                bool shown=false;
                QTimer close;close.setInterval(5);
                connect(&close,&QTimer::timeout,this,[&]{
                    auto* dialog=qobject_cast<QDialog*>(frontStudioDialog());
                    if(!dialog)return;
                    shown=dialog->property("advanced_operation")==operation||dialog->property("modeling_operation")==operation;
                    dialog->reject();close.stop();
                });
                close.start();elapsed.restart();
                const QPointF local=menu->actionGeometry(entry).center(),global=menu->mapToGlobal(local.toPoint());
                QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(menu,&press);QApplication::sendEvent(menu,&release);
                const bool deferred=!shown;
                while(close.isActive()&&elapsed.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,20);
                workflowCase("viewport-transform-opens-"+operation,deferred&&shown&&session_.model()==original);
                if(menu)menu->hide();
            }
            if(kind==HullEntity::Point)selectPoints({});else selectEntities(kind,{});
        }
        setSelectionKind(HullEntity::Point);
        for(auto* view:views_){
            const auto menu=viewportMenu(view);
            workflowCase("ordinary-menu-after-clearing-selection-"+QString::number(int(view->projection())),standardMenu(menu));
            if(menu)menu->hide();
        }

        int destroyedViewCalls=0;
        {
            auto view=std::make_unique<HullView>();view->setAttribute(Qt::WA_DontShowOnScreen);view->setSnapshot(original);
            view->onViewAction=[&](const QString&){++destroyedViewCalls;};
            QContextMenuEvent event(QContextMenuEvent::Keyboard,QPoint(20,20),QPoint(20,20));QApplication::sendEvent(view.get(),&event);
            QAction* save=nullptr;for(auto* a:view->findChildren<QAction*>())if(a->property("view_operation")=="save_image")save=a;
            if(!save)throw std::runtime_error("Queued view action missing");save->trigger();view.reset();QApplication::processEvents();
        }
        workflowCase("destroyed-view-discards-queued-dialog",destroyedViewCalls==0);

        {
            PreferencesDialog preferences({},this);preferences.setAttribute(Qt::WA_DontShowOnScreen);preferences.show();
            QComboBox* theme=nullptr;QPointer<QPushButton> color;
            for(auto* field:preferences.findChildren<QComboBox*>())if(field->property("preference_field")=="theme")theme=field;
            for(auto* button:preferences.findChildren<QPushButton*>())if(button->property("preference_field")=="graphic.viewportcolor")color=button;
            if(!theme||!color||theme->count()<2)throw std::runtime_error("Preference theme/color entries missing");
            QPointer<QWidget> oldPanel=color->parentWidget();
            while(oldPanel&&!qobject_cast<QScrollArea*>(oldPanel.data()))oldPanel=oldPanel->parentWidget();
            bool chosen=false,oldDeleted=false;
            const bool nativeDisabled=QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
            QTimer picker;picker.setInterval(10);
            connect(&picker,&QTimer::timeout,&preferences,[&]{
                if(auto* dialog=qobject_cast<QColorDialog*>(frontStudioDialog())){
                    picker.stop();if(oldPanel)QCoreApplication::sendPostedEvents(oldPanel,QEvent::DeferredDelete);
                    oldDeleted=color.isNull();chosen=true;dialog->setCurrentColor(QColor("#123456"));dialog->accept();
                }
            });picker.start();theme->setCurrentIndex((theme->currentIndex()+1)%theme->count());
            QMetaObject::invokeMethod(theme,"activated",Qt::DirectConnection,Q_ARG(int,theme->currentIndex()));color->click();
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,nativeDisabled);
            workflowArtifacts_["color_rebuild"]=QJsonObject{{"chosen",chosen},{"deleted_at_accept",oldDeleted},{"deleted_after_return",color.isNull()},{"old_panel_alive",!oldPanel.isNull()},{"values",preferences.values()}};
            // Deferred deletion can occur after accept(), before getColor returns.
            workflowCase("color-picker-survives-queued-theme-rebuild",chosen&&color.isNull());
            workflowCase("obsolete-color-result-not-applied",!preferences.values()["graphic"].toObject().contains("viewportcolor"));
            color=nullptr;for(auto* button:preferences.findChildren<QPushButton*>())if(button->property("preference_field")=="graphic.viewportcolor")color=button;
            if(!color)throw std::runtime_error("Rebuilt color button missing");
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);picker.start();color->click();
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,nativeDisabled);
            workflowCase("live-color-result-still-applies",preferences.values()["graphic"].toObject()["viewportcolor"].toInt()==0x563412);
        }

        const QString input=workflowOutput_+"/triangle.obj";QFile file(input);
        if(!file.open(QIODevice::WriteOnly)||file.write("v 0 0 0\nv 4 0 0\nv 0 2 0\nf 1 2 3\n")<0)throw std::runtime_error("Cannot create private import fixture");file.close();
        // Retain the exact continuation passed to beforeImport, as the save
        // guard does. Test both a closed object and an actually destroyed one.
        for(int mode=0;mode<3;++mode){
            ModelSession isolated;isolated.setRecoveryRoot(workflowOutput_+"/import-recovery-"+QString::number(mode));
            QString error;isolated.onError=[&](const QString& e){error=e;};
            auto dialog=std::make_unique<ExchangeDialog>(isolated,true,"obj",this);dialog->setAttribute(Qt::WA_DontShowOnScreen);dialog->show();
            std::function<void()> continuation;dialog->beforeImport=[&](std::function<void()> next){continuation=std::move(next);};
            for(auto* field:dialog->findChildren<QLineEdit*>())if(field->property("exchange_field")=="path")field->setText(input);
            QList<QPushButton*> buttons;for(auto* button:dialog->findChildren<QPushButton*>())if(button->property("exchange_action")=="import")buttons.append(button);
            if(buttons.size()!=1)throw std::runtime_error("Ambiguous import action");buttons.front()->click();
            if(!continuation)throw std::runtime_error("Import continuation was not captured");
            workflowCase("import-waits-for-save-"+QString::number(mode),!isolated.busy()&&!isolated.model());
            if(mode!=2)dialog->reject();if(mode==1)dialog.reset();
            continuation();QElapsedTimer elapsed;elapsed.start();
            while(isolated.busy()&&elapsed.elapsed()<30000)QApplication::processEvents(QEventLoop::AllEvents,20);
            if(isolated.busy())throw std::runtime_error("Private import timed out");
            workflowCase(mode==0?"closed-import-not-executed":mode==1?"destroyed-import-not-executed":"live-import-still-executes",
                error.isEmpty()&&(mode==2?(isolated.ready()&&isolated.modified()):!isolated.model()),error);
            if(mode==2){
                dialog.reset();const auto target=workflowOutput_+"/cancelled-export.csv";
                QFile previous(target);if(!previous.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot create protected export target");previous.write("previous");previous.close();
                auto exporting=std::make_unique<ExchangeDialog>(isolated,false,"offset_table",this);exporting->setAttribute(Qt::WA_DontShowOnScreen);exporting->show();exporting->confirmOverwrite=[](const QString&){return true;};
                answerExchangeSaveDialog(exporting.get(),target);
                for(auto* button:exporting->findChildren<QPushButton*>())if(button->property("exchange_action")=="export")button->click();
                workflowCase("destroy-export-has-pending-worker",exporting->busy()&&!isolated.busy());
                exporting.reset();QApplication::processEvents();previous.open(QIODevice::ReadOnly);
                workflowCase("destroy-export-preserves-destination",previous.readAll()=="previous"&&isolated.ready());
            }
        }
        workflowCase("lifetime-audit-preserves-original-document",session_.model()==original&&!session_.busy());
    }catch(const std::exception& e){workflow_=oldWorkflow;finishWorkflow(QString::fromUtf8(e.what()));return;}
    workflow_=oldWorkflow;
}
