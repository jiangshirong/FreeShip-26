// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/math_notation.h"
#include "app/report_layout.h"
#include "app/studio_ui.h"
#include "text_style.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QFontInfo>
#include <QGlyphRun>
#include <QJsonDocument>
#include <QLabel>
#include <QPdfWriter>
#include <QPixmap>
#include <QTableWidget>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextBrowser>
#include <QTextLayout>
#include <QVBoxLayout>

int main(int argc,char** argv){
    QApplication app(argc,argv);applyStudioTheme();
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QString{};QDir().mkpath(output);
    QJsonArray cases,observations;int failed=0;
    const auto check=[&](QString name,bool ok,QJsonObject detail=QJsonObject{}){detail["name"]=name;detail["status"]=ok?"pass":"fail";cases.append(detail);failed+=!ok;};
    // Expected notation supplied independently from the production registry.
    const QList<QPair<QString,QString>> expected{{"Cb","C_b"},{"Cp","C_p"},{"Cvp","C_{vp}"},{"Cm","C_m"},{"Cwp","C_{wp}"},{"KMt","KM_t"},{"Ix","I_x"},{"LWL","L_{WL}"},{"KT","K_T"},{"η0","η_0"}};
    for(const auto& entry:expected){
        check(entry.first+".canonical",notation::plain(entry.first)==entry.second);
        QTextDocument doc;report::setHtml(&doc,"<p>"+entry.first+"</p>");QString subs;bool subUpright=true;
        for(auto b=doc.begin();b.isValid();b=b.next())for(auto i=b.begin();!i.atEnd();++i){auto f=i.fragment();if(f.charFormat().verticalAlignment()==QTextCharFormat::AlignSubScript){subs+=f.text();subUpright&=!f.charFormat().fontItalic();}}
        check(entry.first+".actual-subscript",!subs.isEmpty()&&subUpright,{{"subscript",subs},{"html",doc.toHtml()}});
    }
    for(const auto& text:QStringList{"LCB","LCF","VCB","Al","Fe","Mn","Re","Rho","Nu","GZ","KN","Bonjean","FreeShip 26","Ctrl+O","C:/Cp/Cb.fbm","https://example.org/Cp","Cb_data"})check("literal-"+text,!notation::contains(text)&&notation::plain(text)==text);
    const auto linked=notation::htmlDocument("<a href='https://example.org/Cp'>Cb</a><img src='studio-message:explanation'>");
    check("attributes-unchanged",linked.contains("href='https://example.org/Cp'")&&linked.contains("src='studio-message:explanation'"));
    // Physical units use invariant symbols, with actual TeX superscripts and
    // fractions, independent of the selected UI translation.
    const QList<QPair<QString,QString>> units{{"m2","m²"},{"m^2","m²"},{"m³","m³"},{"m4","m⁴"},
        {"度","°"},{"deg","°"},{"米","m"},{"吨","t"},{"秒","s"},{"t/m^3","t/m³"},{"m/s","m/s"},{"N","N"},
        {"kg m2","kg·m²"},{"m rad","m·rad"},{"m2/sec","m²/s"},{"n mile","n mile"}};
    for(const auto& item:units)check("unit-canonical-"+item.first,notation::unit(item.first)==item.second);
    check("unit-no-scale-conversion",notation::unit("kN")=="kN"&&notation::unit("mm")=="mm");
    check("unit-imperial-preserved",notation::unit("ft")=="ft"&&notation::unit("inch")=="in"&&notation::unit("long tons")=="long ton"&&notation::unit("knots")=="kn");
    check("unit-scaled-viscosity",notation::unitLatex("10⁻⁶ m²/s")=="10^{-6}\\,\\frac{\\mathrm{m}^{2}}{\\mathrm{s}}");
    check("unit-preserves-unrecognized-name",notation::unit("custom-unit")=="custom-unit");
    check("unit-mixed-label",notation::plain("受风面积 [m2]")=="受风面积 [m²]"&&notation::plain("风向角 [度]")=="风向角 [°]");
    const auto split=notation::splitLabelUnit("横向受风投影面积 [m2]");
    check("unit-label-separation",split.first=="横向受风投影面积"&&split.second=="m²");
    check("non-unit-parentheses-preserved",notation::splitLabelUnit("船型（1–15）").second.isEmpty());
    for(const auto& literal:QStringList{"C:/m2/m3.fbm","https://example.org/m2","model_m2","some text with m and s"})
        check("unit-literal-"+literal,!notation::contains(literal)&&notation::plain(literal)==literal);
    QTextDocument unitDocument;report::setHtml(&unitDocument,"<p>Area [m2], speed [m/s], density [t/m3], angle [度]</p>");
    int superscripts=0,fractionImages=0;bool validImages=true;
    for(auto b=unitDocument.begin();b.isValid();b=b.next())for(auto i=b.begin();!i.atEnd();++i){const auto fragment=i.fragment();const auto format=fragment.charFormat();
        if(format.verticalAlignment()==QTextCharFormat::AlignSuperScript)++superscripts;
        if(format.isImageFormat()){const auto image=format.toImageFormat();if(image.name().startsWith("data:image/png;base64,")){
            ++fractionImages;const auto source=unitDocument.resource(QTextDocument::ImageResource,QUrl(image.name()));
            validImages&=!qvariant_cast<QImage>(source).isNull()||!qvariant_cast<QPixmap>(source).isNull();
            validImages&=image.height()>0&&image.height()<80&&image.width()>0&&image.width()<120;
        }}
    }
    check("unit-actual-superscript",superscripts>0);
    check("unit-degree-is-real-glyph",unitDocument.toPlainText().contains(QChar(0x00b0))&&!unitDocument.toPlainText().contains("SmallCircle"));
    check("unit-actual-fraction-render",fractionImages==2&&validImages,{{"images",fractionImages},{"html",unitDocument.toHtml()}});
    check("unit-tex-fraction-definition",notation::unitLatex("m/s")=="\\frac{\\mathrm{m}}{\\mathrm{s}}");
    QTextDocument ratioReport;
    report::setHtml(&ratioReport,"<p>Bwl/Tc · Lwl/V^(1/3) · Tc/T · Aw/V^0.667</p><p>%</p>");
    int ratioImages=0;
    for(auto b=ratioReport.begin();b.isValid();b=b.next())for(auto i=b.begin();!i.atEnd();++i)
        if(i.fragment().charFormat().isImageFormat())++ratioImages;
    check("legacy-delft-ratios-are-full-fractions",ratioImages==4&&!ratioReport.toPlainText().contains("^"));
    const auto lengthRatio=notation::symbols("Lwl/V^(1/3)").first().toObject()["latex"].toString();
    const auto areaRatio=notation::symbols("Aw/V^0.667").first().toObject()["latex"].toString();
    check("delft-length-fraction-in-exponent",lengthRatio.contains("V^{\\frac{1}{3}}"));
    check("delft-area-exact-fraction-in-exponent",areaRatio.contains("V^{\\frac{2}{3}}"));
    for(const auto& equation:QStringList{"SVLR = S/(VL)^0,5","SVLR = S/(VL)^0.5"}){
        QTextDocument svlrReport;
        report::setHtml(&svlrReport,"<p>"+equation+"</p>");
        const auto fragment=svlrReport.begin().begin().fragment();
        const auto symbols=notation::symbols(equation);
        check("round-bilge-svlr-"+equation.right(3),symbols.size()==1&&
            symbols.first().toObject()["latex"]=="\\mathrm{SVLR}=\\frac{S}{\\sqrt{VL}}"&&
            fragment.charFormat().isImageFormat()&&
            fragment.charFormat().toImageFormat().name().startsWith("data:image/png;base64,")&&
            !svlrReport.toPlainText().contains(equation));
    }
    check("reciprocal-minute-fraction",notation::unitLatex("1/min")=="\\frac{1}{\\mathrm{min}}");
    check("inline-scientific-notation",notation::symbols("ν = 1.0000*10^(-6)").first()
        .toObject()["latex"]=="1.0000\\times 10^{-6}");
    check("unit-percent-is-real-glyph",ratioReport.toPlainText().trimmed().endsWith("%")&&
        !ratioReport.toPlainText().contains("&percent;"));
    for(const QString& ratio:QStringList{"L/V^0.333","Ap/V^0.6667"})
        check("legacy-resistance-ratio-"+ratio,notation::html(ratio).contains("data:image/png;base64,"));
    const QString equation="$C_x=\\frac{F_x}{0.5\\rho_a V_r^2 A_T}$";
    const auto explicitHtml=notation::html(equation);QTextDocument equationDocument;
    report::setHtml(&equationDocument,"<p>"+equation.toHtmlEscaped()+"</p>");
    check("explicit-latex-fraction",notation::contains(equation)&&explicitHtml.contains("data:image/png;base64,")&&!explicitHtml.contains("$C_x"));
    const auto prepared=notation::unitHtml("m/s");
    check("already-typeset-unit-not-rewritten",notation::htmlDocument(prepared)==prepared);
    check("explicit-latex-path-safe",notation::html("C:/work/$C_x$.fbm")==QString("C:/work/$C_x$.fbm").toHtmlEscaped());
    // Readability requirement from the user's actual wind-method screenshot:
    // three complete equations, each on its own larger line; the display-scale
    // explanation stays selectable prose, with only its symbol inline.
    const QString definitions="$C_x=\\frac{F_x}{\\frac{1}{2}\\rho_a V_r^2 A_T}$；"
        "$C_y=\\frac{F_y}{\\frac{1}{2}\\rho_a V_r^2 A_T}$；"
        "$C_{mz}=\\frac{M_z}{\\frac{1}{2}\\rho_a V_r^2 A_T L_{OA}}$。"
        "曲线中的 $10 C_{mz}$ 是显示倍率，力矩计算不乘 10。";
    const auto readable=notation::displayHtml(definitions);
    QTextDocument display;display.setDefaultFont(QFont("Microsoft YaHei UI",11));report::setHtml(&display,readable);
    int displayEquations=0;double smallestEquation=1000;QString proseText;
    for(auto b=display.begin();b.isValid();b=b.next())for(auto it=b.begin();!it.atEnd();++it){
        const auto fragment=it.fragment();if(fragment.charFormat().isImageFormat()){
            ++displayEquations;smallestEquation=qMin(smallestEquation,fragment.charFormat().toImageFormat().height());
        }else proseText+=fragment.text();
    }
    check("display-equations-separate-and-readable",displayEquations==3&&smallestEquation>30,{{"equations",displayEquations},{"minimum_height",smallestEquation}});
    check("display-explanation-stays-live-text",proseText.contains("曲线中的")&&proseText.contains("是显示倍率，力矩计算不乘 10"));
    check("display-prepared-math-not-rewritten",notation::htmlDocument(readable)==readable);
    const auto upright=notation::html("Cx");QTextDocument uprightDocument;report::setHtml(&uprightDocument,upright);bool anyItalic=false;
    for(auto b=uprightDocument.begin();b.isValid();b=b.next())for(auto it=b.begin();!it.atEnd();++it)anyItalic|=it.fragment().charFormat().fontItalic();
    check("compact-symbols-upright",!anyItalic);
    QJsonArray displayCaptures;
    for(qreal ratio:{1.,1.5,2.}){
        display.setTextWidth(720);const QSize logical(760,qCeil(display.size().height())+32);
        QImage frame(QSize(qCeil(logical.width()*ratio),qCeil(logical.height()*ratio)),QImage::Format_ARGB32_Premultiplied);
        frame.setDevicePixelRatio(ratio);frame.fill(Qt::white);QPainter target(&frame);target.setRenderHint(QPainter::TextAntialiasing);target.setRenderHint(QPainter::SmoothPixmapTransform);
        target.translate(18,12);display.drawContents(&target);target.end();const auto path=output+"/display-"+QString::number(ratio)+".png";
        check("display-render-scale-"+QString::number(ratio),frame.save(path));displayCaptures.append(QJsonObject{{"ratio",ratio},{"width",logical.width()},{"height",logical.height()},{"path",path}});
    }
    const QFont font("Microsoft YaHei UI",11);
    for(const auto& value:notation::catalog()){
        auto row=value.toObject();const auto alias=row["alias"].toString();const auto html=notation::html(alias);row["html"]=html;row["width"]=notation::size(alias,font).width();
        const bool fraction=row["latex"].toString().contains("\\frac");
        const bool rendered=fraction?html.contains("data:image/png;base64,"):
            (html.contains("<sub")||html.contains("<sup"))&&!html.contains("_{");
        check("latex-parses-"+alias,rendered&&row["width"].toDouble()>0);observations.append(row);
    }
    auto before=notation::statistics()["parse_count"].toInteger();for(int i=0;i<1000;++i)notation::size("Cb / Cvp / KMt",font);
    auto after=notation::statistics()["parse_count"].toInteger();check("repaint-reuses-layout",after-before<=3);
    QWidget window;window.setAttribute(Qt::WA_DontShowOnScreen);auto* layout=new QVBoxLayout(&window);
    auto* label=new QLabel("方形系数 Cb，棱形系数 Cp，垂向棱形系数 Cvp，横稳心 KMt");label->setWordWrap(true);layout->addWidget(label);
    auto* table=new QTableWidget(4,3);table->setHorizontalHeaderLabels({"Cb / Cp / Cvp","KMt / KMl","Ix / Iy"});
    for(int i=0;i<4;++i)for(int j=0;j<3;++j)table->setItem(i,j,new QTableWidgetItem(QStringList{"Cb","Cvp","KMt"}[j]));table->setMinimumHeight(235);table->resizeColumnsToContents();layout->addWidget(table);
    window.resize(840,360);window.show();app.processEvents();check("widgets-render",window.grab().save(output+"/widgets.png"));
    QImage image(950,320,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);painter.setFont(font);painter.setPen(QColor("#174944"));
    notation::draw(painter,QRectF(18,15,900,55),Qt::AlignLeft|Qt::AlignVCenter,"Cb   Cp   Cvp   Cm   Cwp   KMt   KMl   KT   η0");
    painter.setPen(QColor("#929b96"));notation::draw(painter,QRectF(18,80,900,55),Qt::AlignLeft|Qt::AlignVCenter,"Cb   Cp   Cvp   Cm   Cwp   KMt   KMl   KT   η0");
    painter.setPen(QColor("#174944"));notation::draw(painter,QRectF(18,155,900,100),Qt::AlignLeft|Qt::AlignVCenter,"Area [m2]   Volume [m3]   Speed [m/s]   Density [t/m3]   Angle [度]");
    notation::draw(painter,QRectF(18,250,900,65),Qt::AlignLeft|Qt::AlignVCenter,equation);
    painter.end();check("painter-render",image.save(output+"/painter.png"));
    QTextDocument report;report.setDefaultFont(font);report::setHtml(&report,"<h3>工程参数</h3><p>Cb / Cp / Cvp / Cm / Cwp / KMt / KMl / Ix / Iy / LWL / KT / η0</p><p>Area [m2] · Volume [m3] · Speed [m/s] · Density [t/m3] · Angle [度]</p>");
    QPdfWriter pdf(output+"/symbols.pdf");report.print(&pdf);
    check("latex-parser-no-errors",notation::statistics()["parse_failures"].toInteger()==0,notation::statistics());
    // Capture actual glyph faces for the user's report title/section/header
    // contrast. Font-family names alone do not reveal linked CJK bitmap faces.
    QTextBrowser typography;typography.setAttribute(Qt::WA_DontShowOnScreen);typography.resize(900,490);
    const auto sample=report::heading("FreeShip 26 — 空气动力特性",true,true)+
        report::render({"本次计算参数",{"参数","数值","单位"},{{"总长 Loa","192.6279297","m"},{"侧向受风投影面积","1000","m²"}},{0}},true)+
        report::message("基准说明：吃水、浮心与水线面积。",true,StudioMessageRole::Explanation,"计算基准")+
        "<p><b>普通加粗文字：报告结果与单位</b></p>";
    report::setHtml(typography.document(),sample);typography.show();app.processEvents();
    check("report-typography-capture",typography.grab().save(output+"/report-typography.png"));
    QJsonArray typeRows;
    for(auto block=typography.document()->begin();block.isValid();block=block.next()){
        if(block.text().trimmed().isEmpty())continue;QJsonArray fragments,runs;
        for(auto it=block.begin();!it.atEnd();++it){const auto f=it.fragment();const auto face=f.charFormat().font();
            fragments.append(QJsonObject{{"text",f.text()},{"font",face.toString()},{"resolved",QFontInfo(face).family()},{"weight",face.weight()},{"point_size",face.pointSizeF()}});
        }
        QFont observed=block.begin().fragment().charFormat().font();QTextLayout shaped(block.text(),observed);
        shaped.beginLayout();auto line=shaped.createLine();if(line.isValid())line.setLineWidth(2000);shaped.endLayout();
        for(const auto& run:shaped.glyphRuns()){
            const auto face=run.rawFont();runs.append(QJsonObject{{"family",face.familyName()},{"style",face.styleName()},{"weight",face.weight()},{"pixel_size",face.pixelSize()},
                {"outline",!face.fontTable("glyf").isEmpty()||!face.fontTable("CFF ").isEmpty()},{"embedded_bitmaps",!face.fontTable("EBDT").isEmpty()}});
        }
        typeRows.append(QJsonObject{{"text",block.text()},{"heading",block.blockFormat().headingLevel()},{"fragments",fragments},{"glyph_runs",runs}});
    }
    check("report-default-keeps-ui-font-and-smooth-rasterization",typography.document()->defaultFont().family()==QApplication::font().family()&&typography.document()->defaultFont().hintingPreference()==QFont::PreferNoHinting);
    for(const auto& entry:typeRows){const auto row=entry.toObject();
        if(row["heading"].toInt()==3){const auto f=row["fragments"].toArray().first().toObject();
            check("section-heading-remains-bold-and-larger-than-body",f["weight"].toInt()==QFont::Bold&&f["point_size"].toDouble()>typography.document()->defaultFont().pointSizeF()*1.25&&f["point_size"].toDouble()<typography.document()->defaultFont().pointSizeF()*1.5);
        }
    }
    const auto shades=[](QFont face){
        face.setPointSizeF(10.5);face.setWeight(QFont::Bold);QImage image(240,44,QImage::Format_RGB32);image.fill(Qt::white);
        image.setDotsPerMeterX(3780);image.setDotsPerMeterY(3780);QPainter p(&image);p.setFont(face);p.setPen(QColor("#174944"));p.drawText(QPointF(3,28),"参数数值单位");p.end();
        QSet<QRgb> colors;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto pixel=image.pixel(x,y);if(pixel!=qRgb(255,255,255)&&pixel!=QColor("#174944").rgb())colors.insert(pixel);}return colors.size();
    };
    auto legacy=QApplication::font();legacy.setHintingPreference(QFont::PreferDefaultHinting);
    const int legacyShades=shades(legacy),smoothShades=shades(studioTextFont(legacy));
    check("small-bold-text-retains-intermediate-edge-shades",smoothShades>legacyShades*2,{{"legacy_edge_colors",legacyShades},{"smooth_edge_colors",smoothShades}});
    QFile typeFile(output+"/report-typography.json");typeFile.open(QIODevice::WriteOnly);typeFile.write(QJsonDocument(QJsonObject{{"default_font",typography.document()->defaultFont().toString()},{"rows",typeRows}}).toJson());
    for(int variant=0;variant<5;++variant){
        auto face=QApplication::font();
        if(variant==1)face.setStyleStrategy(QFont::StyleStrategy(face.styleStrategy()|QFont::ForceOutline));
        if(variant==2)face.setHintingPreference(QFont::PreferNoHinting);
        if(variant==3)face.setHintingPreference(QFont::PreferVerticalHinting);
        if(variant==4)face.setHintingPreference(QFont::PreferNoHinting);
        typography.document()->setDefaultFont(face);report::setHtml(typography.document(),sample);
        if(variant==4){QTextCursor all(typography.document());all.select(QTextCursor::Document);QTextCharFormat smooth;smooth.setFontHintingPreference(QFont::PreferNoHinting);all.mergeCharFormat(smooth);}
        app.processEvents();
        check("report-typography-variant-"+QString::number(variant),typography.grab().save(output+"/report-type-variant-"+QString::number(variant)+".png"));
    }
    QFile file(output+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failed?"fail":"pass"},{"cases",cases},{"symbols",observations},{"display_captures",displayCaptures},{"statistics",notation::statistics()}}).toJson());
    return failed?1:0;
}
