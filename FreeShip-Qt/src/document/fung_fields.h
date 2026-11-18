// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonObject>
namespace fs::document {
inline QJsonArray fungFields(){
    QJsonArray fields{QJsonObject{{"key","FungSolver"},{"label","求解方式"},{"group","计算条件"},{"kind","choice"},
        {"choices",QJsonArray{"Fung–Leibman 1995（勘误版，原生）","旧外部程序（对照）"}},{"value",0},{"enabled",true},{"visible",true}}};
    auto add=[&](const char* key,const char* label,double low,double high,double value,const char* group){
        fields.append(QJsonObject{{"key",key},{"label",label},{"unit",""},{"group",group},{"kind","number"},
            {"minimum",low},{"maximum",high},{"value",value},{"decimals",8},{"double_precision",true},{"enabled",true},{"visible",true}});};
    add("FungCwp","水线面系数 CWP（0 表示未提供）",0,1,0,"适用性检查");
    add("FungTransomDraftRatio","尾封板浸深／吃水（-1 表示未提供）",-1,1,-1,"适用性检查");
    add("FungCorrelation","给定附加摩擦系数 ΔCf",0,.01,0,"原生方法条件");
    add("FungWake","给定伴流比 w",0,.99,.1,"推进衔接（用户给定）");
    add("FungDeduction","给定推力减额 t",0,.99,.1,"推进衔接（用户给定）");
    add("FungEtaR","给定相对旋转效率 ηR",.01,2,1,"推进衔接（用户给定）");
    return fields;
}
}
