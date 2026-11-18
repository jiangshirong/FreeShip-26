// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonObject>
namespace fs::document {
inline QJsonArray hollenbachFields(){
    QJsonArray fields;
    auto choice=[&](const char* key,const char* label,const QJsonArray& values){fields.append(QJsonObject{{"key",key},{"label",label},{"group","计算条件"},{"kind","choice"},
        {"choices",values},{"value",0},{"enabled",true},{"visible",true}});};
    choice("HolSolver","求解方式",{"Hollenbach 1999（原生）","旧外部程序（对照）"});
    choice("HolCondition","船况",{"单桨／设计吃水","单桨／压载吃水","双桨／设计吃水"});
    choice("HolBulb","艏部形式（湿表面积估计）",{"有球鼻艏","无球鼻艏"});
    auto add=[&](const char* key,const char* label,const char* unit,double low,double high,double value,const char* group,int decimals=8){
        fields.append(QJsonObject{{"key",key},{"label",label},{"unit",unit},{"group",group},{"kind","number"},
            {"minimum",low},{"maximum",high},{"value",value},{"decimals",decimals},{"double_precision",true},{"enabled",true},{"visible",true}});};
    add("HolLpp","垂线间长 LPP","m",0,10000,0,"原生方法条件");
    add("HolAppendageArea","另加附体湿表面积","m²",0,1e6,0,"原生方法条件");
    add("HolCorrelation","给定附加摩擦系数 ΔCf","",0,.01,0,"原生方法条件");
    add("HolRudders","舵数量","",1,2,1,"原生附体数量",0);
    add("HolBrackets","轴架数量","",0,2,0,"原生附体数量",0);
    add("HolBossings","轴包套数量","",0,2,0,"原生附体数量",0);
    add("HolThrusters","侧推器数量","",0,4,0,"原生附体数量",0);
    add("HolWake","给定伴流比 w","",0,.99,.1,"推进衔接（用户给定）");
    add("HolDeduction","给定推力减额 t","",0,.99,.1,"推进衔接（用户给定）");
    add("HolEtaR","给定相对旋转效率 ηR","",.01,2,1,"推进衔接（用户给定）");
    return fields;
}
}
