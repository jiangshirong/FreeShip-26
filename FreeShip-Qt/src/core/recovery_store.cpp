// SPDX-License-Identifier: GPL-2.0-or-later
#include "recovery_store.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs {
namespace {
const QByteArray Magic("FreeShipRecovery\n");
constexpr qint64 MaxBytes=512LL*1024*1024;
[[noreturn]] void bad(const QString& text){throw std::runtime_error(text.toUtf8().constData());}
bool validId(const QString& id){return !QUuid(id).isNull()&&QUuid(id).toString(QUuid::WithoutBraces)==id;}
void regular(const QString& path){
    const QFileInfo f(path);
    if(f.isSymLink()||(f.exists()&&!f.isFile()))bad("恢复副本路径不是普通文件。");
}
QJsonObject header(QFile& f,const QString& id){
    if(f.size()<Magic.size()+4||f.size()>MaxBytes+65536+Magic.size()+4||!f.open(QIODevice::ReadOnly))bad("无法读取恢复副本。");
    if(f.read(Magic.size())!=Magic)bad("恢复副本格式不正确。");
    const auto size=f.read(4);if(size.size()!=4)bad("恢复副本头部不完整。");
    const auto n=qFromLittleEndian<quint32>(size.constData());if(n<2||n>65536)bad("恢复副本头部长度不正确。");
    const auto raw=f.read(n);QJsonParseError parse;
    const auto h=QJsonDocument::fromJson(raw,&parse).object();
    const auto rev=h["revision"].toDouble(-1),bytes=h["bytes"].toDouble(-1);
    const auto hash=h["sha256"].toString().toLatin1();
    if(raw.size()!=n||parse.error!=QJsonParseError::NoError||h["format"]!="FreeShipRecovery"||h["version"]!=1||h["document_id"]!=id||
       !std::isfinite(rev)||rev<1||rev>9007199254740991.0||std::floor(rev)!=rev||bytes<1||bytes>MaxBytes||std::floor(bytes)!=bytes||
       f.size()-f.pos()!=qint64(bytes)||hash.size()!=64||QByteArray::fromHex(hash).toHex()!=hash||
       !h["source"].isString()||h["source"].toString().size()>32768||h["source"].toString().contains(QChar(0))||
       !QDateTime::fromString(h["saved_utc"].toString(),Qt::ISODateWithMs).isValid())bad("恢复副本信息不完整或版本不支持。");
    return h;
}
RecoveryEntry entry(const QJsonObject& h){return {h["document_id"].toString(),h["source"].toString(),h["saved_utc"].toString(),{},quint64(h["revision"].toDouble()),qint64(h["bytes"].toDouble())};}
}
QString defaultRecoveryRoot(){const auto root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);return root.isEmpty()?QString{}:root+"/recovery";}
RecoveryLease::RecoveryLease(const QString& root,const QString& id):id_(id){
    if(root.isEmpty()||!QFileInfo(root).isAbsolute()||!validId(id)||QFileInfo(root).isSymLink())bad("恢复副本目录无效。");
    if(!QDir().mkpath(root))bad("无法创建恢复副本目录。");
    root_=QFileInfo(root).canonicalFilePath();if(root_.isEmpty())bad("无法访问恢复副本目录。");
    regular(path());regular(root_+'/'+id_+".lock");
    lock_=std::make_unique<QLockFile>(root_+'/'+id_+".lock");lock_->setStaleLockTime(0);
    if(!lock_->tryLock(0))bad("此恢复副本正在被另一个窗口使用。");
}
RecoveryLease::~RecoveryLease()=default;
QString RecoveryLease::path()const{return root_+'/'+id_+".fsr";}
void RecoveryLease::checkpoint(const QByteArray& fbm,quint64 revision,const QString& source){
    if(fbm.isEmpty()||fbm.size()>MaxBytes||revision<1||revision>9007199254740991ULL||source.size()>32768||source.contains(QChar(0)))bad("恢复数据超出允许范围。");
    regular(path());
    const auto h=QJsonDocument(QJsonObject{{"format","FreeShipRecovery"},{"version",1},{"document_id",id_},{"revision",double(revision)},
        {"source",source},{"saved_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"bytes",double(fbm.size())},{"sha256",QString::fromLatin1(QCryptographicHash::hash(fbm,QCryptographicHash::Sha256).toHex())}}).toJson(QJsonDocument::Compact);
    if(h.size()>65536)bad("恢复副本头部过长。");
    char size[4];qToLittleEndian<quint32>(quint32(h.size()),size);
    QSaveFile f(path());f.setDirectWriteFallback(false);
    if(!f.open(QIODevice::WriteOnly)||f.write(Magic)!=Magic.size()||f.write(size,4)!=4||f.write(h)!=h.size()||f.write(fbm)!=fbm.size()||!f.commit())
        bad("恢复副本写入失败，上一份副本已保留。请检查剩余空间，并手动保存船模。");
}
QByteArray RecoveryLease::read(RecoveryEntry* info)const{
    regular(path());QFile f(path());const auto h=header(f,id_);const auto bytes=f.readAll();
    if(f.error()!=QFile::NoError||bytes.size()!=qint64(h["bytes"].toDouble())||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()!=h["sha256"].toString().toLatin1())
        bad("恢复副本校验失败，文件已保留。可以稍后处理其他副本。");
    if(info)*info=entry(h);return bytes;
}
void RecoveryLease::remove(){regular(path());if(QFileInfo::exists(path())&&!QFile::remove(path()))bad("未能清理恢复副本，文件仍保留在恢复目录。");}
QVector<RecoveryEntry> recoveryEntries(const QString& root){
    QVector<RecoveryEntry> result;
    if(root.isEmpty()||!QFileInfo(root).isDir()||QFileInfo(root).isSymLink())return result;
    for(const auto& file:QDir(root).entryInfoList({"*.fsr"},QDir::Files,QDir::Time)){
        const auto id=file.completeBaseName();if(!validId(id)||file.isSymLink())continue;
        std::unique_ptr<RecoveryLease> lease;
        try{lease=std::make_unique<RecoveryLease>(root,id);}catch(const std::exception&){continue;}
        try{QFile f(lease->path());result.append(entry(header(f,id)));}
        catch(const std::exception& ex){result.append({id,{}, {},QString::fromUtf8(ex.what()),0,file.size()});}
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.savedUtc>b.savedUtc;});
    return result;
}
}
