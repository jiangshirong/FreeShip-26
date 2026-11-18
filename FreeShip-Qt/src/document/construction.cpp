// GPL-2.0-or-later. Adapted from current Plus ModelTools.inc, FreeMatrices.pas
// and FreeGometry_Functions.RotatePointAroundVector. Original FREE!ship/Plus
// authorship retained; see NOTICE.md. Single storage and operation order matter.
#include "construction.h"
#include "../../third_party/fpc/trig.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void invalid(const char* message){throw std::invalid_argument(message);}
double number(const QJsonObject& r,const char* key){
    const auto value=r[key];const auto n=value.toDouble();
    if(!value.isDouble()||!std::isfinite(n)||std::abs(n)>1e9)invalid("Invalid construction number");
    return n;
}
int count(const QJsonObject& r,const char* key,int low,int high){
    const auto n=number(r,key);if(n<low||n>high||std::floor(n)!=n)invalid("Construction count outside supported range");
    return int(n);
}
FbmPosition vector(const QJsonObject& r,const char* key){
    const auto a=r[key].toArray();if(!r[key].isArray()||a.size()!=3)invalid("Construction vector requires three coordinates");
    float v[3];for(int i=0;i<3;++i){const double n=a[i].toDouble();if(!a[i].isDouble()||!std::isfinite(n)||std::abs(n)>1e9)invalid("Invalid construction coordinate");v[i]=float(n);}
    return {v[0],v[1],v[2]};
}
bool flag(const QJsonObject& r,const char* key){if(!r[key].isBool())invalid("Invalid construction flag");return r[key].toBool();}
void check(FbmPosition p){for(float v:{p.x,p.y,p.z})if(!std::isfinite(v)||std::abs(v)>1e9f)invalid("Constructed point exceeds supported domain");}
FbmPosition rotate(FbmPosition p,FbmPosition origin,FbmPosition axis){
    const float yz=std::sqrt(axis.y*axis.y+axis.z*axis.z);
    const bool vertical=axis.z==0&&axis.y==0;
    const float sx=vertical?0:-axis.y/yz,cx=vertical?1:axis.z/yz;
    const float length=std::sqrt(axis.x*axis.x+axis.y*axis.y+axis.z*axis.z);
    if(length==0)return p;
    const float sy=axis.x/length,cy=yz/length;
    const FbmPosition a{p.x-origin.x,p.y-origin.y,p.z-origin.z};
    const FbmPosition b{a.x*cy+a.z*sy,a.y,-a.x*sy+a.z*cy};
    return {b.x+origin.x,(b.y*cx-b.z*sx)+origin.y,(b.y*sx+b.z*cx)+origin.z};
}
using Matrix=std::vector<std::vector<float>>;
Matrix inverse(Matrix a){
    const int n=int(a.size());Matrix result(n,std::vector<float>(n));for(int i=0;i<n;++i)result[i][i]=1;
    // Same pivot selection, sweep order and Single matrix cells as TFreeMatrix.
    for(int i=0;i<n;++i){
        int pivot=i;double largest=std::abs(a[i][i]);
        for(int k=i+1;k<n;++k)if(std::abs(a[k][i])>largest){pivot=k;largest=std::abs(a[k][i]);}
        if(largest<1e-5)invalid("Cylinder interpolation matrix is singular");
        if(pivot!=i){for(int j=i;j<n;++j)std::swap(a[i][j],a[pivot][j]);std::swap(result[i],result[pivot]);}
        const auto sweep=[&](int k){
            if(std::abs(a[k][i])<=1e-5)return;
            const long double factor=a[k][i]/a[i][i];if(factor==0)return;
            for(int j=i;j<n;++j)a[k][j]=float(a[k][j]-factor*a[i][j]);
            for(int j=0;j<n;++j)result[k][j]=float(result[k][j]-factor*result[i][j]);
        };
        for(int k=i+1;k<n;++k)sweep(k);
        for(int k=i-1;k>=0;--k)sweep(k);
    }
    if(std::abs(a.back().back())<1e-5)invalid("Cylinder interpolation matrix is singular");
    for(int i=0;i<n;++i){const long double factor=a[i][i];for(int j=0;j<n;++j)result[i][j]=float(result[i][j]/factor);}
    return result;
}
}
ConstructionGrid constructionGrid(const QJsonObject& r){
    ConstructionGrid result;const auto origin=vector(r,"origin");
    if(r["operation"]=="grid_panel"){
        const float sizeA=float(number(r,"size_a")),sizeB=float(number(r,"size_b"));
        if(sizeA<=0||sizeB<=0)invalid("Panel dimensions must be positive");
        result.columns=count(r,"columns",2,100);result.rows=count(r,"rows",2,100);const int axis=count(r,"plane",0,2);
        for(int i=0;i<result.columns;++i)for(int j=0;j<result.rows;++j){
            auto p=origin;const float a=i*sizeA/(result.columns-1),b=j*sizeB/(result.rows-1);
            if(axis==0){p.x+=a;p.y+=b;}else if(axis==1){p.x+=a;p.z+=b;}else{p.y+=a;p.z+=b;}
            check(p);result.points.push_back(p);
        }
        return result;
    }
    if(r["operation"]!="cylinder")invalid("Unknown construction command");
    result.cylinder=true;result.rows=2;result.columns=count(r,"segments",4,128);
    result.closedStart=flag(r,"close_start");result.closedEnd=flag(r,"close_end");
    const auto end=vector(r,"end");const FbmPosition axis{end.x-origin.x,end.y-origin.y,end.z-origin.z};
    if(double(axis.x)*axis.x+double(axis.y)*axis.y+double(axis.z)*axis.z<1e-12)invalid("Cylinder axis has zero length");
    const float radius=float(number(r,"radius"));if(radius<=0)invalid("Cylinder radius must be positive");
    const int n=result.columns,m=n+4;Matrix matrix(m,std::vector<float>(m));matrix[0][0]=matrix[m-1][m-1]=1;
    for(int i=1;i<m-1;++i){matrix[i][i-1]=1.0f/6;matrix[i][i]=2.0f/3;matrix[i][i+1]=1.0f/6;}
    const auto inverted=inverse(std::move(matrix));Matrix samples(m,std::vector<float>(3)),control(m,std::vector<float>(3));
    for(int i=0;i<n;++i){
        constexpr long double pi=3.141592653589793238462643383279502884L;
        const float angle=float(-(double(i)/n)*2*pi);
        FbmPosition p{float(origin.x+fpc::radians(angle,false)*radius),float(origin.y+fpc::radians(angle,true)*radius),origin.z};
        p=rotate(p,origin,axis);samples[i+2]={p.x,p.y,p.z};
    }
    samples[1]=samples[n+1];samples[0]=samples[n];samples[n+2]=samples[2];samples[n+3]=samples[3];
    for(int i=0;i<m;++i)for(int j=0;j<3;++j)for(int k=0;k<m;++k)control[i][j]=control[i][j]+inverted[i][k]*samples[k][j];
    for(int i=0;i<n;++i)for(int j=0;j<2;++j){FbmPosition p{control[i+2][0]+j*axis.x,control[i+2][1]+j*axis.y,control[i+2][2]+j*axis.z};check(p);result.points.push_back(p);}
    return result;
}
}
