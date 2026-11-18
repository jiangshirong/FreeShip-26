// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "sections.h"
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QStringList>
namespace fs::native_geometry {
struct CachedCurves { std::vector<SectionCurve> curves; bool multiple=false; };
// Cache only mathematical cuts. Layout, fill ordering and export remain exact.
class SectionCache {
public:
    SectionCache(const SubdivisionLevel& mesh,const QString& directory,const QByteArray& engine);
    CachedCurves intersect(const SectionIndex& index,const Plane& plane);
    void trim();
    int hits=0,misses=0;
    QJsonObject timings;
private:
    QString directory_;
    QByteArray topology_,engine_;
    std::vector<QByteArray> roots_;
    QJsonObject entries_;
    QStringList order_;
    bool dirty_=false;
};
}
