// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include <algorithm>
#include <cmath>

// Original light controls are a direction in camera coordinates, not a point
// stored in ship units. X faces the viewer, Y points right and Z points up.
struct HullLight {
    fs::Vec3 direction{50,20,50};
    int ambient=64,intensity=192;
    bool enabled=false;
    bool valid()const{
        for(double value:{direction.x,direction.y,direction.z})if(!std::isfinite(value)||value<-180||value>180)return false;
        return ambient>=0&&ambient<=255&&intensity>=0&&intensity<=255;
    }
    bool operator==(const HullLight& b)const{return direction.x==b.direction.x&&direction.y==b.direction.y&&direction.z==b.direction.z&&ambient==b.ambient&&intensity==b.intensity&&enabled==b.enabled;}
};
// Position and triangle centers are relative to the mesh center, in mesh extents.
// Only these uniforms change during navigation; vertex buffers remain immutable.
struct HullLighting {
    fs::Vec3 position{};
    double ambient=1,intensity=0;
    bool enabled=false;
    double factor(fs::Vec3 normal,fs::Vec3 center)const{
        if(!enabled)return 1;
        const fs::Vec3 d{position.x-center.x,position.y-center.y,position.z-center.z};
        const double length=std::hypot(d.x,d.y,d.z);
        const double cosine=length>1e-12?std::abs((normal.x*d.x+normal.y*d.y+normal.z*d.z)/length):0;
        return std::clamp(ambient+intensity*cosine,0.,1.);
    }
};
