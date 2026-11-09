// GPL-2.0-or-later. Read-only IGES surface preparation from native FBM data.
#pragma once
#include "fbm_codec.h"
namespace fs::document {
QJsonObject cadPatches(const Fbm &source, const QJsonObject &bindings,
                      const QString &cancelFile);
}
