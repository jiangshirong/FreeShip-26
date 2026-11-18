// GPL-2.0-or-later. Standalone structural/atomicity face-tool diagnostic.
// --model FBM --request JSON [--bindings JSON] --output JSON [--result FBM]
// Deliberately has no subdivision/renderer, so malformed topology reaches the
// native repair guard instead of the independent scene-publication guard.
#include "document/face_tools.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <iostream>
#include <stdexcept>

QByteArray readFile(const QString &path) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly) || file.size()>fs::document::Fbm::MaximumBytes) throw std::runtime_error("Cannot read face probe input");return file.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QString model,request,bindingsPath,output,resultFile;auto args=app.arguments();
    for (int i=1;i<args.size();++i) {
        if (i+1>=args.size()) return 2;
        const auto key=args[i],value=args[++i];
        if (key=="--model") model=value;else if (key=="--request") request=value;else if (key=="--bindings") bindingsPath=value;
        else if (key=="--output") output=value;else if (key=="--result") resultFile=value;else return 2;
    }
    QJsonObject result;int status=0;
    try {
        const auto source=fs::document::Fbm::read(readFile(model));const auto net=source.controlNet();QJsonObject bindings,high;
        const auto assign=[&](const char *key,size_t count) {QJsonArray ids;for (size_t i=0;i<count;++i) ids.append(qint64(i+1));bindings[key]=ids;high[key]=qint64(count);};
        assign("points",net.points.size());assign("edges",net.edges.size());assign("faces",net.faces.size());assign("curves",net.curves.size());
        const auto layers=source.blockBytes("layers");assign("layers",size_t(std::max(1,qFromLittleEndian<qint32>(layers.constData()))));bindings["high_water"]=high;bindings["fbm_encoding"]="cp1252";
        if (!bindingsPath.isEmpty()) bindings=QJsonDocument::fromJson(readFile(bindingsPath)).object();
        const auto command=QJsonDocument::fromJson(readFile(request)).object();const auto edited=fs::document::editFaceTools(source,bindings,command);
        if (!resultFile.isEmpty()) edited.document.writeAtomic(resultFile);
        result={{"ok",true},{"changed",edited.changed},{"bindings",edited.bindings},{"inspection",edited.document.inspect()}};
    } catch (const std::exception &error) {result={{"ok",false},{"error",error.what()}};status=1;}
    const auto json=QJsonDocument(result).toJson(QJsonDocument::Indented);QSaveFile file(output);
    if (!file.open(QIODevice::WriteOnly) || file.write(json)!=json.size() || !file.commit()) return 3;
    std::cout<<json.constData();return status;
}
