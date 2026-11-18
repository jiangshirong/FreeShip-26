// Native lines-plan assembly derived from current FREE!ship Plus.
// GPL-2.0-or-later. Original copyright 2005 Martijn van Engeland;
// Plus contributors Timoshenko V.F. and Mark Malakanov. See COPYING.
#pragma once
#include "document.h"
#include "../scene.h"
#include <functional>
namespace fs::native_geometry {
struct LinesplanOutput {
    Scene scene;
    QByteArray dxf;
    QByteArray sectionDisplay;
    QByteArray surfaceDisplay;
    QJsonObject diagnostics;
};
using DisplayReady=std::function<void(const QByteArray&,const QByteArray&)>;
LinesplanOutput buildLinesplan(const DocumentInput &document,const QString& sectionCache={},const QByteArray& engine={},const DisplayReady& displayReady={},bool displayOnly=false);
} // namespace fs::native_geometry
