// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "calculation_parameter_ranges.h"
#include "core/fung.h"
#include "core/oortmerssen.h"
#include "ui_language.h"
#include <QMap>
#include <QStringList>
#include <cmath>

// Check input limits that the form can evaluate before starting the solver.
inline void checkPerformanceInputs(const QString& method,const QMap<QString,double>& numbers,
                                  const QMap<QString,hollenbachFeedback::EditorRange>& editors,
                                  QMap<QString,QString>& errors,QMap<QString,QString>& hints,
                                  QSet<QString>& pending,bool imperial){
    const auto value=[&](const QString& key){return numbers.value(key);};
    const auto mark=[&](const QString& key,const QString& reason){
        if(numbers.contains(key)&&!errors.contains(key))errors[key]=reason;
    };
    const auto markTogether=[&](const QStringList& keys,const QString& reason){
        for(const auto& key:keys){
            if(!numbers.contains(key))continue;
            auto& description=errors[key];
            if(description.isEmpty())description=reason;
            else if(!description.contains(reason))description+=QLatin1Char('\n')+reason;
        }
    };
    const auto number=[](double v){return QString::number(v,'g',8);};
    const auto visibleValue=[&](const QString& key,double v,const QString& unit){
        const auto text=hollenbachFeedback::entryText(v,editors.value(key).decimals);
        return unit.isEmpty()?text:text+" "+unit;
    };
    const auto entryLimit=[&](const QString& key,double value,const QString& unit,bool lower,bool open=false){
        const auto boundary=hollenbachFeedback::entryBound({value,"范围",open},1.,editors.value(key).decimals,lower);
        return visibleValue(key,boundary,unit);
    };
    const auto ordered=[&](const QString& lowerKey,const QString& upperKey,const QString& lowerName,
                           const QString& upperName,const QString& unit,bool strict){
        if(!editors.value(lowerKey).parsed||!editors.value(upperKey).parsed)return;
        const double lower=value(lowerKey),upper=value(upperKey);
        if(strict?lower<upper:lower<=upper)return;
        markTogether({lowerKey},QString(uiText(strict?"须小于当前%1（%2）。":"须不大于当前%1（%2）。"))
            .arg(upperName,visibleValue(upperKey,upper,unit)));
        markTogether({upperKey},QString(uiText(strict?"须大于当前%1（%2）。":"须不小于当前%1（%2）。"))
            .arg(lowerName,visibleValue(lowerKey,lower,unit)));
    };
    const auto usable=[&](const QString& key){
        return editors.value(key).parsed&&std::isfinite(value(key))&&value(key)>0;
    };
    using calculationRanges::infinity;
    using calculationRanges::InputRange;
    calculationRanges::Ranges fieldRanges(editors);
    const char* lengthUnit=imperial?"ft":"m";
    const char* areaUnit=imperial?"ft²":"m²";
    const char* volumeUnit=imperial?"ft³":"m³";
    const auto allow=[&](const QString& key,const char* unit,double lo,double hi,const char* rule,
                         bool invalid,bool openLo=false,bool openHi=false){
        fieldRanges.constrain(key,unit,{InputRange{{lo,rule,openLo},{hi,rule,openHi}}},invalid);
    };
    struct Factor {const char* key; const char* unit; double power;};
    const auto product=[&](std::initializer_list<Factor> factors,double scale,double lo,double hi,
                           const char* rule,bool invalid,bool openLo=false,bool openHi=false){
        for(const auto& input:factors){
            double fixed=scale;
            for(const auto& other:factors){
                if(&other==&input)continue;
                if(!usable(other.key)){fixed=0;break;}
                fixed*=std::pow(value(other.key),other.power);
            }
            if(!(fixed>0&&std::isfinite(fixed)))continue;
            const auto invert=[&](double bound){return std::pow(bound/fixed,1./input.power);};
            if(input.power>0)
                allow(input.key,input.unit,invert(lo),invert(hi),rule,invalid,openLo,openHi);
            else
                allow(input.key,input.unit,invert(hi),invert(lo),rule,invalid,openHi,openLo);
        }
    };
    const auto defer=[&](const QStringList& keys,const QString& reason){
        for(const auto& key:keys)if(numbers.contains(key)){
            pending.insert(key);
            hints[key]=reason;
        }
    };
    const QStringList speedFields{"StartSpeedBox","EndSpeedBox","StepSpeedBox"};
    const auto speedRule=[](const QString& key){
        if(key=="StartSpeedBox")return "起始速度限制";
        if(key=="EndSpeedBox")return "设计航速限制";
        return "最大航速限制";
    };
    const auto hullPending=uiText("待校验：请先填写相关船体参数。");
    const auto speedHint=[&](const QString& key,double low,double high){
        const auto editor=editors.value(key);
        hollenbachFeedback::InputRange allowed{{low,"航速"},{high,"航速"}};
        allowed.intersect(editor.minimum,editor.maximum,"输入框范围");
        hints[key]=hollenbachFeedback::rangeMessage(allowed,editor,1.,"kn","航速",
            "按当前船体参数，航速允许 %1–%2 %3。");
    };
    const auto minimum=[&](const QStringList& keys,const QString& quantity,double actual,double limit,bool inclusive=true){
        if(std::isfinite(actual)&&(inclusive?actual>=limit:actual>limit))return;
        markTogether(keys,QString(uiText(inclusive?"%1 为 %2，须不小于 %3。":"%1 为 %2，须大于 %3。"))
            .arg(quantity,number(actual),number(limit)));
    };
    const auto maximum=[&](const QStringList& keys,const QString& quantity,double actual,double limit,bool inclusive=true){
        if(std::isfinite(actual)&&(inclusive?actual<=limit:actual<limit))return;
        markTogether(keys,QString(uiText(inclusive?"%1 为 %2，须不大于 %3。":"%1 为 %2，须小于 %3。"))
            .arg(quantity,number(actual),number(limit)));
    };
    const auto positive=[&](const QStringList& keys){
        for(const auto& key:keys)if(value(key)<=0)mark(key,uiText("数值须大于 0。"));
    };
    const auto edit=[](int n){return "Edit"+QString::number(n);};
    const auto positiveEdits=[&](int from,int to){
        for(int n=from;n<=to;++n)positive({edit(n)});
    };
    const auto range=[&](const QString& key,double lower,double upper){
        if(value(key)<lower||value(key)>upper)
            mark(key,QString(uiText("允许范围为 %1 至 %2。")).arg(lower).arg(upper));
    };
    const auto start=value("StartSpeedBox"),end=value("EndSpeedBox"),step=value("StepSpeedBox");
    if(method=="delft"){
        if(start<0)mark("StartSpeedBox",uiText("起始速度不能小于 0。"));
        if(end<=start){
            if(end>0)ordered("StartSpeedBox","EndSpeedBox",uiText("起始速度"),uiText("终止速度"),"kn",true);
            else mark("EndSpeedBox",QString(uiText("须大于当前%1（%2）。")).arg(uiText("起始速度"),visibleValue("StartSpeedBox",start,"kn")));
        }
        if(step<=0)mark("StepSpeedBox",uiText("步长须大于 0。"));
        else if(end>start&&(end-start)/step>2000){
            markTogether({"StartSpeedBox"},QString(uiText("按当前终止速度和步长，须不低于 %1 kn。")).arg(entryLimit("StartSpeedBox",end-2000*step,{},true)));
            markTogether({"EndSpeedBox"},QString(uiText("按当前起始速度和步长，须不高于 %1 kn。")).arg(entryLimit("EndSpeedBox",start+2000*step,{},false)));
            markTogether({"StepSpeedBox"},QString(uiText("按当前起止速度，须不小于 %1 kn。")).arg(entryLimit("StepSpeedBox",(end-start)/2000,{},true)));
        }
        positive({"DensityBox","ViscosityBox","LwlBox","BwlBox","DraftTotalBox","WlAreabox","DisplacementBox","CpBox"});
        if(!value("EstimateBox"))positive({"WettedSurfacebox"});
        if(value("DraftBox")<=.02)mark("DraftBox",uiText("船体吃水须大于 0.02（当前单位）。"));
        ordered("DraftBox","DraftTotalBox",uiText("船体吃水"),uiText("总吃水"),imperial?"ft":"m",false);
        if(value("CpBox")>double(.85f))mark("CpBox",uiText("棱形系数不能大于 0.85。"));
        const auto length=value("LwlBox"),beam=value("BwlBox");
        if(!(usable("LwlBox")&&usable("BwlBox")&&usable("DraftBox")&&usable("DisplacementBox")&&
            usable("CpBox")&&editors.value("LCBBox").parsed))defer({"StartSpeedBox","EndSpeedBox"},hullPending);
        else{
            const float lengthBeam=float(length)/float(beam),beamDraft=float(beam)/float(value("DraftBox"));
            const float lengthVolume=length/std::cbrt(value("DisplacementBox"));
            const bool yacht=lengthBeam>=2.76&&lengthBeam<=5&&beamDraft>=2.46&&beamDraft<=19.32&&
                lengthVolume>=4.34&&lengthVolume<=8.5&&value("LCBBox")>=-6&&value("LCBBox")<=0&&
                value("CpBox")>=.52&&value("CpBox")<=.60;
            const double scale=std::sqrt(9.81*length*(imperial?.3048:1.))*3600/1852;
            for(const auto* key:{"StartSpeedBox","EndSpeedBox"}){
                if(!yacht) speedHint(key,0,.55*scale);
                else{
                    const int decimals=editors.value(key).decimals;
                    const auto bound=[&](double f,bool lower,bool open=false){
                        return hollenbachFeedback::entryText(
                            hollenbachFeedback::entryBound({f*scale,"航速",open},1.,decimals,lower),decimals);
                    };
                    hints[key]=QString(uiText("按当前船体参数，曲线航速范围为 0–%1 或 %2–%3 kn。"))
                        .arg(bound(.45,false),bound(.475,true,true),bound(.75,false));
                }
            }
        }
        if(usable("LwlBox")&&usable("BwlBox"))
            product({{"WlAreabox",areaUnit,1},{"LwlBox",lengthUnit,-1},{"BwlBox",lengthUnit,-1}},
                1,0,double(.85f),"水线面系数",value("WlAreabox")/(length*beam)>double(.85f),true);
        else defer({"WlAreabox"},uiText("待校验：请先填写水线长和水线宽度。"));
        if(!usable("DraftBox"))defer({"DraftTotalBox"},hullPending);
    }else if(numbers.contains("StartSpeedBox")){
        const bool nativeHollenbach=method=="hollenbach"&&value("HolSolver")==0;
        const bool nativeFung=method=="fung"&&value("FungSolver")==0;
        const bool nativeOort=method=="oortmerssen"&&value("OortSolver")==0&&value("ComboBox")==0;
        const bool legacyHollenbach=method=="hollenbach"&&!nativeHollenbach;
        const bool legacyFung=method=="fung"&&!nativeFung;
        const bool legacyOort=method=="oortmerssen"&&!nativeOort;
        positive({"StartSpeedBox","DensityBox","ViscosityBox"});
        if(!nativeHollenbach)positive({"LwlBox","BwlBox","DraftTotalBox","DisplacementBox"});
        if(legacyHollenbach||legacyFung||legacyOort){
            const float cp=value("CpBox");
            if(fs::legacyUnitCoefficientInvalid(cp,false))
                maximum({"CpBox"},"Cp",cp,1,false);
            minimum({"DraftTotalBox"},QString(uiText("吃水"))+" (m)",float(value("DraftTotalBox")),.02,false);
        }
        if(legacyHollenbach||legacyFung)positive({legacyHollenbach?"Losbox":"LoSbox"});
        if(legacyHollenbach||legacyOort)positive({"NpBox","DpBox"});
        if(legacyOort){
            const float length=value("LwlBox");
            const auto lengthLabel=QString(uiText("水线长"))+" (m)";
            minimum({"LwlBox"},lengthLabel,length,4,false);
            maximum({"LwlBox"},lengthLabel,length,90);
            minimum({"StartSpeedBox"},QString(uiText("起始速度"))+" (kn)",float(start),.5);
        }
        // The built-in Hollenbach branch derives Cb from the displacement.
        if(nativeHollenbach)hollenbachFeedback::validate(numbers,editors,errors,hints,pending,imperial);
        else positive({"CpBox"});
        if(editors.value("StartSpeedBox").parsed&&editors.value("EndSpeedBox").parsed&&start>=end){
            markTogether({"StartSpeedBox"},QString(uiText("须低于当前设计航速 %1 kn。")).arg(number(end)));
            markTogether({"EndSpeedBox"},QString(uiText("须高于当前起始速度 %1 kn。")).arg(number(start)));
        }
        if(editors.value("EndSpeedBox").parsed&&editors.value("StepSpeedBox").parsed&&end>=step){
            markTogether({"EndSpeedBox"},QString(uiText("须低于当前最大航速 %1 kn。")).arg(number(step)));
            markTogether({"StepSpeedBox"},QString(uiText("须高于当前设计航速 %1 kn。")).arg(number(end)));
        }
        if(!nativeHollenbach){
            for(const auto& key:speedFields)allow(key,"kn",0,infinity,"正值要求",false,true);
            if(usable("StartSpeedBox"))allow("EndSpeedBox","kn",start,infinity,"起始速度",false,true);
            if(usable("EndSpeedBox")){
                allow("StartSpeedBox","kn",0,end,"设计航速",false,false,true);
                allow("StepSpeedBox","kn",end,infinity,"设计航速",false,true);
            }
            if(usable("StepSpeedBox"))allow("EndSpeedBox","kn",0,step,"最大航速",false,false,true);
        }
        if(method=="holtrop"){
            positive({"WlAreabox"});
            const float lwl=value("LwlBox"),beam=value("BwlBox"),cp=value("CpBox"),lcb=value("LCBBox");
            if(usable("LwlBox")&&usable("BwlBox")){
                const float cwp=fs::legacyWaterplaneCoefficient(value("WlAreabox"),lwl,beam);
                product({{"WlAreabox",areaUnit,1},{"LwlBox",lengthUnit,-1},{"BwlBox",lengthUnit,-1}},
                    1,0,1,"水线面系数",fs::legacyUnitCoefficientInvalid(cwp,true),true,true);
            }else defer({"WlAreabox"},uiText("待校验：请先填写水线长和水线宽度。"));
            maximum({"CpBox"},"Cp",cp,.9);
            maximum({"LCBBox"},"LCB (%)",lcb,6.1);
            if(beam>0&&lwl>0)
                product({{"LwlBox",lengthUnit,1},{"BwlBox",lengthUnit,-1}},1,2,infinity,
                    "长宽比",!(lwl/beam>2),true);
            if(lwl>0&&cp>0&&cp<=.9&&lcb<=6.1&&editors.value("LCBBox").parsed){
                constexpr auto rule="去流段长度";
                const double denominator=4.*cp-1, target=.2/double(lwl);
                const double slope=.06*cp/denominator, factor=1.-cp+slope*lcb;
                const bool invalid=!std::isfinite(factor)||lwl*factor<.2;
                if(std::isfinite(factor)&&factor>0)allow("LwlBox",lengthUnit,.2/factor,infinity,rule,invalid);
                else fieldRanges.unavailable("LwlBox",lengthUnit,"当前棱形系数和浮心位置",rule,invalid);
                if(std::isfinite(slope)){
                    const double limit=(target-(1.-cp))/slope;
                    allow("LCBBox","%",slope>0?limit:-infinity,slope>0?infinity:limit,rule,invalid);
                }else fieldRanges.unavailable("LCBBox","%","当前棱形系数",rule,invalid);
                allow("LCBBox","%",-infinity,6.1,"Holtrop 方法适用范围",false);
                allow("CpBox","",0,.9,"Holtrop 方法适用范围",false,true);

                // The sign of the denominator changes at Cp = 0.25.
                const double linear=5-4*target+.06*lcb, discriminant=linear*linear+16*(target-1);
                QList<double> breaks{0,.25,.9};
                if(discriminant>=0){
                    for(double root:{(linear-std::sqrt(discriminant))/8,(linear+std::sqrt(discriminant))/8})
                        if(root>0&&root<.9)breaks.append(root);
                }
                std::sort(breaks.begin(),breaks.end());
                QList<InputRange> choices;
                for(int i=1;i<breaks.size();++i){
                    const double lo=breaks[i-1],hi=breaks[i],mid=(lo+hi)/2;
                    if(lo<hi&&(-4*mid*mid+linear*mid+target-1)/(4*mid-1)>=0)
                        choices.append(InputRange{{lo,rule,lo==0||lo==.25},{hi,rule,hi==.25}});
                }
                if(choices.isEmpty())fieldRanges.unavailable("CpBox","","Holtrop 方法适用范围",rule,invalid);
                else fieldRanges.constrain("CpBox","",choices,invalid);
            }
            if(!usable("LwlBox")||!usable("CpBox")||cp>.9)defer({"LCBBox"},hullPending);
        }
        if(method=="ost"||method=="rbhs"||method=="mh"){
            const float lwl=value("LwlBox"),beam=value("BwlBox"),draft=value("DraftTotalBox");
            const float displacement=value("DisplacementBox"),cp=value("CpBox");
            const int series=method=="ost"?int(value("NserBox")):
                fs::legacyResistanceSeries(method=="mh",int(value("ComboBox")),int(value("NserBox")));
            if(lwl>0&&beam>0&&draft>0&&displacement>0){
                const float cb=displacement/lwl/beam/draft;
                const std::initializer_list<Factor> dimensions{{"DisplacementBox",volumeUnit,1},
                    {"LwlBox",lengthUnit,-1},{"BwlBox",lengthUnit,-1},{"DraftTotalBox",lengthUnit,-1}};
                product(dimensions,1,0,.95,"方形系数",cb>.95);
                if((series==14||series==15)&&cp>0){
                    float cm=cp/displacement*(lwl*beam*draft);
                    if(cm>=1)cm=(displacement/(lwl*cp))/(beam*draft);
                    const bool invalid=!fs::ResistanceDomainValue{float(cp*cm),.2,.9}.valid();
                    constexpr auto rule="当前系列的方形系数";
                    QList<QPair<double,double>> blockIntervals;
                    if(cp>=.2)blockIntervals.append({.2,std::min(.9,double(cp))});
                    const double square=double(cp)*cp;
                    const double lower=std::max(double(cp),square/.9),upper=square/.2;
                    if(lower<=upper)blockIntervals.append({lower,upper});
                    for(const auto& input:dimensions){
                        double fixed=1;
                        for(const auto& other:dimensions)
                            if(&input!=&other)fixed*=std::pow(value(other.key),other.power);
                        QList<InputRange> choices;
                        for(const auto& interval:blockIntervals){
                            const auto a=std::pow(interval.first/fixed,1./input.power);
                            const auto b=std::pow(interval.second/fixed,1./input.power);
                            choices.append(InputRange{{std::min(a,b),rule},{std::max(a,b),rule}});
                        }
                        if(choices.isEmpty())fieldRanges.unavailable(input.key,input.unit,"当前棱形系数",rule,invalid);
                        else fieldRanges.constrain(input.key,input.unit,choices,invalid);
                    }
                    if(cb>=.2)allow("CpBox","",std::sqrt(.2*cb),cb>.9?std::sqrt(.9*cb):infinity,rule,invalid);
                    else fieldRanges.unavailable("CpBox","","按船体尺度计算的方形系数",rule,invalid);
                }
            }
            const auto seriesSpeeds=[&](double dimension,const QString& field,double low,double high,bool hullForm){
                const double scale=std::sqrt(9.81*dimension)/.51444;
                for(const auto& key:speedFields){
                    const auto speed=value(key);
                    const bool invalid=!fs::ResistanceDomainValue{speed,low*scale,high*scale}.valid();
                    constexpr auto rule="当前系列的航速范围";
                    speedHint(key,low*scale,high*scale);
                    allow(key,"kn",low*scale,high*scale,rule,invalid);
                    if(speed>0){
                        const bool useVolume=field=="DisplacementBox";
                        const double exponent=useVolume?1/.3333:1.;
                        const double factor=std::pow(speed*.51444,2)/9.81;
                        allow(field,useVolume?volumeUnit:lengthUnit,std::pow(factor/(high*high),exponent),
                            std::pow(factor/(low*low),exponent),speedRule(key),invalid);
                    }
                    if(invalid){
                        QStringList choices{"NserBox","ComboBox"};
                        if(hullForm)choices.append("NfBox");
                        markTogether(choices,uiText("当前航速超出所选系列的适用范围。"));
                    }
                }
            };
            const bool roundBilgeSeries=(method!="mh"&&(series==5||(series>=8&&series<=14)))||(method=="mh"&&series==14);
            if(roundBilgeSeries){
                if(usable("DisplacementBox"))seriesSpeeds(std::pow(displacement,.3333),"DisplacementBox",series==11?.7:.5,2.8,false);
                else defer(speedFields,uiText("待校验：请先填写排水体积。"));
            }
            const auto hullForm=value("NfBox");
            if(method=="ost"&&(series==4||(series==17&&(hullForm==1||hullForm==2)))){
                if(!usable("LwlBox"))defer(speedFields,uiText("待校验：请先填写水线长。"));
                else if(series==4)seriesSpeeds(lwl,"LwlBox",.10,.23,false);
                else if(hullForm==1)seriesSpeeds(lwl,"LwlBox",.10,.50,true);
                else seriesSpeeds(lwl,"LwlBox",.21,.37,true);
            }
            if((series==14||series==15)&&!(usable("LwlBox")&&usable("BwlBox")&&
                usable("DraftTotalBox")&&usable("DisplacementBox")))defer({"CpBox"},hullPending);
            if(series==0){
                for(int i=1;i<=5;++i){
                    const auto key=QString("Edit17_%1").arg(i);
                    positive({key,QString("Edit18_%1").arg(i)});
                    if(i>1){
                        const auto previous=QString("Edit17_%1").arg(i-1);
                        ordered(previous,key,QString("Fr %1").arg(i-1),QString("Fr %1").arg(i),{},true);
                    }
                }
            }
        }
        if(nativeFung||nativeHollenbach||nativeOort){
            if(value("KeBox")<1)mark("KeBox",uiText("手动航海余量系数须不小于 1。"));
            if(value("Estimate2Box"))mark("Estimate2Box",uiText("当前方法仅计算静水阻力，请关闭自动海况修正。"));
            if(nativeFung||!value("EstimateBox"))positive({"WettedSurfacebox"});
            if((nativeFung||nativeOort)&&value("DraftBox")!=0)
                mark("DraftBox",uiText("当前方法要求纵倾差为 0。"));
            if(nativeHollenbach&&value("EstimateBox")&&value("HolAppendageArea")!=0)
                mark("HolAppendageArea",uiText("估计面积已包含附体，请将此项设为 0。"));
            const double unit=imperial?.3048:1,area=unit*unit,volume=area*unit;
            const double lwl=value("LwlBox")*unit,beam=value("BwlBox")*unit,draft=value("DraftTotalBox")*unit;
            const double displacement=value("DisplacementBox")*volume,cp=value("CpBox");
            const double viscosity=value("ViscosityBox")*area*1e-6;
            const auto checkSpeedDomain=[&](double length,double lpp,double lowFn,double highFn){
                const double scale=std::sqrt(9.80665*length)*3600/1852;
                const double reynoldsMinimum=1e5*viscosity/length*3600/1852;
                const auto toWaterline=[&](double bound){return (lpp>0?2*bound-lpp:bound)/unit;};
                for(const auto& key:speedFields){
                    speedHint(key,std::max(lowFn*scale,reynoldsMinimum),highFn*scale);
                    const double speed=value(key);
                    const double fn=speed/scale;
                    const bool invalidFn=fn<lowFn-1e-12||fn>highFn+1e-12;
                    const auto speedSI=speed*1852/3600;
                    const auto lengthAt=[&](double f){return std::pow(speedSI/f,2)/9.80665;};
                    allow(key,"kn",lowFn*scale,highFn*scale,"弗劳德数",invalidFn);
                    if(speed>0)allow("LwlBox",lengthUnit,toWaterline(lengthAt(highFn)),
                        toWaterline(lowFn>0?lengthAt(lowFn):infinity),speedRule(key),invalidFn);
                    if(viscosity>0){
                        const bool invalidRe=speed<reynoldsMinimum;
                        allow(key,"kn",reynoldsMinimum,infinity,"雷诺数",invalidRe);
                        if(speed>0){
                            allow("LwlBox",lengthUnit,toWaterline(1e5*viscosity/speedSI),infinity,"雷诺数",invalidRe);
                            allow("ViscosityBox",imperial?"10⁻⁶ ft²/s":"10⁻⁶ m²/s",0,
                                speedSI*length/(1e5*area*1e-6),"雷诺数",invalidRe,true);
                        }
                    }
                }
            };
            if(nativeFung){
                if(value("EstimateBox"))mark("EstimateBox",uiText("请填写或从船模提取湿表面积。"));
                range("NpBox",1,2);positive({"DpBox"});
                range("CpBox",.526,.774);
                range("KBulbBox",2.600,31.730);
                if(value("FungCwp")!=0)range("FungCwp",.662,.841);
                if(value("FungTransomDraftRatio")!=-1)range("FungTransomDraftRatio",0,.770);
                if(lwl>0&&beam>0&&draft>0&&displacement>0&&cp>0){
                    const fs::FungInput input{lwl,beam,draft,displacement,cp,value("WettedSurfacebox")*area,value("KBulbBox"),
                        value("BABox")*area,value("ZBulbBox")*unit,0,viscosity,0,value("LCBBox"),0,0};
                    const auto limits=fs::fungDomain(input);
                    const auto fit=[&](int index,std::initializer_list<Factor> factors,double scale,const char* rule){
                        const auto& limit=limits[index];
                        product(factors,scale,limit.minimum,limit.maximum,rule,!limit.valid());
                    };
                    fit(0,{{"LwlBox",lengthUnit,1},{"BwlBox",lengthUnit,-1}},1,"长宽比");
                    fit(1,{{"BwlBox",lengthUnit,1},{"DraftTotalBox",lengthUnit,-1}},1,"宽吃水比");
                    allow("LCBBox","%",(.5-limits[2].maximum)*100,(.5-limits[2].minimum)*100,
                        "浮心位置",!limits[2].valid());
                    allow("CpBox","",.526,.774,"Fung 方法适用范围",false);
                    fit(3,{{"DisplacementBox",volumeUnit,1},{"LwlBox",lengthUnit,-1},{"BwlBox",lengthUnit,-1},
                        {"DraftTotalBox",lengthUnit,-1},{"CpBox","",-1}},1,"中横剖面系数");
                    fit(4,{{"DisplacementBox",volumeUnit,1},{"LwlBox",lengthUnit,-3}},1e6/35,"排水长度比");
                    fit(5,{{"WettedSurfacebox",areaUnit,1},{"DisplacementBox",volumeUnit,-.5},{"LwlBox",lengthUnit,-.5}},
                        std::sqrt(35.),"湿表面积系数");
                    fit(6,{{"BABox",areaUnit,1},{"DisplacementBox",volumeUnit,-1},{"LwlBox",lengthUnit,1},{"CpBox","",1}},
                        1,"浸水艉封板面积与中横剖面面积之比");
                    fit(7,{{"ZBulbBox",lengthUnit,1},{"BwlBox",lengthUnit,-1}},1,"艉封板宽度与水线宽度之比");
                    if(input.transomArea>0&&input.transomBeam<=0){
                        markTogether({"BABox"},uiText("当前艉封板宽度为 0，浸水艉封板面积须为 0。"));
                        markTogether({"ZBulbBox"},uiText("有浸水艉封板面积时，艉封板宽度须大于 0。"));
                    }
                }
                if(usable("LwlBox")&&usable("ViscosityBox"))checkSpeedDomain(lwl,0,.15,.9);
                else defer(speedFields,uiText("待校验：请先填写水线长和运动黏度。"));
                if(!usable("BwlBox"))defer({"ZBulbBox"},hullPending);
                if(!(usable("LwlBox")&&usable("BwlBox")&&usable("DraftTotalBox")&&
                    usable("DisplacementBox")&&usable("CpBox")))defer({"LCBBox","WettedSurfacebox","BABox"},hullPending);
            }
            if(nativeOort){
                positive({"OortLpp"});
                range("DpBox",0,25);
                const double lpp=value("OortLpp");
                using namespace hollenbachFeedback;
                const auto lppEditor=editors.value("OortLpp");
                InputRange allowedLpp;
                allowedLpp.intersect(lppEditor.minimum,lppEditor.maximum,"输入框范围");
                if(usable("LwlBox"))
                    allowedLpp.intersect({0,"正值要求",true},{lwl,"水线长"});
                bool invalidLpp=lpp>lwl&&usable("LwlBox");
                if(lwl>0&&lpp>0&&beam>0&&draft>0&&displacement>0&&cp>0){
                    if(lpp>lwl){
                        markTogether({"LwlBox"},QString(uiText("须不小于当前%1（%2）。"))
                            .arg(uiText("垂线间长"),visibleValue("OortLpp",lpp,"m")));
                    }
                    if(lpp<=lwl){
                        const fs::OortmerssenInput input{lwl,lpp,beam,draft,displacement,cp,value("LCBBox"),value("KBulbBox")};
                        const auto limits=fs::oortmerssenDomain(input);
                        const double length=fs::oortmerssenLength(input);
                        const double lcb=value("LCBBox");
                        const auto waterline=[&](double lo,double hi,const char* rule,bool invalid){
                            allow("LwlBox",lengthUnit,lo/unit,hi/unit,rule,invalid);
                        };
                        const auto taper=[&](double lo,double hi,double offset,double slope,const char* rule,bool invalid){
                            // With Lpp fixed, each converted coefficient is offset + slope * Lwl/(Lwl+Lpp).
                            if(slope!=0){
                                double a=(lo-offset)/slope,b=(hi-offset)/slope;
                                if(a>b)std::swap(a,b);
                                if(a<1&&b>0){
                                    waterline(a>0?lpp*a/(1-a):0,b<1?lpp*b/(1-b):infinity,rule,invalid);
                                    return;
                                }
                            }else if(offset>=lo&&offset<=hi)return;
                            fieldRanges.unavailable("LwlBox",lengthUnit,
                                offset==0?"当前棱形系数":"当前浮心位置",rule,invalid);
                        };
                        waterline(lpp,infinity,"垂线间长",false);
                        waterline(2*limits[0].minimum*beam-lpp,2*limits[0].maximum*beam-lpp,
                            "排水长度与船宽之比",!limits[0].valid());
                        allow("BwlBox",lengthUnit,length/(limits[0].maximum*unit),length/(limits[0].minimum*unit),
                            "排水长度与船宽之比",!limits[0].valid());
                        product({{"BwlBox",lengthUnit,1},{"DraftTotalBox",lengthUnit,-1}},1,
                            limits[1].minimum,limits[1].maximum,"宽吃水比",!limits[1].valid());
                        allow("CpBox","",limits[2].minimum*length/lwl,limits[2].maximum*length/lwl,
                            "换算后的棱形系数",!limits[2].valid());
                        taper(limits[2].minimum,limits[2].maximum,0,2*cp,"换算后的棱形系数",!limits[2].valid());
                        product({{"DisplacementBox",volumeUnit,1},{"CpBox","",-1},{"LwlBox",lengthUnit,-1},
                            {"BwlBox",lengthUnit,-1},{"DraftTotalBox",lengthUnit,-1}},1,
                            limits[3].minimum,limits[3].maximum,"中横剖面系数",!limits[3].valid());
                        if(editors.value("LCBBox").parsed){
                            const auto toLCB=[&](double x){return 50+(x-50)*length/lwl;};
                            allow("LCBBox","%",toLCB(limits[4].minimum),toLCB(limits[4].maximum),
                                "换算后的浮心位置",!limits[4].valid());
                            taper(limits[4].minimum,limits[4].maximum,50,2*(lcb-50),
                                "换算后的浮心位置",!limits[4].valid());
                        }
                        allow("KBulbBox","°",limits[5].minimum,limits[5].maximum,"水线半进流角",!limits[5].valid());
                    }
                }
                const bool lengthInputsReady=usable("LwlBox")&&usable("BwlBox")&&usable("CpBox")&&editors.value("LCBBox").parsed;
                if(lengthInputsReady){
                    const double lcb=value("LCBBox");
                    const auto limits=fs::oortmerssenDomain({lwl,lpp,beam,1.,1.,cp,lcb,0.});
                    const auto fromLength=[&](double lo,double hi,const char* rule){
                        allowedLpp.intersect(2*lo-lwl,2*hi-lwl,rule);
                    };
                    // L_D=(L_WL+L_pp)/2. Invert each L_D-dependent domain check.
                    fromLength(limits[0].minimum*beam,limits[0].maximum*beam,"排水长度与船宽之比");
                    fromLength(cp*lwl/limits[2].maximum,cp*lwl/limits[2].minimum,"换算后的棱形系数");
                    const double offset=(lcb-50)*lwl;
                    fromLength(offset/(limits[4].minimum-50),offset/(limits[4].maximum-50),"换算后的浮心位置");
                    if(lpp>0&&lpp<=lwl)
                        invalidLpp|=!limits[0].valid()||!limits[2].valid()||!limits[4].valid();
                }
                const bool speedInputsReady=usable("LwlBox")&&usable("OortLpp")&&usable("ViscosityBox")&&lpp<=lwl;
                const double length=fs::oortmerssenLength({lwl,lpp});
                if(usable("LwlBox")&&usable("ViscosityBox")){
                    for(const auto& key:speedFields)if(usable(key)){
                        const double speed=value(key)*1852/3600;
                        const double fnLength=std::pow(speed/.5,2)/9.80665,reynoldsLength=1e5*viscosity/speed;
                        allowedLpp.intersect(2*fnLength-lwl,infinity,"弗劳德数");
                        allowedLpp.intersect(2*reynoldsLength-lwl,infinity,"雷诺数");
                        if(speedInputsReady)
                            invalidLpp|=speed/std::sqrt(9.80665*length)>.5+1e-12||length<reynoldsLength;
                    }
                }
                if(speedInputsReady)checkSpeedDomain(length,lpp,0,.5);
                else defer(speedFields,uiText("待校验：请先确定水线长、垂线间长和运动黏度。"));
                if(lengthInputsReady||invalidLpp){
                    const auto hint=rangeMessage(allowedLpp,lppEditor,1.,"m","垂线间长","按当前参数，垂线间长允许 %1–%2 %3。");
                    if(invalidLpp&&lppEditor.parsed&&lpp>0)markTogether({"OortLpp"},hint);
                    else hints["OortLpp"]=hint;
                }else if(!errors.contains("OortLpp"))defer({"OortLpp"},hullPending);
                if(!(usable("LwlBox")&&usable("OortLpp")&&usable("BwlBox")&&usable("DraftTotalBox")&&
                    usable("DisplacementBox")))defer({"CpBox","LCBBox"},hullPending);
            }
        }
    }else if(method=="kaper"){
        positiveEdits(2,7);positive({"Edit9"});
    }else if(method=="planing"){
        positive({"Edit2","Edit3","Edit4","Edit7","Edit9"});
        range("Edit6",.4,.9);
        if(value("ComboBox")==9)mark("ComboBox",uiText("所选方法没有计算实现。"));
        const double beam=value("Edit3")*(imperial?.3048:1.),methodIndex=value("ComboBox");
        if(usable("Edit3")&&methodIndex>=0&&methodIndex<2){
            const double scale=std::sqrt(9.80665*beam)/.51444;
            const double speed=value("Edit12");
            speedHint("Edit12",std::nextafter(1.7*scale,std::numeric_limits<double>::infinity()),120*scale);
            const bool invalid=!(speed>1.7*scale)||speed>120*scale;
            allow("Edit12","kn",1.7*scale,120*scale,"弗劳德数",invalid,true);
            if(speed>0){
                const auto factor=std::pow(speed*.51444,2)/9.80665;
                allow("Edit3",lengthUnit,factor/(120*120)/(imperial?.3048:1.),
                    factor/(1.7*1.7)/(imperial?.3048:1.),"弗劳德数",invalid,false,true);
            }
            if(invalid)
                markTogether({"ComboBox"},uiText("当前船宽和最大航速的组合超出所选求解方法的范围。"));
        }
        if(!usable("Edit3")){
            const auto needsBeam=uiText("待校验：请先填写船宽。");
            if(methodIndex>=0&&methodIndex<2)defer({"Edit12"},needsBeam);
            if(methodIndex==0)defer({"Edit9"},needsBeam);
            if(methodIndex==5)defer({"Edit7"},needsBeam);
        }
        const auto planingChoice=[&](bool invalid){
            if(invalid)markTogether({"ComboBox"},uiText("当前输入超出所选方法的适用范围。"));
        };
        if(usable("Edit3")&&methodIndex==0){
            const bool invalid=value("Edit9")/value("Edit3")>6;
            product({{"Edit9",lengthUnit,1},{"Edit3",lengthUnit,-1}},1,0,6,"所选方法的浮心位置限制",invalid);
            allow("Edit9",lengthUnit,0,infinity,"正值要求",false,true);
            planingChoice(invalid);
        }
        if(methodIndex==2&&value("Edit14")==0){
            allow("Edit14",lengthUnit,0,infinity,"所选方法的推力线位置限制",true,true);
            planingChoice(true);
        }
        if(methodIndex==4){
            const bool invalid=!fs::ResistanceDomainValue{value("Edit7")*9.81,5,400}.valid();
            allow("Edit7","t",5/9.81,400/9.81,"所选方法的排水量限制",invalid);
            planingChoice(invalid);
        }
        if(methodIndex==5&&beam>0){
            const bool invalid=value("Edit7")/(beam*beam*beam)>1;
            product({{"Edit7","t",1},{"Edit3",lengthUnit,-3}},1/std::pow(imperial?.3048:1.,3),
                0,1,"所选方法的排水量限制",invalid);
            planingChoice(invalid);
        }
        if(methodIndex==8){
            const bool invalid=!fs::ResistanceDomainValue{value("Edit8"),13,37.4}.valid();
            allow("Edit8","°",13,37.4,"所选方法的斜升角限制",invalid);
            planingChoice(invalid);
        }
    }else if(method=="wind"){
        positiveEdits(2,3);
        if(usable("Edit2"))ordered("Edit8","Edit2",uiText("形心位置"),uiText("船舶总长"),"m",false);
        else defer({"Edit8"},uiText("待校验：请先填写船舶总长。"));
    }else if(method=="maneuver"){
        for(int n:{2,3,4,5,6,8,9,10,11,12,13,14})positive({edit(n)});
        const bool twin=value("Edit17")>1;
        const double length=value("Edit2"),beam=value("Edit3");
        const double minLength=twin?70:50,maxLength=twin?230:330;
        allow("Edit2","m",minLength,maxLength,"当前推进布置的水线长范围",length<minLength||length>maxLength);
        range("Edit6",twin?.4:.5,twin?.7:.9);
        range("Edit15",0,.041);
        range("Edit16",twin?-.1:-.109,twin?.001:.041);
        range("Edit20",0,twin?35:45);
        if(value("CheckBox4"))positive({"Edit20"});
        if(usable("Edit2")){
            if(usable("Edit4")&&usable("Edit5")){
                const double forward=value("Edit4"),aft=value("Edit5"),delta=forward-aft;
                const double lo=twin?-.01:0,hi=twin?.03:.05;
                const bool invalid=!fs::ResistanceDomainValue{delta/length,lo,hi}.valid();
                constexpr auto rule="艏艉吃水差与水线长之比";
                allow("Edit4","m",aft+lo*length,aft+hi*length,rule,invalid);
                allow("Edit5","m",forward-hi*length,forward-lo*length,rule,invalid);
                for(const auto* key:{"Edit4","Edit5"})allow(key,"m",0,infinity,"正值要求",false,true);
                if(delta>=0)allow("Edit2","m",delta/hi,infinity,rule,invalid);
                else if(lo<0)allow("Edit2","m",delta/lo,infinity,rule,invalid);
                else fieldRanges.unavailable("Edit2","m","当前艏吃水小于艉吃水","单桨船要求艏吃水不小于艉吃水",invalid);
            }
            else defer({"Edit4","Edit5"},hullPending);
            if(usable("Edit3")){
                const double upper=twin?17:10;
                product({{"Edit2","m",1},{"Edit3","m",-1}},1,5,upper,"长宽比",
                    !fs::ResistanceDomainValue{length/beam,5,upper}.valid());
            }
            const double scale=std::sqrt(9.81*length)/.51444;
            const double low=twin?.06:.04,high=twin?.7:.35;
            speedHint("Edit9",low*scale,high*scale);
            product({{"Edit9","kn",1},{"Edit2","m",-.5}},.51444/std::sqrt(9.81),low,high,"弗劳德数",
                !fs::ResistanceDomainValue{value("Edit9"),low*scale,high*scale}.valid());
        }else defer({"Edit3","Edit4","Edit5","Edit9"},uiText("待校验：请先填写水线长。"));
    }else if(method=="acceleration"){
        if(value("Edit18")==1&&value("StopAtSpeed")!=0)positive({"TargetSpeed"});
        positiveEdits(2,11);positiveEdits(14,18);
    }else if(method=="addmass"){
        for(const QString& key:QStringList{"stations","waterlines"})
            if(int(value(key))%2!=1)mark(key,uiText("须为奇数。"));
        if(usable("draft"))ordered("draft","depth",uiText("吃水"),uiText("水深"),"m",false);
        else defer({"depth"},uiText("待校验：请先填写吃水。"));
    }else if(method=="propeller1"||method=="propeller2"){
        const bool first=method=="propeller1";
        const auto diagram=value(edit(first?14:12));
        const bool native=value("DesignSolver")==0&&diagram>=1&&diagram<=4;
        if(native){
            if(first){positiveEdits(2,7);positiveEdits(10,13);}
            else{positiveEdits(2,5);positiveEdits(8,11);positiveEdits(15,16);}
            if(value(edit(first?11:9))!=1)mark(edit(first?11:9),uiText("内置方法要求 i2 为 1。"));
            ordered("DesignPitchLow","DesignPitchHigh",uiText("螺距比下限"),uiText("螺距比上限"),{},false);
        }else{
            positiveEdits(2,first?13:16);
        }
        if(first&&value("Edit6")>0){
            const double ratio=native?value("Edit5")/value("Edit6"):float(value("Edit5"))/float(value("Edit6"));
            const double threshold=native?4:5;
            product({{"Edit5","m",1},{"Edit6","m",-1}},1,threshold,infinity,"直径搜索步数",!(ratio>threshold),true);
            allow("Edit6","m",0,infinity,"正值要求",false,true);
        }else if(first)defer({"Edit5"},uiText("待校验：请先填写直径变化步长。"));
        if(first&&!native){
            maximum({"Edit2"},QString(uiText("航速"))+" (kn)",float(value("Edit2")),30,false);
            if(value("Edit9")>=1)mark("Edit9",uiText("推力减额分数须小于 1。"));
            const double area=value("Edit16");
            if(diagram==1)allow("Edit15","",2,6,"所选图谱的桨叶数范围",value("Edit15")<2||value("Edit15")>6);
            if(diagram>=1&&diagram<=3){
                const double lower=diagram==1?.3:diagram==2?.4:.449,upper=diagram==2?1:1.05;
                allow("Edit16","",lower,upper,"所选图谱的展开叶面积比范围",
                    !fs::ResistanceDomainValue{area,lower,upper}.valid());
            }
        }
        if(!first){
            if(!native){
                maximum({"Edit2"},uiText("主机功率（kW）"),float(value("Edit2")),100000,false);
                maximum({"Edit16"},uiText("目标航速（kn）"),float(value("Edit16")),50,false);
            }
            for(int i=1;i<=5;++i){
                const auto speed=QString("Edit17_%1").arg(i),resistance=QString("Edit18_%1").arg(i);
                positive({speed,resistance});
                if(i==1)continue;
                const auto previousSpeed=QString("Edit17_%1").arg(i-1),previousResistance=QString("Edit18_%1").arg(i-1);
                const auto pointSpeed=[&](int row){return QString(uiText("第 %1 个工况的航速")).arg(row);};
                ordered(previousSpeed,speed,pointSpeed(i-1),pointSpeed(i),"kn",true);
                const auto resistanceName=[&](int row){return QString(uiText("第 %1 个工况的阻力")).arg(row);};
                if(native)ordered(previousResistance,resistance,resistanceName(i-1),resistanceName(i),"kN",false);
            }
            const double firstSpeed=value("Edit17_1"),lastSpeed=value("Edit17_5");
            if(usable("Edit17_1")&&usable("Edit17_5")&&lastSpeed>firstSpeed){
                ordered("Edit17_1","Edit16",uiText("首个阻力工况的航速"),uiText("目标航速"),"kn",false);
                ordered("Edit16","Edit17_5",uiText("目标航速"),uiText("末个阻力工况的航速"),"kn",false);
            }
            else defer({"Edit16"},uiText("待校验：请先确定阻力工况的航速范围。"));
            if(!native&&usable("Edit17_1")&&usable("Edit17_5")&&lastSpeed>firstSpeed&&
                usable("Edit18_1")&&usable("Edit18_5")){
                const float lo=firstSpeed*value("Edit18_1")*.51444/2.,hi=lastSpeed*value("Edit18_5")*.51444*2.5;
                product({{"Edit2","kW",1},{"Edit17_1","kn",-1},{"Edit18_1","kN",-1}},1,.51444/2,infinity,
                    "首个阻力工况",value("Edit2")<lo);
                product({{"Edit2","kW",1},{"Edit17_5","kn",-1},{"Edit18_5","kN",-1}},1,0,.51444*2.5,
                    "末个阻力工况",value("Edit2")>hi);
            }else if(!native)defer({"Edit2"},uiText("待校验：请先确定阻力工况的航速和阻力。"));
        }
    }else if(method=="propeller3"){
        const bool native=value("PassportSolver")==0&&value("Edit7")>=1&&value("Edit7")<=4;
        if(native){
            positive({"Edit2","Edit4","Edit5","Edit8","Edit12","Edit13","Edit14","Edit15","Edit16","Edit17"});
            if(value("Edit13")!=1)mark("Edit13",uiText("内置方法要求 i2 为 1。"));
            allow("Edit5","",0,1.5/1.3,"参考进速系数",value("Edit5")*1.3>1.5,true);
            for(const QString& key:QStringList{"Edit10","Edit11"})
                if(value(key)>=1)mark(key,uiText("该系数须小于 1。"));
        }else positiveEdits(2,17);
    }else if(method=="propeller4"){
        positiveEdits(2,16);
        if(value("Edit9")>=1)mark("Edit9",uiText("伴流分数须小于 1。"));
        ordered("Edit11","Edit12",uiText("最小直径"),uiText("最大直径"),"m",false);
    }else if(method=="propeller5"){
        positiveEdits(2,6);
        if(value("ComboBox")==0){
            range("Edit3",.3,1.05);range("Edit4",.5,1.4);range("Edit5",2,7);
            const double diskSpeed=value("Edit2")*value("Edit6")/60;
            if(usable("Edit2")&&usable("Edit6")){
                product({{"Edit7","m/s",1},{"Edit2","r/min",-1},{"Edit6","m",-1}},60,0,1.5,"进速系数",
                    !fs::ResistanceDomainValue{value("Edit7")/diskSpeed,0,1.5}.valid());
            }
            else defer({"Edit7"},uiText("待校验：请先填写转速和螺旋桨直径。"));
        }
    }
    fieldRanges.show(errors);
}
