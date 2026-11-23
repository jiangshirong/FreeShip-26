// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

namespace fs26::macos_update {
inline QString containingApp(const QString &executableDir) {
    const QDir bin(executableDir);
    const QString app = QDir::cleanPath(bin.absoluteFilePath("../.."));
    if (bin.dirName() != "MacOS" || QFileInfo(bin.absoluteFilePath("..")).fileName() != "Contents" ||
        !app.endsWith(".app") || !QFileInfo(app + "/Contents/Info.plist").isFile()) return {};
    return QFileInfo(app).canonicalFilePath();
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
