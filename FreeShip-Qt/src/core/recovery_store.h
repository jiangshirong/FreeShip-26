// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <memory>
class QLockFile;

namespace fs {
struct RecoveryEntry {
    QString id, source, savedUtc, error;
    quint64 revision=0;
    qint64 bytes=0;
};
// One atomic container per document, not a second document authority. The lease
// prevents another application instance from recovering a live editor's file.
class RecoveryLease {
public:
    RecoveryLease(const QString& root,const QString& id);
    ~RecoveryLease();
    void checkpoint(const QByteArray& fbm,quint64 revision,const QString& source);
    QByteArray read(RecoveryEntry* entry=nullptr)const;
    void remove();
    QString path()const;
private:
    QString root_,id_;
    std::unique_ptr<QLockFile> lock_;
};
QString defaultRecoveryRoot();
QVector<RecoveryEntry> recoveryEntries(const QString& root);
}
