// Native Lackenby transformation and its measurement. GPL-2.0-or-later.
#pragma once
#include "fbm_codec.h"

namespace fs::document {
bool nativeLackenbyCommand(const QJsonObject &request);
QJsonObject inspectLackenby(const Fbm &source);
Fbm editLackenby(const Fbm &source,const QJsonObject &bindings,
                 const QJsonObject &request,bool *changed=nullptr);
}
