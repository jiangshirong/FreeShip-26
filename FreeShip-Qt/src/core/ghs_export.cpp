// SPDX-License-Identifier: GPL-2.0-or-later
// GF layout follows FREE!ship Plus File_ExportGHS; common native sections.
#include "ghs_export.h"
#include "hull_sections.h"
#include <QTextStream>
#include <cmath>
namespace fs::ghs {
namespace {
QString n(double v){if(!std::isfinite(v))throw std::runtime_error("Invalid GHS value");return QString::number(v==0?0:v,'g',17);}
}
std::vector<std::array<double,2>> sampleSection(const native_geometry::SectionCurve& c,double tolerance){return hull::sampleSection(c,tolerance);}
Result build(const native_geometry::DocumentInput& input,const ModelSnapshot& m,const QString& units,bool closeDeck){
    if(units!="m"&&units!="ft")throw std::runtime_error("GHS 只支持米或英尺。");
    auto sections=hull::build(input,m,closeDeck);const double scale=sections.metersPerUnit/(units=="ft"?.3048:1.),density=sections.density;
    auto& stations=sections.stations;
    QByteArray data;QTextStream s(&data);s.setEncoding(QStringConverter::Latin1);
    auto title=m.name.toLatin1();for(char& c:title)if(uchar(c)<32||uchar(c)>126)c='?';if(title.isEmpty())title="FreeShip_26 hull";
    s<<title.left(58)<<"\r\nL:"<<n(m.length*scale)<<"\r\nW:"<<n(m.beam*scale)<<"\r\nOL:FreeShip X=0\r\nOT:Centerplane\r\nOV:FreeShip Z=0\r\nP:"<<(units=="m"?'M':'F')<<"\r\n";
    s<<"\\Generated with FreeShip_26\r\n\\Existing stations; symmetric hydrostatics layers only\r\n\\L=-X; T=abs(Y); V=Z; original origin preserved\r\n*\r\nHULL\r\n"<<stations.size()<<"\r\n";
    for(auto it=stations.rbegin();it!=stations.rend();++it){const auto& st=*it;s<<n(-st.x*scale)<<','<<st.points.size()<<"\r\n";for(auto p:st.points)s<<n(p.y*scale)<<','<<n(p.z*scale)<<"\r\n";}
    s<<"0,0,0\r\n**\r\nHULL.C\r\n0\r\n1.0\r\n0,0,0\r\nHULL\r\n***\r\nHULL\r\nWATER\r\n1\r\n"<<n(density)<<"\r\n0,0,0\r\n1\r\nHULL.C\r\n****\r\n";s.flush();
    sections.diagnostics["units"]=units;return {data,sections.diagnostics};
}
}
