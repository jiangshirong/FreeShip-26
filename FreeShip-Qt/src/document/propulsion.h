// GPL-2.0-or-later. Native propulsion workflows; external solver licenses remain separate.
#pragma once
#include "fbm_codec.h"
namespace fs::document {
bool nativePropulsion(const QString &calculator);
QJsonObject propulsion(const Fbm &source,const QJsonObject &request,const QString &output);
}
