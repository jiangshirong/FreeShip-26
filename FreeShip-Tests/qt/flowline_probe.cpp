// GPL-2.0-or-later. Independent flat-panel and degenerate geometry fixtures.
#include "native/flowlines.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <cstdio>
using namespace fs::native_geometry;
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);SubdivisionLevel m;m.level=1;
    for(Vec3 p:std::vector<Vec3>{{0,0,0},{10,0,0},{10,2,0},{0,2,0}})m.points.push_back({p,VertexType::Corner});
    // Face construction uses the declared public member contract.
    m.faces.resize(1);m.faces[0].points={0,1,2,3};m.faces[0].rootControlFace=0;m.faces[0].layer=0;
    m.layers.resize(1);m.layers[0].useInHydrostatics=true;m.minimum={0,0,0};m.maximum={10,2,0};
    DocumentInput d;d.geometry.faces.resize(1);d.layerFaces={{0}};d.draft=1;
    d.flows={{1,2,8,1},{2,2,7,.5f},{3,2,30,1}};std::vector<Vec3> normals(4,Vec3{0,0,1});
    QJsonObject out{{"flat",buildFlowlines(d,m,normals)}};
    // Tangents change across the shared diagonal. Its edge fraction is .35,
    // independently yielding the next boundary hit at x=3.897435897..., y=2.
    normals[2]=normals[3]={.6f,.48f,.64f};d.flows={{1,2,8,1}};
    out["interpolated"]=buildFlowlines(d,m,normals);
    m.layers[0].useInHydrostatics=false;out["dry_layer"]=buildFlowlines(d,m,normals);
    m.layers[0].useInHydrostatics=true;m.faces[0].points={0,0,0};out["degenerate"]=buildFlowlines(d,m,normals);
    auto data=QJsonDocument(out).toJson(QJsonDocument::Compact);std::fwrite(data.data(),1,data.size(),stdout);std::fputc('\n',stdout);
}
