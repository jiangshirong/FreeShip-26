// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

namespace fs {
struct ModelFileVersion {
    QString path;
    bool exists=false;
    qint64 bytes=0;
    QByteArray sha256;
};
QString modelFilePath(const QString& path);
QString modelBackupPath(const QString& target);
ModelFileVersion observeModelFile(const QString& path);
bool sameModelFile(const QString& a,const QString& b);
bool sameModelVersion(const ModelFileVersion& a,const ModelFileVersion& b);
struct ModelSaveResult {ModelFileVersion version;QString backup;bool changed=false;};

// Prepare both files before touching either destination. The expected version
// comes from the opened file, or the destination selected for Save As. Separate
// construction/commit also permits deterministic filesystem fault tests.
class ModelFileSave {
public:
    ModelFileSave(const QString& input,const QString& target,const ModelFileVersion& expected,
                  const ModelFileVersion& preparedInput = {});
    ~ModelFileSave();
    ModelSaveResult commit();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
