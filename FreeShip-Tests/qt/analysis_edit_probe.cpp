// GPL-2.0-or-later. Native settings probe; no legacy runtime or window needed.
#include "document/analysis_edit.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <cstdio>
#include <stdexcept>
static QByteArray read(const QString &path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open input");return f.readAll();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()!=4)return 2;QJsonObject result;
    try {
        auto source=fs::document::Fbm::read(read(args[1]));QJsonParseError error;
        auto json=QJsonDocument::fromJson(read(args[2]),&error);
        if(error.error!=QJsonParseError::NoError||!json.isObject())throw std::runtime_error("Invalid request");
        const auto root=json.object();const auto edit=fs::document::editAnalysis(source,root["bindings"].toObject(),root["request"].toObject());
        edit.document.writeAtomic(args[3]);result={{"ok",true},{"changed",edit.changed},{"version",edit.document.version()}};
    }catch(const std::exception &e){result={{"ok",false},{"error",e.what()}};}
    auto data=QJsonDocument(result).toJson(QJsonDocument::Compact);std::fwrite(data.data(),1,data.size(),stdout);std::fputc('\n',stdout);
    return result["ok"].toBool()?0:1;
}
