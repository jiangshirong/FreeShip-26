// Analytic regression cases for fs::compareStationContours. Coordinates in mm.
#pragma once
#include "core/station_deviation.h"
#include "core/station_pairing.h"

template<typename Record>
void checkStationDeviationCases(Record record){
    using Lines=QVector<QPolygonF>;
    const Lines horizontal{{{0,0},{100,0}}};
    constexpr double tol=0.001;
    auto check=[&](const char* name,const Lines& a,const Lines& b,double known){
        const auto result=fs::compareStationContours(a,b,tol);
        const double eps=1e-9;
        bool valid=result&&result->maximumMm<=known+eps&&
            result->maximumMm+result->errorBoundMm+eps>=known&&result->errorBoundMm<=tol+eps;
        if(result){
            const auto v=result->referenceMm-result->modelMm;
            valid &= std::abs(std::hypot(v.x(),v.y())-result->maximumMm)<eps;
        }
        record(name,valid);
        return result;
    };
    check("deviation.identical-uneven-sampling",horizontal,{{{100,0},{99,0},{39.7,0},{0,0}}},0);
    check("deviation.parallel-shift",horizontal,{{{0,4},{100,4}}},4);
    // Only reference-to-model sees the 52-mm central gap. The maximum falls in
    // the interior of a reference segment, at x=43 rather than the midpoint.
    const Lines split{{{0,0},{17,0}},{{69,0},{100,0}}};
    const auto gap=check("deviation.disconnected-gap",horizontal,split,26);
    record("deviation.gap-witness",gap&&std::abs(gap->referenceMm.x()-43)<=tol&&
           std::min(std::abs(gap->modelMm.x()-17),std::abs(gap->modelMm.x()-69))<1e-9);
    check("deviation.reference-omits-model-only-coverage",split,horizontal,0);
    const Lines protruding{{{0,0},{50,7},{100,0}}};
    check("deviation.nearest-model-point",horizontal,protruding,350/std::hypot(50,7));
    check("deviation.swapping-reference-changes-measurement",protruding,horizontal,7);
    check("deviation.shorter-coverage",horizontal,{{{0,0},{70,0}}},30);
    check("deviation.closed-reversed",{{{0,0},{0,10},{10,10},{0,0}}},
          {{{10,10},{0,10},{0,0},{10,10}}},0);
    record("deviation.missing-lines",!fs::compareStationContours(horizontal,{}));
    record("deviation.zero-length-only",!fs::compareStationContours(horizontal,{{{0,0},{0,0}}}));
    record("deviation.reject-nonfinite",!fs::compareStationContours(horizontal,
           {{{0,0},{std::numeric_limits<double>::quiet_NaN(),0}}}));

    const Lines reference{{{40,12},{0,0}},{{50,60},{40,12}}};
    const Lines model{{{52,70},{53,20},{31.8,12},{0,0}}};
    const auto pairs=fs::pairStationContours({reference},{model});
    record("pairing.chine-to-chine-is-maximum",pairs&&pairs->maximum.referenceMm==QPointF(40,12)&&
        pairs->maximum.modelMm==QPointF(53,20)&&std::abs(pairs->maximum.maximumMm-std::hypot(13,8))<1e-9);
    if(pairs){
        const auto a=pairs->pairAt(pairs->reference.nearest({20,6}),true);
        const auto b=pairs->pairAt(pairs->model.nearest(a.modelMm),false);
        record("pairing.arc-progress-is-reciprocal",QLineF(a.modelMm,{26.5,10}).length()<1e-9&&
            QLineF(a.modelMm,b.modelMm).length()<1e-9&&QLineF(a.referenceMm,b.referenceMm).length()<1e-9);
    }
    const auto reverse=fs::pairStationContours({model},{reference});
    record("pairing.swapping-contours-keeps-maximum",pairs&&reverse&&
        std::abs(pairs->maximum.maximumMm-reverse->maximum.maximumMm)<1e-9);
    record("pairing.refuses-unmatched-corners",!fs::pairStationContours({reference},{horizontal}));
    record("pairing.refuses-disconnected-markers",!fs::pairStationContours({split},{reference}));
}
