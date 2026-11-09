// GPL-2.0-or-later. Native layer/section version and persistence probe.
#include "document/definition_edit.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <cstdio>
#include <stdexcept>
static QByteArray read(const QString &path) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot open probe input");
    return file.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();QJsonObject result;
    if (args.size()!=4) return 2;
    try {
        const auto source=fs::document::Fbm::read(read(args[1]));
        QJsonParseError error;const auto json=QJsonDocument::fromJson(read(args[2]),&error);
        if (error.error!=QJsonParseError::NoError || !json.isObject()) throw std::runtime_error("Invalid probe JSON");
        const auto root=json.object(),bindings=root["bindings"].toObject(),request=root["request"].toObject();
        const int minimum=fs::document::definitionMinimumVersion(source,bindings,request);
        result["minimum_version"]=minimum;
        if (!root["minimum_only"].toBool()) {
            const auto edit=fs::document::editDefinitions(source,bindings,request);
            edit.document.writeAtomic(args[3]);result["changed"]=edit.changed;result["bindings"]=edit.bindings;
        }
        result["ok"]=true;
    } catch (const std::exception &e) {result["ok"]=false;result["error"]=QString::fromUtf8(e.what());}
    const auto output=QJsonDocument(result).toJson(QJsonDocument::Compact);std::fwrite(output.constData(),1,size_t(output.size()),stdout);std::fputc('\n',stdout);
    return result["ok"].toBool() ? 0 : 1;
}
