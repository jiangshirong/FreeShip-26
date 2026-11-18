// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFont>

// Keep the established font, size and weight. Windows default/full hinting
// can turn small CJK bold glyphs into coarse, sparsely shaded pixel shapes.
inline QFont studioTextFont(QFont font){
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias|QFont::PreferQuality));
    return font;
}
