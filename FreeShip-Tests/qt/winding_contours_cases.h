// Independent containment checks for plate screen contours; no simplification.
#pragma once
#include "render/winding_contours.h"
template<class Check> void windingContourCases(Check check){
    const QVector<QPointF> points{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},
        {.2,.2},{.8,.2},{.8,.8},{.2,.8},{3,0},{4,0},{4,1},{3,1}};
    const QVector<QVector<QVector<int>>> cases{
        {{0,1,4,3},{1,2,5,4}}, // shared edge cancellation
        {{0,1,4,3},{9,8,7,6}}, // clockwise hole
        {{0,1,4,3},{0,1,4,3}}, // multiplicity must not turn into an odd-even hole
        {{0,1,4,3},{3,4,1,0}}, // opposite coincident faces cancel fully
        {{0,1,4,3},{10,11,12,13}}, // disjoint island
        {{0,1,4,3},{1,2,5,4},{4,3,0}}, // non-manifold multiplicity
        {{0,2,3,5}}, // self crossing
        {{0,1,4,3},{6,7,8,9}} // nested equal winding
    };
    for(int n=0;n<cases.size();++n){fs::WindingContours builder;QPainterPath reference;reference.setFillRule(Qt::WindingFill);
        for(const auto& face:cases[n]){builder.add(face);reference.moveTo(points[face[0]]);for(int i=1;i<face.size();++i)reference.lineTo(points[face[i]]);reference.closeSubpath();}
        const auto contour=builder.path(points);bool equal=true;
        for(int y=-9;y<65;++y)for(int x=-9;x<235;++x){QPointF p(x*.019+0.000137,y*.019+0.000211);equal&=reference.contains(p)==contour.contains(p);}
        check("winding-contours-"+QString::number(n),equal);
    }
}
