// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fbm_codec.h"
#include <memory>
namespace fs::document {
class Hydrostatics;
bool nativeHydrodynamics(const QString &method);
QJsonObject hydrodynamics(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings,std::shared_ptr<Hydrostatics> prepared={});
QJsonObject hydrodynamicsBindings();
}
