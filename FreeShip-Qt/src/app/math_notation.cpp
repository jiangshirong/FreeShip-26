// SPDX-License-Identifier: GPL-2.0-or-later
#include "math_notation.h"
#include "jkqtmathtext/jkqtmathtext.h"
#include <QCache>
#include <QBuffer>
#include <QAbstractTextDocumentLayout>
#include <QImage>
#include <QFontMetricsF>
#include <QRegularExpression>
#include <QStaticText>
#include <QTextDocument>
#include <QtMath>
#include <algorithm>
#include <utility>

namespace notation { namespace {
struct Symbol {QString alias,base,sub,tex,ascii;};
struct Unit {QString plain,tex;};
Unit unitDescription(QString source){
    source=source.trimmed();
    if(source.startsWith('[')&&source.endsWith(']'))source=source.mid(1,source.size()-2).trimmed();
    static const QHash<QString,QString> names{{"米","m"},{"吨","t"},{"秒","s"},{"度","°"},{"节","kn"},
        {"deg","°"},{"degr","°"},{"degree","°"},{"degrees","°"},{"град","°"},{"град.","°"},
        {"метр","m"},{"тонн","t"},{"сек","s"},{"mkm","µm"},{"kts","kn"},{"knot","kn"},{"knots","kn"},
        {"feet","ft"},{"foot","ft"},{"inch","in"},{"inches","in"},{"long ton","long ton"},{"long tons","long ton"},{"sec","s"},
        {"BHP","bhp"},{"公里/小时","km/h"},{"&percent;","%"}};
    source=names.value(source,source);
    if(source=="long ton")return {"long ton","\\mathrm{long\\ ton}"};
    if(source=="n mile")return {source,"\\mathrm{n\\,mile}"};
    if(source=="r/min")return {source,"\\frac{\\mathrm{r}}{\\mathrm{min}}"};
    if(source=="1/s")return {source,"\\frac{1}{\\mathrm{s}}"};
    if(source=="1/min")return {source,"\\frac{1}{\\mathrm{min}}"};
    static const QRegularExpression scaled(R"(^10(?:\^\{?(-?\d+)\}?|([⁰¹²³⁴⁵⁶⁷⁸⁹⁻]+))\s*(.+)$)");
    const auto scale=scaled.match(source);
    if(scale.hasMatch()){
        QString exponent=scale.captured(1).isEmpty()?scale.captured(2):scale.captured(1);
        const QString super="⁰¹²³⁴⁵⁶⁷⁸⁹";for(int i=0;i<super.size();++i)exponent.replace(super[i],QChar('0'+i));exponent.replace(QChar(0x207b),'-');
        const auto value=unitDescription(scale.captured(3));if(!value.tex.isEmpty())return {"10^"+exponent+' '+value.plain,"10^{"+exponent+"}\\,"+value.tex};
    }
    source.replace(QRegularExpression(R"(\bsec\b)"),"s");
    source.replace("**","^");
    source.replace(QChar(0x00b7),'*');
    source.replace(QRegularExpression(R"(\s*([/*])\s*)"),"\\1");
    source.replace(QRegularExpression(R"(\s+)"),"*");
    source.replace(QChar(0x00b2),"^2").replace(QChar(0x00b3),"^3").replace(QChar(0x2074),"^4");
    source.replace("m2","m^2").replace("m3","m^3").replace("m4","m^4");
    source.replace("^−","^-");
    const auto atom=[](const QString& text)->Unit{
        static const QRegularExpression re(R"(^(m|mm|cm|km|µm|μm|ft|in|yd|mi|s|min|h|t|kg|g|lb|lbf|ton|N|kN|MN|Pa|kPa|MPa|W|kW|MW|hp|bhp|J|kJ|Hz|kn|rpm|rad|°|°C|°F|K|%)(?:\^\{?(-?\d+)\}?)?$)");
        const auto match=re.match(text);if(!match.hasMatch())return {};
        QString plain=match.captured(1),tex=plain=="°"?"\\degree":plain.startsWith("°")?"\\degree\\mathrm{"+plain.mid(1)+"}":plain=="%"?"\\%":"\\mathrm{"+plain+"}";
        if(!match.captured(2).isEmpty()){
            const auto exponent=match.captured(2);tex+="^{"+exponent+"}";
            if(exponent=="2")plain+=QChar(0x00b2);else if(exponent=="3")plain+=QChar(0x00b3);else if(exponent=="4")plain+=QChar(0x2074);else plain+='^'+exponent;
        }
        return {plain,tex};
    };
    const auto product=[&](const QString& text)->Unit{
        QStringList plain,tex;for(const auto& part:text.split('*')){const auto a=atom(part);if(a.tex.isEmpty())return {};plain.append(a.plain);tex.append(a.tex);}
        return {plain.join(QChar(0x00b7)),tex.join("\\,")};
    };
    const auto parts=source.split('/');if(parts.size()>2||parts.isEmpty())return {};
    const auto numerator=product(parts[0]);if(numerator.tex.isEmpty())return {};
    if(parts.size()==1)return numerator;
    const auto denominator=product(parts[1]);if(denominator.tex.isEmpty())return {};
    return {numerator.plain+'/'+denominator.plain,"\\frac{"+numerator.tex+"}{"+denominator.tex+"}"};
}
const QList<Symbol>& registry(){
    static const auto list=[] {
        QList<Symbol> result;
        const auto add=[&](QString alias,QString base,QString sub){
            const auto texBase=base==QString::fromUtf8("η")?QString("\\eta"):(base.size()>1?"\\mathrm{"+base+"}":base);
            result.append({alias,base,sub,texBase+"_{\\mathrm{"+sub+"}}",base+"_"+(sub.size()==1?sub:"{"+sub+"}")});
        };
        for(const auto& alias:QStringList{"Cb","Cp","Cvp","Cm","Cwp","Cw","Cf","Cv","Cr","Ct","Cx","Cy","Cmz","Cd","Ca","Cn","Cl","Cstrn",
            "Fx","Fy","Mz","Fr","Lwl","Bwl","Twl","Lpp","Los","LWL","BWL","TWL","LPP","LOS","LOA","Lmax","Bmax","Tmax","Tmin",
            "Cbt","Capp","Caero","AT","Xg",
            "Am","Aw","Ae","Ao","Ap","Abt","At","Av","Ax","Ay","As","AE","A0","AW","TF","TA","Dp","Np",
            "Rt","Rr","Rf","Rw","Rapp","Rb","Rtr","Ra","RT","RR","RF","RW","Vp","Vs","Va","Vw","Vr","Vk","Vms",
            "Tf","Ta","Tc","Tb","Te","Wt","Jp","Jb","Jv","Jopt","Zg","Za","Zp","Xc","Lc","Bl","Gp","Gt","Mp","Mt","Pc","Mc","Wdz","Kdt","Knt","Ktb","Kqb","Nser","Npr","Nsh","Nf","Na","Nh","Rv","Pe","Pd","Ps","Pb","PE","PD","PS","PB","Kt","Kq","KT","KQ","Fn","Rn","Ix","Iy","Iz","Ixx","Iyy","Izz","Ixy"})
            add(alias,alias.left(1),alias.mid(1));
        for(const auto& alias:QStringList{"KMt","KMl","BMt","BMl","GMt","GMl","GZd"})add(alias,alias.left(2),alias.mid(2));
        for(const auto& alias:QStringList{"η0","ηH","ηR","ηD"})add(alias,alias.left(1),alias.mid(1));
        add("eta0",QString::fromUtf8("η"),"0");
        for(const auto& name:QStringList{"Eta0","EtaH","EtaR","EtaW"})add(name,QString::fromUtf8("η"),name.right(1));
        for(int i=1;i<=5;++i)add("Cr"+QString::number(i),"C","r,"+QString::number(i));
        for(int i=1;i<=11;++i)add("A"+QString::number(i),"A",QString::number(i));
        for(int i=1;i<=7;++i)add("K"+QString::number(i),"K",QString::number(i));
        // Explicit multi-index forms already used by the original reports.
        for(const auto& v:QList<QStringList>{{"Dp_lim","D","p,lim"},{"Ps_n","P","s,n"},{"Ps_r","P","s,r"},
            {"Jp_opt","J","p,opt"},{"Jp_T0","J","p,T0"},{"Jp_Q0","J","p,Q0"},
            {"R_T_e","R","T,e"},{"Rt_e","R","t,e"},{"Rt_min","R","t,min"},{"Pe_e","P","e,e"},
            {"Power_e","Power","e"},{"Fr_V","F","r,V"},{"FrV","F","r,V"},{"FrB","F","r,B"},
            {"m_z","m","z"},{"R_p","R","p"},{"R_e","R","e"},{"R_ap","R","ap"},
            {"Cp_opt","C","p,opt"},{"Lcb_opt","LCB","opt"},{"h_b","h","b"},{"a_f","a","f"},
            {"GZ_lform","GZ","lform"},{"GZ_psi","GZ","psi"},{"GZ_tform","GZ","tform"},{"GZ_teta","GZ","teta"},{"Z_bulb","Z","bulb"}})
            add(v[0],v[1],v[2]);
        for(int i=1;i<=6;++i)for(int j=1;j<=6;++j)add(QString("k%1%2").arg(i).arg(j),"k",QString::number(i)+QString::number(j));
        // The legacy AddMass report writes these diagonal terms as al11 ... al66.
        for(int i=1;i<=6;++i){
            const QString index(2,QChar('0'+i));
            add("al"+index,"al",index);
        }
        for(int i=1;i<=11;++i)add("a"+QString::number(i),"a",QString::number(i));
        for(const auto& ratio:QStringList{"H/T","T/H","L/B","T/B","B/T","L/T","Lwl/B","Lwl/Bwl","LWL/BWL","Bwl/T",
            "Lwl/T","Lwl/L","Bwl/Tc","BWL/Tc","Bwl/TC","BWL/TC","Tc/T","D/B","D/B^3","V/L^3",
            "Lwl/V^(1/3)","Aw/V^0.667","L/V^0.333","L/V^0.3333","L/V^0,333","L/V**0,3333","Ap/V^0.6667",
            "V/Lwl^3","Los/Lwl","Dp/Ta","S/L","LCB/Lpp","Xc/Lpp","AT/AM","BT/B",
            "Lpp/B","LPP/B","C/L","S/Lwl","A/Lwl","Ae/Ao","Ae/A0","P/D","P/Dp","Q0/Q"}){
            // Split once: the Delft volume exponent itself contains a slash.
            const auto slash=ratio.indexOf('/');
            const auto symbol=[&](QString part){
                if(part=="V^(1/3)")return QString("V^{\\frac{1}{3}}");
                // Delft's displayed 0.667 is rounded; its calculation uses 2/3.
                if(ratio=="Aw/V^0.667"&&part=="V^0.667")return QString("V^{\\frac{2}{3}}");
                if(part=="Q0")return QString("Q_0");
                if(part=="AM"||part=="BT")return part.left(1)+"_{"+part.mid(1)+"}";
                part.replace("**","^");
                const auto base=part.section('^',0,0),exponent=part.section('^',1).replace(',','.');
                const auto suffix=exponent.isEmpty()?QString{}:"^{"+exponent+"}";
                for(const auto& item:std::as_const(result))if(item.alias==base)return item.tex+suffix;
                return base+suffix;
            };
            result.append({ratio,{},{},QString("\\frac{%1}{%2}").arg(symbol(ratio.left(slash)),symbol(ratio.mid(slash+1))),ratio});
        }
        for(const auto& alias:QStringList{"Ca*10^3","Capp*10^3","Caero*10^3"}){
            const auto coefficient=alias.section('*',0,0);
            result.append({alias,{},{},"C_{\\mathrm{"+coefficient.mid(1)+"}}\\times10^{3}",alias});
        }
        result.append({"EtaH*EtaR",{},{},"\\eta_{\\mathrm H}\\,\\eta_{\\mathrm R}","EtaH*EtaR"});
        result.append({"v/(g*V^0,3333)^0,5",{},{},"\\frac{v}{\\sqrt{g V^{0.3333}}}","v/(g*V^0,3333)^0,5"});
        for(const auto& alias:QStringList{"SVLR = S/(VL)^0,5","SVLR = S/(VL)^0.5"})
            result.append({alias,{},{},"\\mathrm{SVLR}=\\frac{S}{\\sqrt{VL}}",alias});
        return result;
    }();return list;
}
const QHash<QString,int>& indices(){static const auto result=[] {
    QHash<QString,int> map;const auto& rows=registry();for(int i=0;i<rows.size();++i){
        const auto& s=rows[i];map[s.alias]=i;map[s.ascii]=i;
        if(!s.base.isEmpty()){map[s.base+"_{"+s.sub+"}"]=i;map[s.base+"_"+s.sub]=i;}
    }return map;
}();return result;}
const QRegularExpression& tokens(){static const auto re=[] {
    QStringList words;for(auto it=indices().cbegin();it!=indices().cend();++it)words.append(QRegularExpression::escape(it.key()));
    std::sort(words.begin(),words.end(),[](const auto& a,const auto& b){return a.size()>b.size();});
    return QRegularExpression("(?<![A-Za-z0-9_])(?:"+words.join('|')+")(?![A-Za-z0-9_])");
}();return re;}
bool literal(const QString& text){
    // Never typeset a path/URL, filename, or an explicitly delimited identifier.
    static const QRegularExpression path(R"((?:[A-Za-z]:[\\/]|://|\\|\.(?:fbm|ftm|ini|json|csv|png|dxf|obj|stl|txt)\b))",QRegularExpression::CaseInsensitiveOption);
    return path.match(text).hasMatch();
}
quint64 parses=0,failures=0;
QCache<QString,QString> htmlCache(1024);
QString formula(const Symbol& symbol,qreal pointSize,const QColor& ink=QColor("#174944")){
    const auto key=symbol.tex+':'+QString::number(pointSize,'g',8)+':'+ink.name(QColor::HexArgb);
    if(const auto* cached=htmlCache.object(key))return *cached;
    // Configure the math-sans slot as well as text-sans. Math mode otherwise
    // starts italic and can select the engine's serif fallback, even when the
    // surrounding interface is sans-serif. Use the established UI face upright
    // and keep subordinate symbols legible at ordinary desktop sizes.
    JKQTMathText math;math.useAnyUnicode("Microsoft YaHei UI","Microsoft YaHei UI");
    math.setSubsuperSizeFactor(.76);math.setFracNestedFactor(.84);
    math.setFontSize(pointSize);++parses;bool ok=false;
    QString text;const bool parsed=math.parse("$\\mathsf{"+symbol.tex+"}$");if(parsed)text=math.toHtml(&ok,pointSize*96./72.);
    if(!ok&&parsed){
        // JKQTMathText cannot serialize fractions to Qt's HTML subset. Render
        // that same parsed expression at 4x resolution instead of silently
        // dropping the fraction or substituting a plain slash.
        math.setFontColor(ink);
        const auto image=math.drawIntoImage(false,Qt::transparent,1,4.0);
        if(!image.isNull()){
            QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");
            const auto logical=image.deviceIndependentSize();
            text=QString("<img src='data:image/png;base64,%1' width='%2' height='%3' alt='%4' style='vertical-align:middle'>")
                .arg(QString::fromLatin1(png.toBase64()),QString::number(qCeil(logical.width())),QString::number(qCeil(logical.height())),symbol.ascii.toHtmlEscaped());
            ok=true;
        }
    }
    if(!ok){++failures;return symbol.ascii.toHtmlEscaped();}
    // QTextDocument accepts the Unicode degree sign. Its HTML subset does
    // not recognize MathText's named small-circle entity.
    text.replace("&SmallCircle;","°");
    text.replace("&percent;","%").replace("&percnt;","%");
    // Inherit ink and UI font from the surface, including disabled/selection states.
    static const QRegularExpression inheritedInk(R"((?:color|font-family)\s*:[^;"']+;?)");text.remove(inheritedInk);
    // The report bridge may receive already-typeset fragments (unitHtml).
    // Keep their inner m/s/t or subscript text from being tokenized a second
    // time when htmlDocument processes the enclosing report.
    text="<!--studio-math-->"+text+"<!--/studio-math-->";
    htmlCache.insert(key,new QString(text));return text;
}
struct Span {qsizetype position,length;Symbol symbol;};
QList<Span> parseSpans(const QString& text){
    QList<Span> result;
    static const QRegularExpression explicitMath(R"((?<!\$)\$(?!\$)([^$\r\n]{1,2000})\$(?!\$))");
    static const QRegularExpression path(R"((?:[A-Za-z]:[\\/]|://|\.(?:fbm|ftm|ini|json|csv|png|dxf|obj|stl|txt)\b))",QRegularExpression::CaseInsensitiveOption);
    if(path.match(text).hasMatch())return result;
    auto latex=explicitMath.globalMatch(text);while(latex.hasNext()){
        const auto match=latex.next();result.append({match.capturedStart(),match.capturedLength(),{match.captured(),{},{},match.captured(1),match.captured(1)}});
    }
    if(result.isEmpty()&&literal(text))return result;
    const auto overlaps=[&](qsizetype begin,qsizetype end){for(const auto& span:result)if(begin<span.position+span.length&&span.position<end)return true;return false;};
    auto found=tokens().globalMatch(text);while(found.hasNext()){const auto m=found.next();if(!overlaps(m.capturedStart(),m.capturedEnd()))result.append({m.capturedStart(),m.capturedLength(),registry()[indices()[m.captured()]]});}
    const auto addUnit=[&](qsizetype at,qsizetype length,const QString& source){const auto u=unitDescription(source);if(!u.tex.isEmpty())result.append({at,length,{source,{},{},u.tex,u.plain}});};
    // A lone m/s/t/N is only a unit when explicitly delimited or when the
    // whole field is a unit. Do not turn prose letters into mathematical ink.
    const auto whole=unitDescription(text);if(!whole.tex.isEmpty())addUnit(0,text.size(),text);
    else{
        static const QRegularExpression brackets(R"([\[（(]\s*([^\[\]()（）]{1,40}?)\s*[\]）)])");
        auto bracket=brackets.globalMatch(text);while(bracket.hasNext()){const auto m=bracket.next();if(!overlaps(m.capturedStart(1),m.capturedEnd(1)))addUnit(m.capturedStart(1),m.capturedLength(1),m.captured(1));}
        static const QRegularExpression compound([]{
            const QString atom=R"((?:kg|kN|N|ft|mm|cm|km|min|sec|rad|t|m|s)(?:\^\{?-?\d+\}?|[²³⁴234])?)";
            return R"((?<![A-Za-z0-9_/.])(?:)"+atom+R"((?:\s*[/*·]\s*)"+atom+
                R"()+|kg\s+m(?:\^2|²|2)|m\s+rad|(?:r|1)/min|1/s|n mile|m(?:\^[234]|[²³⁴234]))(?![A-Za-z0-9_/.]))";
        }());
        auto unit=compound.globalMatch(text);while(unit.hasNext()){
            const auto match=unit.next();
            if(!overlaps(match.capturedStart(),match.capturedEnd()))
                addUnit(match.capturedStart(),match.capturedLength(),match.captured());
        }
    }
    static const QRegularExpression powerOfTen(R"((?<![A-Za-z0-9_.])(?:([+-]?\d+(?:[.,]\d+)?)\s*[*×]\s*)?10\^(?:\((-?\d+)\)|\{(-?\d+)\}|(-?\d+))(?![A-Za-z0-9_]))");
    auto powers=powerOfTen.globalMatch(text);while(powers.hasNext()){
        const auto match=powers.next();
        if(overlaps(match.capturedStart(),match.capturedEnd()))continue;
        QString exponent;
        for(int group=2;group<=4;++group)if(match.capturedStart(group)>=0)exponent=match.captured(group);
        const auto factor=match.captured(1);
        const auto tex=(factor.isEmpty()?QString{}:factor+"\\times ")+"10^{"+exponent+"}";
        result.append({match.capturedStart(),match.capturedLength(),{match.captured(),{},{},tex,match.captured()}});
    }
    std::sort(result.begin(),result.end(),[](const Span& a,const Span& b){return a.position<b.position;});return result;
}
// Detection depends only on the source string. Measurement and painting of
// repeated tick labels share even empty results (ordinary, unformatted text).
QList<Span> spans(const QString& text){
    if(text.size()>256)return parseSpans(text);
    thread_local QCache<QString,QList<Span>> cache(1024);
    if(auto* hit=cache.object(text))return *hit;
    auto spans=parseSpans(text);
    cache.insert(text,new QList<Span>(spans));
    return spans;
}
QString renderHtml(const QString& text,qreal pointSize,const QColor& ink=QColor("#174944")){
    QString result;qsizetype at=0;for(const auto& span:spans(text)){result+=text.mid(at,span.position-at).toHtmlEscaped()+formula(span.symbol,pointSize,ink);at=span.position+span.length;}
    return result+text.mid(at).toHtmlEscaped();
}
QCache<QString,QStaticText> layoutCache(1024);
QCache<QString,QTextDocument> documentCache(256);
bool hasFraction(const QString& text){for(const auto& span:spans(text))if(span.symbol.tex.contains("\\frac")||span.symbol.alias.startsWith('$'))return true;return false;}
QTextDocument& richLayout(const QString& text,const QFont& font,qreal width,const QColor& ink=QColor("#174944")){
    const auto key=text+QChar(0x1f)+font.toString()+':'+QString::number(width,'g',8)+':'+ink.name(QColor::HexArgb);
    if(auto* cached=documentCache.object(key))return *cached;
    // QStaticText does not support embedded images. Painter-only fields use
    // the same rich document path as reports when an expression needs one.
    auto* document=new QTextDocument;document->setDocumentMargin(0);document->setDefaultFont(font);
    auto option=document->defaultTextOption();option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document->setDefaultTextOption(option);
    document->setHtml(renderHtml(text,font.pointSizeF()>0?font.pointSizeF():10.5,ink).replace('\n',"<br>"));
    if(width>0)document->setTextWidth(width);else document->adjustSize();
    documentCache.insert(key,document);return *document;
}
const QStaticText& layout(const QString& text,const QFont& font,qreal width){
    const QString key=text+QChar(0x1f)+font.toString()+':'+QString::number(width,'g',8);
    if(const auto* cached=layoutCache.object(key))return *cached;
    auto* value=new QStaticText(html(text,font.pointSizeF()>0?font.pointSizeF():(font.pixelSize()>0?font.pixelSize()*72./96.:10.5)));value->setTextFormat(Qt::RichText);
    value->setTextWidth(width);QTextOption option;option.setWrapMode(width>0?QTextOption::WordWrap:QTextOption::NoWrap);value->setTextOption(option);
    value->prepare(QTransform{},font);layoutCache.insert(key,value);return *value;
}
}
QString unit(const QString& source){const auto value=unitDescription(source);return value.tex.isEmpty()?source.trimmed():value.plain;}
QString unitLatex(const QString& source){return unitDescription(source).tex;}
QString unitHtml(const QString& source,qreal pointSize){const auto value=unitDescription(source);return value.tex.isEmpty()?source.toHtmlEscaped():formula({source,{},{},value.tex,value.plain},pointSize);}
QString latexHtml(const QString& source,qreal pointSize){return formula({source,{},{},source,source},pointSize);}
QString displayHtml(const QString& text,qreal pointSize){
    // Display equations get their own reading measure. Keep the explanation as
    // real text rather than rasterizing a long line of math and Chinese prose.
    static const QRegularExpression equations(R"((?<!\$)\$(?!\$)([^$\r\n]{1,2000})\$(?!\$))");
    auto matches=equations.globalMatch(text);QString result;qsizetype at=0;
    const auto prose=[&](QString value){
        value=value.trimmed();
        while(!value.isEmpty()&&QStringLiteral(";；。.").contains(value.front()))value.remove(0,1);
        value=value.trimmed();if(!value.isEmpty())result+="<p style='margin-top:8px;margin-bottom:10px'>"+html(value)+"</p>";
    };
    while(matches.hasNext()){
        const auto match=matches.next();if(!match.captured(1).contains("\\frac"))continue;
        prose(text.mid(at,match.capturedStart()-at));
        result+="<p style='margin-top:10px;margin-bottom:14px'>"+latexHtml("\\displaystyle "+match.captured(1),pointSize)+"</p>";
        at=match.capturedEnd();
    }
    prose(text.mid(at));return result;
}
QPair<QString,QString> splitLabelUnit(const QString& label){
    static const QRegularExpression numbered(R"(^(.+?)[,，]\s*([^,，:：]+)[:：]\s*(\d+)\s*$)");
    const auto indexed=numbered.match(label);
    if(indexed.hasMatch()&&!unitDescription(indexed.captured(2)).tex.isEmpty())
        return {indexed.captured(1).trimmed()+' '+indexed.captured(3),unit(indexed.captured(2))};
    static const QRegularExpression beforeSymbol(R"(^(.+?)\s*\[([^\[\]]{1,30})\]\s{2,}([A-Za-z][A-Za-z0-9_]{0,7})\s*$)");
    const auto legacy=beforeSymbol.match(label);
    if(legacy.hasMatch()&&!unitDescription(legacy.captured(2)).tex.isEmpty())
        return {legacy.captured(1).trimmed()+' '+legacy.captured(3),unit(legacy.captured(2))};
    static const QRegularExpression suffix(R"(\s*(?:[,，]?\s*[\[（(]\s*([^\[\]()（）]{1,40}?)\s*[\]）)]|[,，]\s*([^,，:：\[\]()（）]{1,40}?))\s*[:：]?\s*$)");
    const auto match=suffix.match(label);
    const auto token=match.captured(1).isEmpty()?match.captured(2):match.captured(1);
    if(!match.hasMatch()||unitDescription(token).tex.isEmpty())return {label,{}};
    return {label.left(match.capturedStart()).trimmed(),unit(token)};
}
QJsonArray catalog(){QJsonArray result;for(const auto& s:registry())result.append(QJsonObject{{"alias",s.alias},{"latex",s.tex},{"plain",s.ascii},{"base",s.base},{"subscript",s.sub}});return result;}
QJsonArray symbols(const QString& text){
    QJsonArray result;for(const auto& span:spans(text))result.append(QJsonObject{{"source",text.mid(span.position,span.length)},{"position",span.position},{"latex",span.symbol.tex},{"plain",span.symbol.ascii}});return result;
}
bool contains(const QString& text){return !spans(text).isEmpty();}
QString plain(const QString& text){
    QString result;qsizetype at=0;for(const auto& span:spans(text)){result+=text.mid(at,span.position-at)+span.symbol.ascii;at=span.position+span.length;}
    return result+text.mid(at);
}
QString html(const QString& text,qreal pointSize){
    return renderHtml(text,pointSize);
}
QString htmlDocument(const QString& source,qreal pointSize){
    // Only HTML text nodes. Attributes, resources and links are never rewritten.
    QString result;qsizetype at=0;while(at<source.size()){
        if(source.mid(at,18)=="<!--studio-math-->"){
            const auto end=source.indexOf("<!--/studio-math-->",at+18);
            if(end>=0){const auto length=end+19-at;result+=source.mid(at,length);at+=length;continue;}
        }
        if(source[at]=='<'){auto end=source.indexOf('>',at);if(end<0){result+=source.mid(at);break;}result+=source.mid(at,end-at+1);at=end+1;}
        else{auto end=source.indexOf('<',at);if(end<0)end=source.size();const auto text=source.mid(at,end-at);
            // Existing entities remain entities; raw text is not escaped twice.
            qsizetype begin=0;for(const auto& span:spans(text)){result+=text.mid(begin,span.position-begin)+formula(span.symbol,pointSize);begin=span.position+span.length;}
            result+=text.mid(begin);at=end;
        }
    }return result;
}
QSizeF size(const QString& text,const QFont& font,qreal width){return width>0||hasFraction(text)?richLayout(text,font,width).size():contains(text)?layout(text,font,width).size():QSizeF(QFontMetricsF(font).horizontalAdvance(text),QFontMetricsF(font).height());}
void draw(QPainter& painter,const QRectF& rect,int flags,const QString& text){
    const auto width=flags&Qt::TextWordWrap?rect.width():-1;
    if(width<=0&&!contains(text)){painter.drawText(rect,flags,text);return;}
    const auto extent=size(text,painter.font(),width);
    qreal x=rect.left(),y=rect.top();if(flags&Qt::AlignRight)x=rect.right()-extent.width();else if(flags&Qt::AlignHCenter)x+=(rect.width()-extent.width())/2;
    if(flags&Qt::AlignVCenter)y+=(rect.height()-extent.height())/2;else if(flags&Qt::AlignBottom)y=rect.bottom()-extent.height();
    if(width>0||hasFraction(text)){
        auto& document=richLayout(text,painter.font(),width,painter.pen().color());QAbstractTextDocumentLayout::PaintContext context;context.palette.setColor(QPalette::Text,painter.pen().color());
        painter.save();painter.setClipRect(rect,Qt::IntersectClip);painter.translate(x,y);document.documentLayout()->draw(&painter,context);painter.restore();
    }else painter.drawStaticText(QPointF(x,y),layout(text,painter.font(),width));
}
void drawBaseline(QPainter& painter,const QPointF& baseline,const QString& text){
    if(!contains(text)){painter.drawText(baseline,text);return;}
    if(hasFraction(text))draw(painter,QRectF(baseline-QPointF(0,QFontMetricsF(painter.font()).ascent()),size(text,painter.font())),Qt::AlignLeft|Qt::AlignTop,text);
    else painter.drawStaticText(baseline-QPointF(0,QFontMetricsF(painter.font()).ascent()),layout(text,painter.font(),-1));
}
QJsonObject statistics(){return {{"engine","JKQTMathText"},{"parse_count",qint64(parses)},{"parse_failures",qint64(failures)},{"html_cache",htmlCache.size()},{"layout_cache",layoutCache.size()},
    {"font_family","Microsoft YaHei UI"},{"math_style","upright-sans"},{"subscript_size_factor",.76},{"nested_fraction_size_factor",.84},{"fraction_raster_scale",4}};}
}
