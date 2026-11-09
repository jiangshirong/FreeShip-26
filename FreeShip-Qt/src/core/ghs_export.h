// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_snapshot.h"
#include "native/document.h"
#include "native/sections.h"
#include <QJsonObject>
namespace fs::ghs {
struct Result { QByteArray data; QJsonObject diagnostics; };
// Chordal approximation of the inherited cubic interpolation, preserving all
// original ordinates/knuckles. This is not a surface reconstruction tolerance.
std::vector<std::array<double,2>> sampleSection(const native_geometry::SectionCurve& curve,double tolerance);
// One symmetric displacement hull, using all hydrostatics layers and the
// document's existing stations. Display visibility and camera are irrelevant.
Result build(const native_geometry::DocumentInput& input,const ModelSnapshot& model,
             const QString& units,bool closeDeck);
}
