// GPL-2.0-or-later. FBM stores text in the active preference's encoding.
#pragma once
#include <QByteArray>
#include <QString>
namespace fs::document {
QString decodeFbmText(const QByteArray &bytes, const QString &encoding);
// Reject invalid Unicode, NUL and any conversion that cannot round-trip.
QByteArray encodeFbmText(const QString &text, const QString &encoding);
}
