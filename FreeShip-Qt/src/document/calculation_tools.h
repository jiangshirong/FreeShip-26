// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QString>
namespace fs::document {
// Explicit paths win. Package-relative paths are relative to the application,
// never the current working directory. A selected FreeShip root accepts Exec/.
QString calculationToolDirectory(const QString& selected = {}, const QString& applicationDirectory = {});
QJsonObject inspectCalculationTool(const QString& directory, const QString& executable);
QJsonObject requireCalculationTool(const QString& directory, const QString& executable);
QJsonObject inspectCalculationTools(const QString& directory);
QString calculationToolsSummary(const QJsonObject& inventory);
// Validate precision-preserving native metadata before passing its legacy text
// projection to another calculator. Old caches without metadata remain valid.
void validateCalculationCache(const QString& directory, bool propulsion = false);
}
