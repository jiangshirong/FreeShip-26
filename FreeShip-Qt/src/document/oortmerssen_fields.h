// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonObject>
namespace fs::document {
inline QJsonArray oortmerssenFields(){
    QJsonArray fields{QJsonObject{{"key","OortSolver"},{"label","求解方式"},{"group","计算条件"},{"kind","choice"},
        {"choices",QJsonArray{"按方法选择（van Oortmerssen 原生）","旧外部程序（对照）"}},{"value",0},{"enabled",true},{"visible",true}}};
    auto add=[&](const char* key,const char* label,const char* unit,double low,double high,double value,const char* group){
        fields.append(QJsonObject{{"key",key},{"label",label},{"unit",unit},{"group",group},{"kind","number"},
            {"minimum",low},{"maximum",high},{"value",value},{"decimals",8},{"double_precision",true},{"enabled",true},{"visible",true}});};
    add("OortLpp","垂线间长 LPP","m",0,10000,0,"原生方法条件");
    add("OortCorrelation","附加摩擦系数 ΔCf","",0,.01,.00051,"原生方法条件");
    add("OortWake","给定伴流比 w","",0,.99,.1,"推进衔接（用户给定）");
    add("OortDeduction","给定推力减额 t","",0,.99,.1,"推进衔接（用户给定）");
    add("OortEtaR","给定相对旋转效率 ηR","",.01,2,1,"推进衔接（用户给定）");
    return fields;
}
}
