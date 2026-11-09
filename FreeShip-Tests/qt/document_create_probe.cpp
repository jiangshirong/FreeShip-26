// GPL-2.0-or-later. Native document creation/conversion, no Pascal adapter.
#include "document/document_create.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <cstdio>
#include <stdexcept>

static QByteArray read(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>fs::document::Fbm::MaximumBytes)
        throw std::runtime_error("Cannot read bounded probe input");
    return file.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()!=3){std::fputs("usage: native_document_create_probe requests.json output-directory\n",stderr);return 2;}
    QJsonObject response;
    try {
        const auto root=QJsonDocument::fromJson(read(args[1])).object();
        const auto context=root["context"].toObject();QJsonArray results;
        if(!root["jobs"].isArray() || !QDir().mkpath(args[2]))throw std::runtime_error("Invalid probe batch/output");
        for(const auto value:root["jobs"].toArray()) {
            const auto job=value.toObject();const auto name=job["name"].toString();QJsonObject result{{"name",name}};
            try {
                if(name.isEmpty() || name.contains('/') || name.contains('\\') || name.contains(".."))throw std::runtime_error("Invalid probe job name");
                const auto operation=job["operation"].toString();
                QJsonObject bindings;
                const auto edited=[&](const fs::document::TopologyEdit &value) {
                    bindings=value.bindings;return value.document;
                };
                const auto document=[&]() -> fs::document::Fbm {
                    if(operation=="empty")return fs::document::emptyDocument(context,job["version"].toInt(-1));
                    if(operation=="new")return edited(fs::document::newDocument(job["request"].toObject(),context));
                    if(operation=="import")return edited(fs::document::importDocument(job["request"].toObject(),context));
                    const auto input=fs::document::Fbm::read(read(job["path"].toString()));
                    if(operation=="load")return edited(fs::document::loadDocument(input,context));
                    if(operation=="promote")return fs::document::promoteDocument(input,job["version"].toInt(-1),context);
                    throw std::runtime_error("Unknown probe operation");
                }();
                const auto path=QDir(args[2]).filePath(name+".fbm");document.writeAtomic(path);
                result["ok"]=true;result["path"]=path;result["inspection"]=document.inspect();
                result["bindings"]=bindings.isEmpty()?fs::document::documentBindings(document,context):bindings;
            } catch(const std::exception &error){result["ok"]=false;result["error"]=QString::fromUtf8(error.what());}
            results.append(result);
        }
        response["ok"]=true;response["results"]=results;
    } catch(const std::exception &error){response["ok"]=false;response["error"]=QString::fromUtf8(error.what());}
    const auto data=QJsonDocument(response).toJson(QJsonDocument::Compact);
    std::fwrite(data.constData(),1,size_t(data.size()),stdout);std::puts("");return response["ok"].toBool()?0:1;
}
