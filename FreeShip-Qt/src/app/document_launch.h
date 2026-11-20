// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <functional>
#include <memory>

// One receiver per installation, user profile and graphical session.
class DocumentLaunch {
public:
    enum Result { Primary, Forwarded, Failed };
    Result start(const QStringList& paths);
    void ready(std::function<void(const QStringList&)> open);
    QString error;
private:
    std::unique_ptr<QLockFile> owner_;
    QLocalServer server_;
    QList<QStringList> pending_;
    std::function<void(const QStringList&)> open_;
};
