// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_exchange.h"
namespace fs::exchange {
// Source curves are retained as markers; these inputs define a fitted surface,
// not a lossless exchange of an already evaluated subdivision mesh.
Document readCurveFile(const QByteArray& bytes,const QString& format,const Options& options);
}
