// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QSet>
#include <optional>
#include <stdexcept>

namespace fs {
using LayerScope = std::optional<QSet<quint64>>;

inline LayerScope readLayerScope(const QJsonValue& value,const QJsonArray& available){
    if(value.isUndefined())return std::nullopt;
    if(!value.isArray()||value.toArray().isEmpty())
        throw std::runtime_error("请先选择图层。");
    QSet<quint64> known,selected;
    for(const auto id:available)known.insert(quint64(id.toInteger()));
    for(const auto item:value.toArray()){
        const auto id=item.toInteger(-1);
        if(id<=0||!known.contains(quint64(id))||selected.contains(quint64(id)))
            throw std::runtime_error("所选图层已更改，请重新选择。");
        selected.insert(quint64(id));
    }
    return selected;
}

template<class Geometry>
void setHydroLayerFlags(Geometry& geometry,const QJsonArray& ids,const LayerScope& scope){
    if(!scope)return;
    if(size_t(ids.size())!=geometry.layers.size())
        throw std::runtime_error("所选图层已更改，请重新选择。");
    for(qsizetype i=0;i<ids.size();++i)
        geometry.layers[size_t(i)].useInHydrostatics=scope->contains(quint64(ids.at(i).toInteger()));
}
}
