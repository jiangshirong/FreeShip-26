// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ghs_export.h"
namespace fs::ship {
// Archimedes HLL/APP and the inherited Windows PAM geometry template.
// References are explicit longitudinal positions / waterline height, in metres.
ghs::Result build(const native_geometry::DocumentInput&,const ModelSnapshot&,
                  const QString& format,const QString& units,bool closeDeck,
                  const QJsonObject& references);
}
