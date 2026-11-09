// Native lines-plan assembly derived from current FREE!ship Plus.
// GPL-2.0-or-later. Original copyright 2005 Martijn van Engeland;
// Plus contributors Timoshenko V.F. and Mark Malakanov. See COPYING.
#pragma once
#include "document.h"
#include "../scene.h"
namespace fs::native_geometry {
struct LinesplanOutput {
    Scene scene;
    QByteArray dxf;
    QByteArray sectionDisplay;
    QByteArray surfaceDisplay;
    QJsonObject diagnostics;
};
LinesplanOutput buildLinesplan(const DocumentInput &document,const QString& sectionCache={},const QByteArray& engine={});
} // namespace fs::native_geometry
