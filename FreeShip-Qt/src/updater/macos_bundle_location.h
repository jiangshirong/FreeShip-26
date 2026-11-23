// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <sys/statvfs.h>
#include <unistd.h>

namespace fs26::macos_update {
inline QString containingApp(const QString &executableDir) {
    if (executableDir.isEmpty()) return {};
    QDir directory(executableDir);
    if (directory.dirName() != "MacOS" || !directory.cdUp() || directory.dirName() != "Contents" ||
        !directory.cdUp() || !directory.dirName().endsWith(".app") ||
        !QFileInfo(directory.filePath("Contents/Info.plist")).isFile()) return {};
    return directory.canonicalPath();
}

inline constexpr char installLocationError[] = "请先将 FreeShip 26 复制到可写的「应用程序」目录，再安装更新。";

inline bool writableInstall(const QString &app) {
    const QFileInfo bundle(app);
    if (app.isEmpty() || !bundle.isDir() || bundle.isSymLink() ||
        app.startsWith("/Volumes/") || app.contains("/AppTranslocation/")) return false;
    // APFS firmlinks (e.g. /Users and /Applications) resolve to the writable
    // Data volume. Ask the OS about these paths directly, not the apparent
    // mount-table prefix, which may be the read-only system root.
    const auto path = QFile::encodeName(app);
    const auto parent = QFile::encodeName(bundle.absolutePath());
    struct statvfs volume{};
    return statvfs(path.constData(), &volume) == 0 && !(volume.f_flag & ST_RDONLY) &&
        access(path.constData(), W_OK | X_OK) == 0 && access(parent.constData(), W_OK | X_OK) == 0;
}
}
