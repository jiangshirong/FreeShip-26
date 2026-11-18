// SPDX-License-Identifier: GPL-2.0-or-later
#include "preview_image.h"
#include <QBuffer>
#include <QImageReader>
#include <QImage>
#include <QJsonArray>
#include <QtEndian>
#include <stdexcept>

namespace fs::document {
Fbm withPreviewEnabled(const Fbm& source,bool enabled) {
    const auto info=source.inspect(),preview=info["preview"].toObject();
    if(preview["enabled"].toBool()==enabled)return source;
    if(source.version()<16||info["opaque_tail_bytes"].toInteger()>0)
        throw std::invalid_argument("Preview setting requires a supported FBM layout");
    qsizetype begin=-1;
    for(const auto& value:info["blocks"].toArray()){
        auto block=value.toObject();if(block["name"]=="project_settings")begin=block["offset"].toInteger();
    }
    auto settings=source.blockBytes("project_settings");
    const qsizetype offset=preview["flag_offset"].toInteger()-begin;
    const qsizetype size=enabled?1:13+preview["bytes"].toInteger();
    if(begin<0||offset<0||offset>=settings.size()||size>settings.size()-offset)
        throw std::invalid_argument("Invalid preview setting span");
    QByteArray replacement(1,enabled?1:0);
    if(enabled){
        // A valid empty canvas keeps headless/raw saves readable by Plus too.
        // ModelSession replaces it with the current four views before saving.
        static const QByteArray blank=[] {
            QImage image(400,300,QImage::Format_RGB32);image.fill(Qt::white);
            QByteArray jpeg;QBuffer output(&jpeg);output.open(QIODevice::WriteOnly);
            if(!image.save(&output,"JPEG",90))throw std::runtime_error("Cannot initialize preview JPEG");
            QByteArray envelope(12,'\0');
            qToLittleEndian<qint32>(400,envelope.data());qToLittleEndian<qint32>(300,envelope.data()+4);
            qToLittleEndian<qint32>(jpeg.size(),envelope.data()+8);return envelope+jpeg;
        }();
        replacement+=blank;
    }
    settings.replace(offset,size,replacement);
    return source.withBlocks({{"project_settings",settings}},false);
}

Fbm withPreviewImage(const Fbm& source,const QByteArray& jpeg) {
    // Old SaveBinary stores width, height, byte length, then the original JPEG.
    // Bound and decode the NEW image only; loading a document stays lossless.
    if(jpeg.size()<4||jpeg.size()>512*1024||!jpeg.startsWith(QByteArray::fromHex("ffd8"))||!jpeg.endsWith(QByteArray::fromHex("ffd9")))
        throw std::invalid_argument("Invalid preview JPEG payload");
    QBuffer buffer;buffer.setData(jpeg);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"JPEG");
    if(reader.size()!=QSize(400,300)||reader.read().isNull())
        throw std::invalid_argument("Preview must be a valid 400 x 300 JPEG");
    const auto info=source.inspect(),preview=info["preview"].toObject();
    if(!preview["enabled"].toBool()||info["opaque_tail_bytes"].toInteger()>0)return source;
    qsizetype begin=-1;
    for(const auto& value:info["blocks"].toArray()){
        auto block=value.toObject();if(block["name"]=="project_settings")begin=block["offset"].toInteger();
    }
    auto settings=source.blockBytes("project_settings");
    const qsizetype offset=preview["offset"].toInteger()-begin,size=12+preview["bytes"].toInteger();
    if(begin<0||offset<0||size<12||offset>settings.size()||size>settings.size()-offset)
        throw std::invalid_argument("Invalid preview span");
    QByteArray replacement(12,'\0');
    qToLittleEndian<qint32>(400,replacement.data());qToLittleEndian<qint32>(300,replacement.data()+4);
    qToLittleEndian<qint32>(qint32(jpeg.size()),replacement.data()+8);replacement+=jpeg;
    settings.replace(offset,size,replacement);
    return source.withBlocks({{"project_settings",settings}},false);
}
}
