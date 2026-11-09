// GPL-2.0-or-later; see COPYING. Semantics and binary layout are ported from
// bridge/ModelEdits.pas, Units/FreeProjectSettings.inc, FreeEdit.Model_Scale,
// FreeGeometry.pas, and the isolated bridge/build_bridge.py unit-conversion
// corrections. The baseline Pascal sources are not modified or called here.
#include "project_edit.h"
#include "fbm_text.h"
#include "preview_image.h"
#include "geometry_input.h"
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) { throw std::invalid_argument(message); }
int integer(const QJsonValue &value, int low, int high) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || n<low || n>high || std::floor(n)!=n)
        bad("Invalid project integer");
    return int(n);
}
float number(const QJsonValue &value, bool nonnegative=true) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || std::abs(n)>1e9 || (nonnegative && n<0))
        bad("Invalid bounded project number");
    return float(n);
}
QByteArray i32(int value) {
    QByteArray bytes(4, '\0');qToLittleEndian<qint32>(value,bytes.data());return bytes;
}
QByteArray f32(float value) {
    if (!std::isfinite(value)) bad("Converted project value is not finite");
    quint32 bits;std::memcpy(&bits,&value,4);QByteArray bytes(4,'\0');qToLittleEndian(bits,bytes.data());return bytes;
}
struct Reader {
    QByteArray bytes;qsizetype at=0;
    void skip(qsizetype n) { if (n<0 || n>bytes.size()-at) bad("Truncated project block");at+=n; }
    int integer() { const auto p=at;skip(4);return qFromLittleEndian<qint32>(bytes.constData()+p); }
    float real() { const auto bits=quint32(integer());float v;std::memcpy(&v,&bits,4);return v; }
    bool flag() { const auto p=at;skip(1);return bytes[p]!=0; }
    QByteArray string() { const int n=integer();const auto p=at;skip(n);return bytes.mid(p,n); }
    void jpeg() { skip(8);const int n=integer();skip(n); }
};
struct Patch { qsizetype size;QByteArray bytes; };
using Patches=std::map<qsizetype,Patch>;
QByteArray replace(const QByteArray &source,const Patches &patches) {
    QByteArray result;result.reserve(source.size());qsizetype at=0;
    const auto append=[&](const QByteArray &v) {
        if (v.size()>Fbm::MaximumBytes-result.size()) bad("Project block exceeds document limit");
        result.append(v);
    };
    for (const auto &[offset,patch]:patches) {
        if (offset<at || offset>source.size() || patch.size>source.size()-offset) bad("Invalid project replacement span");
        append(source.mid(at,offset-at));append(patch.bytes);at=offset+patch.size;
    }
    append(source.mid(at));return result;
}
struct Real { float value=0;qsizetype at=-1; };
struct Flag { bool value=false;qsizetype at=-1; };
struct Int { int value=0;qsizetype at=-1; };
struct Text { QByteArray bytes;qsizetype at=-1; };
// FreeVisibility.SaveBinary: five feature flags after the master switch.
constexpr std::array<const char*,5> HydroFields{"hydro_displacement","hydro_lateral_area",
    "hydro_sectional_areas","hydro_metacentric_height","hydro_lcf"};
struct Settings {
    QByteArray raw;
    std::map<QString,Real> reals{{"length",{1}},{"beam",{1}},{"draft",{1}},
        {"water_density",{1.025f}},{"appendage_coefficient",{1}},{"split_section_location",{0}}};
    std::map<QString,Flag> flags{{"particulars",{false}},{"shade_underwater",{true}},
        {"use_default_split_section_location",{true}},{"simplify_intersections",{true}},
        {"generate_auxiliary_reports",{false}}};
    Int units,color,coefficients{1},alpha{255};std::map<QString,Text> texts{{"name",{}},{"designer",{}},{"comment",{}},{"created_by",{}}};
    std::vector<qsizetype> lengths,masses;
    Patches extra;
    Settings(const Fbm &source,const QJsonObject &bindings) : raw(source.blockBytes("project_settings")) {
        Reader r{raw};const auto version=source.version();
        alpha.value=integer(bindings.contains("default_underwater_alpha")?bindings["default_underwater_alpha"]:bindings.contains("initial_underwater_alpha")?bindings["initial_underwater_alpha"]:QJsonValue(255),0,255);
        const auto text=[&](const char *key) { texts[key]={ {},r.at };texts[key].bytes=r.string(); };
        const auto real=[&](const char *key) { reals[key]={0,r.at};reals[key].value=r.real(); };
        const auto flag=[&](const char *key) { flags[key]={false,r.at};flags[key].value=r.flag(); };
        if (version>=2) {
            text("name");text("designer");real("length");real("beam");real("draft");flag("particulars");
            real("water_density");real("appendage_coefficient");flag("shade_underwater");
            color.at=r.at;color.value=r.integer();units.at=r.at;units.value=r.integer();
            if (version>=6) { flag("use_default_split_section_location");real("split_section_location"); }
            if (version>=7) r.skip(1);
            text("comment");text("created_by");
        } else {
            // Pre-1.2 files rely on the same preference as ProjectSettings.Clear.
            if (bindings.contains("default_underwater_color"))
                color.value=integer(bindings["default_underwater_color"],std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
        }
        if (version>=16) { coefficients.at=r.at;coefficients.value=r.integer();if (r.flag()) r.jpeg(); }
        if (version>=18) flag("simplify_intersections");
        if (version>=20) {
            const auto collect=[&](std::vector<qsizetype> &offsets,int count) {
                for (int i=0;i<count;++i) { offsets.push_back(r.at);r.skip(4); }
            };
            collect(lengths,4);int n=r.integer();collect(masses,n);collect(masses,3);r.skip(1);
            n=r.integer();r.skip(qsizetype(n)*4);n=r.integer();collect(lengths,n);r.skip(1);collect(lengths,1);
        }
        if (version>=37) r.skip(1);
        if (version>=39) flag("generate_auxiliary_reports");
        if (version>=44) { alpha.at=r.at;alpha.value=r.integer(); }
        if (r.at!=raw.size()) bad("Unexpected project settings layout");
    }
    float value(const char *key) const { return reals.at(key).value; }
    bool flag(const char *key) const { return flags.at(key).value; }
    void scalar(const char *key,float v,bool dimension=false,double threshold=1e-7) {
        auto &old=reals.at(key).value;
        if (std::abs(v-old)>threshold) { old=v;if (dimension) flags.at("particulars").value=true; }
    }
    QByteArray bytes() const {
        auto patches=extra;
        for (const auto &[key,v]:reals) { Q_UNUSED(key);if (v.at>=0) patches[v.at]={4,f32(v.value)}; }
        for (const auto &[key,v]:flags) { Q_UNUSED(key);if (v.at>=0) patches[v.at]={1,QByteArray(1,v.value?1:0)}; }
        for (auto v:{units,color,coefficients,alpha}) if (v.at>=0) patches[v.at]={4,i32(v.value)};
        return replace(raw,patches);
    }
};
// Constants are evaluated as Double by FSetProjectUnits, while Model_Scale
// receives a Single scale vector. Keeping these domains separate is essential.
constexpr double Foot=0.3048, Density=1000.0*Foot*Foot*Foot/0.45359237,
                 Mass=1000.0/(2240.0*0.45359237);
struct Conversion {
    double length,density,mass,thickness;float scale;
    explicit Conversion(int units):length(units?1/Foot:Foot),density(units?Density:1/Density),
        mass(units?Mass:1/Mass),thickness(units?1/25.4:25.4),scale(float(length)) {}
};
int resolvedColor(int color,const QJsonObject &bindings) {
    const auto colors=bindings["resolved_colors"].toObject();const auto key=QString::number(color);
    if (colors.contains(key)) return integer(colors[key],0,0xffffff);
    // Literal RGB is its own resolution. Other TColor forms require the
    // adapter's observed ColorToRGB, never a guessed system palette.
    if (color>=0 && color<=0xffffff) return color;
    bad("Missing resolved project system color");
}
struct Plan {
    Settings before,after;int precision,minimum;bool changed=false,unitsChanged=false;
    bool precisionChanged=false,draftChanged=false,simplifyChanged=false;
    bool previewEnabled=false,previewChanged=false,alignToBaseline=false;
    QByteArray visibility;std::map<QString,bool> hydro;
    bool visibilityChanged=false;
    Plan(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request)
        : before(source,bindings),after(before),precision(source.precision()),minimum(source.version()) {
        if (!nativeProjectCommand(request) || !request["changes"].isObject()) bad("Invalid native project request");
        const auto changes=request["changes"].toObject();const auto encoding=bindings["fbm_encoding"].toString();
        previewEnabled=source.inspect()["preview"].toObject()["enabled"].toBool();
        visibility=source.blockBytes("visibility");
        for(size_t i=0;i<HydroFields.size();++i)hydro[HydroFields[i]]=source.version()<17||visibility.at(24+i)!=0;
        std::map<QString,QByteArray> encoded;
        // Validate every field, including encoding, before constructing any edit.
        for (auto it=changes.begin();it!=changes.end();++it) {
            const auto key=it.key();const auto value=it.value();
            if (before.texts.count(key)) {
                if (!value.isString()) bad("Invalid project text");
                encoded[key]=encodeFbmText(value.toString(),encoding);
            } else if (key=="units") {
                if (!value.isString() || (value!="metric" && value!="imperial")) bad("Invalid project units");
            } else if (before.reals.count(key)) number(value,key!="split_section_location");
            else if (key=="precision") integer(value,0,3);
            else if (key=="coefficient_basis") integer(value,0,1);
            else if (key=="underwater_color") integer(value,0,0xffffff);
            else if (key=="underwater_color_alpha") integer(value,0,255);
            else if (key=="use_default_split_section_location" || key=="simplify_intersections" || key=="shade_underwater" || key=="save_preview" || key=="generate_auxiliary_reports" || key=="align_to_baseline" || hydro.count(key)) {
                if (!value.isBool()) bad("Invalid project Boolean");
            } else bad("Unsupported project field");
        }
        if (changes.contains("units")) after.units.value=changes["units"]=="imperial"?1:0;
        unitsChanged=after.units.value!=before.units.value;changed=unitsChanged;
        alignToBaseline=changes["align_to_baseline"].toBool();
        if(alignToBaseline && native_geometry::hydrostaticKeel(documentGeometry(source))!=0) {
            // Reject the entire transaction, including other settings. Never
            // reproduce Face_MoveZAuto's partial translation of unlocked points.
            for(const auto &p:source.controlPoints())if(p.locked||p.constrained||p.hardAnchor)
                bad("船模含锁定点或约束点，请先解除限制再将船底移至基线。");
            changed=true;
        }
        if(changes.contains("underwater_color_alpha")) {
            after.alpha.value=changes["underwater_color_alpha"].toInt();
            if(after.alpha.value!=before.alpha.value){changed=true;minimum=std::max(minimum,44);}
        }
        if(changes.contains("coefficient_basis")) {
            after.coefficients.value=changes["coefficient_basis"].toInt();
            changed|=after.coefficients.value!=before.coefficients.value;
        }
        for(size_t i=0;i<HydroFields.size();++i){
            const auto key=HydroFields[i];
            if(!changes.contains(key)||changes[key].toBool()==hydro.at(key))continue;
            visibilityChanged=true;changed=true;minimum=std::max(minimum,17);
            if(source.version()>=17)visibility[24+i]=char(changes[key].toBool());
        }
        if(changes.contains("save_preview")) {
            previewChanged=previewEnabled!=changes["save_preview"].toBool();
            previewEnabled=changes["save_preview"].toBool();changed=changed||previewChanged;
        }
        if (unitsChanged) {
            const Conversion c(after.units.value);
            after.reals.at("water_density").value=float(double(before.value("water_density"))*c.density);
            for (auto key:{"length","beam","draft"}) after.scalar(key,std::abs(after.value(key)*c.scale),true);
            after.reals.at("split_section_location").value=before.flag("use_default_split_section_location")?
                float(double(before.value("split_section_location"))*c.length):before.value("split_section_location")*c.scale;
            const auto convert=[&](const std::vector<qsizetype> &offsets,double factor) {
                for (auto at:offsets) { Reader r{before.raw,at};after.extra[at]={4,f32(float(double(r.real())*factor))}; }
            };
            convert(before.lengths,c.length);convert(before.masses,c.mass);
        }
        for (auto &[key,bytes]:encoded) {
            const auto &original=before.texts.at(key);
            if (decodeFbmText(original.bytes,encoding)==changes[key].toString()) continue;
            changed=true;
            if (original.at>=0) after.extra[original.at]={4+original.bytes.size(),i32(int(bytes.size()))+bytes};
        }
        for (auto key:{"length","beam","draft","water_density","appendage_coefficient"})
            if (changes.contains(key)) after.scalar(key,number(changes[key]),QString(key)=="length" || QString(key)=="beam" || QString(key)=="draft",
                                                   QString(key)=="water_density"?1e-6:1e-7);
        if (changes.contains("split_section_location")) {
            const float next=number(changes["split_section_location"],false);
            if (next!=after.value("split_section_location")) after.reals.at("split_section_location").value=next;
        }
        for (auto key:{"use_default_split_section_location","simplify_intersections","shade_underwater","generate_auxiliary_reports"})
            if (changes.contains(key)) after.flags.at(key).value=changes[key].toBool();
        if (changes.contains("precision")) precision=integer(changes["precision"],0,3);
        if (changes.contains("underwater_color")) {
            const int next=integer(changes["underwater_color"],0,0xffffff);
            if (next!=resolvedColor(before.color.value,bindings)) { after.color.value=next;changed=true; }
        }
        for (const auto &[key,v]:after.reals) {
            if (!std::isfinite(v.value)) bad("Converted project scalar is not finite");
            changed=changed || v.value!=before.reals.at(key).value;
        }
        for (auto key:{"use_default_split_section_location","simplify_intersections","shade_underwater","generate_auxiliary_reports"})
            changed=changed || after.flag(key)!=before.flag(key);
        precisionChanged=precision!=source.precision();changed=changed || precisionChanged;
        draftChanged=after.value("draft")!=before.value("draft");
        simplifyChanged=after.flag("simplify_intersections")!=before.flag("simplify_intersections");
        if (changed) {
            minimum=std::max(minimum,2);
            if(previewEnabled)minimum=std::max(minimum,16);
            if(after.coefficients.value!=before.coefficients.value)minimum=std::max(minimum,16);
            if (!after.flag("use_default_split_section_location") || after.value("split_section_location")!=before.value("split_section_location")) minimum=std::max(minimum,6);
            if (!after.flag("simplify_intersections")) minimum=std::max(minimum,18);
            if (after.flag("generate_auxiliary_reports")!=before.flag("generate_auxiliary_reports")) minimum=std::max(minimum,39);
        }
    }
};
void scaleCalculations(FbmTransformFields &fields,const Conversion &c) {
    const float s=c.scale;
    for (auto &[name,p]:fields.calculations) {
        if (name=="kaper" || name=="planing") {
            // These two records store mass. Model_Scale scales volume first;
            // FSetProjectUnits then replaces it from the original Double mass.
            const double mass=p.at(4);
            p.at(0)=p[0]*s;p.at(1)=p[1]*s;p.at(2)=p[2]*s;p.at(4)=float(mass*c.mass);p.at(6)=p[6]*s*s;
        } else {
            p.at(3)=p[3]*s;p.at(5)=p[5]*s*s*s;p.at(6)=p[6]*s;p.at(7)=p[7]*s;
            p.at(8)=p[8]*s;p.at(9)=p[9]*s*s;p.at(11)=p[11]*s;p.at(12)=p[12]*s;
            p.at(13)=p[13]*s*s;p.at(15)=p[15]*s*s;
            p.at(16)=(name=="hollenbach" || name=="oortmerssen" || name=="fung")?p[16]*s:p[16]*s*s;
        }
    }
}
QByteArray scaleLayers(const Fbm &source,const Conversion &c) {
    Reader r{source.blockBytes("layers")};const int count=r.integer();Patches patches;
    for (int i=0;i<count;++i) {
        r.string();r.skip(8+3);if (source.version()>=9) r.skip(2);
        if (source.version()>=11) {
            for (double factor:{c.density,c.thickness}) { const auto at=r.at;patches[at]={4,f32(float(double(r.real())*factor))}; }
        }
        if (source.version()>=15) r.skip(1);
        if (source.version()>=21) r.skip(4);
    }
    return replace(r.bytes,patches);
}
QByteArray scaleBackgrounds(const Fbm &source,const Conversion &c) {
    Reader r{source.blockBytes("background_images")};const int count=r.integer();Patches patches;
    for (int i=0;i<count;++i) {
        r.skip(4+1+4+4+4);const auto at=r.at;patches[at]={4,f32(float(double(r.real())*c.length))};
        r.skip(4+1+4+4);r.jpeg();
    }
    return replace(r.bytes,patches);
}
QByteArray flowlines(const Fbm &source,const Conversion *conversion) {
    Reader r{source.blockBytes("flowlines")};const int count=r.integer();QByteArray result=i32(count);
    for (int i=0;i<count;++i) {
        const auto at=r.at;
        if (conversion) { result+=f32(float(double(r.real())*conversion->length));result+=f32(float(double(r.real())*conversion->length)); }
        else { r.skip(8);result+=r.bytes.mid(at,8); }
        result+=i32(r.integer());r.skip(1);result+=QByteArray(1,'\0');result+=QByteArray(1,r.flag()?1:0);
        const int points=r.integer();r.skip(qsizetype(points)*13);result+=i32(0);
    }
    return result;
}
QByteArray clearSections(const Fbm &source,const char *name) {
    Reader r{source.blockBytes(name)};const int count=r.integer();QByteArray result=i32(count);
    for (int i=0;i<count;++i) {
        const auto at=r.at;const int kind=r.integer();if (source.version()>=11) r.skip(1);r.skip(16);
        result+=r.bytes.mid(at,r.at-at);r.skip(1);result+=QByteArray(1,'\0');const int curves=r.integer();result+=i32(0);
        for (int j=0;j<curves;++j) { const int n=r.integer();r.skip(qsizetype(n)*(source.version()>=6 && kind>=1 && kind<=3?9:13)); }
    }
    return result;
}
Fbm alignToBaseline(const Fbm &source) {
    const float dz=-native_geometry::hydrostaticKeel(documentGeometry(source));
    if(dz==0)return source;
    auto fields=source.transformFields();std::vector<FbmPosition> points;
    for(const auto &p:source.controlPoints())points.push_back({p.x,p.y,p.z+dz});
    for(auto &marker:fields.markers)for(auto &p:marker)p.z+=dz;
    for(auto &plane:fields.sections)plane[3]-=plane[2]*dz;
    auto candidate=source.withTransformFields(points,fields);
    if(source.version()>=20) {
        // withTransformFields clears derived traces; move the definition too.
        Reader r{candidate.blockBytes("flowlines")};const int count=r.integer();Patches patches;
        for(int i=0;i<count;++i) {
            r.real();const auto at=r.at;const float y=r.real();const int view=r.integer();
            if(view!=2)patches[at]={4,f32(y+dz)}; // body/profile use Z; plan uses Y.
            r.skip(2);const int n=r.integer();r.skip(qsizetype(n)*13);
        }
        candidate=candidate.withBlocks({{"flowlines",replace(r.bytes,patches)}});
    }
    return candidate;
}
} // namespace

bool nativeProjectCommand(const QJsonObject &request) { return request["method"]=="update_project"; }
int projectMinimumVersion(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    return Plan(source,bindings,request).minimum;
}
TopologyEdit editProject(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    const Plan plan(source,bindings,request);
    if (!plan.changed) return {source,bindings,false};
    if (plan.minimum>source.version()) bad("Project edit requires format promotion");
    Fbm candidate=source;
    std::map<QString,QByteArray> replacements;
    if (plan.unitsChanged) {
        const Conversion c(plan.after.units.value);auto fields=source.transformFields();std::vector<FbmPosition> points;
        points.reserve(source.controlPoints().size());
        // Coordinate property writes bypass anchor/line propagation. Locked
        // points are temporarily unlocked by the old Model_Scale implementation.
        for (const auto &p:source.controlPoints()) points.push_back({p.x*c.scale,p.y*c.scale,p.z*c.scale});
        fields.dimensions={plan.after.value("length"),plan.after.value("beam"),plan.after.value("draft")};
        fields.particularsSet=plan.after.flag("particulars");fields.split=plan.after.value("split_section_location");
        for (auto &plane:fields.sections) plane[3]=float(double(plane[3])*c.length);
        for (auto &marker:fields.markers) for (auto &p:marker) p={p.x*c.scale,p.y*c.scale,p.z*c.scale};
        scaleCalculations(fields,c);candidate=source.withTransformFields(points,fields);
        replacements["layers"]=scaleLayers(candidate,c);
        if (source.version()>=20) {
            replacements["background_images"]=scaleBackgrounds(candidate,c);
            replacements["flowlines"]=flowlines(candidate,&c);
        }
    } else if ((plan.draftChanged || plan.precisionChanged) && source.version()>=20) {
        replacements["flowlines"]=flowlines(candidate,nullptr);
    }
    if (plan.unitsChanged || plan.precisionChanged || plan.simplifyChanged) {
        for (auto name:{"stations","buttocks","waterlines","diagonals"}) {
            if (QString(name)=="diagonals" && source.version()<9) continue;
            replacements[name]=clearSections(candidate,name);
        }
    }
    replacements["project_settings"]=plan.after.bytes();
    if(plan.visibilityChanged)replacements["visibility"]=plan.visibility;
    if (plan.precisionChanged) { auto header=source.blockBytes("header");header.replace(header.size()-4,4,i32(plan.precision));replacements["header"]=header; }
    candidate=candidate.withBlocks(replacements);
    if(plan.previewChanged)candidate=withPreviewEnabled(candidate,plan.previewEnabled);
    if(plan.alignToBaseline)candidate=alignToBaseline(candidate);
    return {std::move(candidate),bindings,true};
}
} // namespace fs::document
