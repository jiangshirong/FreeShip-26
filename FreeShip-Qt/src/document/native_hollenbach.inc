// SPDX-License-Identifier: GPL-2.0-or-later
void ResistanceBase::extractNativeHollenbach(){
    double draft=field("drafttotalbox").value;if(draft<=0)draft=documentGeometrySettings(source).draft;
    Hydrostatics h(source,bindings);auto result=h.calculate({{"mode","single"},{"draft",draft},{"heel",0},{"trim",field("draftbox").value}},output+"/cancel.flag");
    if(result["cancelled"].toBool())bad(result["message"].toString());
    const auto row=result["rows"].toArray().at(0).toObject();if(row["status"]!="ok")bad(row["message"].toString());
    auto number=[&](const QString& key){auto v=row[key];if(!v.isDouble()||!std::isfinite(v.toDouble()))bad(reportText("船模缺少有效静水力数据：")+key);return v.toDouble();};
    auto axis=[&](const QString& key){auto v=row[key].toArray().at(0);if(!v.isDouble()||!std::isfinite(v.toDouble()))bad(reportText("船模缺少有效水下范围：")+key);return v.toDouble();};
    field("drafttotalbox").set(draft);field("lwlbox").set(number("lwl"));field("bwlbox").set(number("bwl"));
    field("wettedsurfacebox").set(number("wetted_area"));field("displacementbox").set(number("volume"));
    field("losbox").set(field("holcondition").itemindex==1?number("lwl"):axis("sub_max")-axis("wl_min"));
    field("densitybox").set(documentGeometrySettings(source).density);field("estimatebox").checked=false;field("wettedsurfacebox").enabled=true;
}
void ResistanceBase::calculateNativeHollenbach(){
    auto n=[&](const char* key){return fields.value(QString::fromLatin1(key).toLower()).value;};
    auto count=[&](const char* key){const double v=n(key);if(v!=std::trunc(v))bad(reportText("附体数量须为整数。"));return int(v);};
    if(field("estimate2box").checked)bad(reportText("此方法仅计算静水阻力；请手动填写航海余量，或选择外部程序计算海况修正。"));
    const double margin=n("KeBox");if(margin<1)bad(reportText("手动航海余量系数须不小于 1。"));
    const double unit=settings.projectunits==1?.3048:1,area=unit*unit,volume=area*unit;
    const bool estimate=field("estimatebox").checked;const int mode=field("holcondition").itemindex;
    if(estimate&&n("HolAppendageArea")!=0)bad(reportText("估计面积已经包含附体，不再叠加另填的附体面积；请将该项清零。"));
    const double manualArea=n("WettedSurfacebox")*area+n("HolAppendageArea");
    if(!estimate&&n("WettedSurfacebox")<=0)bad(reportText("请填写湿表面积或启用估计。"));
    const HollenbachInput input{HollenbachCondition(mode),n("HolLpp"),n("LwlBox")*unit,n("Losbox")*unit,
        n("BwlBox")*unit,n("DraftTotalBox")*unit,n("DraftBox")*unit,n("DisplacementBox")*volume,n("DpBox"),estimate?0:manualArea,
        n("DensityBox")*(settings.projectunits==1?.45359237/volume:1000),n("ViscosityBox")*area*1e-6,n("HolCorrelation"),
        count("HolRudders"),count("HolBrackets"),count("HolBossings"),count("HolThrusters"),field("holbulb").itemindex==0};
    const auto model=hollenbachModel(input);const double first=n("StartSpeedBox"),design=n("EndSpeedBox"),last=n("StepSpeedBox");
    const double speedScale=std::sqrt(9.80665*model.length)*3600/1852;
    if(first/speedScale<model.fnMin-1e-12||last/speedScale>model.fnMax+1e-12)
        bad(QString(reportText("本船况允许航速 %1–%2 kn（Fn %3–%4）；请调整起始、设计和最大航速，未作外推。"))
            .arg(model.fnMin*speedScale,0,'g',10).arg(model.fnMax*speedScale,0,'g',10).arg(model.fnMin,0,'g',8).arg(model.fnMax,0,'g',8));
    const auto designPoint=hollenbachPoint(model,design);hollenbachPoint(model,first);hollenbachPoint(model,last);
    auto encode=[](const HollenbachPoint& p){QJsonObject o{{"knots",p.knots},{"speed_ms",p.speed},{"fn",p.fn},{"reynolds",p.reynolds},
        {"cf",p.cf},{"cr_standard",p.crStandard},{"fn_factor",p.fnFactor},{"cr",p.cr},{"friction_kn",p.frictionKn},{"allowance_kn",p.allowanceKn},
        {"residuary_kn",p.residuaryKn},{"total_kn",p.totalKn},{"power_kw",p.powerKw},{"maximum_kn",p.maximumKn},
        {"minimum_kn",p.minimumValid?QJsonValue(p.minimumKn):QJsonValue()}};return o;};
    QJsonArray rows,plot,cacheData;bool decreasing=false,minimumCrossing=false;double previous=-1;
    for(int i=0;i<=200;++i){check();const auto p=hollenbachPoint(model,first+(last-first)*i/200.);
        if(previous>=0&&p.totalKn<previous-1e-9*std::max(1.,previous))decreasing=true;previous=p.totalKn;
        minimumCrossing|=p.minimumValid&&p.minimumKn>p.totalKn;plot.append(encode(p));
        point("series1",p.knots,p.totalKn);point("series3",p.knots,p.powerKw);
        if(margin>1){point("series2",p.knots,p.totalKn*margin);point("series4",p.knots,p.powerKw*margin);}}
    setSeriesTitle("series1",reportText("静水平均阻力 Rt"));setSeriesTitle("series2",reportText("含手动余量的阻力"));
    setSeriesTitle("series3",reportText("有效功率 Pe"));setSeriesTitle("series4",reportText("含手动余量的有效功率"));
    xTitle=reportText("航速 (kn)");yTitle=reportText("阻力 (kN)");rightTitle=reportText("有效功率 (kW)");
    header(reportText("Hollenbach 1999 静水阻力计算"),resultsmemo);
    const QString condition=QStringList{reportText("单桨／设计吃水"),reportText("单桨／压载吃水"),reportText("双桨／设计吃水")}[mode];
    resultsmemo<<condition<<QString(reportText("LPP=%1 m；LWL=%2 m；LOS=%3 m；Fn 长度=%4 m")).arg(input.lpp,0,'g',12).arg(input.lwl,0,'g',12).arg(input.los,0,'g',12).arg(model.length,0,'g',12)
        <<QString(reportText("CB(LPP)=%1；尾吃水 TA=%2 m；首吃水 TF=%3 m；总湿表面积=%4 m²")).arg(model.cb,0,'g',12).arg(model.ta,0,'g',12).arg(model.tf,0,'g',12).arg(model.wettedArea,0,'g',12)
        <<QString(reportText("推进衔接：桨数=%1，Dp=%2 m，w=%3，t=%4，ηR=%5（后三项均由用户给定）。")).arg(mode==2?2:1).arg(input.diameter,0,'g',12).arg(n("HolWake"),0,'g',12).arg(n("HolDeduction"),0,'g',12).arg(n("HolEtaR"),0,'g',12)
        <<"Vs (kn) | Fn | Rf (kN) | Rallow (kN) | Rr (kN) | Rt (kN) | Pe (kW) | Rmin (kN) | Rmax (kN)";
    for(int i=0;i<9;++i){check();auto p=hollenbachPoint(model,i==8?last:i==7?design:first+(design-first)*i/7.);rows.append(encode(p));
        QStringList cells;for(double value:{p.knots,p.fn,p.frictionKn,p.allowanceKn,p.residuaryKn,p.totalKn,p.powerKw})cells<<QString::number(value,'g',12);
        cells<<(p.minimumValid?QString::number(p.minimumKn,'g',12):QString("—"))<<QString::number(p.maximumKn,'g',12);resultsmemo<<cells.join(" | ");
        cacheData.append(QJsonArray{p.knots,p.totalKn,p.totalKn*margin,p.powerKw,p.powerKw*margin});}
    if(estimate)warning(reportText("湿表面积采用含附体的估计公式；有实测总面积时请优先使用实测值。"));
    else warning(reportText("实际面积应包含附体；从船模提取后，请检查模型是否包含舵及轴系，未计入的面积可单独补填。"));
    if(decreasing)warning(reportText("本航速区间出现平均总阻力下降，保留公式结果但不发布推进输入，请复核适用性。"));
    if(minimumCrossing)warning(reportText("本参数组合的最小估计高于平均估计，未裁剪；请复核船型适用性。"));
    for(const auto& w:warnings)resultsmemo<<w;
    nativeResult={{"method","Hollenbach ICCAS 1999; (1+N) convention"},{"condition",mode},{"cb",model.cb},{"ta_m",model.ta},{"tf_m",model.tf},
        {"length_m",model.length},{"wetted_area_m2",model.wettedArea},{"fn_critical",model.fnCritical},{"fn_min",model.fnMin},{"fn_max",model.fnMax},
        {"geometry_factor",model.geometryFactor},{"length_factor",model.lengthFactor},{"minimum_geometry_valid",model.minimumGeometryValid},
        {"correlation",input.correlation},{"density_kg_m3",input.density},{"design",encode(designPoint)},{"rows",rows},{"plot",plot},{"manual_margin",margin},
        {"sampled_total_decrease",decreasing},{"minimum_above_mean",minimumCrossing},{"cache_published",!decreasing}};
    resultsmemo2<<reportText("LOS 在设计吃水指尾部水线端到水下最前点；压载吃水取该吃水水线的首尾端，均排除舵。")
        <<reportText("纵倾差输入为 TF−TA；平均吃水为 (TA+TF)/2。LPP、桨直径及另加附体面积始终用 m、m、m²，其余尺度依工程单位。")
        <<reportText("原始论文与数量因子约定见 NATIVE-HOLLENBACH.md。");
    if(!decreasing)writeNativeResistanceCache("hollenbach",cacheData,mode==2?2:1,input.density,input.diameter,n("HolWake"),n("HolDeduction"),n("HolEtaR"));
}
