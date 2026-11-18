// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>

namespace fs::developmentCalculation {
inline QJsonObject sample(const QString& method){
    if(method=="delft")return {{"StartSpeedBox",2},{"EndSpeedBox",7},{"StepSpeedBox",.5},
        {"LwlBox",10},{"BwlBox",3},{"DraftBox",.6},{"DraftTotalBox",1.6},
        {"WettedSurfacebox",22},{"WlAreabox",18},{"DisplacementBox",8},{"LCBBox",-3},{"CpBox",.55},
        {"DensityBox",1.025},{"ViscosityBox",1},{"EstimateBox",false}};
    if(method=="kaper")return {{"Edit2",5},{"Edit3",.6},{"Edit4",.2},{"Edit5",3.2},{"Edit6",.55},
        {"Edit7",.2},{"Edit8",8},{"Edit9",.5},{"Edit10",0}};
    if(method=="planing")return {{"Edit2",20},{"Edit3",4},{"Edit4",.9},{"Edit5",70},{"Edit6",.7},
        {"Edit7",40},{"Edit8",15},{"Edit9",8},{"Edit10",3},{"Edit11",.4},{"Edit12",35},{"ComboBox",0}};
    if(method=="hollenbach")return {{"HolSolver",0},{"HolCondition",0},{"HolBulb",0},{"HolLpp",120},
        {"LwlBox",123},{"Losbox",126},{"BwlBox",20},{"DraftTotalBox",6.5},{"DraftBox",0},
        {"DisplacementBox",10608},{"DpBox",4.5},{"WettedSurfacebox",2800},{"DensityBox",1.025},
        {"ViscosityBox",1.189},{"HolRudders",1},{"HolWake",.2},{"HolDeduction",.15},{"HolEtaR",1},
        {"KeBox",1},{"EstimateBox",false},{"Estimate2Box",false},
        {"StartSpeedBox",11},{"EndSpeedBox",16},{"StepSpeedBox",18}};
    if(method=="fung")return {{"FungSolver",0},{"LwlBox",100},{"BwlBox",14},{"DraftTotalBox",4.4},
        {"DraftBox",0},{"DisplacementBox",3138.034},{"CpBox",.633},{"LCBBox",-2.5},{"WettedSurfacebox",1700},
        {"KBulbBox",12.9},{"BABox",6.545},{"ZBulbBox",9},{"DensityBox",1.025},{"ViscosityBox",1.189},
        {"FungCwp",.76},{"FungTransomDraftRatio",.2},{"FungWake",.2},{"FungDeduction",.15},{"FungEtaR",1},
        {"NpBox",2},{"DpBox",3},{"KeBox",1},{"EstimateBox",false},{"Estimate2Box",false},
        {"StartSpeedBox",10},{"EndSpeedBox",20},{"StepSpeedBox",35}};
    if(method=="oortmerssen")return {{"OortSolver",0},{"ComboBox",0},{"OortLpp",27.5},{"LwlBox",30.5},
        {"BwlBox",29/3.45},{"DraftTotalBox",(29/3.45)/2.85},{"DraftBox",0},{"DisplacementBox",376},
        {"WettedSurfacebox",284},{"CpBox",.609*29/30.5},{"LCBBox",(-.0255*29+.75)/30.5*100},
        {"KBulbBox",23},{"DensityBox",1.025},{"ViscosityBox",1.18831},{"NpBox",1},{"DpBox",2.5},
        {"OortCorrelation",.00051},{"OortWake",.13},{"OortDeduction",.09},{"OortEtaR",.97},
        {"KeBox",1},{"EstimateBox",false},{"Estimate2Box",false},{"StartSpeedBox",3},{"EndSpeedBox",13},{"StepSpeedBox",14}};
    if(method=="holtrop"||method=="ost")return {{"StartSpeedBox",5},{"EndSpeedBox",18},{"StepSpeedBox",20},
        {"LwlBox",120},{"BwlBox",18},{"DraftTotalBox",6},{"DraftBox",0},{"WettedSurfacebox",2800},
        {"WlAreabox",1800},{"DisplacementBox",8500},{"LCBBox",0},{"CpBox",.65},{"NpBox",1},{"DpBox",4},
        {"KeBox",1},{"A11Box",0},{"KsBox",150},{"DensityBox",1.025},{"ViscosityBox",1},{"NserBox",1}};
    if(method=="rbhs"||method=="mh")return {{"ComboBox",1},{"StartSpeedBox",12},{"EndSpeedBox",25},{"StepSpeedBox",28},
        {"LwlBox",30},{"BwlBox",5},{"DraftTotalBox",1.5},{"DraftBox",0},{"DisplacementBox",100},
        {"WettedSurfacebox",190},{"WlAreabox",100},{"LCBBox",0},{"CpBox",.65},{"NpBox",1},{"DpBox",1},
        {"KeBox",1},{"A11Box",0},{"DensityBox",1.025},{"ViscosityBox",1}};
    if(method=="propeller1")return {{"DesignSolver",0},{"Edit2",10},{"Edit3",30},{"Edit4",1},{"Edit5",3},
        {"Edit6",.1},{"Edit7",1025},{"Edit8",.1},{"Edit9",.1},{"Edit10",1},{"Edit11",1},{"Edit12",.98},
        {"Edit13",.98},{"Edit14",1},{"Edit15",4},{"Edit16",.6}};
    if(method=="propeller2"){
        QJsonObject values{{"DesignSolver",0},{"Edit2",1000},{"Edit3",200},{"Edit4",1},{"Edit5",1025},
            {"Edit6",.1},{"Edit7",.1},{"Edit8",1},{"Edit9",1},{"Edit10",.98},{"Edit11",.98},
            {"Edit12",1},{"Edit13",4},{"Edit14",.6},{"Edit16",12},{"DesignDiameterLimit",5}};
        const int resistance[]={25,40,60,85,115};
        for(int i=0;i<5;++i){
            values[QString("Edit17_%1").arg(i+1)]=8+2*i;
            values[QString("Edit18_%1").arg(i+1)]=resistance[i];
        }
        return values;
    }
    if(method=="propeller3")return {{"PassportSolver",0},{"Edit2",3},{"Edit3",.6},{"Edit4",1},{"Edit5",.6},
        {"Edit6",4},{"Edit7",1},{"Edit8",1},{"Edit9",200},{"Edit10",.1},{"Edit11",.1},{"Edit12",1},
        {"Edit13",1},{"Edit14",.98},{"Edit15",.98},{"Edit16",1025},{"Edit17",2000}};
    if(method=="propeller4")return {{"Edit2",15},{"Edit3",100},{"Edit8",1.2},{"Edit10",4},{"Edit11",2},{"Edit12",4}};
    if(method=="propeller5")return {{"ComboBox",0},{"Edit2",200},{"Edit3",.6},{"Edit4",1},
        {"Edit5",4},{"Edit6",3},{"Edit7",1},{"BSeriesReynoldsMode",0}};
    if(method=="acceleration")return {{"Edit2",10},{"Edit3",3},{"Edit4",.6},{"Edit5",8},{"Edit6",22},
        {"Edit7",1},{"Edit8",.3},{"Edit9",.9},{"Edit10",.55},{"Edit11",3},{"Edit12",.1},{"Edit13",.05},
        {"Edit14",5},{"Edit15",600},{"Edit16",.95},{"Edit17",.1},{"Edit18",1},{"StopAtSpeed",true},{"TargetSpeed",1}};
    return {};
}

inline QJsonObject sampleForUnits(const QString& method,bool imperial){
    auto values=sample(method);
    if(!imperial)return values;
    constexpr double foot=.3048, squareFoot=foot*foot, cubicFoot=squareFoot*foot;
    const auto convert=[&](std::initializer_list<const char*> keys,double factor){
        for(const auto* key:keys)if(values.contains(key))values[key]=values.value(key).toDouble()*factor;
    };
    if(method=="kaper"){
        convert({"Edit2","Edit3","Edit4"},1/foot);
        convert({"Edit5"},1/squareFoot);
        convert({"Edit7"},1000/(2240*.45359237));
    }else{
        convert({"LwlBox","Losbox","BwlBox","DraftBox","DraftTotalBox","ZBulbBox"},1/foot);
        convert({"WlAreabox","WettedSurfacebox","BABox","ViscosityBox"},1/squareFoot);
        convert({"DisplacementBox"},1/cubicFoot);
        convert({"DensityBox"},1000*cubicFoot/.45359237);
    }
    return values;
}

// Five points from the Delft sample above, in the original RESIST.dat format.
// This table belongs to the sample, never to the open model's resistance cache.
inline constexpr double accelerationMaximumKn=7;
inline QByteArray accelerationResistance(){
    return "2 0.049305900573730466 0.049305900573730466 0.050730293256971572 0.050730293256971572\r\n"
        "3 0.10637673950195313 0.10637673950195313 0.164174767964681 0.164174767964681\r\n"
        "4.5 0.2865438537597656 0.2865438537597656 0.66334902145385743 0.66334902145385743\r\n"
        "5.5 0.49039959716796877 0.49039959716796877 1.3875584157647027 1.3875584157647027\r\n"
        "7 1.2140705566406249 1.2140705566406249 4.3720029711914057 4.3720029711914057\r\n";
}
}
