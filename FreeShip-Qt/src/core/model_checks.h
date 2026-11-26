#pragma once
#include "model_snapshot.h"
#include <QJsonArray>
#include <QSet>
#include <stdexcept>
namespace fs {
struct ModelIssue {QString code,kind,message;QVector<EntityId> ids;};
QVector<ModelIssue> inspectModel(const ModelSnapshot&);
// Orders one selected loop of control edges that can accept an additional face.
QVector<EntityId> faceBoundaryOrder(const ModelSnapshot&, const QSet<EntityId>& selection);
// Plus marker text: heading, XYZ rows, blank lines between paths, optional EOF.
// Coordinates are in the current model's units, as in the original importer.
struct ReferenceCurveCoordinateError : std::runtime_error {
    explicit ReferenceCurveCoordinateError(int lineNumber);
    int lineNumber;
};
QJsonArray parseMarkerText(const QByteArray&,const QString& units);
}
