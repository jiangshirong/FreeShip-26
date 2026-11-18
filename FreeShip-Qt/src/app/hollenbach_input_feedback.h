// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/hollenbach.h"
#include "ui_language.h"
#include <QMap>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace hollenbachFeedback {
inline constexpr double infinity=std::numeric_limits<double>::infinity();
struct EditorRange {
    double minimum=-infinity,maximum=infinity;
    int decimals=3;
    bool parsed=true;
};

struct Boundary {
    double value;
    const char* rule;
    bool open=false;
};
struct InputRange {
    Boundary lower{-infinity,"输入框范围"},upper{infinity,"输入框范围"};

    void intersect(Boundary lo,Boundary hi){
        if(lo.value>lower.value||(lo.value==lower.value&&lo.open))lower=lo;
        if(hi.value<upper.value||(hi.value==upper.value&&hi.open))upper=hi;
    }
    void intersect(double lo,double hi,const char* rule){
        intersect({lo,rule},{hi,rule});
    }
};

inline double entryBound(Boundary bound,double unit,int decimals,bool lower){
    if(!std::isfinite(bound.value))return bound.value;
    const double scale=std::pow(10.,decimals);
    const double value=bound.value/unit*scale;
    const double nearest=std::round(value);
    const double roundingError=4*std::numeric_limits<double>::epsilon()*std::max(1.,std::abs(value));
    const double scaled=std::abs(value-nearest)<=roundingError?nearest:value;
    if(bound.open)
        return (lower?std::floor(scaled)+1:std::ceil(scaled)-1)/scale;
    return (lower?std::ceil(scaled):std::floor(scaled))/scale;
}
inline QString entryText(double value,int decimals){
    auto text=QString::number(value,'f',decimals);
    if(decimals>0){
        while(text.endsWith('0'))text.chop(1);
        if(text.endsWith('.'))text.chop(1);
    }
    return text;
}
inline QString rangeMessage(const InputRange& range,const EditorRange& editor,double unit,const char* units,
                            const char* fieldName,const char* sentence,int displayDecimals=3){
    // Round into the allowed interval. Use more digits only when the displayed
    // field would otherwise lose a valid, representable interval.
    int decimals=std::min(displayDecimals,editor.decimals);
    double lower=entryBound(range.lower,unit,decimals,true);
    double upper=entryBound(range.upper,unit,decimals,false);
    if(lower>upper&&range.lower.value<=range.upper.value&&decimals<editor.decimals){
        decimals=editor.decimals;
        lower=entryBound(range.lower,unit,decimals,true);
        upper=entryBound(range.upper,unit,decimals,false);
    }
    const auto lo=entryText(lower,decimals),hi=entryText(upper,decimals);
    if(lower>upper){
        if(QString(range.lower.rule)==range.upper.rule)return uiText("当前参数下无可输入的数值。");
        return QString(uiText("当前参数下，%1无可用范围。\n下限 %2 %3（%4）\n上限 %5 %3（%6）"))
            .arg(uiText(fieldName),lo,units,uiText(range.lower.rule),hi,uiText(range.upper.rule));
    }
    if(!std::isfinite(upper))return QString(uiText("当前参数要求至少 %1 %2。")).arg(lo,units);
    if(!std::isfinite(lower))return QString(uiText("当前参数要求至多 %1 %2。")).arg(hi,units);
    return QString(uiText(sentence)).arg(lo,hi,units);
}

inline void validate(const QMap<QString,double>& numbers,const QMap<QString,EditorRange>& editors,
                     QMap<QString,QString>& errors,QMap<QString,QString>& hints,QSet<QString>& pending,bool imperial){
    using Limit=fs::HollenbachGeometryLimit;
    const int condition=int(numbers.value("HolCondition"));
    if(condition<0||condition>2)return;
    const double lengthUnit=imperial?.3048:1.;
    const char* lengthSymbol=imperial?"ft":"m";
    enum Field {Lpp,Lwl,Los,Beam,Draft,Trim,Volume,Diameter,Count};
    struct FieldState {
        const char* key;
        const char* name;
        double unit;
        const char* symbol;
        double value=0;
        InputRange range{};
        bool invalid=false;
    };
    FieldState fields[Count]={{"HolLpp","垂线间长",1.,"m"},{"LwlBox","水线长",lengthUnit,lengthSymbol},
        {"Losbox","水下长度",lengthUnit,lengthSymbol},{"BwlBox","水线宽度",lengthUnit,lengthSymbol},
        {"DraftTotalBox","首尾平均吃水",lengthUnit,lengthSymbol},{"DraftBox","纵倾差",lengthUnit,lengthSymbol},
        {"DisplacementBox","排水体积",lengthUnit*lengthUnit*lengthUnit,imperial?"ft³":"m³"},
        {"DpBox","螺旋桨直径",1.,"m"}};
    const auto positive=[](double value){return std::isfinite(value)&&value>0;};
    for(int i=0;i<Count;++i){
        auto& field=fields[i];
        const auto editor=editors.value(field.key);
        field.value=editor.parsed?numbers.value(field.key)*field.unit:std::numeric_limits<double>::quiet_NaN();
        field.range.intersect(editor.minimum*field.unit,editor.maximum*field.unit,"输入框范围");
        if(i!=Trim){
            field.range.intersect({0,"正值要求",true},{infinity,"正值要求"});
            field.invalid=!positive(field.value);
        }
    }
    const auto raw=[&](Field field){return fields[field].value;};
    const fs::HollenbachInput input{fs::HollenbachCondition(condition),raw(Lpp),raw(Lwl),raw(Los),raw(Beam),
        raw(Draft),raw(Trim),raw(Volume),raw(Diameter)};
    const auto checks=fs::hollenbachGeometryChecks(input);
    const auto check=[&](Limit limit)->const fs::HollenbachGeometryCheck& {return checks[int(limit)];};
    const auto invalid=[&](std::initializer_list<Field> related){
        for(const auto field:related)fields[field].invalid=true;
    };
    const auto ratio=[&](Limit limit,Field numerator,Field denominator,const char* rule){
        const auto& bounds=check(limit);
        const auto n=raw(numerator),d=raw(denominator);
        if(positive(d))fields[numerator].range.intersect(bounds.minimum*d,bounds.maximum*d,rule);
        if(positive(n))fields[denominator].range.intersect(n/bounds.maximum,n/bounds.minimum,rule);
        if(positive(n)&&positive(d)&&!bounds.valid())invalid({numerator,denominator});
    };

    const auto& length=check(Limit::Lpp);
    fields[Lpp].range.intersect(length.minimum,length.maximum,"船况");
    if(!length.valid())invalid({Lpp});

    const auto& lengthVolume=check(Limit::LengthVolume);
    constexpr auto volumeRule="垂线间长与排水体积立方根之比";
    if(positive(input.volume)){
        const double root=std::cbrt(input.volume);
        fields[Lpp].range.intersect(lengthVolume.minimum*root,lengthVolume.maximum*root,volumeRule);
    }
    if(positive(input.lpp))
        fields[Volume].range.intersect(std::pow(input.lpp/lengthVolume.maximum,3),
                                       std::pow(input.lpp/lengthVolume.minimum,3),volumeRule);
    if(positive(input.lpp)&&positive(input.volume)&&!lengthVolume.valid())invalid({Lpp,Volume});

    const auto& block=check(Limit::BlockCoefficient);
    constexpr auto blockRule="方形系数";
    const auto blockDenominator=[&](Field field,double a,double b){
        if(positive(a)&&positive(b)&&positive(input.volume)){
            const auto factor=input.volume/(a*b);
            fields[field].range.intersect(factor/block.maximum,factor/block.minimum,blockRule);
        }
    };
    blockDenominator(Lpp,input.beam,input.draft);
    blockDenominator(Beam,input.lpp,input.draft);
    blockDenominator(Draft,input.lpp,input.beam);
    const auto blockVolume=input.lpp*input.beam*input.draft;
    if(positive(input.lpp)&&positive(input.beam)&&positive(input.draft)){
        fields[Volume].range.intersect(block.minimum*blockVolume,block.maximum*blockVolume,blockRule);
        if(positive(input.volume)&&!block.valid())invalid({Lpp,Beam,Draft,Volume});
    }

    ratio(Limit::LengthBeam,Lpp,Beam,"垂线间长与水线宽度之比");
    ratio(Limit::BeamDraft,Beam,Draft,"宽吃水比");
    ratio(Limit::SubmergedWaterline,Los,Lwl,"水下长度与水线长之比");
    ratio(Limit::WaterlineLength,Lwl,Lpp,"水线长与垂线间长之比");

    const auto halfTrim=input.trim/2;
    constexpr auto draftsRule="首尾吃水须为正";
    if(std::isfinite(halfTrim)){
        fields[Draft].range.intersect({std::abs(halfTrim),draftsRule,true},{infinity,draftsRule});
        fields[Lpp].range.intersect({input.trim,"纵倾修正",true},{infinity,"纵倾修正"});
    }
    if(positive(input.draft)){
        fields[Trim].range.intersect({-2*input.draft,draftsRule,true},{2*input.draft,draftsRule,true});
        if(std::abs(halfTrim)>=input.draft)invalid({Draft,Trim});
    }
    if(positive(input.lpp)){
        fields[Trim].range.intersect({-infinity,"纵倾修正"},{input.lpp,"纵倾修正",true});
        if(input.trim>=input.lpp)invalid({Trim,Lpp});
    }

    const auto& propeller=check(Limit::PropellerDraft);
    const auto aftDraft=input.draft-halfTrim;
    const bool knownAftDraft=positive(input.draft)&&positive(aftDraft);
    constexpr auto propellerRule="桨径与艉吃水之比";
    if(knownAftDraft){
        fields[Diameter].range.intersect(propeller.minimum*aftDraft,propeller.maximum*aftDraft,propellerRule);
        if(positive(input.diameter)&&!propeller.valid())invalid({Diameter,Draft,Trim});
    }
    if(positive(input.diameter)){
        const double minDraft=input.diameter/propeller.maximum,maxDraft=input.diameter/propeller.minimum;
        if(std::isfinite(halfTrim))fields[Draft].range.intersect(halfTrim+minDraft,halfTrim+maxDraft,propellerRule);
        if(positive(input.draft))fields[Trim].range.intersect(2*(input.draft-maxDraft),2*(input.draft-minDraft),propellerRule);
    }

    for(int i=0;i<Count;++i){
        const auto& field=fields[i];
        if(!field.invalid||errors.contains(field.key))continue;
        if(QString(field.range.lower.rule)=="正值要求"&&QString(field.range.upper.rule)=="输入框范围"){
            errors[field.key]=uiText("数值须大于 0。");
            continue;
        }
        const auto sentence=i==Diameter&&knownAftDraft?"按当前艉吃水，螺旋桨直径允许 %1–%2 %3。":
            "按当前参数，允许输入 %1–%2 %3。";
        errors[field.key]=rangeMessage(field.range,editors.value(field.key),field.unit,field.symbol,field.name,sentence);
    }

    const auto defer=[&](std::initializer_list<Field> targets,const QString& reason){
        for(auto target:targets){
            pending.insert(fields[target].key);
            hints[fields[target].key]=reason;
        }
    };
    const auto hullPending=uiText("待校验：请先填写相关船体参数。");
    if(!knownAftDraft)
        defer({Diameter},uiText("待校验：请先确定艉吃水。"));
    if(!positive(input.lwl))defer({Los},uiText("待校验：请先填写水线长。"));
    if(!(positive(input.lpp)&&positive(input.los)))defer({Lwl},hullPending);
    if(!(positive(input.lpp)&&positive(input.beam)&&positive(input.draft)&&positive(input.volume))){
        defer({Lpp,Beam,Draft,Volume},hullPending);
    }
    if(!(positive(input.draft)&&positive(input.lpp)&&positive(input.diameter)))defer({Trim},hullPending);

    const auto speedKeys={"StartSpeedBox","EndSpeedBox","StepSpeedBox"};
    if(length.valid()&&block.valid()&&positive(input.beam)&&positive(input.draft)&&positive(input.los)){
        const auto [lo,hi]=fs::hollenbachSpeedRange(input);
        for(const auto* key:speedKeys){
            const double speed=numbers.value(key);
            const auto editor=editors.value(key);
            InputRange range{{lo,"航速"},{hi,"航速"}};
            range.intersect(editor.minimum,editor.maximum,"输入框范围");
            hints[key]=rangeMessage(range,editor,1.,"kn","航速","按当前船体参数，航速允许 %1–%2 %3。");
            if((speed<lo||speed>hi)&&!errors.contains(key))errors[key]=hints[key];
        }
    }else{
        for(const auto* key:speedKeys){
            pending.insert(key);
            hints[key]=uiText("待校验：先确定垂线间长、水下长度和方形系数。");
        }
    }
}
}
