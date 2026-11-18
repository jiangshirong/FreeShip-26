// SPDX-License-Identifier: GPL-2.0-or-later
#include "model_file_save.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace fs {
namespace {
constexpr qint64 MaximumBytes=512LL*1024*1024;
[[noreturn]] void bad(const QString& s){throw std::runtime_error(s.toUtf8().constData());}
void regular(const QString& path){
    const QFileInfo file(path);
    if(file.isSymLink()||(file.exists()&&!file.isFile()))bad("保存位置不是普通文件，请另存为其他文件名："+path);
}
bool equalPath(const QString& a,const QString& b){
#ifdef Q_OS_WIN
    return a.compare(b,Qt::CaseInsensitive)==0;
#else
    return a==b;
#endif
}
ModelFileVersion copyTo(const QString& input,QIODevice& output){
    QFile file(input);regular(input);
    if(!file.open(QIODevice::ReadOnly)||file.size()<0||file.size()>MaximumBytes)bad("无法读取保存所需的文件："+input);
    QCryptographicHash hash(QCryptographicHash::Sha256);qint64 count=0;
    while(!file.atEnd()){
        const auto data=file.read(1024*1024);
        if(file.error()!=QFile::NoError||data.isEmpty()||count>MaximumBytes-data.size()||output.write(data)!=data.size())bad("文件写入未完成，正式船模尚未替换。");
        hash.addData(data);count+=data.size();
    }
    return {modelFilePath(input),true,count,hash.result()};
}
void requireVersion(const ModelFileVersion& actual,const ModelFileVersion& expected){
    if(!sameModelVersion(actual,expected))bad("文件已被其他窗口或程序修改、替换或删除。本次没有覆盖它，当前修改仍保留在窗口中。请另存为不同文件名，或重新打开该文件。\n"+expected.path);
}
std::unique_ptr<QTemporaryFile> snapshot(const QString& path,const QString& parent){
    auto temp=std::make_unique<QTemporaryFile>(parent+"/.freeship-backup-XXXXXX");
    if(!temp->open())bad("无法准备备份保护文件。");
    copyTo(path,*temp);if(!temp->flush())bad("无法完整写入备份保护文件。");temp->close();return temp;
}
}
QString modelFilePath(const QString& path){
    if(path.isEmpty()||path.contains(QChar(0)))bad("保存文件名无效。");
    const QFileInfo file(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    if(file.fileName().contains(':'))bad("请使用普通船模文件名。");
#endif
    const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
    return QDir::cleanPath((parent.isEmpty()?file.absolutePath():parent)+'/'+file.fileName());
}
QString modelBackupPath(const QString& target){
    const QFileInfo file(modelFilePath(target));
    // Opening an old .Bak must not make its backup alias its own destination.
    return file.suffix().compare("bak",Qt::CaseInsensitive)==0?file.absoluteFilePath()+".Bak":file.absolutePath()+'/'+file.completeBaseName()+".Bak";
}
bool sameModelFile(const QString& a,const QString& b){return !a.isEmpty()&&!b.isEmpty()&&equalPath(modelFilePath(a),modelFilePath(b));}
ModelFileVersion observeModelFile(const QString& path){
    ModelFileVersion result;result.path=modelFilePath(path);regular(result.path);
    QFile file(result.path);result.exists=QFileInfo::exists(result.path);if(!result.exists)return result;
    if(!file.open(QIODevice::ReadOnly)||file.size()<0||file.size()>MaximumBytes)bad("无法核对原文件，请另存为其他文件名："+result.path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd()){
        const auto bytes=file.read(1024*1024);
        if(file.error()!=QFile::NoError||bytes.isEmpty()||result.bytes>MaximumBytes-bytes.size())bad("原文件读取未完成，未继续保存。");
        hash.addData(bytes);result.bytes+=bytes.size();
    }
    result.sha256=hash.result();return result;
}
bool sameModelVersion(const ModelFileVersion& a,const ModelFileVersion& b){
    return equalPath(a.path,b.path)&&a.exists==b.exists&&(!a.exists||(a.bytes==b.bytes&&a.sha256==b.sha256));
}
struct ModelFileSave::Impl {
    ModelFileVersion expected,backupBefore,prepared;
    QString target,backup;
    bool noChange=false,committed=false;
    std::vector<std::unique_ptr<QLockFile>> locks;
    std::unique_ptr<QTemporaryDir> newDirectory;
    std::unique_ptr<QTemporaryFile> previousBackup;
    QString newFile;
    // Close the pending file before its private directory is removed (Windows
    // forbids removing a directory that still contains the open temporary file).
    std::unique_ptr<QSaveFile> destination,backupOutput;
    Impl(const QString& input,const QString& path,const ModelFileVersion& version,const ModelFileVersion& preparedInput):expected(version),target(modelFilePath(path)),backup(modelBackupPath(target)){
        if(!equalPath(target,expected.path))bad("保存目标与核对的文件不一致。");
        QStringList resources{target,backup};std::sort(resources.begin(),resources.end());
        for(const auto& resource:resources){
            const auto lockPath=resource+".freeship-save.lock";regular(lockPath);
            auto lock=std::make_unique<QLockFile>(lockPath);lock->setStaleLockTime(0);
            if(!lock->tryLock(0))bad("另一个窗口正在保存这个文件，或目录不可写。请稍后重试，或另存为。\n"+resource);
            locks.push_back(std::move(lock));
        }
        requireVersion(observeModelFile(target),expected);
        if(expected.exists&&!QFileInfo(target).isWritable())bad("原文件为只读，未保存，请另存为其他文件名。");
        const auto candidate=observeModelFile(input);
        if(!candidate.exists||candidate.bytes==0)bad("计算组件未生成完整船模。");
        if(preparedInput.exists)requireVersion(candidate,preparedInput);
        if(expected.exists&&candidate.bytes==expected.bytes&&candidate.sha256==expected.sha256){prepared=expected;noChange=true;return;}
        QString stagedTarget=target;
        if(!expected.exists){
            newDirectory=std::make_unique<QTemporaryDir>(QFileInfo(target).absolutePath()+"/.freeship-save-XXXXXX");
            if(!newDirectory->isValid())bad("无法准备新船模文件。");
            newFile=newDirectory->filePath("model.fbm");stagedTarget=newFile;
        }
        destination=std::make_unique<QSaveFile>(stagedTarget);destination->setDirectWriteFallback(false);
        if(!destination->open(QIODevice::WriteOnly))bad("无法写入船模："+destination->errorString());
        const auto copied=copyTo(input,*destination);requireVersion(copied,candidate);
        prepared=copied;prepared.path=target;
        if(expected.exists){
            backupBefore=observeModelFile(backup);
            if(backupBefore.exists){
                previousBackup=snapshot(backup,QFileInfo(target).absolutePath());
                auto copiedBackup=observeModelFile(previousBackup->fileName());copiedBackup.path=backup;
                requireVersion(copiedBackup,backupBefore);
            }
            backupOutput=std::make_unique<QSaveFile>(backup);backupOutput->setDirectWriteFallback(false);
            if(!backupOutput->open(QIODevice::WriteOnly))bad("无法准备上一版备份，未覆盖原船模："+backupOutput->errorString());
            requireVersion(copyTo(target,*backupOutput),expected);
        }
    }
    void rollbackBackup(){
        auto installedBackup=expected;installedBackup.path=backup;
        requireVersion(observeModelFile(backup),installedBackup);
        if(previousBackup){
            QSaveFile restore(backup);restore.setDirectWriteFallback(false);
            if(!restore.open(QIODevice::WriteOnly))bad("无法恢复之前的备份。");
            copyTo(previousBackup->fileName(),restore);if(!restore.commit())bad("无法恢复之前的备份。");
        }else if(!QFile::remove(backup))bad("无法撤回本次未完成的备份。");
    }
    ModelSaveResult commit(){
        if(committed)bad("此保存事务已完成。");
        requireVersion(observeModelFile(target),expected);
        if(noChange){committed=true;return {prepared,{},false};}
        if(backupOutput){
            requireVersion(observeModelFile(backup),backupBefore);
            if(!backupOutput->commit())bad("备份写入失败，原船模未替换："+backupOutput->errorString());
        }
        try{
            // The lock serializes cooperating Qt windows. Recheck external
            // writers after backup staging; they do not obey this lock.
            requireVersion(observeModelFile(target),expected);
            if(!destination->commit())bad("船模保存失败，原文件未替换："+destination->errorString());
            if(newDirectory){
                // QFile's rename refuses an existing name, including a file
                // created after our final absence check.
                QFile file(newFile);
                if(!file.rename(target))bad("无法安装新船模；该位置可能已出现其他文件，请另存为。\n"+file.errorString());
            }
        }catch(const std::exception& cause){
            const auto message=QString::fromUtf8(cause.what());
            if(backupOutput)try{rollbackBackup();}catch(const std::exception& rollback){
                QString retained;
                if(previousBackup){previousBackup->setAutoRemove(false);retained="\n之前的备份另留在："+previousBackup->fileName();}
                bad(message+"\n"+QString::fromUtf8(rollback.what())+retained);
            }
            bad(message);
        }
        committed=true;return {prepared,backupOutput?backup:QString{},true};
    }
};
ModelFileSave::ModelFileSave(const QString& input,const QString& target,const ModelFileVersion& expected,const ModelFileVersion& preparedInput):impl_(std::make_unique<Impl>(input,target,expected,preparedInput)){}
ModelFileSave::~ModelFileSave()=default;
ModelSaveResult ModelFileSave::commit(){return impl_->commit();}
}
