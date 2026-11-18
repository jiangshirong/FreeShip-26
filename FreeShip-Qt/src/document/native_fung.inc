// SPDX-License-Identifier: GPL-2.0-or-later
void ResistanceBase::extractNativeFung(){
    double draft=field("drafttotalbox").value;if(draft<=0)draft=documentGeometrySettings(source).draft;
    Hydrostatics h(source,bindings);auto result=h.calculate({{"mode","single"},{"draft",draft},{"heel",0},{"trim",0}},output+"/cancel.flag");
    if(result["cancelled"].toBool())bad(result["message"].toString());const auto row=result["rows"].toArray().at(0).toObject();
    if(row["status"]!="ok")bad(row["message"].toString());
    auto number=[&](const QString& key){auto v=row[key];if(!v.isDouble()||!std::isfinite(v.toDouble()))bad(reportText("船模缺少有效静水力数据：")+key);return v.toDouble();};
    field("drafttotalbox").set(draft);field("draftbox").set(0);field("lwlbox").set(number("lwl"));field("bwlbox").set(number("bwl"));
    field("wettedsurfacebox").set(number("wetted_area"));field("displacementbox").set(number("volume"));
    auto axis=[&](const QString& key){auto v=row[key].toArray().at(0);if(!v.isDouble()||!std::isfinite(v.toDouble()))bad(reportText("船模缺少有效位置：")+key);return v.toDouble();};
    field("cpbox").set(number("cp"));field("lcbbox").set(100*(axis("buoyancy")-axis("wl_min")-number("lwl")/2)/number("lwl"));
    field("kbulbbox").set(number("entrance_angle"));
    field("fungcwp").set(number("waterplane_area")/(number("lwl")*number("bwl")));
    field("densitybox").set(documentGeometrySettings(source).density);field("estimatebox").checked=false;field("wettedsurfacebox").enabled=true;
}
void ResistanceBase::calculateNativeFung(){
    auto n=[&](const char* key){return fields.value(QString::fromLatin1(key).toLower()).value;};
    if(field("estimatebox").checked)bad(reportText("Fung–Leibman 请填写或从船模提取裸船湿表面积。"));
    if(field("estimate2box").checked)bad(reportText("此方法仅计算静水阻力，请手动填写航海余量。"));
    if(n("DraftBox")!=0)bad(reportText("Fung–Leibman 仅接受零纵倾差。"));
    if(n("NpBox")!=std::trunc(n("NpBox"))||n("NpBox")<1||n("NpBox")>2||n("DpBox")<=0)bad(reportText("推进衔接须填写 1 或 2 个螺旋桨及正的桨直径。"));
    const double unit=settings.projectunits==1?.3048:1,area=unit*unit,volume=area*unit,margin=n("KeBox");
    if(margin<1)bad(reportText("手动航海余量须不小于 1。"));
    const FungInput input{n("LwlBox")*unit,n("BwlBox")*unit,n("DraftTotalBox")*unit,n("DisplacementBox")*volume,n("CpBox"),
        n("WettedSurfacebox")*area,n("KBulbBox"),n("BABox")*area,n("ZBulbBox")*unit,
        n("DensityBox")*(settings.projectunits==1?.45359237/volume:1000),n("ViscosityBox")*area*1e-6,n("FungCorrelation"),n("LCBBox"),n("FungCwp"),n("FungTransomDraftRatio")};
    const auto model=fungModel(input);const double first=n("StartSpeedBox"),design=n("EndSpeedBox"),last=n("StepSpeedBox"),scale=std::sqrt(9.80665*input.lwl)*3600/1852;
    if(first/scale<.15-1e-12||last/scale>.9+1e-12)bad(QString(reportText("本船允许航速 %1–%2 kn（Fn 0.15–0.90），请调整航速区间，未作外推。")).arg(.15*scale,0,'g',10).arg(.9*scale,0,'g',10));
    auto encode=[](const FungPoint& p){return QJsonObject{{"knots",p.knots},{"speed_ms",p.speed},{"fn",p.fn},{"reynolds",p.reynolds},{"cf",p.cf},
        {"x1",p.x1},{"x2",p.x2},{"log_cr",p.logCr},{"cr",p.cr},{"friction_kn",p.frictionKn},{"allowance_kn",p.allowanceKn},
        {"residuary_kn",p.residuaryKn},{"total_kn",p.totalKn},{"power_kw",p.powerKw}};};
    const auto designPoint=fungPoint(model,design);QJsonArray rows,plot,cache;bool decreasing=false;double previous=-1;
    for(int i=0;i<=200;++i){check();auto p=fungPoint(model,first+(last-first)*i/200.);if(previous>=0&&p.totalKn<previous-1e-9*std::max(1.,previous))decreasing=true;previous=p.totalKn;
        plot.append(encode(p));point("series1",p.knots,p.totalKn);point("series3",p.knots,p.powerKw);
        if(margin>1){point("series2",p.knots,p.totalKn*margin);point("series4",p.knots,p.powerKw*margin);}}
    setSeriesTitle("series1",reportText("静水阻力 Rt"));setSeriesTitle("series2",reportText("含手动余量的阻力"));setSeriesTitle("series3",reportText("有效功率 Pe"));setSeriesTitle("series4",reportText("含手动余量的有效功率"));
    setAxis("series3",2);setAxis("series4",2);
    xTitle=reportText("航速 (kn)");yTitle=reportText("阻力 (kN)");rightTitle=reportText("有效功率 (kW)");header(reportText("Fung–Leibman 1995 静水阻力（1999 勘误版）"),resultsmemo);
    resultsmemo<<QString("Cm=%1；DLR=%2；Cws=%3；AT/AM=%4；BT/B=%5；λ=%6").arg(model.cm,0,'g',12).arg(model.dlr,0,'g',12).arg(model.cws,0,'g',12).arg(model.areaRatio,0,'g',12).arg(model.beamRatio,0,'g',12).arg(model.lambda,0,'g',12)
        <<QString(reportText("推进衔接：桨数=%1；Dp=%2 m；w=%3；t=%4；ηR=%5（后三项由用户给定）。")).arg(n("NpBox")).arg(n("DpBox"),0,'g',12).arg(n("FungWake"),0,'g',12).arg(n("FungDeduction"),0,'g',12).arg(n("FungEtaR"),0,'g',12)
        <<"Vs (kn) | Fn | Cf | Cr | Rf (kN) | Rr (kN) | Rallow (kN) | Rt (kN) | Pe (kW)";
    for(int i=0;i<9;++i){check();auto p=fungPoint(model,i==8?last:i==7?design:first+(design-first)*i/7.);rows.append(encode(p));QStringList cells;
        for(double value:{p.knots,p.fn,p.cf,p.cr,p.frictionKn,p.residuaryKn,p.allowanceKn,p.totalKn,p.powerKw})cells<<QString::number(value,'g',12);
        resultsmemo<<cells.join(" | ");cache.append(QJsonArray{p.knots,p.totalKn,p.totalKn*margin,p.powerKw,p.powerKw*margin});}
    if(input.cwp==0)warning(reportText("尚未提供 CWP，水线面系数的适用范围未检查。"));
    if(input.transomDraftRatio<0)warning(reportText("尚未提供尾封板浸深比，其适用范围未检查。"));
    if(decreasing)warning(reportText("采样航速内总阻力出现下降，保留公式结果供复核，但不发布推进输入。"));
    for(const auto& w:warnings)resultsmemo<<w;
    nativeResult={{"method","Fung-Leibman FAST 1995 / Peacock et al 1999 correction"},{"cm",model.cm},{"dlr",model.dlr},{"cws",model.cws},
        {"transom_area_ratio",model.areaRatio},{"transom_beam_ratio",model.beamRatio},{"lambda",model.lambda},{"wetted_area_m2",input.wettedArea},
        {"density_kg_m3",input.density},{"correlation",input.correlation},{"design",encode(designPoint)},{"rows",rows},{"plot",plot},{"manual_margin",margin},
        {"cwp_checked",input.cwp!=0},{"transom_draft_checked",input.transomDraftRatio>=0},{"sampled_total_decrease",decreasing},{"cache_published",!decreasing}};
    resultsmemo2<<reportText("水线长、宽、吃水、裸船湿表面积、尾封板面积与宽度采用工程单位；桨直径始终用 m，进流角用度。")
        <<reportText("CWP 和尾封板浸深比只用于适用性检查；未提供时报告明确注明。模型提取不猜测尾封板参数，请自行核对。")
        <<reportText("公式版本和参考文献见 NATIVE-FUNG.md。");
    if(!decreasing)writeNativeResistanceCache("fung",cache,n("NpBox"),input.density,n("DpBox"),n("FungWake"),n("FungDeduction"),n("FungEtaR"));
}
