// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QBitArray>
#include <QJsonObject>
#include <stdexcept>
#include <vector>

namespace fs::document {
// The ordinary FBM locked bit remains the effective lock for older readers.
// Only explicit group pins and points locked exclusively by them need extra data.
struct GroupFixes {
    static constexpr auto key = "point_group_fixes";
    QBitArray fixed, only;
    static bool has(const QBitArray& bits, int i) { return i>=0 && i<bits.size() && bits.testBit(i); }
    static void set(QBitArray& bits, int i, bool value) {
        if(i>=bits.size()) { if(!value)return; bits.resize(i+1); }
        bits.setBit(i,value);
    }
    static QString pack(const QBitArray& bits) {
        QByteArray bytes(bits.bits(),(bits.size()+7)/8);
        while(!bytes.isEmpty() && bytes.back()==0)bytes.chop(1);
        return QString::fromLatin1(bytes.toBase64());
    }
    static QBitArray unpack(const QJsonValue& v) {
        const auto bytes=QByteArray::fromBase64Encoding(v.toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        if(!v.isString() || !bytes)throw std::invalid_argument("Invalid point group lock metadata");
        return QBitArray::fromBits(bytes.decoded.constData(),bytes.decoded.size()*8);
    }
    static GroupFixes read(const QJsonObject& metadata) {
        if(!metadata.contains(key))return {};
        const auto o=metadata[key].toObject();return {unpack(o["groups"]),unpack(o["group_only_points"])};
    }
    void write(QJsonObject& metadata) const {
        const auto groups=pack(fixed);
        if(groups.isEmpty())metadata.remove(key);
        else metadata[key]=QJsonObject{{"groups",groups},{"group_only_points",pack(only)}};
    }
    void remap(const std::vector<int>& indices) {
        QBitArray next;for(int i=0;i<int(indices.size());++i)if(indices[i]>=0 && has(only,i))set(next,indices[i],true);
        only=std::move(next);
    }
};
}
