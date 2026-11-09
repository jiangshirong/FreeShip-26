// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fbm_codec.h"
#include "core/model_snapshot.h"
namespace fs::document {
QJsonObject addedMass(const Fbm&,const QJsonObject& request,const QString& output,const QJsonObject& bindings,const ModelSnapshot* model);
// Protocol and empirical calculations are separate from process/UI ownership.
QJsonObject parseAddedMass(const QByteArray& bytes,const QJsonObject& values);
QJsonObject addedMassEmpirical(int objectType,double L,double B,double T,double cb,const QJsonObject& values);
}
