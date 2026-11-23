// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

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
    const QFileInfo parent(bundle.absolutePath());
    const QStorageInfo volume(app);
    return !app.isEmpty() && bundle.isDir() && !bundle.isSymLink() && bundle.isWritable() && parent.isWritable() &&
        !app.startsWith("/Volumes/") && !app.contains("/AppTranslocation/") && volume.isValid() && !volume.isReadOnly();
}
}
