// GPL-2.0-or-later. Native layer grouping, NURBS patch assembly and orientation.
#pragma once
#include "topology_edit.h"

namespace fs::document {
bool nativeFaceToolsCommand(const QJsonObject &request);
TopologyEdit editFaceTools(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request);
}
