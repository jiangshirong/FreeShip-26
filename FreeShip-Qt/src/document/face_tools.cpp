// GPL-2.0-or-later; see COPYING. Native adaptation of ModelTools.inc and
// FreeSubdivisionSurface.AssembleFacesToPatches (amNurbs), plus
// FreeSubdivisionPoint.IsRegularNURBSPoint / FreeSubdivisionFace.FlipNormal.
// Original FREE!ship: Martijn van Engeland (2005); Plus: Timoshenko V.F.
// (2006-2011), Mark Malakanov (2015). Pointer-sorted corner selection is made
// deterministic by stable face ID. Geometry and regularity rules are retained.
#include "face_tools.h"
#include "fbm_text.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) {throw std::invalid_argument(message);}
int integer(QJsonValue value,int minimum=1,int maximum=2000000) {
    const double n=value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n<minimum || n>maximum || std::floor(n)!=n) bad("Invalid face-tool integer");
    return int(n);
}
quint64 pairKey(int a,int b) {if (a>b) std::swap(a,b);return (quint64(a)<<32)|quint32(b);}
void put(QByteArray &data,int value) {char bytes[4];qToLittleEndian<qint32>(value,bytes);data.append(bytes,4);}
void patchInt(QByteArray &data,qsizetype offset,int value) {if (offset<0 || offset+4>data.size()) bad("Invalid layer field span");qToLittleEndian<qint32>(value,data.data()+offset);}
struct Reader {
    QByteArray data;qsizetype at=0;
    QByteArray take(qsizetype n) {if (n<0 || n>data.size()-at) bad("Invalid layer block");auto out=data.mid(at,n);at+=n;return out;}
    int readInt() {if (data.size()-at<4) bad("Invalid face-tool block");const int value=qFromLittleEndian<qint32>(data.constData()+at);at+=4;return value;}
    QByteArray string() {return take(readInt());}
};
struct Layer {
    QByteArray name,fields;
    int stableId=0;
    std::vector<int> faces;
    bool visible() const {return fields[8]!=0;}
    int internalId() const {return qFromLittleEndian<qint32>(fields.constData());}
};

class Faces {
    const Fbm &source_;
    QJsonObject bindings_,high_;
    FbmControlNet net_;
    std::vector<Layer> layers_;
    std::vector<int> faceIds_,stableOrder_;
    QHash<int,int> faceLookup_;
    QHash<quint64,int> edgeLookup_;
    std::vector<std::vector<int>> pointFaces_,edgeFaces_;
    int highLayer_=0,lastLayer_=-1;
    bool changed_=false,geometry_=false;
    static bool contains(const std::vector<int> &values,int v) {return std::find(values.begin(),values.end(),v)!=values.end();}
    int edge(int a,int b) const {return edgeLookup_.value(pairKey(a,b),-1);}
    bool crease(int e) const {return net_.edges[size_t(e)].crease;}
    int layerOf(int f) const {return std::max(0,net_.faces[size_t(f)].layer);}
    QByteArray faceBlock(const std::vector<int> *mapping=nullptr) const {
        Reader reader{source_.blockBytes("control_faces")};const int count=reader.readInt();
        if (size_t(count)!=net_.faces.size()) bad("Face block cardinality changed");
        for (const auto &face:net_.faces) {
            const int n=reader.readInt();if (size_t(n)!=face.points.size()) bad("Face point cardinality changed");
            for (int point:face.points) {const auto at=reader.at;const int previous=reader.readInt();if (point!=previous) patchInt(reader.data,at,point);}
            const auto at=reader.at;const int previous=reader.readInt();
            const int layer=mapping ? mapping->at(size_t(std::max(0,face.layer))) : face.layer;
            if (layer!=previous) patchInt(reader.data,at,layer);
            // Keep the original selection-byte representation verbatim.
            reader.take(1);
        }
        if (reader.at!=reader.data.size()) bad("Unexpected face block data");
        return reader.data;
    }
    void move(int face,int layer) {
        const int previous=layerOf(face);if (previous==layer) return;
        auto &old=layers_[size_t(previous)].faces;old.erase(std::remove(old.begin(),old.end(),face),old.end());
        layers_[size_t(layer)].faces.push_back(face);net_.faces[size_t(face)].layer=layer;
    }
    int addLayer(int original,const QByteArray &name) {
        if (highLayer_>=2000000 || layers_.size()>=2000000 || lastLayer_==std::numeric_limits<int>::max()) bad("Layer ID budget exhausted");
        Layer layer=layers_.at(size_t(original));layer.faces.clear();layer.name=name;layer.stableId=++highLayer_;
        patchInt(layer.fields,0,++lastLayer_);
        // AssignProperties + CopyLayerProperties copy every serialized scalar
        // except AlphaBlend. New old-core layers retain their default 255.
        if (source_.version()>=21) patchInt(layer.fields,layer.fields.size()-4,255);
        layers_.push_back(std::move(layer));return int(layers_.size()-1);
    }
    void editable(int face) const {
        for (int i:net_.faces[size_t(face)].points) {const auto &p=net_.points[size_t(i)].value;
            if (p.locked || p.hardAnchor || p.constrained) bad("Face repair touches a locked or constrained point");}
    }
    bool regular(int point,const std::vector<bool> &component) const {
        if (point<0 || size_t(point)>=net_.points.size()) bad("Invalid NURBS patch boundary");
        const int type=net_.points[size_t(point)].value.vertexType;
        if (type==0 || type==2) return pointFaces_[size_t(point)].size()==4;
        if (type!=1) return false;
        int count=0;for (int f:pointFaces_[size_t(point)]) if (component[size_t(f)]) ++count;
        return count==2;
    }
    int faceAt(int a,int b,int c,int d) const {
        for (int f:pointFaces_[size_t(a)]) {
            const auto &p=net_.faces[size_t(f)].points;
            if (contains(p,b) && contains(p,c) && contains(p,d)) return f;
        }
        return -1;
    }
    std::array<int,2> opposite(int face,int a,int b) const {
        const auto &p=net_.faces[size_t(face)].points;
        const auto ai=std::find(p.begin(),p.end(),a),bi=std::find(p.begin(),p.end(),b);
        if (p.size()!=4 || ai==p.end() || bi==p.end()) bad("Invalid quad boundary");
        const size_t i=size_t(ai-p.begin()),j=size_t(bi-p.begin());
        if ((i+1)%4==j) return {p[(i+3)%4],p[(i+2)%4]};
        if ((j+1)%4==i) return {p[(j+2)%4],p[(j+3)%4]};
        bad("Patch edge is not on a quad boundary");
    }
    using Grid=std::vector<std::vector<int>>;
    void grow(Grid &grid,std::vector<bool> &remaining,const std::vector<bool> &component,int &left) const {
        std::array<bool,4> search{true,true,true,true}; // bottom, right, top, left
        const auto corners=[&]() {
            const auto rows=grid.size(),cols=grid.front().size();
            if (!regular(grid[0][0],component)) search[3]=search[2]=false;
            if (!regular(grid[rows-1][0],component)) search[3]=search[0]=false;
            if (!regular(grid[rows-1][cols-1],component)) search[1]=search[0]=false;
            if (!regular(grid[0][cols-1],component)) search[1]=search[2]=false;
        };
        corners();size_t side=0;
        while (left>0 && std::any_of(search.begin(),search.end(),[](bool b){return b;})) {
            if (search[side]) {
                const size_t rows=grid.size(),cols=grid.front().size();std::vector<int> boundary,candidates;
                if (side==0) boundary=grid.back();else if (side==2) boundary=grid.front();
                else for (const auto &row:grid) boundary.push_back(side==1 ? row.back() : row.front());
                for (size_t i=1;i<boundary.size();++i) {
                    const int e=edge(boundary[i-1],boundary[i]);int chosen=-1;
                    if (e>=0 && !crease(e)) for (int f:edgeFaces_[size_t(e)]) {
                        const auto &points=net_.faces[size_t(f)].points;
                        if (!remaining[size_t(f)] || points.size()!=4 || contains(candidates,f)) continue;
                        if (!candidates.empty()) {
                            const auto &previous=net_.faces[size_t(candidates.back())].points;int common=0;
                            for (int p:points) if (contains(previous,p)) ++common;
                            if (common<2) continue;
                        }
                        chosen=f;break;
                    }
                    if (chosen<0) break;
                    candidates.push_back(chosen);
                }
                if (candidates.size()+1!=boundary.size()) search[side]=false;
                else {
                    std::vector<int> added(boundary.size(),-1);
                    for (size_t i=0;i<candidates.size();++i) {const auto p=opposite(candidates[i],boundary[i],boundary[i+1]);added[i]=p[0];added[i+1]=p[1];}
                    // Old code allows the later face to overwrite a shared
                    // new vertex. Keep that ordering, then validate references.
                    for (int p:added) if (p<0) bad("Incomplete NURBS patch row");
                    Grid next=grid;
                    if (side==0) next.push_back(added);
                    else if (side==2) next.insert(next.begin(),added);
                    else for (size_t row=0;row<rows;++row) {
                        if (side==1) next[row].push_back(added[row]);else next[row].insert(next[row].begin(),added[row]);
                    }
                    bool valid=true;
                    const auto compatible=[&](int a,int b,int c) {const int first=edge(a,b),second=edge(b,c);return first<0 || second<0 || crease(first)==crease(second);};
                    if (side==0) valid=compatible(next[rows][0],next[rows-1][0],next[rows-2][0]) && compatible(next[rows][cols-1],next[rows-1][cols-1],next[rows-2][cols-1]);
                    if (side==2) valid=compatible(next[0][0],next[1][0],next[2][0]) && compatible(next[0][cols-1],next[1][cols-1],next[2][cols-1]);
                    if (side==1) valid=compatible(next[0][cols],next[0][cols-1],next[0][cols-2]) && compatible(next[rows-1][cols],next[rows-1][cols-1],next[rows-1][cols-2]);
                    if (side==3) valid=compatible(next[0][0],next[0][1],next[0][2]) && compatible(next[rows-1][0],next[rows-1][1],next[rows-1][2]);
                    if (side==0 || side==2) {for (size_t i=1;i+1<added.size();++i) if (!regular(added[i],component)) valid=false;}
                    else {
                        // Intentional legacy I-2 indexing: test rows 0..R-3,
                        // rather than silently correcting it to interior rows.
                        for (size_t i=0;i+2<added.size();++i) if (!regular(added[i],component)) valid=false;
                    }
                    if (!valid) search[side]=false;
                    else {grid=std::move(next);for (int f:candidates) {remaining[size_t(f)]=false;--left;}}
                }
            }
            corners();side=(side+1)%4;
        }
    }
    std::vector<std::vector<int>> patches(const std::vector<int> &layerFaces,std::vector<bool> &todo,
                                         std::vector<bool> &check,std::vector<bool> &remaining) const {
        std::vector<std::vector<int>> result;
        for (int f:layerFaces) todo[size_t(f)]=true;
        // FindAttachedFaces uses depth-first traversal, starting at each last
        // remaining face in the layer's insertion order, across smooth edges.
        for (auto seed=layerFaces.rbegin();seed!=layerFaces.rend();++seed) if (todo[size_t(*seed)]) {
            std::vector<int> component;std::vector<int> stack{*seed};todo[size_t(*seed)]=false;
            while (!stack.empty()) {
                const int f=stack.back();stack.pop_back();component.push_back(f);
                const auto &p=net_.faces[size_t(f)].points;
                // Component membership, not DFS order, is used after Sort.
                for (size_t i=0;i<p.size();++i) {const int e=edge(p[(i+p.size()-1)%p.size()],p[i]);if (e<0 || crease(e)) continue;
                    for (int g:edgeFaces_[size_t(e)]) if (todo[size_t(g)]) {todo[size_t(g)]=false;stack.push_back(g);}}
            }
            std::sort(component.begin(),component.end(),[&](int a,int b){return faceIds_[size_t(a)]<faceIds_[size_t(b)];});
            for (int f:component) check[size_t(f)]=remaining[size_t(f)]=true;
            int left=int(component.size());
            while (left) {
                int chosen=-1;
                for (int f:component) if (remaining[size_t(f)]) {
                    for (int p:net_.faces[size_t(f)].points) {bool other=false;for (int g:pointFaces_[size_t(p)]) if (g!=f && remaining[size_t(g)]) {other=true;break;}
                        if (!other) {chosen=f;break;}}
                    if (chosen>=0) break;
                }
                if (chosen<0) for (auto f=component.rbegin();f!=component.rend();++f) if (remaining[size_t(*f)]) {chosen=*f;break;}
                if (chosen<0) bad("No valid corner face found");
                remaining[size_t(chosen)]=false;--left;const auto &p=net_.faces[size_t(chosen)].points;
                if (p.size()!=4) {result.push_back({chosen});continue;}
                Grid grid{{p[1],p[0]},{p[2],p[3]}};grow(grid,remaining,check,left);std::vector<int> patch;
                for (size_t row=1;row<grid.size();++row) for (size_t col=1;col<grid[row].size();++col) {
                    const int f=faceAt(grid[row-1][col],grid[row-1][col-1],grid[row][col-1],grid[row][col]);
                    if (f>=0) patch.push_back(f);
                }
                result.push_back(std::move(patch));
            }
            for (int f:component) check[size_t(f)]=false;
        }
        return result;
    }
public:
    Faces(const Fbm &source,const QJsonObject &bindings):source_(source),bindings_(bindings),high_(bindings["high_water"].toObject()),net_(source.controlNet()) {
        const auto ids=bindings["faces"].toArray();if (size_t(ids.size())!=net_.faces.size()) bad("Face bindings do not match document");
        const int highFace=integer(high_["faces"],0);highLayer_=integer(high_["layers"],0);
        for (qsizetype i=0;i<ids.size();++i) {const int id=integer(ids[i]);if (id>highFace || faceLookup_.contains(id)) bad("Invalid stable face binding");faceIds_.push_back(id);faceLookup_.insert(id,int(i));stableOrder_.push_back(int(i));}
        std::sort(stableOrder_.begin(),stableOrder_.end(),[&](int a,int b){return faceIds_[size_t(a)]<faceIds_[size_t(b)];});
        Reader reader{source.blockBytes("layers")};const int count=reader.readInt();
        const int fields=11+(source.version()>=9?2:0)+(source.version()>=11?8:0)+(source.version()>=15?1:0)+(source.version()>=21?4:0);
        for (int i=0;i<count;++i) {Layer layer;layer.name=reader.string();layer.fields=reader.take(fields);lastLayer_=std::max(lastLayer_,layer.internalId());layers_.push_back(std::move(layer));}
        reader.readInt();if (reader.at!=reader.data.size()) bad("Unexpected layer data");
        if (layers_.empty()) {
            Layer layer;put(layer.fields,0);put(layer.fields,0xffff00);layer.fields.append("\1\1\0",3);
            if (source.version()>=9) layer.fields.append("\1\1",2);
            if (source.version()>=11) layer.fields.append(QByteArray(8,'\0'));
            if (source.version()>=15) layer.fields.append('\1');
            if (source.version()>=21) put(layer.fields,255);
            layers_.push_back(std::move(layer));lastLayer_=0;
        }
        const auto layerIds=bindings["layers"].toArray();if (size_t(layerIds.size())!=layers_.size()) bad("Layer bindings do not match document");QSet<int> used;
        for (int i=0;i<layerIds.size();++i) {const int id=integer(layerIds[i]);if (id>highLayer_ || used.contains(id)) bad("Invalid stable layer binding");used.insert(id);layers_[size_t(i)].stableId=id;}
        if (bindings.contains("last_used_layer_id")) {const int value=integer(bindings["last_used_layer_id"],-1,std::numeric_limits<int>::max());if (value<lastLayer_) bad("Stale layer ID high-water mark");lastLayer_=value;}
        if (bindings.contains("layer_faces")) {
            const auto rows=bindings["layer_faces"].toArray();if (!bindings["layer_faces"].isArray() || size_t(rows.size())!=layers_.size()) bad("Invalid layer face order");QSet<int> seen;
            for (int layer=0;layer<rows.size();++layer) {if (!rows[layer].isArray()) bad("Invalid layer face row");for (auto value:rows[layer].toArray()) {
                const int id=integer(value);if (!faceLookup_.contains(id) || seen.contains(id)) bad("Invalid ordered layer face");const int f=faceLookup_[id];
                if (layerOf(f)!=layer) bad("Ordered face belongs to a different layer");
                seen.insert(id);layers_[size_t(layer)].faces.push_back(f);
            }}
            if (size_t(seen.size())!=net_.faces.size()) bad("Incomplete layer face order");
        } else for (size_t i=0;i<net_.faces.size();++i) layers_.at(size_t(layerOf(int(i)))).faces.push_back(int(i));
        pointFaces_.resize(net_.points.size());edgeFaces_.resize(net_.edges.size());
        for (size_t i=0;i<net_.edges.size();++i) {const auto &e=net_.edges[i];const auto key=pairKey(e.a,e.b);if (edgeLookup_.contains(key)) bad("Duplicate face edge");edgeLookup_.insert(key,int(i));}
        for (size_t i=0;i<net_.faces.size();++i) {const auto &p=net_.faces[i].points;for (size_t j=0;j<p.size();++j) {pointFaces_[size_t(p[j])].push_back(int(i));const int e=edge(p[j],p[(j+1)%p.size()]);if (e>=0) edgeFaces_[size_t(e)].push_back(int(i));}}
    }
    void autoGroup(const QJsonObject &request) {
        std::vector<bool> chosen(net_.faces.size(),false),visited(net_.faces.size(),false);
        if (request.contains("ids")) {
            if (!request["ids"].isArray()) bad("Invalid face selection");
            const auto ids=request["ids"].toArray();if (ids.isEmpty() || ids.size()>100000) bad("Invalid face selection size");QSet<int> seen;
            for (auto value:ids) {const int id=integer(value);if (!faceLookup_.contains(id) || seen.contains(id)) bad("Unknown or duplicate face ID");seen.insert(id);chosen[size_t(faceLookup_[id])]=true;}
        } else for (size_t i=0;i<net_.faces.size();++i) chosen[i]=layers_[size_t(layerOf(int(i)))].visible();
        for (int seed:stableOrder_) if (chosen[size_t(seed)] && !visited[size_t(seed)]) {
            std::vector<int> queue{seed};visited[size_t(seed)]=true;
            for (size_t head=0;head<queue.size();++head) {const int f=queue[head];const auto &p=net_.faces[size_t(f)].points;
                for (size_t i=0;i<p.size();++i) {const int e=edge(p[i],p[(i+1)%p.size()]);if (e<0 || crease(e)) continue;
                    for (int g:edgeFaces_[size_t(e)]) if (chosen[size_t(g)] && !visited[size_t(g)] && layerOf(g)==layerOf(f)) {visited[size_t(g)]=true;queue.push_back(g);}}
            }
            const int original=layerOf(seed);if (queue.size()==layers_[size_t(original)].faces.size()) continue;
            const auto encoding=bindings_["fbm_encoding"].toString();
            QString name=decodeFbmText(layers_[size_t(original)].name,encoding);if (name.isEmpty()) name="Layer";
            const int target=addLayer(original,encodeFbmText(name+' '+QString::number(faceIds_[size_t(seed)]),encoding));for (int f:queue) move(f,target);changed_=true;
        }
    }
    void assemble() {
        const size_t count=layers_.size();
        // Reuse global-index bitmaps and clear only the touched component.
        // Many isolated faces/layers must not repeatedly clear O(all faces).
        std::vector<bool> todo(net_.faces.size(),false),check(net_.faces.size(),false),remaining(net_.faces.size(),false);
        for (size_t layer=0;layer<count;++layer) if (layers_[layer].visible()) {
            const auto assembled=patches(layers_[layer].faces,todo,check,remaining);
            for (size_t i=0;i<assembled.size();++i) {
                int target=-1;
                for (int f:assembled[i]) {if (target<0) target=addLayer(layerOf(f),encodeFbmText("Patch "+QString::number(layer+1)+'-'+QString::number(i+1),bindings_["fbm_encoding"].toString()));move(f,target);changed_=true;}
            }
        }
    }
    void repair() {
        std::vector<int> orientation(net_.faces.size(),0);
        const auto direction=[&](int face,int e) {const auto &p=net_.faces[size_t(face)].points;const auto &ends=net_.edges[size_t(e)];
            const auto a=std::find(p.begin(),p.end(),ends.a),b=std::find(p.begin(),p.end(),ends.b);if (a==p.end() || b==p.end()) bad("Invalid face-edge adjacency");
            const size_t ai=size_t(a-p.begin()),bi=size_t(b-p.begin());if ((ai+1)%p.size()==bi) return 1;if ((bi+1)%p.size()==ai) return -1;bad("Edge is not on the face boundary");};
        for (int seed:stableOrder_) if (!orientation[size_t(seed)]) {
            orientation[size_t(seed)]=1;std::vector<int> queue{seed};
            for (size_t head=0;head<queue.size();++head) {const int f=queue[head];const auto &p=net_.faces[size_t(f)].points;
                for (size_t i=0;i<p.size();++i) {const int e=edge(p[i],p[(i+1)%p.size()]);if (e<0) bad("Missing face edge");const auto &faces=edgeFaces_[size_t(e)];
                    if (faces.size()>2) bad("Repair nonmanifold topology before orienting faces");
                    if (faces.size()!=2) continue;
                    const int g=faces[0]==f?faces[1]:faces[0],next=-orientation[size_t(f)]*direction(f,e)*direction(g,e);
                    if (orientation[size_t(g)] && orientation[size_t(g)]!=next) bad("Surface cannot be consistently oriented");
                    if (!orientation[size_t(g)]) {orientation[size_t(g)]=next;queue.push_back(g);}
                }
            }
        }
        for (int f:stableOrder_) if (orientation[size_t(f)]<0) editable(f);
        for (int f:stableOrder_) if (orientation[size_t(f)]<0) {auto &p=net_.faces[size_t(f)].points;std::reverse(p.begin(),p.end());changed_=geometry_=true;}
    }
    TopologyEdit finish() {
        if (!changed_) return {source_,bindings_,false};
        if (geometry_) return {source_.withBlocks({{"control_faces",faceBlock()}},true),bindings_,true};
        std::vector<int> mapping(layers_.size(),-1);QByteArray layerBytes;QJsonArray layerIds,layerFaces;int count=0;
        for (size_t i=0;i<layers_.size();++i) if (!layers_[i].faces.empty()) mapping[i]=count++;
        if (!count) bad("Face grouping produced no nonempty layers");
        put(layerBytes,count);
        for (size_t i=0;i<layers_.size();++i) if (mapping[i]>=0) {
            const auto &layer=layers_[i];put(layerBytes,int(layer.name.size()));layerBytes+=layer.name;layerBytes+=layer.fields;layerIds.append(layer.stableId);
            QJsonArray row;for (int f:layer.faces) row.append(faceIds_[size_t(f)]);layerFaces.append(row);
        }
        put(layerBytes,0);const auto faceBytes=faceBlock(&mapping);
        bindings_["layers"]=layerIds;bindings_["layer_faces"]=layerFaces;bindings_["last_used_layer_id"]=lastLayer_;high_["layers"]=highLayer_;bindings_["high_water"]=high_;
        return {source_.withBlocks({{"layers",layerBytes},{"control_faces",faceBytes}},true),bindings_,true};
    }
};
}

bool nativeFaceToolsCommand(const QJsonObject &request) {
    if (request["method"]!="modeling") return false;
    const auto operation=request["operation"].toString();return operation=="auto_group_layers" || operation=="assemble_faces" || operation=="repair_normals";
}
TopologyEdit editFaceTools(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if (!nativeFaceToolsCommand(request)) bad("Unsupported native face tool");
    Faces faces(source,bindings);const auto operation=request["operation"].toString();
    if (operation=="auto_group_layers") faces.autoGroup(request);else if (operation=="assemble_faces") faces.assemble();else faces.repair();return faces.finish();
}
}
