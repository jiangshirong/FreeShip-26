// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace fs::document {
// Read-only bootstrap. The directory is the packaged resource directory,
// never the current working directory or the original application's profile.
// Captions deliberately use cn, like the existing Qt bridge. This is not a
// whole-application language switch and does not overwrite edited model text.
QJsonObject preferenceValues(const QString &resourceDirectory, const QJsonObject &overrides = {});
QJsonObject resolvedPreferenceColors(const QJsonObject &graphic);
// Explicit, read-only import. Returns a validated partial values object, source
// paths, and notes; callers must merge it into a draft, never save implicitly.
QJsonObject importLegacyPreferences(const QString &filename, const QString &resourceDirectory);
QJsonObject documentContext(const QString &resourceDirectory, const QJsonArray &colors, const QJsonObject &overrides = {});
}
