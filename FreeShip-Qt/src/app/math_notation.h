// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QString>
#include <QPair>

// Presentation only. Backend keys, arithmetic, filenames and numeric exports
// remain unchanged; symbols are reviewed explicitly, never inferred from Ab.
namespace notation {
// Units are presentation metadata, never translated UI text. Unknown English
// unit names pass through unchanged; no dimension or scale conversion occurs.
QString unit(const QString& source);
QString unitLatex(const QString& source);
QString unitHtml(const QString& source,qreal pointSize=10.5);
QPair<QString,QString> splitLabelUnit(const QString& label);
QString latexHtml(const QString& source,qreal pointSize=10.5);
// Readable explanatory equations, separate from compact control/axis notation.
QString displayHtml(const QString& text,qreal pointSize=14);
QJsonArray symbols(const QString& text);
bool contains(const QString& text);
QString plain(const QString& text);
QString html(const QString& text,qreal pointSize=10.5);
QString htmlDocument(const QString& source,qreal pointSize=10.5);
QSizeF size(const QString& text,const QFont& font,qreal width=-1);
void draw(QPainter& painter,const QRectF& rect,int flags,const QString& text);
void drawBaseline(QPainter& painter,const QPointF& baseline,const QString& text);
QJsonArray catalog();
QJsonObject statistics();
}
