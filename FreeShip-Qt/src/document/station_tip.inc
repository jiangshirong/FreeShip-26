// GPL-2.0-or-later. Included inside Hydrostatics::Impl.
// All recognition bounds are physical, independent of the FBM length unit.
struct TipLoop {
    std::vector<size_t> edges;
    std::set<size_t> roots;
    double ymin=1e300,ymax=-1e300,zmin=1e300,zmax=-1e300,area=0;
    bool closed=false;
    double width()const{return ymax-ymin;}
    double height()const{return zmax-zmin;}
    bool overlaps(const TipLoop& other,double margin=0)const{
        return ymin<=other.ymax+margin && ymax>=other.ymin-margin &&
               zmin<=other.zmax+margin && zmax>=other.zmin-margin;
    }
};
// Split complete (unclipped) station contours before applying a wet-area rule.
// Match using rounded Single mesh coordinates only for incidence. Integrate
// the original double intersection points relative to a local origin.
static std::vector<TipLoop> stationLoops(const std::vector<CutSegment>& cuts){
    using Key=std::pair<float,float>;
    auto key=[](CutPoint p){return Key{float(p.y),float(p.z)};};
    std::map<Key,std::vector<size_t>> incident;
    for(size_t i=0;i<cuts.size();++i){
        incident[key(cuts[i].a)].push_back(i);
        incident[key(cuts[i].b)].push_back(i);
    }
    std::vector<bool> seen(cuts.size());
    std::vector<TipLoop> result;
    for(size_t i=0;i<cuts.size();++i){
        if(seen[i])continue;
        auto& ring=result.emplace_back();
        ring.edges.push_back(i);seen[i]=true;
        std::map<std::pair<Key,Key>,int> directed;
        const auto origin=cuts[i].a;
        for(size_t j=0;j<ring.edges.size();++j){
            const auto& edge=cuts[ring.edges[j]];
            ring.roots.insert(edge.root);
            for(auto p:{edge.a,edge.b}){
                ring.ymin=std::min(ring.ymin,p.y);ring.ymax=std::max(ring.ymax,p.y);
                ring.zmin=std::min(ring.zmin,p.z);ring.zmax=std::max(ring.zmax,p.z);
                for(auto k:incident[key(p)])if(!seen[k]){seen[k]=true;ring.edges.push_back(k);}
            }
            auto a=key(edge.a),b=key(edge.b);
            if(a!=b)directed[std::minmax(a,b)]+=a<b?1:-1;
            ring.area+=.5*((edge.a.y-origin.y)*(edge.b.z-origin.z)-(edge.b.y-origin.y)*(edge.a.z-origin.z));
        }
        std::map<Key,std::pair<int,int>> degrees;
        for(const auto& [ends,count]:directed){
            auto [a,b]=ends;
            if(count<0)std::swap(a,b);
            if(count){degrees[a].first+=std::abs(count);degrees[b].second+=std::abs(count);}
        }
        ring.closed=!degrees.empty() && std::all_of(degrees.begin(),degrees.end(),[](const auto& v){
            return v.second.first==1 && v.second.second==1;
        });
    }
    return result;
}
// Extrema are per connected control net, so a shorter ama can terminate
// inside the main hull's longitudinal range. Compute once per immutable mesh.
std::vector<size_t> tipComponent;
std::map<size_t,std::pair<double,double>> tipRanges;
std::vector<double> tipEnds;
void prepareTipEnds(Job& job){
    if(!tipComponent.empty())return;
    std::vector<size_t> parents(input.faces.size());
    std::iota(parents.begin(),parents.end(),0);
    auto root=[&](size_t i){
        while(i!=parents[i]){parents[i]=parents[parents[i]];i=parents[i];}
        return i;
    };
    std::map<size_t,size_t> owners;
    for(size_t i=0;i<input.faces.size();++i){
        if((i&1023)==0)job.check();
        if(!input.layers[input.faces[i].layer].useInHydrostatics)continue;
        for(auto p:input.faces[i].points){
            auto [it,added]=owners.emplace(p,i);
            if(!added)parents[root(i)]=root(it->second);
        }
    }
    for(size_t i=0;i<parents.size();++i)parents[i]=root(i);
    std::map<size_t,std::pair<double,double>> ranges;
    for(const auto f:faces){
        auto& face=mesh.faces[f];auto& range=ranges.try_emplace(parents[face.rootControlFace],1e300,-1e300).first->second;
        for(auto p:face.points){
            double x=mesh.points[p].position.x;
            range.first=std::min(range.first,x);range.second=std::max(range.second,x);
        }
    }
    for(auto [id,range]:ranges){tipEnds.push_back(range.first);tipEnds.push_back(range.second);}
    std::sort(tipEnds.begin(),tipEnds.end());
    tipComponent=std::move(parents);tipRanges=std::move(ranges);
}
void removeTerminalSlivers(std::vector<CutSegment>& cuts,float x,Job& job){
    if(cuts.empty())return;
    const double unit=settings.geometry.imperial?.3048:1.;
    prepareTipEnds(job);
    const double band=.0001/unit;
    const auto near=std::lower_bound(tipEnds.begin(),tipEnds.end(),x-band);
    if(near==tipEnds.end() || *near>x+band)return;
    const auto rings=stationLoops(cuts);
    std::vector<bool> omit(cuts.size());
    for(const auto& r:rings){
        const auto id=tipComponent.at(*r.roots.begin());
        if(std::any_of(r.roots.begin(),r.roots.end(),[&](auto k){return tipComponent[k]!=id;}))continue;
        const auto [lo,hi]=tipRanges.at(id);
        const bool aft=x-lo<hi-x;
        const double end=aft?lo:hi;
        const double extent=std::min(band,(hi-lo)*.00005);
        const double limit=.2*extent;
        if(extent<=0 || std::abs(x-end)>extent)continue;
        if(!r.closed || r.area>=0)continue;
        // A nested negative loop can be a real hole. Keep it, as well as
        // touching/ambiguous outlines, rather than filling a moonpool.
        if(std::any_of(rings.begin(),rings.end(),[&](const auto& o){return &r!=&o && r.overlaps(o);}))continue;
        // Preserve the diagnostic even if another hull's positive area would
        // conceal this ring. Test its wetted portion in fullArea, not here.
        if(rings.size()>1)for(auto i:r.edges)cuts[i].checkRing=true;
        if(r.width()>limit||r.height()>limit||-r.area>limit*limit)continue;
        auto neighbour=[&](double at){
            return stationLoops(cutPlane(Plane(1,0,0,-float(at)),job));
        };
        auto matching=[&](const std::vector<TipLoop>& neighbours)->std::vector<TipLoop>{
            std::vector<TipLoop> candidates;
            for(const auto& n:neighbours)if(r.overlaps(n,4*extent) &&
               std::any_of(r.roots.begin(),r.roots.end(),[&](auto id){return n.roots.count(id);}))candidates.push_back(n);
            return candidates;
        };
        auto outer=matching(neighbour(.5*(x+end)));
        if(outer.size()!=1 || !outer[0].closed || outer[0].width()>r.width() ||
           outer[0].height()>r.height() || std::abs(outer[0].area)>=-r.area)continue;
        // Beyond the mesh extremum no triangle can intersect the station.
        auto inner=matching(neighbour(x+(aft?1:-1)*2*extent));
        if(inner.size()!=1 || !inner[0].closed || inner[0].area<=0 ||
           inner[0].width()<r.width() || inner[0].height()<r.height())continue;
        for(auto i:r.edges)omit[i]=true;
    }
    size_t kept=0;
    for(size_t i=0;i<cuts.size();++i)if(!omit[i])cuts[kept++]=cuts[i];
    cuts.resize(kept);
}
