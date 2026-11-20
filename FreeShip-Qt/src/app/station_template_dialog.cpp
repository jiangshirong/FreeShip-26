// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_template_dialog.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPromise>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QToolTip>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace frames=fs::frames;
namespace {
struct TaskResult { QList<frames::Station> sheets; QString error; };
template<typename Work,typename Done>
void background(QObject* owner,Work work,Done done) {
    auto promise=std::make_shared<QPromise<TaskResult>>();
    auto* watcher=new QFutureWatcher<TaskResult>(owner);
    QObject::connect(watcher,&QFutureWatcher<TaskResult>::finished,owner,[watcher,done]{
        auto result=watcher->result();watcher->deleteLater();done(std::move(result));
    });
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise,work=std::move(work)]{
        promise->start();TaskResult result;
        try { result.sheets=work(); }
        catch(const std::exception& e) { result.error=QString::fromUtf8(e.what()); }
        promise->addResult(std::move(result));promise->finish();
    });
}
}

class StationPreview final : public QWidget {
public:
    using QWidget::QWidget;
    void display(const frames::Station* s){sheet_=s?*s:frames::Station{};update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),Qt::white);
        if(sheet_.outlines.isEmpty()) return;
        const QRectF b=frames::plotBounds(sheet_).adjusted(-4,-4,4,4);
        auto area=QRectF(rect()).adjusted(14,14,-14,-14);
        const double scale=std::min(area.width()/b.width(),area.height()/b.height());
        if(scale<=0) return;
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(area.center());p.scale(scale,-scale);p.translate(-b.center());
        // Cosmetic weights keep the outline legible even for a large hull.
        QPen axis(QColor(135,135,135));axis.setCosmetic(true);axis.setStyle(Qt::DashLine);p.setPen(axis);
        p.drawLine(QLineF(0,b.top(),0,b.bottom()));p.drawLine(QLineF(b.left(),0,b.right(),0));
        QPen line(QColor(25,82,84));line.setCosmetic(true);line.setWidth(2);p.setPen(line);
        for(const auto& path:sheet_.outlines) p.drawPolyline(path);
    }
private:
    frames::Station sheet_;
};

StationTemplateDialog::StationTemplateDialog(std::shared_ptr<const fs::ModelSnapshot> model,QString inputPath,
                QString fileBase,const QSet<int>& selected,QWidget* parent)
    :QDialog(parent),model_(std::move(model)),inputPath_(std::move(inputPath)),fileBase_(std::move(fileBase)) {
    setObjectName("StationTemplates");
    setWindowTitle(uiText("导出横剖面图"));setStudioWindowIcon(this,"Sections");
    setWindowModality(Qt::WindowModal);resize(790,530);
    auto* root=new QVBoxLayout(this);
    auto* settings=new QHBoxLayout;root->addLayout(settings);
    type_=new QComboBox;type_->setObjectName("FrameContourType");
    addUiItem(type_,"船体横剖线");addUiItem(type_,"造模样板轮廓");
    settings->addWidget(type_);
    settings->addWidget(new QLabel(uiText("图层")));
    layer_=new QComboBox;layer_->setObjectName("FrameLayer");addUiItem(layer_,"参与剖切的图层",-1);
    for(int i=0;i<model_->layers.size();++i) layer_->addItem(model_->layers[i].name,i);
    settings->addWidget(layer_,1);
    auto* thicknessLabel=new QLabel(uiText("外板厚度 (mm)"));settings->addWidget(thicknessLabel);
    thickness_=new QDoubleSpinBox;thickness_->setObjectName("FrameThickness");
    thickness_->setRange(.01,1000);thickness_->setDecimals(2);thickness_->setValue(6);thickness_->setKeyboardTracking(false);
    settings->addWidget(thickness_);cap_=new QCheckBox(uiText("舷顶直线封合"));cap_->setObjectName("FrameCap");
    settings->addWidget(cap_);
    auto updateType=[this,thicknessLabel]{
        const bool inner=type_->currentIndex()==1;
        thicknessLabel->setVisible(inner);thickness_->setVisible(inner);cap_->setVisible(inner);
    };
    updateType();
    auto* middle=new QHBoxLayout;root->addLayout(middle,1);
    stations_=new QListWidget;stations_->setObjectName("FrameStations");stations_->setMaximumWidth(245);
    applyStudioScrollbars(stations_);middle->addWidget(stations_);
    int first=-1;
    for(const auto& s:model_->sections) if(s.kind==1) {
        // FBM station positions have single precision. Display their shortest
        // round-tripping decimal so 2.9 m is also recognisable in the station list.
        char digits[48];
        const auto value=std::to_chars(digits,digits+sizeof digits,float(s.distance));
        auto* item=new QListWidgetItem(QStringLiteral("%1     X = %2 %3").arg(s.index+1).arg(QString::fromLatin1(digits,value.ptr-digits))
                        .arg(model_->units=="imperial"?"ft":"m"),stations_);
        item->setData(Qt::UserRole,s.index);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
        const bool checked=selected.isEmpty()||selected.contains(s.index);
        item->setCheckState(checked?Qt::Checked:Qt::Unchecked);
        if(checked && first<0) first=stations_->count()-1;
    }
    stations_->setCurrentRow(first);
    preview_=new StationPreview;preview_->setMinimumSize(180,150);middle->addWidget(preview_,1);
    message_=new StudioMessage(StudioMessageRole::Feedback);root->addWidget(message_);
    auto* actions=new QHBoxLayout;root->addLayout(actions);
    actions->addWidget(new QLabel(uiText("A4 拼页")));
    noTrim_=new QRadioButton(uiText("无需裁剪"));noTrim_->setObjectName("FramePdfNoTrim");actions->addWidget(noTrim_);
    auto* help=new QToolButton;help->setText("?");help->setObjectName("FramePdfNoTrimHelp");
    bindUiText(help,"toolTip","需要打印机能够同时做到<b>A4 无边距、100% 原尺寸打印</b>");
    help->setAccessibleName(uiText("无需裁剪"));help->setAutoRaise(true);actions->addWidget(help);
    connect(help,&QToolButton::clicked,help,[help]{QToolTip::showText(help->mapToGlobal(help->rect().bottomLeft()),help->toolTip(),help);});
    auto* trim=new QRadioButton(uiText("需裁剪"));trim->setChecked(true);actions->addWidget(trim);
    actions->addStretch();
    auto* scaleLabel=new QLabel(uiText("出图比例"));actions->addWidget(scaleLabel);
    scale_=new QSpinBox;scale_->setObjectName("FramePdfScale");
    scale_->setRange(1,10000);scale_->setPrefix(QStringLiteral("1 : "));scale_->setKeyboardTracking(false);
    scaleLabel->setBuddy(scale_);actions->addWidget(scale_);
    pdf_=new QPushButton;
    dxf_=new QPushButton;
    pdf_->setObjectName("FramePdf");dxf_->setObjectName("FrameDxf");
    const auto refreshFormats=[this]{
        setStudioFormatButton(pdf_,"PDF",uiText("导出 ")+QStringLiteral("PDF"));
        setStudioFormatButton(dxf_,"DXF",uiText("导出 ")+QStringLiteral("DXF"));
    };
    refreshFormats();bindUiRefresh(this,refreshFormats);
    actions->addWidget(pdf_);actions->addWidget(dxf_);
    timer_=new QTimer(this);timer_->setSingleShot(true);
    connect(timer_,&QTimer::timeout,this,&StationTemplateDialog::generate);
    connect(stations_,&QListWidget::itemChanged,this,[this]{schedule();});
    connect(stations_,&QListWidget::currentRowChanged,this,[this]{refresh();});
    connect(type_,&QComboBox::currentIndexChanged,this,[this,updateType]{updateType();schedule();});
    connect(layer_,&QComboBox::currentIndexChanged,this,[this]{schedule();});
    connect(thickness_,&QDoubleSpinBox::valueChanged,this,[this]{schedule();});
    connect(cap_,&QCheckBox::toggled,this,[this]{schedule();});
    auto* layouts=new QMenu(pdf_);
    tiledPdf_=layouts->addAction(QString(),this,[this]{save(true,tileLayout());});
    wholePdf_=layouts->addAction(QString(),this,[this]{save(true,frames::PdfLayout::WholeStation);});
    tiledPdf_->setObjectName("StationPdfTiles");wholePdf_->setObjectName("StationPdfWhole");
    pdf_->setMenu(layouts);
    connect(scale_,&QSpinBox::valueChanged,this,[this](int denominator){
        pageCount_=frames::pdfPageCount(sheets_,denominator,tileLayout());refresh();
    });
    connect(noTrim_,&QRadioButton::toggled,this,[this]{pageCount_=frames::pdfPageCount(sheets_,scale_->value(),tileLayout());refresh();});
    connect(dxf_,&QPushButton::clicked,this,[this]{save(false);});
    schedule();
}
StationTemplateDialog::~StationTemplateDialog(){if(cancel_) cancel_->store(true);}
frames::PdfLayout StationTemplateDialog::tileLayout() const {
    return noTrim_->isChecked()?frames::PdfLayout::A4EdgeToEdge:frames::PdfLayout::A4Tiles;
}
void StationTemplateDialog::schedule() {
    if(cancel_) cancel_->store(true);
    cancel_=std::make_shared<std::atomic_bool>(false);sheets_.clear();error_.clear();pageCount_=0;pending_=true;
    updateStationWarnings();
    refresh();timer_->start(80);
}
void StationTemplateDialog::generate() {
    frames::Options options;
    for(int i=0;i<stations_->count();++i) if(stations_->item(i)->checkState()==Qt::Checked)
        options.indices.insert(stations_->item(i)->data(Qt::UserRole).toInt());
    if(options.indices.isEmpty()){pending_=false;refresh();return;}
    options.layer=layer_->currentData().toInt();
    options.thickness=type_->currentIndex()?thickness_->value():0;
    options.closeSheer=cap_->isChecked();
    const auto token=cancel_;
    background(this,[token,options,path=inputPath_,id=model_->documentId,revision=model_->revision,imperial=model_->units=="imperial"]{
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly)||f.size()>256*1024*1024) throw std::runtime_error("无法读取当前船模几何。");
        auto input=fs::native_geometry::DocumentInput::read(QJsonDocument::fromJson(f.readAll()).object());
        if(input.documentId!=id || input.revision!=revision || input.imperial!=imperial)
            throw std::runtime_error("船模版本已更改，请重新打开实尺图。");
        return frames::sectionTemplates(input,options,*token);
    },[this,token](TaskResult result){
        if(token!=cancel_) return;
        sheets_=std::move(result.sheets);error_=std::move(result.error);pending_=false;
        pageCount_=frames::pdfPageCount(sheets_,scale_->value(),tileLayout());
        updateStationWarnings();refresh();
    });
}
void StationTemplateDialog::updateStationWarnings() {
    const QSignalBlocker block(stations_);
    QHash<int,QString> errors;
    for(const auto& s:sheets_) if(!s.error.isEmpty()) errors.insert(s.number-1,uiText(s.error));
    const auto icon=style()->standardIcon(QStyle::SP_MessageBoxWarning);
    for(int i=0;i<stations_->count();++i) {
        auto* item=stations_->item(i);
        const auto reason=errors.value(item->data(Qt::UserRole).toInt());
        if(item->toolTip()==reason) continue;
        item->setToolTip(reason);item->setIcon(reason.isEmpty()?QIcon{}:icon);
    }
}
void StationTemplateDialog::refresh() {
    const auto* row=stations_->currentItem();
    const int current=row?row->data(Qt::UserRole).toInt()+1:0;
    const frames::Station *shown=nullptr,*failed=nullptr;
    for(const auto& s:sheets_) {
        if(s.number==current) shown=&s;
        if(!s.error.isEmpty() && (!failed || s.number==current)) failed=&s;
    }
    preview_->display(shown);
    const bool valid=!pending_&&!sheets_.isEmpty()&&!failed&&error_.isEmpty();
    const bool fits=pageCount_<=frames::maximumPdfPages;
    const bool wholeFits=sheets_.size()<=frames::maximumPdfPages;
    dxf_->setEnabled(valid&&!writing_);pdf_->setEnabled(dxf_->isEnabled()&&wholeFits);
    tiledPdf_->setEnabled(valid&&fits&&!writing_);
    wholePdf_->setEnabled(pdf_->isEnabled());
    tiledPdf_->setText(fits?QString(uiText("A4 拼页（%1 页）…")).arg(pageCount_):uiText("A4 拼页（超过 500 页）"));
    wholePdf_->setText(QString(uiText("每个站位一张整图（%1 页）…")).arg(sheets_.size()));
    QString status;
    bool warning=false;
    if(writing_) status=uiText("正在导出…");
    else if(pending_) status=uiText("正在生成横剖断面…");
    else if(!error_.isEmpty()) {status=uiText(error_);warning=true;}
    else if(failed) {status=QString(uiText("站 %1：%2")).arg(failed->number).arg(uiText(failed->error));warning=true;}
    else if(sheets_.isEmpty()) status=uiText("请选择横剖站。");
    else if(!wholeFits) {status=uiText("PDF 超过 500 页，请减少所选站位。");warning=true;}
    message_->setMessage(status,warning?StudioMessageRole::Warning:StudioMessageRole::Feedback,pending_||writing_);
    message_->setVisible(!status.isEmpty());
}
void StationTemplateDialog::save(bool pdf,frames::PdfLayout layout) {
    if(!(pdf?pdf_:dxf_)->isEnabled()) return;
    const QString suffix=pdf?"pdf":"dxf";
    auto* picker=new QFileDialog(this);
    if(testAttribute(Qt::WA_DontShowOnScreen)) {
        picker->setOption(QFileDialog::DontUseNativeDialog);picker->setAttribute(Qt::WA_DontShowOnScreen);
    }
    const QFileInfo initial(fileBase_+"."+suffix);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setWindowTitle(windowTitle());picker->setFileMode(QFileDialog::AnyFile);
    picker->setAcceptMode(QFileDialog::AcceptSave);picker->setDefaultSuffix(suffix);
    picker->setNameFilter(pdf?"PDF (*.pdf)":"DXF (*.dxf)");
    picker->setDirectory(initial.absolutePath());picker->selectFile(initial.fileName());
    connect(picker,&QFileDialog::fileSelected,this,[this,pdf,layout](const QString& path){writeSelection(pdf,layout,path);});
    picker->open();
}
void StationTemplateDialog::writeSelection(bool pdf,frames::PdfLayout layout,const QString& path) {
    if(writing_ || path.isEmpty()) return;
    const QString suffix=pdf?"pdf":"dxf";
    if(QFileInfo(path).suffix().compare(suffix,Qt::CaseInsensitive)!=0) {
        message_->setMessage(uiText("文件扩展名与所选导出格式不一致。"),StudioMessageRole::Warning);message_->show();return;
    }
    const double t=type_->currentIndex()?thickness_->value():0;
    frames::SheetLabels labels{type_->currentText(),uiText("站"),uiText("100 mm · 按实际大小打印"),
                               uiText("A4 拼接示意"),
                               uiText("站 %1 · %2 行 × %3 列 · 缩略图"),
                               uiText("带剪刀的虚线：只剪有剪刀的一侧，另一张保留搭接边。"),
                               uiText("十字：上页压住底页，让一条接缝上的两个十字同时对齐。"),
                               layout==frames::PdfLayout::A4EdgeToEdge?uiText("中心线和基线"):uiText("无剪刀的轴线：中心线和基线，保留纸面。"),
                               uiText("黑色实线：当前站位的轮廓。"),
                               uiText("按编号排列，纸边相接后贴合。")};
    if(t>0) labels.heading+=QStringLiteral("  t = %1 mm").arg(t);
    const int denominator=scale_->value();
    writing_=true;scale_->setEnabled(false);type_->setEnabled(false);layer_->setEnabled(false);thickness_->setEnabled(false);cap_->setEnabled(false);stations_->setEnabled(false);
    refresh();
    auto token=cancel_;
    background(this,[sheets=sheets_,path,labels,t,pdf,layout,denominator,token]{
        if(pdf) frames::writePdf(path,sheets,labels,layout,denominator,*token);
        else frames::writeDxf(path,sheets,t,*token);
        return QList<frames::Station>{};
    },[this,path](TaskResult result){
        writing_=false;scale_->setEnabled(true);type_->setEnabled(true);layer_->setEnabled(true);thickness_->setEnabled(true);cap_->setEnabled(true);stations_->setEnabled(true);
        refresh();
        const bool ok=result.error.isEmpty();
        message_->setMessage(ok?QString(uiText("已保存：%1")).arg(QDir::toNativeSeparators(path)):uiText(result.error),
                             ok?StudioMessageRole::Feedback:StudioMessageRole::Warning);
        message_->show();
    });
}

