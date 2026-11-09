// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_snapshot.h"
#include <QJsonObject>
#include <QStringList>

namespace fs::exchange {
struct Options {
    // OBJ/STL have no standard length unit. These are explicit file units.
    QString units="m";
    bool legacyAxes=false; // file (Y,Z,X), as emitted by old FreeShip OBJ
    bool controlNet=false, symmetric=false;
    bool fullHull=true, includeHidden=false, binaryStl=true;
    int columns=0, rows=10; // 0: eight chine samples, or the source surface curve count
    bool intermediateBulkheads=true;
    QString textEncoding="cp1252";
    QVector<EntityId> faceIds; // Empty: all faces in the chosen layer visibility scope.
};
struct Document {
    ModelSnapshot model;
    QString creator;
    bool particularsSet=false;
    QStringList notes;
    QVector<QVector<EntityId>> controlCurves;
    QVector<QVector<Vec3>> markers;
    QVector<double> stations;
    void validate() const;
    QJsonObject bridgeData() const;
};
Document read(const QString& path,const QString& format,const Options& options={});
// Uses the complete original-side subdivision snapshot, independent of camera,
// animation, display symmetry and GPU caches. FEF always stores the control net.
void write(const QString& path,const QString& format,const ModelSnapshot& model,
           const Options& options={});
}
