// Emit the uncut hydrostatic triangles in metres for independent quadrature.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "document/geometry_input.h"
#include <QFile>
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2)return 1;
    QFile file(QString::fromLocal8Bit(argv[1]));
    if(!file.open(QIODevice::ReadOnly))return 2;
    const auto fbm=fs::document::Fbm::read(file.readAll());
    const double u=fs::document::documentGeometrySettings(fbm).imperial?.3048:1.;
    const auto input=fs::document::documentGeometry(fbm);
    fs::native_geometry::SubdivisionSurface surface(input);
    const auto& mesh=surface.level(input.desiredLevel);
    for(const auto& face:mesh.faces){
        const auto& layer=input.layers[face.layer];
        if(!layer.useInHydrostatics)continue;
        for(int mirrored=0;mirrored<=int(layer.symmetric);++mirrored){
            for(size_t i=1;i+1<face.points.size();++i){
                for(auto j:{size_t(0),mirrored?i+1:i,mirrored?i:i+1}){
                    const auto p=mesh.points[face.points[j]].position;
                    std::printf("%.17g %.17g %.17g ",u*p.x,(mirrored?-u:u)*p.y,u*p.z);
                }
                std::puts("");
            }
        }
    }
}
