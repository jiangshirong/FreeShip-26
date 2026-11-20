// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fbm_codec.h"
#include "core/loading_cases.h"
#include "calculation_parameters_format.h"
#include "geometry_input.h"
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs::document {
inline const QSet<QString>& calculationParameterMethods(){
    static const QSet<QString> names{"delft","holtrop","hollenbach","oortmerssen","fung","ost","rbhs","mh","kaper","planing","propeller1","propeller2","propeller3","propeller4","propeller5","wind","maneuver","acceleration","addmass","hydro_single","hydro_table","hydro_cross"};return names;
}
inline QString calculationParameterUnits(const Fbm& source){return documentGeometrySettings(source).imperial?"imperial":"metric";}
inline bool shouldFillCalculationInputs(const QJsonObject& request,const QByteArray& stored={}){
    return request["method"]=="performance_defaults"&&request["auto_extract"].toBool()
        &&!request["extract"].toBool()&&request["values"].toObject().isEmpty()
        &&std::none_of(stored.cbegin(),stored.cend(),[](char byte){return byte!=0;});
}
template<class Run>
QJsonObject fillCalculationInputs(const QJsonObject& request,const QByteArray& stored,Run run){
    if(!shouldFillCalculationInputs(request,stored))return run(request);
    auto fill=request;fill["extract"]=true;
    try{return run(fill);}
    catch(const std::invalid_argument&){}
    catch(const std::runtime_error&){}
    auto defaults=run(request);
    defaults["auto_extract_unavailable"]=true;
    return defaults;
}
inline QJsonObject restoreCalculationRequest(const Fbm& source,QJsonObject request){
    if(request.value("development_sample")==true)return request;
    const auto method=request["calculator"].toString();const auto entry=source.calculationParameters()["calculators"].toObject()[method].toObject();
    if(request.contains("values")&&!request["values"].isObject())throw std::invalid_argument("Calculation parameters must be an object");
    const auto patch=request.value("values").toObject();
    if(!entry.isEmpty()&&entry["units"]==calculationParameterUnits(source)){
        auto values=entry["values"].toObject();
        for(auto it=patch.begin();it!=patch.end();++it)values[it.key()]=it.value();request["values"]=values;
    }
    if(method=="hydro_cross"){
        const auto root=source.calculationParameters();
        auto values=request["values"].toObject();
        if(root.contains("loading_book")){
            const auto book=root["loading_book"].toObject();fs::loading::validateSavedBook(book);
            values["loading_book"]=book;
            if(!values.contains("use_loading_book"))values["use_loading_book"]=entry["values"].toObject()["use_loading_book"].toBool();
        }else if(root.contains("stability_gz_criteria")){
            const auto criteria=root["stability_gz_criteria"];
            if(!criteria.isArray())throw std::invalid_argument("Invalid GZ criteria");
            fs::loading::validateCriteria(criteria.toArray());
            values["gz_criteria"]=criteria;
        }
        if(root.contains("stability_loading_parameters")&&entry["units"]==calculationParameterUnits(source)&&
            !values.contains("loading_parameters"))
            values["loading_parameters"]=root["stability_loading_parameters"];
        if(!values.isEmpty())request["values"]=values;
    }
    // Defaults use the shared design data in preference to an old calculation
    // snapshot, while explicit edits in this request retain their precedence.
    // Apply after geometry extraction, so auto-extract remains available.
    request.remove("project_values");
    if(request["method"]=="performance_defaults"){
        const auto p=source.calculationParameters()["principal_dimensions"].toObject();
        const QString key=fs::principalLengthInput(method);
        if(!key.isEmpty()&&p.contains("length_between_perpendiculars_m")&&!patch.contains(key))
            request["project_values"]=QJsonObject{{key,p["length_between_perpendiculars_m"]}};
    }
    return request;
}
inline QJsonObject completeCalculationEntry(const Fbm& source,const QJsonObject& request){
    const auto method=request["calculator"].toString();
    if(!calculationParameterMethods().contains(method)||!request["values"].isObject())throw std::invalid_argument("Invalid complete calculation snapshot");
    const auto values=request["values"].toObject();if(values.isEmpty()||values.size()>512)throw std::invalid_argument("Invalid complete parameter count");
    for(auto it=values.begin();it!=values.end();++it){
        if(it.key().isEmpty()||it.key().size()>80||it.key().contains(QChar(0))||
           !calculation_parameters_format::validValue(method,it.key(),it.value()))
            throw std::invalid_argument("Invalid calculation parameter value");
    }
    const auto units=calculationParameterUnits(source);
    if(request.contains("snapshot_units")&&request["snapshot_units"]!=units)throw std::invalid_argument("Calculation snapshot unit mismatch");
    return {{"units",units},{"values",values}};
}
inline QJsonObject extractStabilityParameters(QJsonObject& root,const QString& method,QJsonObject values){
    if(method=="hydro_cross"){
        if(values.contains("loading_book")){
            auto book=values.take("loading_book").toObject();fs::loading::validateSavedBook(book);root["loading_book"]=book;
            root.remove("stability_gz_criteria");
        }
        if(values.contains("gz_criteria")){
            const auto criteria=values.take("gz_criteria");
            if(!criteria.isArray())throw std::invalid_argument("Invalid GZ criteria");
            fs::loading::validateCriteria(criteria.toArray());
            if(root.contains("loading_book")){
                auto book=root["loading_book"].toObject();book["criteria"]=criteria;root["loading_book"]=book;
            }else if(criteria.toArray().isEmpty())root.remove("stability_gz_criteria");
            else root["stability_gz_criteria"]=criteria;
        }
        if(values.contains("loading_parameters"))root["stability_loading_parameters"]=values.take("loading_parameters");
    }
    return values;
}
inline Fbm storeCompleteCalculationParameters(const Fbm& source,const QJsonObject& request){
    auto root=source.calculationParameters();auto calculators=root["calculators"].toObject();
    auto copy=request;const auto key=request["calculator"].toString();copy["values"]=extractStabilityParameters(root,key,request["values"].toObject());
    calculators[key]=completeCalculationEntry(source,copy);
    root["calculators"]=calculators;return source.withCalculationParameters(root);
}
inline Fbm storeCalculationDrafts(const Fbm& source,const QJsonObject& drafts){
    auto root=source.calculationParameters();auto calculators=root["calculators"].toObject();
    for(auto it=drafts.begin();it!=drafts.end();++it)
        calculators[it.key()]=completeCalculationEntry(source,{{"calculator",it.key()},{"values",extractStabilityParameters(root,it.key(),it.value().toObject())}});
    root["calculators"]=calculators;return source.withCalculationParameters(root);
}
}
