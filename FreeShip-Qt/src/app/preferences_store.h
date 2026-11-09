// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QString>

// Qt owns its preference file; legacy INI/theme files stay read-only. Unknown
// top-level values are retained for settings owned by other components.
class PreferencesStore {
public:
    explicit PreferencesStore(QString filename = {});
    static QString defaultFilename();
    QJsonObject read();
    void save(const QJsonObject &values);
    QString filename() const { return filename_; }
private:
    QString filename_;
    QByteArray observed_;
    QJsonObject envelope_;
};
QJsonObject nativePreferenceOptions(const QJsonObject &values);
QJsonObject effectivePreferences(const QJsonObject &values);
QJsonObject preferenceAppearance(const QJsonObject &values);
QString validatePreferences(const QJsonObject &values);
void activatePreferences(const QJsonObject &values);
QString preferenceDirectory(const QString &kind);
QString preferenceFileLocation(const QString &kind, const QString &previous = {});
