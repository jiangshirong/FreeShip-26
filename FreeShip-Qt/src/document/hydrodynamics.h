// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fbm_codec.h"
namespace fs::document {
bool nativeHydrodynamics(const QString &method);
QJsonObject hydrodynamics(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings);
QJsonObject hydrodynamicsBindings();
}
