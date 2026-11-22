#include "lines_pdf.h"
#include "lines_reference.h"
#include "pdf_scale_inputs.h"
#include "scene.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QPushButton>
#include <algorithm>
#include <cmath>

namespace {
constexpr double pointsPerMm=72./25.4;
constexpr double margin=10*pointsPerMm;
constexpr double footerHeight=14;
struct Label { QPointF anchor; QString text; QColor ink; QSizeF size; bool numeric; };
struct Sheet {
    QRectF bounds;
    QSizeF labelSpace{0,0};
    QVector<Label> labels;
    QFont font;

    explicit Sheet(const Scene& scene):bounds(scene.bounds){
        font.setFamily("Arial");font.setPixelSize(9);
        const QFontMetricsF metrics(font);
        auto include=[&](QPointF point){
            bounds.setCoords(std::min(bounds.left(),point.x()),std::min(bounds.top(),point.y()),
                             std::max(bounds.right(),point.x()),std::max(bounds.bottom(),point.y()));
        };
        for(const auto& batch:scene.batches){include(batch.bounds.topLeft());include(batch.bounds.bottomRight());}
        for(int index:scene.labels){
            const auto& item=scene.primitives[index];
            const auto text=linesReferenceText(item.text,uiText("基线 %1"),uiText("中心线"));
            if(text.isEmpty())continue;
            include(item.points[0]);
            const QSizeF extent(metrics.horizontalAdvance(text)+2,metrics.height()+2);
            labelSpace=labelSpace.expandedTo(extent);
            labels.append({item.points[0],text,legacyColor(item.color),extent,
                           text.front().isDigit()||text.front()=='-'||text.front()=='+'});
        }
    }
    double fit(QSizeF paper)const{
        return std::min((paper.width()-2*margin-labelSpace.width())/bounds.width(),
                        (paper.height()-2*margin-footerHeight-labelSpace.height())/bounds.height());
    }
    void paint(QPainter& p,const Scene& scene,QSizeF paper,double scale,bool fill,const QString& caption)const{
        const QPointF center((paper.width()-labelSpace.width())/2,(paper.height()-footerHeight-labelSpace.height())/2);
        const QTransform project(scale,0,0,-scale,center.x()-scale*bounds.center().x(),center.y()+scale*bounds.center().y());
        p.save();p.setTransform(project,true);
        for(const auto& batch:scene.batches){
            if(batch.fill&&!fill)continue;
            if(batch.fill){p.setPen(Qt::NoPen);p.setBrush(batch.color);}
            else {p.setBrush(Qt::NoBrush);p.setPen(QPen(batch.color,.5/scale));}
            p.drawPath(batch.path);
        }
        p.restore();p.setFont(font);
        QVector<QRectF> occupied;
        for(bool numeric:{false,true})for(const auto& label:labels){
            if(label.numeric!=numeric)continue;
            const QRectF box(project.map(label.anchor),label.size);
            if(std::any_of(occupied.cbegin(),occupied.cend(),[&](const QRectF& other){return box.intersects(other);}))continue;
            occupied.append(box);p.setPen(label.ink);
            p.drawText(box,Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,label.text);
        }
        p.setPen(Qt::black);
        p.drawText(QRectF(margin,paper.height()-margin-footerHeight,paper.width()-2*margin,footerHeight),
                   Qt::AlignRight|Qt::AlignVCenter,caption);
    }
};
}

QString exportLinesPdf(QWidget* parent,const Scene& scene,bool imperial,bool fill,const QString& suggestedPath){
    const Sheet sheet(scene);
    const double pointsPerUnit=(imperial?304.8:1000.)*pointsPerMm;
    QDialog options(parent);options.setObjectName("LinesPdfSettings");
    options.setWindowIcon(studioFormatWindowIcon("PDF"));options.setProperty("studio_window_icon","format:PDF");
    options.setWindowTitle(QString(uiText("型线图"))+" · PDF");
    QFormLayout form(&options);
    QComboBox paper;
    PdfScaleInputs scale(QStringLiteral("LinesPdf"));
    paper.setObjectName("LinesPdfPaper");
    for(auto id:{QPageSize::A4,QPageSize::A3,QPageSize::A2,QPageSize::A1,QPageSize::A0}){
        QPageSize page(id);auto mm=page.size(QPageSize::Millimeter).transposed();
        paper.addItem(QString("%1  (%2 × %3 mm)").arg(page.key()).arg(mm.width()).arg(mm.height()),int(id));
    }
    form.addRow(uiText("页面设置"),&paper);form.addRow(uiText("出图比例"),&scale);
    auto pageLayout=[&]{return QPageLayout(QPageSize(QPageSize::PageSizeId(paper.currentData().toInt())),
                                           QPageLayout::Landscape,QMarginsF(),QPageLayout::Point);};
    const auto pageFit=[&]{return sheet.fit(pageLayout().fullRect(QPageLayout::Point).size());};
    const auto factor=[&]{return pointsPerUnit*scale.numerator.value()/scale.denominator.value();};
    const auto defaultDenominator=[&]{
        const double fit=pageFit();
        return fit>0?int(std::clamp(std::ceil(pointsPerUnit/fit),1.,double(PdfScaleInputs::maximum)))
                    :PdfScaleInputs::maximum;
    };
    int suggestedDenominator=defaultDenominator();
    scale.denominator.setValue(suggestedDenominator);
    StudioMessage scaleWarning(StudioMessageRole::Warning);
    scaleWarning.setMessage(uiText("当前比例超出纸张范围。"),StudioMessageRole::Warning);
    form.addRow(&scaleWarning);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    const auto validate=[&]{
        const bool fits=factor()<=pageFit();
        form.setRowVisible(&scaleWarning,!fits);
        buttons.button(QDialogButtonBox::Ok)->setEnabled(fits);
    };
    QObject::connect(&paper,&QComboBox::currentIndexChanged,&options,[&]{
        const bool usingDefault=scale.numerator.value()==1&&scale.denominator.value()==suggestedDenominator;
        suggestedDenominator=defaultDenominator();
        if(usingDefault)scale.denominator.setValue(suggestedDenominator);
        validate();
    });
    QObject::connect(&scale.numerator,&QSpinBox::valueChanged,&options,validate);
    QObject::connect(&scale.denominator,&QSpinBox::valueChanged,&options,validate);
    QObject::connect(&buttons,&QDialogButtonBox::accepted,&options,&QDialog::accept);
    QObject::connect(&buttons,&QDialogButtonBox::rejected,&options,&QDialog::reject);
    validate();
    if(runStudioDialog(options)!=QDialog::Accepted)return {};
    auto path=studioSaveFile(parent,options.windowTitle(),suggestedPath,"PDF (*.pdf)");
    if(path.isEmpty())return {};
    if(QFileInfo(path).suffix().isEmpty())path+=".pdf";
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return QString(uiText("无法写入文件："))+file.errorString();
    {
        QPdfWriter pdf(&file);pdf.setResolution(720);
        const auto page=pageLayout();
        if(!pdf.setPageLayout(page))return uiText("无法设置 PDF 纸张尺寸。");
        const auto size=page.fullRect(QPageLayout::Point).size();
        const double drawingScale=factor();
        if(!(drawingScale<=sheet.fit(size)))return uiText("当前比例超出纸张范围。");
        const auto caption=QStringLiteral("%1 : %2").arg(scale.numerator.value()).arg(scale.denominator.value());
        QPainter painter;
        if(!painter.begin(&pdf))return uiText("无法写入 PDF 页面。");
        painter.scale(pdf.resolution()/72.,pdf.resolution()/72.);
        sheet.paint(painter,scene,size,drawingScale,fill,caption);
        if(!painter.end())return uiText("无法写入 PDF 页面。");
    }
    if(!file.commit())return QString(uiText("无法写入文件："))+file.errorString();
    return {};
}
