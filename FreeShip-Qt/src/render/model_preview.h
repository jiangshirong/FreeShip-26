// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include <QByteArray>
#include <memory>
// Four fitted views, using the production renderer and current immutable mesh.
// Does not capture the desktop or modify any of the user's camera settings.
QByteArray modelPreview(std::shared_ptr<const fs::ModelSnapshot> model,const QJsonObject& colors={},QJsonObject* timings=nullptr);
