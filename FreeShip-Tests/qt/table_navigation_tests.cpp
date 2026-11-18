#include "app/studio_ui.h"
#include "app/studio_table.h"
#include "app/studio_numeric.h"
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include "app/math_notation.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QFontMetricsF>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
void settle(){QEventLoop loop;QTimer::singleShot(25,&loop,&QEventLoop::quit);loop.exec();}
void wheel(QTableView& table,int amount,QPoint position={}){
    if(position.isNull())position=table.viewport()->rect().center();
    QWheelEvent event(position,table.viewport()->mapToGlobal(position),{},QPoint(0,amount),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(table.viewport(),&event);settle();
}
QByteArray contents(QAbstractItemModel& model){
    QJsonArray rows;for(int row=0;row<model.rowCount();++row){QJsonArray cells;for(int col=0;col<model.columnCount();++col)cells.append(model.index(row,col).data().toString());rows.append(cells);}
    return QJsonDocument(rows).toJson(QJsonDocument::Compact);
}
double widthOf(const QString& text,QFont font){return notation::contains(text)?notation::size(text,font).width():QFontMetricsF(font).horizontalAdvance(text);}
int headerInkHeight(QTableView& table){
    const auto* header=table.horizontalHeader();const int x=header->sectionViewportPosition(0),width=header->sectionSize(0);
    const auto image=header->viewport()->grab(QRect(x+1,1,width-2,header->height()-4)).toImage();
    int top=image.height(),bottom=-1;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
        const auto color=image.pixelColor(x,y);
        if(color.red()<130&&color.green()<150&&color.blue()<150){top=std::min(top,y);bottom=std::max(bottom,y);}
    }
    return bottom<top?0:bottom-top+1;
}
QJsonObject headerGlyphCoverage(QTableView& table,const QString& out){
    const auto* header=table.horizontalHeader();const auto actual=header->viewport()->grab().toImage();
    QImage expected(actual.size(),QImage::Format_ARGB32_Premultiplied);expected.setDevicePixelRatio(actual.devicePixelRatio());
    // Compare glyphs on the same opaque surface. A transparent oracle retains
    // dark RGB even in partly covered antialiased pixels, whereas the actual
    // header blends those pixels with its light background.
    QHash<QRgb,int> backgroundColors;
    for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width();++x)++backgroundColors[actual.pixel(x,y)];
    QRgb background=0;int mostCommon=0;
    for(auto it=backgroundColors.cbegin();it!=backgroundColors.cend();++it)if(it.value()>mostCommon){background=it.key();mostCommon=it.value();}
    expected.fill(background);
    QPainter painter(&expected);painter.setFont(header->font());painter.setPen(header->palette().color(QPalette::ButtonText));
    const int pad=qMax(1,qRound(6*table.property("studio_table_scale").toDouble()));
    for(int column=0;column<table.model()->columnCount();++column){
        const auto text=table.model()->headerData(column,Qt::Horizontal).toString();if(notation::contains(text))continue;
        const QRect rectangle(header->sectionViewportPosition(column)+pad,0,header->sectionSize(column)-2*pad,header->viewport()->height());
        // No clipping in the oracle: every glyph of the original label must
        // survive in the actual fitted header, including final punctuation.
        painter.drawText(rectangle,header->defaultAlignment()|Qt::TextDontClip,text);
    }
    painter.end();int wanted=0,missing=0;QJsonArray columns;
    const auto ink=[](QColor color){return color.alpha()>128&&color.red()<130&&color.green()<150&&color.blue()<150;};
    for(int y=0;y<expected.height();++y)for(int x=0;x<expected.width();++x)if(ink(expected.pixelColor(x,y))){++wanted;if(!ink(actual.pixelColor(x,y)))++missing;}
    for(int column=0;column<table.model()->columnCount();++column){
        if(notation::contains(table.model()->headerData(column,Qt::Horizontal).toString()))continue;
        int a=0,b=0;
        const int left=qRound(header->sectionViewportPosition(column)*actual.devicePixelRatio());
        const int right=qMin(actual.width(),qRound((header->sectionViewportPosition(column)+header->sectionSize(column))*actual.devicePixelRatio()));
        for(int y=0;y<actual.height();++y)for(int x=qMax(0,left);x<right;++x){a+=ink(actual.pixelColor(x,y));b+=ink(expected.pixelColor(x,y));}
        columns.append(QJsonObject{{"column",column},{"actual_ink",a},{"expected_ink",b}});
    }
    actual.scaled(actual.size()*5).save(out+"/header-actual.png");expected.scaled(expected.size()*5).save(out+"/header-expected.png");
    return {{"expected_ink",wanted},{"missing_ink",missing},{"columns",columns}};
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);applyStudioTheme();
    const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
    const auto check=[&](QString id,bool pass,QJsonObject observation=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"},{"observed",observation}});};
    // Production Studio window policy must compact numeric forms without
    // changing numerical precision, editable ranges or free text sizing.
    QDialog formWindow;auto* form=new QFormLayout(&formWindow);
    QDoubleSpinBox first,second;QLineEdit path("D:/中文路径/Женя.fbm");
    for(auto* spin:{&first,&second}){spin->setDecimals(6);spin->setRange(-1e12,1e12);}
    first.setValue(188.62793);second.setValue(1000);
    form->addRow("Loa (m)",&first);form->addRow("AL (m²)",&second);form->addRow("Path",&path);
    formWindow.resize(900,260);formWindow.show();settle();
    const int fitted=first.width();
    check("numeric-form-measured-instead-of-stretched",fitted==second.width()&&fitted<300&&fitted>QFontMetricsF(first.font()).horizontalAdvance(second.text()),{{"width",fitted},{"text",second.text()}});
    check("free-text-and-path-retain-expanding-space",path.width()>fitted*2);
    second.setValue(-987654321.123456);settle();
    check("longer-signed-values-expand-the-entire-group",first.width()==second.width()&&second.width()>fitted&&second.width()>QFontMetricsF(second.font()).horizontalAdvance(second.text()),{{"before",fitted},{"first",first.width()},{"second",second.width()},{"text",second.text()}});
    check("numeric-sizing-does-not-alter-value-or-range",std::abs(second.value()+987654321.123456)<.000001&&second.minimum()==-1e12&&second.maximum()==1e12&&second.decimals()==6);
    QFont larger=formWindow.font();larger.setPointSizeF(larger.pointSizeF()*1.5);formWindow.setFont(larger);settle();
    check("numeric-width-remeasures-on-font-change",first.width()==second.width()&&second.width()>QFontMetricsF(second.font()).horizontalAdvance(second.text()));
    formWindow.grab().save(out+"/numeric-form.png");formWindow.hide();
    QStandardItemModel model(48,9);
    const QStringList headers{"X","吃水 = 2.17 m (m²)","横剖面面积","Площадь ватерлинии (m²)","C_b","Layer","Z (m)","Volume (m³)","Weight (t)"};
    model.setHorizontalHeaderLabels(headers);
    for(int row=0;row<model.rowCount();++row)for(int col=0;col<model.columnCount();++col)
        model.setData(model.index(row,col),QString::number((row+1)*(col+1)*1.234,'f',3));
    model.setData(model.index(47,5),"最长内容位于最后一行 / Длинное имя слоя");
    const auto original=contents(model);int mutations=0;
    QObject::connect(&model,&QAbstractItemModel::dataChanged,&app,[&]{++mutations;});
    QTableView table;table.setModel(&model);table.setEditTriggers(QAbstractItemView::NoEditTriggers);table.verticalHeader()->hide();
    table.resize(760,500);configureStudioDataTable(&table);table.show();settle();
    check("shared-controller-installed",table.property("studio_table_navigation").toBool());
    QJsonArray measured;
    bool exact=true;
    for(int col=0;col<model.columnCount();++col){
        double longest=widthOf(headers[col],table.horizontalHeader()->font());
        for(int row=0;row<model.rowCount();++row)longest=std::max(longest,widthOf(model.index(row,col).data().toString(),table.font()));
        const int actual=table.columnWidth(col);const int target=int(std::ceil(longest))+14;
        exact&=actual>=target&&actual<=target+1;
        measured.append(QJsonObject{{"column",col},{"width",actual},{"content",longest},{"target",target}});
    }
    check("every-column-includes-header-and-all-rows-with-small-padding",exact,{{"columns",measured}});
    check("short-column-has-no-112px-floor",table.columnWidth(0)<112,{{"width",table.columnWidth(0)}});
    check("last-row-longest-string-is-not-clipped",table.columnWidth(5)>QFontMetricsF(table.font()).horizontalAdvance(model.index(47,5).data().toString()));
    table.verticalScrollBar()->setValue(0);
    const QPointF wheelPoint=table.viewport()->rect().center();
    QWheelEvent scrollEvent(wheelPoint,table.viewport()->mapToGlobal(wheelPoint.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    const double unmodifiedScale=table.property("studio_table_scale").toDouble();
    QApplication::sendEvent(table.viewport(),&scrollEvent);settle();
    check("plain-wheel-scrolls-vertically-without-zoom",table.verticalScrollBar()->value()>0&&table.property("studio_table_scale").toDouble()==unmodifiedScale);
    const double pointSize=table.font().pointSizeF();const int firstWidth=table.columnWidth(0);
    const double headerPointSize=table.horizontalHeader()->font().pointSizeF();const int initialInk=headerInkHeight(table);
    wheel(table,120);
    check("wheel-zooms-native-font-and-column-layout",table.property("studio_table_scale").toDouble()>1&&table.font().pointSizeF()>pointSize&&table.columnWidth(0)>firstWidth);
    check("header-and-cell-font-scale-together",std::abs(table.font().pointSizeF()/pointSize-table.property("studio_table_scale").toDouble())<.001);
    table.grab().save(out+"/zoomed.png");
    wheel(table,-120);check("reverse-wheel-returns-to-native-size",std::abs(table.font().pointSizeF()-pointSize)<.001);
    for(int i=0;i<50;++i)wheel(table,-120);
    const double minimum=table.property("studio_table_scale").toDouble();
    const int smallInk=headerInkHeight(table);
    check("header-font-and-rendered-ink-scale-down",table.horizontalHeader()->font().pointSizeF()<headerPointSize&&smallInk>0&&smallInk<=initialInk*minimum+3,
          {{"initial_ink_pixels",initialInk},{"fit_ink_pixels",smallInk},{"scale",minimum},{"header_point_size",table.horizontalHeader()->font().pointSizeF()}});
    const auto glyphs=headerGlyphCoverage(table,out);
    check("complete-header-glyphs-survive-fit-zoom",glyphs["expected_ink"].toInt()>0&&glyphs["missing_ink"].toInt()<=2,glyphs);
    check("zoom-out-stops-with-entire-table-visible",table.horizontalScrollBar()->maximum()==0&&table.verticalScrollBar()->maximum()==0,
          {{"scale",minimum},{"horizontal",table.horizontalScrollBar()->maximum()},{"vertical",table.verticalScrollBar()->maximum()}});
    wheel(table,-120);check("further-zoom-out-at-fit-is-a-noop",std::abs(table.property("studio_table_scale").toDouble()-minimum)<.00001);
    table.grab().save(out+"/fit.png");
    for(int i=0;i<50;++i)wheel(table,120);
    const double maximum=table.property("studio_table_scale").toDouble();
    wheel(table,120);check("maximum-zoom-is-bounded",maximum<=2.5&&table.property("studio_table_scale").toDouble()==maximum);
    table.horizontalScrollBar()->setValue(table.horizontalScrollBar()->maximum()/2);
    table.verticalScrollBar()->setValue(table.verticalScrollBar()->maximum()/2);
    const QPoint before(table.horizontalScrollBar()->value(),table.verticalScrollBar()->value());
    const QPoint begin=table.viewport()->rect().center(),end=begin-QPoint(36,27);
    QMouseEvent press(QEvent::MouseButtonPress,begin,table.viewport()->mapToGlobal(begin),Qt::RightButton,Qt::RightButton,Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove,end,table.viewport()->mapToGlobal(end),Qt::NoButton,Qt::RightButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,end,table.viewport()->mapToGlobal(end),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(table.viewport(),&press);QApplication::sendEvent(table.viewport(),&move);QApplication::sendEvent(table.viewport(),&release);settle();
    check("right-drag-pans-both-axes-pixel-for-pixel",table.horizontalScrollBar()->value()-before.x()==36&&table.verticalScrollBar()->value()-before.y()==27,
          {{"dx",table.horizontalScrollBar()->value()-before.x()},{"dy",table.verticalScrollBar()->value()-before.y()}});
    check("right-drag-releases-hand-cursor",table.viewport()->cursor().shape()!=Qt::ClosedHandCursor);
    check("gestures-do-not-edit-data",contents(model)==original&&mutations==0);
    model.setHeaderData(0,Qt::Horizontal,"Новое длинное название столбца");settle();
    check("header-language-change-remeasures-column",table.columnWidth(0)>=widthOf("Новое длинное название столбца",table.horizontalHeader()->font())+8);
    model.setData(model.index(0,0),"NEW LONGEST VALUE / 新增的更长内容 / Наибольшее значение");settle();
    check("updated-data-remeasures-column",table.columnWidth(0)>=widthOf(model.index(0,0).data().toString(),table.font())+8);
    table.hide();
    QStandardItemModel smallModel(2,2);smallModel.setHorizontalHeaderLabels({"X","Y"});
    smallModel.setData(smallModel.index(0,0),"12.345");
    QTableView small;small.setModel(&smallModel);small.resize(760,500);configureStudioDataTable(&small);small.show();settle();
    wheel(small,-120);check("already-fitting-table-does-not-shrink",small.property("studio_table_scale").toDouble()==1);
    wheel(small,120);check("already-fitting-table-can-still-enlarge",small.property("studio_table_scale").toDouble()>1);
    small.grab().save(out+"/compact.png");
    int failures=0;for(const auto& item:cases)if(item.toObject()["status"]=="fail")++failures;
    QFile report(out+"/report.json");report.open(QIODevice::WriteOnly);
    report.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases},{"artifacts",QJsonObject{{"device_pixel_ratio",table.devicePixelRatioF()}}}}).toJson());
    return failures?1:0;
}
