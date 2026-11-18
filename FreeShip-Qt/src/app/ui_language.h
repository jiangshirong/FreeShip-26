// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QPair>
#include <QString>
class QWidget;
class QObject;
class QJsonObject;
// Source strings are UI-owned. Never pass model names, paths or editable data.
const char* uiText(const char* source);
QString uiText(const QString& source);
// A stable product context distinguishes shared source words such as an action
// and a menu heading. User-owned text remains outside this API.
const char* uiText(const char* source,const char* context);
QString uiText(const QString& source,const QString& context);
// Product fragments are translated separately. Explicit {value: ...} fragments
// preserve numeric/user data; never translate a completed mixed-data sentence.
QString uiTextParts(const QJsonArray& parts);
QString uiMetadataText(const QJsonObject& metadata,const char* key);
void bindUiText(QObject* object,const char* property,const QString& source,const QString& context = {});
QString uiLanguage();
QString uiLanguageError();
QJsonArray uiLanguages();
QJsonArray uiLanguageChoices();
QPair<QString,QString> quickUiLanguages(const QJsonObject& preferences);
// Optional directory supports isolated catalogs in tests and embedded hosts.
// Rejected catalogs leave the active directory and language unchanged.
QString setUiLanguage(const QString& id,const QString& directory = {});
QString checkUiLanguage(const QString& id);
void bindUiLanguage(QWidget* root);
