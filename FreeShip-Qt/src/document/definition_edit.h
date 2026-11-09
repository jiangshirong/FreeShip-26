// GPL-2.0-or-later. Layer attributes, layer management and section definitions.
#pragma once
#include "topology_edit.h"
namespace fs::document {
bool nativeDefinitionCommand(const QJsonObject &request);
int definitionMinimumVersion(const Fbm &source, const QJsonObject &bindings,
                             const QJsonObject &request);
TopologyEdit editDefinitions(const Fbm &source, const QJsonObject &bindings,
                            const QJsonObject &request);
}
