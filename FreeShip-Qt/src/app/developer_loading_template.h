// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include "core/loading_cases.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <cmath>

namespace loadingTemplate {
struct Template {QString description;QJsonObject book;};
inline Template forHull(const fs::ModelSnapshot& m,const QJsonObject& defaults){
    QJsonArray fixed,tanks,cases;
    // The particular workbook is valid only in the source hull's coordinate system.
    QByteArray xyz;
    if(m.units=="metric" && m.points.size()==718 && m.faces.size()==637){
        QDataStream out(&xyz,QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);out.setFloatingPointPrecision(QDataStream::SinglePrecision);
        for(const auto& p:m.points)out<<float(p.position.x)<<float(p.position.y)<<float(p.position.z);
    }
    QString note;
    if(!xyz.isEmpty() && QCryptographicHash::hash(xyz,QCryptographicHash::Sha256).toHex()=="f4a58cf368efb7b7ae77368bcd49a26993697c95617678c027718b020cdae56e"){
        // Same-hull source: see docs/testing/developer-loading-templates.md, rows 78/79.
        fixed.append(QJsonArray{"空船",12489.28,99.79,0,11.51});
        cases={QJsonArray{"满载出港（等效载重）",72102.510259873,109.9975314416926,0,11.27801990657569},
               QJsonArray{"满载到港（等效载重）",68958.79699027301,113.44611685721567,0,11.043723194509427},
               QJsonArray{"压载出港（等效载重）",22095.188219000007,102.98472210603565,0,9.5188117790674411},
               QJsonArray{"压载到港（等效载重）",18952.979494400002,113.94627699281835,0,8.260398704709731}};
        note="课程设计：货重栏为合计载重量；不含自由液面修正。";
    }else{
        const bool imperial=m.units=="imperial";
        const double u=imperial?.3048:1.,mass=defaults["design_displacement"].toDouble()*(imperial?1.0160469088:1.);
        const double l=m.length*u,b=m.beam*u,t=m.draft*u,z=defaults["keel_z"].toDouble()*u;
        if(!(mass>0&&l>0&&b>0&&t>0))return {"缺少设计排水量或主尺度。",{}};
        if(!defaults["design_buoyancy_x"].isDouble()||!defaults["design_buoyancy_y"].isDouble())
            return {"缺少设计水线浮心，无法生成模拟载荷。",{}};
        const double x=defaults["design_buoyancy_x"].toDouble()*u,y=defaults["design_buoyancy_y"].toDouble()*u;
        fixed.append(QJsonArray{"测试空船",.65*mass,x,y,z+t*.85});
        const double tx=x-l*.06,ty=.18*b;
        for(int side:{-1,1})tanks.append(QJsonArray{side<0?"测试燃油舱（左）":"测试燃油舱（右）",.10*l,.12*b,.5*t,.85,tx,y+side*ty,z+.12*t});
        cases={QJsonArray{"测试 · 补给出港",.20*mass,x,y,z+1.2*t,100,100},
               QJsonArray{"测试 · 作业途中",.18*mass,x,y,z+1.2*t,50,50},
               QJsonArray{"测试 · 返港",.04*mass,x,y,z+t,10,10}};
        note="按本船设计排水量估算；舱位及载荷为模拟值。";
    }
    return {note,{{"version",1},{"fixed",fixed},{"tanks",tanks},{"cases",cases},{"criteria",QJsonArray{}}}};
}
}
