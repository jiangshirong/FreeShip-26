#include "main_window.h"
#include "section_dialog.h"
#include "layer_panel.h"
#include <QLineEdit>
#include "studio_ui.h"
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
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QJsonArray ids(const QSet<fs::EntityId>& values) {
    auto ordered=values.values();std::sort(ordered.begin(),ordered.end());
    QJsonArray result;for(auto id:ordered)result.append(double(id));return result;
}
fs::EntityId chooseLayer(const fs::ModelSnapshot& model,QWidget* parent,const QString& title,bool hidden) {
    QDialog dialog(parent);dialog.setWindowTitle(title);
    dialog.setAttribute(Qt::WA_DontShowOnScreen,hidden);
    auto* layout=new QVBoxLayout(&dialog);auto* field=new QComboBox;
    for(const auto& layer:model.layers)field->addItem(layer.name,QVariant::fromValue(layer.id));
    layout->addWidget(field);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    return dialog.exec()==QDialog::Accepted?field->currentData().toULongLong():0;
}
}
void MainWindow::buildStudioShell(QAction* fit) {
    auto action=[this](const QString& text,const QString& icon,const QString& operation={}) {
        auto* a=new QAction(studioIcon(icon),text,this);a->setObjectName("Studio"+icon);
        if(!operation.isEmpty())a->setProperty("operation",operation);
        addAction(a);return a;
    };
    auto group=[&](const QString& text,const QString& icon,QMenu*& menu) {
        auto* a=action(text,icon);menu=new QMenu(text,this);a->setMenu(menu);return a;
    };
    auto* root=new QMenu(this);
    for(auto* a:menuBar()->actions()){
        if(a->menu()){
            a->menu()->setParent(this,a->menu()->windowFlags());
            for(auto* child:a->menu()->actions())addAction(child);
        }
        root->addAction(a);
    }
    menuBar()->hide();auto* menu=action("菜单","Menu");menu->setMenu(root);
    open_->setIcon(studioIcon("Open"));save_->setIcon(studioIcon("Save"));
    undo_->setIcon(studioIcon("Undo"));redo_->setIcon(studioIcon("Redo"));
    png_->setIcon(studioIcon("ExportImage"));dxf_->setIcon(studioIcon("ExportFile"));
    QMenu *pointMenu,*edgeMenu,*faceMenu,*layerMenu,*visibilityMenu,*transformMenu;
    auto* edges=group("边","Edge",edgeMenu);auto* faces=group("面","Face",faceMenu);
    auto* points=group("点","Point",pointMenu);auto* layers=group("图层","Layers",layerMenu);
    auto* visibility=group("视图","Observe",visibilityMenu);
    auto* transform=group("变换","Transform",transformMenu);
    auto* choices=new QActionGroup(this);choices->setExclusive(true);
    auto selection=[&](QMenu* owner,const QString& text,const QString& icon,HullEntity kind) {
        const auto key=kind==HullEntity::Point?"point":kind==HullEntity::Edge?"edge":kind==HullEntity::Face?"face":kind==HullEntity::Curve?"curve":"flow";
        auto* a=action(text,icon,"select_"+QString(key));
        a->setCheckable(true);a->setChecked(kind==HullEntity::Point);choices->addAction(a);owner->addAction(a);
        connect(a,&QAction::triggered,this,[this,kind]{setSelectionKind(kind);});
    };
    selection(pointMenu,"选择控制点","Point",HullEntity::Point);
    selection(edgeMenu,"选择边","Edge",HullEntity::Edge);selection(faceMenu,"选择面","Face",HullEntity::Face);
    selection(edgeMenu,"选择控制曲线","NewCurve",HullEntity::Curve);
    addCurve_=action("由所选边生成控制曲线","NewCurve","create_control_curve");edgeMenu->addAction(addCurve_);
    connect(addCurve_,&QAction::triggered,this,[this]{session_.editAuxiliary("create_control",{{"ids",ids(entitySelection_)}});});
    placePoint_=action("放置控制点","Point","place_point");placePoint_->setCheckable(true);pointMenu->addAction(placePoint_);
    connect(placePoint_,&QAction::triggered,this,[this](bool enabled){if(enabled)beginPointPlacement();else endPointPlacement();});
    addPoint_=action("按坐标新增控制点…","Point","add_point");pointMenu->addAction(addPoint_);
    connect(addPoint_,&QAction::triggered,this,&MainWindow::createPoint);
    auto* details=action("所选对象详情","ObjectDetails");pointMenu->addAction(details);
    connect(details,&QAction::triggered,this,[this]{inspector_->show();inspector_->setCurrentIndex(0);});
    lock_=action("锁定所选点","Lock","set_locked");unlock_=action("解锁所选点","Unlock","set_unlocked");
    pointMenu->addAction(lock_);pointMenu->addAction(unlock_);
    connect(lock_,&QAction::triggered,this,[this]{session_.editTopology("set_locked",{{"ids",ids(selection_)},{"locked",true}});});
    connect(unlock_,&QAction::triggered,this,[this]{session_.editTopology("set_locked",{{"ids",ids(selection_)},{"locked",false}});});
    splitEdge_=action("拆分边","SplitEdge","split_edge");edgeMenu->addAction(splitEdge_);
    connect(splitEdge_,&QAction::triggered,this,[this]{if(entitySelection_.size()==1)session_.editTopology("split_edge",{{"edge_id",double(*entitySelection_.begin())}});});
    crease_=action("设置折痕","CreaseEdge","set_crease");smooth_=action("取消折痕","CreaseEdge","clear_crease");
    edgeMenu->addAction(crease_);edgeMenu->addAction(smooth_);
    connect(crease_,&QAction::triggered,this,[this]{session_.editTopology("set_crease",{{"ids",ids(entitySelection_)},{"crease",true}});});
    connect(smooth_,&QAction::triggered,this,[this]{session_.editTopology("set_crease",{{"ids",ids(entitySelection_)},{"crease",false}});});
    addFace_=action("由所选点创建面…","Face","add_face");faceMenu->addAction(addFace_);
    addFace_->setToolTip("按轮廓顺序选择至少三个控制点，再创建面。");
    connect(addFace_,&QAction::triggered,this,&MainWindow::createFace);
    assignLayer_=action("将所选面移至图层…","Layers","assign_layer");faceMenu->addAction(assignLayer_);
    connect(assignLayer_,&QAction::triggered,this,&MainWindow::assignFaceLayer);
    delete_=action("删除所选对象","DeleteSelected","delete");delete_->setShortcut(QKeySequence::Delete);
    for(auto* m:{pointMenu,edgeMenu,faceMenu}){m->addSeparator();m->addAction(delete_);}
    connect(delete_,&QAction::triggered,this,&MainWindow::deleteSelection);
    layerProperties_=action("图层属性","Layers","layer_properties");layerMenu->addAction(layerProperties_);
    connect(layerProperties_,&QAction::triggered,this,&MainWindow::openLayers);
    newLayer_=action("新建图层…","NewLayer","create_layer");layerMenu->addAction(newLayer_);
    deleteLayer_=action("删除所选空图层…","DeleteSelected","delete_layer");layerMenu->addAction(deleteLayer_);
    emptyLayers_=action("清理空图层…","DeleteSelected","delete_empty_layers");layerMenu->addAction(emptyLayers_);
    connect(newLayer_,&QAction::triggered,this,&MainWindow::createLayer);
    connect(deleteLayer_,&QAction::triggered,this,[this]{deleteLayer(false);});
    connect(emptyLayers_,&QAction::triggered,this,[this]{deleteLayer(true);});
    layerUp_=action("上移","MoveUp","layer_up");layerDown_=action("下移","MoveDown","layer_down");
    for(auto* a:{layerUp_,layerDown_}){layerMenu->addAction(a);connect(a,&QAction::triggered,this,[this,a]{session_.manageLayers("move",{{"layer_id",double(layers_->selectedLayerId())},{"direction",a==layerUp_?-1:1}});});}
    layers_->setManagementActions({newLayer_,emptyLayers_,layerUp_,layerDown_});
    auto* hideInspector=action("收起属性面板","ObjectDetails");visibilityMenu->addAction(hideInspector);
    connect(hideInspector,&QAction::triggered,this,[this]{inspector_->hide();});
    hullFill_=action("船体填色","FillColor");hullFill_->setCheckable(true);visibilityMenu->addAction(hullFill_);
    connect(hullFill_,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setFillVisible(value);});
    visibilityMenu->addAction(fit);
    auto* sides=action("双舷显示","BothSides","both_sides");sides->setCheckable(true);visibilityMenu->addAction(sides);
    auto* curvature=action("曲率梳","Curvature","show_curvature");curvature->setCheckable(true);visibilityMenu->addAction(curvature);
    auto* increase=action("增大曲率梳","ZoomIn","curvature_increase");visibilityMenu->addAction(increase);
    auto* decrease=action("减小曲率梳","ZoomOut","curvature_decrease");visibilityMenu->addAction(decrease);
    displayActions_={sides,curvature,increase,decrease};
    connect(sides,&QAction::triggered,this,[this](bool enabled){session_.editDisplay({{"both_sides",enabled}});});
    connect(curvature,&QAction::triggered,this,[this](bool enabled){session_.editDisplay({{"show_curvature",enabled}});});
    connect(increase,&QAction::triggered,this,[this]{if(auto m=session_.model())session_.editDisplay({{"curvature_scale",m->curvatureScale*1.1}});});
    connect(decrease,&QAction::triggered,this,[this]{if(auto m=session_.model())session_.editDisplay({{"curvature_scale",m->curvatureScale/1.1}});});
    auto* grid=action("网格","Grid","overlay_grid");grid->setCheckable(true);grid->setChecked(true);visibilityMenu->addAction(grid);
    auto* hydro=action("静水力数据","Hydrostatics","overlay_hydro");hydro->setCheckable(true);hydro->setChecked(true);visibilityMenu->addAction(hydro);
    auto* curveVisible=action("控制曲线","NewCurve","show_control_curves");curveVisible->setCheckable(true);visibilityMenu->addAction(curveVisible);
    auto* flowMenu=visibilityMenu->addMenu("流线");
    auto* flowVisible=action("显示流线","Flowlines","show_flowlines");flowVisible->setCheckable(true);flowMenu->addAction(flowVisible);
    auxiliaryVisibility_={curveVisible,flowVisible};
    connect(curveVisible,&QAction::triggered,this,[this](bool visible){session_.editDisplay({{"show_control_curves",visible}});});
    connect(flowVisible,&QAction::triggered,this,[this](bool visible){session_.editDisplay({{"show_flowlines",visible}});});
    addFlow_=action("添加流线","Flowlines","create_flowline");addFlow_->setCheckable(true);flowMenu->addAction(addFlow_);
    connect(addFlow_,&QAction::triggered,this,[this](bool checked){if(checked)beginFlowline();else endFlowline();});
    selection(flowMenu,"选择流线","Flowlines",HullEntity::Flowline);flowMenu->addAction(delete_);
    connect(grid,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setGridVisible(value);});
    connect(hydro,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setHydroVisible(value);});
    connect(visibilityMenu,&QMenu::aboutToShow,this,[this,grid,hydro]{
        // Per-view context menus can differ; the global switch reflects the
        // focused view, or the first view when focus is outside the canvas.
        auto* current=views_.front();for(auto* view:views_)if(view->hasFocus())current=view;
        QSignalBlocker a(grid),b(hydro);grid->setChecked(current->gridVisible());hydro->setChecked(current->hydroVisible());
    });
    auto* coordinates=visibilityMenu->addMenu("控制点坐标");auto* axisGroup=new QActionGroup(this);
    const QStringList coordinateNames{"不显示坐标","X 坐标","Y 坐标","Z 坐标"};
    for(int i=0;i<4;++i){auto* a=action(coordinateNames[i],"Point","coordinate_axis");a->setProperty("coordinate_axis",i);a->setCheckable(true);a->setChecked(i==0);axisGroup->addAction(a);coordinates->addAction(a);
        connect(a,&QAction::triggered,this,[this,i]{for(auto* view:views_)view->setCoordinateAxis(i);});}
    connect(coordinates,&QMenu::aboutToShow,this,[this,axisGroup]{auto* current=views_.front();for(auto* view:views_)if(view->hasFocus())current=view;
        for(auto* a:axisGroup->actions()){QSignalBlocker block(a);a->setChecked(a->property("coordinate_axis").toInt()==current->coordinateAxis());}});
    auto* fourViews=action("四视图","ViewPerspective");visibilityMenu->addAction(fourViews);
    connect(fourViews,&QAction::triggered,this,[this]{tabs_->setCurrentIndex(0);for(auto* a:viewActions_)a->setChecked(true);layoutViews();});
    const QStringList operations{"translate","scale","rotate","extrude","mirror"};
    const QStringList transformNames{"移动所选对象…","缩放所选对象…","旋转所选对象…","挤出所选边…","镜像所选面…"};
    const QStringList transformIcons{"Move","Scale","RotateCW","ExtrudeEdge","Mirror"};
    for(int i=0;i<5;++i){auto* a=action(transformNames[i],transformIcons[i],operations[i]);advancedActions_[i]=a;
        (i==3?edgeMenu:i==4?faceMenu:transformMenu)->addAction(a);connect(a,&QAction::triggered,this,[this,operation=operations[i]]{advancedDialog(operation);});}
    buildModelingActions(pointMenu,edgeMenu,faceMenu,layerMenu,visibilityMenu,transformMenu);
    auto* checks=action("模型检查","ModelCheck","model_check");modelingActions_.append(checks);
    connect(checks,&QAction::triggered,this,&MainWindow::modelCheckDialog);
    QMenu* calculationsMenu=nullptr;auto* calculations=group("计算","Calculate",calculationsMenu);
    platesAction_=action("板材展开…","Develop","develop_plates");calculationsMenu->addAction(platesAction_);
    connect(platesAction_,&QAction::triggered,this,&MainWindow::openPlates);
    const QStringList hydroModes{"single","table","cross"},hydroNames{"设计静水力…","静水力吃水表…","稳性曲线与平衡…"};
    for(int i=0;i<3;++i){auto* a=action(hydroNames[i],"Hydrostatics","hydro_"+hydroModes[i]);hydroActions_.append(a);calculationsMenu->addAction(a);
        connect(a,&QAction::triggered,this,[this,mode=hydroModes[i]]{openHydro(mode);});}
    auto* resistanceMenu=calculationsMenu->addMenu("阻力计算");auto* propellerMenu=calculationsMenu->addMenu("螺旋桨与推进");
    const QVector<QPair<QString,QString>> methods{
        {"delft","Delft 帆船阻力"},{"holtrop","Holtrop 海船阻力"},{"hollenbach","Hollenbach 阻力"},
        {"oortmerssen","van Oortmerssen 阻力"},{"fung","Fung–Leibman 阻力"},{"ost","OST 阻力"},
        {"rbhs","圆舭高速艇阻力"},{"mh","多体船阻力"},{"kaper","KAPER 独木舟及皮艇阻力"},{"planing","滑行艇阻力"},
        {"propeller1","螺旋桨优化与主机选型"},{"propeller2","主机与螺旋桨匹配"},{"propeller3","船舶推进特性图（Passport）"},{"propeller4","螺旋桨初步设计"},{"propeller5","螺旋桨系列计算"}};
    for(const auto& method:methods){auto* a=action(method.second+"…","Resistance","performance_"+method.first);performanceActions_.append(a);
        (method.first.startsWith("propeller")?propellerMenu:resistanceMenu)->addAction(a);connect(a,&QAction::triggered,this,[this,key=method.first]{openPerformance(key);});}
    auto* hydrodynamicsMenu=calculationsMenu->addMenu("水动力专用工具");
    for(const auto &method:QVector<QPair<QString,QString>>{{"wind","空气动力特性"},{"maneuver","操纵性预估"},{"acceleration","加速与制动"},{"addmass","附加质量计算"}}){
        auto* a=action(method.second+"…","Calculate","performance_"+method.first);performanceActions_.append(a);hydrodynamicsMenu->addAction(a);
        connect(a,&QAction::triggered,this,[this,key=method.first]{openPerformance(key);});}
    auto* sections=action("剖面线","Sections","sections");sectionsAction_=sections;
    connect(sections,&QAction::triggered,this,&MainWindow::editSections);
    const std::array<QString,4> sectionNames{"横剖线","纵剖线","水线","斜剖线"};
    visibilityMenu->addSeparator();
    for(int i=0;i<4;++i){auto* a=action(sectionNames[i],"Sections","section_visibility");a->setProperty("section_kind",i+1);a->setCheckable(true);a->setChecked(true);visibilityMenu->addAction(a);
        connect(a,&QAction::toggled,this,[this,i](bool value){for(auto* view:views_)view->setSectionVisible(i+1,value);});}
    project_->setText("工程设置");project_->setIcon(studioIcon("ProjectSettings"));
    auto* lines=action("型线图","LinesPlan");connect(lines,&QAction::triggered,this,[this]{tabs_->setCurrentIndex(1);});
    auto* net=action("控制网格","ControlNet");net->setCheckable(true);net->setChecked(true);
    connect(net,&QAction::toggled,this,[this](bool value){for(auto* view:views_)view->setControlNetVisible(value);});
    const std::array<QString,4> names{"纵剖面","横剖面","俯视图","透视图"};
    const std::array<QString,4> icons{"ViewProfile","ViewBody","ViewPlan","ViewPerspective"};
    for(size_t i=0;i<4;++i){
        auto* a=action(names[i],icons[i]);viewActions_[i]=a;a->setCheckable(true);a->setChecked(true);
        connect(a,&QAction::triggered,this,[this,a]{
            if(tabs_->currentIndex()==1){a->setChecked(true);tabs_->setCurrentIndex(0);}
            if(std::none_of(viewActions_.begin(),viewActions_.end(),[](auto* item){return item->isChecked();}))a->setChecked(true);
            layoutViews();
        });
    }
    auto* about=action("关于","Info");
    connect(about,&QAction::triggered,this,[this]{QMessageBox::about(this,"FreeShip_26","FreeShip_26\n\n基于 FREE!ship / FreeShip Plus。\n原作者、GPL 许可及资源来源说明随软件保留。");});
    auto* bar=new StudioBar(this);studioBar_=bar;
    bar->addGroup({menu,edges,faces,points,layers,visibility,checks,transform,calculations},QColor(237,244,248));
    bar->addGroup({project_,sections,lines},QColor(248,243,232));
    bar->addGroup({net,viewActions_[1],viewActions_[0],viewActions_[2],viewActions_[3]},QColor(231,240,246),true);
    bar->setAbout(about);setMenuWidget(bar);
}
void MainWindow::layoutViews() {
    for(auto* frame:viewFrames_)viewGrid_->removeWidget(frame);
    int count=0;for(int index:{1,0,2,3})if(viewActions_[index]->isChecked())++count;
    int at=0;for(int index:{1,0,2,3}) {
        bool visible=viewActions_[index]->isChecked();viewFrames_[index]->setVisible(visible);
        if(visible){viewGrid_->addWidget(viewFrames_[index],count==1?0:at/2,count==1?0:at%2);++at;}
    }
    for(int i=0;i<2;++i){viewGrid_->setRowStretch(i,count>2||i==0?1:0);viewGrid_->setColumnStretch(i,count>1||i==0?1:0);}
}
void MainWindow::editSections(){
    if(!session_.ready()||session_.busy())return;
    if(!sections_){
        sections_=new SectionDialog(this);sections_->setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        sections_->onEdit=[this](const QString& operation,int kind,QJsonObject fields){session_.editSections(operation,kind,std::move(fields));};
        sections_->onSelection=[this](int kind,QSet<int> ids){for(auto* view:views_)view->highlightSections(kind,ids);};
        connect(sections_,&QDialog::finished,this,[this]{for(auto* view:views_)view->highlightSections(0,{});});
    }
    sections_->setSnapshot(session_.model());sections_->setEditingEnabled(true);sections_->show();sections_->raise();
}
void MainWindow::openLayers(){
    if(!session_.ready()||session_.busy())return;
    layerDialog_->setAttribute(Qt::WA_DontShowOnScreen,workflow_);layerDialog_->show();layerDialog_->raise();layerDialog_->activateWindow();
}
void MainWindow::createLayer(){
    if(!session_.ready()||session_.busy())return;
    QDialog dialog(this);dialog.setWindowTitle("新建图层");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;auto* name=new QLineEdit("Layer "+QString::number(session_.model()->layers.size()+1));
    name->selectAll();form->addRow("名称",name);layout->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText("确定");buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){
        openLayers();
        for(const auto& layer:session_.model()->layers)pendingNewLayerAfter_=std::max(pendingNewLayerAfter_,layer.id);
        if(!session_.manageLayers("create",{{"changes",QJsonObject{{"name",name->text()}}}}))pendingNewLayerAfter_=0;
    }
}
void MainWindow::deleteLayer(bool allEmpty){
    if(!session_.ready()||session_.busy())return;
    auto id=layers_->selectedLayerId();if(!allEmpty&&!id)return;
    if(!allEmpty&&std::any_of(session_.model()->faces.begin(),session_.model()->faces.end(),[id](const auto& face){return face.layer==id;})){
        showError("这个图层中还有面，请先将面移至其他图层。");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("删除空图层");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);layout->addWidget(new QLabel(allEmpty?"清理全部空图层？至少保留一个图层，可以撤销。":"删除所选空图层？可以撤销。"));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);buttons->button(QDialogButtonBox::No)->setDefault(true);buttons->button(QDialogButtonBox::Yes)->setAutoDefault(false);layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Yes)->setText("删除");buttons->button(QDialogButtonBox::No)->setText("取消");
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted)session_.manageLayers(allEmpty?"delete_empty":"delete",{{"layer_id",double(id)}});
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
    selectionKind_=kind;views_.front()->setSelectedEntities(std::move(selected));entitySelection_=views_.front()->selectedEntities();
    for(auto* view:views_)view->setSelectedEntities(entitySelection_);
    updateState();
}
void MainWindow::createPoint() {
    if(!session_.ready()||session_.busy())return;
    QDialog dialog(this);dialog.setWindowTitle("新增控制点");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);auto* layout=new QVBoxLayout(&dialog);
    auto* form=new QFormLayout;std::array<QDoubleSpinBox*,3> values{};
    for(int i=0;i<3;++i){auto* spin=new QDoubleSpinBox;values[i]=spin;spin->setRange(-1e9,1e9);spin->setDecimals(12);form->addRow(QString(QChar('X'+i))+" ("+(session_.model()->units=="metric"?"m":"ft")+")",spin);}
    layout->addLayout(form);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    if(dialog.exec()==QDialog::Accepted)session_.editTopology("add_point",{{"position",QJsonArray{values[0]->value(),values[1]->value(),values[2]->value()}}});
}
void MainWindow::createFace() {
    if(!session_.ready()||session_.busy()||selectionOrder_.size()<3)return;
    const auto layer=chooseLayer(*session_.model(),this,"新建面的图层",workflow_);if(!layer)return;
    QJsonArray ordered;for(auto id:selectionOrder_)ordered.append(double(id));
    session_.editTopology("add_face",{{"ids",ordered},{"layer_id",double(layer)}});
}
void MainWindow::assignFaceLayer() {
    if(!session_.ready()||session_.busy()||entitySelection_.isEmpty())return;
    const auto layer=chooseLayer(*session_.model(),this,"所选面的图层",workflow_);
    if(layer)session_.editTopology("assign_layer",{{"ids",ids(entitySelection_)},{"layer_id",double(layer)}});
}
void MainWindow::deleteSelection() {
    const auto selected=selectionKind_==HullEntity::Point?selection_:entitySelection_;
    if(!session_.ready()||session_.busy()||selected.isEmpty())return;
    QDialog question(this);question.setWindowTitle("删除所选对象");
    question.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&question);
    const bool auxiliary=selectionKind_==HullEntity::Curve||selectionKind_==HullEntity::Flowline;
    layout->addWidget(new QLabel(auxiliary?"删除所选曲线？船体控制网会保留，可以撤销。":"删除所选对象及依赖它们的边、面？可以撤销。"));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);
    buttons->button(QDialogButtonBox::Yes)->setText("删除");
    buttons->button(QDialogButtonBox::Yes)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::No)->setText("取消");
    buttons->button(QDialogButtonBox::No)->setDefault(true);
    connect(buttons,&QDialogButtonBox::accepted,&question,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&question,&QDialog::reject);layout->addWidget(buttons);
    if(question.exec()!=QDialog::Accepted)return;
    if(auxiliary)session_.editAuxiliary("delete",{{"kind",selectionKind_==HullEntity::Curve?"curve":"flow"},{"ids",ids(selected)}});
    else session_.editTopology("delete",{{"kind",selectionKind_==HullEntity::Point?"point":selectionKind_==HullEntity::Edge?"edge":"face"},{"ids",ids(selected)}});
}
void MainWindow::beginFlowline(){
    if(!session_.ready()||session_.busy())return;
    endPointPlacement();
    if(hullFill_)hullFill_->setChecked(false);
    tabs_->setCurrentIndex(0);for(auto* view:views_){view->setFillVisible(false);view->setFlowPlacement(true);}
    statusBar()->showMessage("在横剖、纵剖或俯视图连续点击流线起点；Esc 或右键结束。");
}
void MainWindow::endFlowline(){
    for(auto* view:views_)if(view&&view->flowPlacement())view->setFlowPlacement(false);
    if(addFlow_){QSignalBlocker block(addFlow_);addFlow_->setChecked(false);}
}
void MainWindow::beginPointPlacement(){
    if(!session_.ready()||session_.busy())return;
    setSelectionKind(HullEntity::Point);
    if(placePoint_){QSignalBlocker block(placePoint_);placePoint_->setChecked(true);}
    if(hullFill_)hullFill_->setChecked(false);tabs_->setCurrentIndex(0);
    for(auto* view:views_){view->setFillVisible(false);view->setPointPlacement(true);}
    statusBar()->showMessage("在正交视图点击放置控制点；纵剖面 Y=0，俯视图 Z=0，横剖面 X=模型最大 X。Esc 或右键取消。");
}
void MainWindow::endPointPlacement(){
    for(auto* view:views_)if(view&&view->pointPlacement())view->setPointPlacement(false);
    if(placePoint_){QSignalBlocker block(placePoint_);placePoint_->setChecked(false);}
}
