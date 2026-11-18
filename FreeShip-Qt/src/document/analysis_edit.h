// GPL-2.0-or-later. Persistent analysis parameters; no calculator/UI execution.
#pragma once
#include "topology_edit.h"
namespace fs::document {
bool nativeAnalysisCommand(const QJsonObject &request);
TopologyEdit editAnalysis(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request);
}
