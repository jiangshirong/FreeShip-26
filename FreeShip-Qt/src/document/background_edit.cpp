// GPL-2.0-or-later; see COPYING. Semantics from bridge/BackgroundEdits.inc,
// FreeBackgroundImageData.inc and FreeFileBuffer.LoadTJPEGImage/SaveBinary.
// The isolated bridge deliberately skips CompressionQuality assignment during
// saving; this writer likewise retains the exact original JPEG byte stream.
#include "background_edit.h"
#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace fs::document {
namespace {
constexpr qsizetype MaximumJpegBytes=32*1024*1024;
[[noreturn]] void bad(const char *message) { throw std::invalid_argument(message); }
int integer(QJsonValue value,int low,int high) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || n<low || n>high || std::floor(n)!=n)
        bad("Invalid background integer");
    return int(n);
}
float scale(QJsonValue value) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || std::abs(n)>1e9) bad("Invalid background scale");
    const float result=float(n);
    // Deliberately compare the stored Single with the original Double literal.
    if (result<1e-9) bad("Background scale must be positive");
    return result;
}
bool flag(QJsonValue value) {
    if (!value.isBool()) bad("Invalid background Boolean");
    return value.toBool();
}
QString text(QJsonValue value) {
    if (!value.isString()) bad("Invalid background text");
    const auto result=value.toString();
    if (result.isEmpty() || result.contains(QChar(0)) || !result.isValidUtf16()) bad("Invalid background text");
    return result;
}
void sizeCheck(QSize size) {
    if (size.width()<=0 || size.height()<=0 || size.width()>16384 || size.height()>16384 ||
        qint64(size.width())*size.height()>32*1024*1024) bad("Background dimensions exceed limits");
}
QSize imageSize(const QByteArray &jpeg,bool decode) {
    if (jpeg.size()<4 || jpeg.size()>MaximumJpegBytes || !jpeg.startsWith(QByteArray::fromHex("ffd8")))
        bad("Invalid background JPEG size or signature");
    QBuffer input;input.setData(jpeg);input.open(QIODevice::ReadOnly);
    QImageReader reader(&input,"JPEG");reader.setAutoTransform(false);
    reader.setDecideFormatFromContent(false);const auto size=reader.size();sizeCheck(size);
    if (decode) {
        const auto pixels=reader.read();
        if (pixels.isNull() || pixels.size()!=size) bad("Cannot decode background JPEG");
        sizeCheck(pixels.size());
    }
    return size;
}
QByteArray imageFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size()<4 || file.size()>MaximumJpegBytes)
        bad("Cannot read bounded background JPEG");
    const auto size=file.size();auto bytes=file.read(MaximumJpegBytes+1);
    if (file.error()!=QFile::NoError || bytes.size()!=size) bad("Background JPEG changed while reading");
    return bytes;
}
void putInt(QByteArray &bytes,int value) { char b[4];qToLittleEndian<qint32>(value,b);bytes.append(b,4); }
void putFloat(QByteArray &bytes,float value) { quint32 bits;std::memcpy(&bits,&value,4);char b[4];qToLittleEndian(bits,b);bytes.append(b,4); }
struct Reader {
    const QByteArray &bytes;qsizetype at=0;
    void skip(qsizetype n) { if (n<0 || n>bytes.size()-at) bad("Truncated background block");at+=n; }
    int integer() { const auto p=at;skip(4);return qFromLittleEndian<qint32>(bytes.constData()+p); }
    float real() { const auto bits=quint32(integer());float result;std::memcpy(&result,&bits,4);return result; }
    bool flag() { const auto p=at;skip(1);return bytes[p]!=0; }
};
struct Background {
    int view=0,quality=100,x=0,y=0,alpha=255,color=0,tolerance=3,width=0,height=0;
    bool visible=true,transparent=false;float scale=10;
    QByteArray jpeg,raw;
    QByteArray bytes() const {
        QByteArray result;putInt(result,view);result.append(visible?char(1):char(0));putInt(result,quality);
        putInt(result,x);putInt(result,y);putFloat(result,scale);putInt(result,alpha);
        result.append(transparent?char(1):char(0));putInt(result,color);putInt(result,tolerance);
        putInt(result,width);putInt(result,height);putInt(result,int(jpeg.size()));result.append(jpeg);return result;
    }
    void check() const {
        if (view<0 || view>2 || !std::isfinite(scale) || scale<1e-9 || std::abs(scale)>1e9f ||
            std::abs(qint64(x))>1000000000 || std::abs(qint64(y))>1000000000 ||
            alpha<0 || alpha>255 || tolerance<0 || tolerance>255 || quality<1 || quality>100 || color<0 || color>0xffffff)
            bad("Invalid stored background appearance or calibration");
        // Stored W/H are legacy envelopes; LoadTJPEGImage ignores them and
        // obtains the dimensions from the JPEG. Do not silently rewrite them.
        imageSize(jpeg,false);
    }
};
std::vector<Background> backgrounds(const Fbm &source) {
    if (source.version()<20) return {};
    const auto data=source.blockBytes("background_images");Reader r{data};const int count=r.integer();
    if (count<0 || count>3) bad("Too many background images");
    std::vector<Background> result;QSet<int> seen;
    for (int i=0;i<count;++i) {
        const auto start=r.at;Background b;b.view=r.integer();b.visible=r.flag();b.quality=r.integer();
        b.x=r.integer();b.y=r.integer();b.scale=r.real();b.alpha=r.integer();b.transparent=r.flag();
        b.color=r.integer();b.tolerance=r.integer();b.width=r.integer();b.height=r.integer();
        const int n=r.integer();const auto at=r.at;r.skip(n);b.jpeg=data.mid(at,n);b.raw=data.mid(start,r.at-start);
        b.check();if (seen.contains(b.view)) bad("Duplicate background view");seen.insert(b.view);result.push_back(std::move(b));
    }
    if (r.at!=data.size()) bad("Unexpected background block suffix");
    return result;
}
struct Plan {
    std::vector<Background> items;int minimum;bool changed=false;
    Plan(const Fbm &source,const QJsonObject &request):items(backgrounds(source)),minimum(source.version()) {
        if (!nativeBackgroundCommand(request)) bad("Unsupported native background operation");
        const auto operation=text(request["operation"]);const int view=integer(request["view"],0,2);
        auto found=std::find_if(items.begin(),items.end(),[&](const auto &b){return b.view==view;});
        if (operation=="remove") {
            if (found==items.end()) return;
            items.erase(found);changed=true;
        } else if (operation=="import") {
            Background added;added.jpeg=imageFile(text(request["path"]));const auto size=imageSize(added.jpeg,true);
            added.view=view;added.width=size.width();added.height=size.height();added.y=size.height();added.scale=scale(request["scale"]);
            if (found!=items.end() && found->jpeg==added.jpeg && found->x==0 && found->y==added.y &&
                found->scale==added.scale && found->visible && found->alpha==255 && !found->transparent &&
                found->color==0 && found->tolerance==3 && found->quality==100) return;
            if (found!=items.end()) items.erase(found);
            added.raw=added.bytes();items.push_back(std::move(added));changed=true;minimum=std::max(minimum,20);
        } else if (operation=="update") {
            if (found==items.end()) bad("No background image in this view");
            if (!request["changes"].isObject()) bad("Invalid background changes");
            auto edited=*found;const auto changes=request["changes"].toObject();
            for (auto it=changes.begin();it!=changes.end();++it) {
                const auto name=it.key();const auto value=it.value();
                if (name=="origin") {
                    if (!value.isArray() || value.toArray().size()!=2) bad("Background origin requires two integer pixels");
                    const auto pair=value.toArray();edited.x=integer(pair[0],-1000000000,1000000000);edited.y=integer(pair[1],-1000000000,1000000000);
                } else if (name=="scale") edited.scale=scale(value);
                else if (name=="alpha") edited.alpha=integer(value,0,255);
                else if (name=="tolerance") edited.tolerance=integer(value,0,255);
                else if (name=="transparent_color") edited.color=integer(value,0,0xffffff);
                else if (name=="visible") edited.visible=flag(value);
                else if (name=="transparent") edited.transparent=flag(value);
                else bad("Unknown background property");
            }
            // Compare Single values, rather than input Double spelling. No-op
            // retains raw fields and position in the background list exactly.
            changed=edited.x!=found->x || edited.y!=found->y || edited.scale!=found->scale || edited.alpha!=found->alpha ||
                edited.tolerance!=found->tolerance || edited.color!=found->color || edited.visible!=found->visible || edited.transparent!=found->transparent;
            if (changed) { edited.raw=edited.bytes();*found=std::move(edited); }
        } else bad("Unknown background operation");
        if (changed && source.inspect()["opaque_tail_bytes"].toInteger()) bad("Cannot edit unknown FBM extensions");
    }
    QByteArray bytes() const {
        QByteArray result;putInt(result,int(items.size()));
        for (const auto &b:items) result.append(b.raw);
        return result;
    }
};
} // namespace

bool nativeBackgroundCommand(const QJsonObject &request) { return request["method"]=="background"; }
int backgroundMinimumVersion(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    Q_UNUSED(bindings);return Plan(source,request).minimum;
}
TopologyEdit editBackground(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    const Plan plan(source,request);
    if (!plan.changed) return {source,bindings,false};
    if (plan.minimum>source.version()) bad("Background import requires format promotion");
    return {source.withBlocks({{"background_images",plan.bytes()}}),bindings,true};
}
} // namespace fs::document
