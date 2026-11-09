// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
struct RecentFiles {QString last;QStringList paths;};
class FileHistory {
public:
    explicit FileHistory(QString filename);
    static QString defaultFilename();
    RecentFiles read()const;
    bool remember(const QString& filename);
    bool stopRestore(const QString& failedFilename);
    QString filename()const{return filename_;}
private:
    bool change(const QString& filename,bool remember);
    QString filename_;
};
