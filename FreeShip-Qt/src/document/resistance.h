// GPL-2.0-or-later. Native resistance workflows; original external solvers remain separate.
#pragma once
#include "fbm_codec.h"
#include <memory>
namespace fs::document {
class Hydrostatics;
bool nativeResistance(const QString &method);
QJsonObject resistance(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings,std::shared_ptr<Hydrostatics> prepared={});
}
