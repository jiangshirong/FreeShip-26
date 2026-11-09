// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_exchange.h"
namespace fs::cad {
QString title(const QString& format);
QString extension(const QString& format);
bool acceptsExtension(const QString& format,const QString& extension);
bool supports(const QString& format);
// IGES patches originate from a disposable document at this exact revision.
// Other formats use immutable complete model data, never viewport buffers.
void write(const QString& path,const QString& format,const ModelSnapshot& model,
           const exchange::Options& options,const QJsonObject& patches={});
}
