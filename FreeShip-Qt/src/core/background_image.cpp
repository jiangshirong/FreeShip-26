// SPDX-License-Identifier: GPL-2.0-or-later
#include "background_image.h"
#include <QBuffer>
#include <QCache>
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <stdexcept>
namespace fs {
namespace {
constexpr qsizetype maxBytes=32*1024*1024;
QCache<QByteArray,QImage> cache(128*1024); // KiB; snapshots share immutable pixels.
QMutex cacheMutex;
[[noreturn]] void bad(){throw std::runtime_error("Invalid background image or calibration");}
void sizeCheck(QSize size){if(size.isEmpty()||size.width()>16384||size.height()>16384||qint64(size.width())*size.height()>32*1024*1024)bad();}
QByteArray digest(const QByteArray& b){return QCryptographicHash::hash(b,QCryptographicHash::Sha256);}
int integer(QJsonValue v,int low,int high){double n=v.toDouble(NAN);if(!std::isfinite(n)||n<low||n>high||std::trunc(n)!=n)bad();return int(n);}
bool boolean(QJsonValue v){if(!v.isBool())bad();return v.toBool();}
QImage lookup(const QByteArray& key){QMutexLocker lock(&cacheMutex);auto* image=cache.object(key);return image?*image:QImage{};}
void retain(const QByteArray& key,const QImage& image){QMutexLocker lock(&cacheMutex);cache.insert(key,new QImage(image),int((image.sizeInBytes()+1023)/1024));}
}
QImage BackgroundImage::decode(const QByteArray& jpeg){
    if(jpeg.isEmpty()||jpeg.size()>maxBytes||!jpeg.startsWith(QByteArray::fromHex("ffd8")))bad();
    const auto key=QByteArray("jpeg:")+fs::digest(jpeg);if(auto cached=lookup(key);!cached.isNull())return cached;
    QBuffer input;input.setData(jpeg);input.open(QIODevice::ReadOnly);QImageReader reader(&input,"JPEG");
    reader.setAutoTransform(false);sizeCheck(reader.size());
    auto image=reader.read();if(image.isNull())bad();sizeCheck(image.size());
    image=image.convertToFormat(QImage::Format_RGB32);retain(key,image);return image;
}
QByteArray BackgroundImage::importFile(const QString& filename){
    QFile file(filename);if(!file.open(QIODevice::ReadOnly)||file.size()<=0||file.size()>maxBytes)bad();
    const auto bytes=file.readAll();if(bytes.startsWith(QByteArray::fromHex("ffd8"))){decode(bytes);return bytes;}
    // Match the original JPEG/BMP import formats. A bitmap is encoded once;
    // subsequent calibration, undo and saves keep that exact JPEG byte stream.
    if(!bytes.startsWith("BM"))bad();
    QBuffer input;input.setData(bytes);input.open(QIODevice::ReadOnly);QImageReader reader(&input,"BMP");sizeCheck(reader.size());
    auto pixels=reader.read();if(pixels.isNull())bad();
    QByteArray encoded;QBuffer output(&encoded);output.open(QIODevice::WriteOnly);
    if(!pixels.convertToFormat(QImage::Format_RGB32).save(&output,"JPEG",100))bad();decode(encoded);return encoded;
}
BackgroundImage BackgroundImage::fromJson(const QJsonObject& o){
    BackgroundImage b;b.view=integer(o["view"],0,2);b.visible=boolean(o["visible"]);
    auto origin=o["origin"].toArray();if(!o["origin"].isArray()||origin.size()!=2)bad();
    b.origin={integer(origin[0],-1000000000,1000000000),integer(origin[1],-1000000000,1000000000)};
    b.scale=o["scale"].toDouble(NAN);b.alpha=integer(o["alpha"],0,255);b.tolerance=integer(o["tolerance"],0,255);
    b.quality=integer(o["quality"],1,100);b.transparent=boolean(o["transparent"]);b.transparentColor=integer(o["transparent_color"],0,0xffffff);
    if(!o["jpeg"].isString()||o["jpeg"].toString().size()>(maxBytes+2)/3*4)bad();
    const auto encoded=o["jpeg"].toString().toLatin1();
    auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded||decoded.decoded.toBase64()!=encoded)bad();
    b.jpeg=std::move(decoded.decoded);b.digest=fs::digest(b.jpeg);b.pixels=decode(b.jpeg);b.validate();return b;
}
void BackgroundImage::validate()const{
    if(view<0||view>2||!std::isfinite(scale)||scale<1e-9||scale>1e9||
       std::abs(qint64(origin.x()))>1000000000||std::abs(qint64(origin.y()))>1000000000||
       alpha<0||alpha>255||tolerance<0||tolerance>255||quality<1||quality>100||transparentColor>0xffffff)bad();
    sizeCheck(pixels.size());if(jpeg.isEmpty()||jpeg.size()>maxBytes)bad();
}
QJsonObject BackgroundImage::metadata()const{return {{"view",view},{"visible",visible},{"origin",QJsonArray{origin.x(),origin.y()}},{"scale",scale},
    {"alpha",alpha},{"transparent",transparent},{"transparent_color",int(transparentColor)},{"tolerance",tolerance},{"quality",quality}};}
QImage BackgroundImage::maskedPixels()const{
    if(!transparent)return pixels;
    const auto key=QByteArray("mask:")+digest+':'+QByteArray::number(transparentColor)+':'+QByteArray::number(tolerance);
    if(auto cached=lookup(key);!cached.isNull())return cached;
    auto image=pixels.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int r=transparentColor&255,g=(transparentColor>>8)&255,b=(transparentColor>>16)&255;
    for(int y=0;y<image.height();++y){auto* row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<image.width();++x)
        if(std::abs(qRed(row[x])-r)<=tolerance&&std::abs(qGreen(row[x])-g)<=tolerance&&std::abs(qBlue(row[x])-b)<=tolerance)row[x]=0;}
    retain(key,image);return image;
}
QRectF BackgroundImage::targetRect(QPointF zero,double pixelsPerUnit)const{
    const double s=scale*pixelsPerUnit;return {zero-QPointF(origin)*s,QSizeF(pixels.size())*s};
}
QPointF BackgroundImage::imagePoint(QPointF screen,QPointF zero,double pixelsPerUnit)const{
    const auto rect=targetRect(zero,pixelsPerUnit);return (screen-rect.topLeft())/(scale*pixelsPerUnit);
}
}
