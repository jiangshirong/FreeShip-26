// GPL-2.0-or-later. Michlet/AddMass record order follows the current Plus
// FreeMichletOutputDlg.SaveToFile and FreeAddMassOutputDlg.SaveToFile.
// Martijn van Engeland, Timoshenko V.F., Mark Malakanov; FreeShip_26 adaptation.
#include "grid_export.h"
#include <QTextStream>
#include <QIODevice>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <map>
namespace fs::grid {
namespace {
namespace ng=fs::native_geometry;
[[noreturn]] void bad(const QString& s){throw std::runtime_error(s.toStdString());}
QString n(double v){if(!std::isfinite(v))bad("输出包含无效数值。");return QString::number(v==0?0:v,'g',17);}
double option(const QJsonObject& o,const QString& key,double fallback,double lo,double hi){auto v=o.value(key);double x=v.isUndefined()?fallback:v.toDouble(std::numeric_limits<double>::quiet_NaN());if(!std::isfinite(x)||x<lo||x>hi)bad("参数超出范围："+key);return x;}
int count(const QJsonObject& o,const QString& key,int fallback,int hi){double x=option(o,key,fallback,5,hi);if(x!=std::floor(x)||int(x)%2!=1)bad("站数和水线数必须是范围内的奇数。");return int(x);}
int samples(const QJsonObject& o,const QString& key){double x=option(o,key,100,2,10000);if(x!=std::floor(x))bad("波形采样数量必须为整数。");return int(x);}

// Michlet represents each demihull by a symmetric half-breadth table. Check
// the actual subdivision mesh before discarding the inner side of a catamaran.
// The tolerance only recognizes Single roundoff; no coordinates are changed.
void validateDemihull(const ng::SubdivisionLevel& mesh,double centre){
    using Cell=std::array<long long,3>;
    std::set<size_t> used;
    std::array<double,3> tolerance{1,std::max(1.,std::abs(centre)),1};
    for(const auto& face:mesh.faces)if(mesh.layers.at(face.layer).useInHydrostatics)
        for(auto id:face.points){used.insert(id);auto p=mesh.points.at(id).position;
            tolerance[0]=std::max(tolerance[0],std::abs(double(p.x)));
            tolerance[1]=std::max(tolerance[1],std::abs(double(p.y)));
            tolerance[2]=std::max(tolerance[2],std::abs(double(p.z)));}
    for(auto& value:tolerance)value*=4*std::numeric_limits<float>::epsilon();
    auto cell=[&](std::array<double,3> p){return Cell{(long long)std::floor(p[0]/tolerance[0]),(long long)std::floor(p[1]/tolerance[1]),(long long)std::floor(p[2]/tolerance[2])};};
    std::map<Cell,std::vector<size_t>> buckets;
    for(auto id:used){auto p=mesh.points[id].position;buckets[cell({p.x,p.y,p.z})].push_back(id);}
    std::map<size_t,size_t> mirror;
    for(auto id:used){auto p=mesh.points[id].position;std::array<double,3> reflected{p.x,2*centre-p.y,p.z};auto key=cell(reflected);size_t match=0;int found=0;
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){auto it=buckets.find({key[0]+x,key[1]+y,key[2]+z});if(it==buckets.end())continue;
            if(it->second.size()>64)bad("片体含密集重合点，不能确认自身对称性。");
            for(auto other:it->second){auto q=mesh.points[other].position;
                if(std::abs(q.x-reflected[0])<=tolerance[0]&&std::abs(q.y-reflected[1])<=tolerance[1]&&std::abs(q.z-reflected[2])<=tolerance[2]){match=other;++found;}}}
        if(found!=1)bad("片体不关于所填中心面对称，或存在重合点；不能用半宽表代替原曲面。");
        mirror[id]=match;
    }
    for(auto [id,other]:mirror)if(mirror.at(other)!=id)bad("片体镜像对应不唯一。");
    auto canonical=[](std::vector<size_t> ids){std::rotate(ids.begin(),std::min_element(ids.begin(),ids.end()),ids.end());return ids;};
    std::set<std::vector<size_t>> faces;
    for(const auto& face:mesh.faces)if(mesh.layers.at(face.layer).useInHydrostatics)faces.insert(canonical(face.points));
    for(const auto& face:mesh.faces)if(mesh.layers.at(face.layer).useInHydrostatics){auto ids=face.points;for(auto& id:ids)id=mirror.at(id);std::reverse(ids.begin(),ids.end());
        if(!faces.count(canonical(ids)))bad("片体两侧的面连接或朝向不对称。");}
    std::map<std::pair<size_t,size_t>,bool> edges;
    for(const auto& edge:mesh.edges)if(used.count(edge.points[0])&&used.count(edge.points[1]))edges[std::minmax(edge.points[0],edge.points[1])]=edge.crease;
    for(const auto& [ends,crease]:edges){auto it=edges.find(std::minmax(mirror.at(ends.first),mirror.at(ends.second)));if(it==edges.end()||it->second!=crease)bad("片体两侧折痕不对称。");}
}
double unit(QString u){if(u=="m")return 1;if(u=="mm")return .001;if(u=="cm")return .01;if(u=="ft")return .3048;if(u=="in")return .0254;bad("长度单位不支持。");}
std::vector<double> breadths(const std::vector<ng::SectionCurve>& curves,float z){
    std::vector<double> ys;
    for(const auto& c:curves){ng::Spline spline(c.points,c.knuckles);const auto& ts=spline.parameters();const auto& ds=spline.secondDerivatives();
        // Solve each cubic polynomial, split at its derivative's roots. This
        // finds tangencies and multiple crossings without fixed-step sampling.
        for(size_t i=1;i<c.points.size();++i){auto a=c.points[i-1],b=c.points[i];double h=float(ts[i]-ts[i-1]);if(h<1e-6){if(b.z==z)ys.push_back(b.y);continue;}
            const double h2=float(float(h)*float(h)/6.f);auto coefficients=[&](double p,double q,double d0,double d1){if(c.knuckles[i-1]&&c.knuckles[i])d0=d1=0;return std::array<double,4>{p,q-p+h2*(-2*d0-d1),3*h2*d0,h2*(-d0+d1)};};
            auto cz=coefficients(a.z,b.z,ds[i-1].z,ds[i].z),cy=coefficients(a.y,b.y,ds[i-1].y,ds[i].y);cz[0]-=z;
            auto eval=[](const auto& c,double t){return ((c[3]*t+c[2])*t+c[1])*t+c[0];};
            auto critical=[](const auto& c){std::vector<double> t{0,1};double a=3*c[3],b=2*c[2],v=c[1];if(a==0){if(b!=0){double r=-v/b;if(r>0&&r<1)t.push_back(r);}}else{double disc=b*b-4*a*v;if(disc>=0){double q=-.5*(b+std::copysign(std::sqrt(disc),b));for(double r:{q/a,q==0?-b/(2*a):v/q})if(r>0&&r<1)t.push_back(r);}}std::sort(t.begin(),t.end());return t;};
            if(cz==std::array<double,4>{0,0,0,0}){for(auto t:critical(cy))ys.push_back(eval(cy,t));continue;}
            auto splits=critical(cz);const double eps=std::numeric_limits<double>::epsilon()*32*std::max({1.,std::abs(double(z)),std::abs(double(a.z)),std::abs(double(b.z))});
            for(auto t:splits)if(std::abs(eval(cz,t))<=eps)ys.push_back(eval(cy,t));
            for(size_t j=1;j<splits.size();++j){double lo=splits[j-1],hi=splits[j],f=eval(cz,lo),g=eval(cz,hi);if(f==0||g==0||std::signbit(f)==std::signbit(g))continue;
                for(int k=0;k<60;++k){double mid=(lo+hi)/2,v=eval(cz,mid);if(v==0){lo=hi=mid;break;}if(std::signbit(v)==std::signbit(f)){lo=mid;f=v;}else hi=mid;}ys.push_back(eval(cy,(lo+hi)/2));}
        }
    }
    // The native spline's coordinates/derivatives are Singles. Publish roots
    // at that same coordinate precision, so the two polynomial spans meeting
    // at a tangent do not invent several ordinates below one representable ULP.
    for(auto& y:ys)y=double(float(y));
    std::sort(ys.begin(),ys.end());ys.erase(std::unique(ys.begin(),ys.end()),ys.end());return ys;
}
}
std::vector<double> sectionBreadths(const std::vector<ng::SectionCurve>& curves,float z){return breadths(curves,z);}
ghs::Result build(const ng::DocumentInput& input,const ModelSnapshot& m,const QString& format,const QString& units,const QJsonObject& o,QJsonObject* preview){
    if(input.documentId!=m.documentId||input.revision!=m.revision||input.imperial!=(m.units=="imperial"))bad("导出几何与当前工程版本不一致。");
    const double metres=input.imperial?.3048:1.,scale=metres/unit(units);
    ng::SubdivisionSurface surface(input.geometry);const auto& mesh=surface.level(input.geometry.desiredLevel);ng::SectionIndex index(surface.input(),mesh);
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());QJsonObject diagnostics;int points=0,stations=0;
    if(format=="dxfcrease"){
        auto pair=[&](int c,const QString& v){out<<c<<'\n'<<v<<'\n';};pair(0,"SECTION");pair(2,"HEADER");pair(9,"$ACADVER");pair(1,"AC1018");pair(9,"$INSUNITS");pair(70,QString::number(units=="m"?6:units=="mm"?4:units=="cm"?5:units=="ft"?2:1));pair(0,"ENDSEC");pair(0,"SECTION");pair(2,"ENTITIES");
        for(const auto& edge:mesh.edges)if(edge.crease){const ng::LayerInput* l=nullptr;size_t layer=0;for(auto f:edge.faces){auto li=mesh.faces.at(f).layer;const auto& candidate=input.geometry.layers.at(li);if(o["include_hidden"].toBool()||candidate.surfaceVisible){l=&candidate;layer=li;break;}}if(!l)continue;
            auto a=mesh.points.at(edge.points[0]).position,b=mesh.points.at(edge.points[1]).position;
            for(int side=0;side<(o["full_hull"].toBool(true)&&l->symmetric&&(a.y!=0||b.y!=0)?2:1);++side){pair(0,"LINE");pair(100,"AcDbEntity");pair(8,"Crease_"+QString::number(layer+1));pair(420,QString::number(((l->color&255)<<16)|(l->color&0xff00)|((l->color>>16)&255)));pair(100,"AcDbLine");for(int j=0;j<2;++j){auto p=j?b:a;pair(10+j,n(p.x*scale));pair(20+j,n(p.y*scale*(side?-1:1)));pair(30+j,n(p.z*scale));}points+=2;}
        }if(!points)bad("所选图层没有折痕或边界线。");pair(0,"ENDSEC");pair(0,"EOF");
    }else if(format=="offset_table"){
        std::vector<float> xs,zs;for(const auto& s:input.sections){auto p=s.plane.coefficients();if(s.kind==1&&p[0]==1&&p[1]==0&&p[2]==0)xs.push_back(-p[3]);if(s.kind==3&&p[0]==0&&p[1]==0&&p[2]==1)zs.push_back(-p[3]);}
        std::sort(xs.begin(),xs.end());std::sort(zs.begin(),zs.end());if(xs.empty()||zs.empty())bad("请先定义横剖站和水线。");if(xs.size()*zs.size()>1000000)bad("型值表超过一百万个单元格。");
        out<<"# FreeShip_26 XYZ; units="<<units<<"; empty=no intersection; semicolon=multiple Y ordinates\nX/Z";for(auto z:zs)out<<','<<n(z*scale);out<<'\n';
        for(auto x:xs){auto curves=index.intersect(ng::Plane(1,0,0,-x)).curves;out<<n(x*scale);for(auto z:zs){out<<',';auto ys=breadths(curves,z);for(size_t i=0;i<ys.size();++i){if(i)out<<';';out<<n(ys[i]*scale);++points;}}out<<'\n';}stations=xs.size();
    }else if(format=="michlet"||format=="addmass"){
        if(units!="m")bad("该计算输入文件必须使用米。");if(!m.particularsSet||m.draft<=0)bad("请先填写有效的主尺度和设计吃水。");
        const QString mode=o.contains("hull_mode")?o["hull_mode"].toString():QString("monohull");
        if(mode!="monohull"&&mode!="duplicate"&&mode!="catamaran")bad("未知船体布置方式。");
        if(format=="addmass"&&mode!="monohull")bad("AddMass 的此文件协议只包含一份型值表，不能输出双体布置。");
        const int hulls=mode=="monohull"?1:2;
        const double separation=hulls==2?option(o,"separation",0,1e-6,1e9):0;
        const double centre=mode=="catamaran"?double(float(.5*separation/metres)):0;
        for(const auto& f:input.geometry.faces){const auto& l=input.geometry.layers.at(f.layer);if(!l.useInHydrostatics)continue;if(!l.symmetric)bad("当前入口要求对称单体半船；不能用单个半宽表表示非对称或多体船。");for(auto p:f.points)if(input.geometry.points.at(p).position.y<0)bad("当前入口要求 Y≥0 的半船控制网。");}
        const int nx=count(o,"stations",format=="michlet"?31:21,format=="michlet"?81:45),nz=count(o,"waterlines",format=="michlet"?21:11,format=="michlet"?81:25);
        double bottom=std::numeric_limits<double>::infinity();for(const auto& t:m.triangles)if(m.layer(t.layer)->useInHydrostatics)for(auto p:t.vertices)bottom=std::min(bottom,p.z);
        if(!std::isfinite(bottom))bad("没有参与静水力的曲面。");const double draft=option(o,"draft",m.draft*metres,1e-6,1e7)/metres,wl=bottom+draft;
        if(mode=="catamaran")validateDemihull(mesh,centre);
        double xmin=std::numeric_limits<double>::infinity(),xmax=-xmin,beam=0,volume=0,ymin=xmin;
        for(const auto& t:m.triangles)if(m.layer(t.layer)->useInHydrostatics){std::vector<Vec3> polygon;for(int i=0;i<3;++i){auto a=t.vertices[i],b=t.vertices[(i+1)%3];if(a.z<=wl)polygon.push_back(a);if((a.z<wl&&b.z>wl)||(a.z>wl&&b.z<wl)){double f=(wl-a.z)/(b.z-a.z);polygon.push_back({a.x+f*(b.x-a.x),a.y+f*(b.y-a.y),wl});}}
            for(auto p:polygon){xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);ymin=std::min(ymin,p.y);beam=std::max(beam,2*(p.y-centre));}
            for(size_t i=2;i<polygon.size();++i){auto a=polygon[0],b=polygon[i-1],c=polygon[i];a.z-=wl;b.z-=wl;c.z-=wl;volume+=(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/3.;}
        }
        if(!std::isfinite(xmin)||xmax<=xmin||beam<=0||std::abs(volume)<=1e-12)bad("水下船体没有有效长度、宽度或排水体积。");
        if(hulls==2&&separation<=beam*metres)bad("片体中心面间距须大于片体水下宽度，避免相交或接触。");
        if(mode=="catamaran"&&ymin<=0)bad("双体模型须在 Y>0 保存一个完整片体，由图层对称生成另一片体。");
        for(const auto& edge:mesh.edges){int hydro=0;for(auto f:edge.faces)hydro+=input.geometry.layers[mesh.faces[f].layer].useInHydrostatics;if(hydro!=1)continue;auto a=mesh.points[edge.points[0]].position,b=mesh.points[edge.points[1]].position;
            if(std::min(a.z,b.z)<wl-1e-6&&(a.y!=0||b.y!=0))bad("水下船体存在敞开边，不能生成可靠的计算输入。");}
        std::vector<std::vector<double>> offsets;for(int i=0;i<nx;++i){float x=float(xmax-double(i)*(xmax-xmin)/(nx-1));auto curves=index.intersect(ng::Plane(1,0,0,-x),{true}).curves;std::vector<double> row;
            for(int j=0;j<nz;++j){float z=float(bottom+double(j)*draft/(nz-1));auto ys=breadths(curves,z);double y=0;for(auto value:ys){if(value<-1e-6)bad("断面曲线越过中心线。");if(mode=="catamaran")value=std::abs(value-centre);if(j>0&&value>1e-6&&y>1e-6&&std::abs(y-value)>1e-6*std::max({1.,value,centre}))bad("水线与横剖面有多个正半宽交点，不能用单值型值表表示。");y=std::max(y,value);}row.push_back(y*metres);}offsets.push_back(std::move(row));}
        const double length=(xmax-xmin)*metres,T=draft*metres,V=std::abs(volume)*metres*metres*metres/(mode=="catamaran"?2:1);
        const double density=(input.imperial?m.waterDensity/(1000*.3048*.3048*.3048/.45359237):m.waterDensity)*1000;
        const double depth=option(o,"depth",10000,T,10000);auto field=[&](const QString& label,const QString& value){out<<"# "<<label<<'\n'<<value<<'\n';};
        auto rows=[&](QChar separator){for(const auto& row:offsets){for(size_t j=0;j<row.size();++j){if(j)out<<separator;out<<n(row[j]);}out<<'\n';}};
        if(format=="michlet"){
            // Michlet 9.33 manual sections 8/8.1: a nonzero first row is parsed
            // as a mathematical hull-series selector, not as a blunt bow.
            if(std::any_of(offsets.front().begin(),offsets.front().end(),[](double y){return y!=0;}))bad("Michlet 型值表要求艏端整行半宽为零；当前平艏不能直接表示，未改写船型。");
            const double speed0=option(o,"start_speed",1,.01,39.9),speed1=option(o,"end_speed",5,.01,40),ns=option(o,"speeds",50,2,50);if(speed1<=speed0||ns!=std::floor(ns))bad("速度区间或速度数量无效。");
            if(density<995||density>1030)bad("Michlet 水密度须在 995–1030 kg/m³ 范围。");
            for(auto label:{"Input type","Input subtype","Output type","Output subtype","Course"})field(label,"0");field("Number of hulls",n(hulls));field("Gravity",n(option(o,"gravity",9.80665,9.6,9.9)));field("Density kg/m3",n(density));field("Kinematic viscosity x 10^-6",n(option(o,"viscosity",1.18831,.8,1.31)));field("Eddy viscosity","10");field("Water depth",n(depth));
            field("Air density","1.26");field("Air viscosity","14.4");field("Wind speed","0");field("Wind direction","0");field("Minimum speed m/s",n(speed0));field("Maximum speed m/s",n(speed1));field("Number of speeds",n(ns));field("Leeway","0");field("Wave drag Ntheta","1024");
            double friction=option(o,"friction",1,0,2),form=option(o,"form_factor",0,0,3);if(friction!=std::floor(friction)||(form!=0&&form!=3))bad("阻力方法参数无效。");field("Skin friction method",n(friction));field("Form factor method",n(form));field("Viscous form factor","1");field("Wave form factor","1");field("Pressure method","1");field("Stations",n(nx));field("Waterlines",n(nz));field("Loading type","3");field("Loading parameters","1.0,0.0,0.0");
            double r0=option(o,"r0",1.5*length,0,1e9),r1=option(o,"r1",5*length,r0,1e9);if(r1==r0)bad("波形半径区间不能为零。");
            int nr=samples(o,"nr"),nb=samples(o,"nbeta"),wx=samples(o,"wave_nx"),wy=samples(o,"wave_ny");if(qint64(nr)*nb>1000000||qint64(wx)*wy>1000000)bad("每个波形区域最多一百万个采样点。");
            double x0=option(o,"wave_x0",1.5*length,-1e9,1e9),x1=option(o,"wave_x1",5*length,x0,1e9);
            double y0=option(o,"wave_y0",-5*length*std::tan(std::acos(-1.)/6),-1e9,1e9),y1=option(o,"wave_y1",5*length*std::tan(std::acos(-1.)/6),y0,1e9);
            if(x1==x0||y1==y0)bad("矩形波形区域的长度和宽度必须大于零。");
            field("R0",n(r0));field("R1",n(r1));field("Beta",n(option(o,"beta",22.5,.01,90)));field("Nr",n(nr));field("Nbeta",n(nb));
            field("Wave x0",n(x0));field("Wave x1",n(x1));field("Wave y0",n(y0));field("Wave y1",n(y1));field("Nwx",n(wx));field("Nwy",n(wy));
            for(auto value:{"200","2200","500","-1","0","90","11","11"})field("Beach/wall parameter",value);
            for(int hull=0;hull<hulls;++hull){
                out<<"# Hull offsets, bow to stern, bottom to waterline, metres\n";rows(',');field("Volume",n(V));field("Length",n(length));field("Draft",n(T));
                // The first hull defines the origin; all following positions
                // are relative to it (manual 3.1 and 7.13.2).
                field("Longitudinal separation","0");field("Lateral separation",n(hull?separation:0));field("Loading","3");field("Loading parameters","1.0,0.0,0.0");
                for(auto label:{"Trim","Sinkage","Heel"}){field(QString(label)+" method",QString(label)=="Heel"?"0":"3");field("Number of speeds","2");field("Speed,value","0.0,0.0");out<<"10.0,0.0\n";}field("Appendages","0");field("Other particulars","0");
            }
        }else{
            QString title=m.name;title.replace('\n',' ');title.replace('\r',' ');out<<title<<" L="<<n(length)<<" B="<<n(beam*metres)<<" T="<<n(T)<<'\n';
            double panels=option(o,"panels",1000,400,4000),panelType=option(o,"panel_type",1,1,2),objectType=option(o,"object_type",1,1,30);
            if(panels!=std::floor(panels)||panelType!=std::floor(panelType)||objectType!=std::floor(objectType))bad("面元数量及类型必须是整数。");
            out<<n(density)<<' '<<n(panels)<<' '<<n(option(o,"area_percent",100,10,100))<<' '<<n(option(o,"accuracy",.2,1e-6,1))<<' '<<n(panelType)<<' '<<n(depth)<<' '<<n(option(o,"drift",0,-90,90))<<' '<<n(option(o,"velocity_knots",10,0,30))<<' '<<n(option(o,"omega",0,-30,30))<<' '<<n(objectType)<<'\n';
            out<<"0 0 0 0 0 0 0 0 0 0\n"<<n(length)<<' '<<n(T)<<'\n'<<nx<<" 1. 0.\n"<<nz<<'\n';rows(' ');out<<"\\*\n";
        }
        points=nx*nz*hulls;stations=nx;diagnostics["hulls"]=hulls;diagnostics["hull_mode"]=mode;diagnostics["separation_m"]=separation;diagnostics["demihull_centre_model_m"]=centre*metres;diagnostics["total_volume_m3"]=V*hulls;
        diagnostics["length_m"]=length;diagnostics["draft_m"]=T;diagnostics["volume_m3"]=V;diagnostics["x_min_m"]=xmin*metres;diagnostics["x_max_m"]=xmax*metres;diagnostics["waterline_z_m"]=wl*metres;diagnostics["scope"]="symmetric hulls; volume_m3 is per hull; external solver not executed";
        if(preview){
            QJsonArray table;for(const auto& row:offsets){QJsonArray values;for(double value:row)values.append(value);table.append(values);}
            // Publish the very values serialized above, not a second sampling
            // or a drawing of the original control net. Rows remain bow first.
            *preview={{"format","FreeShipOffsetPreview"},{"version",1},{"document_id",m.documentId},{"revision",qint64(m.revision)},
                {"stations",nx},{"waterlines",nz},{"hulls",hulls},{"hull_mode",mode},{"separation_m",separation},
                {"length_m",length},{"draft_m",T},{"half_breadths_m",table}};
        }
    }else bad("未知网格输出格式。");
    out.flush();diagnostics["stations"]=stations;diagnostics["points"]=points;return {bytes,diagnostics};
}
}
