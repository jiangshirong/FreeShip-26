// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <array>
#include <vector>
#include <cmath>
#include <QStringList>
#include <algorithm>
#include <stdexcept>

// Workbook quantities are SI (m, t). Tank dimensions follow the model axes.
namespace fs::loading {
struct P { double x=0,y=0,z=0; P operator+(P b)const{return{x+b.x,y+b.y,z+b.z};} P operator-(P b)const{return{x-b.x,y-b.y,z-b.z};} P operator*(double k)const{return{x*k,y*k,z*k};} };
inline double dot(P a,P b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline P cross(P a,P b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
enum class TankShape { Box, Cylinder, Sphere, Trapezoid };
struct TankColumns {
    TankShape shape;
    const char* key;
    const char* title;
    const char* location;
    QStringList dimensions;
};
inline const std::array<TankColumns,4>& tankTables(){
    static const std::array<TankColumns,4> tables{{
        {TankShape::Box,"tanks","矩形液货舱","舱底中心 (m)",{"长 (m)","宽 (m)","高 (m)"}},
        {TankShape::Cylinder,"cylinders","圆柱形液货罐","罐底中心 (m)",{"纵向长度 (m)","直径 (m)"}},
        {TankShape::Sphere,"spheres","球形液货罐","罐底中心 (m)",{"直径 (m)"}},
        {TankShape::Trapezoid,"trapezoids","梯形液货舱","舱底中心 (m)",{"长 (m)","底宽 (m)","顶宽 (m)","高 (m)"}},
    }};
    return tables;
}
struct ItemField { const char* key; const char* title; };
enum ItemColumn { Name, Kind, Mass, X, Y, Z, Length, Width, Height, Diameter, BottomWidth, TopWidth, Density, Fill, ColumnCount };
inline constexpr std::array<ItemField,ColumnCount> itemFields{{
    {"name","名称"},{"kind","种类"},{"mass","重量 (t)"},{"x","X (m)"},{"y","Y (m)"},{"z","Z (m)"},
    {"length","长 (m)"},{"width","宽 (m)"},{"height","高 (m)"},{"diameter","直径 (m)"},
    {"bottom_width","底宽 (m)"},{"top_width","顶宽 (m)"},{"density","液体密度 (t/m³)"},{"fill","装载率 (%)"},
}};
inline const std::array<std::vector<ItemColumn>,4>& tankDimensions(){
    static const std::array<std::vector<ItemColumn>,4> fields{{
        {Length,Width,Height},{Length,Diameter},{Diameter},{Length,BottomWidth,TopWidth,Height}
    }};
    return fields;
}
inline int itemShape(const QString& key){
    for(int i=0;i<int(tankTables().size());++i)if(key==tankTables()[i].key)return i;
    return -1;
}
inline const char* itemTitle(const QString& key){
    const int shape=itemShape(key);
    return shape<0?"固定物体":tankTables()[shape].title;
}
inline bool usesItemColumn(const QString& kind,ItemColumn column){
    if(column==Name||column==Kind||(column>=X&&column<=Z))return true;
    const int shape=itemShape(kind);
    if(shape<0)return column==Mass;
    const auto& dims=tankDimensions()[shape];
    return column==Density||column==Fill||std::find(dims.begin(),dims.end(),column)!=dims.end();
}
inline QJsonObject newWeightItem(const QString& kind){
    QJsonObject item{{"name",QString{}},{"kind",kind}};
    for(int col=Mass;col<ColumnCount;++col)
        if(usesItemColumn(kind,ItemColumn(col)))item[itemFields[col].key]=col>=Length&&col<=Density?1:0;
    return item;
}
inline QJsonObject solidItem(const QJsonArray& row){
    return {{"name",row.at(0)},{"kind","fixed"},{"mass",row.at(1)},{"x",row.at(2)},{"y",row.at(3)},{"z",row.at(4)}};
}
inline QJsonObject unifiedBook(const QJsonObject& book,const QString& cargoName){
    if(book["version"]==4)return book;
    QJsonArray shared,cases;
    for(auto row:book["fixed"].toArray())shared.append(solidItem(row.toArray()));
    const auto details=book["case_weights"].toArray();
    for(auto entry:book["cases"].toArray()){
        const auto row=entry.toArray();
        QJsonArray items;
        const auto weights=details.at(cases.size()).toArray();
        if(row.at(1).toDouble()!=0){
            QSet<QString> names;
            for(auto w:weights)names.insert(w.toArray().at(0).toString().trimmed());
            QString title=cargoName;
            for(int i=2;names.contains(title);++i)title=cargoName+" "+QString::number(i);
            items.append(solidItem({title,row.at(1),row.at(2),row.at(3),row.at(4)}));
        }
        for(auto w:weights)items.append(solidItem(w.toArray()));
        int fill=5;
        for(int i=0;i<int(tankTables().size());++i){
            const auto& dims=tankDimensions()[i];
            const auto* kind=tankTables()[i].key;
            for(auto v:book[kind].toArray()){
                const auto tank=v.toArray();
                QJsonObject item{{"name",tank.at(0)},{"kind",kind},{"fill",row.at(fill++)}};
                int col=1;
                for(auto field:dims)item[itemFields[field].key]=tank.at(col++);
                for(auto field:{Density,X,Y,Z})item[itemFields[field].key]=tank.at(col++);
                items.append(item);
            }
        }
        cases.append(QJsonArray{row.at(0),items});
    }
    return {{"version",4},{"shared_items",shared},{"cases",cases},{"criteria",book["criteria"].toArray()}};
}

constexpr double Pi=3.14159265358979323846;
struct Tank {
    TankShape shape=TankShape::Box;
    P bottom,size;
    double topWidth=0,fill=0,mass=0,rho=0,uprightLevel=0;
};
template<class Volume> double fillLevel(double low,double high,double target,Volume volume){
    for(int i=0;i<55;++i){
        const double level=(low+high)*.5;
        if(volume(level)<target)low=level;else high=level;
    }
    return (low+high)*.5;
}
inline double diskArea(double t){
    if(t<=-1)return 0;
    if(t>=1)return Pi;
    if(t>0)return Pi-diskArea(-t);
    const double a=std::acos(-t),a2=a*a;
    if(a<.01)return a*a2*(2./3.-2./15.*a2+4./315.*a2*a2);
    return a+t*std::sqrt(1-t*t);
}
inline double diskMoment(double t){
    const double chord2=std::max(0.,1-t*t);
    return -2./3.*chord2*std::sqrt(chord2);
}
inline double sphereCap(double height){return height*height*(1-height/3);}
inline double taperedLevel(const Tank& t){
    const double target=t.fill*(t.size.y+t.topWidth)*.5;
    const double slope=t.topWidth-t.size.y;
    return 2*target/(t.size.y+std::sqrt(std::max(0.,t.size.y*t.size.y+2*slope*target)));
}
inline double tankVolume(const Tank& t){
    switch(t.shape){
    case TankShape::Box:return t.size.x*t.size.y*t.size.z;
    case TankShape::Cylinder:return Pi*t.size.y*t.size.y*t.size.x*.25;
    case TankShape::Sphere:return Pi*t.size.y*t.size.y*t.size.y/6;
    case TankShape::Trapezoid:return t.size.x*(t.size.y+t.topWidth)*.5*t.size.z;
    }
    return 0;
}
inline double freeSurfaceMoment(Tank& t){
    if(t.fill==1)return 0;
    double breadth=t.size.y;
    switch(t.shape){
    case TankShape::Box:break;
    case TankShape::Cylinder:
        t.uprightLevel=fillLevel(-1,1,t.fill*Pi,diskArea);
        breadth*=std::sqrt(std::max(0.,1-t.uprightLevel*t.uprightLevel));
        break;
    case TankShape::Sphere:{
        t.uprightLevel=fillLevel(0,2,t.fill*4./3.,sphereCap);
        const double radius2=breadth*breadth*.25*t.uprightLevel*(2-t.uprightLevel);
        return t.rho*Pi*radius2*radius2*.25;
    }
    case TankShape::Trapezoid:
        t.uprightLevel=taperedLevel(t);
        breadth+=(t.topWidth-breadth)*t.uprightLevel;
        break;
    }
    return t.rho*t.size.x*breadth*breadth*breadth/12;
}

// Numerical integration is needed only for cylinders with longitudinal tilt.
// The liquid area and its radial moment in each circular slice are analytic.
inline P cylinderCenter(const Tank& t,P n){
    const double length=t.size.x*.5,radius=t.size.y*.5;
    const P center=t.bottom+P{0,0,radius};
    if(t.fill==1)return center;
    const double radialLength=std::hypot(n.y,n.z);
    const double axial=n.x*length, radial=radialLength*radius;
    if(radial<=std::abs(axial)*1e-12)
        return center+P{std::copysign(length*(1-t.fill),-axial),0,0};
    const P direction{0,n.y/radialLength,n.z/radialLength};
    const double slope=std::abs(axial)/radial;
    const double area=t.fill*Pi;
    if(slope<1e-12)return center+direction*(radius*diskMoment(t.uprightLevel)/area);

    // Integrate in normalized axial coordinates. Split off the full and empty
    // regions before quadrature so a nearly axial cut remains well resolved.
    const auto slice=[slope](double level){
        const double full=std::clamp((level-1)/slope,-1.,1.);
        const double end=std::clamp((level+1)/slope,-1.,1.);
        P sum{Pi*(full+1),Pi*(full*full-1)*.5,0};
        constexpr std::array nodes{.0950125098376374,.281603550779259,.458016777657227,.617876244402644,
                                    .755404408355003,.865631202387832,.944575023073233,.98940093499165};
        constexpr std::array weights{.189450610455069,.182603415044924,.169156519395003,.149595988816577,
                                      .124628971255534,.095158511682493,.062253523938648,.027152459411754};
        // Integrating in disk angle removes the square-root endpoint behavior
        // whenever the cut traverses a substantial part of the circular section.
        const double upper=std::clamp(level-slope*full,-1.,1.);
        const double lower=std::clamp(level-slope*end,-1.,1.);
        if(end<=full)return sum;
        if(upper-lower>.1){
            const double lo=std::acos(-lower),hi=std::acos(-upper);
            const double middle=(lo+hi)*.5,half=(hi-lo)*.5;
            for(size_t i=0;i<nodes.size();++i)for(const double sign:{-1.,1.}){
                const double a=middle+sign*half*nodes[i],q=-std::cos(a);
                const double u=(level-q)/slope, jacobian=std::sin(a)*half/slope;
                const double A=diskArea(q);
                sum=sum+P{A,u*A,diskMoment(q)}*(weights[i]*jacobian);
            }
        }else{
            const double middle=(full+end)*.5,half=(end-full)*.5;
            for(size_t i=0;i<nodes.size();++i)for(const double sign:{-1.,1.}){
                const double u=middle+sign*half*nodes[i],q=level-slope*u;
                const double A=diskArea(q);
                sum=sum+P{A,u*A,diskMoment(q)}*(weights[i]*half);
            }
        }
        return sum;
    };
    const double level=fillLevel(-1-slope,1+slope,2*area,[&](double h){return slice(h).x;});
    const auto moments=slice(level);
    if(moments.x<=0)return center;
    const double x=length*(axial<0?-1:1)*moments.y/moments.x;
    return center+P{x,0,0}+direction*(radius*moments.z/moments.x);
}

// Clip the six oriented prism faces. A point on the liquid plane is
// the tetrahedron apex, so the closing face contributes zero.
inline P prismCenter(const Tank& t,P normal){
    const bool box=t.shape==TankShape::Box;
    const double lower=t.size.y,upper=box?lower:t.topWidth;
    if(t.fill==1||(normal.x==0&&normal.y==0&&normal.z>0)){
        double z=t.fill*.5;
        if(!box){
            const double h=t.fill==1?1:t.uprightLevel;
            z=h*(lower/2+(upper-lower)*h/3)/(lower+(upper-lower)*h/2);
        }
        return t.bottom+P{0,0,z*t.size.z};
    }
    const double breadth=std::max(lower,upper);
    const double b=lower/(2*breadth),u=upper/(2*breadth);
    const P vertices[]={{-.5,-b,-.5},{.5,-b,-.5},{.5,b,-.5},{-.5,b,-.5},
                        {-.5,-u,.5},{.5,-u,.5},{.5,u,.5},{-.5,u,.5}};
    static constexpr int faces[][4]={{3,2,1,0},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
    const P n={normal.x*t.size.x,normal.y*breadth,normal.z*t.size.z};
    double lo=dot(n,vertices[0]),hi=lo;
    for(auto point:vertices){lo=std::min(lo,dot(n,point));hi=std::max(hi,dot(n,point));}
    const double target=t.fill*(b+u);
    P c{};
    for(int step=0;step<64;++step){
        const double h=(lo+hi)*.5;
        const P apex=n*(h/dot(n,n));
        P moment{};double volume=0;
        for(auto& face:faces){
            P poly[8];int k=0;
            for(int j=0;j<4;++j){
                const P a=vertices[face[j]],d=vertices[face[(j+1)%4]];
                const double da=dot(n,a)-h,db=dot(n,d)-h;
                if(da<=0)poly[k++]=a;
                if((da<0&&db>0)||(da>0&&db<0))poly[k++]=a+(d-a)*(da/(da-db));
            }
            for(int j=2;j<k;++j){
                const double v=dot(poly[0]-apex,cross(poly[j-1]-apex,poly[j]-apex))/6;
                volume+=v;moment=moment+(apex+poly[0]+poly[j-1]+poly[j])*(v*.25);
            }
        }
        if(volume>0)c=moment*(1/volume);
        if(std::abs(volume-target)<=1e-13*target)break;
        if(volume<target)lo=h;else hi=h;
    }
    return t.bottom+P{c.x*t.size.x,c.y*breadth,(c.z+.5)*t.size.z};
}
inline P liquidCenter(const Tank& t,P normal){
    if(t.shape==TankShape::Sphere){
        const double r=t.size.y*.5;
        const P centre=t.bottom+P{0,0,r};
        if(t.fill==1)return centre;
        const double h=t.uprightLevel;
        return centre+normal*(-r*(2-h)*(2-h)/(4*(1-h/3)*std::sqrt(dot(normal,normal))));
    }
    if(t.shape==TankShape::Cylinder)return cylinderCenter(t,normal);
    return prismCenter(t,normal);
}
struct Case {QString name;double mass=0,fsm=0;P solidMoment{};std::vector<Tank> tanks;
    P gravity(P n)const{P m=solidMoment;for(auto& t:tanks)m=m+liquidCenter(t,n)*t.mass;return m*(1/mass);}
};
inline void require(bool ok,const char* msg){if(!ok)throw std::invalid_argument(msg);}
inline P itemPosition(const QJsonObject& item){
    return {item["x"].toDouble(),item["y"].toDouble(),item["z"].toDouble()};
}
inline Tank makeTank(int shape,const QJsonObject& item){
    Tank tank;
    tank.shape=tankTables()[shape].shape;
    tank.bottom=itemPosition(item);
    tank.rho=item["density"].toDouble();
    for(auto field:tankDimensions()[shape])
        require(item[itemFields[field].key].toDouble()>0,"液舱尺寸和液体密度必须大于零。");
    require(tank.rho>0,"液舱尺寸和液体密度必须大于零。");
    const auto d=[&](const char* key){return item[key].toDouble();};
    switch(tank.shape){
    case TankShape::Box:tank.size={d("length"),d("width"),d("height")};break;
    case TankShape::Cylinder:tank.size={d("length"),d("diameter"),0};break;
    case TankShape::Sphere:tank.size.y=d("diameter");break;
    case TankShape::Trapezoid:
        tank.size={d("length"),d("bottom_width"),d("height")};
        tank.topWidth=d("top_width");break;
    }
    return tank;
}
inline void addTank(Case& c,Tank tank,double fill){
    require(fill>=0&&fill<=100,"舱液装载率必须在 0 至 100% 之间。");
    if(fill==0)return;
    tank.fill=fill/100;
    tank.mass=tankVolume(tank)*tank.rho*tank.fill;
    c.mass+=tank.mass;c.fsm+=freeSurfaceMoment(tank);
    c.tanks.push_back(tank);
}
inline void addWeight(Case& c,double mass,P position){
    require(mass>=0,"固定重量不能为负。");
    c.mass+=mass;
    c.solidMoment=c.solidMoment+position*mass;
}
inline void validateCriteria(const QJsonArray& criteria){
    QSet<double> angles;
    require(criteria.size()<=32,"装载表缺失或行数超限。");
    for(auto v:criteria){
        const auto row=v.toArray();const double a=row.at(0).toDouble(-1),gz=row.at(1).toDouble(-1);
        require(row.size()==2&&a>=0&&a<=180&&!angles.contains(a)&&gz>=0&&gz<=1e9,
            "衡准角度须在 0 至 180° 之间且不能重复，最低 GZ 不能为负。");
        angles.insert(a);
    }
}
inline void validateMass(const Case& c){
    require(c.mass>=1e-6&&c.mass<=1e9,"工况总重量必须在有效排水量范围内。");
}
inline void validateName(const QString& name,QSet<QString>& names){
    require(!name.isEmpty()&&name.size()<=80&&!names.contains(name),"名称不能为空或重复，最多 80 字。");
    names.insert(name);
}
inline int addItems(Case& c,const QJsonValue& entries,int weightLimit){
    require(entries.isArray()&&entries.toArray().size()<=weightLimit+32,"装载表缺失或行数超限。");
    QSet<QString> weights,tanks;
    for(auto entry:entries.toArray()){
        const auto item=entry.toObject();
        const auto name=item["name"].toString().trimmed(),kind=item["kind"].toString();
        const int shape=itemShape(kind);
        require(kind=="fixed"||shape>=0,"重量项种类无效。");
        validateName(name,shape<0?weights:tanks);
        for(int col=Mass;col<ColumnCount;++col){
            if(!usesItemColumn(kind,ItemColumn(col)))continue;
            const auto value=item[itemFields[col].key];
            require(value.isDouble()&&std::isfinite(value.toDouble())&&std::abs(value.toDouble())<=1e9,"装载表数值无效。");
        }
        if(shape<0)addWeight(c,item["mass"].toDouble(),itemPosition(item));
        else addTank(c,makeTank(shape,item),item["fill"].toDouble());
    }
    require(weights.size()<=weightLimit&&tanks.size()<=32,"装载表缺失或行数超限。");
    return tanks.size();
}
inline std::vector<Case> parseUnified(const QJsonObject& book,bool allowEmptyCases=false){
    require(book["criteria"].isArray()&&book["cases"].isArray(),"装载表缺失或行数超限。");
    validateCriteria(book["criteria"].toArray());
    const auto cases=book["cases"].toArray();
    require(allowEmptyCases||!cases.isEmpty(),"至少需要一个装载工况。");
    require(cases.size()<=100,"装载表缺失或行数超限。");
    Case base;
    const int commonTanks=addItems(base,book["shared_items"],100);
    QSet<QString> names;
    std::vector<Case> result;
    for(auto entry:cases){
        const auto row=entry.toArray();
        require(row.size()==2,"装载表列数不符。");
        Case c=base;c.name=row.at(0).toString().trimmed();
        validateName(c.name,names);
        const int localTanks=addItems(c,row.at(1),101); // A legacy cargo row can accompany 100 detailed weights.
        require(commonTanks+localTanks<=32,"装载表缺失或行数超限。");
        validateMass(c);result.push_back(std::move(c));
    }
    return result;
}
inline std::vector<Case> parse(const QJsonObject& book){
    if(book["version"]==4)return parseUnified(book);
    const bool multipleShapes=book["version"]==3;
    const bool detailed=book["version"]==2||(multipleShapes&&book.contains("case_weights"));
    require(multipleShapes||detailed||(book["version"]==1&&!book.contains("case_weights")),"不支持的装载工况版本。");
    auto table=[&](const QJsonValue& value,int cols,int limit,bool named){std::vector<QJsonArray> rows;QSet<QString> names;
        require(value.isArray()&&value.toArray().size()<=limit,"装载表缺失或行数超限。");for(auto v:value.toArray()){auto r=v.toArray();require(r.size()==cols,"装载表列数不符。");if(named){auto s=r[0].toString().trimmed();require(!s.isEmpty()&&s.size()<=80&&!names.contains(s),"名称不能为空或重复，最多 80 字。");names.insert(s);}for(int i=named?1:0;i<cols;++i)require(r[i].isDouble()&&std::isfinite(r[i].toDouble())&&std::abs(r[i].toDouble())<=1e9,"装载表数值无效。");rows.push_back(r);}return rows;};
    auto point=[](const QJsonArray& r,int i){return P{r[i].toDouble(),r[i+1].toDouble(),r[i+2].toDouble()};};
    auto fixed=table(book["fixed"],5,100,true);
    require(book["criteria"].isArray(),"装载表缺失或行数超限。");
    validateCriteria(book["criteria"].toArray());
    std::vector<Tank> tanks;
    QSet<QString> tankLabels;
    for(const auto& schema:tankTables()){
        if(schema.shape!=TankShape::Box&&!book.contains(schema.key))continue;
        require(schema.shape==TankShape::Box||multipleShapes,"不支持的装载工况版本。");
        const int dimensions=schema.dimensions.size();
        for(const auto& row:table(book[schema.key],dimensions+5,32,true)){
            const auto name=row[0].toString().trimmed();
            require(!tankLabels.contains(name),"名称不能为空或重复，最多 80 字。");
            tankLabels.insert(name);
            const int shape=itemShape(schema.key);
            QJsonObject item;
            int col=1;
            for(auto field:tankDimensions()[shape])item[itemFields[field].key]=row.at(col++);
            for(auto field:{Density,X,Y,Z})item[itemFields[field].key]=row.at(col++);
            tanks.push_back(makeTank(shape,item));
        }
    }
    require(tanks.size()<=32,"装载表缺失或行数超限。");
    auto cases=table(book["cases"],5+int(tanks.size()),100,true);
    require(!cases.empty(),"至少需要一个装载工况。");
    Case base;
    for(const auto& row:fixed)addWeight(base,row[1].toDouble(),point(row,2));
    const auto weights=book["case_weights"].toArray();
    require(!detailed || (book["case_weights"].isArray() && weights.size()==qsizetype(cases.size())),"工况与重量明细数量不符。");
    std::vector<Case> out;
    for(size_t index=0;index<cases.size();++index){
        const auto& r=cases[index];
        Case c=base;
        if(detailed)for(const auto& row:table(weights[qsizetype(index)],5,100,true))addWeight(c,row[1].toDouble(),point(row,2));
        c.name=r[0].toString().trimmed();double cargo=r[1].toDouble();require(cargo>=0,"货重不能为负。");c.mass+=cargo;c.solidMoment=c.solidMoment+point(r,2)*cargo;
        for(size_t i=0;i<tanks.size();++i)addTank(c,tanks[i],r[int(i)+5].toDouble());
        validateMass(c);out.push_back(std::move(c));
    }
    return out;
}
inline void validateSavedBook(const QJsonObject& book){
    if(book["version"]==4)parseUnified(book,true);
    else parse(book);
}
}
