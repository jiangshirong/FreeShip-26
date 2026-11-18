// Retained standalone native Lackenby diagnostic, GPL-2.0-or-later.
// lackenby_probe --model input.fbm [--request request.json] --output result.json
#include "document/lackenby_edit.h"
#include "document/geometry_input.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <iostream>
#include <stdexcept>

QByteArray readFile(const QString &name) {
    QFile f(name);if (!f.open(QIODevice::ReadOnly) || f.size()>fs::document::Fbm::MaximumBytes) throw std::runtime_error("Cannot read probe input");return f.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QString model,request,output;const auto arguments=app.arguments();
    for (int i=1;i<arguments.size();++i) {
        if (arguments[i]=="--model" && i+1<arguments.size()) model=arguments[++i];
        else if (arguments[i]=="--request" && i+1<arguments.size()) request=arguments[++i];
        else if (arguments[i]=="--output" && i+1<arguments.size()) output=arguments[++i];
        else return 2;
    }
    QJsonObject result;int status=0;
    try {
        result["stage"]="read";
        const auto source=fs::document::Fbm::read(readFile(model));
        result["stage"]="geometry";
        const auto input=fs::document::documentGeometry(source);
        result["geometry"]=QJsonObject{{"points",qint64(input.points.size())},{"edges",qint64(input.edges.size())},
            {"faces",qint64(input.faces.size())},{"layers",qint64(input.layers.size())},{"level",int(input.desiredLevel)}};
        result["stage"]="measure";result["analysis"]=fs::document::inspectLackenby(source);
        if (!request.isEmpty()) {
            auto command=QJsonDocument::fromJson(readFile(request)).object();QJsonObject bindings;QJsonArray layers;
            for (size_t i=0;i<fs::document::documentGeometry(source).layers.size();++i) layers.append(int(i+1));bindings["layers"]=layers;
            bool changed=false;const auto edited=fs::document::editLackenby(source,bindings,command,&changed);QJsonArray points;
            for (const auto &p:edited.controlPoints()) points.append(QJsonArray{p.x,p.y,p.z});result["points"]=points;result["changed"]=changed;
            result["after"]=fs::document::inspectLackenby(edited);
        }
        result["ok"]=true;result["stage"]="complete";
    } catch (const std::exception &e) {result["ok"]=false;result["error"]=e.what();status=1;}
    const auto json=QJsonDocument(result).toJson(QJsonDocument::Indented);
    if (!output.isEmpty()) {QSaveFile f(output);if (!f.open(QIODevice::WriteOnly) || f.write(json)!=json.size() || !f.commit()) return 3;}
    std::cout<<json.constData();return status;
}
