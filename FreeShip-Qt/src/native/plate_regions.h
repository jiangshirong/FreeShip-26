// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "subdivision.h"
#include <algorithm>
#include <map>
#include <numeric>
#include <tuple>

namespace fs::native_geometry {
// A plate region is an edge-connected component of a single layer.
// Compute it on control faces so all children inherit the same region.
inline std::vector<std::size_t> plateRegions(const GeometryInput& input) {
    std::vector<std::size_t> parent(input.faces.size()), count(input.faces.size(),1);
    std::iota(parent.begin(),parent.end(),0);
    auto root=[&](std::size_t face) {
        while(parent[face]!=face) {
            parent[face]=parent[parent[face]];
            face=parent[face];
        }
        return face;
    };
    std::map<std::tuple<std::size_t,std::size_t,std::size_t>,std::size_t> firstFace;
    for(std::size_t i=0;i<input.faces.size();++i) {
        const auto& face=input.faces[i];
        for(std::size_t j=0;j<face.points.size();++j) {
            const auto [a,b]=std::minmax(face.points[j],face.points[(j+1)%face.points.size()]);
            const auto [it,inserted]=firstFace.try_emplace({face.layer,a,b},i);
            if(inserted)continue;
            auto left=root(i),right=root(it->second);
            if(left==right)continue;
            if(count[left]<count[right])std::swap(left,right);
            parent[right]=left;
            count[left]+=count[right];
        }
    }
    for(std::size_t i=0;i<parent.size();++i)parent[i]=root(i);
    return parent;
}
}
