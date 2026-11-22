// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_sheets.h"
#include "station_pdf_units.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs::frames {
namespace {
constexpr double overlap=10.;
constexpr double edgeHeader=12.;
const QRectF ink(10,20,277,168); // A4 landscape, physical millimetres
const QColor shellInk(105,105,105);
QPen templatePen(double width,bool dashed) {
    QPen pen(Qt::black,width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    if(dashed)pen.setDashPattern({12,6});
    return pen;
}
QRectF drawingArea(const Station& sheet,double divisor) {
    const QRectF model=plotBounds(sheet);
    return QRectF(model.topLeft()/divisor,model.bottomRight()/divisor).adjusted(-4,-4,4,4);
}
const QRectF a4Paper(0,0,297,210);
int tileCount(double size,double span,double lap) {
    const double count=1+std::ceil(std::max(0.,size-span)/(span-lap));
    if(!std::isfinite(count) || count>maximumPdfPages) return maximumPdfPages+1;
    return int(count);
}
QRectF tiledArea(const Station& sheet,double divisor,bool edge) {
    auto bounds=drawingArea(sheet,divisor);
    if(!edge)return bounds;
    bounds.adjust(0,0,0,edgeHeader);
    // Keep Z=0 on the drawing when it fits. A datum outside the part
    // should not force an extra row of sheets.
    double low=INFINITY,high=-INFINITY;
    const auto expand=[&](const QPolygonF& path){
        const auto extent=path.boundingRect();
        low=std::min(low,extent.top());
        high=std::max(high,extent.bottom());
    };
    for(const auto& path:sheet.outlines)expand(path);
    for(const auto& path:sheet.shellReference)expand(path);
    const double bottom=low/divisor-4,top=high/divisor+4+edgeHeader;
    if(tileCount(top-bottom,a4Paper.height(),0)<tileCount(bounds.height(),a4Paper.height(),0)){
        bounds.setTop(bottom);
        bounds.setBottom(top);
    }
    return bounds;
}
struct Tiles {int rows,cols;};
bool firstAssemblyGuide(Tiles tiles,QSet<QPair<int,int>>& layouts) {
    if(tiles.rows==1 && tiles.cols==1)return false;
    const QPair<int,int> layout{tiles.rows,tiles.cols};
    if(layouts.contains(layout))return false;
    layouts.insert(layout);
    return true;
}
QString tileCode(Tiles tiles,int row,int col) {
    return QString::number(row*tiles.cols+col+1);
}
Tiles tilesFor(const Station& sheet,double divisor,bool edge) {
    const auto b=tiledArea(sheet,divisor,edge);
    const auto area=edge?a4Paper:ink;
    const double lap=edge?0.:overlap;
    return {tileCount(b.height(),area.height(),lap),tileCount(b.width(),area.width(),lap)};
}
void ensureSheets(const QList<Station>& sheets) {
    if(sheets.isEmpty()) throw std::runtime_error("请选择横剖站。");
    for(const auto& s:sheets) if(!s.error.isEmpty() || s.outlines.isEmpty())
        throw std::runtime_error("所选站位含无效轮廓。");
}
void openTarget(QSaveFile& file) {
    file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)) throw std::runtime_error(file.errorString().toStdString());
}
void checkCancelled(const std::atomic_bool& cancelled) {
    if(cancelled.load()) throw std::runtime_error("已取消导出。");
}
void crossMark(QPainter& p,QPointF at) {
    p.drawLine(at-QPointF(2,0),at+QPointF(2,0));
    p.drawLine(at-QPointF(0,2),at+QPointF(0,2));
}
void scissors(QPainter& p,QPointF position,bool vertical,QColor color) {
    p.save();
    p.translate(position);
    if(vertical)p.rotate(-90);
    p.setPen(QPen(color,.30));p.setBrush(Qt::NoBrush);
    for(double side:{-1.,1.}){
        p.drawEllipse(QPointF(-1.75,side*.95),.78,.68);
        p.drawLine(QPointF(-1.1,side*.65),QPointF(2.3,-side*1.0));
    }
    p.restore();
}
// Work in paper mm. Clip each hull segment to a narrow strip about the cut,
// including segments which run along it, then leave those intervals unprinted.
void cutGuide(QPainter& p,const Station& sheet,const QPointF& corner,double divisor,bool vertical) {
    const double axis=vertical?ink.left()+overlap/2:ink.top()+overlap/2;
    const double begin=6,end=vertical?204:291;
    QList<std::pair<double,double>> gaps;
    const auto paperPoint=[&](QPointF model){
        const QPointF rel=model/divisor-corner;
        const QPointF paper=ink.topLeft()+QPointF(rel.x(),-rel.y());
        return vertical?paper:QPointF(paper.y(),paper.x());
    };
    const auto collectGaps=[&](const QPolygonF& path){for(int i=1;i<path.size();++i){
        auto a=paperPoint(path[i-1]),b=paperPoint(path[i]);
        if(a.x()>b.x())std::swap(a,b);
        constexpr double clearance=1.0;
        if(b.x()<axis-clearance||a.x()>axis+clearance)continue;
        const auto d=b-a;
        double y0=a.y(),y1=b.y();
        if(d.x()>0){
            y0+=d.y()*std::clamp((axis-clearance-a.x())/d.x(),0.,1.);
            y1=a.y()+d.y()*std::clamp((axis+clearance-a.x())/d.x(),0.,1.);
        }
        const double lo=std::max(begin,std::min(y0,y1)-clearance);
        const double hi=std::min(end,std::max(y0,y1)+clearance);
        if(lo<hi)gaps.append({lo,hi});
    }};
    for(const auto& path:sheet.outlines)collectGaps(path);
    for(const auto& path:sheet.shellReference)collectGaps(path);
    std::sort(gaps.begin(),gaps.end());
    QPen pen(QColor(155,155,155),.16,Qt::CustomDashLine,Qt::FlatCap);
    pen.setDashPattern({12,8});p.setPen(pen);
    const auto line=[&](double from,double to){
        if(from<to)p.drawLine(vertical?QLineF(axis,from,axis,to):QLineF(from,axis,to,axis));
    };
    double cursor=begin;
    for(auto [lo,hi]:gaps){line(cursor,lo);cursor=std::max(cursor,hi);}
    line(cursor,end);
    // Two handles and two blades, independent of the printer's symbol fonts.
    scissors(p,vertical?QPointF(axis+3,199):QPointF(8,axis-3),vertical,QColor(110,110,110));
}
void drawAssemblyGuide(QPainter& p,const Station& sheet,Tiles tiles,double divisor,const SheetLabels& labels,bool edge) {
    p.save();
    p.scale(p.device()->logicalDpiX()/25.4,p.device()->logicalDpiY()/25.4);
    const QColor dark("#203344"),cut("#b64a18"),align("#126c68"),datum("#546b8c");
    auto caption=[&](QRectF rect,const QString& value,int size){
        QFont font=p.font();font.setPixelSize(size);p.setFont(font);p.setPen(dark);
        p.drawText(rect,Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap,value);
    };
    const auto part=sheet.slotLabels.isEmpty()?QStringLiteral("%1 %2").arg(labels.station).arg(sheet.number):labels.spine;
    caption({12,9,273,15},labels.guideTitle.arg(part),7);
    // Fit the actual outline and its axes; a lightly tinted area is the part
    // retained from each sheet after trimming half of its overlap.
    const QRectF b=tiledArea(sheet,divisor,edge);
    const auto area=edge?a4Paper:ink;
    const double lap=edge?0.:overlap;
    const QRectF panel(12,41,145,130);
    const double scale=std::min(panel.width()/b.width(),panel.height()/b.height());
    const QSizeF extent=b.size()*scale;
    const QRectF map(panel.center()-QPointF(extent.width()/2,extent.height()/2),extent);
    auto point=[&](QPointF q){return map.topLeft()+QPointF(q.x()-b.left(),b.bottom()-q.y())*scale;};
    const QColor fills[]={"#ebf2f9","#fcf0dc","#e4f2ea","#f3e9ef"};
    QList<QRectF> tileRects;
    tileRects.reserve(tiles.rows*tiles.cols);
    for(int r=0;r<tiles.rows;++r)for(int c=0;c<tiles.cols;++c){
        const double x=c*(area.width()-lap),y=r*(area.height()-lap);
        QRectF tile(map.left()+(x+(c?lap/2:0))*scale,map.top()+(y+(r?lap/2:0))*scale,0,0);
        tile.setRight(std::min(map.right(),map.left()+(x+area.width()-lap/2)*scale));
        tile.setBottom(std::min(map.bottom(),map.top()+(y+area.height()-lap/2)*scale));
        if(c+1==tiles.cols)tile.setRight(map.right());
        if(r+1==tiles.rows)tile.setBottom(map.bottom());
        tileRects.append(tile);
        p.fillRect(tile,fills[2*(r%2)+c%2]);
        p.setPen(QPen(edge?align:cut,.38,edge?Qt::SolidLine:Qt::DashLine));
        if(c)p.drawLine(tile.topLeft(),tile.bottomLeft());
        if(r)p.drawLine(tile.topLeft(),tile.topRight());
    }
    p.setPen(QPen(datum,.3,Qt::DashLine));
    const double top=b.bottom()-(edge?edgeHeader:0);
    const bool showBaseline=b.top()<=0 && top>=0;
    p.drawLine(point({0,b.top()}),point({0,top}));
    if(showBaseline)p.drawLine(point({b.left(),0}),point({b.right(),0}));
    QPainterPath outlinePath;
    const auto drawOutline=[&](const QPolygonF& path){
        QPolygonF outline;outline.reserve(path.size());
        for(auto q:path)outline.append(point(q/divisor));
        outlinePath.addPolygon(outline);
        p.drawPolyline(outline);
    };
    const bool hasReference=!sheet.shellReference.isEmpty();
    p.setPen(QPen(shellInk,.34));
    for(const auto& path:sheet.shellReference)drawOutline(path);
    p.setPen(templatePen(.6,hasReference));
    for(const auto& path:sheet.outlines)drawOutline(path);
    QPainterPathStroker contourClearance;
    contourClearance.setWidth(1.2);
    const auto outlineInk=contourClearance.createStroke(outlinePath);
    p.save();
    QFont codeFont=p.font();
    codeFont.setPixelSize(5);
    codeFont.setBold(true);
    p.setFont(codeFont);
    const QFontMetricsF metrics(codeFont,p.device());
    for(int r=0;r<tiles.rows;++r)for(int c=0;c<tiles.cols;++c){
        const auto& tile=tileRects.at(r*tiles.cols+c);
        const auto code=tileCode(tiles,r,c);
        const QRectF badge(0,0,metrics.horizontalAdvance(code)+3,metrics.height()+1);
        double fit=std::min({1.,tile.width()*.8/badge.width(),tile.height()*.65/badge.height()});
        QPointF location=tile.center();
        // In a short last row, even a small white badge can hide most of a
        // contour. Try other locations inside its own tile before reducing it.
        bool clear=false;
        for(double factor:{1.,.82,.65}){
            const double trial=fit*factor;
            const QSizeF size=badge.size()*trial;
            const QRectF centres=tile.adjusted(size.width()/2,size.height()/2,-size.width()/2,-size.height()/2);
            for(auto candidate:{tile.center(),QPointF(tile.center().x(),centres.top()),QPointF(tile.center().x(),centres.bottom()),
                                QPointF(centres.left(),tile.center().y()),QPointF(centres.right(),tile.center().y()),
                                centres.topLeft(),centres.topRight(),centres.bottomLeft(),centres.bottomRight()}){
                const QRectF box(candidate-QPointF(size.width()/2,size.height()/2),size);
                if(outlineInk.intersects(box))continue;
                location=candidate;
                fit=trial;
                clear=true;
                break;
            }
            if(clear)break;
        }
        p.save();
        p.translate(location);
        p.scale(fit,fit);
        const auto centered=badge.translated(-badge.center());
        if(clear){
            p.setPen(Qt::NoPen);p.setBrush(Qt::white);
            p.drawRoundedRect(centered,1,1);
        }
        p.setPen(dark);
        p.drawText(centered,Qt::AlignCenter,code);
        p.restore();
    }
    p.restore();
    // A magnified seam and the same symbols as the monochrome working sheets.
    p.fillRect(QRectF(169,44,55,34),fills[0]);p.fillRect(QRectF(224,44,60,34),fills[1]);
    if(edge){
        p.setPen(QPen(align,.5));p.drawLine(QLineF(224,44,224,78));
        caption({169,83,118,30},labels.joinEdges,4);
    }else{
        p.setPen(QPen(cut,.5,Qt::DashLine));p.drawLine(QLineF(224,43,224,79));
        scissors(p,{228,61},true,cut);
        p.setPen(QPen(align,.6));crossMark(p,{224,50});crossMark(p,{224,72});
        p.setPen(QPen(cut,.5,Qt::DashLine));p.drawLine(QLineF(168,91,179,91));scissors(p,{174,88},false,cut);
        caption({184,81,103,21},labels.cut,4);
        p.setPen(QPen(align,.6));crossMark(p,{174,114});
        caption({184,104,103,21},labels.align,4);
    }
    p.setPen(QPen(datum,.35,Qt::DashLine));p.drawLine(QLineF(168,137,179,137));
    caption({184,127,103,21},showBaseline?labels.datum:labels.centreline,4);
    p.setPen(templatePen(.6,hasReference));p.drawLine(QLineF(168,160,179,160));
    caption({184,150,103,21},hasReference?labels.templateCut:labels.contour,4);
    if(hasReference){
        p.setPen(QPen(shellInk,.34));p.drawLine(QLineF(168,177,179,177));
        caption({184,167,103,21},labels.shellReference,4);
    }
    p.restore();
}
QSizeF wholePage(const Station& sheet,double divisor) {
    const auto bounds=drawingArea(sheet,divisor);
    // Leave room for the title and the 100 mm rule even on narrow end stations.
    return {std::max(297.,std::ceil(bounds.width()+20)),std::ceil(bounds.height()+42)};
}
void drawSheet(QPainter& p,const Station& sheet,const SheetLabels& labels,QSizeF paper,
               PdfLayout layout,int row,int col,Tiles tiles,int unit,double divisor,const QString& scaleLabel) {
    const bool tiled=layout!=PdfLayout::WholeStation;
    const bool edge=layout==PdfLayout::A4EdgeToEdge;
    const QRectF drawing=edge?a4Paper:QRectF(10,20,paper.width()-20,paper.height()-42);
    const QRectF header=QRectF(10,edge?1:5,paper.width()-20,10).adjusted(tiled&&col>0?10:0,0,0,0);
    p.save();
    const double px=p.device()->logicalDpiX()/25.4, py=p.device()->logicalDpiY()/25.4;
    p.scale(px/unit,py/unit);
    QFont font; font.setPixelSize(4);p.setFont(font);
    p.setPen(QPen(Qt::black,.18));
    const double rulerX=(paper.width()-100)/2;
    if(!edge||row==0){
        auto title=header;
        title.setRight(rulerX-4);
        const QFontMetricsF metrics(font,p.device());
        const auto text=labels.title(sheet);
        const double squeeze=std::min(1.,title.width()/std::max(1.,metrics.horizontalAdvance(text)+1));
        p.save();
        p.translate(title.topLeft());
        p.scale(squeeze,1);
        p.drawText(QRectF(0,0,title.width()/squeeze,title.height()),Qt::AlignLeft|Qt::AlignVCenter,text);
        p.restore();
        p.drawText(header.adjusted(0,0,edge?-15:0,0),Qt::AlignRight|Qt::AlignVCenter,scaleLabel);
    }
    const QRectF bounds=tiledArea(sheet,divisor,edge);
    const auto span=edge?a4Paper:ink;
    const double lap=edge?0.:overlap;
    const QPointF corner(bounds.left()+col*(span.width()-lap),bounds.bottom()-row*(span.height()-lap));
    if(tiled&&!edge){
        if(col>0)cutGuide(p,sheet,corner,divisor,true);
        if(row>0)cutGuide(p,sheet,corner,divisor,false);
        // Separate the two pairs from the four-way junction so trimming a
        // second edge leaves both halves of every registration pair usable.
        p.setPen(QPen(QColor(85,85,85),.16));
        for(double y:{ink.top()+15,ink.bottom()-15}){
            if(col>0)crossMark(p,{ink.left()+overlap/2,y});
            if(col+1<tiles.cols)crossMark(p,{ink.right()-overlap/2,y});
        }
        for(double x:{ink.left()+15,ink.right()-15}){
            if(row>0)crossMark(p,{x,ink.top()+overlap/2});
            if(row+1<tiles.rows)crossMark(p,{x,ink.bottom()-overlap/2});
        }
    }
    p.save();
    p.setClipRect(drawing);
    p.translate(drawing.topLeft());p.scale(1,-1);p.translate(-corner);
    p.setPen(QPen(QColor(120,120,120),.16,Qt::DashLine));
    const double top=bounds.bottom()-(edge?edgeHeader:0);
    p.drawLine(QLineF(0,bounds.top(),0,top));
    if(bounds.top()<=0 && top>=0)p.drawLine(QLineF(bounds.left(),0,bounds.right(),0));
    p.scale(1./divisor,1./divisor);
    const bool hasReference=!sheet.shellReference.isEmpty();
    p.setPen(QPen(shellInk,.18*divisor));
    for(const auto& path:sheet.shellReference)p.drawPolyline(path);
    p.setPen(templatePen(.25*divisor,hasReference));
    for(const auto& path:sheet.outlines)p.drawPolyline(path);
    for(const auto& mark:sheet.slotLabels){
        p.save();p.translate(mark.second);p.scale(divisor,-divisor);
        p.drawText(QPointF(0,0),QString::number(mark.first));p.restore();
    }
    p.restore();
    p.setPen(QPen(Qt::black,.2));
    const double rulerY=header.center().y();
    if(!edge||row==0){
        p.drawLine(QLineF(rulerX,rulerY,rulerX+100,rulerY));p.drawLine(QLineF(rulerX,rulerY-2,rulerX,rulerY+2));p.drawLine(QLineF(rulerX+100,rulerY-2,rulerX+100,rulerY+2));
        font.setPixelSize(3);p.setFont(font);
        p.drawText(QRectF(rulerX+103,rulerY-5,24,10),Qt::AlignLeft|Qt::AlignVCenter,QStringLiteral("100 mm"));
        if(tiled&&!edge)p.drawText(QRectF(205,paper.height()-18,82,10),
                                 Qt::AlignRight|Qt::AlignVCenter,tileCode(tiles,row,col));
    }
    if(edge){
        font.setPixelSize(3);p.setFont(font);p.setPen(QColor(85,85,85));
        p.drawText(QRectF(274,2,18,7),Qt::AlignRight|Qt::AlignVCenter,tileCode(tiles,row,col));
    }
    if(hasReference&&(!edge||row==0)){
        font.setPixelSize(3);p.setFont(font);
        const double y=edge?12:paper.height()-10;
        const auto label=[&](double x,const QPen& pen,const QString& name){
            p.setPen(pen);p.drawLine(QLineF(x,y,x+15,y));
            p.setPen(Qt::black);p.drawText(QRectF(x+18,y-3,72,6),Qt::AlignLeft|Qt::AlignVCenter,name);
        };
        label(10,templatePen(.32,true),labels.templateCut);
        label(101,QPen(shellInk,.18),labels.shellReference);
    }
    p.restore();
}
}
QRectF plotBounds(const Station& station) {
    double left=0,right=0,low=0,high=0;
    const auto expand=[&](const QPolygonF& path){
        auto b=path.boundingRect();
        left=std::min(left,b.left());right=std::max(right,b.right());
        low=std::min(low,b.top());high=std::max(high,b.bottom());
    };
    for(const auto& path:station.outlines)expand(path);
    for(const auto& path:station.shellReference)expand(path);
    return {QPointF(left,low),QPointF(right,high)};
}
QString SheetLabels::title(const Station& sheet) const {
    if(!sheet.slotLabels.isEmpty())return spine;
    const auto location=QStringLiteral("%1 %2     X = %3 mm").arg(station).arg(sheet.number).arg(sheet.x,0,'f',2);
    return interlocking?location:location+QStringLiteral("     ")+heading;
}
int pdfPageCount(const QList<Station>& stations,PdfScale scale,PdfLayout layout) {
    if(scale.numerator<1 || scale.denominator<1)throw std::invalid_argument("Invalid PDF scale");
    if(layout==PdfLayout::WholeStation) return int(std::min(stations.size(),qsizetype(maximumPdfPages+1)));
    const double divisor=double(scale.denominator)/scale.numerator;
    int total=0;
    QSet<QPair<int,int>> layouts;
    for(const auto& sheet:stations) {
        const auto tiles=tilesFor(sheet,divisor,layout==PdfLayout::A4EdgeToEdge);
        total+=tiles.rows*tiles.cols+int(firstAssemblyGuide(tiles,layouts));
        if(total>maximumPdfPages) return maximumPdfPages+1;
    }
    return total;
}
void writePdf(const QString& path,const QList<Station>& stations,const SheetLabels& labels,PdfLayout layout,PdfScale scale,const std::atomic_bool& cancelled) {
    ensureSheets(stations);
    const bool tiled=layout!=PdfLayout::WholeStation;
    const bool edge=layout==PdfLayout::A4EdgeToEdge;
    const int pageCount=pdfPageCount(stations,scale,layout);
    if(pageCount>maximumPdfPages)
        throw std::runtime_error("PDF 超过 500 页，请减少所选站位。");
    const double divisor=double(scale.denominator)/scale.numerator;
    const QString scaleLabel=QStringLiteral("%1:%2").arg(scale.numerator).arg(scale.denominator);
    int unit=1;
    if(!tiled) for(const auto& station:stations) {
        const auto paper=wholePage(station,divisor);
        const double needed=std::ceil(std::max(paper.width(),paper.height())/5080.);
        if(!std::isfinite(needed) || needed>75000) throw std::runtime_error("站位超出 PDF 的纸张尺寸范围。");
        unit=std::max(unit,int(needed));
    }
    QSaveFile file(path);
    openTarget(file);
    const bool precisePageSize=unit>1||edge;
    QTemporaryFile large(path+".XXXXXX");
    if(precisePageSize && !large.open()) throw std::runtime_error(large.errorString().toStdString());
    {
        QPdfWriter pdf(precisePageSize?static_cast<QIODevice*>(&large):static_cast<QIODevice*>(&file));
        if(unit>1) pdf.setPdfVersion(QPagedPaintDevice::PdfVersion_1_6);
        pdf.setTitle(labels.heading);pdf.setCreator("FreeShip 26");pdf.setResolution(254);
        QPainter painter;
        const auto beginPage=[&]{
            checkCancelled(cancelled);
            if(!painter.isActive()){
                if(!painter.begin(&pdf))throw std::runtime_error("无法创建 PDF。");
            }else if(!pdf.newPage())throw std::runtime_error("无法写入 PDF 页面。");
        };
        QSet<QPair<int,int>> layouts;
        for(const auto& station:stations) {
            QSizeF paper=tiled?QSizeF(297,210):wholePage(station,divisor);
            const auto size=tiled?QPageSize(QPageSize::A4):QPageSize(paper/unit,QPageSize::Millimeter,{},QPageSize::ExactMatch);
            const auto orientation=tiled?QPageLayout::Landscape:QPageLayout::Portrait;
            if(!size.isValid() || !pdf.setPageLayout(QPageLayout(size,orientation,QMarginsF(),QPageLayout::Millimeter)))
                throw std::runtime_error("无法设置 PDF 纸张尺寸。");
            if(!tiled) paper=QSizeF(size.sizePoints())*(25.4*unit/72.);
            const auto tiles=tiled?tilesFor(station,divisor,edge):Tiles{1,1};
            if(firstAssemblyGuide(tiles,layouts)){
                beginPage();
                drawAssemblyGuide(painter,station,tiles,divisor,labels,edge);
            }
            for(int row=0;row<tiles.rows;++row) for(int col=0;col<tiles.cols;++col) {
                beginPage();
                drawSheet(painter,station,labels,paper,layout,row,col,tiles,unit,divisor,scaleLabel);
            }
        }
        if(!painter.end()) throw std::runtime_error("未能完成 PDF。");
    }
    if(precisePageSize) {
        finishStationPdfPages(large,unit,edge,pageCount,cancelled);
        if(!large.seek(0)) throw std::runtime_error(large.errorString().toStdString());
        char chunk[65536];
        while(!large.atEnd()) {
            checkCancelled(cancelled);
            auto bytes=large.read(chunk,sizeof(chunk));
            if(bytes<=0 || file.write(chunk,bytes)!=bytes) throw std::runtime_error("无法保存大幅面 PDF。");
        }
    }
    checkCancelled(cancelled);
    if(!file.commit()) throw std::runtime_error(file.errorString().toStdString());
}
void writeDxf(const QString& path,const QList<Station>& stations,double thickness,const std::atomic_bool& cancelled) {
    ensureSheets(stations);
    QSaveFile file(path);
    openTarget(file);
    QTextStream s(&file);s.setLocale(QLocale::c());s.setRealNumberPrecision(16);
    // R2000 symbol tables and entities have explicit handles and owners.
    // Model and paper space share the block-record table, with separate owners.
    const bool hasReference=std::any_of(stations.cbegin(),stations.cend(),
        [](const Station& station){return !station.shellReference.isEmpty();});
    const QString refLayer=QStringLiteral("HULL_REFERENCE");
    unsigned nextHandle=0x20;
    qint64 seed=nextHandle+10+4*stations.size()+(hasReference?2:0);
    for(const auto& station:stations)seed+=station.outlines.size()+station.shellReference.size()+station.slotLabels.size();
    s<<"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n9\n$HANDSEED\n5\n"<<QString::number(seed,16).toUpper()<<"\n0\nENDSEC\n";
    const auto handle=[&]{return QString::number(nextHandle++,16).toUpper();};
    s<<"0\nSECTION\n2\nTABLES\n";
    auto table=[&](const char* name,const char* id,int count){
        s<<"0\nTABLE\n2\n"<<name<<"\n5\n"<<id<<"\n330\n0\n100\nAcDbSymbolTable\n70\n"<<count<<'\n';
    };
    table("LTYPE","2",hasReference?4:3);
    for(const char* name:{"BYBLOCK","BYLAYER","CONTINUOUS"})
        s<<"0\nLTYPE\n5\n"<<handle()<<"\n330\n2\n100\nAcDbSymbolTableRecord\n100\nAcDbLinetypeTableRecord\n2\n"<<name<<"\n70\n0\n3\n\n72\n65\n73\n0\n40\n0\n";
    if(hasReference)
        s<<"0\nLTYPE\n5\n"<<handle()<<"\n330\n2\n100\nAcDbSymbolTableRecord\n100\nAcDbLinetypeTableRecord\n2\nTEMPLATE_DASH\n70\n0\n3\nTemplate cut line\n72\n65\n73\n2\n40\n4.5\n49\n3\n74\n0\n49\n-1.5\n74\n0\n";
    s<<"0\nENDTAB\n";
    table("LAYER","1",int(stations.size())+3+int(hasReference));
    auto layer=[&](const QString& name,int color,const char* linetype="CONTINUOUS",bool plot=true){
        s<<"0\nLAYER\n5\n"<<handle()<<"\n330\n1\n100\nAcDbSymbolTableRecord\n100\nAcDbLayerTableRecord\n2\n"<<name<<"\n70\n0\n62\n"<<color<<"\n6\n"<<linetype<<'\n';
        if(!plot)s<<"290\n0\n";
    };
    layer("0",7);layer("DATUM",8);layer("LABEL",7);
    auto contourLayer=[&](int number){if(number==0)return QStringLiteral("SPINE");return QString(thickness>0?"MOULD_%1":"OUTER_%1").arg(number);};
    for(const auto& sheet:stations)
        layer(contourLayer(sheet.number),7,sheet.shellReference.isEmpty()?"CONTINUOUS":"TEMPLATE_DASH");
    if(hasReference)layer(refLayer,-8,"CONTINUOUS",false);
    s<<"0\nENDTAB\n";
    table("BLOCK_RECORD","3",2);
    struct Space {const char* id; const char* name;};
    const Space spaces[]={{"10","*Model_Space"},{"11","*Paper_Space"}};
    for(const auto& space:spaces)
        s<<"0\nBLOCK_RECORD\n5\n"<<space.id<<"\n330\n3\n100\nAcDbSymbolTableRecord\n100\nAcDbBlockTableRecord\n2\n"<<space.name<<'\n';
    s<<"0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nBLOCKS\n";
    for(const auto& space:spaces){
        s<<"0\nBLOCK\n5\n"<<handle()<<"\n330\n"<<space.id<<"\n100\nAcDbEntity\n8\n0\n100\nAcDbBlockBegin\n2\n"<<space.name<<"\n70\n0\n10\n0\n20\n0\n30\n0\n3\n"<<space.name<<"\n1\n\n";
        s<<"0\nENDBLK\n5\n"<<handle()<<"\n330\n"<<space.id<<"\n100\nAcDbEntity\n8\n0\n100\nAcDbBlockEnd\n";
    }
    s<<"0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    auto entity=[&](const char* kind,const QString& group){
        s<<"0\n"<<kind<<"\n5\n"<<handle()<<"\n330\n10\n100\nAcDbEntity\n8\n"<<group<<'\n';
    };
    auto datum=[&](double x1,double y1,double x2,double y2){
        entity("LINE","DATUM");s<<"100\nAcDbLine\n10\n"<<x1<<"\n20\n"<<y1<<"\n30\n0\n11\n"<<x2<<"\n21\n"<<y2<<"\n31\n0\n";
    };
    double cursor=0;
    for(const auto& sheet:stations) {
        checkCancelled(cancelled);
        const QRectF b=plotBounds(sheet);
        const double dx=cursor-b.left();
        const auto polyline=[&](const QPolygonF& outline,const QString& group) {
            const int count=int(outline.size())-(outline.isClosed()?1:0);
            entity("LWPOLYLINE",group);
            const int flags=(outline.isClosed()?1:0)|(group==refLayer||sheet.shellReference.isEmpty()?0:128);
            s<<"100\nAcDbPolyline\n90\n"<<count<<"\n70\n"<<flags<<'\n';
            for(int i=0;i<count;++i) s<<"10\n"<<outline[i].x()+dx<<"\n20\n"<<outline[i].y()<<'\n';
        };
        const auto group=contourLayer(sheet.number);
        for(const auto& outline:sheet.outlines)polyline(outline,group);
        for(const auto& outline:sheet.shellReference)polyline(outline,refLayer);
        datum(dx,b.top()-4,dx,b.bottom()+4);datum(cursor-4,0,cursor+b.width()+4,0);
        for(const auto& mark:sheet.slotLabels){
            entity("TEXT","LABEL");s<<"100\nAcDbText\n10\n"<<dx+mark.second.x()<<"\n20\n"<<mark.second.y()
                <<"\n30\n0\n40\n3\n1\nST "<<mark.first<<"\n100\nAcDbText\n";
        }
        const auto label=sheet.number==0?QStringLiteral("CENTRE SPINE"):QString("ST %1   X=%2 mm   %3").arg(sheet.number).arg(sheet.x,0,'f',2)
                .arg(thickness>0?QString("MOULD  t=%1 mm").arg(thickness):QString("OUTER"));
        entity("TEXT","LABEL");s<<"100\nAcDbText\n10\n"<<cursor<<"\n20\n"<<b.top()-12
         <<"\n30\n0\n40\n3\n1\n"<<label<<"\n100\nAcDbText\n";
        cursor+=std::max(b.width(),double(label.size())*3)+30;
    }
    s<<"0\nENDSEC\n0\nEOF\n";s.flush();
    checkCancelled(cancelled);
    if(s.status()!=QTextStream::Ok || !file.commit()) throw std::runtime_error("DXF 写入失败。");
}
}

