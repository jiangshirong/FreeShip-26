// GPL-2.0-or-later. Transactional control-net editing; no widgets or Pascal calls.
#pragma once
#include "fbm_codec.h"

namespace fs::document {
struct TopologyEdit { Fbm document; QJsonObject bindings; bool changed=false; };
bool nativeTopologyCommand(const QJsonObject &request);
TopologyEdit editTopology(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request);
TopologyEdit constructHullNet(const Fbm &empty, const QJsonObject &bindings,
                             const std::vector<FbmPosition> &points, int columns, int rows);
}
