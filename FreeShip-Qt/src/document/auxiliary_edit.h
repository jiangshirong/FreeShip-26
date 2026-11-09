// GPL-2.0-or-later. Persistent display and auxiliary-curve definitions.
#pragma once
#include "topology_edit.h"
namespace fs::document {
bool nativeAuxiliaryCommand(const QJsonObject &request);
int auxiliaryMinimumVersion(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request);
TopologyEdit editAuxiliary(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request);
}
