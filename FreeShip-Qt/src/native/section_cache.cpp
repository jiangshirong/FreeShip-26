// SPDX-License-Identifier: GPL-2.0-or-later
#include "section_cache.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QElapsedTimer>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace fs::native_geometry {
namespace {
constexpr qint64 EntryLimit=4*1024*1024;
const QByteArray Magic="FSCUT004\n";
QByteArray hash(const QByteArray& data){return QCryptographicHash::hash(data,QCryptographicHash::Blake2b_256).toHex();}
template<class T> void add(QByteArray& data,T value){data.append(reinterpret_cast<const char*>(&value),sizeof(value));}
template<class T> void add(std::vector<quint32>& data,T value){data.push_back(quint32(value));}
template<class T> QByteArray hash(const std::vector<T>& data){return hash(QByteArray::fromRawData(reinterpret_cast<const char*>(data.data()),qsizetype(data.size()*sizeof(T))));}
quint32 bits(float value){quint32 result;std::memcpy(&result,&value,4);return result;}
void point(std::vector<quint32>& data,Vec3 p){data.push_back(bits(p.x));data.push_back(bits(p.y));data.push_back(bits(p.z));}
bool plain(const QString& name){QFileInfo f(name);return !f.isSymLink()&&(!f.exists()||f.isFile());}
QByteArray read(const QString& name){
    if(!plain(name))throw std::runtime_error("Nonregular section cache");
    QFile f(name);if(!f.open(QIODevice::ReadOnly)||f.size()>EntryLimit)throw std::runtime_error("Missing section cache");
    const auto data=f.readAll();
    if(!data.startsWith(Magic)||data.size()<Magic.size()+65)throw std::runtime_error("Invalid section cache");
    const auto payload=data.mid(Magic.size()+65);
    if(data.mid(Magic.size(),64)!=hash(payload)||data[Magic.size()+64]!='\n')throw std::runtime_error("Section cache checksum");
    return payload;
}
CachedCurves decode(const QByteArray& payload,const QByteArray& key){
    const auto root=QJsonDocument::fromJson(payload).object();
    if(root["key"].toString().toLatin1()!=key||!root["multiple"].isBool()||!root["curves"].isArray())throw std::runtime_error("Invalid cut metadata");
    CachedCurves result;result.multiple=root["multiple"].toBool();qsizetype count=0;
    for(const auto& value:root["curves"].toArray()){
        static_assert(sizeof(Vec3)==12);
        const auto object=value.toObject();
        const auto points=QByteArray::fromBase64(object["points"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        const auto flags=QByteArray::fromBase64(object["knuckles"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        const double fragments=object["fragments"].toDouble(-1);const auto size=points.size()/sizeof(Vec3);count+=size;
        if(points.size()%sizeof(Vec3)||size<2||size!=size_t(flags.size())||count>1000000||fragments<0||fragments>4000000||std::floor(fragments)!=fragments)throw std::runtime_error("Invalid cut size");
        SectionCurve curve;curve.fragments=std::size_t(fragments);
        curve.points.resize(size);std::memcpy(curve.points.data(),points.constData(),points.size());
        for(size_t i=0;i<size;++i){
            const auto p=curve.points[i];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||(flags[i]!=0&&flags[i]!=1))throw std::runtime_error("Invalid cut point");
            curve.knuckles.push_back(flags[i]!=0);
        }
        result.curves.push_back(std::move(curve));
    }
    return result;
}
QByteArray encode(const CachedCurves& cut,const QByteArray& key){
    QJsonArray curves;
    for(const auto& curve:cut.curves){
        const auto points=QByteArray::fromRawData(reinterpret_cast<const char*>(curve.points.data()),qsizetype(curve.points.size()*sizeof(Vec3)));
        QByteArray flags(qsizetype(curve.knuckles.size()),0);for(size_t i=0;i<curve.knuckles.size();++i)flags[qsizetype(i)]=curve.knuckles[i]?1:0;
        curves.append(QJsonObject{{"points",QString::fromLatin1(points.toBase64())},{"knuckles",QString::fromLatin1(flags.toBase64())},{"fragments",double(curve.fragments)}});
    }
    return QJsonDocument(QJsonObject{{"key",QString::fromLatin1(key)},{"multiple",cut.multiple},{"curves",curves}}).toJson(QJsonDocument::Compact);
}
}
SectionCache::SectionCache(const SubdivisionLevel& mesh,const QString& directory,const QByteArray& engine):directory_(directory),engine_(engine){
    QElapsedTimer timer;timer.start();
    if(directory_.isEmpty()||QFileInfo(directory_).isSymLink()||!QDir().mkpath(directory_)){directory_.clear();return;}
    try{
        const auto root=QJsonDocument::fromJson(read(QDir(directory_).filePath("cuts.fscache"))).object();
        const auto entries=root["entries"].toObject();const auto order=root["order"].toArray();
        if(entries.size()>128||order.size()!=entries.size())throw std::runtime_error("Invalid section cache bundle");
        for(const auto& value:order){const auto key=value.toString();if(!entries.contains(key)||order_.contains(key))throw std::runtime_error("Invalid section cache order");order_.append(key);}
        entries_=entries;
    }catch(const std::exception&){entries_={};order_.clear();}
    timings["read_ms"]=timer.nsecsElapsed()/1e6;timer.restart();
    // Topology affects connectivity even outside a cut. Coordinates only affect
    // roots selected by the exact same conservative bounds as SectionIndex.
    // Validated mesh IDs/counts are bounded below 2^32 by the geometry reader.
    std::vector<quint32> topology;topology.reserve(mesh.faces.size()*12+mesh.edges.size()*8+mesh.points.size());add(topology,mesh.level);add(topology,mesh.points.size());
    for(const auto& p:mesh.points)add(topology,int(p.type));
    add(topology,mesh.edges.size());
    for(const auto& e:mesh.edges){for(auto p:e.points)add(topology,p);add(topology,e.crease);add(topology,e.curve);add(topology,e.reversed);add(topology,e.faces.size());for(auto f:e.faces)add(topology,f);}
    add(topology,mesh.faces.size());std::size_t count=0;
    for(const auto& f:mesh.faces)count=std::max(count,f.rootControlFace+1);
    std::vector<std::vector<quint32>> roots(count);
    for(const auto& f:mesh.faces){
        add(topology,f.rootControlFace);add(topology,f.childIndex);add(topology,f.layer);add(topology,f.points.size());
        for(auto p:f.points){add(topology,p);point(roots[f.rootControlFace],mesh.points.at(p).position);}
    }
    // BLAKE2b retains a 256-bit content key without hashing the same root bytes
    // again for every station, waterline and buttock.
    timings["dependency_ms"]=timer.nsecsElapsed()/1e6;timer.restart();
    topology_=hash(topology);for(const auto& root:roots)roots_.push_back(hash(root));
    timings["hash_ms"]=timer.nsecsElapsed()/1e6;
}
CachedCurves SectionCache::intersect(const SectionIndex& index,const Plane& plane){
    QByteArray key;QString name;
    if(!directory_.isEmpty()){
        QByteArray data=engine_+topology_;for(auto c:plane.coefficients())add(data,c);
        const auto roots=index.selectedRoots(plane);add(data,roots.size());
        for(auto root:roots){add(data,root);add(data,roots_.at(root).size());data+=roots_.at(root);}
        key=hash(data);name=QString::fromLatin1(key);
        try{
            auto value=decode(QJsonDocument(entries_[name].toObject()).toJson(QJsonDocument::Compact),key);
            order_.removeAll(name);order_.append(name);dirty_=true;++hits;return value;
        }catch(const std::exception&){}
    }
    ++misses;auto calculated=index.intersect(plane);
    CachedCurves result{std::move(calculated.curves),calculated.connectedCurves.size()>1};
    if(!name.isEmpty()){
        const auto payload=encode(result,key);
        if(payload.size()<EntryLimit/2){
            entries_[name]=QJsonDocument::fromJson(payload).object();order_.removeAll(name);order_.append(name);dirty_=true;
            while(order_.size()>128)entries_.remove(order_.takeFirst());
        }
    }
    return result;
}
void SectionCache::trim(){
    if(directory_.isEmpty()||!dirty_)return;
    while(order_.size()>128)entries_.remove(order_.takeFirst());
    QByteArray payload;
    do{
        payload=QJsonDocument(QJsonObject{{"entries",entries_},{"order",QJsonArray::fromStringList(order_)}}).toJson(QJsonDocument::Compact);
        if(payload.size()+Magic.size()+65<=EntryLimit)break;
        entries_.remove(order_.takeFirst());
    }while(!order_.isEmpty());
    const auto path=QDir(directory_).filePath("cuts.fscache");if(!plain(path))return;
    const auto data=Magic+hash(payload)+'\n'+payload;
    QSaveFile f(path);f.setDirectWriteFallback(false);
    if(f.open(QIODevice::WriteOnly)&&f.write(data)==data.size())f.commit();
}
}
