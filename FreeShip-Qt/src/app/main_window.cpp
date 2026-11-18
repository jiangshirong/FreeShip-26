#include "app/ui_language.h"
#include "main_window.h"
#include "section_dialog.h"
#include "history_shortcuts.h"
#include "project_dialog.h"
#include "layer_panel.h"
#include "studio_ui.h"
#include "studio_caption.h"
#include "point_coordinate_popup.h"
#include "studio_view_grid.h"
#include "hydro_dialog.h"
#include "hydro_task_ball.h"
#include "studio_motion.h"
#include "performance_dialog.h"
#include "plates_dialog.h"
#include "exchange_dialog.h"
#include "core/cad_export.h"
#include "render/model_preview.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QSplitter>
#include "data_motion.h"
#include "message_motion.h"
#include <QStatusBar>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

namespace {
QByteArray contents(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read verification evidence");
    return file.readAll();
}
bool samePoint(fs::Vec3 a, fs::Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
class CoordinateEditor final : public QDoubleSpinBox {
protected:
    QString textFromValue(double value) const override {
        QString text = QDoubleSpinBox::textFromValue(value);
        if (text.contains(locale().decimalPoint())) {
            while (text.endsWith('0')) text.chop(1);
            if (text.endsWith(locale().decimalPoint())) text.chop(locale().decimalPoint().size());
        }
        return text;
    }
};
}

MainWindow::MainWindow(QWidget* parent,bool rememberFiles) : QMainWindow(parent), session_(this) {
    session_.setProperty("defer_startup_drawing",rememberFiles);
    if(rememberFiles)fileHistory_=std::make_unique<FileHistory>(FileHistory::defaultFilename());
    if(rememberFiles){
        session_.setRecoveryRoot(fs::defaultRecoveryRoot());
        if(!fs::defaultRecoveryRoot().isEmpty())session_.setLinesCacheRoot(QFileInfo(fs::defaultRecoveryRoot()).dir().filePath("linesplan-cache-v1"));
    }
    applyStudioTheme();
    enableStudioMinimizeCaption(this);
    setStudioWindowIcon(this,"brand");
    setWindowTitle(uiText("未命名船模")); resize(1440, 960);
    // Keep a hidden diagnostic channel for the existing workflow observers.
    // It takes no layout space; user-facing feedback is a short-lived overlay.
    statusBar()->setSizeGripEnabled(false);statusBar()->hide();
    auto* file = primaryMenu_ = new QMenu(this);
    open_ = file->addAction(uiText("打开模型…"), QKeySequence::Open, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, uiText("打开船模"), preferenceFileLocation("open",session_.sourcePath()), uiText("FreeShip 船模 (*.fbm *.ftm)"));
        if (!path.isEmpty()) openModel(path);
    });
    save_ = file->addAction(uiText("保存"), QKeySequence::Save, this, [this] { saveModel(); });
    saveAs_ = file->addAction(uiText("另存副本"), QKeySequence::SaveAs, this, [this] { saveModel(true); });
    for(auto* action:{open_,saveAs_})action->setProperty("studio_opens_dialog",true);
    open_->setProperty("file_operation","open");save_->setProperty("file_operation","save");saveAs_->setProperty("file_operation","save_as");
    for(auto* action:{open_,save_,saveAs_})action->setProperty("shortcut_target","当前船模");
    recentMenu_=file->addMenu(uiText("最近打开"));
    recentMenu_->setIcon(studioIcon("History"));saveAs_->setIcon(studioIcon("SaveAs"));
    connect(recentMenu_,&QMenu::aboutToShow,this,&MainWindow::refreshRecentFiles);
    refreshRecentFiles();
    auto* recover=file->addAction(uiText("恢复未保存的船模…"),this,[this]{showRecoveries();});
    recover->setProperty("studio_opens_dialog",true);
    recover->setIcon(studioIcon("Restore"));
    recover->setProperty("file_operation","recover");
    auto* resume=file->addAction(uiText("恢复当前工作"),this,[this]{session_.resumeCurrent();});
    resume->setProperty("file_operation","resume_current");
    resume->setIcon(studioIcon("Restore"));
    resume->setEnabled(false);
    for(bool importing:{true,false}){
        auto* menu=file->addMenu(importing?uiText("导入模型"):uiText("导出模型"));
        menu->setIcon(studioIcon(importing?"ImportFile":"ExportFile"));
        menu->menuAction()->setProperty("file_operation",importing?"import_model":"export_model");
        QStringList formats{"obj","stl","fef"};if(importing)formats<<"polycad"<<"hull"<<"carene"<<"chines"<<"surface"<<"vrml"<<"part"<<"waves";else formats<<"iges"<<"dxf3d"<<"dxfmesh"<<"offsets"<<"coordinates"<<"ghs"<<"arch_hll"<<"arch_app"<<"pam"<<"part"<<"michlet"<<"addmass"<<"offset_table"<<"dxfcrease";
        for(const QString& format:formats){
            auto* a=menu->addAction(fs::cad::title(format)+"…",this,[this,importing,format]{openExchange(importing,format);});
            setStudioFormatMenuAction(menu,a,fs::cad::extension(format),importing);
            a->setProperty("operation",(importing?"import_":"export_")+format);a->setProperty("importing",importing);exchangeActions_.append(a);
        }
    }
    auto* quit = new QShortcut(QKeySequence::Quit, this);
    quit->setProperty("shortcut_source","退出程序");
    quit->setProperty("shortcut_target","程序");
    connect(quit, &QShortcut::activated, this, &QWidget::close);
    undo_ = new QAction(uiText("撤销"), this);
    redo_ = new QAction(uiText("重做"), this);
    connect(undo_, &QAction::triggered, this, [this] { session_.undo(); });
    connect(redo_, &QAction::triggered, this, [this] { session_.redo(); });
    bindHistoryShortcut(this, undo_, QKeySequence(Qt::CTRL | Qt::Key_Z));
    bindHistoryShortcut(this, redo_, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    project_ = new QAction(studioIcon("ProjectSettings"), uiText("工程设置"), this);
    project_->setProperty("studio_opens_dialog",true);
    project_->setProperty("document_operation","project_settings");
    connect(project_, &QAction::triggered, this, &MainWindow::editProject);
    auto* fit = new QShortcut(QKeySequence("F"), this);
    fit->setProperty("shortcut_source","缩放至全图");
    fit->setProperty("shortcut_target","全部四个视图");
    connect(fit, &QShortcut::activated, this, &MainWindow::fitCurrent);
    preferencesAction_ = new QAction(studioIcon("Preferences"), uiText("首选项"), this);
    preferencesAction_->setProperty("studio_opens_dialog",true);
    preferencesAction_->setProperty("operation", "preferences");
    connect(preferencesAction_, &QAction::triggered, this, &MainWindow::editPreferences);
    languageShortcut_=new QShortcut(qApp);
    languageShortcut_->setKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    languageShortcut_->setContext(Qt::ApplicationShortcut);
    languageShortcut_->setAutoRepeat(false);
    languageShortcut_->setEnabled(false);
    languageShortcut_->setProperty("shortcut_source","切换语言 / Language");
    languageShortcut_->setProperty("shortcut_target","界面语言");
    connect(languageShortcut_,&QShortcut::activated,this,&MainWindow::switchUiLanguage);
    tabs_ = new QTabWidget; tabs_->setDocumentMode(true);
    tabs_->tabBar()->hide();
    auto* hulls = new QWidget; auto* grid = new QGridLayout(hulls);
    viewGrid_=grid;grid->setContentsMargins(0,0,0,0);grid->setSpacing(2);
    const std::array<HullProjection, 4> projections{
        HullProjection::Profile, HullProjection::Body, HullProjection::Plan, HullProjection::Perspective};
    for (size_t i = 0; i < views_.size(); ++i) {
        auto* frame = new QFrame;viewFrames_[i]=frame;frame->setFrameShape(QFrame::NoFrame);
        auto* layout = new QVBoxLayout(frame); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
        auto* view = new HullView; views_[i] = view; view->setProjection(projections[i]); view->setMinimumSize(200, 160);
        view->setStudioAppearance(true);view->setFillVisible(false);
        layout->addWidget(view, 1); grid->addWidget(frame, int(i / 2), int(i % 2));
        view->onSelectionChanged = [this](QSet<fs::EntityId> ids) { selectPoints(std::move(ids)); };
        view->onPointsMoved = [this](QVector<fs::PointMove> moves) { session_.movePoints(moves); };
        view->onEditRejected = [this](const QString& message) { showStatusMessage(message,StudioMessageRole::Warning); };
        view->onEntitySelectionChanged=[this](HullEntity kind,QSet<fs::EntityId> ids){selectEntities(kind,std::move(ids));};
        view->onBackgroundAction=[this,view](const QString& operation,QPointF position){backgroundAction(view,operation,position);};
        view->onBackgroundChanged=[this,view](const QJsonObject& changes){
            if(view->snapshot()==session_.model()&&session_.ready()&&!session_.busy())session_.editBackground(view->backgroundView(),"update",{{"changes",changes}});
        };
        view->onFlowPlacementEnded=[this]{endFlowline();};
        view->onPointPlacementEnded=[this]{endPointPlacement();};
        view->onPointCreationRequested=[this,view](fs::Vec3 p){
            if(!session_.ready()||session_.busy())return;
            pendingNewPointAfter_=1;pendingPointView_=view;pendingPointIds_.clear();
            for(const auto& point:session_.model()->points)pendingPointIds_.insert(point.id);
            if(!session_.editTopology("add_point",{{"position",QJsonArray{p.x,p.y,p.z}}})){pendingNewPointAfter_=0;pendingPointView_.clear();}
        };
        view->onViewAction=[this,view](const QString& action){viewAction(view,action);};
        view->onViewSettingsChanged=[this,view]{refreshLightDialog(view);};
        view->onFlowRequested=[this](int projection,QPointF source){session_.editAuxiliary("create_flow",{{"view",projection},{"source",QJsonArray{source.x(),source.y()}}});};
    }
    studio_view_grid::install(grid,{viewFrames_[0],viewFrames_[1],viewFrames_[2],viewFrames_[3]});
    tabs_->addTab(hulls, uiText("四视图"));
    linesDialog_=new QDialog(this,Qt::Window|Qt::WindowCloseButtonHint);
    setStudioWindowIcon(linesDialog_,"LinesPlan");
    linesDialog_->setObjectName("LinesPlanWindow");linesDialog_->setWindowTitle(uiText("型线图"));
    linesDialog_->setModal(false);linesDialog_->resize(1100,760);
    auto* linesLayout=new QVBoxLayout(linesDialog_);linesLayout->setContentsMargins(0,0,0,0);linesLayout->setSpacing(0);
    auto* linesTools=new QToolBar(linesDialog_);linesTools->setIconSize(QSize(44,24));
    linesTools->setToolButtonStyle(Qt::ToolButtonIconOnly);linesLayout->addWidget(linesTools);
    lines_=new LinesView(linesDialog_);linesLayout->addWidget(lines_,1);
    lines_->setReferenceCaptions(uiText("基线 %1"),uiText("中心线"));
    auto* fill=linesTools->addAction(studioIcon("ShowFillColor"),uiText("填色"));fill->setCheckable(true);fill->setChecked(true);
    connect(fill,&QAction::toggled,lines_,&LinesView::setFill);
    if(auto* button=qobject_cast<QToolButton*>(linesTools->widgetForAction(fill))){
        button->setObjectName("LinesFillToggle");
        button->setIconSize(QSize(24,24));
        button->setStyleSheet(
            "QToolButton#LinesFillToggle {background:transparent; border:1px solid transparent; border-radius:5px; padding:3px 6px;}"
            "QToolButton#LinesFillToggle:hover {background:#f0f7f3;}"
            "QToolButton#LinesFillToggle:checked {background:#e5f2ec; border-color:#a2c5b4;}"
            "QToolButton#LinesFillToggle:checked:hover {background:#d8ede3;}"
            "QToolButton#LinesFillToggle:pressed {background:#cde7db;}"
            "QToolButton#LinesFillToggle:focus {border-color:#5b957f;}");
    }
    linesTools->addSeparator();
    linesPng_ = linesTools->addAction(studioFormatIcon("png"), "PNG", this, &MainWindow::exportLinesImage);
    linesPng_->setToolTip(uiText("保存图片…"));
    dxf_ = linesTools->addAction(studioFormatIcon("dxf"), "DXF", this, &MainWindow::exportDxf);
    dxf_->setToolTip(uiText("导出型线图 DXF…"));
    connect(linesDialog_,&QDialog::finished,this,[this]{views_.front()->setFocus();});
    auto* inspector = new QWidget;
    auto* properties = new QVBoxLayout(inspector); properties->setContentsMargins(14, 14, 14, 14);
    auto* heading = new QLabel(uiText("控制点")); QFont font = heading->font(); font.setBold(true); heading->setFont(font);
    properties->addWidget(heading);
    pointLabel_ = new StudioMessage(StudioMessageRole::Explanation,uiText("在视图中点击一个控制点"));pointLabel_->setObjectName("PointSelectionNotice");properties->addWidget(pointLabel_);
    auto* form = new QFormLayout;
    for (size_t i = 0; i < coordinates_.size(); ++i) {
        auto* number = new CoordinateEditor; coordinates_[i] = number;
        number->setRange(-1e9, 1e9); number->setDecimals(12); number->setSingleStep(0.01);
        number->setKeyboardTracking(false); number->setMinimumWidth(150);
        auto* label = new QLabel(QString(QChar('X' + int(i)))); coordinateLabels_[i] = label;
        form->addRow(label, number);
        connect(number, &QDoubleSpinBox::valueChanged, this, [this, i] {
            if (!fillingCoordinates_) coordinatesChanged_[i] = true;
        });
    }
    properties->addLayout(form); apply_ = new QPushButton(uiText("应用坐标"));
    connect(apply_, &QPushButton::clicked, this, &MainWindow::applyCoordinates); properties->addWidget(apply_);
    properties->addStretch();
    auto* side = new QTabWidget;inspector_=side;side->setMinimumWidth(270);side->setMaximumWidth(390);data_motion::installProperties(inspector_);
    side->addTab(inspector, uiText("控制点"));
    layerDialog_=createLayerPropertiesDialog(this,layers_);
    connect(layerDialog_,&QDialog::finished,this,[this]{if(!session_.busy())layers_->discardDrafts();});
    layers_->onLayerChanged = [this](fs::EntityId id, const QJsonObject& changes) { session_.updateLayer(id, changes); };
    layers_->onSelectionChanged=[this]{updateState();};
    layers_->onControlNetChanged=[this](fs::EntityId id,bool show){
        if(show)hiddenControlLayers_.remove(id);else hiddenControlLayers_.insert(id);
        for(auto* view:views_)view->setHiddenControlLayers(hiddenControlLayers_);
        selectPoints(views_[0]->selectedPoints());
    };
    auto* split = new QSplitter; split->addWidget(tabs_); split->addWidget(side);
    split->setStretchFactor(0, 1); split->setStretchFactor(1, 0); split->setSizes({1140, 300}); setCentralWidget(split);
    side->hide();buildStudioShell();layoutViews();
    session_.onFirstFrame=[this](std::shared_ptr<const fs::ModelSnapshot> model){
        for(auto* view:views_)view->setSnapshot(model);
    };
    session_.onUpdated = [this] { updateModel(); };
    session_.onStateChanged = [this] { updateState(); };
    session_.onError = [this](const QString& message) { pendingNewPointAfter_=0;pendingPointIds_.clear();pendingPointView_.clear();if(sections_)sections_->discardPending();showError(message,true); };
    session_.onCompleted = [this](const QString& method) { operationCompleted(method); };
    session_.onFileCommitted=[this](const QString& path){if(!workflow_)rememberFile(path);};
    session_.makeSavePreview=[this](std::shared_ptr<const fs::ModelSnapshot> model){return modelPreview(std::move(model),preferenceAppearance(preferences_));};
    session_.onHydroDefaults=[this](const QJsonObject& defaults){
        if(hydroMode_=="cross"){
            if(!stabilityDialog_){
                stabilityDialog_=new HydroDialog(session_,hydroMode_,defaults,workflow_,this);
                stabilityDocument_=session_.model()->documentId;stabilityRevision_=session_.model()->revision;
                stabilityDialog_->onBackground=[this]{QTimer::singleShot(0,this,[this]{updateStabilityBall(true);});};
            }
            showStability();return;
        }
        HydroDialog dialog(session_,hydroMode_,defaults,workflow_,this);hydroDialog_=&dialog;
        if(hydroMode_=="table")dialog.addAction(save_);
        dialog.exec();hydroDialog_=nullptr;
        if(hydroWorkflow_)QTimer::singleShot(0,this,[this]{hydroStep("closed");});
    };
    session_.onHydrostatics=[this](const QJsonObject& result){
        if(result["mode"]=="cross"&&stabilityDialog_){
            stabilityDialog_->receive(result);stabilityUnread_=!stabilityDialog_->isVisible();stabilityFailed_=result["cancelled"].toBool();updateStabilityBall();
        }else if(hydroDialog_)hydroDialog_->receive(result);
    };
    session_.onHydroProgress=[this](const QJsonObject& progress){if(stabilityDialog_&&session_.hydroRunningMode()=="cross"){stabilityDialog_->showProgress(progress);updateStabilityBall();}};
    session_.onPlates=[this](const QJsonObject& result){if(platesDialog_)platesDialog_->receive(result);};
    session_.onCad=[this](const QJsonObject& result){if(exchangeDialog_)exchangeDialog_->receiveCad(result);};
    session_.onPerformance=[this](const QJsonObject& result,bool defaults){
        if(performanceDialog_){performanceDialog_->receive(result,defaults);return;}
        if(!defaults)return;
        PerformanceDialog dialog(session_,result,workflow_,this);performanceDialog_=&dialog;
        dialog.exec();performanceDialog_=nullptr;
        if(!workflow_)preferences_=QCoreApplication::instance()->property("freeship.preferences").toJsonObject();
        if(performanceWorkflow_)QTimer::singleShot(0,this,[this]{performanceStep("closed");});
    };
    updateState(); updateCoordinates();
    if(rememberFiles)loadPreferences(PreferencesStore::defaultFilename());
}

MainWindow::~MainWindow() {
    for(const auto& task:calculationWindows_){
        if(task->session){task->session->onCompleted={};task->session->onStateChanged={};task->session->onError={};
            task->session->onHydroDefaults={};task->session->onHydrostatics={};task->session->onHydroProgress={};task->session->onPerformance={};task->session->onPlates={};}
        delete task->dialog.data();delete task->ball.data();delete task->session.data();
    }
    calculationWindows_.clear();hydroDialog_=nullptr;performanceDialog_=nullptr;platesDialog_=nullptr;stabilityDialog_=nullptr;stabilityBall_=nullptr;
    session_.onFirstFrame={};session_.onUpdated = {}; session_.onStateChanged = {}; session_.onError = {}; session_.onCompleted = {};session_.onFileCommitted={};
    session_.onHydroDefaults={};session_.onHydrostatics={};session_.onHydroProgress={};
    session_.onPerformance={};session_.onPlates={};session_.onCad={};
}
void MainWindow::updateModel() {
    const auto changed=coordinatesChanged_;
    const std::array<double,3> draft{coordinates_[0]->value(),coordinates_[1]->value(),coordinates_[2]->value()};
    const auto model = session_.model(); const bool fresh = shownDocument_ != model->documentId;
    if(!calculationWindows_.contains("hydro:cross")&&stabilityDialog_&&(stabilityDocument_!=model->documentId||stabilityRevision_!=model->revision)){
        if(hydroDialog_==stabilityDialog_)hydroDialog_=nullptr;
        stabilityDialog_->onBackground={};stabilityDialog_->hide();stabilityDialog_->deleteLater();stabilityDialog_=nullptr;
        stabilityUnread_=false;if(stabilityBall_)stabilityBall_->dismiss();
    }
    refreshCalculationIdentity();
    if(pointPopup_&&pointPopup_->isVisible()&&(pointPopupDocument_!=model->documentId||pointPopupRevision_!=model->revision||!model->point(pointPopup_->pointId())))pointPopup_->hide();
    if (fresh) {selection_.clear();hiddenControlLayers_.clear();endFlowline();endPointPlacement();clearDisplayChanges();}
    const auto presentation=displayModel();
    for (auto* view : views_) view->setSnapshot(model,presentation,session_.drawingUnchanged());
    if (!fresh) selection_ = views_.front()->selectedPoints();
    fs::EntityId createdPoint=0;
    if(pendingNewPointAfter_){
        for(const auto& point:model->points)if(!pendingPointIds_.contains(point.id))createdPoint=point.id;
        if(createdPoint)selection_={createdPoint};
        pendingNewPointAfter_=0;pendingPointIds_.clear();
    }
    lines_->setScene(session_.scene(), fresh || shownUnits_ != model->units);
    shownDocument_ = model->documentId; shownUnits_ = model->units;
    layers_->setSnapshot(model);
    if(pendingNewLayerAfter_){
        for(const auto& layer:model->layers)if(layer.id>pendingNewLayerAfter_)layers_->setSelectedLayerId(layer.id);
        pendingNewLayerAfter_=0;
    }
    if(sections_)sections_->setSnapshot(model);
    selectPoints(selection_);
    if(createdPoint&&pendingPointView_){showPointPopup(pendingPointView_,createdPoint);pendingPointView_.clear();}
    if(selectionKind_!=HullEntity::Point)selectEntities(selectionKind_,fresh?QSet<fs::EntityId>{}:entitySelection_);
    if(preferencesPending_){for(size_t i=0;i<3;++i){QSignalBlocker block(coordinates_[i]);coordinates_[i]->setValue(draft[i]);}coordinatesChanged_=changed;}
}
void MainWindow::updateState() {
    save_->setProperty("studio_opens_dialog",session_.sourcePath().isEmpty());
    for (auto* action : findChildren<QAction*>()) {
        if (action->property("file_operation") == "resume_current") {
            const bool canResume = session_.canResumeCurrent();
            action->setVisible(canResume);
            action->setEnabled(canResume);
        }
    }
    if(preferencesAction_)preferencesAction_->setEnabled(!session_.busy());
    if(exchangeDialog_)exchangeDialog_->refreshState();
    for(auto* a:exchangeActions_)a->setEnabled(!session_.busy()&&(a->property("importing").toBool()||session_.ready()));
    if(performanceDialog_)performanceDialog_->refreshState();
    if(platesDialog_)platesDialog_->refreshState();
    if(platesAction_)platesAction_->setEnabled(session_.ready());
    for(auto* action:performanceActions_)action->setEnabled(session_.ready());
    if(hydroDialog_)hydroDialog_->refreshState();
    updateStabilityBall();
    for(auto* action:hydroActions_)action->setEnabled(session_.ready());
    const bool ready = session_.ready(), idle = !session_.busy(); open_->setEnabled(idle);
    if(placePoint_)placePoint_->setEnabled(ready&&idle);
    if(layerProperties_)layerProperties_->setEnabled(ready&&idle);
    if(layerUp_&&layerDown_){int at=-1;if(ready)for(int i=0;i<session_.model()->layers.size();++i)if(session_.model()->layers[i].id==layers_->selectedLayerId())at=i;
        layerUp_->setEnabled(ready&&idle&&at>0);layerDown_->setEnabled(ready&&idle&&at>=0&&at+1<session_.model()->layers.size());}
    for (auto* view : views_) view->setEditingEnabled(ready && idle);
    save_->setEnabled(ready && idle); saveAs_->setEnabled(ready && idle);
    project_->setEnabled(ready); layers_->setEditingEnabled(ready && idle);
    if(activeProject_&&activeProject_->property("project_waiting").toBool()&&ready&&idle){
        activeProject_->refreshSnapshot(*session_.model());activeProject_->setEditingEnabled(true);
    }
    if(sections_)sections_->setEditingEnabled(ready&&(idle||session_.canQueueSectionEdits()));
    if(sectionsAction_)sectionsAction_->setEnabled(ready);
    if(newLayer_)newLayer_->setEnabled(ready&&idle);
    QSet<fs::EntityId> occupiedLayers;
    if(ready)for(const auto& face:session_.model()->faces)occupiedLayers.insert(face.layer);
    const bool removableLayer=ready&&idle&&session_.model()->layers.size()>1;
    if(deleteLayer_)deleteLayer_->setEnabled(removableLayer&&layers_->selectedLayerId()&&!occupiedLayers.contains(layers_->selectedLayerId()));
    if(emptyLayers_)emptyLayers_->setEnabled(removableLayer&&std::any_of(session_.model()->layers.cbegin(),session_.model()->layers.cend(),[&](const auto& layer){return !occupiedLayers.contains(layer.id);}));
    updateDisplayControls();
    if(addCurve_)addCurve_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Edge&&!entitySelection_.isEmpty());
    if(addFlow_)addFlow_->setEnabled(ready&&idle);
    const auto transformed=transformPoints();bool transformable=ready&&idle&&!transformed.isEmpty();
    updateModelingActions();
    if(transformable)for(auto id:transformed){auto* p=session_.model()->point(id);if(!p||p->locked||!p->batchMovable){transformable=false;break;}}
    for(int i=0;i<5;++i)if(auto* action=advancedActions_[i])action->setEnabled(transformable&&(i<3||(i==3&&selectionKind_==HullEntity::Edge)||(i==4&&selectionKind_==HullEntity::Face)));
    undo_->setEnabled((idle&&session_.canUndo())||session_.editingSections());
    redo_->setEnabled((idle&&session_.canRedo())||session_.editingSections());
    if(linesAction_)linesAction_->setEnabled(bool(session_.model()));
    linesPng_->setEnabled(ready&&idle); dxf_->setEnabled(ready && idle);
    bool editable = ready && idle && !selection_.isEmpty();
    if (editable) for (auto id : selection_) {
        const auto* point = session_.model()->point(id);
        if (!point || point->locked || (selection_.size() > 1 && !point->batchMovable)) { editable = false; break; }
    }
    for (auto* coordinate : coordinates_) coordinate->setEnabled(editable);
    if(pointPopup_)pointPopup_->setEditingEnabled(ready&&idle&&session_.model()->point(pointPopup_->pointId())&&!session_.model()->point(pointPopup_->pointId())->locked);
    apply_->setEnabled(editable);
    if(addPoint_) {
        addPoint_->setEnabled(ready&&idle);addFace_->setEnabled(ready&&idle&&selectionOrder_.size()>=3);
        crease_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Edge&&!entitySelection_.isEmpty());
        smooth_->setEnabled(crease_->isEnabled());
        assignLayer_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Face&&!entitySelection_.isEmpty());
        delete_->setEnabled(ready&&idle&&(selectionKind_==HullEntity::Point?!selection_.isEmpty():!entitySelection_.isEmpty()));
        bool canLock=false,canUnlock=false;
        if(ready&&idle&&selectionKind_==HullEntity::Point)for(auto id:selection_)if(const auto* point=session_.model()->point(id)){
            canLock|=!point->locked;canUnlock|=point->locked;
        }
        lock_->setEnabled(canLock);unlock_->setEnabled(canUnlock);
    }
    QString name = QDir::toNativeSeparators(session_.sourcePath());
    if(name.isEmpty()&&!session_.recoveryOrigin().isEmpty())name=QDir::toNativeSeparators(session_.recoveryOrigin())+uiText("（恢复副本）");
    if (name.isEmpty()) name = uiText("未命名船模");
    setWindowTitle(name + (session_.modified() ? " *" : ""));
    if (session_.busy()) statusBar()->showMessage(uiText(session_.model() ? "正在更新模型…" : "正在打开船模…"));
}
void MainWindow::selectPoint(fs::EntityId id) {
    selectPoints(id ? QSet<fs::EntityId>{id} : QSet<fs::EntityId>{});
}
void MainWindow::showStatusMessage(const QString& message){showStatusMessage(message,StudioMessageRole::Feedback);}
QWidget* MainWindow::workspaceOverlayHost()const{
    // QSplitter automatically turns direct child widgets into panes. Overlay
    // children belong to the existing canvas page, never the outer splitter.
    return tabs_?(tabs_->currentWidget()?tabs_->currentWidget():tabs_):nullptr;
}
void MainWindow::showStatusMessage(const QString& message,StudioMessageRole role){
    statusBar()->showMessage(message);
    auto* host=workspaceOverlayHost();if(!host)return;
    if(!statusNotice_){
        statusNotice_=new StudioMessage(role,{},host);statusNotice_->setObjectName("ShellStatusNotice");statusNotice_->setProperty("studio_overlay",true);
        statusNotice_->setAttribute(Qt::WA_TransparentForMouseEvents);
        statusNoticeTimer_=new QTimer(this);statusNoticeTimer_->setSingleShot(true);
        connect(statusNoticeTimer_,&QTimer::timeout,statusNotice_,&QWidget::hide);
    }
    if(statusNotice_->parentWidget()!=host)statusNotice_->setParent(host);
    statusNotice_->setMessage(message,role);positionStatusNotice();statusNotice_->raise();
    if(role==StudioMessageRole::Warning)statusNoticeTimer_->stop();
    else statusNoticeTimer_->start(role==StudioMessageRole::Explanation?7000:1200);
}
void MainWindow::positionStatusNotice(){
    if(!statusNotice_||!statusNotice_->parentWidget())return;
    const auto area=statusNotice_->parentWidget()->rect();const int width=std::max(1,std::min(480,area.width()-32));
    const int height=std::max(statusNotice_->minimumSizeHint().height(),statusNotice_->heightForWidth(width));
    statusNotice_->setGeometry(std::max(0,area.width()-width-16),std::max(0,area.height()-height-16),width,height);
}
void MainWindow::resizeEvent(QResizeEvent* event){QMainWindow::resizeEvent(event);QTimer::singleShot(0,this,[this]{positionStatusNotice();updateStabilityBall();});}
void MainWindow::selectPoints(QSet<fs::EntityId> ids) {
    // Group handles and the group dialog select members together. Selecting an
    // individual point must still allow editing its own coordinates/properties.
    // The views filter hidden and missing points without emitting callbacks.
    views_.front()->setSelectedPoints(std::move(ids)); selection_ = views_.front()->selectedPoints();
    selectionOrder_.erase(std::remove_if(selectionOrder_.begin(),selectionOrder_.end(),[this](auto id){return !selection_.contains(id);}),selectionOrder_.end());
    auto added=selection_.values();std::sort(added.begin(),added.end());
    for(auto id:added)if(!selectionOrder_.contains(id))selectionOrder_.append(id);
    selected_ = selection_.size() == 1 ? *selection_.begin() : 0;
    for (auto* view : views_) view->setSelectedPoints(selection_);
    updateCoordinates(); updateState();
}
void MainWindow::editProject() {
    if (!session_.ready()) return;
    ProjectDialog dialog(*session_.model(), this);
    dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_||startupProbe_);
    activeProject_=&dialog;dialog.setEditingEnabled(!session_.busy());
    const auto accepted=dialog.exec()==QDialog::Accepted;activeProject_.clear();
    if(accepted&&!session_.busy())session_.updateProject(dialog.changes());
}
void MainWindow::updateCoordinates() {
    const auto model = session_.model(); const auto* point = model ? model->point(selected_) : nullptr;
    const bool multiple = selection_.size() > 1;
    bool locked = false, constrained = false;
    if (model) for (auto id : selection_) { const auto* p = model->point(id); locked |= p && p->locked; constrained |= p && !p->batchMovable; }
    const QString restriction = locked ? uiText(" · 含锁定点") : constrained ? uiText(" · 含约束点，请逐点编辑") : "";
    pointLabel_->setMessage(multiple ? QString(uiText("已选 %1 个控制点%2")).arg(selection_.size()).arg(restriction) :
        point ? QString(uiText("控制点 %1%2")).arg(selected_).arg(point->locked ? uiText(" · 已锁定") : "") : uiText("在视图中点击或框选控制点"),
        locked||constrained?StudioMessageRole::Warning:point||multiple?StudioMessageRole::Feedback:StudioMessageRole::Explanation);
    apply_->setText(multiple ? uiText("移动所选控制点") : uiText("应用坐标"));
    const auto pos = point ? point->position : fs::Vec3{}; const std::array<double, 3> values{pos.x, pos.y, pos.z};
    fillingCoordinates_ = true;
    for (size_t i = 0; i < coordinates_.size(); ++i) {
        coordinateLabels_[i]->setText((multiple ? QString("Δ") : QString()) + QChar('X' + int(i)));
        coordinates_[i]->setSuffix(model && model->units == "imperial" ? " ft" : " m");
        coordinates_[i]->setValue(values[i]); coordinatesChanged_[i] = false;
    }
    fillingCoordinates_ = false;
    QStringList identity;auto sorted=selection_.values();std::sort(sorted.begin(),sorted.end());for(auto id:sorted)identity.append(QString::number(id));
    data_motion::pulseProperties(inspector_,"point:"+identity.join(','));
}
void MainWindow::applyCoordinates() {
    if (!session_.ready() || session_.busy()) return;
    for (auto* coordinate : coordinates_) coordinate->interpretText();
    if (selection_.size() > 1) {
        QVector<fs::PointMove> moves; auto ids = selection_.values(); std::sort(ids.begin(), ids.end());
        for (auto id : ids) {
            const auto* p = session_.model()->point(id); if (!p || p->locked || !p->batchMovable) return;
            auto position = p->position;
            if (coordinatesChanged_[0]) position.x += coordinates_[0]->value();
            if (coordinatesChanged_[1]) position.y += coordinates_[1]->value();
            if (coordinatesChanged_[2]) position.z += coordinates_[2]->value();
            moves.append({id, position});
        }
        session_.movePoints(moves); return;
    }
    const auto* point = session_.model()->point(selected_); if (!point || point->locked) return;
    auto position = point->position;
    // Decimal display must not round any coordinate the user did not edit.
    if (coordinatesChanged_[0]) position.x = coordinates_[0]->value();
    if (coordinatesChanged_[1]) position.y = coordinates_[1]->value();
    if (coordinatesChanged_[2]) position.z = coordinates_[2]->value();
    session_.movePoint(selected_, position);
}
void MainWindow::withSavedChanges(std::function<void()> next) {
    if (session_.busy()) return;
    if (!session_.modified()&&calculationDrafts().isEmpty()) { next(); return; }
    QMessageBox question(QMessageBox::Question,uiText("保存修改"),uiText("当前船模有未保存的修改，要先保存吗？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,this);
    question.setOption(QMessageBox::Option::DontUseNativeDialog);
    question.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));
    if(testAttribute(Qt::WA_DontShowOnScreen)){question.setWindowOpacity(0);question.setAttribute(Qt::WA_ShowWithoutActivating);}
    question.setDefaultButton(QMessageBox::Save);
    question.button(QMessageBox::Save)->setText(uiText("保存"));
    question.button(QMessageBox::Discard)->setText(uiText("不保存"));
    question.button(QMessageBox::Cancel)->setText(uiText("取消"));
    const auto result=question.exec();
    if (result == QMessageBox::Discard) { afterSave_ = {}; next(); }
    else if (result == QMessageBox::Save) { afterSave_ = std::move(next); saveModel(); }
    else afterSave_ = {};
}
void MainWindow::openModel(const QString& path) {
    if (!path.isEmpty()) withSavedChanges([this, path] { session_.load(path); });
}
void MainWindow::saveModel(bool saveAs) {
    if (!session_.ready() || session_.busy()) {
        afterSave_ = {};
        showError(session_.busy() ? uiText("当前模型操作尚未完成。") : uiText("计算组件不可用，无法保存当前修改。请保留已保存的文件。"));
        return;
    }
    QString path = session_.sourcePath();
    if(QFileInfo(path).suffix().compare("ftm",Qt::CaseInsensitive)==0){
        const QFileInfo source(path);path=source.absolutePath()+'/'+source.completeBaseName()+".fbm";saveAs=true;
    }
    if (saveAs || path.isEmpty()) {
        if(path.isEmpty()&&!session_.recoveryOrigin().isEmpty()){
            const QFileInfo original(session_.recoveryOrigin());path=original.absolutePath()+'/'+original.completeBaseName()+uiText("-恢复.fbm");
        }
        path=QFileDialog::getSaveFileName(this,uiText("保存船模"),preferenceFileLocation("save",path),uiText("FreeShip 船模 (*.fbm)"),nullptr,
            (startupProbe_||testAttribute(Qt::WA_DontShowOnScreen))?QFileDialog::DontUseNativeDialog:QFileDialog::Options{});
        if(path.isEmpty()){afterSave_={};return;}
        if (QFileInfo(path).suffix().isEmpty()) path += ".fbm";
    }
    pendingCalculationDrafts_=calculationDrafts(true);
    if (!session_.save(path,pendingCalculationDrafts_)){afterSave_={};pendingCalculationDrafts_={};}
}
void MainWindow::openLinesWindow() {
    if(!session_.model())return;
    if(linesDialog_->isMinimized())linesDialog_->showNormal();else linesDialog_->show();
    linesDialog_->raise();linesDialog_->activateWindow();lines_->setFocus();
    if(!session_.scene()&&!session_.busy())session_.ensureDrawing();
}
void MainWindow::fitCurrent() { for(auto* view:views_)view->fit(); }

void MainWindow::exportLinesImage() {
    if (!session_.ready()||session_.busy()) return;
    if(!session_.scene()){afterDrawing_=[this]{exportLinesImage();};if(!session_.ensureDrawing())afterDrawing_={};return;}
    auto path = QFileDialog::getSaveFileName(linesDialog_, uiText("保存当前视图图片"), preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()), uiText("PNG 图片 (*.png)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".png";
    const QImage pixels = lines_->image();
    QSaveFile target(path); target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly) || !pixels.save(&target, "PNG") || !target.commit())
        showError(uiText("图片保存失败，原文件未被替换：") + target.errorString());
    else showStatusMessage(uiText("图片已保存：") + path);
}
void MainWindow::exportDxf() {
    if (!session_.ready() || session_.busy()) return;
    if(!session_.scene()){afterDrawing_=[this]{exportDxf();};if(!session_.ensureDrawing())afterDrawing_={};return;}
    auto path = QFileDialog::getSaveFileName(linesDialog_, uiText("导出型线图 DXF"), preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()), "DXF (*.dxf)");
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".dxf";
    try { ModelSession::atomicCopy(session_.dxfPath(), path); showStatusMessage(uiText("DXF 已导出：") + path); }
    catch (const std::exception& ex) { showError(QString::fromUtf8(ex.what())); }
}
void MainWindow::showError(const QString& sourceMessage,bool sessionError) {
    afterDrawing_={};
    const QString exchangePrefix=QStringLiteral("模型交接：");
    const QString translatedPrefix=uiText(exchangePrefix);
    const QString message=sourceMessage.startsWith(exchangePrefix)
        ?translatedPrefix+(translatedPrefix==exchangePrefix?QString{}:QStringLiteral(" "))
            +uiText(sourceMessage.mid(exchangePrefix.size()))
        :uiText(sourceMessage);
    if(!submittedDisplay_.isEmpty()||!queuedDisplay_.isEmpty())clearDisplayChanges();
    closingAfterRecoveryDiscard_=false;
    if(preferencesPending_){preferencesFinished(message);return;}
    if(exchangeDialog_){exchangeDialog_->showFailure(message);return;}
    if(sessionError&&!calculationWindows_.contains("hydro:cross")&&stabilityDialog_&&!stabilityDialog_->property("hydro_finished").toBool()&&stabilityDialog_->property("hydro_finished").isValid()){
        stabilityDialog_->showFailure(message,!session_.hydroRunning());stabilityFailed_=true;stabilityUnread_=!stabilityDialog_->isVisible();updateStabilityBall();return;
    }
    if(hydroDialog_&&hydroDialog_!=stabilityDialog_&&!hydroDialog_->property("calculation_task_key").isValid()){hydroDialog_->showFailure(message);return;}
    if(performanceDialog_&&!performanceDialog_->property("calculation_task_key").isValid()){performanceDialog_->showFailure(message);return;}
    if(platesDialog_&&!platesDialog_->property("calculation_task_key").isValid()){platesDialog_->showFailure(message);return;}
    pendingNewLayerAfter_=0;
    pendingNewPointAfter_=0;
    if(!restoringFile_.isEmpty()){
        if(fileHistory_)fileHistory_->stopRestore(restoringFile_);
        restoringFile_.clear();afterSave_={};refreshRecentFiles();
        showStatusMessage(uiText("上次的船模未能打开，请从“打开模型”重新选择。")+message,StudioMessageRole::Warning);
        if(startupProbe_)startupStep("error",message);
        return;
    }
    if(startupProbe_){afterSave_={};statusBar()->showMessage(message);startupStep("error",message);return;}
    afterSave_ = {}; statusBar()->showMessage(uiText("操作未完成。当前画面已保留。"));
    if(sections_&&sections_->isVisible()&&!workflow_){sections_->setSnapshot(session_.model());sections_->showError(message);return;}
    if (workflow_) finishWorkflow(message); else {
        QMessageBox dialog(QMessageBox::Warning,uiText("操作未完成"),message,QMessageBox::Ok,this);
        dialog.setOption(QMessageBox::Option::DontUseNativeDialog);dialog.exec();
    }
}
void MainWindow::operationCompleted(const QString& method) {
    if(method=="display"){
        submittedDisplay_={};
        submitDisplayChanges();
    }
    if(method=="discard_recovery"&&closingAfterRecoveryDiscard_){
        // Discard removes the recovery checkpoint, not the document's dirty flag.
        // Carry the user's close decision through the second close event.
        closingAfterRecoveryDiscard_=false;closeApproved_=true;
        QTimer::singleShot(0,this,[this]{close();qApp->quit();});return;
    }
    if(method=="configure_preferences"&&preferencesPending_){preferencesFinished();return;}
    if((method=="import_model"||method=="append_model")&&exchangeDialog_)exchangeDialog_->imported();
    restoringFile_.clear();
    updateState();
    if(method=="save"){
        for(const auto& task:calculationWindows_){
            const auto key=calculationInputKey(*task);
            if(task->inputDocument==session_.model()->documentId&&pendingCalculationDrafts_.contains(key))
                task->savedInputs=pendingCalculationDrafts_[key].toObject();
        }
        pendingCalculationDrafts_={};
        message_motion::confirmation(findChild<QWidget*>("StudioCaptionTitle"));
    }
    statusBar()->showMessage(method == "save" ? uiText("船模已保存：") + session_.sourcePath() +
        (session_.lastBackupPath().isEmpty()?QString{}:uiText("；上一版备份：")+session_.lastBackupPath()) :
        QString(uiText("%1 · %2 个控制点")).arg(QFileInfo(session_.sourcePath()).fileName())
            .arg(session_.model()->points.size()));
    if(!workflow_&&(method=="load"||method=="save"))rememberFile(session_.sourcePath());
    if(!session_.recoveryError().isEmpty())showStatusMessage(uiText("自动恢复副本未更新：")+session_.recoveryError(),StudioMessageRole::Warning);
    else if(method=="load"&&session_.modified()&&session_.sourcePath().isEmpty())showStatusMessage(uiText("船模已恢复，请另存为文件。"),StudioMessageRole::Explanation);
    // Real save continuations also apply to isolated production probes. Do not
    // let workflow routing swallow "save, then close" after the file is written.
    if(method=="save"&&afterSave_){
        auto next=std::move(afterSave_);afterSave_={};QTimer::singleShot(0,this,std::move(next));return;
    }
    if(startupProbe_){startupStep(method);return;}
    if(preferencesWorkflow_){preferencesStep(method);return;}
    if(modelUiWorkflow_){modelUiStep();return;}
    if(viewportWorkflow_){viewportStep();return;}
    if(shadingWorkflow_){shadingStep();return;}
    if (workflow_) { if(exchangeWorkflow_)exchangeStep(method);else if(platesWorkflow_)platesStep(method);else if(performanceWorkflow_)performanceStep(method);else if(hydroWorkflow_)hydroStep(method);else if(modelingWorkflow_)modelingStep(method);else if(advancedWorkflow_)advancedStep(method);else if(auxiliaryWorkflow_)auxiliaryStep(method);else if(displayWorkflow_)displayStep(method);else if(backgroundWorkflow_)backgroundStep(method);else if(overlayWorkflow_)overlayStep(method);else if(sectionsWorkflow_)sectionsStep(method);else if(topologyWorkflow_)topologyStep(method);else if (editingWorkflow_) editingStep(method); else workflowStep(method); return; }
    if(method=="snapshot"&&afterDrawing_){auto next=std::move(afterDrawing_);afterDrawing_={};QTimer::singleShot(0,this,std::move(next));}
}
ModelSession* MainWindow::calculationSession(const QString& key)const{
    const auto task=calculationWindows_.value(key);return task?task->session.data():nullptr;
}
QDialog* MainWindow::calculationDialog(const QString& key)const{
    const auto task=calculationWindows_.value(key);return task?task->dialog.data():nullptr;
}
bool MainWindow::calculationsBusy()const{
    for(const auto& task:calculationWindows_)if(task->session&&(task->initializing||task->session->busy()))return true;
    return false;
}
void MainWindow::openHydro(const QString& mode){hydroMode_=mode;openCalculation("hydro",mode);}
void MainWindow::openPerformance(const QString& method){openCalculation("performance",method);}
QString MainWindow::calculationInputKey(const CalculationWindow& task){
    if(task.kind=="hydro")return QString("hydro_")+task.method;
    if(task.kind=="performance")return task.method;
    return {};
}
QJsonObject MainWindow::calculationInputs(const CalculationWindow& task){
    if(auto* dialog=dynamic_cast<HydroDialog*>(task.dialog.data()))return dialog->savedInputs();
    if(auto* dialog=dynamic_cast<PerformanceDialog*>(task.dialog.data()))
        if(!dialog->usingDevelopmentSample())return dialog->inputs()["values"].toObject();
    return {};
}
QJsonObject MainWindow::calculationDrafts(bool commitEditors)const{
    QJsonObject drafts;
    if(!session_.model())return drafts;
    const auto model=session_.model();
    for(const auto& task:calculationWindows_){
        if(!task->dialog||task->inputDocument!=model->documentId||task->inputUnits!=model->units)continue;
        const auto key=calculationInputKey(*task);
        if(key.isEmpty())continue;
        if(commitEditors)for(auto* field:task->dialog->findChildren<QDoubleSpinBox*>())
            if(field->hasFocus())field->interpretText();
        const auto inputs=calculationInputs(*task);
        if(!inputs.isEmpty()&&inputs!=task->savedInputs)drafts[key]=inputs;
    }
    return drafts;
}
void MainWindow::openCalculation(const QString& kind,const QString& method){
    if(calculationExitPending_)return;
    const auto key=kind+":"+method;
    if(const auto task=calculationWindows_.value(key)){
        // Units and captions must be rebuilt together before accepting values
        // for a different unit system. Active tasks retain their old snapshot.
        const bool reset=task->session&&!task->session->busy()&&!task->initializing&&session_.ready()&&
            (!task->session->ready()||task->inputDocument!=session_.model()->documentId||
             task->inputUnits!=session_.model()->units);
        if(!reset){
            if(session_.ready()&&task->session&&!task->session->busy())
                if(auto* dialog=dynamic_cast<PerformanceDialog*>(task->dialog.data()))dialog->syncPrincipalDimensions(session_.model()->principalDimensions);
            showCalculation(key);return;
        }
        if(hydroDialog_==task->dialog)hydroDialog_=nullptr;
        if(performanceDialog_==task->dialog)performanceDialog_=nullptr;
        if(platesDialog_==task->dialog)platesDialog_=nullptr;
        if(stabilityDialog_==task->dialog){stabilityDialog_=nullptr;stabilityBall_=nullptr;}
        task->session->onCompleted={};task->session->onStateChanged={};task->session->onError={};
        task->session->onHydroDefaults={};task->session->onHydrostatics={};task->session->onHydroProgress={};task->session->onPerformance={};task->session->onPlates={};
        delete task->dialog.data();delete task->ball.data();delete task->session.data();calculationWindows_.remove(key);
    }
    if(!session_.ready())return;
    auto task=std::make_shared<CalculationWindow>();task->key=key;task->kind=kind;task->method=method;
    task->session=new ModelSession(this);task->session->setProperty("calculation_task_key",key);
    calculationWindows_.insert(key,task);
    auto bindDialog=[this,task](QDialog* dialog){
        task->dialog=dialog;task->title=dialog->windowTitle();task->initializing=false;
        const auto model=task->session->model();
        task->inputDocument=model->documentId;task->inputUnits=model->units;task->savedInputs=calculationInputs(*task);
        dialog->setProperty("calculation_task_key",task->key);
        dialog->setProperty("calculation_session",QVariant::fromValue<QObject*>(task->session.data()));
        dialog->setModal(false);dialog->setWindowModality(Qt::NonModal);
        dialog->addAction(save_);
        connect(dialog,&QDialog::finished,this,[this,task]{
            if(hydroDialog_==task->dialog)hydroDialog_=nullptr;
            if(performanceDialog_==task->dialog)performanceDialog_=nullptr;
            if(platesDialog_==task->dialog)platesDialog_=nullptr;
            QTimer::singleShot(0,this,[this,task]{updateCalculationBalls(task->key);});
            if(task->kind=="hydro"&&hydroWorkflow_)QTimer::singleShot(0,this,[this]{hydroStep("closed");});
            if(task->kind=="performance"&&performanceWorkflow_)QTimer::singleShot(0,this,[this]{performanceStep("closed");});
            if(task->kind=="plates"&&platesWorkflow_)QTimer::singleShot(0,this,[this]{platesStep("closed");});
        });
        if(task->key=="hydro:cross")stabilityDialog_=static_cast<HydroDialog*>(dialog);
        if(!calculationExitPending_)showCalculation(task->key);
    };
    task->session->onCompleted=[this,task,bindDialog](const QString& operation){
        if(operation=="calculation_ready"&&!task->dialog){
            if(task->kind=="hydro")task->session->hydroDefaults();
            else if(task->kind=="performance")task->session->performanceDefaults({{"calculator",task->method},{"auto_extract",true}});
            else bindDialog(new PlatesDialog(*task->session,workflow_,this));
        }
    };
    task->session->onStateChanged=[this,task]{
        if(auto* d=dynamic_cast<HydroDialog*>(task->dialog.data()))d->refreshState();
        if(auto* d=dynamic_cast<PerformanceDialog*>(task->dialog.data()))d->refreshState();
        if(auto* d=dynamic_cast<PlatesDialog*>(task->dialog.data()))d->refreshState();
        if(task->session&&task->session->busy()){task->failed=false;task->unread=false;}
        updateCalculationBalls();refreshCalculationIdentity();
    };
    task->session->onHydroDefaults=[this,task,bindDialog](const QJsonObject& defaults){
        if(task->dialog)return;
        auto* dialog=new HydroDialog(*task->session,task->method,defaults,workflow_,this);
        bindDialog(dialog);
    };
    task->session->onHydrostatics=[this,task](const QJsonObject& result){
        if(auto* dialog=dynamic_cast<HydroDialog*>(task->dialog.data())){
            dialog->receive(result);task->unread=!dialog->isVisible();task->failed=result["cancelled"].toBool();
            for(const auto& value:result["rows"].toArray())if(value.toObject()["status"]!="ok")task->failed=true;
            if(!result["cancelled"].toBool()){const auto model=task->session->model();task->resultDocument=model->documentId;task->resultRevision=model->revision;task->resultName=model->name;}
        }
        updateCalculationBalls();refreshCalculationIdentity();
    };
    task->session->onHydroProgress=[this,task](const QJsonObject& progress){
        if(auto* dialog=dynamic_cast<HydroDialog*>(task->dialog.data()))dialog->showProgress(progress);
        updateCalculationBalls();
    };
    task->session->onPerformance=[this,task,bindDialog](const QJsonObject& result,bool defaults){
        if(auto* dialog=dynamic_cast<PerformanceDialog*>(task->dialog.data())){
            dialog->receive(result,defaults);if(!defaults){task->unread=!dialog->isVisible();task->failed=false;
                const auto model=task->session->model();task->resultDocument=model->documentId;task->resultRevision=model->revision;task->resultName=model->name;
                if(task->method!="acceleration"){
                    if(auto* acceleration=dynamic_cast<PerformanceDialog*>(calculationDialog("performance:acceleration")))
                        acceleration->refreshState();
                }
            }
        }else if(defaults)bindDialog(new PerformanceDialog(*task->session,result,workflow_,this));
        updateCalculationBalls();refreshCalculationIdentity();
    };
    task->session->onPlates=[this,task](const QJsonObject& result){
        if(auto* dialog=dynamic_cast<PlatesDialog*>(task->dialog.data())){dialog->receive(result);task->unread=!dialog->isVisible();task->failed=!dialog->failure().isEmpty();
            if(!task->failed){const auto model=task->session->model();task->resultDocument=model->documentId;task->resultRevision=model->revision;task->resultName=model->name;}}
        updateCalculationBalls();refreshCalculationIdentity();
    };
    task->session->onError=[this,task](const QString& message){
        task->initializing=false;task->failed=true;task->unread=!task->dialog||!task->dialog->isVisible();
        if(auto* dialog=dynamic_cast<HydroDialog*>(task->dialog.data()))dialog->showFailure(message,!task->session->busy());
        else if(auto* dialog=dynamic_cast<PerformanceDialog*>(task->dialog.data()))dialog->showFailure(message);
        else if(auto* dialog=dynamic_cast<PlatesDialog*>(task->dialog.data()))dialog->showFailure(message);
        else {showStatusMessage(message,StudioMessageRole::Warning);calculationWindows_.remove(task->key);task->session->deleteLater();}
        updateCalculationBalls();
    };
    if(!task->session->loadCalculationSnapshot(session_)&&calculationWindows_.value(key)==task&&task->initializing){
        task->initializing=false;calculationWindows_.remove(key);task->session->deleteLater();
        showStatusMessage(uiText("请等待模型更新完成后再计算。"),StudioMessageRole::Warning);
    }
}
void MainWindow::showCalculation(const QString& key){
    const auto task=calculationWindows_.value(key);if(!task||!task->dialog)return;
    task->unread=false;if(task->ball)task->ball->dismiss();
    if(auto* dialog=dynamic_cast<HydroDialog*>(task->dialog.data())){
        hydroMode_=task->method;hydroDialog_=dialog;dialog->refreshState();
        if(task->session->hydroRunning())dialog->showProgress(task->session->hydroProgress());
    }else if(auto* dialog=dynamic_cast<PerformanceDialog*>(task->dialog.data())){performanceDialog_=dialog;dialog->refreshState();}
    else if(auto* dialog=dynamic_cast<PlatesDialog*>(task->dialog.data())){platesDialog_=dialog;dialog->refreshState();}
    refreshCalculationIdentity();task->dialog->show();task->dialog->raise();task->dialog->activateWindow();
}
void MainWindow::showStability(){showCalculation("hydro:cross");}
void MainWindow::updateStabilityBall(bool animate){updateCalculationBalls(animate?QStringLiteral("hydro:cross"):QString{});}
void MainWindow::refreshCalculationIdentity(){
    for(const auto& task:calculationWindows_){
        if(!task->dialog||!task->session)continue;
        const auto model=task->session->model();
        const bool oldResult=model&&!task->session->busy()&&task->resultRevision&&
            (task->resultDocument!=model->documentId||task->resultRevision!=model->revision);
        const bool current=task->session->calculationSnapshotCurrent()&&!oldResult;
        task->dialog->setProperty("calculation_snapshot_current",current);
        const auto name=oldResult?task->resultName:model?model->name:QString{};
        const auto revision=oldResult?task->resultRevision:model?model->revision:0;
        task->dialog->setWindowTitle(task->title+(!current?QString(" — %1 · %2 %3").arg(name.isEmpty()?uiText("未命名船模"):name,uiText("版本")).arg(revision):QString{}));
    }
}
void MainWindow::updateCalculationBalls(const QString& animateKey){
    int dockIndex=0;
    for(const auto& task:calculationWindows_){
        if(!task->session||!task->dialog)continue;
        const bool queued=task->session->calculationQueued();
        const bool running=task->session->busy()&&!queued;
        const bool needed=!task->dialog->isVisible()&&(running||queued||task->unread);
        if(!needed){if(task->ball)task->ball->dismiss();continue;}
        if(!task->ball){
            task->ball=new HydroTaskBall(this);
            task->ball->setObjectName(task->key=="hydro:cross"?"HydroTaskBall":"CalculationTaskBall");
            task->ball->setProperty("calculation_task_key",task->key);
            task->ball->setTaskIcon(task->dialog->windowIcon());
            connect(task->ball,&QAbstractButton::clicked,this,[this,key=task->key]{showCalculation(key);});
            if(task->key=="hydro:cross")stabilityBall_=task->ball;
        }
        const int position=task->session->calculationQueuePosition();
        const auto label=task->title+(queued?QString(" — %1 · %2").arg(uiText("排队中")).arg(position):QString{});
        task->ball->setToolTip(label);task->ball->setAccessibleName(label);
        task->ball->setState(running,task->unread,task->failed,queued,position);
        const int firstY=std::clamp(height()/2,80,std::max(80,height()-58));
        const int rows=std::max(1,(height()-58-firstY)/56+1);const int column=dockIndex/rows,row=dockIndex%rows;++dockIndex;
        const QPoint target(std::max(0,width()-58-column*56),firstY+row*56);
        QPoint start=mapFromGlobal(task->dialog->frameGeometry().center())-QPoint(24,24);
        start.setX(std::clamp(start.x(),0,std::max(0,width()-48)));start.setY(std::clamp(start.y(),0,std::max(0,height()-48)));
        task->ball->dock(target,start,task->key==animateKey&&studio_motion::enabled());
    }
}
void MainWindow::openPlates(){
    openCalculation("plates","develop");
}
void MainWindow::stopCalculationsAndClose(){
    calculationExitPending_=true;session_.pauseCalculationQueue(true);
    // Freeze dispatch before cancelling: releasing a running slot must not start
    // a queued solver while the user is exiting.
    for(const auto& task:calculationWindows_)if(task->session){
        task->session->cancelHydrostatics();task->session->cancelPerformance();task->session->cancelPlates();
    }
    setEnabled(false);showStatusMessage(uiText("正在取消计算…"),StudioMessageRole::Feedback);
    auto* poll=new QTimer(this);poll->setInterval(25);
    auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
    connect(poll,&QTimer::timeout,this,[this,poll,elapsed]{
        bool busy=false;
        for(const auto& task:calculationWindows_)if(task->session){
            if(elapsed->elapsed()>=2000&&task->session->busy())task->session->abortCalculationForExit();
            busy|=task->session->busy();
        }
        if(busy||session_.busy()||session_.recordingCalculationParameters())return;
        poll->stop();poll->deleteLater();
        for(const auto& task:calculationWindows_)task->initializing=false;
        calculationExitPending_=false;session_.pauseCalculationQueue(false);setEnabled(true);
        statusBar()->clearMessage();close();
    });poll->start();
}
void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if(event->type()==QEvent::LanguageChange&&lines_)
        lines_->setReferenceCaptions(uiText("基线 %1"),uiText("中心线"));
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if(closeApproved_){linesDialog_->close();event->accept();return;}
    if(calculationExitPending_||calculationClosePrompt_){event->ignore();return;}
    if(calculationsBusy()||session_.recordingCalculationParameters()){
        event->ignore();QScopedValueRollback<bool> prompting(calculationClosePrompt_,true);
        QMessageBox question(QMessageBox::Question,uiText("退出软件"),
            uiText("退出将取消正在运行和排队的计算，未完成的结果不会保留。是否退出？"),QMessageBox::Yes|QMessageBox::Cancel,this);
        question.setObjectName("CalculationExitConfirmation");
        question.setOption(QMessageBox::Option::DontUseNativeDialog);
        question.setDefaultButton(QMessageBox::Cancel);
        question.button(QMessageBox::Yes)->setText(uiText("退出"));question.button(QMessageBox::Cancel)->setText(uiText("取消"));
        if(question.exec()==QMessageBox::Yes)stopCalculationsAndClose();
        return;
    }
    if (session_.busy()) { event->ignore(); showStatusMessage(uiText("请等当前模型操作完成后再关闭。"),StudioMessageRole::Warning); }
    else if (session_.modified()) {
        event->ignore(); withSavedChanges([this] {
            closingAfterRecoveryDiscard_=true;
            if(!session_.discardRecovery())closingAfterRecoveryDiscard_=false;
        });
    } else {linesDialog_->close();event->accept();}
}
void MainWindow::workflowCase(const QString& name, bool pass, const QString& message) {
    workflowCases_.append(QJsonObject{{"name", name}, {"status", pass ? "pass" : "fail"}, {"message", message}});
}
void MainWindow::verifyWorkflow(const QString& path, const QString& output) {
    workflow_ = true; workflowOutput_ = QFileInfo(output).absoluteFilePath(); workflowTimer_.start();
    if (!QDir().mkpath(workflowOutput_ + "/views")) { finishWorkflow("Cannot create workflow output"); return; }
    ensurePolished(); if (centralWidget()->layout()) centralWidget()->layout()->activate();
    if(!modelUiWorkflow_)for (auto* view : views_) view->resize(640, 400);
    lines_->resize(1280, 800);
    QTimer::singleShot(0, this, [this, path] { session_.load(path); });
    QTimer::singleShot(300000, this, [this] { if (!workflowFinished_) finishWorkflow("Workflow timed out"); });
}
void MainWindow::captureWorkflow(const QString& phase) {
    const QString folder = workflowOutput_ + '/' + phase;
    if (!QDir().mkpath(folder)) throw std::runtime_error("Cannot create workflow stage");
    const std::array<std::pair<QString, QString>, 5> sources{{
        {"model", session_.modelPath()}, {"scene", session_.scenePath()}, {"dxf", session_.dxfPath()},
        {"native", QFileInfo(session_.scenePath()).absolutePath()+"/native-report.json"},
        {"sections", QFileInfo(session_.scenePath()).absolutePath()+"/sections.json"}}};
    for (const auto& item : sources) {
        const QString destination = folder + '/' + QFileInfo(item.second).fileName();
        ModelSession::atomicCopy(item.second, destination); workflowArtifacts_.insert(phase + '_' + item.first, destination);
    }
    workflowTimings_.insert(phase + "_operation_ms", session_.lastOperationMs());
    workflowCase(phase + "_shared_snapshot", std::all_of(views_.begin(), views_.end(), [this](HullView* view) {
        return view->snapshot() == session_.model();
    }));
}
void MainWindow::workflowStep(const QString& method) {
    if (workflowFinished_) return;
    try {
        const auto model = session_.model();
        switch (workflowStage_++) {
        case 0: {
            if (method != "load") throw std::runtime_error("Expected initial model load");
            captureWorkflow("original"); workflowOriginalDigest_ = contents(session_.scenePath());
            for (size_t i = 0; i < views_.size(); ++i) {
                views_[i]->fit();
                if (!views_[i]->image().save(workflowOutput_ + "/views/view-" + QString::number(i) + ".png"))
                    throw std::runtime_error("Cannot save four-view evidence");
            }
            lines_->fit();
            if (!lines_->image().save(workflowOutput_ + "/views/linesplan.png")) throw std::runtime_error("Cannot save linesplan evidence");
            if (!grab().save(workflowOutput_ + "/views/main-window.png")) throw std::runtime_error("Cannot save hidden window evidence");
            const auto point = std::find_if(model->points.begin(), model->points.end(), [](const auto& p) { return !p.locked; });
            if (point == model->points.end()) throw std::runtime_error("Workflow fixture has no editable point");
            workflowPoint_ = point->id; workflowOriginalPoint_ = point->position; selectPoint(workflowPoint_);
            coordinates_[2]->setValue(point->position.z + 0.01); apply_->click(); break;
        }
        case 1: {
            if (method != "move_point") throw std::runtime_error("Expected point move");
            captureWorkflow("moved"); workflowMovedDigest_ = contents(session_.scenePath());
            const auto* point = model->point(workflowPoint_);
            workflowCase("move_coordinates", point && point->position.x == workflowOriginalPoint_.x &&
                point->position.y == workflowOriginalPoint_.y && point->position.z != workflowOriginalPoint_.z);
            workflowCase("move_dirty_history", session_.modified() && session_.canUndo() && !session_.canRedo()); undo_->trigger(); break;
        }
        case 2: {
            if (method != "undo") throw std::runtime_error("Expected undo");
            captureWorkflow("undo");
            const auto* point = model->point(workflowPoint_);
            workflowCase("undo_exact_point", point && samePoint(point->position, workflowOriginalPoint_));
            workflowCase("undo_exact_scene", contents(session_.scenePath()) == workflowOriginalDigest_);
            workflowCase("undo_clean_history", !session_.modified() && !session_.canUndo() && session_.canRedo()); redo_->trigger(); break;
        }
        case 3: {
            if (method != "redo") throw std::runtime_error("Expected redo");
            captureWorkflow("redo");
            workflowCase("redo_exact_scene", contents(session_.scenePath()) == workflowMovedDigest_);
            workflowCase("redo_dirty_history", session_.modified() && session_.canUndo() && !session_.canRedo());
            const QString saved = workflowOutput_ + "/saved.fbm"; workflowArtifacts_.insert("saved_file", saved); session_.save(saved); break;
        }
        case 4: {
            if (method != "save") throw std::runtime_error("Expected save");
            const QString saved = workflowArtifacts_.value("saved_file").toString();
            workflowCase("save_committed", !session_.modified() && QFileInfo(saved).size() > 0 && session_.sourcePath() == saved);
            workflowTimings_.insert("save_operation_ms", session_.lastOperationMs()); session_.load(saved); break;
        }
        case 5:
            if (method != "load") throw std::runtime_error("Expected saved-model reload");
            captureWorkflow("reopened");
            workflowCase("reopen_clean_history", !session_.modified() && !session_.canUndo() && !session_.canRedo());
            workflowCases_.append(QJsonObject{{"name", "desktop_acceptance"}, {"status", "not_run"},
                {"message", "Hidden Qt objects and offscreen images; real desktop interaction is not exercised."}});
            finishWorkflow(); break;
        default: throw std::runtime_error("Unexpected workflow callback");
        }
    } catch (const std::exception& ex) { finishWorkflow(QString::fromUtf8(ex.what())); }
}
void MainWindow::finishWorkflow(const QString& error) {
    if (workflowFinished_) return;
    workflowFinished_ = true;
    bool failed = !error.isEmpty();
    for (const auto& item : workflowCases_) failed = failed || item.toObject().value("status").toString() == "fail";
    const QJsonObject report{{"status", !error.isEmpty() ? "error" : (failed ? "fail" : "partial")}, {"error", error},
        {"cases", workflowCases_}, {"artifacts", workflowArtifacts_}, {"timings", workflowTimings_}, {"total_ms", workflowTimer_.elapsed()},
        {"scope", "Hidden production window, actions, persistent model service and atomic save. No desktop input."}};
    QFile file(workflowOutput_ + "/report.json");
    if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(report).toJson()); else failed = true;
    QCoreApplication::exit(failed ? 1 : 0);
}
