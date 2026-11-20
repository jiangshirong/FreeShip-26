// Isolated production callbacks. No global input or personal files.
#include "main_window.h"
#include "exchange_dialog.h"
#include "exchange_save_probe.h"
#include "plates_dialog.h"
#include "preferences_dialog.h"
#include <QApplication>
#include <QAction>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <stdexcept>

void MainWindow::auditObjectLifetime(){
    if(!startupProbe_||!session_.ready()||session_.busy()){finishWorkflow("Lifetime audit needs an idle private model");return;}
    const auto original=session_.model();const bool oldWorkflow=workflow_;workflow_=true;
    qApp->setQuitOnLastWindowClosed(false);
    try{
        // The real shared Plates action used to bypass the local popup fix.
        QJsonArray popups;
        for(auto* view:views_)for(bool ctrl:{false,true}){
            view->setAttribute(Qt::WA_DontShowOnScreen);
            QContextMenuEvent event(QContextMenuEvent::Keyboard,QPoint(20,20),view->mapToGlobal(QPoint(20,20)));
            QApplication::sendEvent(view,&event);
            QPointer<QMenu> menu;
            for(auto* m:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(m->isVisible())menu=m;
            if(!menu||!menu->actions().contains(platesAction_))throw std::runtime_error("Shared plates popup action missing");
            bool opened=false;int opens=0;QElapsedTimer elapsed;elapsed.start();QTimer close;close.setInterval(10);
            connect(&close,&QTimer::timeout,this,[&]{if(platesDialog_){opened=true;++opens;platesDialog_->reject();close.stop();}});close.start();
            const QPointF local=menu->actionGeometry(platesAction_).center(),global=menu->mapToGlobal(local.toPoint());
            const auto modifiers=ctrl?Qt::ControlModifier:Qt::NoModifier;
            QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,modifiers);
            QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,modifiers);
            QApplication::sendEvent(menu,&press);QApplication::sendEvent(menu,&release);
            const bool deferred=!opened;
            while(!opened&&elapsed.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,20);
            popups.append(QJsonObject{{"view",int(view->projection())},{"ctrl",ctrl},{"deferred",deferred},{"opens",opens},{"closed",platesDialog_==nullptr}});
            workflowCase("plates-popup-"+QString::number(int(view->projection()))+(ctrl?"-ctrl":"-normal"),deferred&&opens==1&&!platesDialog_);
            if(menu)menu->hide();
        }
        workflowArtifacts_["plates_popup_lifetime"]=popups;

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
                if(auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget())){
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
