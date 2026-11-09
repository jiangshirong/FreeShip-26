// GPL-2.0-or-later. Names, point groups and marker polylines.
#pragma once
#include "topology_edit.h"
namespace fs::document {
bool nativeAttributeCommand(const QJsonObject &request);
TopologyEdit editAttributes(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request);
}
