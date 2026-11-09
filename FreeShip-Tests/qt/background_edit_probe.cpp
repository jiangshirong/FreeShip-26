// GPL-2.0-or-later. Standalone background transaction and byte-preservation probe.
#include "document/background_edit.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cstdio>
#include <stdexcept>

static QByteArray read(const QString &path) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read probe input");return file.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();QJsonObject result;
    if (args.size()!=4) { std::fputs("usage: native_background_edit_probe input.fbm request.json output.fbm\n",stderr);return 2; }
    try {
        const auto source=fs::document::Fbm::read(read(args[1]));QJsonParseError error;
        const auto root=QJsonDocument::fromJson(read(args[2]),&error);
        if (error.error!=QJsonParseError::NoError || !root.isObject()) throw std::invalid_argument("Invalid probe request");
        const auto json=root.object(),request=json["request"].toObject(),bindings=json["bindings"].toObject();
        result["minimum_version"]=fs::document::backgroundMinimumVersion(source,bindings,request);
        if (json["minimum_only"].toBool()) result["ok"]=true;
        else {
            const auto candidate=fs::document::editBackground(source,bindings,request);candidate.document.writeAtomic(args[3]);
            result["ok"]=true;result["changed"]=candidate.changed;result["byte_identical"]=candidate.document.bytes()==source.bytes();
            result["bindings_unchanged"]=candidate.bindings==bindings;
        }
    } catch (const std::exception &error) { result["ok"]=false;result["error"]=QString::fromUtf8(error.what()); }
    const auto text=QJsonDocument(result).toJson(QJsonDocument::Compact);std::fwrite(text.data(),1,size_t(text.size()),stdout);std::puts("");return 0;
}
