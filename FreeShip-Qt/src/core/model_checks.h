#pragma once
#include "model_snapshot.h"
#include <QJsonArray>
namespace fs {
struct ModelIssue {QString code,kind,message;QVector<EntityId> ids;};
QVector<ModelIssue> inspectModel(const ModelSnapshot&);
// Plus marker text: heading, XYZ rows, blank lines between paths, optional EOF.
// Coordinates are in the current model's units, as in the original importer.
QJsonArray parseMarkerText(const QByteArray&,const QString& units);
}
