// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_comparison_dialog.h"
#include "core/station_pairing.h"
#include "core/station_templates.h"
#include "document/geometry_input.h"
#include "render/label_layout.h"
#include "studio_ui.h"
#include "studio_flow_layout.h"
#include "studio_block_button.h"
#include "ui_language.h"
#include <QComboBox>
#include <QFile>
#include "comparison_parameters.h"
#include <QTabWidget>
#include <QToolButton>
#include <QFileDialog>
#include <QPushButton>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPromise>
#include <QSignalBlocker>
#include <QThreadPool>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
std::shared_ptr<const fs::document::Fbm> readModel(const QString& path){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>256LL*1024*1024)throw std::runtime_error("无法读取当前船模几何。");
    return std::make_shared<fs::document::Fbm>(fs::document::Fbm::read(file.readAll()));
}
struct Prepared {
    std::shared_ptr<const fs::document::Fbm> fbm;
    std::shared_ptr<fs::native_geometry::DocumentInput> document;
    QVector<fs::StationReference> groups;
    QString error;
};
struct Comparison {
    QVector<QPolygonF> reference,model;
    std::shared_ptr<const fs::StationPairing> pairing;
    QString error;
};
fs::native_geometry::DocumentInput snapshotGeometry(const QByteArray& bytes,const fs::ModelSnapshot& model){
    auto input=fs::native_geometry::DocumentInput::read(QJsonDocument::fromJson(bytes).object());
    if(input.documentId!=model.documentId || input.revision!=model.revision || input.imperial!=(model.units=="imperial"))
        throw std::runtime_error("船模已更改，请重新打开横剖线对照。");
    return input;
}
Prepared readReference(const ComparisonSource& source,bool full){
    Prepared result;QFile file(source.path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>256LL*1024*1024)throw std::runtime_error("无法读取参考船模。");
    fs::native_geometry::DocumentInput ref;
    if(source.snapshot){
        ref=snapshotGeometry(file.readAll(),*source.snapshot);
        if(full)result.fbm=readModel(source.stateFile);
    }else{
        result.fbm=std::make_shared<fs::document::Fbm>(fs::document::Fbm::read(file.readAll()));
        ref.geometry=fs::document::documentGeometry(*result.fbm);
        ref.imperial=fs::document::documentGeometrySettings(*result.fbm).imperial;
    }
    result.document=std::make_shared<fs::native_geometry::DocumentInput>(std::move(ref));
    return result;
}
template<class Result,class Work,class Done>
void runGeometry(QObject* owner,Work work,Done done) {
    auto promise=std::make_shared<QPromise<Result>>();
    auto* watcher=new QFutureWatcher<Result>(owner);
    QObject::connect(watcher,&QFutureWatcher<Result>::finished,owner,[watcher,done=std::move(done)]() mutable {
        auto result=watcher->result();watcher->deleteLater();done(std::move(result));
    });
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise,work=std::move(work)]{
        promise->start();Result result;
        try {result=work();}
        catch(const std::exception& e){result.error=QString::fromUtf8(e.what());}
        promise->addResult(std::move(result));promise->finish();
    });
}
const QColor modelColor(29,75,105), referenceColor(0,131,110), gapColor(179,73,20);
}

class ComparisonCanvas final : public QWidget {
public:
    Comparison data;
    bool modelReference=false;
    ComparisonCanvas(){setMouseTracking(true);}
    void display(Comparison next){
        clearHover();
        labelBox_={};
        data=std::move(next);
        const auto* d=data.pairing?&data.pairing->maximum:nullptr;
        setProperty("ready",d!=nullptr);
        setProperty("maximumMm",d?QVariant(d->maximumMm):QVariant());
        setProperty("referenceYmm",d?QVariant(d->referenceMm.x()):QVariant());
        setProperty("referenceZmm",d?QVariant(d->referenceMm.y()):QVariant());
        setProperty("modelYmm",d?QVariant(d->modelMm.x()):QVariant());
        setProperty("modelZmm",d?QVariant(d->modelMm.y()):QVariant());
        setAccessibleDescription(d?maximumLabel():QString());
        if(!data.reference.isEmpty()){
            const auto seed=data.reference.first().first();
            bounds_=QRectF(seed,seed);
            for(const auto* paths:{&data.model,&data.reference})for(const auto& path:*paths)for(const auto& pt:path){
                bounds_.setLeft(std::min(bounds_.left(),pt.x()));bounds_.setRight(std::max(bounds_.right(),pt.x()));
                bounds_.setTop(std::min(bounds_.top(),pt.y()));bounds_.setBottom(std::max(bounds_.bottom(),pt.y()));
            }
        }
        update();
    }
protected:
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!data.pairing||!plotArea().contains(event->position())){clearHover();return;}
        const auto plot=plotTransform();
        const auto point=plot.inverted().map(event->position());
        const auto& reference=data.pairing->reference;
        const auto& model=data.pairing->model;
        const auto r=reference.nearest(point);
        const auto& d=data.pairing->maximum;
        const auto gap=d.modelMm-d.referenceMm;
        const double squared=QPointF::dotProduct(gap,gap);
        const double t=squared?std::clamp(QPointF::dotProduct(point-d.referenceMm,gap)/squared,0.,1.):0.;
        const auto delta=point-(d.referenceMm+gap*t);
        const double gapPixels=std::hypot(delta.x(),delta.y())*plot.m11();
        constexpr double hitPixels=7;
        if(gapPixels<=3){
            hover_.reset();
            showHover(event,maximumLabel());
        }else{
            const auto m=model.nearest(point);
            const bool onReference=r.distance<=m.distance;
            const auto& hit=onReference?r:m;
            if(hit.distance*plot.m11()>hitPixels){clearHover();return;}
            const auto pair=data.pairing->pairAt(hit,onReference);
            hover_=QLineF(pair.referenceMm,pair.modelMm);
            showHover(event,QString(uiText("此处偏差 %1 mm")).arg(pair.maximumMm,0,'f',2));
        }
    }
    void leaveEvent(QEvent* event) override {clearHover();QWidget::leaveEvent(event);}
    void resizeEvent(QResizeEvent* event) override {clearHover();QWidget::resizeEvent(event);}
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.fillRect(rect(),Qt::white);p.setRenderHint(QPainter::Antialiasing);
        int legendX=12;
        const auto legend=[&](QColor color,Qt::PenStyle style,const QString& text){
            const int y=9+fontMetrics().height()/2;
            p.setPen(QPen(color,2,style));p.drawLine(legendX,y,legendX+25,y);
            p.setPen(QColor(39,54,63));p.drawText(legendX+31,y+(fontMetrics().ascent()-fontMetrics().descent())/2,text);
            legendX+=44+fontMetrics().horizontalAdvance(text);
        };
        legend(modelColor,Qt::SolidLine,uiText("船体横剖线"));
        legend(referenceColor,Qt::DashLine,modelReference?uiText("参考船模"):uiText("参考曲线"));
        if(data.reference.isEmpty())return;
        const QRectF area=plotArea();
        if(area.isEmpty())return;
        const auto plot=plotTransform();
        if(property("contourToCanvas").value<QTransform>()!=plot)setProperty("contourToCanvas",QVariant::fromValue(plot));
        p.setClipRect(QRectF(rect()).adjusted(0,18+fontMetrics().height(),0,0));
        p.save();p.setWorldTransform(plot);
        auto draw=[&](const QVector<QPolygonF>& lines,QColor color,Qt::PenStyle style){
            QPen pen(color,2,style);pen.setCosmetic(true);p.setPen(pen);
            for(const auto& path:lines)p.drawPolyline(path);
        };
        draw(data.model,modelColor,Qt::SolidLine);
        // Continue the dash phase through separate reference paths. Imported
        // polylines can consist of many two-point records at the same station.
        QPen referencePen(referenceColor,2,Qt::CustomDashLine,Qt::FlatCap);
        referencePen.setCosmetic(true);
        referencePen.setDashPattern({4.,2.});
        double dashPhase=0;
        for(const auto& path:data.reference){
            referencePen.setDashOffset(dashPhase);p.setPen(referencePen);
            p.drawPolyline(path);
            for(qsizetype i=1;i<path.size();++i){
                const auto segment=path[i]-path[i-1];
                dashPhase=std::fmod(dashPhase+std::hypot(segment.x(),segment.y())*plot.m11()/2.,6.);
            }
        }
        p.restore();
        if(!data.pairing)return;
        const auto& d=data.pairing->maximum;
        if(hover_){
            p.setPen(QPen(QColor(179,73,20,190),1.3,Qt::DashLine));p.setBrush(Qt::NoBrush);
            const auto line=plot.map(*hover_);
            p.drawLine(line);p.drawEllipse(line.p1(),2,2);p.drawEllipse(line.p2(),2,2);
        }
        const auto r=plot.map(d.referenceMm),m=plot.map(d.modelMm);
        p.setPen(QPen(Qt::white,6));p.drawLine(r,m);p.setPen(QPen(gapColor,2.8));p.drawLine(r,m);
        p.setBrush(Qt::NoBrush);p.drawEllipse(r,6,6);p.setBrush(gapColor);p.drawEllipse(m,2,2);
        const auto anchor=(r+m)/2;
        const auto text=maximumLabel();
        const auto box=labelBox(plot,r,m,text);
        const QPointF end(std::clamp(anchor.x(),box.left(),box.right()),std::clamp(anchor.y(),box.top(),box.bottom()));
        p.setPen(QPen(Qt::white,4));p.drawLine(anchor,end);
        p.setPen(QPen(gapColor,1.2));p.drawLine(anchor,end);
        p.fillRect(box,Qt::white);p.drawText(box,Qt::AlignCenter,text);
    }
private:
    QRectF labelBox(const QTransform& plot,QPointF r,QPointF m,const QString& text){
        if(!labelBox_.isEmpty()&&labelCanvasSize_==size()&&labelFont_==font()&&labelText_==text)return labelBox_;
        const QFontMetricsF metrics(font());
        const QSizeF size(metrics.horizontalAdvance(text)+10,metrics.height()+6);
        const auto anchor=(r+m)/2;
        const auto labelArea=QRectF(rect()).adjusted(6,22+fontMetrics().height(),-6,-6);
        fs::presentation::LabelLayout placement;
        placement.begin(labelArea,0,false);
        for(const auto* paths:{&data.model,&data.reference})for(const auto& path:*paths)
            for(qsizetype i=1;i<path.size();++i)placement.segment(QLineF(plot.map(path[i-1]),plot.map(path[i])));
        placement.reserve(QRectF(r,m).normalized().adjusted(-6,-6,6,6));
        const auto preferred=anchor+QPointF(-size.width()/2,19);
        const auto label=placement.place(QStringLiteral("maximumDeviation"),text,anchor,preferred,size,
                                         fs::presentation::LabelKind::Point);
        auto position=label.position;
        if(!label.visible||label.clip.size()!=size)
            position={std::clamp(preferred.x(),labelArea.left(),std::max(labelArea.left(),labelArea.right()-size.width())),
                      std::clamp(preferred.y(),labelArea.top(),std::max(labelArea.top(),labelArea.bottom()-size.height()))};
        labelCanvasSize_=this->size();labelFont_=font();labelText_=text;
        labelBox_=QRectF(position,size);
        return labelBox_;
    }
    QString maximumLabel() const {return QString(uiText("最大偏差 ≈ %1 mm")).arg(data.pairing->maximum.maximumMm,0,'f',2);}
    QRectF plotArea() const {return QRectF(rect()).adjusted(15,26+fontMetrics().height(),-15,-35);}
    QTransform plotTransform() const {
        const auto area=plotArea();
        const double scale=std::min(area.width()/std::max(.01,bounds_.width()),area.height()/std::max(.01,bounds_.height()));
        QTransform plot;
        plot.translate(area.center().x(),area.center().y());plot.scale(scale,-scale);
        plot.translate(-bounds_.center().x(),-bounds_.center().y());
        return plot;
    }
    void showHover(QMouseEvent* event,const QString& text){
        tip_=text;
        QToolTip::showText(event->globalPosition().toPoint(),text,this);
        update();
    }
    void clearHover(){
        if(tip_.isEmpty())return;
        QToolTip::hideText();tip_.clear();hover_.reset();update();
    }
    QRectF bounds_;
    QRectF labelBox_;
    QSize labelCanvasSize_;
    QFont labelFont_;
    QString labelText_;
    QString tip_;
    std::optional<QLineF> hover_;
};

StationComparisonDialog::StationComparisonDialog(std::shared_ptr<const fs::ModelSnapshot> model,
        QString inputPath,int preferredStation,QWidget* parent,QVector<ComparisonSource> sources,bool compareModels,QString stateFile,QString currentName)
        :QDialog(parent),currentName_(std::move(currentName)),referenceSnapshot_(model),
         preferredStation_(preferredStation),models_(std::move(sources)){
    setObjectName("StationComparison");setWindowTitle(compareModels?uiText("比较船模"):uiText("横剖线对照"));setStudioWindowIcon(this,"Sections");
    if(!compareModels)setProperty("studio_caption_maximize",true);
    resize(compareModels?880:735,compareModels?650:515);
    auto* layout=new QVBoxLayout(this);
    auto* sourceBar=new QHBoxLayout;layout->addLayout(sourceBar);
    if(compareModels){
        auto* chipPanel=new QWidget;
        chipPanel->setObjectName("ComparisonSourceChips");
        QSizePolicy policy(QSizePolicy::Expanding,QSizePolicy::Fixed);policy.setHeightForWidth(true);
        chipPanel->setSizePolicy(policy);
        chips_=new StudioFlowLayout;chipPanel->setLayout(chips_);
        sourceBar->addWidget(chipPanel,1);
        browse_=new QPushButton;browse_->setObjectName("ComparisonBrowse");
        const auto refreshBrowse=[button=browse_]{
            setStudioFormatButton(button,"FBM",uiText("从文件选择船模…"));
        };
        refreshBrowse();bindUiRefresh(browse_,refreshBrowse);
        sourceBar->addWidget(browse_,0,Qt::AlignTop);browse_->setEnabled(false);
        connect(browse_,&QPushButton::clicked,this,[this]{
            const auto files=studioOpenFiles(this,uiText("打开船模"),{},QString(uiText("船模文件"))+" (*.fbm)");
            bool added=false;
            for(const auto& file:files){
                const auto existing=std::find_if(models_.cbegin(),models_.cend(),[&](const auto& source){
                    return !source.snapshot && QFileInfo(source.path)==QFileInfo(file);
                });
                if(existing!=models_.cend()){activeSource_=int(existing-models_.cbegin())+1;continue;}
                models_.append({QFileInfo(file).fileName(),file,{},{}});
                activeSource_=models_.size();added=true;
            }
            if(added){rebuildSourceChips();loadModelSources();}
            else if(!files.isEmpty())changeSource(activeSource_);
        });
        if(!models_.isEmpty())activeSource_=1;
        rebuildSourceChips();
    }else{
        sourceBar->addWidget(new QLabel(uiText("对照来源")));
        sources_=new QComboBox;sources_->setObjectName("ComparisonSource");sourceBar->addWidget(sources_,1);
        sources_->addItem(uiText("参考曲线"));
        for(const auto& source:models_)sources_->addItem(source.label);
        sources_->setEnabled(false);
        connect(sources_,&QComboBox::currentIndexChanged,this,&StationComparisonDialog::changeSource);
    }
    QTabWidget* tabs=nullptr;
    if(compareModels){
        tabs=new QTabWidget;tabs->setObjectName("ModelComparisonTabs");layout->addWidget(tabs,1);
        parameters_=new ComparisonParameters(currentName_,model->draft*(model->units=="imperial"?.3048:1.));
        tabs->addTab(parameters_,uiText("船模参数"));
        auto* sectionPage=new QWidget;tabs->addTab(sectionPage,uiText("横剖线对照"));layout=new QVBoxLayout(sectionPage);
    }
    auto* bar=new QHBoxLayout;layout->addLayout(bar);
    bar->addWidget(new QLabel(uiText("横剖线")));station_=new QComboBox;station_->setObjectName("ComparisonStation");bar->addWidget(station_);
    bar->addStretch();
    canvas_=new ComparisonCanvas;canvas_->setObjectName("ComparisonCanvas");canvas_->setMinimumSize(300,190);layout->addWidget(canvas_,1);
    status_=new QLabel;status_->setWordWrap(true);status_->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(status_);
    if(tabs)connect(tabs,&QTabWidget::currentChanged,this,[this](int){compareSelected();});
    connect(station_,&QComboBox::currentIndexChanged,this,[this]{if(station_->isEnabled())compareSelected();});
    readCurrentHull(std::move(model),std::move(inputPath),std::move(stateFile));
}
void StationComparisonDialog::updateCurrentHull(std::shared_ptr<const fs::ModelSnapshot> model,
                                               const QString& path,const QString& stateFile){
    if(model->documentId!=referenceSnapshot_->documentId){close();return;}
    if(requestedModel_&&requestedModel_->revision==model->revision)return;
    readCurrentHull(std::move(model),path,stateFile);
}
void StationComparisonDialog::setStations(const fs::ModelSnapshot& model){
    const bool imperial=model.units=="imperial";
    const double factor=imperial?304.8:1000.;
    stationItems_.clear();stationPositions_.clear();
    for(const auto& s:model.sections)if(s.kind==1){
        const auto label=QStringLiteral("%1    X = %2 %3").arg(s.index+1).arg(s.distance,0,'g',9).arg(imperial?"ft":"m");
        stationItems_.append({label,s.index});
        stationPositions_.append(s.distance*factor);
    }
}
void StationComparisonDialog::readCurrentHull(std::shared_ptr<const fs::ModelSnapshot> model,QString path,QString stateFile){
    if(const auto current=station_->currentData(Qt::UserRole+1);current.isValid())preferredX_=current;
    if(preparing_)preparing_->store(true);
    if(comparing_)comparing_->store(true);
    requestedModel_=model;
    document_.reset();
    station_->setEnabled(false);
    if(sources_)sources_->setEnabled(false);
    if(browse_)browse_->setEnabled(false);
    showStatus(uiText("正在读取横剖线…"));
    preparing_=std::make_shared<std::atomic_bool>(false);
    runGeometry<Prepared>(this,[model,path=std::move(path),full=bool(parameters_),stateFile,
                               reference=referenceSnapshot_,token=preparing_]{
        Prepared result;
        if(full)result.fbm=readModel(stateFile);
        else result.groups=fs::stationReferences(reference->markers,reference->units=="imperial"?304.8:1000.,token.get());
        if(token->load())return result;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)||file.size()>256LL*1024*1024)throw std::runtime_error("无法读取当前船模几何。");
        auto input=snapshotGeometry(file.readAll(),*model);
        result.document=std::make_shared<fs::native_geometry::DocumentInput>(std::move(input));return result;
    },[this,model,token=preparing_](Prepared result){
        if(token->load())return;
        if(!result.error.isEmpty()){showStatus(uiText(result.error),true);if(parameters_)parameters_->setModels({{currentName_,{},result.error}});return;}
        currentFbm_=std::move(result.fbm);
        document_=std::move(result.document);
        setStations(*model);
        if(parameters_){
            browse_->setEnabled(true);refreshStations();
            if(loadedSources_.isEmpty())loadModelSources();
            else {updateParameterModels();compareSelected();}
            return;
        }
        references_.clear();
        // Match each group once, including when neighbouring stations are unusually close.
        QVector<std::pair<double,int>> positions;
        for(qsizetype row=0;row<stationItems_.size();++row)positions.append({stationPositions_[row],stationItems_[row].second});
        std::sort(positions.begin(),positions.end());
        for(auto& group:result.groups){
            const auto it=std::lower_bound(positions.cbegin(),positions.cend(),group.xMm,[](const auto& a,double x){return a.first<x;});
            auto best=it;
            if(it!=positions.cbegin() && (it==positions.cend() || group.xMm-(it-1)->first < it->first-group.xMm))best=it-1;
            if(best==positions.cend())continue;
            const auto distance=[&](auto p){return std::abs(p->first-group.xMm);};
            if(distance(best)>fs::stationMatchToleranceMm ||
               (best!=positions.cbegin() && std::abs(distance(best-1)-distance(best))<1e-9) ||
               (best+1!=positions.cend() && std::abs(distance(best+1)-distance(best))<1e-9))continue;
            auto found=references_.find(best->second);
            if(found==references_.end())references_.insert(best->second,std::move(group));
            else {found->linesMm+=group.linesMm;if(found->side!=group.side)found->side=fs::ReferenceSide::Both;}
        }
        sources_->setEnabled(true);
        if(references_.isEmpty()&&!models_.isEmpty()&&activeSource_==0)sources_->setCurrentIndex(1);
        else {refreshStations();compareSelected();}
    });
}
StationComparisonDialog::~StationComparisonDialog(){
    preparing_->store(true);if(comparing_)comparing_->store(true);
    if(loadingSource_)loadingSource_->store(true);
}
void StationComparisonDialog::rebuildSourceChips(){
    while(auto* item=chips_->takeAt(0)){
        item->widget()->hide();item->widget()->deleteLater();delete item;
    }
    for(int i=0;i<models_.size();++i){
        const auto& source=models_[i];
        auto* chip=new StudioBlockButton;
        chip->setObjectName("ComparisonModelChip");
        chip->setStyleSheet(
            "QToolButton#ComparisonModelChip {background:#e5efea;border:1px solid #bfd4cb;"
            "border-radius:4px;color:#225e55;padding:5px 10px}"
            "QToolButton#ComparisonModelChip:hover,QToolButton#ComparisonModelChip:checked {background:#d0e4da}");
        chip->setProperty("source_index",i+1);
        chip->setText(source.label);
        chip->setToolTip(source.snapshot?source.label:source.path);
        chip->setCheckable(true);chip->setChecked(activeSource_==i+1);
        connect(chip,&QToolButton::clicked,this,[this,i]{changeSource(i+1);});
        chip->onRemove=[this,i]{
            models_.removeAt(i);
            if(activeSource_==i+1)activeSource_=std::min(i+1,int(models_.size()));
            else if(activeSource_>i+1)--activeSource_;
            rebuildSourceChips();loadModelSources();
        };
        chips_->addWidget(chip);
    }
}
void StationComparisonDialog::loadModelSources(){
    if(loadingSource_)loadingSource_->store(true);
    if(comparing_)comparing_->store(true);
    loadedSources_.clear();referenceDocument_.reset();canvas_->display({});
    QVector<ComparisonHull> hulls{{currentName_,currentFbm_,{}}};
    for(const auto& source:models_)hulls.append({source.label,{},{}});
    parameters_->setModels(hulls,!models_.isEmpty());
    if(models_.isEmpty()){showStatus({});return;}
    showStatus(uiText("正在读取横剖线…"));
    struct Loaded {QVector<LoadedSource> sources;QString error;};
    loadingSource_=std::make_shared<std::atomic_bool>(false);
    runGeometry<Loaded>(this,[sources=models_,token=loadingSource_]{
        Loaded result;
        for(const auto& source:sources){
            if(token->load())break;
            try{
                auto prepared=readReference(source,true);
                result.sources.append({std::move(prepared.document),std::move(prepared.fbm),{}});
            }catch(const std::exception& e){result.sources.append({{},{},QString::fromUtf8(e.what())});}
        }
        return result;
    },[this,hulls=std::move(hulls),token=loadingSource_](Loaded result) mutable {
        if(token->load())return;
        if(!result.error.isEmpty()){
            showStatus(uiText(result.error),true);
            hulls.first().error=result.error;
            hulls.first().model.reset();
        }else{
            loadedSources_=std::move(result.sources);
            for(int i=0;i<loadedSources_.size();++i){
                hulls[i+1].model=loadedSources_[i].model;
                hulls[i+1].error=loadedSources_[i].error;
            }
        }
        parameters_->setModels(std::move(hulls));
        if(result.error.isEmpty())changeSource(activeSource_);
    });
}
void StationComparisonDialog::updateParameterModels(){
    QVector<ComparisonHull> hulls{{currentName_,currentFbm_,{}}};
    for(qsizetype i=0;i<loadedSources_.size();++i)
        hulls.append({models_[i].label,loadedSources_[i].model,loadedSources_[i].error});
    parameters_->setModels(std::move(hulls));
}
void StationComparisonDialog::refreshStations(){
    if(const auto current=station_->currentData(Qt::UserRole+1);current.isValid())preferredX_=current;
    const QSignalBlocker block(station_);
    station_->clear();
    int selected=-1;
    for(qsizetype i=0;i<stationItems_.size();++i){
        const auto& [label,id]=stationItems_[i];
        if(activeSource_<=0&&!references_.contains(id))continue;
        station_->addItem(label,id);
        const int row=station_->count()-1;
        const auto x=stationPositions_[i];
        station_->setItemData(row,x,Qt::UserRole+1);
        if(preferredX_.isValid()?std::abs(x-preferredX_.toDouble())<=fs::stationMatchToleranceMm:id==preferredStation_)
            selected=row;
    }
    if(selected>=0)station_->setCurrentIndex(selected);
    station_->setEnabled(station_->count()>0);
}
void StationComparisonDialog::changeSource(int row){
    if(comparing_)comparing_->store(true);
    activeSource_=row;referenceDocument_.reset();canvas_->display({});
    canvas_->modelReference=row>0;
    if(parameters_){
        for(auto* chip:chips_->parentWidget()->findChildren<QToolButton*>("ComparisonModelChip"))
            chip->setChecked(chip->property("source_index").toInt()==row);
        if(row<=0){showStatus({});return;}
        refreshStations();
        if(row>loadedSources_.size())return;
        const auto& source=loadedSources_[row-1];
        if(!source.error.isEmpty()){showStatus(uiText(source.error),true);return;}
        referenceDocument_=source.document;
        compareSelected();
        return;
    }
    if(loadingSource_)loadingSource_->store(true);
    refreshStations();
    if(row<=0){compareSelected();return;}
    loadingSource_=std::make_shared<std::atomic_bool>(false);
    showStatus(uiText("正在读取横剖线…"));
    runGeometry<Prepared>(this,[source=models_.at(row-1)]{
        return readReference(source,false);
    },[this,token=loadingSource_](Prepared result){
        if(token->load())return;
        if(!result.error.isEmpty()){showStatus(uiText(result.error),true);return;}
        referenceDocument_=std::move(result.document);compareSelected();
    });
}
void StationComparisonDialog::showStatus(const QString& text,bool error){
    canvas_->setProperty("error",error?text:QString());status_->setText(text);
    status_->setVisible(!text.isEmpty());
}
void StationComparisonDialog::compareSelected(){
    if(comparing_)comparing_->store(true);
    if(!document_)return;
    canvas_->display({});
    if(parameters_){
        if(!activeSource_) {showStatus({});return;}
        if(!parameters_->isHidden())return;
        if(!station_->count()){showStatus(uiText("请选择横剖站。"));return;}
    }
    if(!activeSource_&&station_->currentIndex()<0){
        showStatus(uiText("没有与横剖站匹配的可见参考曲线。"),true);return;
    }
    const int index=station_->currentData().toInt();
    const auto found=references_.constFind(index);
    if(activeSource_ && !referenceDocument_)return;
    if(!activeSource_ && (found==references_.cend() || !document_)){
        showStatus(uiText("此站没有可见的参考曲线。请选择参考船模，或导入此站的参考曲线。"),true);return;
    }
    showStatus(uiText("正在读取横剖线…"));
    comparing_=std::make_shared<std::atomic_bool>(false);
    const auto ref=activeSource_?fs::StationReference{}:*found;
    runGeometry<Comparison>(this,[index,ref,other=referenceDocument_,doc=document_,token=comparing_]{
        Comparison result;fs::frames::Options options;options.indices.insert(index);
        auto stations=fs::frames::sectionTemplates(*doc,options,*token);
        if(token->load())return result;
        if(stations.isEmpty())throw std::runtime_error("请选择横剖站。");
        if(!stations.first().error.isEmpty()){result.error=stations.first().error;return result;}
        if(other){
            const double units=other->imperial?304.8:1000.;
            const fs::native_geometry::SectionInput section{1,index,{1,0,0,float(-stations.first().x/units)}};
            auto cuts=fs::frames::sectionTemplatesAt(*other,{section},options,*token);
            if(token->load())return result;
            if(cuts.isEmpty()||!cuts.first().error.isEmpty()||cuts.first().outlines.isEmpty()){
                result.error=cuts.isEmpty()||cuts.first().error.isEmpty()?QStringLiteral("参考船模在此位置没有横剖线。"):cuts.first().error;return result;
            }
            result.reference=std::move(cuts.first().outlines);
        }else result.reference=ref.linesMm;
        result.model=fs::clipStationToReferenceSide(stations.first().outlines,ref.side,token.get());
        const auto sides=[&](const QVector<QPolygonF>& paths)->QVector<QVector<QPolygonF>>{
            if(ref.side!=fs::ReferenceSide::Both)return {paths};
            return {fs::clipStationToReferenceSide(paths,fs::ReferenceSide::Negative,token.get()),
                    fs::clipStationToReferenceSide(paths,fs::ReferenceSide::Positive,token.get())};
        };
        auto pairing=fs::pairStationContours(sides(result.reference),sides(result.model),token.get());
        if(pairing)result.pairing=std::make_shared<fs::StationPairing>(std::move(*pairing));
        else result.error=QStringLiteral("无法配对两条轮廓。请检查两侧轮廓、折角和端点是否对应。");
        return result;
    },[this,side=ref.side,token=comparing_,revision=document_->revision](Comparison result){
        if(token->load())return;
        if(!result.pairing){
            const auto message=result.error.isEmpty()?uiText("该站没有可比较的船体轮廓。"):uiText(result.error);
            canvas_->display(std::move(result));
            showStatus(message,true);return;
        }
        const QString scope=side==fs::ReferenceSide::Both?QString():side==fs::ReferenceSide::Positive?"   (Y ≥ 0)":"   (Y ≤ 0)";
        showStatus(scope);
        canvas_->display(std::move(result));
        canvas_->setProperty("comparison_revision",revision);
    });
}
