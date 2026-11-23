// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_ui.h"
#include "studio_motion.h"
#include "ui_language.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVariantAnimation>
#include <cmath>
#include <functional>

// Draw in the export renderer's page coordinates. Only the display transform
// changes here; pagination, margins and geometry belong to the export path.
class PdfPagePreview final : public QWidget {
public:
    explicit PdfPagePreview(QWidget* parent=nullptr):QWidget(parent) {
        setObjectName("PdfFirstPagePreview");
        setMinimumSize(310,260);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        animation_.setDuration(320);
        animation_.setStartValue(0.);animation_.setEndValue(1.);
        animation_.setEasingCurve(QEasingCurve::InOutCubic);
        connect(&animation_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){
            progress_=value.toDouble();update();
        });
        connect(&animation_,&QVariantAnimation::finished,this,[this]{previous_=QPixmap();update();});
    }
    using Paint=std::function<void(QPainter&,QSizeF,double)>;
    void showPage(const QSizeF& paper,double factor,int pageKind,Paint paint) {
        if(!paper.isValid()||paper.isEmpty()||!std::isfinite(factor)||factor<=0)return;
        const bool first=!paint_;
        const bool changedKind=!first&&pageKind!=kind_;
        const bool same=!first&&paper==targetPaper_&&factor==targetFactor_&&!changedKind;
        if(same){paint_=std::move(paint);update();return;}
        QPixmap snapshot;
        if(changedKind&&isVisible())snapshot=grab();
        startPaper_=first?paper:currentPaper();startFactor_=first?factor:currentFactor();
        targetPaper_=paper;targetFactor_=factor;kind_=pageKind;paint_=std::move(paint);
        animation_.stop();previous_=std::move(snapshot);progress_=1.;
        if(!first&&isVisible()&&studio_motion::enabled())animation_.start();
        else previous_=QPixmap();
        update();
    }
    void clearPage() {animation_.stop();previous_=QPixmap();paint_={};update();}
    double currentFactor() const {return startFactor_*std::pow(targetFactor_/startFactor_,progress_);}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);
        p.setPen(QColor("#d8e2dc"));p.setBrush(QColor("#edf2ef"));
        p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),8,8);
        if(!paint_)return;
        const QSizeF paper=currentPaper();
        const QRectF area=QRectF(rect()).adjusted(18,18,-18,-18);
        const double scale=qMin(area.width()/paper.width(),area.height()/paper.height());
        const QSizeF size=paper*scale;
        const QRectF page(area.center()-QPointF(size.width()/2,size.height()/2),size);
        p.fillRect(page.translated(2,3),QColor(32,58,46,27));p.fillRect(page,Qt::white);
        p.save();p.setClipRect(page);p.translate(page.topLeft());p.scale(scale,scale);
        paint_(p,paper,currentFactor());p.restore();
        p.setBrush(Qt::NoBrush);p.setPen(QColor("#d3ddd7"));p.drawRect(page);
        if(!previous_.isNull()){p.setOpacity(1.-progress_);p.drawPixmap(QPoint(),previous_);}
    }
private:
    QSizeF currentPaper() const {return startPaper_*(1.-progress_)+targetPaper_*progress_;}
    QVariantAnimation animation_;
    QPixmap previous_;
    QSizeF startPaper_,targetPaper_;
    double startFactor_=1.,targetFactor_=1.,progress_=1.;
    int kind_=0;
    Paint paint_;
};

class PdfExportDialog final : public QDialog {
public:
    explicit PdfExportDialog(QWidget* parent):QDialog(parent),preview(this),
        fit(uiText("适合单页"),this),buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this) {
        setWindowIcon(studioFormatWindowIcon("PDF"));setProperty("studio_window_icon","format:PDF");
        resize(840,530);
        auto* box=new QVBoxLayout(this);
        auto* columns=new QHBoxLayout;box->addLayout(columns,1);
        auto* settings=new QVBoxLayout;columns->addLayout(settings);
        pageOptions.setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        scaleOptions.setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        settings->addLayout(&pageOptions);
        divider=new QFrame(this);divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet("color:#d8e2dc;");settings->addWidget(divider);
        settings->addLayout(&scaleOptions);settings->addStretch();
        columns->setSpacing(20);
        auto* right=previewColumn_=new QVBoxLayout;columns->addLayout(right,1);
        auto* head=new QHBoxLayout;right->addLayout(head);
        auto* heading=new QLabel(uiText("第一页预览"),this);
        heading->setProperty("studio_content_role","section_heading");head->addWidget(heading);head->addStretch();
        pageDetail=new QLabel(this);pageDetail->setTextFormat(Qt::PlainText);head->addWidget(pageDetail);
        right->addWidget(&preview,1);
        box->addWidget(&buttons);
        buttons.button(QDialogButtonBox::Ok)->setText(uiText("确定"));
        buttons.button(QDialogButtonBox::Cancel)->setText(uiText("取消"));
        fit.setObjectName("PdfFitSinglePage");
        connect(&buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);
        connect(&buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    }
    void addScale(QWidget* fields,bool showFit=true) {
        divider->setVisible(pageOptions.rowCount()>0);
        if(showFit)scaleOptions.addRow(&fit);
        scaleOptions.addRow(uiText("出图比例"),fields);
    }
    void setPageDimensions(const QSizeF& millimetres) {
        pageDetail->setText(millimetres.isValid()?QStringLiteral("%1 × %2 mm")
            .arg(locale().toString(millimetres.width(),'f',1),locale().toString(millimetres.height(),'f',1)):QString());
    }
    void addPreviewFooter(QWidget* widget){previewColumn_->addWidget(widget);}
    QPushButton* confirm() const {return buttons.button(QDialogButtonBox::Ok);}
    QFormLayout pageOptions,scaleOptions;
    PdfPagePreview preview;
    QCheckBox fit;
    QDialogButtonBox buttons;
private:
    QVBoxLayout* previewColumn_;
    QFrame* divider;
    QLabel* pageDetail;
};
