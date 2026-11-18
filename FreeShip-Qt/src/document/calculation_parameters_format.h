// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <QtEndian>
#include <stdexcept>
#include "core/principal_dimensions.h"

namespace fs::document::calculation_parameters_format {
inline QByteArray magic(){return QByteArray("FS26CALCPARAMS\0",14);}
inline constexpr qsizetype MaximumPayload=1024*1024;
inline bool validValue(const QString& calculator,const QString& key,const QJsonValue& value){
    const auto finite=[](const QJsonValue& number){
        return number.isDouble()&&std::isfinite(number.toDouble())&&std::abs(number.toDouble())<=1e15;
    };
    if(value.isBool()||finite(value))return true;
    if(calculator!="hydro_cross"||(key!="angles"&&key!="displacements"))return false;
    if(value.isString())return value.toString().size()<=8192;
    const auto list=value.toArray();
    return value.isArray()&&list.size()<=181&&std::all_of(list.cbegin(),list.cend(),finite);
}
inline QJsonObject validate(const QByteArray& bytes){
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    const auto root=doc.object();
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||root["format"]!="FreeShip26CalculationParameters"||root["version"]!=1||!root["calculators"].isObject())
        throw std::invalid_argument("Invalid calculation-parameter extension");
    const auto calculators=root["calculators"].toObject();if(calculators.size()>64)throw std::invalid_argument("Too many calculation input sets");
    if(root.contains("principal_dimensions"))fs::validatePrincipalDimensions(root["principal_dimensions"]);
    for(auto it=calculators.begin();it!=calculators.end();++it){
        const auto set=it.value().toObject();const auto values=set["values"].toObject();
        if(it.key().isEmpty()||it.key().size()>80||!it.value().isObject()||
           (set["units"]!="metric"&&set["units"]!="imperial")||!set["values"].isObject()||values.isEmpty()||values.size()>512)
            throw std::invalid_argument("Invalid calculation input set");
        for(auto v=values.begin();v!=values.end();++v)if(v.key().isEmpty()||v.key().size()>80||
            !validValue(it.key(),v.key(),v.value()))
            throw std::invalid_argument("Invalid calculation input value");
    }
    return root;
}
inline qsizetype size(const QByteArray& bytes,qsizetype at){
    const auto mark=magic();if(bytes.mid(at,mark.size())!=mark)return 0;
    if(bytes.size()-at<mark.size()+4+32)throw std::invalid_argument("Truncated calculation-parameter extension");
    const auto length=qFromLittleEndian<quint32>(bytes.constData()+at+mark.size());
    const auto total=mark.size()+4+qsizetype(length)+32;
    if(length>MaximumPayload||total>bytes.size()-at)throw std::invalid_argument("Oversized or truncated calculation-parameter extension");
    const auto payload=bytes.mid(at+mark.size()+4,length);
    if(QCryptographicHash::hash(payload,QCryptographicHash::Sha256)!=bytes.mid(at+mark.size()+4+length,32))
        throw std::invalid_argument("Calculation-parameter extension checksum mismatch");
    validate(payload);return total;
}
inline QJsonObject decode(const QByteArray& bytes){
    if(bytes.isEmpty())return {{"format","FreeShip26CalculationParameters"},{"version",1},{"calculators",QJsonObject{}}};
    if(size(bytes,0)!=bytes.size())throw std::invalid_argument("Invalid calculation-parameter extension size");
    const auto length=qFromLittleEndian<quint32>(bytes.constData()+magic().size());
    return validate(bytes.mid(magic().size()+4,length));
}
inline QByteArray encode(const QJsonObject& object){
    const auto payload=QJsonDocument(object).toJson(QJsonDocument::Compact);validate(payload);
    if(payload.size()>MaximumPayload)throw std::invalid_argument("Calculation parameters exceed 1 MiB");
    QByteArray length(4,0);qToLittleEndian<quint32>(quint32(payload.size()),length.data());
    return magic()+length+payload+QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
}
}
