// GPL-2.0-or-later. Native selected and whole-model affine edits.
#pragma once
#include "fbm_codec.h"

namespace fs::document {
bool nativeTransformCommand(const QJsonObject &request);
// Stable bindings map serialized point/edge/face/curve order to document IDs.
// Returns an independent candidate; validation failures leave source untouched.
// changed follows legacy transaction semantics: whole scale with a non-unit
// factor is an edit even if every stored float rounds back to its old value.
Fbm editTransforms(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request,
                   bool *changed = nullptr);
}
