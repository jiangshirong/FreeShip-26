// GPL-2.0-or-later. Calculation-only native hydrostatics; see
// docs/migration/NATIVE-HYDROSTATICS.md for provenance and numerical boundaries.
#pragma once
#include "fbm_codec.h"
#include <memory>
#include <array>
#include <optional>
#include <functional>
namespace fs::document {
// Owns a subdivision mesh and its section index for one immutable FBM revision.
// A calculation cannot edit the document, history, cached drawing or FBM bytes.
struct ComparisonConditions {
    double draftM=0,trimM=0,densityKgM3=1025,massKg=0;
    std::array<double,3> gravityM{};
    bool equilibrium=false;
};
class Hydrostatics {
public:
    explicit Hydrostatics(const Fbm &source,const QJsonObject &bindings={});
    ~Hydrostatics();
    static QJsonObject comparison(const Fbm&,const ComparisonConditions&,const QString& cancelFile);
    QJsonObject calculate(const QJsonObject &request, const QString &cancelFile,
                          const std::function<void(int,int)> &progress={});
    QJsonObject defaults(const QString &encoding);
    // Read-only display data; zero-draft crease placement is deliberately
    // separate from project-draft hydrostatic annotations.
    float presentationCreaseSplit();
    QJsonObject presentation(const QJsonObject &visibility,const QJsonObject &context,std::optional<float> creaseSplit={});
    // Publication-only reuse after a verified section-definition-only edit.
    void updateSectionDefinitions(const Fbm &source);
    // Reuse only the exact mesh inputs and accumulation order. Settings are
    // refreshed even when no mesh rebuild is needed (e.g. a saved draft range).
    bool updateCompatibleSource(const Fbm &source,const QJsonObject &bindings);
    double longitudinalSpan() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
