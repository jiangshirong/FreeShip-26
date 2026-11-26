// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <array>
#include <optional>
#include <algorithm>
#include <stdexcept>

namespace fs {
// Byte spans are indexed once, AFTER the complete snapshot has been parsed and
// validated by its owner. Replacing indexed top-level fields never reformats or
// traverses the unchanged geometry. A copy owns immutable, implicitly shared bytes.
class SnapshotMetadata {
    struct Span {qsizetype begin=0,end=0;int field=0;};
    QByteArray bytes_;
    QJsonObject settings_;
    QString name_;
    std::array<Span,4> spans_{};
    static void bad(){throw std::runtime_error("Invalid snapshot metadata envelope");}
    static bool space(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
    static qsizetype quoted(const QByteArray &b,qsizetype i) {
        if(i>=b.size()||b[i]!='"')bad();
        while(++i<b.size()) {if(b[i]=='\\'){++i;continue;}if(b[i]=='"')return i+1;}
        bad();return 0;
    }
    static QByteArray value(const QJsonValue &v) {
        const auto array=QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact);
        return array.mid(1,array.size()-2);
    }
public:
    bool empty() const {return bytes_.isEmpty();}
    const QByteArray &bytes() const {return bytes_;}
    static SnapshotMetadata capture(const QByteArray &bytes,const QJsonObject &settings,const QString &name) {
        SnapshotMetadata result;result.bytes_=bytes;result.settings_=settings;result.name_=name;
        qsizetype i=0;while(i<bytes.size()&&space(bytes[i]))++i;
        if(i>=bytes.size()||bytes[i++]!='{')bad();
        std::array<bool,4> seen{};
        while(i<bytes.size()) {
            while(i<bytes.size()&&(space(bytes[i])||bytes[i]==','))++i;
            if(i>=bytes.size()||bytes[i]=='}')break;
            const auto start=i;i=quoted(bytes,i);
            const auto key=QJsonDocument::fromJson('['+bytes.mid(start,i-start)+']').array().at(0).toString();
            while(i<bytes.size()&&space(bytes[i]))++i;
            if(i>=bytes.size()||bytes[i++]!=':')bad();
            while(i<bytes.size()&&space(bytes[i]))++i;
            const auto begin=i;int depth=0;
            while(i<bytes.size()) {
                const char c=bytes[i];
                if(c=='"'){i=quoted(bytes,i);continue;}
                if(depth==0&&(c==','||c=='}'))break;
                if(c=='{'||c=='[')++depth;else if(c=='}'||c==']')--depth;
                if(depth<0)bad();++i;
            }
            if(depth||i>=bytes.size())bad();
            qsizetype end=i;while(end>begin&&space(bytes[end-1]))--end;
            const int field=key=="name"?0:key=="revision"?1:key=="settings"?2:key=="last_layer_number"?3:-1;
            if(field>=0){if(seen[field])bad();seen[field]=true;result.spans_[field]={begin,end,field};}
        }
        if(!seen[0]||!seen[1]||!seen[2])bad();
        std::sort(result.spans_.begin(),result.spans_.end(),[](const Span&a,const Span&b){return a.begin<b.begin;});return result;
    }
    // Only used by the private, bounded history cache. Offsets still refer to
    // the validated uncompressed bytes; expand before applying a revision.
    SnapshotMetadata packedForHistory() const {auto copy=*this;copy.bytes_=qCompress(bytes_,1);return copy;}
    SnapshotMetadata expandedFromHistory() const {auto copy=*this;copy.bytes_=qUncompress(bytes_);if(copy.bytes_.isEmpty())bad();return copy;}
    SnapshotMetadata updated(const QJsonObject &patch,quint64 revision,std::optional<int> historyLayer={}) const {
        if(empty())bad();SnapshotMetadata result=*this;
        for(auto i=patch.begin();i!=patch.end();++i)result.settings_[i.key()]=i.value();
        if(patch.contains("name"))result.name_=patch["name"].toString();
        const std::array<QByteArray,4> replacement{value(result.name_),QByteArray::number(revision),
            QJsonDocument(result.settings_).toJson(QJsonDocument::Compact),historyLayer?QByteArray::number(*historyLayer):QByteArray{}};
        if(historyLayer&&std::none_of(spans_.begin(),spans_.end(),[](const Span& s){return s.end&&s.field==3;}))bad();
        QByteArray next;next.reserve(bytes_.size());qsizetype at=0;
        const auto append=[&](const char *data,qsizetype size){if(size>256LL*1024*1024-next.size())bad();next.append(data,size);};
        for(size_t j=0;j<spans_.size();++j) {
            const auto s=spans_[j];if(!s.end)continue;
            append(bytes_.constData()+at,s.begin-at);const auto begin=next.size();
            if(s.field==3&&!historyLayer)append(bytes_.constData()+s.begin,s.end-s.begin);
            else {const auto &v=replacement[s.field];append(v.constData(),v.size());}
            result.spans_[j]={begin,next.size(),s.field};at=s.end;
        }
        append(bytes_.constData()+at,bytes_.size()-at);result.bytes_=std::move(next);return result;
    }
};
}
