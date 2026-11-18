// GPL-2.0-or-later. Native project settings and complete model-unit conversion.
#pragma once
#include "topology_edit.h"

namespace fs::document {
bool nativeProjectCommand(const QJsonObject &request);
// Validate the complete patch and its actual effect before any format promotion.
// Bindings supply FBM text encoding and resolved legacy system colors.
int projectMinimumVersion(const Fbm &source, const QJsonObject &bindings,
                          const QJsonObject &request);
TopologyEdit editProject(const Fbm &source, const QJsonObject &bindings,
                         const QJsonObject &request);
}
