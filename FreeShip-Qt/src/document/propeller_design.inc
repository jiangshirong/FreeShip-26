// SPDX-License-Identifier: GPL-2.0-or-later
void Calculation::designB(){
    auto value=[&](int n){return fields["Edit"+QString::number(n)]["value"].toDouble();};
    const int diagram=int(value(task==1?14:12));
    const double blades=diagram==1?value(task==1?15:13):diagram+2;
    const double area=diagram==4?.8:value(task==1?16:14),count=value(4);
    if(blades!=std::trunc(blades)||count!=std::trunc(count))bad("桨叶数和螺旋桨数量必须为整数。");
    if(value(task==1?11:9)!=1)bad("内置初选仅支持旧 i2=1；其他值请选择旧 CALCPROP 方法。");
    double limit=task==1?25:fields["DesignDiameterLimit"]["value"].toDouble();if(limit==0)limit=25;
    PropellerDesignInput input{value(task==1?2:16),task==1?value(3):1.,value(task==1?7:5),
        value(task==1?8:6),value(task==1?9:7),value(task==1?10:8),value(task==1?12:10),value(task==1?13:11),
        area,fields["DesignPitchLow"]["value"].toDouble(),fields["DesignPitchHigh"]["value"].toDouble(),limit,int(blades),int(count)};
    auto encode=[](const PropellerDesignPoint& p){return QJsonObject{{"speed_kn",p.speedKnots},{"resistance_kn",p.resistanceKn},
        {"diameter_m",p.diameter},{"rpm",p.rpm},{"pitch_ratio",p.pitch},{"j",p.j},{"kt",p.kt},{"kq",p.kq},{"eta0",p.eta0},
        {"total_thrust_kn",p.totalThrustKn},{"total_torque_kn_m",p.totalTorqueKnM},{"delivered_power_kw",p.deliveredPowerKw},
        {"shaft_input_power_kw",p.shaftInputPowerKw},{"engine_power_kw",p.enginePowerKw},{"thrust_residual_kn",p.thrustResidualKn},
        {"pitch_boundary",p.pitchBoundary},{"advance_boundary",p.advanceBoundary},{"diameter_boundary",p.diameterBoundary}};};
    auto show=[&](const PropellerDesignPoint& p){QStringList cells;
        for(double v:{p.speedKnots,p.resistanceKn,p.diameter,p.rpm,p.pitch,p.j,p.eta0,p.enginePowerKw})cells<<QString::number(v,'g',12);
        line(cells.join(" | "));
    };
    control("Edit"+QString::number(task==1?15:13),blades);control("Edit"+QString::number(task==1?16:14),area);
    line("B 系列敞水初选（内置计算，Re=2×10^6）");
    line("Vs (kn) | R (kN) | D (m) | n (rpm) | P/D | J | η0 | Pe (kW)");
    nativeResult=QJsonObject{{"method","B-series 1975 bounded preliminary design"},{"reynolds",2e6},
        {"diagram",diagram},{"blades",blades},{"area_ratio",area},{"pitch_intervals",360},{"refinement_steps",48},
        {"pitch_bounds",QJsonArray{input.pitchLow,input.pitchHigh}},{"max_diameter_m",limit},{"global_optimum_proven",false}};
    QJsonArray candidates;
    if(task==1){
        if(value(6)<=0||value(5)-4*value(6)<=0)bad("五个候选直径必须为正，直径步长必须大于零。");
        std::vector<PropellerDesignPoint> valid;
        for(int i=0;i<5;++i){double diameter=value(5)-i*value(6);
            auto search=designPropeller(input,PropellerConstraint::Diameter,diameter,[&]{check();});
            QJsonObject row{{"requested_diameter_m",diameter},{"evaluations",search.evaluations},{"max_roots",search.maxRootCount}};
            if(search.best){row["solution"]=encode(*search.best);valid.push_back(*search.best);show(*search.best);
            }else{row["solution"]=QJsonValue();warnings.append("直径 "+QString::number(diameter)+" m 在本搜索范围未找到有效候选。");}
            candidates.append(row);
        }
        if(valid.empty())bad("本次直径和螺距搜索范围内没有有效初选结果。");
        std::sort(valid.begin(),valid.end(),[](const auto& a,const auto& b){return a.rpm<b.rpm;});
        for(const auto& p:valid){point("Series1",p.rpm,p.shaftInputPowerKw);point("Series2",p.rpm,p.enginePowerKw);
            point("Series3",p.rpm,p.diameter);point("Series4",p.rpm,p.enginePowerKw/.9);}
    }else{
        std::array<double,5> speeds,resistances;
        for(int i=0;i<5;++i){speeds[i]=fields[QString("Edit17_%1").arg(i+1)]["value"].toDouble();resistances[i]=fields[QString("Edit18_%1").arg(i+1)]["value"].toDouble();}
        const auto match=matchPropeller(input,value(3),value(2),speeds,resistances,[&]{check();});
        for(const auto& p:match.table){candidates.append(encode(p));show(p);}
        QJsonArray envelope;for(const auto& p:match.search){
            envelope.append(encode(p));point("Series1",p.speedKnots,p.enginePowerKw);
            point("Series2",p.speedKnots,p.resistanceKn*p.speedKnots*(1852./3600.));point("Series3",p.speedKnots,value(2));
        }
        nativeResult["target"]=encode(match.target);nativeResult["matched"]=encode(match.matched);
        nativeResult["design_envelope"]=envelope;nativeResult["table_limited"]=match.tableLimited;
        nativeResult["power_residual_kw"]=match.powerResidualKw;
        line("目标航速初选：");show(match.target);line("主机匹配候选：");show(match.matched);
        line("功率差（所需−可用）："+QString::number(match.powerResidualKw,'g',12)+" kW");
        if(match.tableLimited)warnings.append("主机功率覆盖到阻力表最高航速；结果受数据范围限制，不是极限航速或功率平衡点。");
        if(fields["DesignDiameterLimit"]["value"].toDouble()==0)warnings.append("未指定船体安装直径限制；搜索上限为 25 m，必须核对安装空间。");
        const auto& m=match.matched;QStringList legacy;
        for(double v:{m.diameter,m.pitch,area,blades,m.j,m.kt,10*m.kq,m.eta0,input.etaR,1.,input.etaS,input.etaG,double(diagram),m.rpm,value(2)})legacy<<QString::number(v,'g',17);
        pendingPropeller=("# FreeShip26-native-design-v1 D P/D Ae/Ao Z J KT 10KQ eta0 etaR i2 etaS etaG diagram rpm power\r\n"+legacy.join(' ')+"\r\n").toLatin1();
        QJsonObject extracted;int n=2;
        for(double v:{m.diameter,area,m.pitch,m.j,blades,double(diagram),count,m.rpm,input.wake,input.deduction,input.etaR,1.,input.etaS,input.etaG,input.density,value(2)})extracted["Edit"+QString::number(n++)]=v;
        pendingPropellerMetadata=QJsonDocument(QJsonObject{{"version",1},{"inputs",extracted},
            {"inputs_sha256",QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(extracted).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())},
            {"legacy_sha256",QString::fromLatin1(QCryptographicHash::hash(pendingPropeller,QCryptographicHash::Sha256).toHex())}}).toJson();
    }
    nativeResult["candidates"]=candidates;
    for(auto warning:warnings)line(warning.toString());
}
