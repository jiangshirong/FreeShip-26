#include "app/ui_language.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "recovery_dialog.h"
#include "core/recovery_store.h"
#include "studio_ui.h"
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

RecoveryDialog::RecoveryDialog(const QString& root,QWidget* parent):QDialog(parent){
    setWindowTitle(uiText("恢复未保存的船模"));setStudioWindowIcon(this,"Restore");resize(680,380);
    auto* layout=new QVBoxLayout(this);
    list_=new QListWidget(this);layout->addWidget(list_,1);
    auto* empty=new StudioMessage(StudioMessageRole::Explanation,uiText("暂无可恢复副本"),this);empty->setObjectName("RecoveryEmptyState");layout->addWidget(empty);
    auto* error=new StudioMessage(StudioMessageRole::Warning,{},this);layout->addWidget(error);
    for(const auto& entry:fs::recoveryEntries(root)){
        QString name=entry.source.isEmpty()?uiText("未命名船模"):QFileInfo(entry.source).fileName();
        const auto time=QDateTime::fromString(entry.savedUtc,Qt::ISODateWithMs).toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
        auto* row=new QListWidgetItem(name+"\n"+(entry.error.isEmpty()?time+QString(" · %1 MiB").arg(entry.bytes/1048576.0,0,'f',2):entry.error),list_);
        row->setData(Qt::UserRole,entry.id);row->setData(Qt::UserRole+1,entry.error);
        row->setToolTip(entry.source.isEmpty()?uiText("尚未保存过的船模"):entry.source);
    }
    auto* buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto* restore=new QPushButton(studioIcon("LoadFile"),uiText("恢复所选船模"),this);restore->setProperty("recovery_action","restore");
    auto* remove=new QPushButton(uiText("删除所选副本…"),this);remove->setProperty("recovery_action","delete");
    auto* later=new QPushButton(uiText("稍后处理"),this);later->setProperty("recovery_action","later");
    buttons->addWidget(restore);buttons->addWidget(remove);buttons->addStretch();buttons->addWidget(later);
    auto update=[this,restore,remove,empty]{empty->setVisible(list_->count()==0);const auto* row=list_->currentItem();restore->setEnabled(row&&row->data(Qt::UserRole+1).toString().isEmpty());remove->setEnabled(row);};
    connect(list_,&QListWidget::currentRowChanged,this,[update](int){update();});list_->setCurrentRow(0);update();
    connect(restore,&QPushButton::clicked,this,&QDialog::accept);
    connect(later,&QPushButton::clicked,this,&QDialog::reject);
    connect(remove,&QPushButton::clicked,this,[this,root,error,update]{
        const auto id=selectedId();if(id.isEmpty())return;
        if(!confirmStudioAction(this,uiText("删除恢复副本"),uiText("确定删除这份未保存的恢复副本吗？原船模文件不受影响。")))return;
        try{fs::RecoveryLease lease(root,id);lease.remove();delete list_->takeItem(list_->currentRow());update();}
        catch(const std::exception& ex){error->setText(QString::fromUtf8(ex.what()));}
    });
}
QString RecoveryDialog::selectedId()const{const auto* row=list_->currentItem();return row?row->data(Qt::UserRole).toString():QString{};}
