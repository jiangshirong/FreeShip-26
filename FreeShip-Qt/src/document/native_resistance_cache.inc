// SPDX-License-Identifier: GPL-2.0-or-later
// Common full-precision handoff for native resistance methods. The 9-row
// protocol and 5-row legacy view remain readable by the existing propeller UI.
namespace {
using ResistanceRow=std::array<double,5>;
std::vector<ResistanceRow> transferRows(const QByteArray& bytes){
    const auto cells=bytes.simplified().split(' ');
    if(cells.size()%5!=0)return {};
    std::vector<ResistanceRow> rows;
    for(qsizetype i=0;i<cells.size();i+=5){
        ResistanceRow row{};
        for(int n=0;n<5;++n){
            bool ok=false;
            row[n]=cells[i+n].toDouble(&ok);
            if(!ok||!std::isfinite(float(row[n])))return {};
        }
        rows.push_back(row);
    }
    return rows;
}
bool transferable(const ResistanceRow& row){
    return float(row[0])>0&&float(row[1])>=0&&
        std::all_of(row.begin(),row.end(),[](double v){return std::isfinite(float(v));});
}
bool orderedTransfer(const std::vector<ResistanceRow>& rows){
    if(rows.size()!=5)return false;
    for(size_t i=0;i<rows.size();++i)
        if(!transferable(rows[i])||(i>0&&float(rows[i][0])<=float(rows[i-1][0])))return false;
    return true;
}
}

void ResistanceBase::prepareAccelerationTransfer(){
    const auto statePath=work+"/RESIST-acceleration.json";
    if(QFile::exists(statePath))return;
    QFile table(work+"/RESIST.dat");
    const auto existing=table.open(QIODevice::ReadOnly)?transferRows(table.readAll()):std::vector<ResistanceRow>{};
    QJsonObject state{{"version",1},{"method",method},{"available",orderedTransfer(existing)}};
    if(!state["available"].toBool()){
        auto rows=accelerationRows;
        // The original external series also publish their evaluated speed rows
        // in the longer propulsion table, with the same five columns.
        if(rows.empty()&&(method=="ost"||method=="rbhs"||method=="mh")){
            QFile full(work+"/RESISTp.dat");
            if(full.open(QIODevice::ReadOnly)){
                const auto header=full.readLine();
                full.readLine();
                const auto columns=full.readLine();
                if(header.startsWith('#')&&columns.startsWith('#'))rows=transferRows(full.readAll());
            }
        }
        std::vector<ResistanceRow> distinct;
        bool incompatible=false;
        for(const auto& row:rows){
            if(!transferable(row)){incompatible=true;break;}
            if(!distinct.empty()&&float(row[0])<=float(distinct.back()[0])){
                if(row==distinct.back())continue;
                incompatible=true;break;
            }
            distinct.push_back(row);
        }
        if(!incompatible&&distinct.size()>=5){
            QStringList lines;
            for(size_t i=0;i<5;++i){
                QStringList cells;
                for(double value:distinct[i*(distinct.size()-1)/4])cells<<QString::number(value,'g',17);
                lines<<cells.join(' ');
            }
            const auto bytes=(lines.join("\r\n")+"\r\n").toLatin1();
            saveBytes(work+"/RESIST-acceleration.dat",bytes);
            state["available"]=true;
            state["separate_table"]=true;
            state["speed_max_kn"]=distinct.back()[0];
            state["resist_sha256"]=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        }else{
            const auto reason=existing.size()==5||incompatible
                ?"本次阻力结果无法提供五个递增的有效航速点。请扩大起始、设计和最大航速之间的间隔，或更换适用的阻力方法。"
                :"本次阻力结果未提供加速与制动所需的五个有效航速点，请检查航速设置和方法适用范围。";
            state["reason"]=reason;
            warnings<<reason;
        }
    }
    saveBytes(statePath,QJsonDocument(state).toJson());
}

void ResistanceBase::writeAccelerationSamples(bool separate,double margin){
    // Force is captured before display scaling: knots and kN, never rounded table text.
    QJsonArray valid;QString reason;
    for(auto value:accelerationSamples){auto p=value.toArray();double v=p[0].toDouble(),force=p[1].toDouble();
        if(v==0)continue;
        if(!std::isfinite(v)||!std::isfinite(force)||v<0||force<0){reason="阻力曲线含负值或无效数值，不能用于加速计算。";break;}
        if(!valid.isEmpty()&&float(v)<=float(valid.last().toArray()[0].toDouble())){reason="阻力曲线航速重复或倒序，不能用于加速计算。";break;}
        valid.append(p);
    }
    if(reason.isEmpty()&&valid.size()<5)reason="加速计算至少需要五个不同的正航速点，请扩大有效航速范围或减小步长。";
    if(accelerationGap)reason="Delft 航速范围跨越公式未覆盖区间，请缩小航速范围后重新计算。";
    QJsonObject state{{"version",1},{"method",method},{"available",reason.isEmpty()},{"manual_propulsion",true}};
    if(!reason.isEmpty()){state["reason"]=reason;warnings<<reason;}
    else{
        QStringList rows;QJsonArray selected;
        for(int i=0;i<5;++i){const auto p=valid[i*(valid.size()-1)/4].toArray();double v=p[0].toDouble(),force=p[1].toDouble(),power=force*v*1852./3600.;
            QStringList cells;for(double x:{v,force,force*margin,power,power*margin})cells<<QString::number(x,'g',17);rows<<cells.join(' ');selected.append(p);}
        const auto bytes=(rows.join("\r\n")+"\r\n").toLatin1();
        saveBytes(work+(separate?"/RESIST-acceleration.dat":"/RESIST.dat"),bytes);
        if(separate)state["separate_table"]=true;
        state["resist_sha256"]=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        state["selected_kn_kN"]=selected;state["speed_min_kn"]=valid.first().toArray()[0];state["speed_max_kn"]=valid.last().toArray()[0];
    }
    saveBytes(work+"/RESIST-acceleration.json",QJsonDocument(state).toJson());
}

void ResistanceBase::writeNativeResistanceCache(const QString& methodName,const QJsonArray& rows,double count,double density,double diameter,double wake,double deduction,double etaR){
    if(rows.size()!=9)bad("原生阻力衔接表必须有 9 行。");
    QStringList fullRows,selected,headerValues;
    for(int i=0;i<9;++i){const auto row=rows[i].toArray();if(row.size()!=5)bad("原生阻力衔接表列数无效。");QStringList cells;
        for(auto v:row){if(!v.isDouble()||!std::isfinite(v.toDouble()))bad("原生阻力衔接表数值无效。");cells<<QString::number(v.toDouble(),'g',17);}
        accelerationRows.push_back({row[0].toDouble(),row[1].toDouble(),row[2].toDouble(),
                                    row[3].toDouble(),row[4].toDouble()});
        fullRows<<cells.join(' ');if(i==0||i==2||i==4||i==7||i==8)selected<<cells.join(' ');}
    for(double value:{53.,count,wake,deduction,etaR,diameter})headerValues<<QString::number(value,'g',17);
    const QByteArray full=("# FreeShip26-native-"+methodName+"-v1 Nser Np w t etaR Dp\r\n"+headerValues.join(' ')+"\r\n# Vs Rt Rte Pe Pee\r\n"+fullRows.join("\r\n")+"\r\n").toLatin1();
    const QByteArray shortTable=(selected.join("\r\n")+"\r\n").toLatin1();const auto design=rows[7].toArray();
    QJsonObject task1{{"Edit2",design[0]},{"Edit3",design[2]},{"Edit4",count},{"Edit5",diameter},
        {"Edit7",density},{"Edit8",wake},{"Edit9",deduction},{"Edit10",etaR}};
    QJsonObject task2{{"Edit4",count},{"Edit5",density},{"Edit6",wake},{"Edit7",deduction},
        {"Edit8",etaR},{"Edit16",design[0]},{"DesignDiameterLimit",diameter}};
    for(int i=0;i<5;++i){const auto row=rows[i==4?8:2*i+1].toArray();task2[QString("Edit17_%1").arg(i+1)]=row[0];task2[QString("Edit18_%1").arg(i+1)]=row[2];}
    QJsonObject payload{{"task1",task1},{"task2",task2}};
    auto hash=[](const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());};
    QJsonObject metadata{{"version",1},{"payload",payload},{"payload_sha256",hash(QJsonDocument(payload).toJson(QJsonDocument::Compact))},
        {"resist_sha256",hash(shortTable)},{"resistp_sha256",hash(full)}};
    saveBytes(work+"/RESIST.dat",shortTable);saveBytes(work+"/RESISTp.dat",full);saveBytes(work+"/RESIST-native.json",QJsonDocument(metadata).toJson());
}
