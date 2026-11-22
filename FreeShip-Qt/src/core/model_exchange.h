// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_snapshot.h"
#include <QJsonObject>
#include <QStringList>
#include <atomic>
#include <optional>

namespace fs::exchange {
struct Options {
    // OBJ/STL have no standard length unit. These are explicit file units.
    QString units="m";
    bool legacyAxes=false; // file (Y,Z,X), as emitted by old FreeShip OBJ
    bool controlNet=false, symmetric=false;
    bool reconstructObj=false, inferObjCreases=true;
    double objToleranceMm=0; // zero: 0.1% of the reference bounding box diagonal
    bool fullHull=true, includeHidden=false, binaryStl=true;
    int columns=0, rows=10; // 0: eight chine samples, or the source surface curve count
    bool intermediateBulkheads=true;
    QString textEncoding="cp1252";
    QVector<EntityId> faceIds; // Empty: all faces in the chosen layer visibility scope.
};
struct ObjFitReport {
    double toleranceMm=0, maximumMm=0, rmsMm=0;
    qsizetype samples=0;
};
struct Document {
    ModelSnapshot model;
    QString creator;
    bool particularsSet=false;
    QStringList notes;
    int discardedZeroAreaFaces=0; // Source-degenerate VRML faces; UI confirmation required.
    std::optional<ObjFitReport> objFit;
    QVector<QVector<EntityId>> controlCurves;
    QVector<QVector<Vec3>> markers;
    QVector<double> stations;
    void validate() const;
    QJsonObject bridgeData() const;
};
Document read(const QString& path,const QString& format,const Options& options={},
              const std::atomic_bool* cancel=nullptr);
// Uses the complete original-side subdivision snapshot, independent of camera,
// animation, display symmetry and GPU caches. FEF always stores the control net.
void write(const QString& path,const QString& format,const ModelSnapshot& model,
           const Options& options={});
}
