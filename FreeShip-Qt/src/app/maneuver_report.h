// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "report_layout.h"
#include "ui_language.h"
#include <QHash>
#include <QRegularExpression>

namespace report {
inline QString maneuverResult(const QString& source,bool html) {
    struct Field {const char* name;const char* label;const char* unit;const char* symbol;};
    struct Section {const char* title;QList<Field> fields;};
    static const QList<Section> sections{
    {"船体与工况的派生量",{
        {"Mean Draft (m)","平均吃水","m",""},
        {"Molded Volume (m^3)","型排水体积","m³",""},
        {"Center of Gravity LCG (m from FP)","重心距艏垂线","m",""},
        {"Rudder Center of Effort XR (m from FP)","舵作用点距艏垂线","m",""},
        {"Initial Ship Speed (m/s)","初始航速","m/s",""},
    }},
    {"线性操纵性导数",{
        {"Nondimensional Mass M prime","无量纲质量","","M'"},
        {"Nondimensional Mass Moment I sub zz","无量纲艏摇惯性矩","","I_{zz}'"},
        {"Sway Velocity Derivative Y sub v","横向速度导数","","Y_v"},
        {"Sway Acceleration Derivative Y sub v dot","横向加速度导数","","Y_{\\dot v}"},
        {"Yaw Velocity Derivative N sub v","横向速度艏摇力矩导数","","N_v"},
        {"Yaw Acceleration Derivative N sub v dot","横向加速度艏摇力矩导数","","N_{\\dot v}"},
        {"Sway Velocity Derivative Y sub r","艏摇角速度横向力导数","","Y_r"},
        {"Sway Acceleration Derivative Y sub r dot","艏摇角加速度横向力导数","","Y_{\\dot r}"},
        {"Yaw Velocity Derivative N sub r","艏摇角速度导数","","N_r"},
        {"Yaw Acceleration Derivative N sub r dot","艏摇角加速度导数","","N_{\\dot r}"},
        {"Sway Rudder Derivative Y sub delta","舵角横向力导数","","Y_{\\delta}"},
        {"Yaw Rudder Derivative N sub delta","舵角艏摇力矩导数","","N_{\\delta}"},
    }},
    {"Nomoto 时间常数与增益",{
        {"Dominant Ship Time Constant T1 prime","主导时间常数","","T_1'"},
        {"Ship Time Constant T2 prime","第二时间常数","","T_2'"},
        {"Numerator Time Constant T3 prime","分子时间常数","","T_3'"},
        {"Numerator Time Constant T4 prime","分子时间常数","","T_4'"},
        {"1st Order Eqn. Time Constant T prime","一阶模型时间常数","","T'"},
        {"Rudder Gain Factor K prime","舵角增益","","K'"},
        {"Rudder Gain Factor K sub v prime","横向速度增益","","K_v'"},
        {"Steering Gear Time Constant TE prime","舵机时间常数","","T_E'"},
    }},
    {"回转能力与航向稳定性",{
        {"Inverse Time Constant 1/|T prime|","时间常数绝对值的倒数","","\\frac{1}{|T'|}"},
        {"Inverse Gain Factor 1/|K prime|","增益绝对值的倒数","","\\frac{1}{|K'|}"},
        {"Clarke\"s Turning Index P","Clarke 回转指标","","P"},
        {"Linear Dynamic Stability Criterion C","线性动稳定性判据","","C"},
        {"Closed Loop Phase Margin with Steering Engine","含舵机的闭环相位裕度","°",""},
    }},
    {"回转预估",{
        {"Approach Speed","回转前航速","kn",""},
        {"Rudder Angle","舵角","°",""},
        {"Steady Turning Diameter","稳定回转直径","m",""},
        {"Tactical Diameter","战术直径","m",""},
        {"Advance","进距","m",""},
        {"Transfer","横距","m",""},
        {"Steady Speed in Turn","回转中稳定航速","kn",""},
    }},
    {"状态变量方程",{
        {"Open Loop Dynamics Matrix Coeff. a22","开环动力学矩阵系数","","a_{22}"},
        {"Open Loop Dynamics Matrix Coeff. a23","开环动力学矩阵系数","","a_{23}"},
        {"Open Loop Dynamics Matrix Coeff. a32","开环动力学矩阵系数","","a_{32}"},
        {"Open Loop Dynamics Matrix Coeff. a33","开环动力学矩阵系数","","a_{33}"},
        {"Control Distribution Matrix Coeff. b2","舵角输入系数","","b_2"},
        {"Control Distribution Matrix Coeff. b3","舵角输入系数","","b_3"},
        {"Ext. Force Distr. Matrix Coeff. gamma 21","外力输入矩阵系数","","\\gamma_{21}"},
        {"Ext. Force Distr. Matrix Coeff. gamma 22","外力输入矩阵系数","","\\gamma_{22}"},
        {"Ext. Force Distr. Matrix Coeff. gamma 31","外力输入矩阵系数","","\\gamma_{31}"},
        {"Ext. Force Distr. Matrix Coeff. gamma 32","外力输入矩阵系数","","\\gamma_{32}"},
    }},
    };

    // The solver emits the same fixed captions with variable column padding.
    // Keep only computed fields. The original report is retained in JSON.
    static const QRegularExpression numeric(R"(^([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eEdD][+-]?\d+)?)(?:\s+(?:meters|knots|degrees))?$)");
    QHash<QString,QString> values;
    QString stability;
    for(const auto& raw:source.split('\n')){
        const auto line=raw.simplified();
        if(line=="Vessel is hydrodynamically open loop course stable")stability=uiText("稳定");
        if(line=="Vessel is hydrodynamically open loop course unstable")stability=uiText("不稳定");
        const auto equals=line.indexOf('=');
        if(equals<0)continue;
        const auto match=numeric.match(line.mid(equals+1).trimmed());
        if(match.hasMatch())values.insert(line.left(equals).trimmed(),match.captured(1));
    }

    QString result;
    for(int i=0;i<sections.size();++i){
        const auto& section=sections[i];
        Table table{uiText(section.title),{uiText("计算结果"),uiText("数值"),uiText("单位")},{},{0,2},{55,28,17},780};
        for(const auto& field:section.fields){
            const auto value=values.constFind(QString::fromLatin1(field.name));
            if(value==values.cend())continue;
            QString label=uiText(field.label);
            if(*field.symbol)label+=" $"+QString::fromLatin1(field.symbol)+"$";
            table.rows.append({label,*value,notation::unit(QString::fromUtf8(field.unit))});
        }
        if(i==3&&!stability.isEmpty())table.rows.append({uiText("开环航向稳定性"),stability,{}});
        result+=render(table,html);
    }
    return result;
}
}
