// GPL-2.0-or-later. Calculation-only native hydrostatics; see
// docs/migration/NATIVE-HYDROSTATICS.md for provenance and numerical boundaries.
#pragma once
#include "fbm_codec.h"
#include <memory>
namespace fs::document {
// Owns a subdivision mesh and its section index for one immutable FBM revision.
// A calculation cannot edit the document, history, cached drawing or FBM bytes.
class Hydrostatics {
public:
    explicit Hydrostatics(const Fbm &source,const QJsonObject &bindings={});
    ~Hydrostatics();
    QJsonObject calculate(const QJsonObject &request, const QString &cancelFile);
    QJsonObject defaults(const QString &encoding);
    // Read-only display data; zero-draft crease placement is deliberately
    // separate from project-draft hydrostatic annotations.
    QJsonObject presentation(const QJsonObject &visibility,const QJsonObject &context);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
