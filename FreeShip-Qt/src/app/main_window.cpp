#include "main_window.h"
#include "section_dialog.h"
#include "project_dialog.h"
#include "layer_panel.h"
#include "studio_ui.h"
#include "hydro_dialog.h"
#include "performance_dialog.h"
#include "plates_dialog.h"
#include "exchange_dialog.h"
#include "core/cad_export.h"
#include "render/model_preview.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
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
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
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
    if(rememberFiles)fileHistory_=std::make_unique<FileHistory>(FileHistory::defaultFilename());
    if(rememberFiles){
        session_.setRecoveryRoot(fs::defaultRecoveryRoot());
        if(!fs::defaultRecoveryRoot().isEmpty())session_.setLinesCacheRoot(QFileInfo(fs::defaultRecoveryRoot()).dir().filePath("linesplan-cache-v1"));
    }
    applyStudioTheme();
    setWindowTitle("FreeShip_26 · Qt 开发版"); resize(1440, 960);
    auto* file = menuBar()->addMenu("文件(&F)");
    open_ = file->addAction("打开模型…", QKeySequence::Open, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "打开船模", preferenceFileLocation("open",session_.sourcePath()), "FreeShip 船模 (*.fbm)");
        if (!path.isEmpty()) openModel(path);
    });
    save_ = file->addAction("保存", QKeySequence::Save, this, [this] { saveModel(); });
    saveAs_ = file->addAction("另存为…", QKeySequence::SaveAs, this, [this] { saveModel(true); });
    save_->setProperty("file_operation","save");saveAs_->setProperty("file_operation","save_as");
    recentMenu_=file->addMenu("最近打开");
    connect(recentMenu_,&QMenu::aboutToShow,this,&MainWindow::refreshRecentFiles);
    refreshRecentFiles();
    auto* recover=file->addAction("恢复未保存的船模…",this,[this]{showRecoveries();});
    recover->setProperty("file_operation","recover");
    file->addSeparator();
    png_ = file->addAction("保存当前视图图片…", this, &MainWindow::exportImage);
    dxf_ = file->addAction("导出型线图 DXF…", this, &MainWindow::exportDxf);
    for(bool importing:{true,false}){
        auto* menu=file->addMenu(importing?"导入模型":"导出模型");
        QStringList formats{"obj","stl","fef"};if(importing)formats<<"polycad"<<"hull"<<"carene"<<"chines"<<"surface"<<"vrml"<<"part"<<"waves";else formats<<"iges"<<"dxf3d"<<"dxfmesh"<<"offsets"<<"coordinates"<<"ghs"<<"arch_hll"<<"arch_app"<<"pam"<<"part"<<"michlet"<<"addmass"<<"offset_table"<<"dxfcrease";
        for(const QString& format:formats){
            auto* a=menu->addAction(fs::cad::title(format)+"…",this,[this,importing,format]{openExchange(importing,format);});
            a->setProperty("operation",(importing?"import_":"export_")+format);a->setProperty("importing",importing);exchangeActions_.append(a);
        }
    }
    file->addSeparator(); file->addAction("退出", QKeySequence::Quit, this, &QWidget::close);
    auto* edit = menuBar()->addMenu("编辑(&E)");
    undo_ = edit->addAction("撤销", QKeySequence::Undo, this, [this] { session_.undo(); });
    redo_ = edit->addAction("重做", QKeySequence::Redo, this, [this] { session_.redo(); });
    project_ = edit->addAction("工程设置…", this, &MainWindow::editProject);
    project_->setProperty("document_operation","project_settings");
    preferencesAction_=edit->addAction(studioIcon("Preferences"),"偏好设置…",this,&MainWindow::editPreferences);
    preferencesAction_->setProperty("operation","preferences");
    auto* viewMenu = menuBar()->addMenu("视图(&V)");
    auto* fit = viewMenu->addAction("全图", QKeySequence("F"), this, &MainWindow::fitCurrent);
    auto* help = menuBar()->addMenu("帮助(&H)");
    help->addAction("关于", this, [this] {
        QMessageBox::about(this, "关于 FreeShip_26", "FreeShip_26 · Qt 开发版\n\n"
            "已迁移：四视图、型线图、多点选择与移动、工程设置、图层属性、撤销与重做、FBM 保存、图片和 DXF 导出。\n"
            "其他编辑工具及船舶计算功能正在迁移。\n\n基于 FreeShip / FreeShip Plus，保留原项目许可与作者声明。");
    });
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
        view->onEditRejected = [this](const QString& message) { statusBar()->showMessage(message); };
        view->onEntitySelectionChanged=[this](HullEntity kind,QSet<fs::EntityId> ids){selectEntities(kind,std::move(ids));};
        view->onBackgroundAction=[this,view](const QString& operation,QPointF position){backgroundAction(view,operation,position);};
        view->onBackgroundChanged=[this,view](const QJsonObject& changes){
            if(view->snapshot()==session_.model()&&session_.ready()&&!session_.busy())session_.editBackground(view->backgroundView(),"update",{{"changes",changes}});
        };
        view->onFlowPlacementEnded=[this]{endFlowline();};
        view->onPointPlacementEnded=[this]{endPointPlacement();};
        view->onPointCreationRequested=[this](fs::Vec3 p){
            if(!session_.ready()||session_.busy())return;pendingNewPointAfter_=1;for(const auto& point:session_.model()->points)pendingNewPointAfter_=std::max(pendingNewPointAfter_,point.id+1);
            if(!session_.editTopology("add_point",{{"position",QJsonArray{p.x,p.y,p.z}}}))pendingNewPointAfter_=0;
        };
        view->onViewAction=[this,view](const QString& action){viewAction(view,action);};
        view->onViewSettingsChanged=[this,view]{refreshLightDialog(view);};
        view->onFlowRequested=[this](int projection,QPointF source){session_.editAuxiliary("create_flow",{{"view",projection},{"source",QJsonArray{source.x(),source.y()}}});};
    }
    tabs_->addTab(hulls, "四视图"); lines_ = new LinesView; tabs_->addTab(lines_, "型线图");
    viewMenu->addAction("四视图", this, [this] { tabs_->setCurrentIndex(0); });
    viewMenu->addAction("型线图", this, [this] { tabs_->setCurrentIndex(1); });
    auto* fill = viewMenu->addAction("型线图填色"); fill->setCheckable(true); fill->setChecked(true);
    connect(fill, &QAction::toggled, lines_, &LinesView::setFill);
    auto* inspector = new QWidget;
    auto* properties = new QVBoxLayout(inspector); properties->setContentsMargins(14, 14, 14, 14);
    auto* heading = new QLabel("控制点"); QFont font = heading->font(); font.setBold(true); heading->setFont(font);
    properties->addWidget(heading);
    pointLabel_ = new QLabel("在视图中点击一个控制点"); pointLabel_->setWordWrap(true); properties->addWidget(pointLabel_);
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
    properties->addLayout(form); apply_ = new QPushButton("应用坐标");
    connect(apply_, &QPushButton::clicked, this, &MainWindow::applyCoordinates); properties->addWidget(apply_);
    properties->addSpacing(18);
    auto* guide = new QLabel("左键选择控制点；Ctrl 或 Shift 点选可增减选择。正交视图空白处拖动可框选，拖动已选点可整体移动。\n\n右键拖动平移，滚轮缩放。\n\n透视图空白处左键拖动旋转。");
    guide->setWordWrap(true); guide->setStyleSheet("color: #586472;"); properties->addWidget(guide); properties->addStretch();
    auto* side = new QTabWidget;inspector_=side;side->setMinimumWidth(270);side->setMaximumWidth(390);
    side->addTab(inspector, "控制点");
    layerDialog_=new QDialog(this);layerDialog_->setWindowTitle("图层属性");layerDialog_->resize(800,727);layerDialog_->setMinimumSize(650,480);
    auto* layerLayout=new QVBoxLayout(layerDialog_);layerLayout->setContentsMargins(0,0,0,0);layers_=new LayerPanel;layerLayout->addWidget(layers_,1);
    auto* closeLayers=new QPushButton(studioIcon("Confirm"),"关闭");closeLayers->setProperty("layer_action","close");layerLayout->addWidget(closeLayers,0,Qt::AlignRight);
    connect(closeLayers,&QPushButton::clicked,layerDialog_,&QDialog::accept);
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
    side->hide();buildStudioShell(fit);layoutViews();
    session_.onUpdated = [this] { updateModel(); };
    session_.onStateChanged = [this] { updateState(); };
    session_.onError = [this](const QString& message) { showError(message); };
    session_.onCompleted = [this](const QString& method) { operationCompleted(method); };
    session_.onFileCommitted=[this](const QString& path){if(!workflow_)rememberFile(path);};
    session_.makeSavePreview=[this](std::shared_ptr<const fs::ModelSnapshot> model){return modelPreview(std::move(model),preferenceAppearance(preferences_));};
    session_.onHydroDefaults=[this](const QJsonObject& defaults){
        HydroDialog dialog(session_,hydroMode_,defaults,workflow_,this);hydroDialog_=&dialog;
        dialog.exec();hydroDialog_=nullptr;
        if(hydroWorkflow_)QTimer::singleShot(0,this,[this]{hydroStep("closed");});
    };
    session_.onHydrostatics=[this](const QJsonObject& result){if(hydroDialog_)hydroDialog_->receive(result);};
    session_.onPlates=[this](const QJsonObject& result){if(platesDialog_)platesDialog_->receive(result);};
    session_.onCad=[this](const QJsonObject& result){if(exchangeDialog_)exchangeDialog_->receiveCad(result);};
    session_.onPerformance=[this](const QJsonObject& result,bool defaults){
        if(performanceDialog_){performanceDialog_->receive(result,defaults);return;}
        if(!defaults)return;
        PerformanceDialog dialog(session_,result,workflow_,this);performanceDialog_=&dialog;
        dialog.exec();performanceDialog_=nullptr;
        if(performanceWorkflow_)QTimer::singleShot(0,this,[this]{performanceStep("closed");});
    };
    updateState(); updateCoordinates(); statusBar()->showMessage("打开一个 FBM 船模开始编辑。");
    if(rememberFiles)loadPreferences(PreferencesStore::defaultFilename());
}

MainWindow::~MainWindow() {
    session_.onUpdated = {}; session_.onStateChanged = {}; session_.onError = {}; session_.onCompleted = {};session_.onFileCommitted={};
}
void MainWindow::updateModel() {
    const auto changed=coordinatesChanged_;
    const std::array<double,3> draft{coordinates_[0]->value(),coordinates_[1]->value(),coordinates_[2]->value()};
    const auto model = session_.model(); const bool fresh = shownDocument_ != model->documentId;
    if (fresh) {selection_.clear();hiddenControlLayers_.clear();endFlowline();endPointPlacement();}
    for (auto* view : views_) view->setSnapshot(model);
    if (!fresh) selection_ = views_.front()->selectedPoints();
    if(pendingNewPointAfter_){for(const auto& point:model->points)if(point.id>=pendingNewPointAfter_)selection_={point.id};pendingNewPointAfter_=0;inspector_->show();inspector_->setCurrentIndex(0);}
    lines_->setScene(session_.scene(), fresh || shownUnits_ != model->units);
    shownDocument_ = model->documentId; shownUnits_ = model->units;
    layers_->setSnapshot(model);
    if(pendingNewLayerAfter_){
        for(const auto& layer:model->layers)if(layer.id>pendingNewLayerAfter_)layers_->setSelectedLayerId(layer.id);
        pendingNewLayerAfter_=0;
    }
    if(sections_)sections_->setSnapshot(model);
    selectPoints(selection_);
    if(selectionKind_!=HullEntity::Point)selectEntities(selectionKind_,fresh?QSet<fs::EntityId>{}:entitySelection_);
    if(preferencesPending_){for(size_t i=0;i<3;++i){QSignalBlocker block(coordinates_[i]);coordinates_[i]->setValue(draft[i]);}coordinatesChanged_=changed;}
}
void MainWindow::updateState() {
    if(preferencesAction_)preferencesAction_->setEnabled(!session_.busy());
    if(exchangeDialog_)exchangeDialog_->refreshState();
    for(auto* a:exchangeActions_)a->setEnabled(!session_.busy()&&(a->property("importing").toBool()||session_.ready()));
    if(performanceDialog_)performanceDialog_->refreshState();
    if(platesDialog_)platesDialog_->refreshState();
    if(platesAction_)platesAction_->setEnabled(session_.ready()&&!session_.busy());
    for(auto* action:performanceActions_)action->setEnabled(session_.ready()&&!session_.busy());
    if(hydroDialog_)hydroDialog_->refreshState();
    for(auto* action:hydroActions_)action->setEnabled(session_.ready()&&!session_.busy());
    const bool ready = session_.ready(), idle = !session_.busy(); open_->setEnabled(idle);
    if(placePoint_)placePoint_->setEnabled(ready&&idle);
    if(layerProperties_)layerProperties_->setEnabled(ready&&idle);
    if(layerUp_&&layerDown_){int at=-1;if(ready)for(int i=0;i<session_.model()->layers.size();++i)if(session_.model()->layers[i].id==layers_->selectedLayerId())at=i;
        layerUp_->setEnabled(ready&&idle&&at>0);layerDown_->setEnabled(ready&&idle&&at>=0&&at+1<session_.model()->layers.size());}
    for (auto* view : views_) view->setEditingEnabled(ready && idle);
    save_->setEnabled(ready && idle); saveAs_->setEnabled(ready && idle);
    project_->setEnabled(ready && idle); layers_->setEditingEnabled(ready && idle);
    if(sections_)sections_->setEditingEnabled(ready&&idle);
    for(auto* action:{sectionsAction_,newLayer_,deleteLayer_,emptyLayers_})if(action)action->setEnabled(ready&&idle);
    for(int i=0;i<4;++i)if(auto* action=displayActions_[i]){
        QSignalBlocker block(action);action->setEnabled(ready&&idle);
        if(i<2)action->setChecked(ready&&(i==0?session_.model()->bothSides:session_.model()->showCurvature));
        if(i>=2&&ready)action->setEnabled(idle&&session_.model()->showCurvature&&(i!=2||session_.model()->curvatureScale*1.1<=1e9));
    }
    for(int i=0;i<2;++i)if(auto* action=auxiliaryVisibility_[i]){QSignalBlocker block(action);action->setEnabled(ready&&idle);action->setChecked(ready&&(i==0?session_.model()->showControlCurves:session_.model()->showFlowlines));}
    if(addCurve_)addCurve_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Edge&&!entitySelection_.isEmpty());
    if(addFlow_)addFlow_->setEnabled(ready&&idle);
    const auto transformed=transformPoints();bool transformable=ready&&idle&&!transformed.isEmpty();
    updateModelingActions();
    if(transformable)for(auto id:transformed){auto* p=session_.model()->point(id);if(!p||p->locked||!p->batchMovable){transformable=false;break;}}
    for(int i=0;i<5;++i)if(auto* action=advancedActions_[i])action->setEnabled(transformable&&(i<3||(i==3&&selectionKind_==HullEntity::Edge)||(i==4&&selectionKind_==HullEntity::Face)));
    undo_->setEnabled(idle && session_.canUndo()); redo_->setEnabled(idle && session_.canRedo());
    png_->setEnabled(bool(session_.model())); dxf_->setEnabled(ready && idle);
    bool editable = ready && idle && !selection_.isEmpty();
    if (editable) for (auto id : selection_) {
        const auto* point = session_.model()->point(id);
        if (!point || point->locked || (selection_.size() > 1 && !point->batchMovable)) { editable = false; break; }
    }
    for (auto* coordinate : coordinates_) coordinate->setEnabled(editable);
    apply_->setEnabled(editable);
    if(addPoint_) {
        addPoint_->setEnabled(ready&&idle);addFace_->setEnabled(ready&&idle&&selectionOrder_.size()>=3);
        splitEdge_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Edge&&entitySelection_.size()==1);
        crease_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Edge&&!entitySelection_.isEmpty());
        smooth_->setEnabled(crease_->isEnabled());
        assignLayer_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Face&&!entitySelection_.isEmpty());
        delete_->setEnabled(ready&&idle&&(selectionKind_==HullEntity::Point?!selection_.isEmpty():!entitySelection_.isEmpty()));
        lock_->setEnabled(ready&&idle&&selectionKind_==HullEntity::Point&&!selection_.isEmpty());unlock_->setEnabled(lock_->isEnabled());
    }
    QString name = QFileInfo(session_.sourcePath()).fileName();
    if(name.isEmpty()&&!session_.recoveryOrigin().isEmpty())name=QFileInfo(session_.recoveryOrigin()).fileName()+"（恢复副本）";
    if (name.isEmpty()) name = "FreeShip_26";
    setWindowTitle(name + (session_.modified() ? " *" : "") + " · FreeShip_26 Qt 开发版");
    if (session_.busy()) statusBar()->showMessage("正在更新模型… 仍可平移和缩放视图。");
}
void MainWindow::selectPoint(fs::EntityId id) {
    selectPoints(id ? QSet<fs::EntityId>{id} : QSet<fs::EntityId>{});
}
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
    if (!session_.ready() || session_.busy()) return;
    ProjectDialog dialog(*session_.model(), this);
    if (dialog.exec() == QDialog::Accepted) session_.updateProject(dialog.changes());
}
void MainWindow::updateCoordinates() {
    const auto model = session_.model(); const auto* point = model ? model->point(selected_) : nullptr;
    const bool multiple = selection_.size() > 1;
    bool locked = false, constrained = false;
    if (model) for (auto id : selection_) { const auto* p = model->point(id); locked |= p && p->locked; constrained |= p && !p->batchMovable; }
    const QString restriction = locked ? " · 含锁定点" : constrained ? " · 含约束点，请逐点编辑" : "";
    pointLabel_->setText(multiple ? QString("已选 %1 个控制点%2\n输入各方向的移动距离。").arg(selection_.size()).arg(restriction) :
        point ? QString("控制点 %1%2").arg(selected_).arg(point->locked ? " · 已锁定" : "") : "在视图中点击或框选控制点");
    apply_->setText(multiple ? "移动所选控制点" : "应用坐标");
    const auto pos = point ? point->position : fs::Vec3{}; const std::array<double, 3> values{pos.x, pos.y, pos.z};
    fillingCoordinates_ = true;
    for (size_t i = 0; i < coordinates_.size(); ++i) {
        coordinateLabels_[i]->setText((multiple ? QString("Δ") : QString()) + QChar('X' + int(i)));
        coordinates_[i]->setSuffix(model && model->units == "imperial" ? " ft" : " m");
        coordinates_[i]->setValue(values[i]); coordinatesChanged_[i] = false;
    }
    fillingCoordinates_ = false;
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
    if (!session_.modified()) { next(); return; }
    const auto result = QMessageBox::question(this, "保存修改", "当前船模有未保存的修改，要先保存吗？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
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
        showError(session_.busy() ? "当前模型操作尚未完成。" : "计算组件不可用，无法保存当前修改。请保留已保存的文件。");
        return;
    }
    QString path = session_.sourcePath();
    if (saveAs || path.isEmpty()) {
        if(path.isEmpty()&&!session_.recoveryOrigin().isEmpty()){
            const QFileInfo original(session_.recoveryOrigin());path=original.absolutePath()+'/'+original.completeBaseName()+"-恢复.fbm";
        }
        path = QFileDialog::getSaveFileName(this, "保存船模", preferenceFileLocation("save",path), "FreeShip 船模 (*.fbm)");
        if (path.isEmpty()) { afterSave_ = {}; return; }
        if (QFileInfo(path).suffix().isEmpty()) path += ".fbm";
    }
    if (!session_.save(path)) afterSave_ = {};
}
void MainWindow::fitCurrent() {
    if (tabs_->currentIndex() == 1) lines_->fit(); else for (auto* view : views_) view->fit();
}
void MainWindow::exportImage() {
    if (!session_.model()) return;
    auto path = QFileDialog::getSaveFileName(this, "保存当前视图图片", preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()), "PNG 图片 (*.png)");
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".png";
    QImage pixels = tabs_->currentIndex() == 1 ? lines_->image() : tabs_->widget(0)->grab().toImage();
    QSaveFile target(path); target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly) || !pixels.save(&target, "PNG") || !target.commit())
        showError("图片保存失败，原文件未被替换：" + target.errorString());
    else statusBar()->showMessage("图片已保存：" + path);
}
void MainWindow::exportDxf() {
    if (!session_.ready() || session_.busy()) return;
    auto path = QFileDialog::getSaveFileName(this, "导出型线图 DXF", preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()), "DXF (*.dxf)");
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".dxf";
    try { ModelSession::atomicCopy(session_.dxfPath(), path); statusBar()->showMessage("DXF 已导出：" + path); }
    catch (const std::exception& ex) { showError(QString::fromUtf8(ex.what())); }
}
void MainWindow::showError(const QString& message) {
    closingAfterRecoveryDiscard_=false;
    if(preferencesPending_){preferencesFinished(message);return;}
    if(exchangeDialog_){exchangeDialog_->showFailure(message);return;}
    if(hydroDialog_){hydroDialog_->showFailure(message);return;}
    if(performanceDialog_){performanceDialog_->showFailure(message);return;}
    if(platesDialog_){platesDialog_->showFailure(message);return;}
    pendingNewLayerAfter_=0;
    pendingNewPointAfter_=0;
    if(!restoringFile_.isEmpty()){
        if(fileHistory_)fileHistory_->stopRestore(restoringFile_);
        restoringFile_.clear();afterSave_={};refreshRecentFiles();
        statusBar()->showMessage("上次的船模未能打开，请从“打开模型”重新选择。"+message);
        if(startupProbe_)startupStep("error",message);
        return;
    }
    if(startupProbe_){afterSave_={};statusBar()->showMessage(message);startupStep("error",message);return;}
    afterSave_ = {}; statusBar()->showMessage("操作未完成。当前画面已保留。");
    if(sections_&&sections_->isVisible()&&!workflow_){sections_->setSnapshot(session_.model());sections_->showError(message);return;}
    if (workflow_) finishWorkflow(message); else QMessageBox::warning(this, "操作未完成", message);
}
void MainWindow::operationCompleted(const QString& method) {
    if(method=="discard_recovery"&&closingAfterRecoveryDiscard_){QTimer::singleShot(0,qApp,&QApplication::quit);return;}
    if(method=="configure_preferences"&&preferencesPending_){preferencesFinished();return;}
    if((method=="import_model"||method=="append_model")&&exchangeDialog_)exchangeDialog_->imported();
    restoringFile_.clear();
    updateState();
    if(method=="hydro_settings"&&hydroDialog_)hydroDialog_->settingsSaved();
    if(method=="performance_settings"&&performanceDialog_)performanceDialog_->settingsSaved();
    statusBar()->showMessage(method == "save" ? "船模已保存：" + session_.sourcePath() +
        (session_.lastBackupPath().isEmpty()?QString{}:"；上一版备份："+session_.lastBackupPath()) :
        QString("%1 · %2 个控制点 · 更新 %3 ms · 右键平移，滚轮缩放").arg(QFileInfo(session_.sourcePath()).fileName())
            .arg(session_.model()->points.size()).arg(session_.lastOperationMs(), 0, 'f', 0));
    if(!workflow_&&(method=="load"||method=="save"))rememberFile(session_.sourcePath());
    if(!session_.recoveryError().isEmpty())statusBar()->showMessage("自动恢复副本未更新："+session_.recoveryError());
    else if(method=="load"&&session_.modified()&&session_.sourcePath().isEmpty())statusBar()->showMessage("已恢复未保存的船模，请另存为文件。");
    if(startupProbe_){startupStep(method);return;}
    if(preferencesWorkflow_){preferencesStep(method);return;}
    if(modelUiWorkflow_){modelUiStep();return;}
    if(viewportWorkflow_){viewportStep();return;}
    if(shadingWorkflow_){shadingStep();return;}
    if (workflow_) { if(exchangeWorkflow_)exchangeStep(method);else if(platesWorkflow_)platesStep(method);else if(performanceWorkflow_)performanceStep(method);else if(hydroWorkflow_)hydroStep(method);else if(modelingWorkflow_)modelingStep(method);else if(advancedWorkflow_)advancedStep(method);else if(auxiliaryWorkflow_)auxiliaryStep(method);else if(displayWorkflow_)displayStep(method);else if(backgroundWorkflow_)backgroundStep(method);else if(overlayWorkflow_)overlayStep(method);else if(sectionsWorkflow_)sectionsStep(method);else if(topologyWorkflow_)topologyStep(method);else if (editingWorkflow_) editingStep(method); else workflowStep(method); return; }
    if (method == "save" && afterSave_) {
        auto next = std::move(afterSave_); afterSave_ = {}; QTimer::singleShot(0, this, std::move(next));
    }
}
void MainWindow::openHydro(const QString& mode){if(session_.busy()||!session_.ready())return;hydroMode_=mode;session_.hydroDefaults();}
void MainWindow::openPerformance(const QString& method){if(!session_.busy()&&session_.ready())session_.performanceDefaults({{"calculator",method}});}
void MainWindow::openPlates(){
    if(session_.busy()||!session_.ready())return;
    PlatesDialog dialog(session_,workflow_,this);platesDialog_=&dialog;
    dialog.exec();platesDialog_=nullptr;
    if(platesWorkflow_)QTimer::singleShot(0,this,[this]{platesStep("closed");});
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (session_.busy()) { event->ignore(); statusBar()->showMessage("请等当前模型操作完成后再关闭。"); }
    else if (session_.modified()) {
        event->ignore(); withSavedChanges([this] {
            closingAfterRecoveryDiscard_=true;
            if(!session_.discardRecovery())closingAfterRecoveryDiscard_=false;
        });
    } else event->accept();
}
void MainWindow::workflowCase(const QString& name, bool pass, const QString& message) {
    workflowCases_.append(QJsonObject{{"name", name}, {"status", pass ? "pass" : "fail"}, {"message", message}});
}
void MainWindow::verifyWorkflow(const QString& path, const QString& output) {
    workflow_ = true; workflowOutput_ = QFileInfo(output).absoluteFilePath(); workflowTimer_.start();
    if (!QDir().mkpath(workflowOutput_ + "/views")) { finishWorkflow("Cannot create workflow output"); return; }
    ensurePolished(); if (centralWidget()->layout()) centralWidget()->layout()->activate();
    for (auto* view : views_) view->resize(640, 400);
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
