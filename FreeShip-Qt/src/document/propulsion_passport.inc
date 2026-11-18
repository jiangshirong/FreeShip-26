// SPDX-License-Identifier: GPL-2.0-or-later
// Presentation only. All loads/intersections come from the independent core.
void Calculation::task3B(){
    auto value=[&](int n){return fields["Edit"+QString::number(n)]["value"].toDouble();};
    const int diagram=int(value(7));
    double blades=diagram==1?value(6):diagram+2,area=diagram==4?.8:value(3);
    if(blades!=std::trunc(blades)||value(8)!=std::trunc(value(8)))bad("桨叶数和螺旋桨数量必须为整数。");
    PassportInput input{value(2),area,value(4),value(5),int(blades),int(value(8)),value(9),value(10),
        value(11),value(12),value(13),value(14),value(15),value(16),value(17)};
    const auto calculated=propulsionPassport(input);
    control("Edit6",blades);control("Edit3",area);enabled(6,diagram==1);enabled(3,diagram!=4);
    auto encode=[](const PassportPoint& p){return QJsonObject{{"j",p.j},{"rpm",p.rpm},
        {"advance_speed_m_s",p.advanceSpeed},{"ship_speed_kn",p.shipSpeedKnots},{"kt",p.kt},{"kq",p.kq},
        {"total_thrust_kn",p.thrust},{"total_torque_kn_m",p.torque},{"delivered_power_kw",p.deliveredPower},
        {"engine_power_kw",p.enginePower},{"effective_thrust_kn",p.effectiveThrust},
        {"open_water_efficiency",p.efficiency?QJsonValue(*p.efficiency):QJsonValue()}};};
    line("B 系列推进特性图（内置计算）");
    line("D="+QString::number(input.diameter,'g',17)+" m; P/D="+QString::number(input.pitch,'g',17)+
         "; Ae/A0="+QString::number(area,'g',17)+"; Z="+QString::number(blades));
    line("J | n (rpm) | Vs (kn) | KT | KQ | Te (kN) | Pe (kW)");
    QJsonArray rows,branches,limits;bool nonPropulsive=false,excessEfficiency=false;
    for(int g=0;g<5;++g){QJsonArray branch;
        for(int i=0;i<5;++i){const auto& p=calculated.points[g][i];branch.append(encode(p));
            point("Series"+QString::number(2*g+1),p.shipSpeedKnots,p.effectiveThrust);
            point("Series"+QString::number(2*g+2),p.shipSpeedKnots,-p.enginePower);
            point("Series_"+QString::number(2*i+1),p.shipSpeedKnots,p.effectiveThrust);
            point("Series_"+QString::number(2*i+2),p.shipSpeedKnots,-p.enginePower);
            QStringList cells;for(double v:{p.j,p.rpm,p.shipSpeedKnots,p.kt,p.kq,p.effectiveThrust,p.enginePower})cells<<QString::number(v,'g',12);
            line(cells.join(" | "));nonPropulsive|=p.kt<0||p.kq<=0;
            excessEfficiency|=p.efficiency&&*p.efficiency>1;
        }rows.append(branch);
    }
    // Do not join ambiguous roots or reverse-ordered RPM branches. Keep every
    // root in the report so an invalid envelope cannot masquerade as a solution.
    std::vector<PassportPoint> envelope;bool ambiguous=false;
    for(int g=0;g<4;++g){const auto& branch=calculated.engine[g];QJsonArray hits;
        for(const auto& p:branch.intersections)hits.append(encode(p));
        branches.append(QJsonObject{{"rpm",branch.rpm},{"target_power_kw",branch.targetPower},{"roots",hits}});
        if(branch.intersections.size()>1)ambiguous=true;
        if(g<3&&branch.intersections.size()==1)envelope.push_back(branch.intersections.front());
    }
    std::vector<PassportPoint> nominal=calculated.engine[3].intersections;
    for(const auto& p:calculated.points[3])if(p.enginePower<=input.enginePower&&p.enginePower>0&&p.effectiveThrust>=0)nominal.push_back(p);
    std::sort(nominal.begin(),nominal.end(),[](const auto& a,const auto& b){return a.j<b.j;});
    nominal.erase(std::unique(nominal.begin(),nominal.end(),[](const auto& a,const auto& b){return std::abs(a.j-b.j)<1e-11;}),nominal.end());
    // If any nominal interval is infeasible, do not connect across it.
    for(size_t i=1;i<nominal.size();++i){
        const auto& a=nominal[i-1];const auto& b=nominal[i];
        if(!calculated.polynomial.propulsiveInterval(a.j,b.j))ambiguous=true;
    }
    if(!nominal.empty())envelope.erase(std::remove_if(envelope.begin(),envelope.end(),[&](const auto& p){return p.shipSpeedKnots>=nominal.front().shipSpeedKnots;}),envelope.end());
    envelope.insert(envelope.end(),nominal.begin(),nominal.end());
    for(size_t i=1;i<envelope.size();++i)if(envelope[i].shipSpeedKnots<=envelope[i-1].shipSpeedKnots)ambiguous=true;
    if(ambiguous)envelope.clear();
    for(const auto& p:envelope){point("Series_11",p.shipSpeedKnots,-p.enginePower);point("Series_12",p.shipSpeedKnots,p.effectiveThrust);limits.append(encode(p));}
    if(envelope.empty())warnings.append("主机功率在本次范围内没有唯一可连接的限制曲线；未作外推或强行连接。");
    if(nonPropulsive)warnings.append("部分工况超出正推力/正转矩推进区；保留有符号载荷，不作为主机工作点。");
    if(excessEfficiency)warnings.append("多项式在部分工况给出效率大于 1；该拟合结果不具物理可用性。");
    static const double low[]={0,0,.3,.35,.4,.45,.5,.55},high[]={0,0,.3,.8,1.,1.05,.95,.85};
    if(area<low[int(blades)]||area>high[int(blades)])warnings.append("所选桨叶数/面积比超出该桨叶数的试验组合覆盖；仅在总体回归范围内。");
    resistance_curves(1,1);
    for(const auto& warning:warnings)if(warning.toString().startsWith("未叠加船体阻力")==false)line(warning.toString());
    for(const auto& b:calculated.engine){line("主机交点 n="+QString::number(b.rpm,'g',17)+" rpm; P="+QString::number(b.targetPower,'g',17)+" kW");
        if(b.intersections.empty())line("范围内无推进工作点。");
        for(const auto& p:b.intersections)line("J="+QString::number(p.j,'g',17)+"; Vs="+QString::number(p.shipSpeedKnots,'g',17)+" kn; Te="+QString::number(p.effectiveThrust,'g',17)+" kN");}
    nativeResult=QJsonObject{{"method","Wageningen B-series 1975; constant hull factors"},{"reynolds",2e6},
        {"diagram",diagram},{"blades",blades},{"area_ratio",area},{"points",rows},{"engine_intersections",branches},
        {"engine_envelope",limits},{"knot_m_s",1852./3600.},{"extrapolated",false}};
    engineCurve=QJsonObject{{"power_law","Pnom*(n/nnom)^2"},{"intersections",branches},{"extrapolated",false},
        {"intersection_method","bounded cubic roots"},{"plotted_points",int(envelope.size())}};
}
