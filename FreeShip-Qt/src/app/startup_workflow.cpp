// Retained hidden-process observation of the production startup/open/save paths.
#include "main_window.h"
#include "document_workspace.h"
#include <QAbstractButton>
#include "ui_language.h"
#include "project_dialog.h"
#include "exchange_dialog.h"
#include "section_dialog.h"
#include "station_template_workflow.h"
#include "preferences_dialog.h"
#include "layer_panel.h"
#include "light_dialog.h"
#include <QGroupBox>
#include "plates_dialog.h"
#include "studio_ui.h"
#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QStatusBar>
#include <QSplitter>
#include <QTimer>
#include <QTableWidget>
#include <QHeaderView>
#include <QFontInfo>
#include <QFontMetrics>
#include <QLayout>
#include <QScrollBar>
#include <QScrollArea>
#include <QLabel>
#include <QShortcut>
#include <QStyleOptionMenuItem>
#include <QPainter>
#include <QScopeGuard>
#include <QToolBar>
#include <QToolButton>
#include <QTextEdit>
#include "studio_motion.h"
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QTabWidget>
#include <QTabBar>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileDialog>
#include <QScreen>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QProcess>

namespace {
// Installed only by the queued-edit close probe, immediately before its decision.
class CloseDialogObserver final : public QObject {
public:
    explicit CloseDialogObserver(QObject* owner):QObject(owner){
        owner->setProperty("probe_exit_dialogs",0);
        qApp->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* object,QEvent* event) override {
        if(event->type()==QEvent::Show){
            const auto* dialog=qobject_cast<QDialog*>(object);
            if(dialog&&dialog->isWindow())parent()->setProperty("probe_exit_dialogs",++shown_);
        }
        return false;
    }
private:
    int shown_=0;
};
}
#include <QPointer>
#include <QDropEvent>
#include <QMimeData>
#include <QThread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// Only the explicitly requested hidden startup probe can kill its own service.
// This is an actual QProcess exit, not a simulated successful recovery reply.
struct BackendExitProbe {static void terminate(ModelSession& s){s.process_.kill();}};
void MainWindow::verifyDocumentWindows(const QString& request,const QString& output){
    workflowOutput_=output;workflowTimer_.start();QFile input(request);
    if(!input.open(QIODevice::ReadOnly)){finishWorkflow("Missing request");return;}
    const auto p=QJsonDocument::fromJson(input.readAll()).object();const auto a=p["a"].toString(),b=p["b"].toString();
    if(!workspace_){finishWorkflow("Missing tab workspace");return;}
    setAttribute(Qt::WA_DeleteOnClose,false); // Keep the observer until its final report is written.
    auto* host=workspace_;auto* tabs=host->tabBar();host->resize(1000,720);host->show();
    auto drop=[&](const QList<QUrl>& urls){QMimeData mime;mime.setUrls(urls);
        QDragEnterEvent enter({1,1},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(host,&enter);
        if(!enter.isAccepted())return false;
        QDropEvent drop(QPointF(1,1),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(host,&drop);return drop.isAccepted();};
    auto wait=[](auto predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<20000){QCoreApplication::processEvents(QEventLoop::AllEvents,30);QThread::msleep(5);}return predicate();};
    auto explorerLaunch=[&](const QString& path){
        QProcess child;child.setWorkingDirectory(QFileInfo(path).absolutePath());
        child.start(QCoreApplication::applicationFilePath(),{QFileInfo(path).fileName()});
        if(!child.waitForStarted(5000))return false;
        const bool finished=wait([&]{return child.state()==QProcess::NotRunning;});
        if(!finished){child.kill();child.waitForFinished();return false;}
        return child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0;
    };
    const auto externalPath=p["external"].toString();
    workflowCase("irrelevant-and-remote-drops-ignored",!drop({QUrl("https://example.invalid/ship.fbm"),QUrl::fromLocalFile(request)}));
    workflowCase("multi-file-drop-accepted",drop({QUrl::fromLocalFile(a),QUrl::fromLocalFile(b),QUrl::fromLocalFile(a)}));
    workflowCase("new-document-tabs-use-source-names-immediately",tabs->count()==2&&tabs->tabText(0)==QFileInfo(a).fileName()&&tabs->tabText(1)==QFileInfo(b).fileName());
    QPointer<MainWindow> other=windowForFile(b);
    if(!other||other==this||!wait([&]{return session_.ready()&&other->session_.ready()&&!session_.busy()&&!other->session_.busy();})){finishWorkflow("Both dropped models must load");return;}
    workflowCase("independent-documents-and-deduplicated-load",documentWindows().size()==2&&windowForFile(a)==this&&session_.model()->documentId!=other->session_.model()->documentId);
    const auto id=other->session_.model()->documentId;const auto rev=other->session_.model()->revision;
    const bool edited=session_.updateLayer(session_.model()->layers.front().id,{{"name","QA temporary name"}})&&wait([&]{return !session_.busy();});
    const bool externalLaunched=explorerLaunch(externalPath);
    QPointer<MainWindow> external=windowForFile(externalPath);
    const bool loadedExternal=externalLaunched&&external&&wait([&]{return external->session_.ready()&&!external->session_.busy();});
    workflowCase("explorer-second-process-opens-new-tab",loadedExternal&&host->active()==external&&tabs->count()==3&&session_.modified());
    host->showMinimized();
    const bool repeat=explorerLaunch(b);
    workflowCase("explorer-repeat-focuses-existing-tab",repeat&&host->active()==other&&!host->isMinimized()&&tabs->count()==3&&session_.modified());
    if(external)external->close();
    openDocument(b);
    workflowCase("switch-preserves-edit-and-isolates-undo",edited&&session_.modified()&&session_.canUndo()&&!other->session_.modified()&&!other->session_.canUndo()&&other->session_.model()->revision==rev&&documentWindows().size()==2);
    workflowCase("one-frame-two-tabs",!isWindow()&&!other->isWindow()&&window()==host&&other->window()==host&&host->active()==other&&tabs->count()==2&&other->isVisible()&&!isVisible());
    workflowCase("caption-filenames-and-full-path-tooltips",host->windowTitle()=="FreeShip 26"&&tabs->tabText(0)==QFileInfo(a).fileName()+" *"&&tabs->tabText(1)==QFileInfo(b).fileName()&&QFileInfo(tabs->tabToolTip(0))==QFileInfo(a)&&QFileInfo(tabs->tabToolTip(1))==QFileInfo(b));
    tabs->setCurrentIndex(0);workflowCase("select-tab-restores-editor",host->active()==this&&isVisible()&&!other->isVisible());
    const auto tabPoint=tabs->tabRect(0).center();
    QWheelEvent tabWheel(tabPoint,tabs->mapToGlobal(tabPoint),{},QPoint(0,-120),
                         Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(tabs,&tabWheel);
    workflowCase("document-tab-wheel-keeps-active-model",tabs->currentIndex()==0&&host->active()==this);
    tabs->setCurrentIndex(0);
    bool cancelSeen=false;
    QTimer::singleShot(0,this,[&]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){cancelSeen=true;box->button(QMessageBox::Cancel)->click();}});
    const bool closed=host->close();
    workflowCase("cancel-close-keeps-dirty-document",cancelSeen&&!closed&&isVisible()&&session_.modified()&&documentWindows().size()==2);
    const bool undone=session_.undo()&&wait([&]{return !session_.busy();})&&session_.model()->layers.front().name!="QA temporary name";
    const bool redone=session_.redo()&&wait([&]{return !session_.busy();})&&session_.model()->layers.front().name=="QA temporary name";
    workflowCase("undo-redo-affects-only-active-document",undone&&redone&&other->session_.model()->revision==rev&&!other->session_.modified());
    const auto saved=p["saved"].toString();workflowCase("save-edited-model",session_.save(saved)&&wait([&]{return !session_.busy();})&&!session_.modified()&&QFileInfo(session_.sourcePath())==QFileInfo(saved));
    workflowCase("save-as-renames-tab",tabs->tabText(0)==QFileInfo(saved).fileName()&&QFileInfo(tabs->tabToolTip(0))==QFileInfo(saved));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if(auto* caption=host->menuWidget()){
        const auto path=workflowOutput_+"/document-tabs.png";
        if(caption->grab().save(path))workflowArtifacts_["document_tabs_image"]="document-tabs.png";
    }
    const auto closeButton=[&](QTabBar::ButtonPosition side){return qobject_cast<QAbstractButton*>(tabs->tabButton(0,side));};
    auto* close=closeButton(QTabBar::RightSide);if(!close)close=closeButton(QTabBar::LeftSide);
    if(close)close->click();
    workflowCase("closing-first-keeps-second",close&&documentWindows().size()==1&&other->isVisible()&&other->session_.model()->documentId==id&&other->session_.ready());
    host->close();workflowCase("closed-documents-leave-registry",documentWindows().isEmpty());finishWorkflow();
}
void MainWindow::verifyStartup(const QString& request,const QString& output){
    startupProbe_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QDir().mkpath(workflowOutput_);QFile file(request);
    if(!file.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read startup request");return;}
    const auto config=QJsonDocument::fromJson(file.readAll()).object();
    if(config["measure_first_frame"].toBool()||config["observe_restore_notice"].toBool()) {
#ifdef Q_OS_WIN
        wchar_t name[256]{};DWORD bytes=0;
        if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,name,sizeof(name),&bytes)||
           !QString::fromWCharArray(name).startsWith("FreeShip26-QA-")||
           QString::fromWCharArray(name)!=qEnvironmentVariable("FREESHIP_QA_DESKTOP")){
            finishWorkflow("First-frame measurement requires the isolated QA desktop");return;
        }
#endif
        if(config["observe_restore_notice"].toBool()){
            const auto language=config["language"].toString("en");
            if(!setUiLanguage(language).isEmpty()){finishWorkflow("Notice language unavailable: "+language);return;}
            showNormal();resize(980,700);
        }else show();
    }
    session_.setProperty("disable_first_frame",config["disable_first_frame"].toBool());
    session_.setProperty("disable_startup_cache",config["disable_startup_cache"].toBool());
    session_.setProperty("defer_startup_drawing",config["defer_drawing"].toBool(config["measure_first_frame"].toBool()));
    if(config["abort_after_first_frame"].toBool()){
        const auto show=session_.onFirstFrame;
        session_.onFirstFrame=[this,show](std::shared_ptr<const fs::ModelSnapshot> model){
            if(show)show(model);if(model){setProperty("first_frame_abort_seen",true);BackendExitProbe::terminate(session_);}
        };
    }
    setProperty("first_hull_frame_ms",QVariant{});
    for(auto* view:views_)view->onHullFrame=[this]{if(!property("first_hull_frame_ms").isValid())setProperty("first_hull_frame_ms",workflowTimer_.elapsed());};
    for(auto* view:views_)view->onHullVisibleFrame=[this,view]{
        const auto now=workflowTimer_.elapsed();
        if(!property("first_visible_hull_ms").isValid())setProperty("first_visible_hull_ms",now);
        auto times=property("view_first_visible_ms").toJsonObject();const auto key=QString::number(int(view->projection()));
        if(!times.contains(key)){times[key]=now;setProperty("view_first_visible_ms",times);}
    };
    fileHistory_=std::make_unique<FileHistory>(config["history_file"].toString());
    if(config.contains("recovery_root"))session_.setRecoveryRoot(config["recovery_root"].toString());
    if(config.contains("lines_cache_root"))session_.setLinesCacheRoot(config["lines_cache_root"].toString());
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{if(startupClosing_&&!workflowFinished_)startupStep("close");});
    startupCommands_=config["commands"].toArray();startupCommand_=0;refreshRecentFiles();
    QTimer::singleShot(120000,this,[this]{if(!workflowFinished_)finishWorkflow("Startup workflow timed out");});
    startupStep("begin");
}
void MainWindow::startupStep(const QString& completedOperation,const QString& error){
    QString operation=completedOperation;
    if(workflowFinished_)return;
    if(property("probe_edit_during_close").toBool()&&startupClosing_&&operation!="close"&&operation!="error")return;
    if(operation=="snapshot"&&property("deferred_startup_probe").toBool()){
        setProperty("deferred_startup_probe",false);operation="load";
        auto evidence=property("deferred_drawing_evidence").toJsonObject();
        evidence["generation_ms"]=session_.lastOperationMs();evidence["parse_background"]=session_.lastLoadTimings()["ui_parse_background"];
        QFile model(session_.modelPath());if(model.open(QIODevice::ReadOnly))evidence["model_unchanged"]=QString::fromLatin1(QCryptographicHash::hash(model.readAll(),QCryptographicHash::Sha256).toHex())==evidence["model_sha256"];
        const auto scene=session_.scene();openLinesWindow();
        evidence["reopen_reused_scene"]=session_.scene()==scene&&!session_.busy();
        evidence["view_geometry_reused"]=session_.drawingUnchanged();
        setProperty("deferred_drawing_evidence",evidence);linesDialog_->hide();
    }
    if(operation=="load"&&session_.ready()&&!property("startup_ready_ms").isValid())setProperty("startup_ready_ms",workflowTimer_.elapsed());
    if(operation=="load"&&session_.ready()&&!property("first_hull_frame_ms").isValid()&&property("frame_probe_waits").toInt()<60){
        setProperty("frame_probe_waits",property("frame_probe_waits").toInt()+1);
        QTimer::singleShot(16,this,[this,operation,error]{startupStep(operation,error);});return;
    }
    if(operation=="load"&&session_.ready()&&!session_.scene()){
        const auto published=QFileInfo(session_.modelPath()).dir();QFile native(published.filePath("native-report.json")),model(session_.modelPath());
        QJsonObject evidence{{"no_scene_or_dxf_at_load",!QFileInfo::exists(published.filePath("scene.fslp"))&&!QFileInfo::exists(published.filePath("reference.dxf"))},
            {"load_timings",session_.lastLoadTimings()},{"operation_ms",session_.lastOperationMs()}};
        if(native.open(QIODevice::ReadOnly))evidence["native_report"]=QJsonDocument::fromJson(native.readAll()).object();
        if(model.open(QIODevice::ReadOnly))evidence["model_sha256"]=QString::fromLatin1(QCryptographicHash::hash(model.readAll(),QCryptographicHash::Sha256).toHex());
        setProperty("deferred_drawing_evidence",evidence);setProperty("deferred_startup_probe",true);
        openLinesWindow();return;
    }
    if(operation!="begin"){
        QJsonArray recent;for(const auto& path:fileHistory_->read().paths)recent.append(path);
        QJsonArray actions;for(auto* action:recentMenu_->actions())actions.append(QJsonObject{{"text",action->text()},{"path",action->data().toString()},{"enabled",action->isEnabled()},{"operation",action->property("file_operation").toString()}});
        QJsonObject item{{"operation",operation},{"error",error},{"source",session_.sourcePath()},{"ready",session_.ready()},{"modified",session_.modified()},
            {"last",fileHistory_->read().last},{"recent",recent},{"actions",actions},{"status_text",statusBar()->currentMessage()}};
        item["window_maximized"]=isMaximized();
        item["window_title"]=windowTitle();item["status_bar_visible"]=statusBar()->isVisibleTo(this);
        item["window_full_screen"]=isFullScreen();
        item["window_visible"]=isVisible();item["lines_visible"]=linesDialog_->isVisible();
        if(property("probe_close_decision_ms").isValid()){
            if(property("probe_exit_dialogs").isValid())item["dialogs_shown_after_decision"]=property("probe_exit_dialogs").toInt();
            item["decision_to_observation_ms"]=workflowTimer_.elapsed()-property("probe_close_decision_ms").toLongLong();
            item["window_enabled"]=isEnabled();
            item["decision_to_close_return_ms"]=property("probe_close_return_ms").toLongLong();
            item["main_hidden_on_close_return"]=property("probe_main_hidden").toBool();
            item["main_enabled_on_close_return"]=property("probe_main_enabled").toBool();
            item["revision_after_decision"]=session_.model()?double(session_.model()->revision)-property("probe_close_revision").toDouble():0.;
        }
        item["close_prompt_count"]=property("close_probe_prompt_count").toInt();
        item["discard_caption"]=property("close_probe_discard_caption").toString();
        item["recovery_file"]=session_.recoveryFile();item["recovery_error"]=session_.recoveryError();item["recovery_origin"]=session_.recoveryOrigin();
        item["backup_file"]=session_.lastBackupPath();
        item["can_resume_current"]=session_.canResumeCurrent();
        item["can_undo"]=session_.canUndo();item["can_redo"]=session_.canRedo();
        item["load_timings"]=session_.lastLoadTimings();
        item["startup_profile"]=session_.property("startup_profile").toJsonObject();
        item["first_frame_abort_seen"]=property("first_frame_abort_seen").toBool();
        item["preview_cleared"]=std::all_of(views_.begin(),views_.end(),[](auto* v){return !v->snapshot();});
        item["first_hull_frame_ms"]=QJsonValue::fromVariant(property("first_hull_frame_ms"));
        item["first_visible_hull_ms"]=QJsonValue::fromVariant(property("first_visible_hull_ms"));
        item["view_first_visible_ms"]=property("view_first_visible_ms").toJsonObject();
        item["operation_ms"]=session_.lastOperationMs();
        item["startup_ready_ms"]=QJsonValue::fromVariant(property("startup_ready_ms"));
        item["save_preview_ms"]=session_.lastSavePreviewMs();
        if(!session_.lastBackupPath().isEmpty()&&QFileInfo(session_.lastBackupPath()).isFile()){
            const auto saved=workflowOutput_+"/backup-"+QString::number(startupCommand_)+".fbm";
            ModelSession::atomicCopy(session_.lastBackupPath(),saved);item["backup_snapshot"]=saved;
        }
        if(session_.model()){
            QFile native(QFileInfo(session_.modelPath()).dir().filePath("native-report.json"));
            if(native.open(QIODevice::ReadOnly))item["native_report"]=QJsonDocument::fromJson(native.readAll()).object();
            const auto target=workflowOutput_+"/model-"+QString::number(startupCommand_)+".json";
            ModelSession::atomicCopy(session_.modelPath(),target);item["model"]=target;
            const auto published=QFileInfo(session_.modelPath()).dir();
            for(const auto& artifact:{qMakePair(QString("scene"),session_.scenePath()),qMakePair(QString("dxf"),session_.dxfPath()),qMakePair(QString("surface"),published.filePath("surface.json")),qMakePair(QString("sections"),published.filePath("sections.json"))}){
                const auto saved=workflowOutput_+'/'+artifact.first+'-'+QString::number(startupCommand_)+'.'+artifact.first;
                ModelSession::atomicCopy(artifact.second,saved);item[artifact.first]=saved;
            }
        }
        if(operation=="load"&&!property("deferred_drawing_evidence").toJsonObject().isEmpty()){
            const auto evidence=property("deferred_drawing_evidence").toJsonObject();item["deferred_drawing"]=evidence;
            item["deferred_native_report"]=item["native_report"];item["native_report"]=evidence["native_report"];
            item["load_timings"]=evidence["load_timings"];item["operation_ms"]=evidence["operation_ms"];
            setProperty("deferred_drawing_evidence",QVariant{});
        }
        startupObservations_.append(item);
    }
    if(startupCommand_>=startupCommands_.size()){
        QFile file(workflowOutput_+"/observations.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(startupObservations_).toJson());
        workflowArtifacts_["observations"]=file.fileName();workflowCase("startup_commands_observed",true);
        workflowCases_.append(QJsonObject{{"name","desktop_acceptance"},{"status","not_run"}});finishWorkflow();return;
    }
    const auto command=startupCommands_[startupCommand_++].toObject();
    QTimer::singleShot(0,this,[this,command]{
        const auto observed=startupObservations_.size();
        const auto kind=command["kind"].toString(),path=command["path"].toString();
        if(command.contains("recovery_decision"))QTimer::singleShot(0,this,[this,command]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog){finishWorkflow("Recovery dialog did not open");return;}
            const auto id=command["recovery_id"].toString();
            if(!id.isEmpty())for(auto* list:dialog->findChildren<QListWidget*>())for(int i=0;i<list->count();++i)
                if(list->item(i)->data(Qt::UserRole).toString()==id)list->setCurrentRow(i);
            QDir().mkpath(workflowOutput_+"/views");dialog->grab().save(workflowOutput_+"/views/recovery-"+QString::number(startupCommand_)+".png");
            QList<QPushButton*> found;for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("recovery_action")==command["recovery_decision"].toString())found.append(b);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recovery_action_unavailable"},{"status","not_run"}});dialog->reject();return;
            }
            if(command["recovery_decision"]=="delete")QTimer::singleShot(0,this,[dialog,command]{
                if(auto* question=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
                    auto* button=question->button(command["confirm_delete"].toBool()?QMessageBox::Yes:QMessageBox::No);
                    if(button)button->click();
                }
                QTimer::singleShot(0,dialog,&QDialog::reject);
            });
            found[0]->click();
        });
        if(kind=="station_templates"){
            try {
                editSections();
                if(!sections_) throw std::runtime_error("Section editor unavailable");
                auto result=observeStationExport(*sections_,workflowOutput_,command);
                workflowArtifacts_[command["label"].toString()]=result;
            }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));return;}
        }
        else if(kind=="close_controls_check"){
            const auto language=command["review_language"].toString();
            if(!language.isEmpty()){
                const auto error=setUiLanguage(language);
                if(!error.isEmpty()){finishWorkflow(error);return;}
            }
            auditCloseControls();
        }
        else if(kind=="live_window_language"){
            if(!session_.model())throw std::runtime_error("Live window language probe needs a model");
            loadPreferences(command["settings"].toString());
            if(!languageShortcut_||!languageShortcut_->isEnabled())throw std::runtime_error("Live language shortcut did not load");
            const auto alternate=command["target"].toString("en");
            // The shortcut resolver uses the active widget window. Show the
            // owner too, on the runner's non-input desktop only.
#ifdef Q_OS_WIN
            wchar_t desktop[256]{};DWORD bytes=0;
            if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&bytes)||
               !QString::fromWCharArray(desktop).startsWith("FreeShip26-QA-")||
               QString::fromWCharArray(desktop)!=qEnvironmentVariable("FREESHIP_QA_DESKTOP"))
                throw std::runtime_error("Live language probe requires the private desktop");
#endif
            const bool wasVisible=isVisible();const auto restoreOwner=qScopeGuard([this,wasVisible]{if(!wasVisible)hide();});
            if(!wasVisible)show();
            const auto collect=[this](QDialog* dialog){
                QJsonArray labels,tooltips,values,headers,tabs,choices,sectionHints,cad;
                for(auto* widget:dialog->findChildren<QWidget*>()){
                    if(!widget->toolTip().isEmpty())tooltips.append(widget->toolTip());
                    if(auto* label=qobject_cast<QLabel*>(widget);label&&!label->text().isEmpty())labels.append(label->text());
                    if(auto* button=qobject_cast<QAbstractButton*>(widget);button&&!button->text().isEmpty())labels.append(button->text());
                    if(auto* group=qobject_cast<QGroupBox*>(widget);group&&!group->title().isEmpty())labels.append(group->title());
                    if(auto* line=qobject_cast<QLineEdit*>(widget);line&&!line->isReadOnly()){
                        if(!line->placeholderText().isEmpty())tooltips.append(line->placeholderText());
                        if(line->property("modelField").isValid()||line->property("exchange_field").isValid()||line->property("exchange_number").isValid())values.append(line->text());
                    }
                    if(auto* box=qobject_cast<QComboBox*>(widget)){
                        values.append(QJsonArray{box->currentIndex(),QJsonValue::fromVariant(box->currentData())});
                        QJsonArray list;for(int i=0;i<box->count();++i)list.append(box->itemText(i));choices.append(list);
                    }
                    if(auto* tab=qobject_cast<QTabWidget*>(widget)){
                        QJsonArray list;for(int i=0;i<tab->count();++i)list.append(tab->tabText(i));tabs.append(list);
                    }
                    if(auto* table=qobject_cast<QTableWidget*>(widget)){
                        QJsonArray list;for(int i=0;i<table->columnCount();++i)if(auto* h=table->horizontalHeaderItem(i))list.append(h->text());headers.append(list);
                        if(table->objectName()=="SectionDefinitions")for(int i=0;i<table->rowCount();++i)
                            if(auto* item=table->item(i,2)){sectionHints.append(item->toolTip());values.append(int(item->checkState()));}
                    }
                }
                if(auto* layers=dialog->findChild<QListWidget*>("layer_list");layers&&layers->count()>1)
                    values.append(layers->item(0)->text());
                for(const auto* action:exchangeActions_)cad.append(action->text());
                return QJsonObject{{"language",uiLanguage()},{"title",dialog->windowTitle()},{"labels",labels},{"tooltips",tooltips},
                    {"choices",choices},{"headers",headers},{"tabs",tabs},{"section_hints",sectionHints},{"values",values},{"exchange_actions",cad}};
            };
            auto snapshot=std::make_shared<fs::ModelSnapshot>(*session_.model());
            // A fixture with an arbitrary user caption verifies that only product text changes.
            const auto name=QStringLiteral("客户自定义船体");
            if(!snapshot->layers.isEmpty()){snapshot->layers.front().name=name;snapshot->layers.front().defaultNameNumber.reset();}
            if(std::none_of(snapshot->sections.cbegin(),snapshot->sections.cend(),[](const auto& section){return section.kind==1;})){
                fs::Section section{};section.kind=1;snapshot->sections.append(section);
            }
            const auto make=[&](const QString& kind)->std::unique_ptr<QDialog>{
                if(kind=="project")return std::make_unique<ProjectDialog>(*snapshot,this);
                if(kind=="sections"){
                    auto dialog=std::make_unique<SectionDialog>(this);dialog->setSnapshot(snapshot);dialog->setEditingEnabled(true);return dialog;
                }
                if(kind=="layer"){
                    LayerPanel* panel=nullptr;std::unique_ptr<QDialog> dialog(createLayerPropertiesDialog(this,panel));
                    panel->setSnapshot(snapshot);panel->setEditingEnabled(true);return dialog;
                }
                if(kind=="preferences")return std::make_unique<PreferencesDialog>(preferences_,this);
                if(kind=="plates")return std::make_unique<PlatesDialog>(session_,true,this);
                if(kind=="lighting"){
                    auto* view=views_[0];openLightDialog(view);return std::unique_ptr<QDialog>(lightDialogs_.take(view));
                }
                const bool importing=kind.startsWith("import:");
                return std::make_unique<ExchangeDialog>(session_,importing,importing?kind.mid(7):kind,this);
            };
            const auto chord=[](QWidget* window){
                auto* target=window->focusWidget();if(!target)target=window;
                QKeyEvent press(QEvent::KeyPress,Qt::Key_L,Qt::ControlModifier|Qt::ShiftModifier,"l");QApplication::sendEvent(target,&press);
                QKeyEvent release(QEvent::KeyRelease,Qt::Key_L,Qt::NoModifier);QApplication::sendEvent(target,&release);
            };
            QJsonObject colorCoverage;
            for(const auto& entry:command["color_pickers"].toArray()){
                const auto id=entry.toString();
                const auto observePicker=[&](bool switchWhileOpen){
                    auto owner=make(id);owner->show();
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    QPushButton* trigger=nullptr;
                    for(auto* button:owner->findChildren<QPushButton*>()){
                        const bool matches=id=="preferences"?button->property("preference_field")=="graphic.viewportcolor":button->property("colorValue").isValid();
                        if(!matches)continue;
                        if(trigger)throw std::runtime_error("Ambiguous color-picker action");
                        trigger=button;
                    }
                    if(!trigger||!trigger->isEnabled())throw std::runtime_error("Missing enabled color-picker action");
                    QPointer<QPushButton> colorButton(trigger);
                    const auto currentColor=[&]{return !colorButton?QVariant{}:colorButton->property(id=="preferences"?"color":"colorValue");};
                    const auto original=currentColor();
                    QJsonObject result;
                    QTimer ready;ready.setSingleShot(true);
                    connect(&ready,&QTimer::timeout,owner.get(),[&]{
                        auto* picker=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
                        if(!picker){if(auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget()))modal->reject();return;}
                        if(picker->parentWidget()!=(id=="preferences"?static_cast<QWidget*>(owner.get()):static_cast<QWidget*>(colorButton.data()))){picker->reject();return;}
                        picker->setCurrentColor(QColor("#3a70a1"));
                        QApplication::setActiveWindow(picker);
                        const auto frame=[&]{
                            auto surface=collect(picker);
                            surface["color"]=picker->currentColor().name(QColor::HexArgb);
                            return surface;
                        };
                        result["before"]=frame();
                        if(switchWhileOpen){chord(picker);result["after"]=frame();chord(picker);result["restored"]=frame();}
                        picker->reject();
                    });
                    ready.start(0);trigger->click();
                    result["cancel_keeps_original"]=bool(colorButton)&&original==currentColor();
                    return result;
                };
                auto observed=observePicker(true);
                if(!setUiLanguage(alternate).isEmpty())throw std::runtime_error("Cannot load color-picker target language");
                observed["fresh"]=observePicker(false).value("before");
                if(!setUiLanguage("zh").isEmpty())throw std::runtime_error("Cannot restore Chinese after color picker");
                colorCoverage[id]=observed;
            }
            workflowArtifacts_["color_picker_language"]=colorCoverage;
            QJsonObject coverage;
            for(const auto& entry:command["windows"].toArray()){
                const auto id=entry.toString();
                auto dialog=make(id);dialog->show();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                QApplication::setActiveWindow(dialog.get());
                const auto before=collect(dialog.get());chord(dialog.get());const auto after=collect(dialog.get());chord(dialog.get());const auto restored=collect(dialog.get());
                dialog.reset();
                if(!setUiLanguage(alternate).isEmpty())throw std::runtime_error("Cannot load target language");
                auto fresh=make(id);fresh->setAttribute(Qt::WA_DontShowOnScreen);fresh->show();
                coverage[id]=QJsonObject{{"before",before},{"after",after},{"restored",restored},{"fresh",collect(fresh.get())}};
                fresh.reset();
                if(!setUiLanguage("zh").isEmpty())throw std::runtime_error("Cannot restore Chinese");
            }
            workflowArtifacts_["live_window_language"]=coverage;
        }
        else if(kind=="read_surfaces_check")auditReadSurfaces(command["copy_only"].toBool());
        else if(kind=="print_preview_check")auditPrintPreview();
        else if(kind=="menu_interaction_check")auditMenuInteraction();
        else if(kind=="object_lifetime_check")auditObjectLifetime();
        else if(kind=="view_reactivation_check")auditViewReactivation();
        else if(kind=="view_grid_resize_check")auditViewGridResize(command.value("frame_timing").toBool());
        else if(kind=="dependency_fixture"){
            // Private startup verification only: real document operations,
            // without global input or changes to a user's model.
            const auto operation=command["operation"].toString();
            if(operation=="add_markers"||operation=="delete_markers")session_.editModeling(operation,command["fields"].toObject());
            else if(operation=="create_control"&&!session_.model()->edges.isEmpty())session_.editAuxiliary(operation,{{"ids",QJsonArray{double(session_.model()->edges.front().id)}}});
            else if(operation=="lock_first"&&!session_.model()->points.isEmpty())session_.editTopology("set_locked",{{"ids",QJsonArray{double(session_.model()->points.front().id)}},{"locked",command["locked"].toBool()}});
            else{finishWorkflow("Unknown dependency fixture operation");return;}
        }
        else if(kind=="dependency_action"){
            QAction* found=nullptr;for(auto* action:findChildren<QAction*>())if(action->property("operation").toString()==command["operation"].toString())found=action;
            if(!found){finishWorkflow("Missing dependency action");return;}found->trigger();
        }
        else if(kind=="data_dependencies_check"){
            if(command["select_first"].toBool()&&session_.model()&&!session_.model()->points.isEmpty())selectPoints({session_.model()->points.front().id});
            if(command["selection_kind"]=="curve")setSelectionKind(HullEntity::Curve);
            if(command["selection_kind"]=="point")setSelectionKind(HullEntity::Point);
            updateState();const auto model=session_.model();QJsonObject actions;
            for(auto* action:findChildren<QAction*>()){
                auto op=action->property("operation").toString();if(op.isEmpty())continue;
                if(op=="section_visibility")op+=QString::number(action->property("section_kind").toInt());
                actions[op]=QJsonObject{{"enabled",action->isEnabled()},{"checked",action->isChecked()}};
            }
            QJsonArray kinds;if(model)for(const auto& section:model->sections)kinds.append(section.kind);
            QJsonObject row{{"label",command["label"]},{"ready",session_.ready()},{"actions",actions},{"sections",kinds},
                {"markers",model?model->markers.size():0},{"curves",model?model->controlCurves.size():0},{"flows",model?model->flowlines.size():0},
                {"faces",model?model->faces.size():0},{"points",model?model->points.size():0},{"hydro_available",model&&model->overlay.hydroAvailable},{"revision",model?double(model->revision):0}};
            if(model&&model->markers.isEmpty()){
                const auto revision=model->revision;
                requestDisplayChange("show_markers",!model->showMarkers);
                workflowCase(command["label"].toString()+".empty-reference-request-does-not-edit",!session_.busy()&&session_.model()->revision==revision&&queuedDisplay_.isEmpty());
            }
            if(command["plates"].toBool()){
                PlatesDialog dialog(session_,true,this);
                auto controls=[&]{QJsonObject states;for(auto* button:dialog.findChildren<QPushButton*>())if(button->property("plates_action").isValid())states[button->property("plates_action").toString()]=button->isEnabled();
                    for(auto* field:dialog.findChildren<QWidget*>())if(field->property("plate_field").isValid())states[field->property("plate_field").toString()]=field->isEnabled();return states;};
                row["plates_empty"]=controls();
                if(command["plate_result"].isObject()){
                    dialog.receive(command["plate_result"].toObject());row["plates_ready"]=dialog.hasResult();row["plates_populated"]=controls();
                    dialog.findChild<QListWidget*>()->setCurrentRow(-1);row["plates_unselected"]=controls();
                }
            }
            auto rows=workflowArtifacts_["data_dependencies"].toArray();rows.append(row);workflowArtifacts_["data_dependencies"]=rows;
        }
        else if(kind=="menu_groups_check"){
            const auto reviewLanguage=command["review_language"].toString();
            if(!reviewLanguage.isEmpty()){
                const auto error=setUiLanguage(reviewLanguage);
                if(!error.isEmpty()){finishWorkflow(error);return;}
            }
            // This inventory compares final static pixels. Animated intermediate
            // frames are exercised separately by qt-ui-motion.
            const auto motionOverride=qApp->property("studio_motion_enabled");
            qApp->setProperty("studio_motion_enabled",false);
            const auto restoreMotion=qScopeGuard([motionOverride]{qApp->setProperty("studio_motion_enabled",motionOverride);});
            qApp->setQuitOnLastWindowClosed(false);QDir().mkpath(workflowOutput_+"/views");
            const auto originalModel=session_.model();const bool modified=session_.modified();
            QJsonArray menus;QSet<QMenu*> seen;int serial=0;QMenu* directPreview=nullptr;
            const auto inspect=[&](QMenu* menu,const QString& scope,bool picture){
                menu->setAttribute(Qt::WA_DontShowOnScreen);menu->popup({40,40});
                menu->setActiveAction(nullptr);
                const auto rendered=menu->grab().toImage();const auto scale=rendered.devicePixelRatio();
                QJsonArray rows;
                for(auto* action:menu->actions())if(action->isVisible()){
                    const auto rect=menu->actionGeometry(action);
                    const auto tint=action->property("studio_menu_tint").toString();int tintPixels=0;
                    if(QColor(tint).isValid()){
                        const auto pixels=QRectF(rect.topLeft()*scale,QSizeF(rect.size())*scale).toAlignedRect().intersected(rendered.rect());
                        for(int y=pixels.top();y<=pixels.bottom();++y)for(int x=pixels.left();x<=pixels.right();++x)
                            if(rendered.pixelColor(x,y)==QColor(tint))++tintPixels;
                    }
                    const bool formatIcon=action->icon().name().startsWith("studio-format:");
                    const QSize iconSize=formatIcon?QSize(44,24):QSize(16,16);
                    const auto iconRect=QRectF(QPointF(rect.left()+9,rect.center().y()-iconSize.height()/2)*scale,QSizeF(iconSize)*scale).toAlignedRect().intersected(rendered.rect());
                    int green=0,blue=0;QRect inkBounds;for(int y=iconRect.top();y<=iconRect.bottom();++y)for(int x=iconRect.left();x<=iconRect.right();++x){
                        const auto c=rendered.pixelColor(x,y);
                        const bool greenInk=c.red()<155&&c.green()>c.red()*1.15&&c.green()>c.blue()*1.1;
                        const bool blueInk=c.red()<155&&c.blue()>c.red()*1.15&&c.blue()>c.green()*1.05;
                        if(greenInk)++green;if(blueInk)++blue;if(greenInk||blueInk)inkBounds=inkBounds.united(QRect(x,y,1,1));
                    }
                    const auto markerRect=QRectF(QPointF(rect.right()-25,rect.top())*scale,QSizeF(23,rect.height())*scale).toAlignedRect().intersected(rendered.rect());
                    int markerInk=0;for(int y=markerRect.top();y<=markerRect.bottom();++y)for(int x=markerRect.left();x<=markerRect.right();++x){
                        const auto c=rendered.pixelColor(x,y);if(c.lightness()<210)++markerInk;
                    }
                    rows.append(QJsonObject{{"text",action->text()},{"caption",studioMenuCaption(action->text())},
                        {"opens_dialog",studioMenuOpensDialog(action)},{"icon",action->icon().name()},{"icon_present",!action->icon().isNull()},{"icon_green_pixels",green},{"icon_blue_pixels",blue},
                        {"explicit_dialog",action->property("studio_opens_dialog").isValid()&&action->property("studio_opens_dialog").toBool()},
                        {"icon_ink_width",inkBounds.width()/scale},{"marker_ink_pixels",markerInk},
                        {"checkable",action->isCheckable()},
                        {"checked",action->isChecked()},{"enabled",action->isEnabled()},{"submenu",bool(action->menu())},
                        {"separator",action->isSeparator()},{"y",rect.y()},{"height",rect.height()},
                        {"operation",action->property("operation").toString()},
                        {"view_operation",action->property("view_operation").toString()},
                        {"background_operation",action->property("background_operation").toString()},
                        {"file_operation",action->property("file_operation").toString()},
                        {"tint",tint},{"tint_pixels",tintPixels}});
                }
                const auto path=workflowOutput_+"/views/menu-"+QString::number(serial++)+".png";
                if(picture)menu->grab().save(path);
                const QColor background(menu->property("studio_menu_background").toString());int backgroundPixels=0;
                if(background.isValid())for(int y=0;y<rendered.height();++y)for(int x=0;x<rendered.width();++x)if(rendered.pixelColor(x,y)==background)++backgroundPixels;
                menus.append(QJsonObject{{"scope",scope},{"rows",rows},{"width",menu->width()},
                    {"height",menu->height()},{"background",background.name()},{"background_pixels",backgroundPixels},{"image",picture?path:QString{}}});menu->hide();
            };
            std::function<void(QMenu*,QString)> walk=[&](QMenu* menu,QString scope){
                if(scope=="StudioPoint")directPreview=menu;
                if(seen.contains(menu))return;seen.insert(menu);
                const auto before=menu->actions();menu->ensurePolished();
                QSet<QAction*> oldActions,newActions;
                for(auto* a:before)if(!a->isSeparator())oldActions.insert(a);
                for(auto* a:menu->actions())if(!a->isSeparator())newActions.insert(a);
                workflowCase("menu-action-identities-"+QString::number(seen.size()),oldActions==newActions);
                // aboutToShow may legitimately rebuild recent-file entries;
                // pointer identity above concerns the shared style's polish.
                inspect(menu,scope,true);
                for(auto* a:menu->actions())if(auto* child=a->menu())walk(child,scope+"/"+a->text());
            };
            for(auto* button:menuWidget()->findChildren<QToolButton*>()){
                if(auto* action=button->defaultAction();action&&action->menu())walk(action->menu(),action->objectName());
                if(button->menu())walk(button->menu(),button->objectName().isEmpty()?"toolbar-overflow":button->objectName());
            }
            for(auto* view:views_){
                view->setAttribute(Qt::WA_DontShowOnScreen);view->finishPresentation();
                QContextMenuEvent event(QContextMenuEvent::Mouse,{100,100},{100,100});QApplication::sendEvent(view,&event);
                for(auto* menu:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(menu->isVisible())
                    walk(menu,"viewport-"+QString::number(int(view->projection())));
            }
            QJsonArray toggleStates;
            for(auto* menu:seen)for(auto* action:menu->actions())if(action->property("operation")=="overlay_grid"){
                const bool before=action->isChecked();
                for(int i=0;i<3;++i){
                    if(i)action->trigger();
                    inspect(menu,"visibility-toggle-"+QString::number(i),true);
                    QJsonArray viewStates;for(auto* view:views_)viewStates.append(view->gridVisible());
                    toggleStates.append(QJsonObject{{"checked",action->isChecked()},{"enabled",action->isEnabled()},{"views",viewStates}});
                }
                workflowCase("menu-toggle-restored",action->isChecked()==before);
            }
            // Compare native row painting with every state in the same gutter.
            QMenu sample;sample.ensurePolished();QJsonObject marks;
            for(const auto& state:QStringList{"plain","disabled-plain","dialog","dialog-unicode","disabled-dialog","off","on","disabled-off","disabled-on","submenu"}){
                QImage image(240,32,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);
                QStyleOptionMenuItem option;option.initFrom(&sample);option.rect={0,0,240,32};option.text="Sample";option.font=sample.font();
                const bool dialog=state.contains("dialog");
                if(dialog)option.text+=state=="dialog-unicode"?QString(QChar(0x2026)):QString("...");
                option.menuItemType=state=="submenu"?QStyleOptionMenuItem::SubMenu:QStyleOptionMenuItem::Normal;
                option.checkType=state.endsWith("plain")||state=="submenu"||dialog?QStyleOptionMenuItem::NotCheckable:QStyleOptionMenuItem::NonExclusive;
                option.checked=state.endsWith("on");option.state.setFlag(QStyle::State_Enabled,!state.startsWith("disabled"));
                sample.style()->drawControl(QStyle::CE_MenuItem,&option,&p,&sample);p.end();
                const auto path=workflowOutput_+"/views/state-"+state+".png";image.save(path);int ink=0;
                for(int y=0;y<image.height();++y)for(int x=212;x<239;++x)if(image.pixelColor(x,y)!=QColor(Qt::white))++ink;
                int darkest=255;for(int y=0;y<image.height();++y)for(int x=35;x<210;++x)darkest=qMin(darkest,image.pixelColor(x,y).lightness());
                marks[state]=QJsonObject{{"image",path},{"gutter_pixels",ink},{"text_min_lightness",darkest}};
            }
            if(directPreview){
                directPreview->popup({40,40});directPreview->setActiveAction(nullptr);
                const auto path=workflowOutput_+"/views/direct-action-menu.png";
                directPreview->grab().save(path);directPreview->hide();
                workflowArtifacts_["direct_action_menu_image"]=path;
            }
            QMenu mixed;auto* state=mixed.addAction("State");state->setCheckable(true);
            mixed.addMenu("Submenu");auto* details=mixed.addAction("Details...");mixed.addAction("Direct");
            // Match production callbacks: the controller owns the connection.
            // Qt removes QAction -> QMenu connections when detaching a row;
            // a QMenu receiver here would test that Qt behavior, not our bindings.
            int activations=0;connect(details,&QAction::triggered,this,[&]{++activations;});
            details->trigger();const int beforeActivation=activations;const bool beforeEnabled=details->isEnabled();
            mixed.setAttribute(Qt::WA_DontShowOnScreen);mixed.popup({40,40});
            QJsonArray order;for(auto* a:mixed.actions())if(!a->isSeparator())order.append(studioMenuCaption(a->text()));
            details->trigger();mixed.hide();
            workflowArtifacts_["mixed_menu_order"]=order;
            workflowArtifacts_["mixed_menu_activation"]=QJsonObject{{"before",beforeActivation},{"after",activations},{"before_enabled",beforeEnabled},{"after_enabled",details->isEnabled()}};
            workflowCase("menu-grouping-preserves-action-activation",beforeActivation==1&&activations==2);
            workflowArtifacts_["menu_groups"]=menus;workflowArtifacts_["menu_toggle_states"]=toggleStates;workflowArtifacts_["menu_marks"]=marks;
            workflowCase("menus-preserve-model",session_.model()==originalModel&&session_.modified()==modified);
        }
        else if(kind=="typography_check"){
            const auto stage=QString::number(startupCommand_);ensurePolished();resize(1536,864);
            if(layout())layout()->activate();if(menuWidget()->layout())menuWidget()->layout()->activate();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            for(auto* view:views_){view->setAnimationsEnabled(false);view->finishPresentation();view->fit();}
            QJsonArray views;for(auto* view:views_){view->image();views.append(QJsonObject{{"title",view->projectionLabel()},{"evidence",view->overlayEvidence()}});}
            QDir().mkpath(workflowOutput_+"/views");const auto image=workflowOutput_+"/views/typography-"+stage+".png";grab().save(image);
            auto* title=findChild<QLabel*>("StudioCaptionTitle");
            QJsonObject sample{{"title",windowTitle()},{"source",session_.sourcePath()},{"modified",session_.modified()},
                {"application_font",QApplication::font().family()},{"application_point_size",QApplication::font().pointSizeF()},
                {"status_bar_hidden",statusBar()->isHidden()},{"central_bottom",centralWidget()->geometry().bottom()},{"window_bottom",rect().bottom()},
                {"caption_tooltip",title?title->parentWidget()->toolTip():QString{}},{"views",views},{"image",image}};
            if(command["feedback"].toBool()){
                showStatusMessage("Private typography feedback",StudioMessageRole::Feedback);
                auto* notice=findChild<QWidget*>("ShellStatusNotice");
                sample["notice_visible"]=notice&&!notice->isHidden();sample["notice_role"]=notice?notice->property("studio_message_role").toString():QString{};
                sample["notice_inside"]=notice&&notice->parentWidget()->contentsRect().contains(notice->geometry());
                const auto before=centralWidget()->geometry();QEventLoop wait;QTimer::singleShot(4700,&wait,&QEventLoop::quit);wait.exec();
                sample["notice_expired"]=notice&&notice->isHidden();sample["notice_did_not_reflow"]=before==centralWidget()->geometry();
            }
            auto samples=workflowArtifacts_["typography"].toArray();samples.append(sample);workflowArtifacts_["typography"]=samples;
        }
        else if(kind=="toolbar_layout_check"){
            const auto initialLanguage=uiLanguage();
            const auto initialSize=size();
            QJsonArray layouts;QDir().mkpath(workflowOutput_+"/views");
            auto* rail=studioBar_->sideBar();
            const auto rectInWindow=[this](QWidget* widget){return QRect(widget->mapTo(this,QPoint()),widget->size());};
            const auto observeRect=[](const QRect& r){return QJsonObject{{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()}};};
            const auto arrange=[&](int width,int height){
                resize(width,height);if(layout())layout()->activate();
                if(menuWidget()->layout())menuWidget()->layout()->activate();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                studioBar_->setIconSize(studioBar_->height()-38);
                if(layout())layout()->activate();
            };
            for(const auto& info:uiLanguages()){
                const auto language=info.toObject()["id"].toString();
                if(!setUiLanguage(language).isEmpty())throw std::runtime_error("Toolbar language unavailable");
                for(const int width:{2400,1920,1600,1280,1024,680}){
                    arrange(width,900);
                    const auto toolbarImage=studioBar_->grab().toImage();
                    const auto railImage=rail->isHidden()?QImage{}:rail->grab().toImage();
                    QMap<int,QWidget*> topPanels,sidePanels;
                    QJsonArray buttons;
                    for(auto* button:studioBar_->buttons()){
                        auto* panel=button->parentWidget();
                        const bool side=rail->isAncestorOf(button);
                        const int group=panel->property("studio_toolbar_group").toInt();
                        (side?sidePanels:topPanels)[group]=panel;
                        auto row=observeRect(button->geometry());
                        row["group"]=group;row["side"]=side;row["text"]=button->text();
                        row["tooltip"]=button->toolTip();row["icon_null"]=button->icon().isNull();
                        row["visible"]=button->isVisibleTo(this);
                        const auto verticalSize=studio_motion::verticalToolButtonSize(*button);
                        row["required_width"]=side?verticalSize.width():std::max({58,button->iconSize().width()+18,
                            QFontMetrics(button->font()).horizontalAdvance(button->text())+18});
                        row["required_height"]=side?verticalSize.height():button->iconSize().height()+28;
                        row["one_character_width"]=button->iconSize().width()+button->fontMetrics().height()+22;
                        row["label_layout"]=button->property("studio_vertical_toolbar_label").toBool()?"vertical_right":
                            button->toolButtonStyle()==Qt::ToolButtonTextUnderIcon?"under_icon":"other";
                        if(auto* action=button->defaultAction(); action&&action->property("operation")=="about"){
                            const auto previous=action->property("update_available");
                            action->setProperty("update_available",false);
                            const auto idle=button->grab().toImage();
                            action->setProperty("update_available",true);
                            const auto notified=button->grab().toImage();
                            action->setProperty("update_available",previous);
                            button->update();
                            QRect changed;
                            if(idle.size()==notified.size())for(int y=0;y<idle.height();++y)for(int x=0;x<idle.width();++x){
                                const auto before=idle.pixel(x,y),after=notified.pixel(x,y);
                                if(std::max({std::abs(qRed(before)-qRed(after)),std::abs(qGreen(before)-qGreen(after)),
                                             std::abs(qBlue(before)-qBlue(after))})>8)changed|=QRect(x,y,1,1);
                            }
                            const qreal scale=notified.devicePixelRatio();
                            row["badge_pixels"]=observeRect(QRectF(QPointF(changed.topLeft())/scale,QSizeF(changed.size())/scale).toAlignedRect());
                            if(width==680||width==2400){
                                const auto path=workflowOutput_+"/views/update-dot-"+language+"-"+QString::number(width)+".png";
                                if(notified.save(path))row["badge_image"]=path;
                            }
                        }
                        buttons.append(row);
                    }
                    const auto observePanels=[&](const QMap<int,QWidget*>& panels,QWidget* surface,const QImage& pixels){
                        QJsonArray result;
                        for(auto it=panels.cbegin();it!=panels.cend();++it){
                            auto* panel=it.value();auto row=observeRect(rectInWindow(panel));
                            const auto pos=(QPointF(panel->mapTo(surface,QPoint(1,1)))*pixels.devicePixelRatio()).toPoint();
                            row["group"]=it.key();row["background"]=panel->palette().color(QPalette::Window).name();
                            if(pixels.rect().contains(pos))row["painted_background"]=pixels.pixelColor(pos).name();
                            else{
                                // A tall rail can place the next group beyond its scroll viewport.
                                const auto swatch=panel->grab(QRect(0,0,3,3)).toImage();
                                if(!swatch.isNull())row["painted_background"]=swatch.pixelColor(swatch.rect().center()).name();
                            }
                            result.append(row);
                        }
                        return result;
                    };
                    QJsonObject sample{{"language",language},{"requested_width",width},{"width",studioBar_->width()},
                        {"top",observePanels(topPanels,studioBar_,toolbarImage)},{"side",observePanels(sidePanels,rail,railImage)},
                        {"buttons",buttons},{"rail_visible",!rail->isHidden()},{"rail",observeRect(rectInWindow(rail))},
                        {"toolbar",observeRect(rectInWindow(studioBar_))},{"canvas",observeRect(rectInWindow(centralWidget()))}};
                    if((language=="ja"||language=="en"||language=="ru")&&
                       (width==2400||width==1920||width==1600||width==1280||width==1024||width==680)){
                        const auto path=workflowOutput_+"/views/toolbar-"+language+"-"+QString::number(width)+".png";
                        grab().save(path);sample["image"]=path;
                    }
                    layouts.append(sample);
                }
            }
            setUiLanguage("en");
            const bool wasVisible=isVisible();
            const bool wasOffscreen=testAttribute(Qt::WA_DontShowOnScreen);
            setAttribute(Qt::WA_DontShowOnScreen);show();
            arrange(680,460);
            auto* scroll=rail->findChild<QScrollArea*>("StudioSideScroll");
            workflowCase("toolbar.short-window-rail-can-scroll",scroll&&scroll->verticalScrollBar()->maximum()>0);
            if(scroll){
                auto* last=studioBar_->buttons().last();
                scroll->ensureWidgetVisible(last,0,2);
                workflowCase("toolbar.scroll-reaches-last-action",scroll->viewport()->rect().contains(
                    scroll->viewport()->mapFromGlobal(last->mapToGlobal(last->rect().center()))));
            }
            arrange(680,900);
            if(scroll)scroll->verticalScrollBar()->setValue(0);
            const auto buttons=studioBar_->buttons();
            const auto target=std::find_if(buttons.cbegin(),buttons.cend(),[rail](auto* button){
                return rail->isAncestorOf(button)&&button->isEnabled()&&button->defaultAction()->menu();
            });
            workflowCase("toolbar.side-menu-available",target!=buttons.cend());
            if(target!=buttons.cend()){
                auto* button=*target;auto* menu=button->defaultAction()->menu();
                const QPoint pos=button->rect().center(),global=button->mapToGlobal(pos);
                QMouseEvent move(QEvent::MouseMove,pos,global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(button,&move);
                QEventLoop wait;QTimer::singleShot(160,&wait,&QEventLoop::quit);wait.exec();
                workflowCase("toolbar.hover-opens-side-menu",menu->isVisible());
                workflowCase("toolbar.side-menu-opens-inward",menu->isVisible()&&menu->frameGeometry().right()<=button->mapToGlobal(QPoint()).x());
                menu->close();
                QMouseEvent press(QEvent::MouseButtonPress,pos,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(button,&press);
                QMouseEvent release(QEvent::MouseButtonRelease,pos,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(button,&release);
                workflowCase("toolbar.click-opens-side-menu",menu->isVisible());menu->close();
            }
            if(!wasVisible)hide();
            setAttribute(Qt::WA_DontShowOnScreen,wasOffscreen);
            setUiLanguage(initialLanguage);
            arrange(initialSize.width(),initialSize.height());
            workflowArtifacts_["toolbar_layouts"]=layouts;
        }
        else if(kind=="overlay_language_check"){
            const auto original=session_.model();
            if(!original){finishWorkflow("Overlay language test needs a production model");return;}
            auto fixture=std::make_shared<fs::ModelSnapshot>(*original);
            fixture->overlay.present=true;
            fixture->overlay.hydroAvailable=true;
            fixture->overlay.markers.clear();
            const auto lo=fixture->minimum,hi=fixture->maximum;
            fixture->overlay.markers.append({"lateral","侧面积=12.34",{lo.x+(hi.x-lo.x)*.33,lo.y,(lo.z+hi.z)*.5}});
            fixture->overlay.markers.append({"wind","横向受风面积=56.78",{lo.x+(hi.x-lo.x)*.67,hi.y,(lo.z+hi.z)*.5}});
            const auto initialLanguage=uiLanguage();
            HullView view;
            view.setAttribute(Qt::WA_DontShowOnScreen);
            view.resize(1080,680);
            view.setAnimationsEnabled(false);
            view.setSnapshot(fixture);
            view.setFillVisible(false);
            view.setGridVisible(false);
            view.setControlNetVisible(false);
            view.setHydroVisible(true);
            QJsonObject languages;
            for(const auto& info:uiLanguages()){
                const auto language=info.toObject()["id"].toString();
                if(!setUiLanguage(language).isEmpty())throw std::runtime_error("Overlay language unavailable");
                QJsonArray projections;
                for(const auto projection:{HullProjection::Body,HullProjection::Profile,HullProjection::Plan,HullProjection::Perspective}){
                    view.setProjection(projection);
                    view.fit();
                    view.image();
                    QJsonObject markers;
                    for(const auto& value:view.overlayEvidence()["labels"].toArray()){
                        const auto label=value.toObject();
                        const auto key=label["key"].toString();
                        if(key.startsWith("hydro:"))markers[key.mid(6)]=label["text"];
                    }
                    projections.append(markers);
                }
                languages[language]=projections;
            }
            setUiLanguage(initialLanguage);
            workflowArtifacts_["overlay_marker_languages"]=languages;
        }
        else if(kind=="shortcut_input_check"){
            show();
            QApplication::setActiveWindow(this);
            auto* perspective=views_.back();
            perspective->setFocus();
            QWidget* keyTarget=perspective;
            const fs::Vec3 reference{1,2,3};
            std::array<QPointF,4> fitted;
            for(size_t i=0;i<views_.size();++i){
                views_[i]->setAnimationsEnabled(false);
                views_[i]->fit();
                fitted[i]=views_[i]->project(reference);
                views_[i]->panBy(QPointF(35,22));
            }
            const auto pressKey=[&](int key,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
                QKeyEvent press(QEvent::KeyPress,key,modifiers);
                QApplication::sendEvent(keyTarget,&press);
                QKeyEvent release(QEvent::KeyRelease,key,Qt::NoModifier);
                QApplication::sendEvent(keyTarget,&release);
            };
            const auto panned=perspective->project(reference);
            pressKey(Qt::Key_Home);
            workflowCase("shortcut.home-does-not-fit",panned==perspective->project(reference));
            pressKey(Qt::Key_F);
            bool allFit=true;
            for(size_t i=0;i<views_.size();++i)allFit&=views_[i]->project(reference)==fitted[i];
            workflowCase("shortcut.f-fits-all-four-views",allFit);

            const auto drag=[](HullView* view,QPointF from,QPointF to,Qt::KeyboardModifiers modifiers,bool sendMove=true){
                QMouseEvent press(QEvent::MouseButtonPress,from,from,Qt::LeftButton,Qt::LeftButton,modifiers);
                QMouseEvent move(QEvent::MouseMove,to,to,Qt::NoButton,Qt::LeftButton,modifiers);
                QMouseEvent release(QEvent::MouseButtonRelease,to,to,Qt::LeftButton,Qt::NoButton,modifiers);
                QApplication::sendEvent(view,&press);
                if(sendMove)QApplication::sendEvent(view,&move);
                QApplication::sendEvent(view,&release);
            };
            setSelectionKind(HullEntity::Point);
            for(auto* view:{views_[0],perspective}){
                view->setControlNetVisible(true);view->setFillVisible(false);view->fit();
                // Start at a blank margin and enclose visible points. Compare
                // a projected mark as well as selection to distinguish orbit.
                QPointF from(1,1),to(view->width()-2,view->height()-2);
                const auto before=view->project(reference);
                workflowCase(view==perspective?"shortcut.perspective-drag-starts-on-blank":"shortcut.orthographic-drag-starts-on-blank",!view->entityAt(from));
                drag(view,from,to,Qt::NoModifier);
                workflowCase(view==perspective?"shortcut.perspective-left-box-selects":"shortcut.orthographic-left-box-selects",
                    view->project(reference)==before&&!view->selectedPoints().isEmpty());
            }
            const QPointF from(1,1),to=from+QPointF(60,20);
            const auto beforeDrag=perspective->project(reference);
            drag(perspective,from,to,Qt::AltModifier);
            workflowCase("shortcut.alt-left-does-not-rotate",perspective->project(reference)==beforeDrag);
            drag(perspective,from,QPointF(perspective->width()-2,perspective->height()-2),Qt::NoModifier);
            const auto selected=perspective->selectedPoints();
            QPointF picked;fs::EntityId hit=0;
            for(const auto& pt:session_.model()->points){
                const auto pos=perspective->project(pt.position);
                const auto id=perspective->entityAt(pos);
                if(selected.contains(id)){picked=pos;hit=id;break;}
            }
            workflowCase("shortcut.perspective-click-fixture",hit!=0);
            if(hit){
                const auto beforeClick=perspective->project(reference);
                drag(perspective,picked,picked+QPointF(1,0),Qt::ControlModifier);
                auto without=selected;without.remove(hit);
                workflowCase("shortcut.ctrl-left-click-still-toggles-point",perspective->selectedPoints()==without&&perspective->project(reference)==beforeClick);
                drag(perspective,picked,picked+QPointF(32,14),Qt::ControlModifier,false);
                workflowCase("shortcut.ctrl-left-drag-over-point-orbits-without-selecting",perspective->selectedPoints()==without&&perspective->project(reference)!=beforeClick);
            }
            const auto beforeBlankOrbit=perspective->project(reference);
            const auto beforeBlankSelection=perspective->selectedPoints();
            drag(perspective,from,to,Qt::ControlModifier);
            workflowCase("shortcut.ctrl-left-drag-in-blank-orbits-without-selecting",
                perspective->project(reference)!=beforeBlankOrbit&&perspective->selectedPoints()==beforeBlankSelection);
            const auto axesMatch=[&](std::array<int,4> expected){
                for(size_t index=0;index<views_.size();++index)
                    if(views_[index]->coordinateAxis()!=expected[index])return false;
                return true;
            };
            pressKey(Qt::Key_X,Qt::ControlModifier|Qt::AltModifier);
            workflowCase("shortcut.legacy-axis-binding-removed",axesMatch({0,0,0,0}));
            const auto modifiers=Qt::ControlModifier|Qt::ShiftModifier;
            pressKey(Qt::Key_1,modifiers);
            workflowCase("shortcut.coordinate-affects-only-focused-view",axesMatch({0,0,0,1}));
            keyTarget=views_[0];
            keyTarget->setFocus();
            pressKey(Qt::Key_2,modifiers);
            workflowCase("shortcut.coordinate-follows-new-focus",axesMatch({2,0,0,1}));
            pressKey(Qt::Key_3,modifiers);
            workflowCase("shortcut.coordinate-changes-axis-locally",axesMatch({3,0,0,1}));
            pressKey(Qt::Key_3,modifiers);
            workflowCase("shortcut.coordinate-repeat-hides-only-focused-view",axesMatch({0,0,0,1}));
        }
        else if(kind=="shortcuts_check"){
            qApp->setQuitOnLastWindowClosed(false);
            const auto requestedLanguage=command["language"].toString();
            const auto previousLanguage=uiLanguage();
            if(!requestedLanguage.isEmpty())setUiLanguage(requestedLanguage);
            const auto suffix=requestedLanguage.isEmpty()?QString{}:"-"+requestedLanguage;
            QTimer::singleShot(0,this,[this,suffix]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                auto* table=dialog?dialog->findChild<QTableWidget*>("ShortcutGuideTable"):nullptr;
                if(!table){finishWorkflow("Shortcut guide missing");return;}
                QJsonArray rows,groups,ctrlGestures;QJsonObject scopes;int previewCount=0,viewGroup=-1;
                for(int row=0;row<table->rowCount();++row){
                    if(table->item(row,0)->data(Qt::UserRole)=="group"){
                        if(table->item(row,0)->text()==uiText("视图"))viewGroup=row;
                        groups.append(table->item(row,0)->text());
                        continue;
                    }
                    QJsonArray cells;
                    if(auto* cell=table->cellWidget(row,1)){
                        if(cell->findChild<QWidget*>("ShortcutGesturePreview"))++previewCount;
                        if(auto* key=cell->findChild<QWidget*>("StudioKeycap");key&&key->accessibleName()=="Ctrl")
                            ctrlGestures.append(table->item(row,0)->text());
                    }
                    if(auto* scope=table->cellWidget(row,2)){
                        QJsonArray icons;
                        for(auto* icon:scope->findChildren<QLabel*>("ShortcutViewIcon"))
                            if(!icon->pixmap().isNull())icons.append(icon->property("shortcut_view_icon").toString());
                        scopes[table->item(row,1)->text()]=icons;
                    }
                    for(int col=0;col<table->columnCount();++col)cells.append(table->item(row,col)->text());rows.append(cells);}
                workflowArtifacts_["shortcut_rows"+suffix]=rows;
                workflowArtifacts_["shortcut_groups"+suffix]=groups;
                workflowArtifacts_["shortcut_gesture_previews"+suffix]=previewCount;
                workflowArtifacts_["shortcut_scope_icons"+suffix]=scopes;
                workflowArtifacts_["shortcut_ctrl_gestures"+suffix]=ctrlGestures;
                workflowCase("shortcut-header-hidden"+suffix,table->horizontalHeader()->isHidden());
                QDir().mkpath(workflowOutput_+"/views");
                const auto wait=[](int ms){
                    QEventLoop loop;
                    QTimer::singleShot(ms,&loop,&QEventLoop::quit);
                    loop.exec();
                };
                wait(220);
                workflowCase("shortcut-guide-picture",dialog->grab().save(workflowOutput_+"/views/shortcuts"+suffix+".png"));
                if(viewGroup>=0){
                    table->scrollToItem(table->item(viewGroup,0),QAbstractItemView::PositionAtTop);
                    wait(80);
                    const auto first=table->viewport()->grab().toImage();
                    wait(255);
                    workflowCase("shortcut-gesture-frames-change",first!=table->viewport()->grab().toImage());
                    workflowCase("shortcut-gesture-picture",dialog->grab().save(workflowOutput_+"/views/shortcuts-gestures"+suffix+".png"));
                }
                if(!suffix.isEmpty()){dialog->reject();return;}
                workflowCase("shortcut.save-and-delete-bindings-retained",save_->shortcuts().contains(QKeySequence("Ctrl+S"))&&delete_->shortcut()==QKeySequence(Qt::Key_Delete));
                workflowArtifacts_["save_copy_text"]=saveAs_->text();
                workflowArtifacts_["save_copy_key"]=saveAs_->shortcut().toString(QKeySequence::PortableText);
                menuWidget()->resize(1920,menuWidget()->height());
                workflowCase("shortcut-toolbar-picture",menuWidget()->grab().save(workflowOutput_+"/views/toolbar.png"));
                QToolButton *guide=nullptr,*about=nullptr;
                for(auto* button:menuWidget()->findChildren<QToolButton*>())if(auto* a=button->defaultAction()){
                    if(a->property("operation")=="shortcut_guide")guide=button;
                    if(a->property("operation")=="about")about=button;
                }
                workflowCase("shortcut-toolbar-position",guide&&about&&guide->geometry().right()<about->geometry().left());
                workflowCase("shortcut-toolbar-icon",guide&&!guide->icon().isNull());
                if(guide){
                    const auto pixels=guide->icon().pixmap(QSize(48,48)).toImage();
                    workflowCase("shortcut-icon-transparent",!pixels.isNull()&&pixels.pixelColor(0,0).alpha()==0);
                    workflowCase("shortcut-icon-picture",pixels.save(workflowOutput_+"/views/keyboard.png"));
                }
                dialog->reject();
            });
            QList<QAction*> found;for(auto* a:findChildren<QAction*>())if(a->property("operation")=="shortcut_guide")found.append(a);
            if(found.size()!=1){finishWorkflow("Shortcut guide action missing or ambiguous");return;}
            found[0]->trigger();
            if(!requestedLanguage.isEmpty()){setUiLanguage(previousLanguage);startupStep("idle");return;}
            for(auto* menu:findChildren<QMenu*>())if(menu->actions().contains(saveAs_)){
                menu->ensurePolished();menu->resize(menu->sizeHint());
                workflowCase("file-menu-picture",menu->grab().save(workflowOutput_+"/views/file-menu.png"));
                // Compare the actual menu painter in an identical row rectangle.
                // Removing QAction bindings also changes Qt's submenu geometry.
                const auto renderRow=[menu](const QString& caption){
                    QImage image(280,32,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
                    QPainter painter(&image);QStyleOptionMenuItem option;option.initFrom(menu);
                    option.rect=QRect(0,0,280,32);option.menuItemType=QStyleOptionMenuItem::Normal;
                    option.checkType=QStyleOptionMenuItem::NotCheckable;option.text=caption;option.font=menu->font();
                    menu->style()->drawControl(QStyle::CE_MenuItem,&option,&painter,menu);return image;
                };
                workflowCase("menu-shortcuts-hidden",renderRow("Save\tCtrl+S")==renderRow("Save"));
                for(auto* item:menu->actions())if(auto* child=item->menu())
                    if(child->styleSheet().contains("#edf3f7")){
                        child->ensurePolished();child->resize(child->sizeHint());
                        workflowCase("import-menu-picture",child->grab().save(workflowOutput_+"/views/import-menu.png"));
                    }
            }
        }
        else if(kind=="about_check"){
            qApp->setQuitOnLastWindowClosed(false);
            const auto previousLanguage=uiLanguage();
            const auto requestedLanguage=command["language"].toString();
            if(!requestedLanguage.isEmpty()&&!setUiLanguage(requestedLanguage).isEmpty()){
                finishWorkflow("About test language unavailable");return;
            }
            QTimer::singleShot(0,this,[this]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog){finishWorkflow("About window missing");return;}
                auto* table=dialog->findChild<QTableWidget*>("AboutCredits");
                workflowCase("about-credits-table",table&&table->columnCount()>0);
                if(table){
                    const int yearColumn=table->property("credit_year_column").toInt(),nameColumn=table->property("credit_name_column").toInt(),workColumn=table->property("credit_work_column").toInt();
                    QJsonArray rows;
                    QJsonArray logos;
                    for(int row=0;row<table->rowCount();++row){
                        QJsonArray cells;
                        for(int column:{yearColumn,nameColumn,workColumn})
                            cells.append(table->item(row,column)?table->item(row,column)->text():QString{});
                        rows.append(cells);
                        for(int column=0;column<table->columnCount();++column){
                            const auto* cell=table->item(row,column);
                            if(!cell||cell->data(Qt::UserRole).toString().isEmpty())continue;
                            const auto image=cell->icon().pixmap(QSize(56,56)).toImage();
                            int pixels=0,inkPixels=0,purplePixels=0;QRect inkBounds;
                            for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
                                {const auto pixel=image.pixelColor(x,y);if(pixel.alpha()>0)++pixels;
                                    if(pixel.alpha()>40&&qMin(pixel.red(),qMin(pixel.green(),pixel.blue()))<238){++inkPixels;inkBounds=inkBounds.united(QRect(x,y,1,1));}
                                    if(pixel.alpha()>40&&pixel.blue()>pixel.red()+30&&pixel.red()>pixel.green()+10)++purplePixels;
                                }
                            logos.append(QJsonObject{{"text",cell->text()},{"vendor",cell->data(Qt::UserRole).toString()},
                                {"width",image.width()},{"height",image.height()},{"visible_pixels",pixels},
                                {"ink_pixels",inkPixels},{"ink_width",inkBounds.width()},{"ink_height",inkBounds.height()},
                                {"ink_center_x",QRectF(inkBounds).center().x()},{"ink_center_y",QRectF(inkBounds).center().y()},{"purple_pixels",purplePixels}});
                            if(cell->data(Qt::UserRole)=="qwen"){
                                QFile resource(":/ai-vendors/qwen.png");resource.open(QIODevice::ReadOnly);
                                workflowArtifacts_["qwen_embedded_sha256"]=QString::fromLatin1(QCryptographicHash::hash(resource.readAll(),QCryptographicHash::Sha256).toHex());
                                QDir().mkpath(workflowOutput_+"/views");image.save(workflowOutput_+"/views/qwen-rendered.png");
                            }
                        }
                    }
                    workflowArtifacts_["about_credits"]=rows;
                    QJsonObject authorNames=workflowArtifacts_["about_author_names"].toObject();
                    auto* identity=table->cellWidget(table->rowCount()-1,nameColumn);
                    if(identity)for(auto* label:identity->findChildren<QLabel*>())
                        if(label->property("credit_part")=="name")authorNames[uiLanguage()]=QJsonObject{{"credit",label->text()}};
                    workflowArtifacts_["about_author_names"]=authorNames;
                    workflowArtifacts_["about_vendor_logos"]=logos;
                    QDir().mkpath(workflowOutput_+"/views");
                    // Capture the settled window, not a frame of the feature-window entrance veil.
                    {QPointer<QDialog> guard(dialog);QElapsedTimer settle;settle.start();
                        while(guard&&guard->property("studio_window_motion_active").toBool()&&settle.elapsed()<3000)
                            QCoreApplication::processEvents(QEventLoop::AllEvents,16);
                        if(!guard){finishWorkflow("About window closed during capture");return;}}
                    workflowCase("about-entrance-settled",!dialog->property("studio_window_motion_active").toBool());
                    workflowCase("about-picture",dialog->grab().save(workflowOutput_+"/views/about.png"));
                    workflowArtifacts_["about_credits_scroll_max"]=table->verticalScrollBar()->maximum();
                    workflowArtifacts_["about_headers_hidden"]=table->horizontalHeader()->isHidden();
                    QJsonArray styles;
                    int firstCredit=0;while(firstCredit<table->rowCount()&&(!table->item(firstCredit,nameColumn)||table->item(firstCredit,nameColumn)->text().isEmpty()))++firstCredit;
                    for(int column:{yearColumn,nameColumn,workColumn}){
                        const auto* cell=table->item(firstCredit,column);
                        const QFontInfo resolved(cell->font());
                        styles.append(QJsonObject{{"point_size",cell->font().pointSizeF()},{"weight",cell->font().weight()},
                            {"color",cell->foreground().color().name()},{"family",cell->font().family()},
                            {"resolved_family",resolved.family()},{"resolved_style",resolved.styleName()},
                            {"resolved_weight",resolved.weight()}});
                    }
                    workflowArtifacts_["about_column_styles"]=styles;
                }
                auto* brand=dialog->findChild<QLabel*>("AboutProductLogo");
                auto* title=dialog->findChild<QLabel*>("AboutProductName");
                workflowCase("about-product-logo",brand&&!brand->pixmap().isNull());
                if(brand&&title){
                    const auto bounds=brand->geometry().united(title->geometry());
                    workflowCase("about-masthead-centered",qAbs(bounds.center().x()-brand->parentWidget()->rect().center().x())<=1);
                }
                for(bool version:{true,false}){
                    auto* button=dialog->findChild<QPushButton*>(version?"AboutVersionToggle":"AboutCopyrightToggle");
                    workflowCase(version?"about-version-button":"about-license-button",button!=nullptr);if(!button)continue;
                    QTimer::singleShot(0,this,[this,version]{
                        auto* child=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                        auto* grid=child?child->findChild<QTableWidget*>("InformationGrid"):nullptr;
                        workflowCase(version?"about-version-details":"about-license-details",grid&&grid->rowCount()==(version?6:4));
                        if(!version&&grid)if(auto* identity=grid->cellWidget(grid->rowCount()-1,0)){
                            auto names=workflowArtifacts_["about_author_names"].toObject();
                            auto current=names[uiLanguage()].toObject();
                            for(auto* label:identity->findChildren<QLabel*>())if(label->property("person_name").toBool())current["license"]=label->text();
                            names[uiLanguage()]=current;workflowArtifacts_["about_author_names"]=names;
                        }
                        if(child)child->reject();
                    });button->click();
                }
                QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(dialog,&escape);workflowCase("about-escape-closes",!dialog->isVisible());
            });
            QList<QAction*> found;for(auto* a:findChildren<QAction*>())if(a->property("operation")=="about")found.append(a);
            if(found.size()!=1){finishWorkflow("About action missing");return;}found[0]->trigger();
            if(!requestedLanguage.isEmpty())setUiLanguage(previousLanguage);
        }
        else if(kind=="startup")startSession(path);
        else if(kind=="canvas_notice_check"){
            auto* split=qobject_cast<QSplitter*>(centralWidget());
            if(!split){finishWorkflow("Workspace splitter missing");return;}
            resize(1100,700);split->widget(1)->show();split->setSizes({830,270});
            qApp->setProperty("studio_motion_enabled",true);
            // The close workflow can now use its own confirmation. Exercise
            // the screenshot's exact translated text through the shared notice.
            showStatusMessage(uiText("请等当前模型操作完成后再关闭。"),StudioMessageRole::Warning);
            auto* notice=findChild<QWidget*>("ShellStatusNotice");
            auto* body=notice?notice->findChild<QTextEdit*>("ShellStatusNoticeText"):nullptr;
            auto* dismiss=notice?notice->findChild<QToolButton*>("ShellStatusNoticeClose"):nullptr;
            if(!notice||!body||!dismiss){workflowCase("canvas-notice-controls-present",false);return;}
            const auto endVisible=[&]{
                auto end=body->textCursor();end.movePosition(QTextCursor::End);
                return body->viewport()->rect().contains(body->cursorRect(end));
            };
            const auto fits=[&]{
                return notice->isVisible()&&notice->parentWidget()->contentsRect().contains(notice->geometry())
                    &&body->verticalScrollBar()->maximum()==0&&endVisible();
            };
            workflowCase("canvas-close-warning-text",body->toPlainText()==command["text"].toString());
            workflowCase("canvas-close-warning-fits-immediately",fits());
            if(notice->grab().save(workflowOutput_+"/close-busy-warning.png"))workflowArtifacts_["close_busy_warning"]=workflowOutput_+"/close-busy-warning.png";
            const auto wide=notice->size();
            split->setSizes({430,670});
            workflowCase("canvas-close-warning-refits-on-split",notice->width()<wide.width()&&fits());
            if(notice->grab().save(workflowOutput_+"/close-busy-warning-narrow.png"))workflowArtifacts_["close_busy_warning_narrow"]=workflowOutput_+"/close-busy-warning-narrow.png";
            const bool canPlace=addFlow_&&addFlow_->isEnabled()&&!addFlow_->isChecked();
            if(canPlace)addFlow_->trigger();
            workflowCase("canvas-menu-action-shows-complete-hint",canPlace&&fits()&&
                body->toPlainText()==uiText("在横剖、纵剖或俯视图连续点击流线起点；Esc 或右键结束。"));
            endFlowline();
            auto* scroll=body->verticalScrollBar();
            const auto longPath=QString(460,QChar('x'))+".fbm";
            showStatusMessage(longPath,StudioMessageRole::Warning);
            workflowCase("canvas-unbroken-text-wraps",body->toPlainText()==longPath&&body->horizontalScrollBar()->maximum()==0&&fits(),
                QString("%1x%2, horizontal range %3, vertical range %4, end visible %5")
                    .arg(body->viewport()->width()).arg(body->viewport()->height()).arg(body->horizontalScrollBar()->maximum())
                    .arg(scroll->maximum()).arg(endVisible()));
            auto paragraphs=QString("Directory: %1\n").arg(longPath).repeated(12)+"End of notice.";
            showStatusMessage(paragraphs,StudioMessageRole::Feedback);
            workflowCase("canvas-oversize-scrolls-without-overflow",notice->parentWidget()->contentsRect().contains(notice->geometry())&&scroll->maximum()>0&&dismiss->isVisible());
            scroll->setValue(scroll->maximum());
            workflowCase("canvas-last-line-reachable",endVisible());
            dismiss->click();
            workflowCase("canvas-notice-dismisses",notice->isHidden());
            showStatusMessage("Saved.",StudioMessageRole::Feedback);
            workflowCase("canvas-short-feedback-does-not-block-canvas",fits()&&dismiss->isHidden()&&notice->testAttribute(Qt::WA_TransparentForMouseEvents));
        }
        else if(kind=="restore_notice_check"){
            auto* notice=findChild<QWidget*>("StartupRestoreNotice");
            if(command["action"]=="verify_open"){
                workflowCase("successful-model-open-dismisses-startup-notice",session_.ready()&&notice&&notice->isHidden());
            }else{
                auto* body=notice?notice->findChild<QTextEdit*>("StartupRestoreNoticeText"):nullptr;
                auto* close=notice?notice->findChild<QToolButton*>("StartupRestoreNoticeClose"):nullptr;
                if(!notice||!body||!close){workflowCase("startup-notice-controls-present",false);finishWorkflow();return;}
                QApplication::processEvents();
                const auto text=body->toPlainText();
                workflowCase("startup-notice-is-visible-and-nonmodal",notice->isVisible()&&!QApplication::activeModalWidget());
                workflowCase("startup-notice-text-and-path-complete",text==command["text"].toString());
                auto last=body->textCursor();last.movePosition(QTextCursor::End);
                workflowCase("startup-notice-wrapped-text-fits",body->viewport()->rect().contains(body->cursorRect(last))&&
                    !body->verticalScrollBar()->maximum()&&notice->parentWidget()->rect().contains(notice->geometry()));
                const auto image=workflowOutput_+"/restore-notice.png";
                if(notice->grab().save(image))workflowArtifacts_["restore_notice"]=image;
                if(command["action"]=="close"){
                    const QPoint point=close->rect().center();
                    const auto global=close->mapToGlobal(point);
                    workflowCase("startup-notice-close-receives-pointer",notice->childAt(close->mapTo(notice,point))==close);
                    QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                    QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(close,&press);QApplication::sendEvent(close,&release);
                    workflowCase("startup-notice-dismisses-on-click",notice->isHidden());
                }else if(command["action"]=="wait"){
                    QEventLoop waiting;
                    QTimer::singleShot(13000,&waiting,&QEventLoop::quit);
                    waiting.exec();
                    workflowCase("startup-notice-dismisses-without-input",notice->isHidden());
                }
            }
        }
        else if(kind=="open")openModel(path);
        else if(kind=="lines_window_check"){
            qApp->setQuitOnLastWindowClosed(false);
            linesDialog_->setAttribute(Qt::WA_DontShowOnScreen);
            const auto originalLanguage=uiLanguage();
            if(command["localized_references"].toBool()&&!setUiLanguage("en").isEmpty()){
                finishWorkflow("Lines-plan English catalog unavailable");return;
            }
            const auto model=session_.model();const auto scene=session_.scene();
            const auto source=session_.sourcePath();const auto tab=tabs_->currentIndex();
            const auto hullCamera=views_.front()->project({1,2,3});
            workflowCase("lines-action-enabled",linesAction_&&linesAction_->isEnabled());
            linesAction_->trigger();QApplication::processEvents();
            workflowCase("lines-independent-window",linesDialog_->isWindow()&&linesDialog_->isVisible()&&!linesDialog_->isModal()&&lines_->window()==linesDialog_);
            workflowCase("lines-close-button",bool(linesDialog_->windowFlags()&Qt::WindowCloseButtonHint));
            workflowCase("four-views-remain-central",tabs_->count()==1&&tabs_->currentIndex()==tab&&views_.front()->isVisibleTo(tabs_));
            auto* original=linesDialog_;linesAction_->trigger();
            workflowCase("lines-open-reuses-window",linesDialog_==original&&findChildren<QDialog*>("LinesPlanWindow").size()==1);
            lines_->panBy(QPointF(12,-7));lines_->zoomAt(lines_->rect().center(),1.1);
            const auto camera=lines_->camera();const auto image=lines_->image();
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
            auto* toolbar = linesDialog_->findChild<QToolBar*>();
            QList<QAction*> tools;
            QJsonArray captions;
            if (toolbar) {
                for (auto* action : toolbar->actions()) {
                    if (action->isSeparator() || !action->isVisible()) continue;
                    tools.append(action);
                    captions.append(action->text());
                }
                const auto screenshot = workflowOutput_ + "/lines-toolbar.png";
                if (toolbar->grab().save(screenshot)) workflowArtifacts_["lines_toolbar"] = screenshot;
                QList<QToolButton*> fillButtons;
                for(auto* action:tools)
                    if(action->isCheckable())
                        if(auto* button=qobject_cast<QToolButton*>(toolbar->widgetForAction(action)))fillButtons.append(button);
                if(fillButtons.size()==1){
                    auto* button=fillButtons.front();
                    const bool checked=button->isChecked();
                    const auto on=button->grab().toImage();
                    button->click();
                    const auto off=button->grab().toImage();
                    const bool switched=!button->isChecked()&&lines_->image()!=image;
                    button->click();
                    const auto path=workflowOutput_+"/lines-fill-unchecked.png";
                    if(off.save(path))workflowArtifacts_["lines_fill_unchecked"]=path;
                    int selectedPixels=0,uncheckedPixels=0;
                    for(int y=0;y<on.height();++y)for(int x=0;x<on.width();++x){
                        selectedPixels+=on.pixelColor(x,y)==QColor("#e5f2ec");
                        uncheckedPixels+=off.pixelColor(x,y)==QColor("#e5f2ec");
                    }
                    workflowCase("lines-fill-checked-mint",selectedPixels>40&&uncheckedPixels==0);
                    workflowCase("lines-fill-click-switches-render",checked&&switched&&button->isChecked()&&lines_->image()==image);
                }else workflowCases_.append(QJsonObject{{"name","lines-fill-style"},{"status","not_run"},{"message","Fill toolbar control was missing or ambiguous"}});
            }
            workflowCase("lines-toolbar-observed", toolbar != nullptr);
            workflowArtifacts_["lines_toolbar_actions"] = captions;
            const auto exportAction = [&tools](const QString& format) -> QAction* {
                for (auto* action : tools) if (action->text() == format) return action;
                return nullptr;
            };
            auto* pngAction = exportAction("PNG");
            auto* dxfAction = exportAction("DXF");
            if (!pngAction || !dxfAction || !pngAction->isEnabled() || !dxfAction->isEnabled()) {
                finishWorkflow("Missing enabled lines-plan export actions");
                return;
            }
            const auto png=workflowOutput_+"/lines-export-"+QString::number(startupCommand_)+".png";
            QTimer::singleShot(0,this,[this,png]{
                auto* picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                if(!picker){finishWorkflow("Missing lines image save dialog");return;}
                picker->selectFile(png);QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
            });
            pngAction->trigger();
            workflowCase("lines-save-exports-lines",QImage(png).convertToFormat(QImage::Format_RGBA8888)==image.convertToFormat(QImage::Format_RGBA8888));
            const auto dxf = workflowOutput_ + "/lines-export-" + QString::number(startupCommand_) + ".dxf";
            QTimer::singleShot(0, this, [this, dxf] {
                auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                if (!picker) { finishWorkflow("Missing lines DXF save dialog"); return; }
                picker->selectFile(dxf);
                QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
            });
            dxfAction->trigger();
            QFile exported(dxf), originalDxf(session_.dxfPath());
            workflowCase("lines-save-exports-dxf", exported.open(QIODevice::ReadOnly) &&
                originalDxf.open(QIODevice::ReadOnly) && exported.readAll() == originalDxf.readAll());
            workflowCase("lines-title-close",linesDialog_->close()&&!linesDialog_->isVisible());
            linesAction_->trigger();QApplication::processEvents();
            workflowCase("lines-reopen-preserves-camera",lines_->camera()==camera&&lines_->image()==image);
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(linesDialog_,&escape);
            workflowCase("lines-escape-closes",!linesDialog_->isVisible());
            workflowCase("lines-keeps-model",session_.model()==model&&session_.scene()==scene&&session_.sourcePath()==source);
            workflowCase("lines-keeps-four-view-camera",views_.front()->project({1,2,3})==hullCamera);
            // The same production view survives close/reopen and receives the
            // current immutable scene after model edits.
            LinesView expected;expected.setReferenceCaptions(uiText("基线 %1"),uiText("中心线"));expected.resize(lines_->size());expected.setScene(scene);expected.fit();lines_->fit();
            workflowCase("lines-current-scene",expected.image()==lines_->image());
            if(command["localized_references"].toBool()){
                linesAction_->trigger();QApplication::processEvents();
                const auto stableCamera=lines_->camera();
                const auto drawing=lines_->image(false,true,false);
                QJsonObject captions;
                for(const auto& locale:QStringList{"en","zh","ru","ja","de","fr","es"}){
                    if(!setUiLanguage(locale).isEmpty()){finishWorkflow("Lines-plan catalog unavailable: "+locale);return;}
                    workflowCase("lines-geometry-and-camera-stable-"+locale,session_.scene()==scene&&
                        lines_->camera()==stableCamera&&lines_->image(false,true,false)==drawing);
                    captions[locale]=QJsonArray::fromStringList(lines_->screenLabelTexts());
                }
                workflowArtifacts_["lines_reference_labels"]=captions;
                workflowArtifacts_["english_lines_export"]=png;
                linesDialog_->close();
                setUiLanguage(originalLanguage);
            }
        }
        else if(kind=="save")session_.save(path);
        else if(kind=="new_hull")session_.newHull(command["fields"].toObject());
        else if(kind=="update_project")session_.updateProject(command["changes"].toObject());
        else if(kind=="project_action"){
            QList<QAction*> actions;for(auto* action:findChildren<QAction*>())if(action->property("document_operation")=="project_settings")actions.append(action);
            if(actions.size()!=1||!actions[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","project_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }
            QTimer::singleShot(0,this,[this,command]{
                auto* dialog=dynamic_cast<ProjectDialog*>(QApplication::activeModalWidget());
                if(!dialog){finishWorkflow("Project dialog did not open");return;}
                if(command["observe_fields"].toBool()){
                    QJsonArray fields;
                    for(const auto* field:dialog->findChildren<QWidget*>()){
                        const auto key=field->property("modelField").toString();
                        if(!key.isEmpty())fields.append(key);
                    }
                    workflowArtifacts_["project_fields"]=fields;
                }
                const auto changes=command["changes"].toObject();
                for(auto it=changes.begin();it!=changes.end();++it){
                    QList<QWidget*> found;for(auto* field:dialog->findChildren<QWidget*>())if(field->property("modelField")==it.key())found.append(field);
                    if(found.size()!=1){workflowCases_.append(QJsonObject{{"name","project_field_unavailable"},{"status","not_run"}});dialog->reject();return;}
                    if(auto* box=qobject_cast<QCheckBox*>(found[0]);box&&it.value().isBool())box->setChecked(it.value().toBool());
                    else if(auto* box=qobject_cast<QLineEdit*>(found[0]);box&&it.value().isString())box->setText(it.value().toString());
                    else if(auto* box=qobject_cast<QComboBox*>(found[0]);box&&it.value().isDouble()&&it.value().toDouble()==it.value().toInt()&&it.value().toInt()>=0&&it.value().toInt()<box->count())box->setCurrentIndex(it.value().toInt());
                    else if(auto* box=qobject_cast<QDoubleSpinBox*>(found[0]);box&&!box->isReadOnly()&&it.value().isDouble()&&it.value().toDouble()>=box->minimum()&&it.value().toDouble()<=box->maximum())box->setValue(it.value().toDouble());
                    else if(auto* box=qobject_cast<QSpinBox*>(found[0]);box&&it.value().isDouble()&&it.value().toDouble()==it.value().toInt()&&it.value().toInt()>=box->minimum()&&it.value().toInt()<=box->maximum())box->setValue(it.value().toInt());
                    else {dialog->reject();finishWorkflow("Project field is outside the safe probe adapter");return;}
                }
                if(command["observe_fields"].toBool()){
                    QJsonObject numbers;
                    for(const auto* field:dialog->findChildren<QDoubleSpinBox*>()){
                        const auto key=field->property("modelField").toString();
                        if(!key.isEmpty())numbers[key]=QJsonObject{{"value",field->value()},{"read_only",field->isReadOnly()},{"enabled",field->isEnabled()},{"suffix",field->suffix()}};
                    }
                    workflowArtifacts_["project_numbers"]=numbers;
                    workflowArtifacts_["project_validation"]=dialog->validationError();
                    auto states=workflowArtifacts_["project_field_states"].toArray();states.append(numbers);
                    workflowArtifacts_["project_field_states"]=states;
                    if(command["capture_dimensions"].toBool()){
                        auto* target=dialog->findChild<QWidget*>("project_aft_perpendicular");
                        for(auto* tabs:dialog->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)
                            if(target&&tabs->widget(i)->isAncestorOf(target))tabs->setCurrentIndex(i);
                        const auto path=workflowOutput_+QString("/project-dimensions-%1.png").arg(states.size());
                        // Capture the actual page, not the outgoing page's
                        // temporary tab-transition animation overlay.
                        workflowCase("project-dimensions-image",target&&target->parentWidget()->grab().save(path));
                    }
                }
                if(command["expect_invalid"].toBool()){
                    workflowCase("project-invalid-rejected",!dialog->validationError().isEmpty());dialog->reject();
                }else if(command["cancel"].toBool())dialog->reject();else dialog->accept();
            });
            actions[0]->trigger();
        }
        else if(kind=="save_action"){
            if(command.contains("save_dialog")){
              auto* timer=new QTimer(this);timer->setInterval(10);timer->setProperty("attempts",0);
              connect(timer,&QTimer::timeout,this,[this,command,timer]{
                if(workflowFinished_){timer->stop();timer->deleteLater();return;}
                auto* picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                if(!picker){const int n=timer->property("attempts").toInt()+1;timer->setProperty("attempts",n);if(n<200)return;
                    timer->stop();timer->deleteLater();finishWorkflow("Save file picker did not open: "+QString::fromUtf8(QJsonDocument(startupObservations_).toJson(QJsonDocument::Compact)));return;}
                const auto options=command["save_dialog"].toObject();
                workflowArtifacts_["save_dialog_options"]=options;
                if(!timer->property("visited").toBool()){
                    timer->setProperty("visited",true);
                    workflowCase("save-picker-only-fbm",picker->nameFilters().size()==1&&picker->nameFilters().first().contains("*.fbm")&&!picker->nameFilters().first().contains("*.ftm"));
                    workflowCase("save-picker-default-fbm",!picker->selectedFiles().isEmpty()&&QFileInfo(picker->selectedFiles().first()).suffix()=="fbm");
                    if(!options["cancel"].toBool()){
                        if(auto* focused=picker->focusWidget())focused->clearFocus();
                        const QFileInfo target(options["path"].toString());picker->setDirectory(target.absolutePath());picker->selectFile(target.fileName());return;
                    }
                }
                if(auto* buttons=picker->findChild<QDialogButtonBox*>()){
                    const auto role=options["cancel"].toBool()?QDialogButtonBox::Cancel:QDialogButtonBox::Save;
                    if(auto* button=buttons->button(role);button&&button->isEnabled()){
                        timer->stop();timer->deleteLater();button->click();
                    }
                }
              });timer->start();
            }
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="save")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","save_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="move"){
            if(!session_.model()){finishWorkflow("No model to edit");return;}
            const fs::ControlPoint* chosen=nullptr;
            for(const auto& point:session_.model()->points)if(!point.locked&&point.batchMovable){chosen=&point;break;}
            if(!chosen){finishWorkflow("No movable point");return;}
            auto position=chosen->position;position.x+=command["dx"].toDouble(.125);session_.movePoint(chosen->id,position);
        }
        else if(kind=="undo")session_.undo();
        else if(kind=="redo")session_.redo();
        else if(kind=="discard_recovery")session_.discardRecovery();
        else if(kind=="backend_exit"){
            if(!session_.ready()||session_.busy()){finishWorkflow("Backend exit probe requires its own idle live document");return;}
            BackendExitProbe::terminate(session_);return; // Advance on the actual process error.
        }
        else if(kind=="resume_current"){
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="resume_current")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","resume_current_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="close"){
            startupClosing_=true;
            if(command["native_system_close"].toBool()){
                // Only this explicitly launched private process is controlled.
                // Transparent, non-activating HWNDs keep the user's desktop intact.
                setProperty("studio_probe_transparent_dialogs",true);setWindowOpacity(0);
                setAttribute(Qt::WA_DontShowOnScreen,false);setAttribute(Qt::WA_ShowWithoutActivating);showNormal();
#ifdef Q_OS_WIN
                const auto hwnd=reinterpret_cast<HWND>(winId());DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
                if(pid!=GetCurrentProcessId()){finishWorkflow("Refusing another process's close HWND");return;}
                ShowWindow(hwnd,SW_SHOWNOACTIVATE);
                if(command["owner_state"]=="minimized")ShowWindow(hwnd,SW_SHOWMINNOACTIVE);
                if(command["owner_state"]=="hidden")hide();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#endif
            }
            if(command["visible_window"].toBool()){
                // Logical visibility exercises QApplication's real close-all route.
                // Keep both owned test windows off the user's desktop.
                setAttribute(Qt::WA_DontShowOnScreen);show();
                if(command["with_lines"].toBool()){
                    linesDialog_->setAttribute(Qt::WA_DontShowOnScreen);openLinesWindow();
                }
            }
            auto* responder=new QTimer(this);responder->setInterval(5);
            connect(responder,&QTimer::timeout,this,[this,command,responder]{
                auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if(!dialog)return;
                QJsonObject evidence{{"edit_in_flight",session_.busy()},{"owner_minimized",isMinimized()},{"owner_visible",isVisible()},
                    {"dialog_frame_x",dialog->frameGeometry().x()},{"dialog_frame_y",dialog->frameGeometry().y()},
                    {"dialog_frame_w",dialog->frameGeometry().width()},{"dialog_frame_h",dialog->frameGeometry().height()},
                    {"icon_key",dialog->property("studio_window_icon").toString()},
                    {"icon_name",dialog->windowIcon().name()}};
                if(auto* monitor=screen()){
                    const auto area=monitor->availableGeometry();evidence["screen_x"]=area.x();evidence["screen_y"]=area.y();evidence["screen_w"]=area.width();evidence["screen_h"]=area.height();
                }
#ifdef Q_OS_WIN
                if(command["native_system_close"].toBool()){
                    const auto owner=reinterpret_cast<HWND>(winId()),child=reinterpret_cast<HWND>(dialog->winId());
                    DWORD ownerPid=0,childPid=0;GetWindowThreadProcessId(owner,&ownerPid);GetWindowThreadProcessId(child,&childPid);
                    evidence["native_owned"]=ownerPid==GetCurrentProcessId()&&childPid==ownerPid;evidence["native_owner_iconic"]=bool(IsIconic(owner));
                    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);RECT frame{};
                    if(GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor)&&GetWindowRect(child,&frame)){
                        evidence["native_center_dx"]=double(frame.left+frame.right-monitor.rcWork.left-monitor.rcWork.right)/2;
                        evidence["native_center_dy"]=double(frame.top+frame.bottom-monitor.rcWork.top-monitor.rcWork.bottom)/2;
                    }
                }
#endif
                int blue=0;const auto icon=dialog->iconPixmap().toImage();
                for(int y=0;y<icon.height();++y)for(int x=0;x<icon.width();++x){const auto color=icon.pixelColor(x,y);if(color.alpha()>30&&color.blue()>color.green()+15&&color.blue()>color.red()+30)++blue;}
                evidence["blue_icon_pixels"]=blue;evidence["body_icon_present"]=!icon.isNull();
                QDir().mkpath(workflowOutput_+"/views");
                const auto image=workflowOutput_+"/views/unsaved-close.png";
                workflowCase("close-prompt-picture",dialog->grab().save(image));evidence["image"]=image;
                workflowArtifacts_["close_prompt"]=evidence;
                const int count=property("close_probe_prompt_count").toInt()+1;
                setProperty("close_probe_prompt_count",count);
                if(auto* discard=dialog->button(QMessageBox::Discard))setProperty("close_probe_discard_caption",discard->text());
                if(count>1){
                    responder->stop();dialog->button(QMessageBox::Cancel)->click();
                    finishWorkflow("Close confirmation repeated after the user chose to continue");return;
                }
                const auto decision=command["choice"].toString(command["discard"].toBool()?"discard":"cancel");
                const auto choice=decision=="discard"?QMessageBox::Discard:decision=="save"?QMessageBox::Save:QMessageBox::Cancel;
                setProperty("probe_close_decision_ms",workflowTimer_.elapsed());
                setProperty("probe_close_revision",session_.model()?double(session_.model()->revision):0.);
                if(command["queued_layers"].toInt()>0&&choice!=QMessageBox::Cancel)new CloseDialogObserver(this);
                if(auto* button=dialog->button(choice))button->click();
                if(choice==QMessageBox::Cancel){
                    responder->stop();
                    // DefWindowProc(SC_CLOSE) can post WM_CLOSE. Do not finish
                    // the startup workflow until that close prompt was observed.
                    if(command["native_system_close"].toBool()){
                        startupClosing_=false;QTimer::singleShot(0,this,[this]{startupStep("idle");});
                    }
                }
            });
            responder->start();
            if(command["busy_edit"].toBool()){
                // Exercise the earliest possible close: the real session has accepted
                // the edit but cannot finish it until this state callback returns.
                setProperty("probe_edit_during_close",true);
                auto previous=session_.onStateChanged;
                bool requested=false;
                session_.onStateChanged=[&,previous]{
                    previous();
                    if(!requested&&session_.busy()){
                        requested=true;
                        for(int i=0;i<command["queued_layers"].toInt();++i){
                            if(!session_.layerListCommand("update_layer",{{"layer_id",double(session_.model()->layers.front().id)},
                                    {"changes",QJsonObject{{"name",QString("close-probe-%1").arg(i)}}}})){
                                finishWorkflow("Cannot queue layer edit");return;
                            }
                        }
                        close();
                        setProperty("probe_close_return_ms",workflowTimer_.elapsed()-property("probe_close_decision_ms").toLongLong());
                        setProperty("probe_main_hidden",!isVisible());setProperty("probe_main_enabled",isEnabled());
                    }
                };
                const bool accepted=command["queued_layers"].toInt()?session_.layerListCommand("update_layer",
                    {{"layer_id",double(session_.model()->layers.front().id)},{"changes",QJsonObject{{"name","close-probe-first"}}}}):session_.updateProject({{"water_density",1.027}});
                session_.onStateChanged=previous;
                if(!accepted||!requested){finishWorkflow("Close probe did not start an edit");return;}
                if(command["choice"]=="cancel")startupClosing_=false;
                return;
            }
            if(command["native_system_close"].toBool()){
#ifdef Q_OS_WIN
                SendMessageW(reinterpret_cast<HWND>(winId()),WM_SYSCOMMAND,SC_CLOSE,0);
#else
                close();
#endif
            }else close();
            const auto decision=command["choice"].toString(command["discard"].toBool()?"discard":"cancel");
            if(command["native_system_close"].toBool())return;
            if(decision!="cancel")return; // Observe aboutToQuit, never a synthetic idle completion.
            startupClosing_=false;
        }
        else if(kind=="recover"){
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="recover")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recover_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="recent"){
            QList<QAction*> found;for(auto* action:recentMenu_->actions())if(action->property("file_operation")=="recent"&&action->data().toString()==path)found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recent_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }else{finishWorkflow("Unknown startup observation command");return;}
        // A first launch, or an absent file, intentionally starts no backend.
        if(startupObservations_.size()==observed&&!session_.busy()&&!workflowFinished_)startupStep("idle");
    });
}
