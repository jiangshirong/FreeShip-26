// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "recovery_dialog.h"
#include <QAction>
#include <QFileInfo>
#include <QMenu>
#include <QStatusBar>
void MainWindow::refreshRecentFiles(){
    recentMenu_->clear();const auto state=fileHistory_?fileHistory_->read():RecentFiles{};
    for(const auto& path:state.paths){
        auto label=path;label.replace('&',"&&");
        auto* action=recentMenu_->addAction(label);action->setData(path);action->setProperty("file_operation","recent");
        action->setEnabled(!session_.busy());connect(action,&QAction::triggered,this,[this,path]{openModel(path);});
    }
    if(state.paths.isEmpty()){auto* empty=recentMenu_->addAction("暂无最近打开的文件");empty->setEnabled(false);}
}
void MainWindow::rememberFile(const QString& path){
    if(!fileHistory_||path.isEmpty())return;
    if(!fileHistory_->remember(path))statusBar()->showMessage("船模已打开或保存，但未能更新最近文件记录。");
    refreshRecentFiles();
}
void MainWindow::startSession(const QString& explicitFile){
    if(!explicitFile.isEmpty()){openModel(explicitFile);return;}
    if(!fs::recoveryEntries(session_.recoveryRoot()).isEmpty()&&showRecoveries())return;
    const auto path=fileHistory_?fileHistory_->read().last:QString{};if(path.isEmpty())return;
    if(!QFileInfo(path).isFile()){
        fileHistory_->stopRestore(path);refreshRecentFiles();
        statusBar()->showMessage("找不到上次打开的船模，请从“打开模型”重新选择："+path);return;
    }
    restoringFile_=path;session_.load(path);
}
bool MainWindow::showRecoveries(){
    if(session_.busy())return false;
    RecoveryDialog dialog(session_.recoveryRoot(),this);
    if(dialog.exec()!=QDialog::Accepted||dialog.selectedId().isEmpty())return false;
    withSavedChanges([this,id=dialog.selectedId()]{session_.recover(id);});return true;
}
