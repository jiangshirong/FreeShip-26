// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <array>
#include <optional>
#include <utility>

namespace fs {
// The native worker and the document snapshot carry the same two display
// arrays. Retain the exact decoded artifact for one document transaction.
class SnapshotSurface {
public:
    static SnapshotSurface parse(QByteArray bytes);
    const QByteArray& bytes() const {return bytes_;}
    const QJsonObject& object() const {return object_;}

    // Inline the worker's already encoded arrays when both envelopes match.
    QByteArray encodeModel(const QJsonObject& model) const;
    // Reuse decoded arrays only when their actual on-disk bytes are identical.
    std::optional<QJsonObject> decodeModel(const QByteArray& bytes) const;
private:
    using Span=std::pair<qsizetype,qsizetype>;
    using Spans=std::array<Span,2>;
    static std::optional<Spans> displayArrays(QByteArrayView bytes);
    static QByteArray splice(QByteArrayView document,const Spans& fields,const std::array<QByteArrayView,2>& values);
    QByteArray bytes_;
    QJsonObject object_;
    std::optional<Spans> arrays_;
};
}
