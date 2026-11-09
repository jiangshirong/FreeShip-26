// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QColor>
#include <algorithm>

// FreeSubdivisionControlFace.Draw blends the water tint into layer RGB before
// lighting. This is independent of the layer's geometric transparency. Use
// the actual blue channel; the old Pascal routine accidentally reads green.
inline QColor underwaterTint(QColor layer,QColor water,int alpha) {
    alpha=std::clamp(alpha,0,255);
    const auto mix=[&](int base,int tint){return (base*(255-alpha)+tint*alpha)/255;};
    return QColor(mix(layer.red(),water.red()),mix(layer.green(),water.green()),
                  mix(layer.blue(),water.blue()),layer.alpha());
}
