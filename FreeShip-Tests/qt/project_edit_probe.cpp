// GPL-2.0-or-later. Standalone native project transaction/format probe.
#include "document/project_edit.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cstdio>
#include <stdexcept>

static QByteArray read(const QString &path) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot open probe input");
    return file.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QJsonObject result;const auto args=app.arguments();
    if (args.size()!=4) { std::fputs("usage: native_project_edit_probe input.fbm request.json output.fbm\n",stderr);return 2; }
    try {
        const auto source=fs::document::Fbm::read(read(args[1]));QJsonParseError error;
        const auto json=QJsonDocument::fromJson(read(args[2]),&error);
        if (error.error!=QJsonParseError::NoError || !json.isObject()) throw std::invalid_argument("Invalid probe JSON");
        const auto root=json.object();const auto bindings=root["bindings"].toObject(),request=root["request"].toObject();
        const auto minimum=fs::document::projectMinimumVersion(source,bindings,request);
        result["minimum_version"]=minimum;
        if (root["minimum_only"].toBool()) result["ok"]=true;
        else {
            const auto edited=fs::document::editProject(source,bindings,request);edited.document.writeAtomic(args[3]);
            result["ok"]=true;result["changed"]=edited.changed;result["byte_identical"]=source.bytes()==edited.document.bytes();
            result["bindings_unchanged"]=edited.bindings==bindings;result["inspection"]=edited.document.inspect();
        }
    } catch (const std::exception &error) { result["ok"]=false;result["error"]=QString::fromUtf8(error.what()); }
    const auto output=QJsonDocument(result).toJson(QJsonDocument::Compact);std::fwrite(output.data(),1,size_t(output.size()),stdout);std::puts("");
    return 0; // Rejection is a successful probe result, never a swallowed crash.
}
