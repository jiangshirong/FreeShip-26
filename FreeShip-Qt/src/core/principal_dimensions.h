// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs {
inline QString principalLengthInput(const QString& method){
    return method=="hollenbach"?"HolLpp":method=="oortmerssen"?"OortLpp":method=="acceleration"?"Edit2":QString{};
}
// Optional design particulars, always in metres. Absence means unknown.
inline QJsonObject validatePrincipalDimensions(const QJsonValue& value){
    if(!value.isObject())throw std::invalid_argument("工程主尺度必须为对象。");
    const auto p=value.toObject();
    for(auto i=p.begin();i!=p.end();++i){
        if(i.key()!="aft_perpendicular_m"&&i.key()!="forward_perpendicular_m"&&
           i.key()!="length_between_perpendiculars_m"&&i.key()!="moulded_depth_m")
            throw std::invalid_argument("未知的工程主尺度字段。");
        if(!i.value().isDouble()||!std::isfinite(i.value().toDouble())||std::abs(i.value().toDouble())>1e9)
            throw std::invalid_argument("工程主尺度超出范围。");
    }
    const bool aft=p.contains("aft_perpendicular_m"),fore=p.contains("forward_perpendicular_m"),length=p.contains("length_between_perpendiculars_m");
    if(aft!=fore||aft!=length)throw std::invalid_argument("请同时设置艏艉垂线位置与垂线间长。");
    if(aft){
        const double d=p["forward_perpendicular_m"].toDouble()-p["aft_perpendicular_m"].toDouble();
        if(d<=0||d>1e9)throw std::invalid_argument("艏垂线 X 必须大于艉垂线 X。");
        if(std::abs(p["length_between_perpendiculars_m"].toDouble()-d)>1e-10*std::max(1.,d))
            throw std::invalid_argument("垂线间长必须等于艏艉垂线位置之差。");
    }
    if(p.contains("moulded_depth_m")&&p["moulded_depth_m"].toDouble()<=0)
        throw std::invalid_argument("型深必须大于零。");
    return p;
}
}
