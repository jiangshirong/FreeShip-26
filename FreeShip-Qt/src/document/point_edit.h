// GPL-2.0-or-later. Native absolute point editing and dependency propagation.
#pragma once
#include "fbm_codec.h"

namespace fs::document {
// Shared dependency graph for coordinate moves and constraint-definition edits.
// Reverse lists are built once and updated when a relation changes.
class PointEditor {
public:
    explicit PointEditor(std::vector<FbmControlPoint>& points, const QJsonObject& bindings = {});
    bool move(size_t point, FbmPosition target);
    void anchor(size_t point, int parent, bool hard);
    void line(size_t point, int a, int b);
    void writeOrder(QJsonObject& bindings) const;
private:
    std::vector<FbmControlPoint>& points_;
    std::vector<std::vector<size_t>> anchored_, constrained_;
};
FbmPosition projectPointOnLine(FbmPosition p, FbmPosition a, FbmPosition b);
// The ID array maps serialized point order to document-scoped stable IDs.
// Throws before a candidate is published. No mutable live model is modified.
Fbm editPoints(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request);
}
