// SPDX-License-Identifier: GPL-2.0-or-later
void ResistanceBase::calculateNativeOort(){
    auto n=[&](const char* key){return fields.value(QString::fromLatin1(key).toLower()).value;};
    if(field("estimate2box").checked)bad(reportText("此方法仅计算静水阻力；请手动填写航海余量，或选择外部程序计算海况修正。"));
    if(n("DraftBox")!=0)bad(reportText("van Oortmerssen 不提供纵倾修正；请使用对应吃水的尺度并将纵倾差设为零。"));
    const double margin=n("KeBox"),count=n("NpBox"),diameter=n("DpBox");
    if(margin<1)bad(reportText("手动航海余量系数须不小于 1；0 的自动海况流程仅在旧方法中提供。"));
    if(count!=std::trunc(count)||count<0||count>2||diameter<0||diameter>25)bad(reportText("螺旋桨数量须为 0–2 的整数，直径须在 0–25 m 内。"));
    const double lengthUnit=settings.projectunits==1?.3048:1,areaUnit=lengthUnit*lengthUnit,volumeUnit=areaUnit*lengthUnit;
    // LPP is a new explicitly metre-valued field; the existing Dp field is also SI.
    const OortmerssenInput input{n("LwlBox")*lengthUnit,n("OortLpp"),n("BwlBox")*lengthUnit,n("DraftTotalBox")*lengthUnit,n("DisplacementBox")*volumeUnit,
        n("CpBox"),n("LCBBox"),n("KBulbBox"),field("estimatebox").checked?0:n("WettedSurfacebox")*areaUnit,
        n("DensityBox")*(settings.projectunits==1?.45359237/volumeUnit:1000),n("ViscosityBox")*areaUnit*1e-6,n("OortCorrelation")};
    if(!field("estimatebox").checked&&input.wettedArea<=0)bad(reportText("请填写湿表面积或启用估计，未把零值当作有效面积。"));
    const auto model=oortmerssenModel(input);
    const double first=n("StartSpeedBox"),design=n("EndSpeedBox"),last=n("StepSpeedBox");
    const auto designPoint=oortmerssenPoint(model,design);oortmerssenPoint(model,last);
    auto encode=[](const OortmerssenPoint& p){return QJsonObject{{"knots",p.knots},{"speed_ms",p.speed},{"fn",p.fn},
        {"reynolds",p.reynolds},{"cf",p.cf},{"friction_kn",p.frictionKn},{"allowance_kn",p.allowanceKn},
        {"residuary_kn",p.residuaryKn},{"total_kn",p.totalKn},{"power_kw",p.powerKw}};};
    QJsonArray rows,plot,coefficients;for(double c:model.coefficients)coefficients.append(c);
    bool decreasing=false;double previous=-1;
    for(int i=0;i<=200;++i){check();const auto p=oortmerssenPoint(model,first+(last-first)*i/200.);
        if(previous>=0&&p.residuaryKn<previous-1e-9*std::max(1.,previous))decreasing=true;previous=p.residuaryKn;
        plot.append(encode(p));point("series1",p.knots,p.totalKn);point("series3",p.knots,p.powerKw);
        if(margin>1){point("series2",p.knots,p.totalKn*margin);point("series4",p.knots,p.powerKw*margin);}}
    setSeriesTitle("series1",reportText("静水总阻力 Rt"));setSeriesTitle("series2",reportText("含手动余量的阻力"));
    setSeriesTitle("series3",reportText("有效功率 Pe"));setSeriesTitle("series4",reportText("含手动余量的有效功率"));
    xTitle=reportText("航速 (kn)");yTitle=reportText("阻力 (kN)");rightTitle=reportText("有效功率 (kW)");
    header(reportText("van Oortmerssen 1971 阻力计算（Helmore 2008 勘误）"),resultsmemo);
    resultsmemo<<QString("LD=%1 m；Cp(LD)=%2；LCB(LD)=%3%；Cm=%4；S=%5 m²").arg(model.length,0,'g',12).arg(model.cp,0,'g',12).arg(model.lcb,0,'g',12).arg(model.cm,0,'g',12).arg(model.wettedArea,0,'g',12)
        <<QString(reportText("推进衔接采用用户给定值：w=%1，t=%2，ηR=%3；本阻力方法不推算这些参数。")).arg(n("OortWake"),0,'g',12).arg(n("OortDeduction"),0,'g',12).arg(n("OortEtaR"),0,'g',12)
        <<"Vs (kn) | Fn | Rf (kN) | Rallow (kN) | Rr (kN) | Rt (kN) | Pe (kW)";
    QJsonArray cacheData;
    for(int i=0;i<9;++i){check();auto p=oortmerssenPoint(model,i==8?last:i==7?design:first+(design-first)*i/7.);rows.append(encode(p));
        QStringList cells;for(double value:{p.knots,p.fn,p.frictionKn,p.allowanceKn,p.residuaryKn,p.totalKn,p.powerKw})cells<<QString::number(value,'g',12);resultsmemo<<cells.join(" | ");
        cacheData.append(QJsonArray{p.knots,p.totalKn,p.totalKn*margin,p.powerKw,p.powerKw*margin});
    }
    if(decreasing)warning(reportText("所选航速范围出现剩余阻力下降段；本组结果不可用于后续推进计算，请复核方法适用性。"));
    for(const auto& w:warnings)resultsmemo<<w;
    nativeResult={{"method","van Oortmerssen 1971, Helmore 2008 corrections"},{"length_m",model.length},
        {"cp_ld",model.cp},{"lcb_ld_percent",model.lcb},{"cm",model.cm},{"wetted_area_m2",model.wettedArea},
        {"density_kg_m3",input.density},{"correlation",input.correlation},{"coefficients",coefficients},{"m",model.m},
        {"design",encode(designPoint)},{"rows",rows},{"plot",plot},{"manual_margin",margin},
        {"sampled_residuary_decrease",decreasing},{"monotonic_samples",201},{"cache_published",!decreasing}};
    resultsmemo2<<reportText("原论文 Table II 的 d3,5 按 Helmore 勘误取正；回归和式整体除以 1000。")
        <<reportText("采用 ITTC-1957 摩擦线、标准重力 9.80665 m/s²、1 kn=1852/3600 m/s。零速阻力为零；Re<10^5 拒绝。")
        <<reportText("适用域：LD/B 3–6.2，B/T 1.9–4，Cp(LD) 0.50–0.725，Cm 0.73–0.97，LCB(LD) -8%–2.8%，半进流角 10–46°，Fn≤0.50。");
    if(decreasing)return;
    writeNativeResistanceCache("oortmerssen",cacheData,count,input.density,diameter,n("OortWake"),n("OortDeduction"),n("OortEtaR"));
}
