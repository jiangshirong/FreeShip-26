#include "app/ui_language.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "document_workspace.h"
#include "studio_ui.h"
#include "recovery_dialog.h"
#include <algorithm>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include "view.h"
#include <QUrl>
#include <QAction>
#include <QFileInfo>
#include <QMenu>
#include <QStatusBar>
namespace {
QStringList droppedModels(const QMimeData* mime){
    QStringList paths;
    for(const auto& url:mime->urls()){
        if(!url.isLocalFile())continue;
        const QFileInfo file(url.toLocalFile());const auto ext=file.suffix().toLower();
        if((ext=="fbm"||ext=="ftm") && file.isFile())paths.append(file.absoluteFilePath());
    }
    return paths;
}
}
QList<MainWindow*> MainWindow::documentWindows(){
    QList<MainWindow*> windows;
    for(auto* w:QApplication::topLevelWidgets()){
        if(auto* host=dynamic_cast<DocumentWorkspace*>(w))windows.append(host->documents());
        else if(auto* doc=dynamic_cast<MainWindow*>(w);doc&&!doc->documentClosed_)windows.append(doc);
    }
    return windows;
}
MainWindow* MainWindow::windowForFile(const QString& path) const {
    const QFileInfo target(path);
    for(auto* w:documentWindows())for(const auto& p:{w->session_.sourcePath(),w->session_.pendingFilePath()})
        if(!p.isEmpty() && QFileInfo(p)==target)return w;
    return nullptr;
}
void MainWindow::activateDocument(){
    if(suspendedExitUi_)return;
    if(workspace_){workspace_->activate(this);return;}
    setWindowState(windowState() & ~Qt::WindowMinimized);raise();activateWindow();
}
MainWindow* MainWindow::destinationWindow(){
    if(!session_.model()&&!session_.busy()&&!documentClosed_&&!suspendedExitUi_)return this;
    if(workspace_)return workspace_->addDocument();
    auto* w=new MainWindow(nullptr,persistentWindow_);w->setAttribute(Qt::WA_DeleteOnClose);
    if(testAttribute(Qt::WA_DontShowOnScreen))w->setAttribute(Qt::WA_DontShowOnScreen);
    if(isMaximized())w->showMaximized();else{w->resize(size());w->show();}
    return w;
}
void MainWindow::openDocument(const QString& path){
    if(path.isEmpty())return;
    if(auto* w=windowForFile(path)){w->activateDocument();return;}
    destinationWindow()->openModel(path);
}
void MainWindow::dragEnterEvent(QDragEnterEvent* e){
    if(e->possibleActions().testFlag(Qt::CopyAction)&&!droppedModels(e->mimeData()).isEmpty()){e->setDropAction(Qt::CopyAction);e->accept();}else e->ignore();
}
void MainWindow::dropEvent(QDropEvent* e){
    const auto paths=droppedModels(e->mimeData());
    if(!e->possibleActions().testFlag(Qt::CopyAction)||paths.isEmpty()){e->ignore();return;}
    e->setDropAction(Qt::CopyAction);e->accept();for(const auto& path:paths)openDocument(path);
}
void MainWindow::acceptDocumentClose(QCloseEvent* event){
    documentClosed_=true;linesDialog_->close();event->accept();
    if(workspace_){workspace_->accepted(this);return;}
    if(persistentWindow_ && documentWindows().isEmpty())qApp->quit();
}
void MainWindow::installWindowMenu(QMenu* file){
    auto* menu=file->addMenu(uiText("窗口"));menu->setObjectName("DocumentWindows");
    bindUiText(menu,"title","窗口");
    connect(menu,&QMenu::aboutToShow,this,[this,menu]{
        menu->clear();auto docs=documentWindows();
        std::sort(docs.begin(),docs.end(),[](auto* a,auto* b){return a->windowTitle()<b->windowTitle();});
        for(auto* w:docs){auto title=w->windowTitle();auto* action=menu->addAction(title.replace('&',"&&"));
            action->setCheckable(true);action->setChecked(w==this);action->setEnabled(!w->suspendedExitUi_);
            connect(action,&QAction::triggered,w,&MainWindow::activateDocument);}
    });
}
void MainWindow::refreshRecentFiles(){
    recentMenu_->clear();const auto state=fileHistory_?fileHistory_->read():RecentFiles{};
    for(const auto& path:state.paths){
        auto label=path;label.replace('&',"&&");
        auto* action=recentMenu_->addAction(label);action->setData(path);action->setProperty("file_operation","recent");
        connect(action,&QAction::triggered,this,[this,path]{openDocument(path);});
    }
    if(state.paths.isEmpty()){auto* empty=recentMenu_->addAction(uiText("暂无最近打开的文件"));empty->setEnabled(false);}
}
void MainWindow::rememberFile(const QString& path){
    if(!fileHistory_||path.isEmpty())return;
    if(!fileHistory_->remember(path))showStatusMessage(uiText("船模已打开或保存，但未能更新最近文件记录。"),StudioMessageRole::Warning);
    refreshRecentFiles();
}
void MainWindow::startSession(const QString& explicitFile,const std::function<void()>& reveal){
    bool revealed=false;
    const auto revealFor=[&](const QString& path){
        if(!reveal||revealed)return;
        startupFile_=path;
        updateDocumentTitle();
        revealed=true;
        reveal();
        startupFile_.clear();
    };
    if(!explicitFile.isEmpty()){
        revealFor(QFileInfo(explicitFile).absoluteFilePath());
        openDocument(explicitFile);
        return;
    }
    const auto path=fileHistory_?fileHistory_->read().last:QString{};
    const auto restore=QFileInfo(path).isFile();
    if(!fs::recoveryEntries(session_.recoveryRoot()).isEmpty()){
        revealFor(restore?path:QString{});
        if(showRecoveries()){updateDocumentTitle();return;}
    }
    revealFor(restore?path:QString{});
    if(path.isEmpty())return;
    if(!restore){
        fileHistory_->stopRestore(path);refreshRecentFiles();
        showStartupRestoreWarning(QString::fromUtf8(uiText("找不到上次打开的船模，请从“打开模型”重新选择："))+"\n"+path);return;
    }
    restoringFile_=path;session_.load(path);
}
bool MainWindow::showRecoveries(){
    if(session_.busy())return false;
    RecoveryDialog dialog(session_.recoveryRoot(),this);
    if(runStudioDialog(dialog)!=QDialog::Accepted||dialog.selectedId().isEmpty())return false;
    // Reclaiming the same interrupted editor is not replacing another live
    // document. A Save prompt here offers an operation the dead service cannot
    // perform. Let the session validate revision and capsule before publishing.
    if(session_.canResumeCurrent() && session_.model()->documentId==dialog.selectedId())
        return session_.recover(dialog.selectedId());
    withSavedChanges([this,id=dialog.selectedId()]{session_.recover(id);});return true;
}
