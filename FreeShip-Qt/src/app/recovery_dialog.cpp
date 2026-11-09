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
    setWindowTitle("恢复未保存的船模");resize(680,380);
    auto* layout=new QVBoxLayout(this);
    auto* hint=new QLabel("发现未正常结束的编辑。恢复后请另存为船模文件。\n选择“稍后处理”会保留副本，之后可从文件菜单打开。",this);
    hint->setWordWrap(true);layout->addWidget(hint);
    list_=new QListWidget(this);layout->addWidget(list_,1);
    auto* error=new QLabel(this);error->setWordWrap(true);error->setStyleSheet("color:#a83030;");layout->addWidget(error);
    for(const auto& entry:fs::recoveryEntries(root)){
        QString name=entry.source.isEmpty()?"未命名船模":QFileInfo(entry.source).fileName();
        const auto time=QDateTime::fromString(entry.savedUtc,Qt::ISODateWithMs).toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
        auto* row=new QListWidgetItem(name+"\n"+(entry.error.isEmpty()?time+QString(" · %1 MiB").arg(entry.bytes/1048576.0,0,'f',2):entry.error),list_);
        row->setData(Qt::UserRole,entry.id);row->setData(Qt::UserRole+1,entry.error);
        row->setToolTip(entry.source.isEmpty()?"尚未保存过的船模":entry.source);
    }
    if(list_->count()==0)error->setText("没有可恢复的副本。正在其他窗口编辑的船模不会出现在这里。");
    auto* buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto* restore=new QPushButton(studioIcon("LoadFile"),"恢复所选船模",this);restore->setProperty("recovery_action","restore");
    auto* remove=new QPushButton("删除所选副本…",this);remove->setProperty("recovery_action","delete");
    auto* later=new QPushButton("稍后处理",this);later->setProperty("recovery_action","later");
    buttons->addWidget(restore);buttons->addWidget(remove);buttons->addStretch();buttons->addWidget(later);
    auto update=[this,restore,remove]{const auto* row=list_->currentItem();restore->setEnabled(row&&row->data(Qt::UserRole+1).toString().isEmpty());remove->setEnabled(row);};
    connect(list_,&QListWidget::currentRowChanged,this,[update](int){update();});list_->setCurrentRow(0);update();
    connect(restore,&QPushButton::clicked,this,&QDialog::accept);
    connect(later,&QPushButton::clicked,this,&QDialog::reject);
    connect(remove,&QPushButton::clicked,this,[this,root,error,update]{
        const auto id=selectedId();if(id.isEmpty())return;
        if(QMessageBox::question(this,"删除恢复副本","确定删除这份未保存的恢复副本吗？原船模文件不受影响。",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        try{fs::RecoveryLease lease(root,id);lease.remove();delete list_->takeItem(list_->currentRow());update();}
        catch(const std::exception& ex){error->setText(QString::fromUtf8(ex.what()));}
    });
}
QString RecoveryDialog::selectedId()const{const auto* row=list_->currentItem();return row?row->data(Qt::UserRole).toString():QString{};}
