// GPL-2.0-or-later. Names, point groups and marker polylines.
#pragma once
#include "topology_edit.h"
namespace fs::document {
TopologyEdit editScopedPointLocks(const Fbm&,const QJsonObject&,const std::vector<int>& points,bool lock,bool all);
bool nativeAttributeCommand(const QJsonObject &request);
TopologyEdit editAttributes(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request);
}
