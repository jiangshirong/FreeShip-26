// SPDX-License-Identifier: GPL-2.0-or-later
#include "calculation_tools.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>
namespace fs::document {
namespace {
QString normalize(QString path,const QString& base){
    if(path.isEmpty())return {};
    path=QDir::cleanPath(QFileInfo(path).isAbsolute()?path:QDir(base).absoluteFilePath(path));
    if(QFileInfo(QDir(path).filePath("Exec")).isDir())path=QDir(path).filePath("Exec");
    return QDir::cleanPath(path);
}
QStringList executables(){return {"hship.EXE","CALCPROP.EXE","HOLLENBH.EXE","fungleib.EXE","OORTMERS.EXE",
    "RNTSP.EXE","UBCRT.EXE","SeaMargn.EXE","PowerPrd.EXE","CLEMPOUP.EXE","CLEMBLOU.EXE","BUNKOV.EXE",
    "COMPTON.EXE","WOLFSON.EXE","RADOJCIC.EXE","PropPred.EXE","FPPCALC.EXE","Ishercof.exe","ManeuvPP.EXE","RVRSSHIP.EXE","ADD_MASS.EXE"};}
QJsonObject identity(const QString& root,const QString& name){
    QFile f(QDir(root).filePath(name));QJsonObject v{{"file",name},{"available",false}};
    if(!f.open(QIODevice::ReadOnly)){v["reason"]="缺少或无法读取";return v;}
    if(f.size()<=0||f.size()>32*1024*1024){v["reason"]="文件为空或超出允许大小";return v;}
    QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&f)){v["reason"]="读取失败";return v;}
    v["available"]=true;v["bytes"]=double(f.size());v["sha256"]=QString::fromLatin1(hash.result().toHex());return v;
}
}
QString calculationToolDirectory(const QString& selected,const QString& applicationDirectory){
    const auto base=applicationDirectory.isEmpty()?QCoreApplication::applicationDirPath():applicationDirectory;
    if(!selected.trimmed().isEmpty())return normalize(selected.trimmed(),base);
    QFile config(QDir(base).filePath("calculation-tools.json"));
    if(config.exists()){
        if(!config.open(QIODevice::ReadOnly)||config.size()>65536)return {};
        auto data=QJsonDocument::fromJson(config.readAll()).object();return normalize(data["directory"].toString().trimmed(),base);
    }
    // Optional user-supplied tools beside a portable installation.
    for(const auto& relative:{"CalculationTools/Exec","Exec"}){auto path=QDir(base).filePath(relative);if(QFileInfo(path).isDir())return normalize(path,base);}
    return {};
}
QJsonObject inspectCalculationTool(const QString& directory,const QString& executable){
    QJsonObject result{{"program",executable},{"directory",directory},{"ready",false}};QJsonArray files,missing;
    QStringList needed{executable};
    if(executable.compare("hship.EXE",Qt::CaseInsensitive)==0){needed<<"RESIST.CNF";for(int i=1;i<=4;++i)needed<<QString("../BD/RESIST%1.BD").arg(i);}
    if(!directory.isEmpty()&&QFileInfo(QDir(directory).filePath("CONF.PRO")).exists())needed<<"CONF.PRO";
    for(const auto& name:needed){auto entry=directory.isEmpty()?QJsonObject{{"file",name},{"available",false},{"reason","未配置目录"}}:identity(directory,name);
        files.append(entry);if(!entry["available"].toBool())missing.append(name);}
    result["files"]=files;result["missing"]=missing;result["ready"]=missing.isEmpty();return result;
}
QJsonObject requireCalculationTool(const QString& directory,const QString& executable){
    auto result=inspectCalculationTool(directory,executable);if(!result["ready"].toBool()){
        QStringList missing;for(auto name:result["missing"].toArray())missing<<name.toString();
        throw std::invalid_argument(QString("%1 所需文件缺失或不可读：%2。请在首选项的目录设置中指定原软件根目录或 Exec 目录，并保留相邻的 BD 数据目录。")
            .arg(executable,missing.join("、")).toStdString());}
    return result;
}
QJsonObject inspectCalculationTools(const QString& directory){
    const auto path=calculationToolDirectory(directory);QJsonArray tools;int ready=0;
    for(const auto& name:executables()){auto tool=inspectCalculationTool(path,name);ready+=tool["ready"].toBool();tools.append(tool);}
    auto viewer=inspectCalculationTool(path,"DBFview.EXE");
    return {{"directory",path},{"tools",tools},{"ready_count",ready},{"total_count",int(tools.size())},{"optional_database_viewer",viewer},
        {"note","检查文件完整性，不执行程序；计算时另行检查输出。内置方法无需这些工具。"}};
}
QString calculationToolsSummary(const QJsonObject& inventory){
    QString text=QString("外部工具：%1/%2 项文件齐全\n目录：%3\n").arg(inventory["ready_count"].toInt()).arg(inventory["total_count"].toInt()).arg(inventory["directory"].toString());
    for(auto v:inventory["tools"].toArray()){auto tool=v.toObject();QStringList missing;for(auto n:tool["missing"].toArray())missing<<n.toString();
        text+=tool["program"].toString()+"："+(tool["ready"].toBool()?QString("文件齐全"):"缺少 "+missing.join("、"))+"\n";}
    if(!inventory["optional_database_viewer"].toObject()["ready"].toBool())text+="可选数据库查看器 DBFview.EXE 未安装，不影响其他工具。\n";
    return text+inventory["note"].toString();
}
void validateCalculationCache(const QString& directory,bool propulsion){
    auto read=[&](const QString& name){QFile f(QDir(directory).filePath(name));
        if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>2*1024*1024)
            throw std::invalid_argument("计算缓存不完整，请重新计算阻力和推进参数。");
        return f.readAll();};
    auto hash=[](const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());};
    auto fail=[](){throw std::invalid_argument("计算缓存校验失败，请重新计算阻力和推进参数。");};
    if(QFile::exists(QDir(directory).filePath("RESIST-native.json"))){
        const auto m=QJsonDocument::fromJson(read("RESIST-native.json")).object();
        if(m["version"]!=1||!m["payload"].isObject()||m["resist_sha256"]!=hash(read("RESIST.dat"))||
           m["resistp_sha256"]!=hash(read("RESISTp.dat"))||m["payload_sha256"]!=hash(QJsonDocument(m["payload"].toObject()).toJson(QJsonDocument::Compact)))fail();
    }
    if(propulsion&&QFile::exists(QDir(directory).filePath("Propres-native.json"))){
        const auto m=QJsonDocument::fromJson(read("Propres-native.json")).object();
        if(m["version"]!=1||!m["inputs"].isObject()||m["legacy_sha256"]!=hash(read("Propres.dat"))||
           m["inputs_sha256"]!=hash(QJsonDocument(m["inputs"].toObject()).toJson(QJsonDocument::Compact)))fail();
    }
}
}
