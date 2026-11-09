// GPL-2.0-or-later.
#pragma once
#include "ghs_export.h"
namespace fs::grid {
std::vector<double> sectionBreadths(const std::vector<native_geometry::SectionCurve>& curves,float z);
ghs::Result build(const native_geometry::DocumentInput&,const ModelSnapshot&,
                  const QString& format,const QString& units,const QJsonObject& options,
                  QJsonObject* preview=nullptr);
}
