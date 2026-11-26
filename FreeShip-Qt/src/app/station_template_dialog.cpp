// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_template_dialog.h"
#include "pdf_scale_inputs.h"
#include "pdf_export_dialog.h"
#include "studio_row_actions.h"
#include "studio_motion.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "render/curve_edit_motion.h"
#include "render/label_layout.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPromise>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QThreadPool>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <mutex>
#include <stdexcept>

namespace frames=fs::frames;
namespace {
class PdfLayoutList final : public QListView {
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(watched!=viewport())return QListView::eventFilter(watched,event);
        if(event->type()==QEvent::MouseMove){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const auto item=indexAt(mouse->position().toPoint());
            const auto tip=item.data(Qt::ToolTipRole).toString();
            if(tip!=shownTip_||(!tip.isEmpty()&&!QToolTip::isVisible())){
                clearTip();
                shownTip_=tip;
                if(!tip.isEmpty())QToolTip::showText(mouse->globalPosition().toPoint(),tip,viewport(),visualRect(item));
            }
        }else if(event->type()==QEvent::Leave||event->type()==QEvent::Hide){
            clearTip();
        }
        return QListView::eventFilter(watched,event);
    }
private:
    void clearTip(){
        if(!shownTip_.isEmpty()&&QToolTip::text()==shownTip_)QToolTip::hideText();
        shownTip_.clear();
    }
    QString shownTip_;
};

constexpr int maximumStations=200;
constexpr double maximumStationCoordinate=1e8;
constexpr int modelStationRole=Qt::UserRole+1;
class StationCoordinateEditor final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override {
        auto text=option;
        text.state&=~(QStyle::State_Selected|QStyle::State_HasFocus);
        QStyledItemDelegate::paint(painter,text,index);
    }
    QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem&,const QModelIndex&) const override {
        auto* editor=new QDoubleSpinBox(parent);
        editor->setObjectName("StationCoordinate");
        editor->setRange(-maximumStationCoordinate,maximumStationCoordinate);
        editor->setDecimals(2);
        editor->setFrame(false);
        return editor;
    }
};
class StationCoordinateItem final : public QTableWidgetItem {
public:
    QVariant data(int role)const override {
        if(role==Qt::DisplayRole){
            auto locale=QLocale();
            locale.setNumberOptions(locale.numberOptions()|QLocale::OmitGroupSeparator);
            return locale.toString(QTableWidgetItem::data(Qt::EditRole).toDouble(),'f',2);
        }
        return QTableWidgetItem::data(role);
    }
    QTableWidgetItem* clone()const override{return new StationCoordinateItem(*this);}
};

struct TaskResult { QList<frames::Station> sheets; QString error; QHash<int,QString> stationWarnings; };
template<typename Work,typename Done>
void background(QObject* owner,Work work,Done done) {
    auto promise=std::make_shared<QPromise<TaskResult>>();
    auto* watcher=new QFutureWatcher<TaskResult>(owner);
    QObject::connect(watcher,&QFutureWatcher<TaskResult>::finished,owner,[watcher,done]{
        auto result=watcher->result();watcher->deleteLater();done(std::move(result));
    });
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise,work=std::move(work)]() mutable {
        promise->start();TaskResult result;
        try { work(result); }
        catch(const std::exception& e) { result.error=QString::fromUtf8(e.what()); }
        promise->addResult(std::move(result));promise->finish();
    });
}
}


// All entry points in an export window are tied to the same model revision.
struct StationTemplateDialog::GeometryCache {
    struct StationPosition {
        int order;
        double x;
        bool fromModel;
        bool operator==(const StationPosition& other) const {
            return order==other.order&&x==other.x&&fromModel==other.fromModel;
        }
    };
    struct Cut {
        frames::Options options;
        QList<StationPosition> positions;
        QList<frames::Station> sheets;
    };
    std::mutex mutex;
    std::optional<fs::native_geometry::DocumentInput> input;
    int layer=-1;
    bool shell=false;
    std::unique_ptr<frames::StationGeometry> geometry;
    QList<Cut> recentCuts;
};

class StationStatus final : public QLabel {
public:
    explicit StationStatus(QWidget* parent) : QLabel(parent) {
        setObjectName("StationTemplateStatus");
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    }
    void setMessage(const QString& text,StudioMessageRole role,bool working=false) {
        role_=role;
        working_=working;
        setProperty("studio_message_busy",working);
        setText(text);
        setToolTip(text);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if(text().isEmpty())return;
        QPainter p(this);
        p.save();
        p.translate(2,(height()-16)/2.);
        paintStudioMessageMark(p,role_,working_);
        p.restore();
        p.setPen(studioMessageStyle(role_).foreground);
        const auto area=rect().adjusted(26,0,-4,0);
        p.drawText(area,Qt::AlignLeft|Qt::AlignVCenter,fontMetrics().elidedText(text(),Qt::ElideRight,area.width()));
    }
private:
    StudioMessageRole role_=StudioMessageRole::Feedback;
    bool working_=false;
};

class StationPreview final : public QWidget {
public:
    StationPreview() {
        motion_.setDuration(CurveMotion::DurationMs);
        motion_.setEasingCurve(QEasingCurve::OutCubic);
        motion_.setStartValue(0.);
        motion_.setEndValue(1.);
        connect(&motion_,&QVariantAnimation::valueChanged,this,[this]{update();});
        connect(&motion_,&QVariantAnimation::finished,this,[this]{finishMotion();});
    }
    void display(const frames::Station* section,const frames::Station* spine,bool animate){
        Drawing next{section?*section:frames::Station{},spine?*spine:frames::Station{}};
        if(next.section.outlines.isEmpty())next.section.outlines=next.section.diagnosticOutlines;
        if(sameDrawing(target_.section,next.section)&&sameDrawing(target_.spine,next.spine))return;
        if(!animate||!studio_motion::enabled()||!isVisible()||
           (target_.section.outlines.isEmpty()&&target_.spine.outlines.isEmpty())){
            finishMotion();
            target_=std::move(next);
            update();
            return;
        }
        auto from=currentDrawing();
        auto sectionTracks=tracks(from.section,next.section);
        auto spineTracks=tracks(from.spine,next.spine);
        // A changed loop (for example, closing the sheer) needs a blend.
        // Restart a blend from the picture currently visible on screen.
        const bool morph=fadeFrom_.isNull()&&sectionTracks&&spineTracks;
        auto previous=morph?QPixmap{}:grab();
        finishMotion();
        from_=std::move(from);
        target_=std::move(next);
        if(morph){
            sectionTracks_=std::move(*sectionTracks);
            spineTracks_=std::move(*spineTracks);
        }
        fadeFrom_=std::move(previous);
        setProperty("station_preview_motion",morph?"morph":"fade");
        motion_.start();
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),Qt::white);
        const auto drawing=currentDrawing();
        paintDrawing(p,drawing);
        if(!fadeFrom_.isNull()){
            p.setOpacity(1.-motion_.currentValue().toDouble());
            p.drawPixmap(rect(),fadeFrom_);
        }
    }
private:
    using CurveMotion=fs::curveEdit::Motion;
    using Tracks=QList<CurveMotion::Track>;
    struct Drawing {frames::Station section,spine;};
    enum class Guides { None, Centerline, Both };
    static bool sameDrawing(const frames::Station& a,const frames::Station& b){
        return a.number==b.number&&a.outlines==b.outlines&&a.slotLabels==b.slotLabels&&
            a.shellReference==b.shellReference&&a.diagnosticOutlines.isEmpty()==b.diagnosticOutlines.isEmpty();
    }
    static QVector<fs::Vec3> vertices(const QPolygonF& path){
        QVector<fs::Vec3> result;
        result.reserve(path.size());
        if(path.size()<3||!path.isClosed()){
            for(const auto& p:path)result.append({p.x(),p.y(),0});
            return result;
        }
        // Closed loops can begin at different vertices after clipping. Use
        // the same geometric end before matching length along the two loops.
        const auto end=std::prev(path.cend());
        const auto start=std::min_element(path.cbegin(),end,[](QPointF a,QPointF b){
            return a.x()<b.x()||(a.x()==b.x()&&a.y()<b.y());
        });
        for(auto p=start;p!=end;++p)result.append({p->x(),p->y(),0});
        for(auto p=path.cbegin();p!=start;++p)result.append({p->x(),p->y(),0});
        result.append(result.first());
        return result;
    }
    static std::optional<Tracks> tracks(const frames::Station& a,const frames::Station& b){
        if(a.number!=b.number||a.outlines.size()!=b.outlines.size()||a.outlines.size()>1||
           a.shellReference!=b.shellReference||
           a.diagnosticOutlines.isEmpty()!=b.diagnosticOutlines.isEmpty()||a.slotLabels.size()!=b.slotLabels.size())return {};
        for(qsizetype i=0;i<a.slotLabels.size();++i)if(a.slotLabels[i].first!=b.slotLabels[i].first)return {};
        Tracks result;
        for(qsizetype i=0;i<a.outlines.size();++i){
            auto track=CurveMotion::pathTrack(vertices(a.outlines[i]),vertices(b.outlines[i]));
            if(!track)return {};
            result.append(std::move(*track));
        }
        return result;
    }
    static frames::Station sample(const frames::Station& from,frames::Station to,const Tracks& tracks,double t){
        for(qsizetype i=0;i<tracks.size();++i){
            const auto shape=CurveMotion::sample(tracks[i],t);
            QPolygonF path;
            path.reserve(shape.path.size());
            for(const auto& p:shape.path)path.append(QPointF(p.x,p.y));
            to.outlines[i]=std::move(path);
        }
        for(qsizetype i=0;i<to.slotLabels.size();++i){
            const auto p=from.slotLabels[i].second;
            to.slotLabels[i].second=p+(to.slotLabels[i].second-p)*t;
        }
        return to;
    }
    Drawing currentDrawing() const {
        if(motion_.state()!=QAbstractAnimation::Running||!fadeFrom_.isNull())return target_;
        const auto t=motion_.currentValue().toDouble();
        return {sample(from_.section,target_.section,sectionTracks_,t),sample(from_.spine,target_.spine,spineTracks_,t)};
    }
    void finishMotion(){
        motion_.stop();
        fadeFrom_={};
        from_={};
        sectionTracks_.clear();
        spineTracks_.clear();
        setProperty("station_preview_motion",QVariant{});
        update();
    }
    void paintDrawing(QPainter& p,const Drawing& drawing){
        if(drawing.spine.outlines.isEmpty()){
            paintStation(p,drawing.section,rect(),Guides::Both);
            return;
        }
        const int divider=height()*3/5;
        paintStation(p,drawing.section,QRectF(0,0,width(),divider),Guides::Centerline);
        paintStation(p,drawing.spine,QRectF(0,divider,width(),height()-divider),Guides::None);
        p.setPen(QPen(QColor(135,135,135),1,Qt::DashLine));
        p.drawLine(QLineF(14,divider,width()-14,divider));
    }
    void paintStation(QPainter& p,const frames::Station& sheet,const QRectF& pane,Guides guides){
        if(sheet.outlines.isEmpty())return;
        // The transverse view shows the model datum. The longitudinal board
        // is framed by its own extents, including when the stations start far from X=0.
        const auto outlineBounds=guides==Guides::None?sheet.outlines.first().boundingRect():frames::plotBounds(sheet);
        const QRectF b=outlineBounds.adjusted(-4,-4,4,4);
        const bool diagnostic=!sheet.diagnosticOutlines.isEmpty();
        const bool reference=!sheet.shellReference.isEmpty();
        const QColor diagnosticColor("#9c5823");
        const QColor referenceColor("#687d82"),cutColor(25,82,84);
        const QPen outerPen(referenceColor,1.2),cutPen(cutColor,2,Qt::DashLine);
        const QFontMetricsF metrics(p.font());
        const auto outlineLabel=QString(uiText("船壳外轮廓"));
        const auto cutLabel=QString(uiText("样板切割线"));
        const double firstKeyWidth=metrics.horizontalAdvance(outlineLabel)+44;
        const bool wrapKey=reference&&firstKeyWidth+metrics.horizontalAdvance(cutLabel)+56>pane.width();
        double labelSpace=(diagnostic||reference)?metrics.height()+6:0;
        if(wrapKey)labelSpace+=metrics.height()+4;
        if(!sheet.slotLabels.isEmpty())
            labelSpace=std::min(metrics.height()+12,pane.height()*.35);
        const auto area=pane.adjusted(14,14+labelSpace,-14,-14);
        const double scale=std::min(area.width()/b.width(),area.height()/b.height());
        if(scale<=0) return;
        p.save();
        p.setClipRect(pane);
        p.setRenderHint(QPainter::Antialiasing);
        const auto legend=[&](QPointF key,const QPen& pen,const QString& label){
            p.setPen(pen);
            p.drawLine(QLineF(key,key+QPointF(22,0)));
            p.drawText(key+QPointF(28,metrics.ascent()-metrics.height()/2),label);
        };
        const QPointF key(pane.left()+14,pane.top()+12+metrics.height()/2);
        if(diagnostic)legend(key,QPen(diagnosticColor,2,Qt::DashLine),uiText("原始剖面"));
        if(reference){
            legend(key,outerPen,outlineLabel);
            legend(key+(wrapKey?QPointF(0,metrics.height()+4):QPointF(firstKeyWidth,0)),cutPen,cutLabel);
        }
        p.translate(area.center());p.scale(scale,-scale);p.translate(-b.center());
        // Cosmetic weights keep the outline legible even for a large hull.
        QPen axis(QColor(135,135,135));axis.setCosmetic(true);axis.setStyle(Qt::DashLine);p.setPen(axis);
        if(guides!=Guides::None)p.drawLine(QLineF(0,b.top(),0,b.bottom()));
        if(guides==Guides::Both)p.drawLine(QLineF(b.left(),0,b.right(),0));
        QPen outer=outerPen;outer.setCosmetic(true);p.setPen(outer);
        for(const auto& path:sheet.shellReference)p.drawPolyline(path);
        QPen line=diagnostic?QPen(diagnosticColor,2,Qt::DashLine):reference?cutPen:QPen(cutColor,2);
        line.setCosmetic(true);line.setWidth(2);
        p.setPen(line);
        for(const auto& path:sheet.outlines) p.drawPolyline(path);
        paintSlotLabels(p,sheet,pane);
        p.restore();
    }
    void paintSlotLabels(QPainter& p,const frames::Station& sheet,const QRectF& pane){
        if(sheet.slotLabels.isEmpty())return;
        const auto transform=p.worldTransform();
        p.resetTransform();
        const auto outline=transform.mapRect(sheet.outlines.first().boundingRect());
        fs::presentation::LabelLayout placement;
        placement.begin(pane.adjusted(5,5,-5,-5),0,false);
        placement.reserve(outline.adjusted(-3,-3,3,3));
        const QFontMetricsF metrics(p.font());
        struct Label {QString text;QRectF box;};
        QList<Label> labels;
        for(const auto& mark:sheet.slotLabels){
            const auto text=QString::number(mark.first);
            const QPointF anchor(transform.map(mark.second).x(),outline.top());
            const QSizeF size(metrics.horizontalAdvance(text)+8,metrics.height()+4);
            const auto preferred=anchor-QPointF(size.width()/2,size.height()+8);
            const auto placed=placement.place(text,text,anchor,preferred,size,fs::presentation::LabelKind::Point);
            if(!placed.visible||placed.clip.size()!=size)continue;
            const QRectF box(placed.position,size);
            if(box.intersects(outline))continue;
            const bool above=box.center().y()<outline.center().y();
            const QPointF edge(anchor.x(),above?outline.top():outline.bottom());
            const QPointF end(box.center().x(),above?box.bottom():box.top());
            p.setPen(QPen(QColor(96,129,125),1));
            p.drawLine(QLineF(edge,end));
            labels.append({text,box});
        }
        p.setPen(QColor(25,82,84));
        for(const auto& label:labels){
            p.fillRect(label.box,Qt::white);
            p.drawText(label.box,Qt::AlignCenter,label.text);
        }
    }
    Drawing from_,target_;
    Tracks sectionTracks_,spineTracks_;
    QPixmap fadeFrom_;
    QVariantAnimation motion_;
};

StationTemplateDialog::StationTemplateDialog(std::shared_ptr<const fs::ModelSnapshot> model,QString inputPath,
                QString fileBase,const QSet<int>& selected,QWidget* parent)
    :QDialog(parent),model_(std::move(model)),inputPath_(std::move(inputPath)),fileBase_(std::move(fileBase)) {
    setObjectName("StationTemplates");
    setWindowTitle(uiText("导出横剖面图"));setStudioWindowIcon(this,"Sections");
    resize(790,530);
    auto* root=new QVBoxLayout(this);
    auto* settings=new QHBoxLayout;root->addLayout(settings);
    type_=new QComboBox;type_->setObjectName("FrameContourType");
    addUiItem(type_,"船体横剖线");addUiItem(type_,"造模样板");addUiItem(type_,"插接模架");
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
    kitSettings_=new QWidget;
    auto* kit=new QHBoxLayout(kitSettings_);
    kit->setContentsMargins(0,0,0,0);
    root->addWidget(kitSettings_);
    kit->addWidget(new QLabel(uiText("模架板厚 (mm)")));
    stock_=new QDoubleSpinBox;
    stock_->setObjectName("KitStock");
    stock_->setRange(.01,1000);stock_->setDecimals(2);stock_->setValue(9);
    stock_->setKeyboardTracking(false);
    kit->addWidget(stock_);
    kit->addStretch();
    auto* middle=new QHBoxLayout;root->addLayout(middle,1);
    stations_=new QListWidget;stations_->setObjectName("FrameStations");stations_->setMaximumWidth(245);
    applyStudioScrollbars(stations_);middle->addWidget(stations_);
    int first=-1;
    QList<const fs::Section*> modelStations;
    for(const auto& s:model_->sections) if(s.kind==1) {
        modelStations.append(&s);
        kit_.nextOrder=std::max(kit_.nextOrder,s.index+1);
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
    std::stable_sort(modelStations.begin(),modelStations.end(),[](const auto* a,const auto* b){
        return a->distance<b->distance;
    });
    QList<const fs::Section*> mouldStations;
    for(const auto* station:modelStations)
        if(selected.isEmpty()||selected.contains(station->index))mouldStations.append(station);
    mould_.nextOrder=kit_.nextOrder;
    createStationTable(mould_,QStringLiteral("Mould"),mouldStations);
    createStationTable(kit_,QStringLiteral("Kit"),modelStations);
    middle->addWidget(mould_.panel);
    middle->addWidget(kit_.panel);
    auto updateType=[this,thicknessLabel]{
        const int mode=type_->currentIndex();
        const bool kit=mode==2;
        stations_->setVisible(mode==0);
        mould_.panel->setVisible(mode==1);
        kit_.panel->setVisible(kit);
        kitSettings_->setVisible(kit);
        thicknessLabel->setVisible(mode!=0);thickness_->setVisible(mode!=0);cap_->setVisible(mode!=0);
    };
    updateType();
    preview_=new StationPreview;preview_->setObjectName("StationTemplatePreview");preview_->setMinimumSize(180,150);middle->addWidget(preview_,1);
    auto* actions=new QHBoxLayout;root->addLayout(actions);
    message_=new StationStatus(this);actions->addWidget(message_,1);
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
    connect(thickness_,&QDoubleSpinBox::valueChanged,this,[this]{schedule(true);});
    connect(cap_,&QCheckBox::toggled,this,[this]{schedule(true);});
    connect(stock_,&QDoubleSpinBox::valueChanged,this,[this]{schedule(true);});
    connect(pdf_,&QPushButton::clicked,this,&StationTemplateDialog::configurePdf);
    bindUiRefresh(this,[this]{updateStationWarnings();refresh();});
    connect(dxf_,&QPushButton::clicked,this,[this]{save(false);});
    schedule();
}
StationTemplateDialog::~StationTemplateDialog(){if(cancel_) cancel_->store(true);}
void StationTemplateDialog::createStationTable(StationTable& table,const QString& name,
                                               const QList<const fs::Section*>& stations) {
    table.panel=new QWidget;
    auto* layout=new QVBoxLayout(table.panel);
    layout->setContentsMargins(0,0,0,0);
    auto* rows=table.rows=new RowActionsTable(3,true);
    rows->setObjectName(name+QStringLiteral("Stations"));
    const auto headers=[rows]{
        rows->setHorizontalHeaderLabels({uiText("站"),QStringLiteral("X (mm)"),uiText("状态")});
        rows->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    };
    headers();bindUiRefresh(rows,headers);
    rows->verticalHeader()->hide();
    rows->setSelectionMode(QAbstractItemView::SingleSelection);
    rows->setSelectionBehavior(QAbstractItemView::SelectItems);
    rows->horizontalHeader()->setHighlightSections(false);
    rows->setEditTriggers(QAbstractItemView::EditKeyPressed|QAbstractItemView::AnyKeyPressed);
    rows->setItemDelegateForColumn(1,new StationCoordinateEditor(rows));
    table.add=rows->addHeaderButton("添加站位");
    table.add->setObjectName(name+QStringLiteral("AddStation"));
    applyStudioScrollbars(rows);
    configureStudioDataTable(rows);
    auto* header=rows->horizontalHeader();
    const auto fitPanel=[panel=table.panel,rows,header]{
        const int scrollbar=rows->verticalScrollBar()->maximum()>0?rows->verticalScrollBar()->sizeHint().width():0;
        const int width=std::clamp(header->length()+2*rows->frameWidth()+scrollbar,100,310);
        if(panel->width()!=width)panel->setFixedWidth(width);
    };
    connect(header,&QHeaderView::sectionResized,table.panel,fitPanel);
    connect(rows->verticalScrollBar(),&QScrollBar::rangeChanged,table.panel,fitPanel);
    layout->addWidget(rows,1);
    const double unit=model_->units=="imperial"?304.8:1000.;
    for(const auto* station:stations)
        insertStation(table,rows->rowCount(),station->distance*unit,station->index,true);
    updateStationActions(table);
    if(rows->rowCount()>0)rows->setCurrentCell(0,1);
    connect(rows,&QTableWidget::itemChanged,this,[this](QTableWidgetItem* item){if(item->column()==1)schedule();});
    connect(rows,&QTableWidget::currentCellChanged,this,[this]{refresh();});
    connect(rows,&QTableWidget::cellClicked,this,[rows](int row,int column){
        auto* item=rows->item(row,column);
        if(item&&item->flags().testFlag(Qt::ItemIsEditable))rows->editItem(item);
    });
    connect(table.add,&QToolButton::clicked,this,[this,&table,rows,unit]{
        const int row=rows->currentRow()+1;
        const auto xAt=[rows](int i){return rows->item(i,1)->data(Qt::EditRole).toDouble();};
        const double left=row>0?xAt(row-1):model_->minimum.x*unit;
        const double right=row<rows->rowCount()?xAt(row):model_->maximum.x*unit;
        {
            const QSignalBlocker block(rows);
            insertStation(table,row,(left+right)/2,table.nextOrder++,false);
            updateStationActions(table);
        }
        rows->setCurrentCell(row,1);
        rows->editItem(rows->item(row,1));
        schedule();
    });
}
void StationTemplateDialog::insertStation(StationTable& table,int row,double x,int order,bool fromModel) {
    auto* rows=table.rows;
    rows->insertRow(row);
    auto* number=new QTableWidgetItem(QString::number(order+1));
    number->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    number->setTextAlignment(Qt::AlignCenter);
    number->setData(Qt::UserRole,order);
    number->setData(modelStationRole,fromModel);
    rows->setItem(row,0,number);
    auto* coordinate=new StationCoordinateItem;
    if(fromModel){
        coordinate->setFlags(coordinate->flags()&~Qt::ItemIsEditable);
        coordinate->setToolTip(uiText("船模中的横剖线。可在「剖面线」窗口修改。"));
    }else{
        x=std::clamp(std::round(x*100)/100,-maximumStationCoordinate,maximumStationCoordinate);
    }
    coordinate->setData(Qt::EditRole,x);
    coordinate->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    rows->setItem(row,1,coordinate);
    auto* status=new QTableWidgetItem;
    status->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    status->setTextAlignment(Qt::AlignCenter);
    rows->setItem(row,2,status);
}
void StationTemplateDialog::updateStationActions(StationTable& table) {
    auto* rows=table.rows;
    rows->clearRowButtons();
    const int count=rows->rowCount();
    const QString name=rows->objectName().chopped(QStringLiteral("Stations").size());
    for(int row=0;row<count;++row){
        auto* remove=RowActionsTable::actionButton("DeleteOne","删除站位",rows->viewport());
        remove->setObjectName(name+QStringLiteral("RemoveStation"));
        rows->appendRowButton(remove);
        connect(remove,&QToolButton::clicked,this,[this,&table,rows,row]{
            {
                const QSignalBlocker block(rows);
                rows->removeRow(row);
                updateStationActions(table);
            }
            if(rows->rowCount()>0)rows->setCurrentCell(std::min(row,rows->rowCount()-1),1);
            schedule();
        });
    }
    rows->placeButtons();
    table.add->setEnabled(count<maximumStations);
}
void StationTemplateDialog::schedule(bool animate) {
    if(cancel_) cancel_->store(true);
    cancel_=std::make_shared<std::atomic_bool>(false);sheets_.clear();error_.clear();stationWarnings_.clear();pending_=true;
    animatePreview_=animate;
    updateStationWarnings();
    refresh();timer_->start(80);
}
void StationTemplateDialog::generate() {
    frames::Options options;
    const int mode=type_->currentIndex();
    const bool kit=mode==2;
    const auto* rows=mode==0?nullptr:kit?kit_.rows:mould_.rows;
    QList<GeometryCache::StationPosition> positions;
    if(!rows){
        for(int i=0;i<stations_->count();++i) if(stations_->item(i)->checkState()==Qt::Checked)
            options.indices.insert(stations_->item(i)->data(Qt::UserRole).toInt());
    }else{
        for(int row=0;row<rows->rowCount();++row){
            const auto* number=rows->item(row,0);
            const int order=number->data(Qt::UserRole).toInt();
            const double x=rows->item(row,1)->data(Qt::EditRole).toDouble();
            positions.append({order,x,number->data(modelStationRole).toBool()});
            options.indices.insert(order);
        }
    }
    if(options.indices.isEmpty()){pending_=false;refresh();return;}
    options.layer=layer_->currentData().toInt();
    options.thickness=mode?thickness_->value():0;
    options.closeSheer=cap_->isChecked();
    options.measureShell=property("measure_station_shell").toBool();
    const auto token=cancel_;
    if(!geometryCache_)geometryCache_=std::make_shared<GeometryCache>();
    background(this,[token,options,kit,positions,stock=stock_->value(),cache=geometryCache_,
                     path=inputPath_,id=model_->documentId,revision=model_->revision,
                     imperial=model_->units=="imperial"](TaskResult& result) {
        {
            const std::lock_guard lock(cache->mutex);
            if(token->load())return;
            if(!cache->input){
                QFile f(path);
                if(!f.open(QIODevice::ReadOnly)||f.size()>256*1024*1024)
                    throw std::runtime_error("无法读取当前船模几何。");
                auto input=fs::native_geometry::DocumentInput::read(QJsonDocument::fromJson(f.readAll()).object());
                if(input.documentId!=id || input.revision!=revision || input.imperial!=imperial)
                    throw std::runtime_error("船模版本已更改，请重新打开实尺图。");
                cache->input=std::move(input);
            }
            const auto& input=*cache->input;
            const auto existing=std::find_if(cache->recentCuts.cbegin(),cache->recentCuts.cend(),
                [&](const auto& cut){return cut.options==options&&cut.positions==positions;});
            if(!options.measureShell&&existing!=cache->recentCuts.cend()){
                result.sheets=existing->sheets;
            }else{
                const bool shell=options.thickness>0;
                if(!cache->geometry||cache->layer!=options.layer||cache->shell!=shell){
                    cache->geometry=std::make_unique<frames::StationGeometry>(input,options.layer,shell);
                    cache->layer=options.layer;
                    cache->shell=shell;
                }
                if(token->load())return;
                std::vector<fs::native_geometry::SectionInput> customSections;
                if(!positions.isEmpty()){
                    QHash<int,fs::native_geometry::SectionInput> original;
                    for(const auto& section:input.sections)if(section.kind==1)original.insert(section.order,section);
                    for(const auto& station:positions){
                        if(station.fromModel){
                            const auto section=original.constFind(station.order);
                            if(section==original.cend())throw std::runtime_error("所选横剖站已更改，请重新选择。");
                            customSections.push_back(section.value());
                        }else{
                            const float x=float(station.x/(imperial?304.8:1000.));
                            customSections.push_back({1,station.order,fs::native_geometry::Plane(1,0,0,-x)});
                        }
                    }
                }
                result.sheets=cache->geometry->cut(positions.isEmpty()?input.sections:customSections,options,*token);
                if(!options.measureShell&&!token->load()){
                    cache->recentCuts.prepend({options,positions,result.sheets});
                    if(cache->recentCuts.size()>4)cache->recentCuts.removeLast();
                }
            }
        }
        if(!kit||token->load())return;
        for(int i=1;i<positions.size();++i){
            const auto& previous=positions[i-1];
            const auto& current=positions[i];
            if(current.x>previous.x)continue;
            result.error=QStringLiteral("站位 X 坐标必须从上到下依次增大。");
            result.stationWarnings.insert(previous.order+1,result.error);
            result.stationWarnings.insert(current.order+1,result.error);
        }
        if(!result.error.isEmpty())return;
        if(result.sheets.size()<2){
            result.error=QStringLiteral("至少需要两个站位。可用表头的加号添加站位。");
            return;
        }
        if(std::any_of(result.sheets.cbegin(),result.sheets.cend(),[](const auto& station){return !station.error.isEmpty();}))return;
        try {
            result.sheets=frames::interlockFrames(result.sheets,stock);
        }catch(const frames::StationAssemblyError& e){
            result.stationWarnings.insert(e.number,e.reason);
            if(e.adjacent)result.stationWarnings.insert(e.adjacent,e.reason);
        }
    },[this,token,measure=options.measureShell](TaskResult result){
        if(token!=cancel_) return;
        if(measure){
            const auto polygons=[](const QList<QPolygonF>& list){
                QJsonArray output;
                for(const auto& polygon:list){QJsonArray path;for(auto p:polygon)path.append(QJsonArray{p.x(),p.y()});output.append(path);}
                return output;
            };
            QJsonArray sample;
            for(const auto& s:result.sheets){
                if(!s.shellProfile)continue;
                const auto& p=*s.shellProfile;
                const QJsonObject geometry{{"outline",polygons(s.outlines)},{"diagnostic",polygons(s.diagnosticOutlines)},
                    {"shell",polygons(s.shellReference)},{"error",s.error}};
                const auto fingerprint=QCryptographicHash::hash(QJsonDocument(geometry).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();
                sample.append(QJsonObject{{"station",s.number},{"x",s.x},{"error",s.error},{"geometry_sha256",QString::fromLatin1(fingerprint)},
                    {"triangles",double(p.triangles)},{"bands",double(p.bands)},{"vertices",double(p.vertices)},
                    {"envelope_ms",p.envelopeMs},{"union_ms",p.unionMs},{"difference_ms",p.differenceMs}});
            }
            setProperty("station_shell_profile",sample);
        }
        sheets_=std::move(result.sheets);error_=std::move(result.error);
        stationWarnings_=std::move(result.stationWarnings);pending_=false;
        updateStationWarnings();refresh(animatePreview_);
    });
}
void StationTemplateDialog::updateStationWarnings() {
    const int mode=type_->currentIndex();
    auto warnings=stationWarnings_;
    for(const auto& sheet:sheets_)
        if(!sheet.error.isEmpty())warnings.insert(sheet.number,sheet.error);
    const auto icon=style()->standardIcon(QStyle::SP_MessageBoxWarning);
    {
        const QSignalBlocker block(stations_);
        for(int i=0;i<stations_->count();++i) {
            auto* item=stations_->item(i);
            const auto reason=mode==0?uiText(warnings.value(item->data(Qt::UserRole).toInt()+1)):QString{};
            if(item->toolTip()==reason)continue;
            item->setToolTip(reason);
            item->setIcon(reason.isEmpty()?QIcon{}:icon);
        }
    }
    for(auto* rows:{mould_.rows,kit_.rows}){
        const QSignalBlocker block(rows);
        for(int row=0;row<rows->rowCount();++row){
            const int number=rows->item(row,0)->data(Qt::UserRole).toInt()+1;
            auto* status=rows->item(row,2);
            const auto reason=warnings.value(number);
            const bool active=(mode==1&&rows==mould_.rows)||(mode==2&&rows==kit_.rows);
            const auto tip=active&&!reason.isEmpty()?uiText(reason):QString{};
            if(status->toolTip()==tip)continue;
            status->setToolTip(tip);
            status->setData(Qt::AccessibleTextRole,tip);
            status->setIcon(tip.isEmpty()?QIcon{}:icon);
        }
    }
}
void StationTemplateDialog::refresh(bool animate) {
    const int mode=type_->currentIndex();
    const bool kit=mode==2;
    const auto* rows=mode==0?nullptr:kit?kit_.rows:mould_.rows;
    const auto* row=rows?rows->item(rows->currentRow(),0):nullptr;
    const auto* listed=stations_->currentItem();
    const int current=rows?(row?row->data(Qt::UserRole).toInt()+1:0):
        (listed?listed->data(Qt::UserRole).toInt()+1:0);
    const frames::Station *shown=nullptr,*spine=nullptr,*failed=nullptr;
    for(const auto& s:sheets_) {
        if(s.number==current) shown=&s;
        if(kit && s.number==0) spine=&s;
        if(!s.error.isEmpty() && (!failed || s.number==current)) failed=&s;
    }
    if(!pending_)preview_->display(shown,spine,animate);
    const bool valid=!pending_&&!sheets_.isEmpty()&&!failed&&error_.isEmpty()&&stationWarnings_.isEmpty();
    const bool ready=valid&&!writing_;
    const bool wholeFits=sheets_.size()<=frames::maximumPdfPages;
    dxf_->setEnabled(ready);pdf_->setEnabled(ready&&wholeFits);
    QString status;
    bool warning=false;
    if(writing_) status=uiText("正在导出…");
    else if(pending_) status=uiText("正在生成横剖断面…");
    else if(!error_.isEmpty()) {status=uiText(error_);warning=true;}
    else if(failed) {status=QString(uiText("站 %1：%2")).arg(failed->number).arg(uiText(failed->error));warning=true;}
    else if(!stationWarnings_.isEmpty()){
        const auto found=stationWarnings_.contains(current)?current:*std::min_element(stationWarnings_.keyBegin(),stationWarnings_.keyEnd());
        status=QString(uiText("站 %1：%2")).arg(found).arg(uiText(stationWarnings_.value(found)));
        warning=true;
    }
    else if(sheets_.isEmpty()){
        if(kit)status=uiText("至少需要两个站位。可用表头的加号添加站位。");
        else if(rows)status=uiText("可用表头的加号添加站位。");
        else status=uiText("请选择横剖站。");
    }
    else if(!wholeFits) {status=uiText("PDF 超过 500 页，请减少所选站位。");warning=true;}
    message_->setMessage(status,warning?StudioMessageRole::Warning:StudioMessageRole::Feedback,pending_||writing_);
}
void StationTemplateDialog::configurePdf() {
    const auto generation=cancel_;
    using frames::PdfLayout;
    PdfExportDialog options(this);
    options.setObjectName("StationPdfSettings");
    options.setWindowTitle(QString::fromUtf8(uiText("导出横剖面图"))+QStringLiteral(" · PDF"));
    if(testAttribute(Qt::WA_DontShowOnScreen)) options.setAttribute(Qt::WA_DontShowOnScreen);

    QComboBox layout;
    layout.setObjectName("StationPdfLayout");
    auto* layoutList=new PdfLayoutList;
    layoutList->setMouseTracking(true);
    layout.setView(layoutList);
    layoutList->viewport()->installEventFilter(layoutList);
    const bool chinese=uiLanguage()=="zh";
    const auto tiledLabel=[chinese](const char* trim,const char* chineseLabel){
        return chinese?QString::fromUtf8(chineseLabel):QStringLiteral("%1 (%2)").arg(uiText("A4 拼页"),uiText(trim));
    };
    layout.addItem(tiledLabel("无需裁剪","A4拼页（无需剪裁）"),int(PdfLayout::A4EdgeToEdge));
    layout.setItemData(layout.count()-1,uiText("需要打印机能够同时做到<b>A4 无边距、100% 原尺寸打印</b>"),Qt::ToolTipRole);
    layout.addItem(tiledLabel("需裁剪","A4拼页（需剪裁）"),int(PdfLayout::A4Tiles));
    const auto wholeLabel=QString::fromUtf8(uiText("每个站位一张整图（%1 页）…")).section(u'(',0,0).section(u'（',0,0).trimmed();
    layout.addItem(wholeLabel,int(PdfLayout::WholeStation));
    layout.setCurrentIndex(layout.findData(int(pdfLayout_)));
    options.pageOptions.addRow(uiText("导出模式"),&layout);

    PdfScaleInputs scale(QStringLiteral("StationPdf"));
    scale.numerator.setValue(pdfScale_.numerator);
    scale.denominator.setValue(pdfScale_.denominator);
    options.addScale(&scale);
    options.fit.setChecked(pdfScale_.fit);

    QLabel pages;
    pages.setObjectName("StationPdfPageCount");
    options.scaleOptions.addRow(uiText("页数"),&pages);
    const auto sheets=sheets_;
    const auto selectedLayout=[&]{return PdfLayout(layout.currentData().toInt());};
    const auto selectedScale=[&]{return frames::PdfScale{scale.numerator.value(),scale.denominator.value(),
        options.fit.isChecked()};};
    const auto refreshPages=[&]{
        const auto mode=selectedLayout();const auto ratio=selectedScale();
        const double divisor=frames::pdfDivisor(sheets,ratio,mode);
        scale.showFitRatio(ratio.fit,divisor);
        const int count=frames::pdfPageCount(sheets,ratio,mode);
        const bool fits=count<=frames::maximumPdfPages;
        pages.setText(fits?options.locale().toString(count):uiText("超过 500 页"));
        options.confirm()->setEnabled(fits);
        layout.setToolTip(layout.currentData(Qt::ToolTipRole).toString());
        if(!fits){options.setPageDimensions({});options.preview.clearPage();return;}
        const auto labels=sheetLabels(mode);
        const bool guide=frames::firstPdfIsGuide(sheets.front(),mode,divisor);
        const auto paper=frames::firstPdfPaper(sheets.front(),mode,divisor,ratio.fit);
        options.setPageDimensions(paper);
        const auto caption=ratio.fit?labels.fitScale:QStringLiteral("%1:%2").arg(ratio.numerator,0,'g',15).arg(ratio.denominator,0,'g',15);
        options.preview.showPage(paper,1./divisor,2*int(mode)+int(guide),[&,labels,mode,caption,guide](QPainter& p,QSizeF size,double k){
            frames::paintFirstPdfPage(p,sheets.front(),labels,mode,1./k,size,caption,guide);
        });
    };
    connect(&layout,&QComboBox::currentIndexChanged,&options,refreshPages);
    connect(&options.fit,&QCheckBox::toggled,&options,refreshPages);
    connect(&scale.numerator,&QDoubleSpinBox::valueChanged,&options,refreshPages);
    connect(&scale.denominator,&QDoubleSpinBox::valueChanged,&options,refreshPages);
    refreshPages();
    if(runStudioDialog(options)!=QDialog::Accepted) return;
    if(generation!=cancel_||!pdf_->isEnabled())return;
    pdfLayout_=selectedLayout();pdfScale_=selectedScale();
    save(true,pdfLayout_,pdfScale_);
}
void StationTemplateDialog::save(bool pdf,frames::PdfLayout layout,frames::PdfScale scale) {
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
    picker->setOption(QFileDialog::DontConfirmOverwrite);
    picker->setNameFilter(pdf?"PDF (*.pdf)":"DXF (*.dxf)");
    picker->setDirectory(initial.absolutePath());picker->selectFile(initial.fileName());
    const auto generation=cancel_;
    connect(picker,&QDialog::accepted,this,[this,picker,pdf,layout,scale,generation]{
        const auto path=picker->selectedFiles().value(0);
        if(path.isEmpty()||generation!=cancel_||!(pdf?pdf_:dxf_)->isEnabled())return;
        if(QFileInfo::exists(path)&&!confirmStudioAction(this,uiText("替换文件"),uiText("文件已存在，是否替换？\n")+path))return;
        if(generation==cancel_&&(pdf?pdf_:dxf_)->isEnabled())writeSelection(pdf,layout,scale,path);
    });
    showStudioDialog(*picker);
}
frames::SheetLabels StationTemplateDialog::sheetLabels(frames::PdfLayout layout) const {
    return {type_->currentText(),uiText("站"),uiText("A4 拼接示意（以%1为例）"),
                               uiText("带剪刀的虚线：只剪有剪刀的一侧，另一张保留搭接边。"),
                               uiText("十字：上页压住底页，让一条接缝上的两个十字同时对齐。"),
                               layout==frames::PdfLayout::A4EdgeToEdge?uiText("中心线和基线"):uiText("无剪刀的轴线：中心线和基线，保留纸面。"),
                               uiText("黑色实线：当前站位的轮廓。"),
                               uiText("按编号排列，纸边相接后贴合。"),uiText("中心纵板"),uiText("中心线"),
                               uiText("样板切割线"),uiText("船壳外轮廓"),
                               type_->currentIndex()==2,uiText("基线"),uiText("适合单页")};
}
void StationTemplateDialog::writeSelection(bool pdf,frames::PdfLayout layout,frames::PdfScale scale,const QString& path) {
    if(writing_ || path.isEmpty()) return;
    const QString suffix=pdf?"pdf":"dxf";
    if(QFileInfo(path).suffix().compare(suffix,Qt::CaseInsensitive)!=0) {
        message_->setMessage(uiText("文件扩展名与所选导出格式不一致。"),StudioMessageRole::Warning);return;
    }
    const double t=type_->currentIndex()?thickness_->value():0;
    const auto labels=sheetLabels(layout);
    writing_=true;kitSettings_->setEnabled(false);kit_.panel->setEnabled(false);mould_.panel->setEnabled(false);type_->setEnabled(false);layer_->setEnabled(false);thickness_->setEnabled(false);cap_->setEnabled(false);stations_->setEnabled(false);
    refresh();
    auto token=cancel_;
    background(this,[sheets=sheets_,path,labels,t,pdf,layout,scale,token](TaskResult&){
        if(pdf) frames::writePdf(path,sheets,labels,layout,scale,*token);
        else frames::writeDxf(path,sheets,t,*token);
    },[this,path](TaskResult result){
        writing_=false;kitSettings_->setEnabled(true);kit_.panel->setEnabled(true);mould_.panel->setEnabled(true);type_->setEnabled(true);layer_->setEnabled(true);thickness_->setEnabled(true);cap_->setEnabled(true);stations_->setEnabled(true);
        refresh();
        const bool ok=result.error.isEmpty();
        message_->setMessage(ok?QString(uiText("已保存：%1")).arg(QDir::toNativeSeparators(path)):uiText(result.error),
                             ok?StudioMessageRole::Feedback:StudioMessageRole::Warning);
    });
}

