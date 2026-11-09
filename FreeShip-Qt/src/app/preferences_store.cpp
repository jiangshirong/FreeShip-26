// SPDX-License-Identifier: GPL-2.0-or-later
#include "preferences_store.h"
#include "document/preferences.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QVariant>
#include <cmath>
#include <stdexcept>
namespace {
[[noreturn]] void bad(const QString &s){throw std::runtime_error(s.toStdString());}
QString resources(){return QCoreApplication::applicationDirPath()+"/bridge";}
QByteArray readBytes(const QString &path){
    if(path.isEmpty() || !QFileInfo::exists(path))return {};
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>1024*1024)bad("无法读取偏好设置文件："+path);
    auto data=file.readAll();if(file.error()!=QFile::NoError)bad("偏好设置读取未完成。");return data;
}
QByteArray hash(const QByteArray &bytes){return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);}
}
PreferencesStore::PreferencesStore(QString filename):filename_(std::move(filename)),observed_(hash({})){}
QString PreferencesStore::defaultFilename(){return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/preferences.json";}
QJsonObject PreferencesStore::read(){
    const auto data=readBytes(filename_);observed_=hash(data);envelope_={};if(data.isEmpty())return {};
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())bad("偏好设置文件损坏，当前使用默认设置。");
    const auto root=doc.object();
    if(root["format"]!="FreeShipQtPreferences" || root["version"]!=1 || !root["values"].isObject())bad("不支持此偏好设置文件版本。");
    envelope_=root;return root["values"].toObject();
}
void PreferencesStore::save(const QJsonObject &values){
    if(filename_.isEmpty())bad("偏好设置的保存位置不可用。");
    if(!QDir().mkpath(QFileInfo(filename_).absolutePath()))bad("无法创建偏好设置目录。");
    QLockFile lock(filename_+".lock");if(!lock.tryLock(0))bad("另一窗口正在保存偏好设置，请稍后重试。");
    if(hash(readBytes(filename_))!=observed_)bad("偏好设置已被其他窗口修改，请重新打开设置窗口。");
    auto root=envelope_;root["format"]="FreeShipQtPreferences";root["version"]=1;root["values"]=values;
    const auto bytes=QJsonDocument(root).toJson(QJsonDocument::Indented);
    QSaveFile file(filename_);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())bad("无法保存偏好设置："+file.errorString());
    observed_=hash(bytes);envelope_=root;
}
QJsonObject nativePreferenceOptions(const QJsonObject &values){
    QJsonObject out;for(const QString key:{"theme","fbm_encoding","undo_memory_mb","graphic"})if(values.contains(key))out[key]=values[key];return out;
}
QJsonObject effectivePreferences(const QJsonObject &values){
    auto result=fs::document::preferenceValues(resources(),nativePreferenceOptions(values));
    auto graphic=result["graphic"].toObject();
    // Match the accepted Qt appearance until a theme or individual color is
    // explicitly selected. The old bridge's unused control colors were not the
    // colors actually painted by the Qt renderer.
    if(!values.contains("theme")){
        const QJsonObject displayed{{"viewportcolor",0xecefe8},{"edgecolor",0x7d8566},{"creaseedgecolor",0x2243a0},
            {"regularpointcolor",0x555d31},{"creasepointcolor",0x324d62},{"cornerpointcolor",0x3a4152},
            {"dartpointcolor",0xff00ff},{"leakpointcolor",0x99ff00},{"selectcolor",0x1965e0},{"normalcolor",0xc0c0c0}};
        const auto explicitFields=values["graphic"].toObject();
        for(auto it=displayed.begin();it!=displayed.end();++it)if(!explicitFields.contains(it.key()))graphic[it.key()]=it.value();
    }
    result["graphic"]=graphic;result["tool_icon_size"]=values.value("tool_icon_size").toInt(20);
    result["directories"]=values["directories"].toObject();return result;
}
QJsonObject preferenceAppearance(const QJsonObject &values){
    auto fields=values.contains("theme")?effectivePreferences(values)["graphic"].toObject():values["graphic"].toObject();
    return fs::document::resolvedPreferenceColors(fields);
}
QString validatePreferences(const QJsonObject &values){
    try{
        effectivePreferences(values);
        if(values.contains("tool_icon_size")){
            const auto v=values["tool_icon_size"];const double n=v.toDouble();
            if(!v.isDouble() || std::floor(n)!=n || n<12 || n>128)bad("工具图标大小应为 12 至 128。");
        }
        if(values.contains("directories") && !values["directories"].isObject())bad("目录设置无效。");
        const auto paths=values["directories"].toObject();
        for(const QString key:{"open","save","import","export","solver","temporary"}){
            if(!paths.contains(key))continue;
            if(!paths[key].isString())bad("目录格式无效。");
            const auto path=paths[key].toString();if(path.isEmpty())continue;
            if(path.size()>32767 || path.contains(QChar(0)) || !QFileInfo(path).isAbsolute() || !QFileInfo(path).isDir())bad("目录不存在或不是完整路径："+path);
            if(key=="temporary"){
                QTemporaryFile test(QDir(path).filePath("freeship-preferences-XXXXXX"));
                if(!test.open())bad("临时目录不可写："+path);
            }
        }
    }catch(const std::exception &e){return QString::fromUtf8(e.what());}
    return {};
}
void activatePreferences(const QJsonObject &values){QCoreApplication::instance()->setProperty("freeship.preferences",values);}
QString preferenceDirectory(const QString &kind){return QCoreApplication::instance()->property("freeship.preferences").toJsonObject()["directories"].toObject()[kind].toString();}
QString preferenceFileLocation(const QString &kind,const QString &previous){
    const auto folder=preferenceDirectory(kind);if(folder.isEmpty())return previous;
    const QFileInfo info(previous);return !previous.isEmpty() && !info.suffix().isEmpty()?QDir(folder).filePath(info.fileName()):folder;
}
