// GPL-2.0-or-later. Read-only plate development on an immutable native FBM.
#pragma once
#include "fbm_codec.h"
namespace fs::document {
QJsonObject developPlates(const Fbm &source, const QJsonObject &bindings,
                         const QJsonObject &context, const QString &cancelFile);
}
