// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QLocalServer>
#include <QLockFile>
#ifdef Q_OS_LINUX
#include <QFile>
#endif
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
#ifdef Q_OS_LINUX
    // A PID is not a process identity across separate Flatpak namespaces.
    QFile sandboxLock_;
#endif
    QLocalServer server_;
    QList<QStringList> pending_;
    std::function<void(const QStringList&)> open_;
};
