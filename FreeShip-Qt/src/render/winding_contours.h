// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPainterPath>
#include <QVector>
#include <map>
#include <algorithm>
#include <cmath>

namespace fs {
// Cancel only opposite copies of the SAME indexed edge. No coordinate welding,
// simplification, sampling or tolerance. Closed face chains have the same winding
// number after cancellation, including holes, overlaps and disconnected islands.
class WindingContours {
    std::map<std::pair<int,int>,int> edges_;
public:
    void add(const QVector<int>& polygon) {
        for(int i=0;i<polygon.size();++i){int a=polygon[i],b=polygon[(i+1)%polygon.size()];
            if(a==b)continue;const auto key=std::minmax(a,b);auto& n=edges_[key];n+=a<b?1:-1;
            if(!n)edges_.erase(key);
        }
    }
    QPainterPath path(const QVector<QPointF>& points)const {
        std::multimap<int,int> directed;
        for(const auto& [edge,count]:edges_)for(int i=0;i<std::abs(count);++i)
            directed.emplace(count>0?edge.first:edge.second,count>0?edge.second:edge.first);
        QPainterPath result;result.setFillRule(Qt::WindingFill);
        while(!directed.empty()){
            const int start=directed.begin()->first;int current=start;result.moveTo(points[start]);
            do{auto next=directed.find(current);
                // Each input is closed, so incoming and outgoing multiplicities
                // balance at every vertex, even at non-manifold junctions.
                Q_ASSERT(next!=directed.end());if(next==directed.end())return {};
                current=next->second;directed.erase(next);result.lineTo(points[current]);
            }while(current!=start);
            result.closeSubpath();
        }
        return result;
    }
};
}
