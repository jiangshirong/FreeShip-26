// SPDX-License-Identifier: GPL-2.0-or-later
// Record layouts: Plus FreeEdit.inc File_ExportArchimedes / File_ExportPAM.
// Archimedes axes, metric units and draught-mark semantics: MacSween 2006 manual.
#include "ship_formats.h"
#include "hull_sections.h"
#include <QJsonArray>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <cmath>
namespace fs::ship {
namespace {
[[noreturn]] void bad(const QString& s){throw std::runtime_error(s.toStdString());}
QString n(double v){if(!std::isfinite(v))bad("船体输出包含无效数值。");return QString::number(v==0?0:v,'g',17);}
double reference(const QJsonObject& refs,const QString& key,double fallback){
    if(!refs.contains(key))return fallback;
    auto v=refs[key];if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>1e9)bad("参考位置无效："+key);
    return v.toDouble();
}
QString plainTitle(const QString& title){
    auto text=title.toLatin1();for(char& c:text)if(uchar(c)<32||uchar(c)>126)c='?';
    return text.isEmpty()?QString("FreeShip_26 hull"):QString::fromLatin1(text);
}
QString xmlTitle(const QString& title){
    // XML 1.0 cannot encode arbitrary control characters even as references.
    for(auto c:title.toUcs4())if(!(c==9||c==10||c==13||(c>=32&&c<=0xd7ff)||(c>=0xe000&&c<=0xfffd)||(c>=0x10000&&c<=0x10ffff)))bad("工程名称包含 XML 不支持的控制字符。");
    return title.isEmpty()?QString("FreeShip_26 hull"):title;
}
std::vector<hull::Point> fullContour(const hull::Station& station,double scale){
    auto half=station.points;
    // A modeled centerplane face is internal after reflection. Remove only its
    // final downward CL run; retain the top centerline vertex as the deck seam.
    while(half.size()>2&&half.back().y==0&&half[half.size()-2].y==0&&half.back().z<half[half.size()-2].z)half.pop_back();
    std::vector<hull::Point> out;out.reserve(2*half.size()-1);
    for(auto i=half.rbegin();i!=half.rend();++i)out.push_back({-i->y*scale,i->z*scale});
    for(size_t i=1;i<half.size();++i)out.push_back({half[i].y*scale,half[i].z*scale});
    return out;
}
}
ghs::Result build(const native_geometry::DocumentInput& input,const ModelSnapshot& m,
                  const QString& format,const QString& units,bool closeDeck,const QJsonObject& refs){
    if(format!="arch_hll"&&format!="arch_app"&&format!="pam")bad("未知的船舶输出格式。");
    if(units!="m")bad("该格式使用米，请转换到米后导出。");
    const double scale=input.imperial?.3048:1.;
    const double ap=reference(refs,"ap",0),fp=reference(refs,"fp",m.length*scale);
    const double aft=reference(refs,"aft_mark",ap),mid=reference(refs,"mid_mark",(ap+fp)/2),fwd=reference(refs,"fwd_mark",fp);
    const double midship=reference(refs,"midship",m.length*scale/2),waterline=reference(refs,"waterline",m.waterlineZ*scale);
    if(format!="pam"&&ap>=fp)bad("艉垂线位置必须小于艏垂线位置。");
    auto hull=hull::build(input,m,closeDeck);auto& stations=hull.stations;QByteArray data;quint64 pointCount=0;
    QJsonObject referenceReport;
    if(format=="arch_hll"){
        QXmlStreamWriter s(&data);s.setAutoFormatting(true);s.writeStartDocument();s.writeStartElement("HULL");
        auto field=[&](const char* name,double value){s.writeTextElement(QLatin1StringView(name),n(value));};
        s.writeTextElement("HULL_NAME",xmlTitle(m.name));field("DENSITY",hull.density);field("AP",ap);field("FP",fp);
        field("FWD_DRAUGHT_MARK",fwd);field("MID_DRAUGHT_MARK",mid);field("AFT_DRAUGHT_MARK",aft);
        field("APPENDAGE_COUNT",1);s.writeStartElement("APPENDAGE");s.writeTextElement("APPENDAGE_NAME","Main Hull");
        field("ACTIVE",1);field("FREE_FLOODING",0);field("APPENDAGE_COLOUR",0);field("SECTION_COUNT",stations.size());
        for(const auto& station:stations){auto points=fullContour(station,scale);pointCount+=points.size();
            s.writeStartElement("SECTION");s.writeTextElement("SECTION_NAME",n(station.x*scale));field("YCOORD",station.x*scale);field("OFFSET_COUNT",points.size());
            s.writeStartElement("OFFSETS");s.writeCharacters("\n");for(auto p:points)s.writeCharacters(n(p.y)+" "+n(p.z)+"\n");s.writeEndElement();s.writeEndElement();
        }
        s.writeEndElement();s.writeEndElement();s.writeEndDocument();if(s.hasError())bad("无法生成 Archimedes XML。");
        referenceReport={{"ap",ap},{"fp",fp},{"aft_mark",aft},{"mid_mark",mid},{"fwd_mark",fwd}};
    }else{
        QTextStream s(&data);s.setEncoding(QStringConverter::Latin1);
        if(format=="arch_app"){
            s<<"***************************************************************\r\n** Generated with FreeShip_26; metres, tonnes\r\n***************************************************************\r\n";
            s<<"APPENDAGE_NAME\r\n"<<plainTitle(m.name).left(58)<<"\r\nAP\r\n"<<n(ap)<<"\r\nFP\r\n"<<n(fp)<<"\r\nDENSITY\r\n"<<n(hull.density)<<"\r\nNUMBER_OF_FRAMES\r\n"<<stations.size()<<"\r\n***START_OF_FRAME_INFORMATION***\r\n";
            size_t index=0;for(const auto& station:stations){auto points=fullContour(station,scale);pointCount+=points.size();
                s<<"FRAME_INDEX: "<<index++<<"\r\nFRAME_NAME\r\nFrame @ Y= "<<n(station.x*scale)<<"\r\nFRAME_Y\r\n"<<n(station.x*scale)<<"\r\nNUMBER_OF_COORDINATES\r\n"<<points.size()<<"\r\n***START_OF_ORDINATES***\r\n";
                for(auto p:points)s<<n(p.y)<<"\r\n"<<n(p.z)<<"\r\n";
                s<<"***END_OF_ORDINATES***\r\n";
            }s<<"***END_OF_FILE***\r\n";referenceReport={{"ap",ap},{"fp",fp}};
        }else{
            // This is the old Windows PAM template, not the separate INA. input
            // protocol of Add_Mass.EXE. Retain opaque solver defaults verbatim.
            s<<plainTitle(m.name+" L="+n(m.length*scale)+" B="+n(m.beam*scale)+" T="+n(m.draft*scale)).left(58)<<"\r\n";
            s<<n(hull.density*1000)<<" 1600 100 .2 2. 1000 0\r\n0 0 0 0 0 0 0 0 0 0\r\n"<<stations.size()<<" 1. 0.\r\n";
            for(auto it=stations.rbegin();it!=stations.rend();++it){pointCount+=it->points.size();s<<n(midship-it->x*scale)<<' '<<it->points.size()<<"\r\n";
                for(auto p:it->points)s<<n(p.z*scale-waterline)<<' '<<n(p.y*scale)<<"\r\n";
            }s<<"\\*\r\n";referenceReport={{"midship",midship},{"waterline",waterline}};
        }s.flush();
    }
    const QJsonValue halfPointCount=hull.diagnostics.value("points");
    hull.diagnostics.insert("half_section_points",halfPointCount);hull.diagnostics["points"]=double(pointCount);
    hull.diagnostics["units"]="m";hull.diagnostics["format"]=format;hull.diagnostics["references_m"]=referenceReport;
    return {data,hull.diagnostics};
}
}
