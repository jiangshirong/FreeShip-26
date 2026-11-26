#include "about_dialog.h"
#include "shortcuts_dialog.h"
#include "tutorial_dialog.h"
#include "update_controller.h"
#include "app/ui_language.h"
#include "core/model_checks.h"
#include "main_window.h"
#include "point_coordinate_popup.h"
#include "studio_view_grid.h"
#include "section_dialog.h"
#include "station_template_dialog.h"
#include "station_comparison_dialog.h"
#include <QDir>
#include <QFileInfo>
#include "history_shortcuts.h"
#include "layer_panel.h"
#include <QLineEdit>
#include "studio_ui.h"
#include "layout_motion.h"
#include "data_motion.h"
#include "studio_caption.h"
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QSettings>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace {
using DisplayFlag=bool fs::ModelSnapshot::*;
const QHash<QString,DisplayFlag> DisplayFlags{
    {"both_sides",&fs::ModelSnapshot::bothSides},
    {"show_curvature",&fs::ModelSnapshot::showCurvature},
    {"show_control_curves",&fs::ModelSnapshot::showControlCurves},
    {"show_flowlines",&fs::ModelSnapshot::showFlowlines},
    {"show_normals",&fs::ModelSnapshot::showNormals},
    {"show_markers",&fs::ModelSnapshot::showMarkers}
};
bool hasDisplayData(const fs::ModelSnapshot* model,const QString& key){
    if(!model)return false;
    if(key=="show_markers")return !model->markers.isEmpty();
    if(key=="show_flowlines")return !model->flowlines.isEmpty();
    if(key=="show_control_curves")return !model->controlCurves.isEmpty();
    if(key=="show_normals")return !model->faces.isEmpty();
    if(key=="show_curvature"||key=="curvature_scale")return !model->controlCurves.isEmpty()||!model->sections.isEmpty();
    return !model->points.isEmpty();
}
QJsonValue displaySetting(const fs::ModelSnapshot& model,const QString& key){
    if(key=="curvature_scale")return model.curvatureScale;
    const auto flag=DisplayFlags.value(key);
    return flag?QJsonValue(model.*flag):QJsonValue{};
}
void applyDisplaySettings(fs::ModelSnapshot& preview,const QJsonObject& changes){
    for(auto item=changes.begin();item!=changes.end();++item){
        if(item.key()=="curvature_scale")preview.curvatureScale=item.value().toDouble();
        else if(const auto flag=DisplayFlags.value(item.key()))preview.*flag=item.value().toBool();
    }
}

QJsonArray ids(const QSet<fs::EntityId>& values) {
    auto ordered=values.values();std::sort(ordered.begin(),ordered.end());
    QJsonArray result;for(auto id:ordered)result.append(double(id));return result;
}
fs::EntityId chooseLayer(const fs::ModelSnapshot& model,QWidget* parent,const QString& title,bool hidden,const char* operation) {
    QDialog dialog(parent);setStudioWindowIcon(&dialog,"Layers");dialog.setWindowTitle(title);
    dialog.setProperty("modeling_operation",QString::fromLatin1(operation));
    dialog.setAttribute(Qt::WA_DontShowOnScreen,hidden);
    auto* layout=new QVBoxLayout(&dialog);auto* field=new QComboBox;
    for(const auto& layer:model.layers)field->addItem(layerDisplayName(layer),QVariant::fromValue(layer.id));
    layout->addWidget(field);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    return runStudioDialog(dialog)==QDialog::Accepted?field->currentData().toULongLong():0;
}
}
bool MainWindow::canEditDisplay() const {
    return session_.ready()&&(!session_.busy()||!submittedDisplay_.isEmpty());
}
void MainWindow::loadDisplayDefaults(){
    restoredDisplay_={};
    if(!preferencesStore_)return;
    QSettings store(preferencesStore_->filename()+".views.ini",QSettings::IniFormat);
    store.beginGroup("visibility");
    for(const auto& key:store.childKeys()){
        const auto value=store.value(key);
        if(DisplayFlags.contains(key))restoredDisplay_[key]=value.toBool();
        else if(key=="curvature_scale"){
            const double scale=value.toDouble();
            if(scale>=1e-9 && scale<=1e9)restoredDisplay_[key]=scale;
        }
    }
}
void MainWindow::rememberDisplayChanges(const QJsonObject& changes){
    if(!preferencesStore_)return;
    QSettings store(preferencesStore_->filename()+".views.ini",QSettings::IniFormat);
    store.beginGroup("visibility");
    for(auto it=changes.begin();it!=changes.end();++it){
        store.setValue(it.key(),it.value().toVariant());
        // Subsequent edits and undo in this document follow its live snapshot.
        restoredDisplay_.remove(it.key());
    }
}
std::shared_ptr<const fs::ModelSnapshot> MainWindow::withDisplayDefaults(std::shared_ptr<const fs::ModelSnapshot> model)const{
    if(model)for(auto it=restoredDisplay_.begin();it!=restoredDisplay_.end();++it){
        if(displaySetting(*model,it.key())==it.value())continue;
        auto shown=std::make_shared<fs::ModelSnapshot>(*model);
        applyDisplaySettings(*shown,restoredDisplay_);
        return shown;
    }
    return model;
}
std::shared_ptr<const fs::ModelSnapshot> MainWindow::displayModel() const {
    const auto model=withDisplayDefaults(session_.model());
    if(!model||displayDocument_!=model->documentId||(submittedDisplay_.isEmpty()&&queuedDisplay_.isEmpty()))
        return model;
    // The vectors are implicitly shared. Only these display fields differ from
    // the authoritative snapshot used to validate edits and revisions.
    auto preview=std::make_shared<fs::ModelSnapshot>(*model);
    applyDisplaySettings(*preview,submittedDisplay_);
    applyDisplaySettings(*preview,queuedDisplay_);
    return preview;
}
void MainWindow::bindDisplayToggle(QAction* action) {
    const auto key=action->property("operation").toString();
    action->setCheckable(true);
    displayToggles_.append(action);
    connect(action,&QAction::triggered,this,[this,key](bool checked){requestDisplayChange(key,checked);});
}
void MainWindow::requestDisplayChange(const QString& key,const QJsonValue& value) {
    if(!canEditDisplay()||!hasDisplayData(session_.model().get(),key))return;
    displayDocument_=session_.model()->documentId;
    queuedDisplay_.insert(key,value);
    if(session_.busy())updateDisplayControls();
    else submitDisplayChanges();
}
void MainWindow::submitDisplayChanges() {
    const auto model=session_.model();
    if(!session_.ready()||displayDocument_!=model->documentId){
        clearDisplayChanges();
        return;
    }
    if(session_.busy())return;
    auto changes=std::exchange(queuedDisplay_,{});
    for(auto it=changes.begin();it!=changes.end();){
        if(it.value()==displaySetting(*model,it.key())){
            rememberDisplayChanges({{it.key(),it.value()}});
            it=changes.erase(it);
        }
        else ++it;
    }
    if(changes.isEmpty()){
        clearDisplayChanges();
        return;
    }
    submittedDisplay_=std::move(changes);
    if(!session_.editDisplay(submittedDisplay_))clearDisplayChanges();
}
void MainWindow::clearDisplayChanges() {
    submittedDisplay_={};
    queuedDisplay_={};
    displayDocument_.clear();
    updateDisplayControls();
}
void MainWindow::updateDisplayControls() {
    const auto model=displayModel();
    const bool enabled=canEditDisplay();
    for(auto* toggle:displayToggles_){
        const auto key=toggle->property("operation").toString();
        toggle->setEnabled(enabled&&hasDisplayData(model.get(),key));
        toggle->setChecked(model&&displaySetting(*model,key).toBool());
    }
    for(auto* action:findChildren<QAction*>()){
        const auto operation=action->property("operation").toString();
        if(operation=="section_visibility"){
            const int kind=action->property("section_kind").toInt();
            action->setEnabled(model&&std::any_of(model->sections.cbegin(),model->sections.cend(),[kind](const auto& section){return section.kind==kind;}));
        }else if(operation=="overlay_hydro")action->setEnabled(model&&model->overlay.hydroAvailable);
    }
    const bool canScale=enabled&&hasDisplayData(model.get(),"curvature_scale")&&model->showCurvature;
    if(increaseCurvature_)increaseCurvature_->setEnabled(canScale&&model->curvatureScale*1.1<=1e9);
    if(decreaseCurvature_)decreaseCurvature_->setEnabled(canScale);
    // An opening view can already show a first frame while the session has no
    // committed model. Keep the saved visibility used to prepare that frame.
    if(model&&model->documentId==shownDocument_)
        for(auto* view:views_)view->setDisplayPreview(model);
}
void MainWindow::buildStudioShell() {
    auto action=[this](const QString& text,const QString& icon,const QString& operation={}) {
        auto* a=new QAction(studioIcon(icon),text,this);a->setObjectName("Studio"+icon);
        if(!operation.isEmpty())a->setProperty("operation",operation);
        static const QSet<QString> windows{"add_face","assign_layer","delete","layer_properties",
            "translate","scale","rotate","extrude","mirror",
            "model_check","compare_models","develop_plates","sections","lines_window","shortcut_guide","tutorial"};
        if(windows.contains(operation)||operation.startsWith("hydro_")||operation.startsWith("performance_"))
            a->setProperty("studio_opens_dialog",true);
        addAction(a);return a;
    };
    auto group=[&](const QString& text,const QString& icon,QMenu*& menu) {
        auto* a=action(text,icon);menu=new QMenu(text,this);a->setMenu(menu);return a;
    };
    auto* root=primaryMenu_;
    root->insertAction(open_,preferencesAction_);
    for(auto* child:root->actions())addAction(child);
    menuBar()->hide();auto* menu=action(uiText("菜单"),"Menu");menu->setMenu(root);
    open_->setIcon(studioIcon("Open"));save_->setIcon(studioIcon("Save"));
    QMenu *selectionMenu,*pointMenu,*edgeMenu,*faceMenu,*visibilityMenu;
    auto* selectionModes=group(uiText("选择"),"SelectAll",selectionMenu);
    selectionModes->setObjectName("StudioSelection");selectionModes->setProperty("operation","selection_modes");
    auto* edges=group(uiText("边"),"Edge",edgeMenu);auto* faces=group(uiText("面"),"Face",faceMenu);
    auto* points=group(uiText("点"),"Point",pointMenu);
    auto* visibility=group(uiText("视图"),"Observe",visibilityMenu);
    auto* choices=new QActionGroup(this);choices->setExclusive(true);
    auto selection=[&](const QString& text,const QString& icon,HullEntity kind) {
        const auto key=kind==HullEntity::Point?"point":kind==HullEntity::Edge?"edge":kind==HullEntity::Face?"face":kind==HullEntity::Curve?"curve":"flow";
        auto* a=action(text,icon,"select_"+QString(key));
        a->setCheckable(true);a->setChecked(kind==HullEntity::Point);choices->addAction(a);selectionMenu->addAction(a);
        connect(a,&QAction::triggered,this,[this,kind]{setSelectionKind(kind);});
    };
    selection(uiText("选择控制点"),"Point",HullEntity::Point);
    selection(uiText("选择边"),"Edge",HullEntity::Edge);selection(uiText("选择面"),"Face",HullEntity::Face);
    selection(uiText("选择控制曲线"),"NewCurve",HullEntity::Curve);
    selection(uiText("选择流线"),"Flowlines",HullEntity::Flowline);
    for(const auto& [label,operation]:QList<QPair<QString,QString>>{{"全选点","select_all"},{"反选点","invert_selection"}}){
        auto* command=action(uiText(label),"SelectAll",operation);
        selectionMenu->addAction(command);modelingActions_.append(command);
        connect(command,&QAction::triggered,this,[this,operation]{modelingAction(operation);});
    }
    addCurve_=action(uiText("由所选边生成控制曲线"),"NewCurve","create_control_curve");edgeMenu->addAction(addCurve_);
    connect(addCurve_,&QAction::triggered,this,[this]{session_.editAuxiliary("create_control",{{"ids",ids(entitySelection_)}});});
    placePoint_=action(uiText("放置控制点"),"Point","place_point");placePoint_->setCheckable(true);pointMenu->addAction(placePoint_);
    connect(placePoint_,&QAction::triggered,this,[this](bool enabled){if(enabled)beginPointPlacement();else endPointPlacement();});
    lock_=action(uiText("锁定所选点"),"Lock","set_locked");unlock_=action(uiText("解锁所选点"),"Unlock","set_unlocked");
    pointMenu->addAction(lock_);pointMenu->addAction(unlock_);
    connect(lock_,&QAction::triggered,this,[this]{session_.editTopology("set_locked",{{"ids",ids(selection_)},{"locked",true}});});
    connect(unlock_,&QAction::triggered,this,[this]{session_.editTopology("set_locked",{{"ids",ids(selection_)},{"locked",false}});});
    crease_=action(uiText("设置折痕"),"CreaseEdge","set_crease");smooth_=action(uiText("取消折痕"),"CreaseEdge","clear_crease");
    edgeMenu->addAction(crease_);edgeMenu->addAction(smooth_);
    connect(crease_,&QAction::triggered,this,[this]{session_.editTopology("set_crease",{{"ids",ids(entitySelection_)},{"crease",true}});});
    connect(smooth_,&QAction::triggered,this,[this]{session_.editTopology("set_crease",{{"ids",ids(entitySelection_)},{"crease",false}});});
    addFace_=action(uiText("由所选点创建面…"),"Face","add_face");faceMenu->addAction(addFace_);
    addFace_->setToolTip(uiText("按轮廓顺序选择至少三个控制点，再创建面。"));
    connect(addFace_,&QAction::triggered,this,&MainWindow::createFace);
    assignLayer_=action(uiText("将所选面移至图层…"),"Layers","assign_layer");faceMenu->addAction(assignLayer_);
    connect(assignLayer_,&QAction::triggered,this,&MainWindow::assignFaceLayer);
    delete_=action(uiText("删除所选对象"),"DeleteSelected","delete");delete_->setShortcut(QKeySequence::Delete);
    delete_->setProperty("shortcut_target","所选对象");
    delete_->setProperty("studio_opens_dialog",true);
    connect(delete_,&QAction::triggered,this,&MainWindow::deleteSelection);
    layerProperties_=action(uiText("图层","collection"),"Layers","layer_properties");
    bindUiText(layerProperties_,"text","图层","collection");visibilityMenu->addAction(layerProperties_);
    layerProperties_->setProperty("studio_opens_dialog",true);
    connect(layerProperties_,&QAction::triggered,this,&MainWindow::openLayers);
    auto* sides=action(uiText("双舷显示"),"BothSides","both_sides");bindDisplayToggle(sides);visibilityMenu->addAction(sides);
    auto* curvature=action(uiText("曲率梳"),"Curvature","show_curvature");bindDisplayToggle(curvature);
    auto* normals=action(uiText("法向"),"Normals","show_normals");bindDisplayToggle(normals);
    increaseCurvature_=action(uiText("增大曲率梳"),"ZoomIn","curvature_increase");
    decreaseCurvature_=action(uiText("减小曲率梳"),"ZoomOut","curvature_decrease");
    connect(increaseCurvature_,&QAction::triggered,this,[this]{requestDisplayChange("curvature_scale",double(float(displayModel()->curvatureScale*1.1)));});
    connect(decreaseCurvature_,&QAction::triggered,this,[this]{requestDisplayChange("curvature_scale",double(float(displayModel()->curvatureScale/1.1)));});
    auto* grid=action(uiText("网格"),"Grid","overlay_grid");grid->setProperty("viewPreference","overlay_grid");grid->setCheckable(true);grid->setChecked(true);visibilityMenu->addAction(grid);
    auto* hydro=action(uiText("静水力数据"),"Hydrostatics","overlay_hydro");hydro->setProperty("viewPreference","overlay_hydro");hydro->setCheckable(true);hydro->setChecked(true);visibilityMenu->addAction(hydro);
    auto* curveVisible=action(uiText("控制曲线"),"NewCurve","show_control_curves");bindDisplayToggle(curveVisible);visibilityMenu->addAction(curveVisible);
    auto* flowMenu=visibilityMenu->addMenu(uiText("流线"));
    flowMenu->setIcon(studioIcon("Flowlines"));
    auto* flowVisible=action(uiText("显示流线"),"Flowlines","show_flowlines");bindDisplayToggle(flowVisible);flowMenu->addAction(flowVisible);
    addFlow_=action(uiText("添加流线"),"Flowlines","create_flowline");addFlow_->setCheckable(true);flowMenu->addAction(addFlow_);
    connect(addFlow_,&QAction::triggered,this,[this](bool checked){if(checked)beginFlowline();else endFlowline();});
    auto* markers=action(uiText("参考曲线"),"Markers","show_markers");bindDisplayToggle(markers);
    connect(grid,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setGridVisible(value);});
    connect(hydro,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setHydroVisible(value);});
    connect(visibilityMenu,&QMenu::aboutToShow,this,[this,grid,hydro]{
        QSignalBlocker a(grid),b(hydro);
        grid->setChecked(views_.front()->gridVisible());
        hydro->setChecked(views_.front()->hydroVisible());
    });
    const QStringList operations{"translate","scale","rotate","extrude","mirror"};
    const QStringList transformNames{uiText("移动所选对象…"),uiText("缩放所选对象…"),uiText("旋转所选对象…"),uiText("挤出所选边…"),uiText("镜像所选面…")};
    const QStringList transformIcons{"Move","Scale","RotateCW","ExtrudeEdge","Mirror"};
    QList<QAction*> transforms;
    for(int i=0;i<5;++i){auto* a=action(transformNames[i],transformIcons[i],operations[i]);advancedActions_[i]=a;
        if(i<3)transforms.append(a);else (i==3?edgeMenu:faceMenu)->addAction(a);
        connect(a,&QAction::triggered,this,[this,operation=operations[i]]{advancedDialog(operation);});}
    QMenu* checksMenu=nullptr;
    auto* checks=group(uiText("模型检查"),"ModelCheck",checksMenu);
    auto* checkModel=action(uiText("拓扑检查报告…"),"ModelCheck","model_check");
    checkModel->setProperty("studio_opens_dialog",true);
    checksMenu->addAction(checkModel);modelingActions_.append(checkModel);
    connect(checkModel,&QAction::triggered,this,&MainWindow::modelCheckDialog);
    auto* tools=group(uiText("工具"),"Tools",toolsMenu_);
    bindUiText(tools,"text","工具");bindUiText(toolsMenu_,"title","工具");
    compareModels_=action(uiText("比较船模…"),"Sections","compare_models");
    compareModels_->setObjectName("StudioCompareModels");
    connect(compareModels_,&QAction::triggered,this,[this]{openHullComparison();});
    QMenu* calculationsMenu=nullptr;auto* calculations=group(uiText("计算","menu"),"Calculate",calculationsMenu);
    bindUiText(calculations,"text","计算","menu");bindUiText(calculationsMenu,"title","计算","menu");
    platesAction_=action(uiText("板材展开"),"Develop","develop_plates");
    connect(platesAction_,&QAction::triggered,this,&MainWindow::openPlates);
    toolsMenu_->addActions({compareModels_,platesAction_});
    buildModelingActions(pointMenu,edgeMenu,faceMenu,visibilityMenu,transforms,checksMenu);
    visibilityMenu->addActions({markers,normals,curvature,decreaseCurvature_,increaseCurvature_});
    for(auto* view:views_){
        view->onPopulateSelectionMenu=[this,transforms](QMenu& menu){
            if(transformPoints().isEmpty())return false;
            for(auto* source:transforms){
                auto* entry=new QAction(&menu);
                entry->setProperty("operation",source->property("operation"));
                entry->setProperty("studio_opens_dialog",true);
                const auto sync=[entry,source]{entry->setText(source->text());entry->setIcon(source->icon());entry->setEnabled(source->isEnabled());};
                sync();
                connect(source,&QAction::changed,entry,sync);
                // Open dialogs after the self-deleting popup finishes mouse dispatch.
                connect(entry,&QAction::triggered,source,&QAction::trigger,Qt::QueuedConnection);
                menu.addAction(entry);
            }
            return true;
        };
        view->onPopulateContextMenu=[](QMenu& menu){
            for(auto* item:menu.actions()){
                const auto operation=item->property("view_operation").toString();
                if(operation=="save_image")setStudioFormatMenuAction(&menu,item,"png");
                else if(operation=="export_pdf")setStudioFormatMenuAction(&menu,item,"pdf");
            }
        };
    }
    const QStringList hydroModes{"single","table","cross"},hydroNames{uiText("设计静水力…"),uiText("静水力吃水表…"),uiText("稳性曲线与平衡…")};
    for(int i=0;i<3;++i){auto* a=action(hydroNames[i],"Hydrostatics","hydro_"+hydroModes[i]);hydroActions_.append(a);calculationsMenu->addAction(a);
        connect(a,&QAction::triggered,this,[this,mode=hydroModes[i]]{openHydro(mode);});}
    auto* resistanceMenu=calculationsMenu->addMenu(uiText("阻力计算"));auto* propellerMenu=calculationsMenu->addMenu(uiText("螺旋桨与推进"));
    resistanceMenu->setIcon(studioIcon("Resistance"));propellerMenu->setIcon(studioIcon("Propeller"));
    const auto addPerformanceAction=[this,&action](QMenu* menu,const QString& key,const QString& source,const QString& icon){
        auto* entry=action({},icon,"performance_"+key);
        bindUiText(entry,"text",source);
        performanceActions_.append(entry);
        menu->addAction(entry);
        connect(entry,&QAction::triggered,this,[this,key]{openPerformance(key);});
    };
    const QVector<QPair<QString,QString>> methods{
        {"delft","Delft 帆船阻力"},{"holtrop","Holtrop 海船阻力"},{"hollenbach","Hollenbach 阻力"},
        {"oortmerssen","van Oortmerssen 阻力"},{"fung","Fung–Leibman 阻力"},{"ost","OST 阻力"},
        {"rbhs","圆舭高速艇阻力"},{"mh","多体船阻力"},{"kaper","KAPER 独木舟及皮艇阻力"},{"planing","滑行艇阻力"},
        {"propeller1","螺旋桨优化与主机选型"},{"propeller2","主机与螺旋桨匹配"},{"propeller3","船舶推进特性图（Passport）"},{"propeller4","螺旋桨初步设计"},{"propeller5","螺旋桨系列计算"}};
    for(const auto& [key,source]:methods)
        addPerformanceAction(key.startsWith("propeller")?propellerMenu:resistanceMenu,key,source,"Resistance");
    auto* hydrodynamicsMenu=calculationsMenu->addMenu(uiText("水动力专用工具"));
    hydrodynamicsMenu->setIcon(studioIcon("Hydrostatics"));
    for(const auto& [key,source]:QVector<QPair<QString,QString>>{{"wind","空气动力特性"},{"maneuver","操纵性预估"},{"acceleration","加速与制动"},{"addmass","附加质量计算"}})
        addPerformanceAction(hydrodynamicsMenu,key,source,"Calculate");
    auto* sections=action(uiText("剖面线"),"Sections","sections");sectionsAction_=sections;
    sections->setProperty("studio_opens_dialog",true);
    connect(sections,&QAction::triggered,this,&MainWindow::editSections);
    const std::array<QString,4> sectionNames{uiText("横剖线"),uiText("纵剖线"),uiText("水线"),uiText("斜剖线")};
    visibilityMenu->addSeparator();
    for(int i=0;i<4;++i){auto* a=action(sectionNames[i],"Sections","section_visibility");a->setProperty("section_kind",i+1);a->setProperty("viewPreference","section_"+QString::number(i+1));a->setCheckable(true);a->setChecked(true);visibilityMenu->addAction(a);
        connect(a,&QAction::toggled,this,[this,i](bool value){for(auto* view:views_)view->setSectionVisible(i+1,value);});}
    auto* lines=action(uiText("型线图"),"LinesPlan","lines_window");linesAction_=lines;connect(lines,&QAction::triggered,this,&MainWindow::openLinesWindow);
    lines->setProperty("studio_opens_dialog",true);
    auto* net=action(uiText("控制网格"),"ControlNet");net->setProperty("viewPreference","control_net");net->setCheckable(true);net->setChecked(true);
    connect(net,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setControlNetVisible(value);});
    for(auto* view:views_)view->onShowControlNet=[net]{if(!net->isChecked())net->trigger();};
    const std::array<QString,4> names{uiText("纵剖面"),uiText("横剖面"),uiText("俯视图"),uiText("透视图")};
    const std::array<QString,4> icons{"ViewProfile","ViewBody","ViewPlan","ViewPerspective"};
    for(size_t i=0;i<4;++i){
        auto* a=action(names[i],icons[i]);viewActions_[i]=a;a->setProperty("viewPreference","visible_"+QString::number(i));a->setCheckable(true);a->setChecked(true);
        connect(a,&QAction::triggered,this,[this,a]{
            if(std::none_of(viewActions_.begin(),viewActions_.end(),[](auto* item){return item->isChecked();}))a->setChecked(true);
            layoutViews();
        });
    }
    auto* about=action(uiText("关于"),"Info");
    about->setProperty("studio_opens_dialog",true);
    about->setProperty("operation","about");
    installStudioUpdates(this,about,workflow_,[this](std::function<void()> action){
        if(documentWindows().size()>1){showError(uiText("请先关闭其他船模窗口，再安装更新。"));return;}
        withSavedChanges(std::move(action));});
    connect(about,&QAction::triggered,this,[this]{showFreeShipAbout(this);});
    auto* bar=new StudioBar(this);studioBar_=bar;
    auto* shortcuts=action(uiText("快捷方式"),"Shortcuts","shortcut_guide");
    shortcuts->setProperty("studio_opens_dialog",true);
    connect(shortcuts,&QAction::triggered,this,[this]{
        while(showShortcutGuide(this,languageShortcut_,[this]{switchUiLanguage();})){}
    });
    bar->addGroup({menu,selectionModes,points,edges,faces,visibility,checks,tools,calculations},QColor(237,244,248));
    bar->addGroup({project_,sections,lines},QColor(248,243,232));
    bar->addGroup({net,viewActions_[1],viewActions_[0],viewActions_[2],viewActions_[3]},QColor(229,239,223));
    auto* tutorial=action(uiText("教程"),"Tutorial","tutorial");tutorial->setProperty("studio_opens_dialog",true);
    connect(tutorial,&QAction::triggered,this,[this]{showTutorialGuide(this);});
    bar->addGroup({tutorial,shortcuts,about},QColor(241,235,243));
    auto* header=new QWidget(this);auto* headerLayout=new QVBoxLayout(header);
    headerLayout->setContentsMargins(0,0,0,0);headerLayout->setSpacing(0);
    if(!workspace_)headerLayout->addWidget(createStudioCaption(this));
    headerLayout->addWidget(bar);setMenuWidget(header);
    addToolBar(Qt::RightToolBarArea,bar->sideBar());
}
void MainWindow::layoutViews() {
    layout_motion::animateViewLayout(viewGrid_->parentWidget(),QList<QWidget*>{viewFrames_[0],viewFrames_[1],viewFrames_[2],viewFrames_[3]},[this]{
    for(auto* frame:viewFrames_)viewGrid_->removeWidget(frame);
    int count=0;for(int index:{1,0,2,3})if(viewActions_[index]->isChecked())++count;
    int at=0;for(int index:{1,0,2,3}) {
        bool visible=viewActions_[index]->isChecked();viewFrames_[index]->setVisible(visible);
        if(visible){
            viewGrid_->addWidget(viewFrames_[index],at/2,at%2);
            const bool right=at%2==0,bottom=count>2&&at<2;
            views_[index]->setDisplayModeButtonCorner(bottom?(right?Qt::BottomRightCorner:Qt::BottomLeftCorner):
                                                             (right?Qt::TopRightCorner:Qt::TopLeftCorner));
            ++at;
        }
    }
    studio_view_grid::setVisibleCount(viewGrid_,count);
    });
}
void MainWindow::openHullComparison(int index,QWidget* sectionEditor){
    if(!session_.ready() || session_.busy())return;
    QVector<ComparisonSource> sources;
    for(auto* other:documentWindows()){
        const auto& session=other->session_;
        if(other==this || !session.ready() || session.busy())continue;
        QString label=QFileInfo(other->documentDisplayPath()).fileName();
        if(label.isEmpty())label=uiText("未命名船模");
        if(session.modified())label+=" *";
        sources.append({label,QFileInfo(session.modelPath()).dir().filePath("native-input.json"),session.model(),session.calculationStatePath()});
    }
    QString currentName=QFileInfo(documentDisplayPath()).fileName();
    if(currentName.isEmpty())currentName=uiText("未命名船模");
    auto* dialog=new StationComparisonDialog(session_.model(),
        QFileInfo(session_.modelPath()).dir().filePath("native-input.json"),index,
        sectionEditor?sectionEditor:this,std::move(sources),!sectionEditor,session_.calculationStatePath(),currentName);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    showStudioDialog(*dialog);
}
void MainWindow::editSections(){
    if(!session_.ready())return;
    if(!sections_){
        sections_=new SectionDialog(this);sections_->setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        bindHistoryShortcut(sections_,undo_,QKeySequence(Qt::CTRL | Qt::Key_Z));
        bindHistoryShortcut(sections_,redo_,QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
        sections_->onPreviewHistory=[this](bool redo){return session_.sectionPreviewHistory(redo);};
        sections_->onPredictionMismatch=[this]{session_.dropQueuedSectionCommands();};
        sections_->onEdit=[this](const QString& operation,int kind,QJsonObject fields){return session_.editSections(operation,kind,std::move(fields));};
        sections_->onSelection=[this](int kind,QSet<int> ids){for(auto* view:views_)view->highlightSections(kind,ids);};
        sections_->onComparison=[this](int index){openHullComparison(index,sections_);};
        sections_->onTemplates=[this](QSet<int> ids){
            if(!session_.ready() || session_.busy()) return;
            const auto source=QFileInfo(session_.sourcePath());
            auto* dialog=new StationTemplateDialog(session_.model(),QFileInfo(session_.modelPath()).dir().filePath("native-input.json"),
                    source.dir().filePath(source.completeBaseName()+"-stations"),ids,sections_);
            dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);
            showStudioDialog(*dialog);
        };
        connect(sections_,&QDialog::finished,this,[this]{for(auto* view:views_)view->highlightSections(0,{});});
    }
    sections_->setSnapshot(session_.model());sections_->setEditingEnabled(!session_.busy()||session_.canQueueSectionEdits());sections_->show();sections_->raise();sections_->activateWindow();
}
void MainWindow::openLayers(){
    if(!session_.ready())return;
    layerDialog_->setAttribute(Qt::WA_DontShowOnScreen,workflow_);layerDialog_->show();layerDialog_->raise();layerDialog_->activateWindow();
}
void MainWindow::createLayer(){
    if(session_.ready())layers_->previewCreate();
}
void MainWindow::deleteLayer(fs::EntityId id){
    if(!session_.ready()||!layers_->canDelete(id))return;
    layers_->setSelectedLayerId(id);
    layers_->previewDelete(id);
}
void MainWindow::setSelectionKind(HullEntity kind) {
    endFlowline();endPointPlacement();
    selectionKind_=kind;entitySelection_.clear();
    const QString operation="select_"+QString(kind==HullEntity::Point?"point":kind==HullEntity::Edge?"edge":kind==HullEntity::Face?"face":kind==HullEntity::Curve?"curve":"flow");
    for(auto* action:findChildren<QAction*>())if(action->property("operation")==operation)action->setChecked(true);
    if(kind!=HullEntity::Point)selectPoints({});
    for(auto* view:views_)view->setSelectionKind(kind);
    tabs_->setCurrentIndex(0);updateState();
}
void MainWindow::selectEntities(HullEntity kind,QSet<fs::EntityId> selected) {
    selectionKind_=kind;if(!selection_.isEmpty())selectPoints({});
    entitySelection_.clear();
    for(auto* view:views_){
        if(view->selectionKind()!=kind){view->setSelectedEntities({});continue;}
        view->setSelectedEntities(selected);entitySelection_.unite(view->selectedEntities());
    }
    updateState();
    QStringList identity;auto ids=entitySelection_.values();std::sort(ids.begin(),ids.end());for(auto id:ids)identity.append(QString::number(id));
    data_motion::pulseProperties(inspector_,QString::number(int(kind))+":"+identity.join(','));
}
void MainWindow::createFace() {
    if(!session_.ready()||session_.busy()||selectionOrder_.size()<3)return;
    const auto model=session_.model();
    auto points=pointSelectionHasBatchOrder_?fs::faceBoundaryOrder(*model,selection_):selectionOrder_;
    if(points.isEmpty()&&views_.front()->snapshot()==model){
        const auto boundary=views_.front()->selectedMissingFace(selection_);
        points=fs::faceBoundaryOrder(*model,boundary);
    }
    if(points.isEmpty()){
        showError(uiText("无法从所选点确定可补面的闭环。请选择一个缺口，或沿轮廓逐点选择。"));
        return;
    }
    QJsonArray ordered;for(auto id:points)ordered.append(double(id));
    const auto layer=chooseLayer(*model,this,uiText("新建面的图层"),workflow_,"add_face");if(!layer)return;
    if(!session_.ready()||session_.busy()||session_.model()!=model){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
    session_.editTopology("add_face",{{"ids",ordered},{"layer_id",double(layer)}});
}
void MainWindow::assignFaceLayer() {
    if(!session_.ready()||session_.busy()||entitySelection_.isEmpty())return;
    const auto model=session_.model();const auto selected=ids(entitySelection_);
    const auto layer=chooseLayer(*model,this,uiText("所选面的图层"),workflow_,"assign_layer");if(!layer)return;
    if(!session_.ready()||session_.busy()||session_.model()!=model){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
    session_.editTopology("assign_layer",{{"ids",selected},{"layer_id",double(layer)}});
}
void MainWindow::deleteSelection() {
    const auto kind=selectionKind_;
    const auto selected=kind==HullEntity::Point?selection_:entitySelection_;
    if(!session_.ready()||session_.busy()||selected.isEmpty())return;
    const auto model=session_.model();
    QDialog question(this);setStudioWindowIcon(&question,"DeleteSelected");question.setWindowTitle(uiText("删除所选对象"));
    question.setProperty("modeling_operation","delete");
    question.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&question);
    const bool auxiliary=kind==HullEntity::Curve||kind==HullEntity::Flowline;
    layout->addWidget(new StudioMessage(StudioMessageRole::Warning,auxiliary?uiText("删除所选曲线？船体控制网会保留。"):uiText("删除所选对象及依赖它们的边、面？")));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);
    buttons->button(QDialogButtonBox::Yes)->setText(uiText("确认"));
    buttons->button(QDialogButtonBox::Yes)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::No)->setText(uiText("取消"));
    buttons->button(QDialogButtonBox::No)->setDefault(true);
    connect(buttons,&QDialogButtonBox::accepted,&question,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&question,&QDialog::reject);layout->addWidget(buttons);
    if(runStudioDialog(question)!=QDialog::Accepted)return;
    if(!session_.ready()||session_.busy()||session_.model()!=model){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
    if(auxiliary)session_.editAuxiliary("delete",{{"kind",kind==HullEntity::Curve?"curve":"flow"},{"ids",ids(selected)}});
    else session_.editTopology("delete",{{"kind",kind==HullEntity::Point?"point":kind==HullEntity::Edge?"edge":"face"},{"ids",ids(selected)}});
}
void MainWindow::beginFlowline(){
    if(!session_.ready()||session_.busy())return;
    endPointPlacement();
    tabs_->setCurrentIndex(0);for(auto* view:views_){view->setFillVisible(false);view->setFlowPlacement(true);}
    showStatusMessage(uiText("在横剖、纵剖或俯视图连续点击流线起点；Esc 或右键结束。"),StudioMessageRole::Explanation);
}
void MainWindow::endFlowline(){
    for(auto* view:views_)if(view&&view->flowPlacement())view->setFlowPlacement(false);
    if(addFlow_){QSignalBlocker block(addFlow_);addFlow_->setChecked(false);}
}
void MainWindow::beginPointPlacement(){
    if(!session_.ready()||session_.busy())return;
    if(pointPopup_)pointPopup_->hide();
    inspector_->hide();
    setSelectionKind(HullEntity::Point);
    if(placePoint_){QSignalBlocker block(placePoint_);placePoint_->setChecked(true);}
    tabs_->setCurrentIndex(0);
    for(auto* view:views_){view->setFillVisible(false);view->setControlNetVisible(true);view->setPointPlacement(true);}
    showStatusMessage(uiText("横剖面按所选站面放点；按住 Shift 点击可吸附中线或本站参考线。Esc 或右键结束。"),StudioMessageRole::Explanation);
}
void MainWindow::endPointPlacement(){
    placementUndos_=std::max(0,placementUndos_-int(waitingPlacements_.size()));
    waitingPlacements_.clear();updatePlacementPreview();
    if(pointPopup_)pointPopup_->hide();
    for(auto* view:views_)if(view&&view->pointPlacement())view->setPointPlacement(false);
    if(placePoint_){QSignalBlocker block(placePoint_);placePoint_->setChecked(false);}
}
namespace {
struct MeshDetails {QString layers;int count=0;bool crease=false;};
MeshDetails meshDetails(const fs::ModelSnapshot& model,HullEntity kind,fs::EntityId id){
    MeshDetails result;QSet<fs::EntityId> layerIds;fs::EntityId a=0,b=0;
    if(kind==HullEntity::Edge)for(const auto& edge:model.edges)if(edge.id==id){a=edge.a;b=edge.b;result.crease=edge.crease;break;}
    for(const auto& face:model.faces){
        if(kind==HullEntity::Point){if(face.points.contains(id))layerIds.insert(face.layer);}
        else if(kind==HullEntity::Face){if(face.id==id){layerIds.insert(face.layer);result.count=face.points.size();break;}}
        else for(qsizetype i=0;i<face.points.size();++i){const auto x=face.points[i],y=face.points[(i+1)%face.points.size()];
            if((x==a&&y==b)||(x==b&&y==a)){layerIds.insert(face.layer);++result.count;break;}}
    }
    QStringList names;for(const auto& layer:model.layers)if(layerIds.contains(layer.id))names<<layerDisplayName(layer);
    result.layers=names.isEmpty()?uiText("无"):names.join("\n");return result;
}
}
void MainWindow::showEntityPopup(HullView* view,HullEntity kind,fs::EntityId id,QPoint at){
    const auto model=session_.model();if(!model||view->snapshot()!=model||!entitySelection_.contains(id))return;
    ensurePointPopup();const auto info=meshDetails(*model,kind,id);
    pointPopup_->presentEntity(kind==HullEntity::Edge,view->mapTo(pointPopup_->parentWidget(),at),info.layers,info.count,info.crease);
}
void MainWindow::ensurePointPopup(){
    if(!pointPopup_){
        pointPopup_=new PointCoordinatePopup(workspaceOverlayHost());
        pointPopup_->canUndoPlacement=[this]{return canUndoPlacement();};
        pointPopup_->undoPlacement=[this]{undoPlacement();};
        pointPopup_->onCancelPlacement=[this]{endPointPlacement();};
        pointPopup_->onProperties=[this](fs::EntityId id,const QString& name,int type){
            auto m=session_.model();if(!session_.ready()||session_.busy()||!m||m->documentId!=pointPopupDocument_||m->revision!=pointPopupRevision_||id!=selected_)return;
            const auto* p=m->point(id);if(!p)return;
            if((p->name==name&&p->vertexType==type)||session_.editModeling("point_properties",{{"ids",QJsonArray{double(id)}},{"name",name},{"vertex_type",type}}))pointPopup_->hide();
        };
        pointPopup_->onApply=[this](fs::EntityId pointId,fs::Vec3 position){
            const auto model=session_.model();
            if(!session_.ready()||session_.busy()||!model||model->documentId!=pointPopupDocument_||model->revision!=pointPopupRevision_)return;
            const auto* point=model->point(pointId);if(!point||point->locked||pointId!=selected_)return;
            const bool unchanged=point->position.x==position.x&&point->position.y==position.y&&point->position.z==position.z;
            if(selectedPointView_&&!selectedPointView_->pointPlacement()){
                if(!unchanged)selectedPointView_->onPointsMoved({{pointId,position}});
            }else if(unchanged||session_.movePoint(pointId,position))pointPopup_->hide();
        };
    }
}
void MainWindow::showPointPopup(HullView* view,fs::EntityId id){
    const auto model=session_.model();const auto* point=model?model->point(id):nullptr;if(!point||!view)return;
    ensurePointPopup();
    if(view->pointPlacement()||pointPopupDocument_!=model->documentId)pointPropertiesPage_=false;
    pointPopup_->setProperty("placement_undo_hidden",false);
    pointPopupDocument_=model->documentId;pointPopupRevision_=model->revision;
    const QPoint anchor=view->mapTo(pointPopup_->parentWidget(),view->projectControlPoint(id).toPoint());
    pointPopup_->present(id,view->displayedControlPoint(id).value_or(point->position),model->units,view->projection(),anchor,point->batchMovable && !point->locked?84:12,view->pointPlacement(),pointPropertiesPage_,point->name,point->vertexType,pointPropertiesPage_?meshDetails(*model,HullEntity::Point,id).layers:QString());
    pointPopup_->setEditingEnabled(!session_.busy(),!point->locked,point->batchMovable);
}
void MainWindow::followPointPopup(){
    if(!pointPopup_||!pointPopup_->isVisible()||!selectedPointView_||pointPopup_->pointId()!=selected_)return;
    auto* view=selectedPointView_.data();
    if(const auto position=view->displayedControlPoint(selected_))
        pointPopup_->follow(*position,view->mapTo(pointPopup_->parentWidget(),view->projectControlPoint(selected_).toPoint()));
}
