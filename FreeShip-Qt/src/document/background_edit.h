// GPL-2.0-or-later. Native persistent background-image transactions.
#pragma once
#include "topology_edit.h"

namespace fs::document {
bool nativeBackgroundCommand(const QJsonObject &request);
int backgroundMinimumVersion(const Fbm &source,const QJsonObject &bindings,
                             const QJsonObject &request);
TopologyEdit editBackground(const Fbm &source,const QJsonObject &bindings,
                            const QJsonObject &request);
}
