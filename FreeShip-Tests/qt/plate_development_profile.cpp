// SPDX-License-Identifier: GPL-2.0-or-later
#include "document/document_create.h"
#include "document/plates.h"
#include "document/preferences.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 3) return 2;
    QFile fixture(args[1]);
    if (!fixture.open(QIODevice::ReadOnly)) return 3;
    try {
        using namespace fs::document;
        const auto source = Fbm::read(fixture.readAll());
        const auto context = documentContext(args[2], documentColors(source));
        const auto document = loadDocument(source, context);
        for (int i = 0; i != 3; ++i) {
            const auto result = developPlates(document.document, document.bindings,
                                              context["plate_context"].toObject(), {});
            std::cout << result["plates"].toArray().size() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 4;
    }
}
