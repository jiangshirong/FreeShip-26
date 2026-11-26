#include "app/ui_language.h"
#include "main_window.h"
#include "layer_panel.h"
#include "studio_ui.h"
#include "core/model_checks.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include "preferences_store.h"
#include <QFormLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
class ModelingNumber final : public QDoubleSpinBox {
protected:
    QString textFromValue(double value)const override {
        QString text=QDoubleSpinBox::textFromValue(value);
        if(text.contains(locale().decimalPoint())){
            while(text.endsWith('0'))text.chop(1);
            if(text.endsWith(locale().decimalPoint()))text.chop(locale().decimalPoint().size());
        }
        return text;
    }
};
class CheckMenuTips final : public QObject {
public:
    CheckMenuTips(QMenu* menu,std::function<void()> refresh)
        : QObject(menu), menu_(menu), refresh_(std::move(refresh)) {
        menu_->installEventFilter(this);
        connect(menu_,&QMenu::aboutToShow,this,[this]{refresh_();});
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(event->type()!=QEvent::ToolTip)return QObject::eventFilter(watched,event);
        const auto* help=static_cast<QHelpEvent*>(event);
        auto* action=menu_->actionAt(help->pos());
        if(!action||!action->property("model_check_targets").toBool())return QObject::eventFilter(watched,event);
        refresh_();
        if(action->isEnabled())return QObject::eventFilter(watched,event);
        QToolTip::showText(help->globalPos(),action->toolTip(),menu_,menu_->actionGeometry(action));
        return true;
    }
private:
    QMenu* menu_;
    std::function<void()> refresh_;
};
QJsonArray array(const QVector<fs::EntityId>& ids){QJsonArray a;for(auto id:ids)a.append(double(id));return a;}
QJsonArray array(const QSet<fs::EntityId>& ids){auto list=ids.values();std::sort(list.begin(),list.end());return array(list);}
void ensureSelectionView(const std::array<HullView*,4>& views,HullEntity kind){
    const auto visible=[](const HullView* view){return view->isVisibleTo(view->window());};
    if(std::any_of(views.begin(),views.end(),[&](const HullView* view){return visible(view)&&view->selectionKind()==kind;}))return;
    const auto view=std::find_if(views.begin(),views.end(),visible);
    if(view!=views.end()){
        (*view)->restoreDisplayMode("wireframe");
        (*view)->setSelectionKind(kind);
    }
}
QSet<fs::EntityId> orientationConflicts(const fs::ModelSnapshot& model){
    for(const auto& issue:fs::inspectModel(model))
        if(issue.code=="winding")return {issue.ids.begin(),issue.ids.end()};
    return {};
}
QSet<fs::EntityId> checkSelectionTargets(const fs::ModelSnapshot& model,const QString& operation,
        const QSet<fs::EntityId>& hidden,const QSet<fs::EntityId>& visibilityChanges){
    QSet<fs::EntityId> ids;
    if(operation=="select_points_without_faces"){
        ids.reserve(model.points.size());
        for(const auto& point:model.points)ids.insert(point.id);
        for(const auto& face:model.faces)for(auto point:face.points)ids.remove(point);
        return ids;
    }
    QSet<fs::EntityId> visible;
    for(const auto& layer:model.layers)
        if(layer.visible!=visibilityChanges.contains(layer.id)&&!hidden.contains(layer.id))visible.insert(layer.id);
    if(operation=="select_leaks"){
        QSet<fs::EntityId> leaks;
        for(const auto& point:model.points)if(point.leak)leaks.insert(point.id);
        if(leaks.isEmpty())return ids;
        for(const auto& face:model.faces)if(visible.contains(face.layer))
            for(auto point:face.points)if(leaks.contains(point))ids.insert(point);
        return ids;
    }
    constexpr double centerlineTolerance=1e-7;
    QSet<fs::EntityId> negative,positive;
    for(const auto& point:model.points){
        if(point.position.y < -centerlineTolerance)negative.insert(point.id);
        if(point.position.y > centerlineTolerance)positive.insert(point.id);
    }
    if(negative.isEmpty())return ids;
    for(const auto& face:model.faces){
        if(!visible.contains(face.layer))continue;
        const auto& points=face.points;
        if(std::any_of(points.cbegin(),points.cend(),[&](auto id){return negative.contains(id);}) &&
           std::none_of(points.cbegin(),points.cend(),[&](auto id){return positive.contains(id);}))ids.insert(face.id);
    }
    return ids;
}
QSet<fs::EntityId> hiddenLayers(const fs::ModelSnapshot& model){
    QSet<fs::EntityId> result;
    for(const auto& layer:model.layers)if(!layer.visible)result.insert(layer.id);
    return result;
}
QLabel* orientationLegend(const QString& color,const QString& name,QWidget* parent){
    auto* label=new QLabel(parent);
    const auto swatch=color=="#ffffff"?QStringLiteral("<span style='color:#314b60;background-color:#ffffff'>□</span>"):
        QStringLiteral("<span style='color:%1'>■</span>").arg(color);
    label->setText(swatch+" "+name.toHtmlEscaped());
    return label;
}
void buttons(QDialog& dialog,QVBoxLayout* layout){
    auto* b=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    b->button(QDialogButtonBox::Ok)->setText(uiText("确定"));b->button(QDialogButtonBox::Cancel)->setText(uiText("取消"));
    QObject::connect(b,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(b,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(b);
}
}

void MainWindow::buildModelingActions(QMenu* point,QMenu* edge,QMenu* face,QMenu* view,QList<QAction*>& transforms,QMenu* checks){
    auto add=[this](QMenu* menu,const QString& title,const QString& icon,const QString& op,const QString& kind={},int count=0){
        auto* a=new QAction(studioIcon(icon),title,this);a->setProperty("operation",op);a->setProperty("selection_kind",kind);a->setProperty("minimum_selection",count);
        const QStringList direct{"collapse_points","collapse_edges","connect_points","split_edges","flip_faces","assemble_faces","auto_group_layers","clear_constraints","unlock_all","clear_selection","select_leaks","select_points_without_faces","select_negative_faces"};
        a->setProperty("studio_opens_dialog",!direct.contains(op));
        if(menu)menu->addAction(a);
        addAction(a);modelingActions_.append(a);connect(a,&QAction::triggered,this,[this,op]{modelingAction(op);});return a;
    };
    auto* fresh=add(nullptr,uiText("新建船型…"),"NewFile","new");fresh->setShortcut(QKeySequence::New);
    fresh->setProperty("shortcut_target","当前船模");
    primaryMenu_->insertAction(open_,fresh);
    point->addSeparator();
    add(point,uiText("折叠所选点"),"Collapse","collapse_points","point",1);
    auto* constraints=point->addMenu(uiText("对齐与约束"));
    constraints->setIcon(studioIcon("AlignPoints"));
    add(constraints,uiText("投影到首尾两点的直线"),"AlignPoints","align","point",3);
    add(constraints,uiText("建立直线约束"),"AlignPoints","linear_constraint","point",3);
    add(constraints,uiText("锚定到第一点…"),"Lock","anchor","point",2);
    add(constraints,uiText("与第一点重合"),"AlignPoints","coincide","point",2);
    add(constraints,uiText("解除所选点的约束"),"Unlock","clear_constraints","point",1);
    add(point,uiText("解锁全部点"),"Unlock","unlock_all");
    add(point,uiText("建立点组…"),"Point","create_group","point",2);add(point,uiText("管理点组…"),"Point","point_groups");
    add(point,uiText("用平面插入控制点…"),"SplitSection","insert_plane");
    add(point,uiText("插入图层交点…"),"Sections","intersect_layers");
    const std::array entries{
        std::pair{add(checks,uiText("选择漏点"),"Point","select_leaks"),"没有可选择的漏点。"},
        std::pair{add(checks,uiText("选择无面点"),"Point","select_points_without_faces"),"没有可选择的无面点。"},
        std::pair{add(checks,uiText("选择负舷的面"),"Face","select_negative_faces"),"没有可选择的负舷面。"},
        std::pair{add(checks,uiText("修复相邻面朝向…"),"InvertFace","repair_normals"),"未发现相邻面朝向冲突。"}
    };
    for(const auto& [action,reason]:entries){action->setProperty("model_check_targets",true);action->setEnabled(false);}
    new CheckMenuTips(checks,[this,entries,checked=std::weak_ptr<const fs::ModelSnapshot>{},available=std::array<bool,4>{},
                             hidden=QSet<fs::EntityId>{},visibility=QSet<fs::EntityId>{}]() mutable {
        const auto model=session_.model();
        if(session_.busy()||!session_.ready()){
            const QString reason=session_.busy()?uiText("请等待模型更新完成。"):
                model?uiText("当前船模暂不可用。"):uiText("请先打开船模。");
            for(const auto& [action,unused]:entries){action->setEnabled(false);action->setToolTip(reason);}
            return;
        }
        const auto projected=layers_->projectedVisibilityChanges();
        const bool changed=checked.lock()!=model;
        if(changed||hidden!=hiddenControlLayers_||visibility!=projected){
            checked=model;
            hidden=hiddenControlLayers_;
            visibility=projected;
            for(size_t i=0;i+1<entries.size();++i)
                available[i]=!checkSelectionTargets(*model,entries[i].first->property("operation").toString(),hidden,visibility).isEmpty();
            if(changed)available.back()=!orientationConflicts(*model).isEmpty();
        }
        for(size_t i=0;i<entries.size();++i){
            const auto& [action,reason]=entries[i];
            action->setEnabled(available[i]);
            action->setToolTip(available[i]?QString{}:uiText(reason));
        }
    });
    edge->addSeparator();add(edge,uiText("连接所选控制点"),"Edge","connect_points","point",2);
    splitEdge_=add(edge,uiText("拆分所选边"),"SplitEdge","split_edges","edge",1);
    add(edge,uiText("合并相邻面"),"Collapse","collapse_edges","edge",1);
    add(edge,uiText("控制曲线名称…"),"NewCurve","rename_control","curve",1);
    face->addSeparator();add(face,uiText("翻转所选面的朝向"),"InvertFace","flip_faces","face",1);
    add(face,uiText("添加网格面板…"),"Grid","grid_panel");add(face,uiText("添加圆柱…"),"Face","cylinder");
    toolsMenu_->setProperty("studio_menu_has_help",true);
    auto* autoGroup=add(toolsMenu_,uiText("按折痕自动分组"),"LayerAutoGroup","auto_group_layers","face",1);
    bindUiText(autoGroup,"text","按折痕自动分组");
    bindUiText(autoGroup,"studio_menu_help","沿非折痕边把相连的面归入一个图层");
    auto* assemble=add(toolsMenu_,uiText("按曲面片分图层"),"Face","assemble_faces","face",1);
    bindUiText(assemble,"text","按曲面片分图层");
    bindUiText(assemble,"studio_menu_help","把能排成行列的相邻四边面归入一个图层");
    transforms.append(add(nullptr,uiText("绕船中旋转…"),"RotateCW","rotate_midship","transform",1));
    transforms.append(add(nullptr,uiText("Lackenby 船型变换…"),"Transform","lackenby"));
    add(view,uiText("导入参考曲线…"),"OpenFile","add_markers");
    add(view,uiText("删除全部参考曲线…"),"DeleteSelected","delete_markers");
    session_.onAnalysis=[this](const QJsonObject& info){modelingDialog("lackenby",info);
        if(modelingWorkflow_&&!session_.busy())QTimer::singleShot(0,this,[this]{modelingStep("analysis_closed");});};
}

void MainWindow::updateModelingActions(){
    const bool ready=session_.ready(),idle=!session_.busy();const auto selectedKind=selectionKind_==HullEntity::Point?"point":selectionKind_==HullEntity::Edge?"edge":selectionKind_==HullEntity::Face?"face":selectionKind_==HullEntity::Curve?"curve":"flow";
    for(auto* a:modelingActions_){const auto op=a->property("operation").toString(),kind=a->property("selection_kind").toString();const int minimum=a->property("minimum_selection").toInt();
        int count=kind=="transform"?transformPoints().size():kind=="point"?selection_.size():entitySelection_.size();
        bool enabled=idle&&(op=="new"||ready)&&(kind.isEmpty()||kind=="transform"||kind==selectedKind)&&count>=minimum;
        if(a->property("model_check_targets").toBool()){if(!enabled)a->setEnabled(false);continue;}
        if(op=="rename_control")enabled=enabled&&count==1;
        if(ready&&op=="point_groups")enabled=enabled&&!session_.model()->pointGroups.isEmpty();
        if(ready&&op=="delete_markers")enabled=enabled&&!session_.model()->markers.isEmpty();
        if(ready){
            const auto& model=*session_.model();
            if(op=="unlock_all")enabled=enabled&&std::any_of(model.points.cbegin(),model.points.cend(),[](const auto& point){return point.locked;});
            if(op=="clear_constraints")enabled=enabled&&std::any_of(model.points.cbegin(),model.points.cend(),[this](const auto& point){return selection_.contains(point.id)&&(point.anchor||point.lineA||point.lineB);});
            if(op=="select_all"||op=="invert_selection")enabled=enabled&&!model.points.isEmpty();
        }
        a->setEnabled(enabled);
    }
}

void MainWindow::modelingAction(const QString& op){
    if(session_.busy()||(op!="new"&&!session_.ready()))return;endFlowline();const auto m=session_.model();
    if(op=="new"){modelingDialog("new");return;}
    if(op=="point_groups"){pointGroupsDialog();return;}
    if(op=="repair_normals"){faceOrientationDialog();return;}
    if(op=="lackenby"){session_.inspectModeling();return;}
    if(op=="rotate_midship"){advancedDialog(op);return;}
    if(op=="clear_selection"){selectEntities(selectionKind_,{});selectPoints({});return;}
    if(op=="select_leaks"||op=="select_points_without_faces"||op=="select_negative_faces"){
        auto ids=checkSelectionTargets(*m,op,hiddenControlLayers_,layers_->projectedVisibilityChanges());
        if(ids.isEmpty())return;
        const auto kind=op=="select_negative_faces"?HullEntity::Face:HullEntity::Point;
        setSelectionKind(kind);
        ensureSelectionView(views_,kind);
        if(kind==HullEntity::Point){
            if(views_.front()->onShowControlNet)views_.front()->onShowControlNet();
            pointSelectionHasBatchOrder_=true;
            selectPoints(std::move(ids));
        }else selectEntities(kind,std::move(ids));
        return;
    }
    if(op=="select_all"||op=="invert_selection"){
        if(selectionKind_!=HullEntity::Point)setSelectionKind(HullEntity::Point);
        QSet<fs::EntityId> ids;
        for(const auto& p:m->points)ids.insert(p.id);
        if(op=="invert_selection")ids.subtract(selection_);
        pointSelectionHasBatchOrder_=true;
        selectPoints(ids);return;
    }
    if(op=="add_markers"){
        QFileDialog dialog(this,uiText("导入参考曲线"),preferenceFileLocation("import"));dialog.setProperty("modeling_operation",op);dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilter(uiText("参考曲线文本 (*.txt);;所有文件 (*)"));dialog.setLabelText(QFileDialog::Accept,uiText("打开"));dialog.setLabelText(QFileDialog::Reject,uiText("取消"));
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(runStudioDialog(dialog)!=QDialog::Accepted||dialog.selectedFiles().size()!=1)return;
        if(!session_.ready()||session_.busy()||session_.model()->documentId!=m->documentId||session_.model()->units!=m->units){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
        const auto filename=dialog.selectedFiles().first();
        QFile file(filename);if(file.size()>8*1024*1024||!file.open(QIODevice::ReadOnly)){showError(uiText("无法读取参考曲线文件，或文件超过 8 MB。"));return;}
        // Plus markers have a heading and XYZ paths separated by blank lines.
        // Coordinates keep the current model units.
        try{
            session_.editModeling(op,{{"paths",fs::parseMarkerText(file.readAll(),m->units)}});
        }catch(const fs::ReferenceCurveCoordinateError& error){
            showError(uiText(QString::fromUtf8(error.what())).arg(error.lineNumber));
        }catch(const std::exception& error){
            showError(QString::fromUtf8(error.what()));
        }
        return;
    }
    if(op=="auto_group_layers"||op=="assemble_faces"){
        if(selectionKind_==HullEntity::Face&&!entitySelection_.isEmpty())
            session_.editModeling(op,{{"ids",array(entitySelection_)}});
        return;
    }
    const QStringList immediate{"collapse_points","collapse_edges","connect_points","split_edges","flip_faces","clear_constraints","unlock_all"};
    if(immediate.contains(op)){
        QJsonObject fields;if(op!="unlock_all")fields["ids"]=op=="connect_points"?array(selectionOrder_):selectionKind_==HullEntity::Point?array(selection_):array(entitySelection_);
        session_.editModeling(op,fields);return;
    }
    modelingDialog(op);
}

void MainWindow::modelingDialog(const QString& op,const QJsonObject& initial){
    if(session_.busy()||(op!="new"&&!session_.ready()))return;const auto m=session_.model();
    QDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);dialog.setProperty("modeling_operation",op);
    setStudioWindowIcon(&dialog,op=="new"?"NewFile":"Point");
    QString title;for(auto* a:modelingActions_)if(a->property("operation")==op){title=a->text();if(!a->icon().isNull()){dialog.setWindowIcon(a->icon());dialog.setProperty("studio_window_icon","action:"+op);}break;}
    dialog.setWindowTitle(title.isEmpty()?uiText("编辑点组"):title);auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;QMap<QString,QWidget*> fields;
    auto note=[&](QString text){auto* label=new StudioMessage(StudioMessageRole::Explanation,text);layout->addWidget(label);};
    auto num=[&](QString key,QString label,double value,double low=-1e9,double high=1e9){auto* w=new ModelingNumber;w->setDecimals(12);w->setRange(low,high);w->setValue(initial.value(key).toDouble(value));w->setProperty("field",key);fields[key]=w;form->addRow(label,w);return w;};
    auto integer=[&](QString key,QString label,int value,int low,int high){auto* w=new QSpinBox;w->setRange(low,high);w->setValue(initial.value(key).toInt(value));w->setProperty("field",key);fields[key]=w;form->addRow(label,w);};
    auto text=[&](QString key,QString label,QString value){auto* w=new QLineEdit(initial.value(key).toString(value));w->setMaxLength(4096);w->setProperty("field",key);fields[key]=w;form->addRow(label,w);};
    auto check=[&](QString key,QString label,bool value){auto* w=new QCheckBox(label);w->setChecked(initial.value(key).toBool(value));w->setProperty("field",key);fields[key]=w;form->addRow(w);};
    auto combo=[&](QString key,QString label,QStringList choices,int selected=0){auto* w=new QComboBox;addUiItems(w,choices);w->setCurrentIndex(selected);w->setProperty("field",key);fields[key]=w;form->addRow(label,w);return w;};
    auto layer=[&](QString key,QString label){auto* w=combo(key,label,{});for(const auto& l:m->layers)w->addItem(layerDisplayName(l),double(l.id));int i=w->findData(double(layers_->selectedLayerId()));if(i>=0)w->setCurrentIndex(i);};
    auto vector=[&](QString key,QString label,fs::Vec3 v={}){num(key+"_x",label+" X",v.x);num(key+"_y",label+" Y",v.y);num(key+"_z",label+" Z",v.z);};
    QJsonObject request;QListWidget* layerList=nullptr;
    if(op=="new"){
        num("length",uiText("船长"),20,0.001);num("beam",uiText("船宽"),5,0.001);num("draft",uiText("设计吃水"),2,0.001);
        integer("columns",uiText("纵向控制点数"),7,2,100);integer("rows",uiText("横向控制点数"),5,2,100);combo("units",uiText("单位"),{"米","英尺"});
    }else if(op=="grid_panel"){
        vector("origin",uiText("起点"));num("size_a",uiText("第一方向尺寸"),1,0.000001);num("size_b",uiText("第二方向尺寸"),1,0.000001);
        integer("columns",uiText("第一方向点数"),4,2,100);integer("rows",uiText("第二方向点数"),4,2,100);combo("plane",uiText("平面"),{"水平面 XY","纵向面 XZ","横向面 YZ"});layer("layer_id",uiText("图层"));
    }else if(op=="cylinder"){
        vector("origin",uiText("轴起点"));vector("end",uiText("轴终点"),{0,1,0});num("radius",uiText("半径"),1,0.000001);integer("segments",uiText("圆周控制点数"),12,4,128);
        check("close_start",uiText("封闭起端"),false);check("close_end",uiText("封闭末端"),false);layer("layer_id",uiText("图层"));
    }else if(op=="insert_plane"){
        combo("axis",uiText("平面"),{"横剖面 X","纵剖面 Y","水平面 Z"});num("distance",uiText("坐标"),0);check("create_curve",uiText("生成控制曲线"),true);
    }else if(op=="intersect_layers"){
        layer("layer_id",uiText("插入控制点的图层"));layer("other_layer_id",uiText("用于求交的图层"));
    }else if(op=="rename_control"){
        request["ids"]=array(entitySelection_);QString name;for(const auto& c:m->controlCurves)if(entitySelection_.contains(c.id))name=c.name;text("name",uiText("名称"),name);
    }else if(op=="anchor"||op=="coincide"||op=="align"||op=="linear_constraint"){
        request["ids"]=array(selectionOrder_);
        if(op=="anchor"){note(uiText("第一点作为锚点，其余点保持当前相对位置。移动锚点时，关联点一起移动。"));check("hard",uiText("禁止单独移动关联点"),false);}
        else if(op=="coincide")note(uiText("其余所选点移动到第一点的位置。点与面仍分别保留，不合并拓扑。"));
        else note(op=="align"?uiText("把中间所选点投影到第一点和最后一点确定的直线上。"):uiText("以第一点和最后一点定义直线，其余点持续跟随这条直线。"));
        for(int i=0;i<selectionOrder_.size();++i){const auto* p=m->point(selectionOrder_[i]);note(QString("%1：%2，%3，%4").arg(i+1).arg(p->position.x).arg(p->position.y).arg(p->position.z));}
    }else if(op=="create_group"||op=="update_group"){
        request["ids"]=array(selection_);if(initial.contains("index"))request["index"]=initial["index"];
        text("name",uiText("组名"),QString("Group %1").arg(m->pointGroups.size()+1));check("locked",uiText("固定整组"),false);note(QString(uiText("包含当前选中的 %1 个控制点。")).arg(selection_.size()));
    }else if(op=="lackenby"){
        note(QString(uiText("当前排水量 %1，浮心纵坐标 %2，棱形系数 %3。")).arg(initial["displacement"].toDouble()).arg(initial["lcb"].toDouble()).arg(initial["cp"].toDouble()));
        num("displacement",m->units=="metric"?uiText("目标排水量 (t)"):uiText("目标排水量 (long ton)"),initial["displacement"].toDouble(),0.000001);
        num("lcb",uiText("目标浮心纵坐标"),initial["lcb"].toDouble());integer("iterations",uiText("最大迭代次数"),30,1,100);
        layerList=new QListWidget;layerList->setProperty("field","layer_ids");for(const auto& l:m->layers){auto* item=new QListWidgetItem(layerDisplayName(l),layerList);item->setData(Qt::UserRole,double(l.id));item->setCheckState(l.visible?Qt::Checked:Qt::Unchecked);}form->addRow(uiText("参与变换的图层"),layerList);
    }else if(op=="delete_markers")note(uiText("删除模型中的全部参考曲线。"));
    else if(op=="delete_group"){request["index"]=initial["index"];note(uiText("解除这个点组。组内的控制点和曲面会保留。"));}
    else return;
    if(form->rowCount()>0)layout->addLayout(form);
    else delete form;
    buttons(dialog,layout);if(runStudioDialog(dialog)!=QDialog::Accepted)return;
    if(op!="new"&&(!session_.ready()||session_.busy()||session_.model()!=m)){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
    for(auto it=fields.begin();it!=fields.end();++it){QWidget* w=it.value();QJsonValue v;
        if(auto* n=qobject_cast<QDoubleSpinBox*>(w))v=n->value();else if(auto* n=qobject_cast<QSpinBox*>(w))v=n->value();
        else if(auto* c=qobject_cast<QComboBox*>(w))v=it.key().endsWith("layer_id")?c->currentData().toDouble():c->currentIndex();
        else if(auto* c=qobject_cast<QCheckBox*>(w))v=c->isChecked();else if(auto* t=qobject_cast<QLineEdit*>(w))v=t->text();request[it.key()]=v;
    }
    for(const auto& key:{"origin","end","position"})if(request.contains(QString(key)+"_x")){
        QJsonArray a;for(auto axis:{"_x","_y","_z"})a.append(request.take(QString(key)+axis));request[key]=a;
    }
    if(op=="insert_plane"){QJsonArray a{0,0,0};a[request["axis"].toInt()]=request.take("distance");request["position"]=a;}
    if(layerList){QJsonArray ids;for(int i=0;i<layerList->count();++i)if(layerList->item(i)->checkState()==Qt::Checked)ids.append(layerList->item(i)->data(Qt::UserRole).toDouble());request["ids"]=ids;}
    if(op=="new")destinationWindow()->session_.newHull(request);else if(op=="rename_control")session_.editAuxiliary(op,request);else session_.editModeling(op,request);
}

void MainWindow::pointGroupsDialog(){
    const auto m=session_.model();if(!m||session_.busy())return;
    QDialog dialog(this);setStudioWindowIcon(&dialog,"Point");dialog.setWindowTitle(uiText("点组"));dialog.setProperty("modeling_operation","point_groups");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);auto* list=new QListWidget;
    for(const auto& g:m->pointGroups)list->addItem(QString(uiText("%1 · %2 个点%3")).arg(g.name).arg(g.points.size()).arg(g.locked?uiText(" · 已固定"):""));
    layout->addWidget(list);int action=0;auto* controls=new QDialogButtonBox;
    for(const auto& pair:{std::pair<QString,int>{uiText("选择","action"),1},{uiText("编辑"),2},{uiText("解组"),3}}){auto* b=controls->addButton(pair.first,QDialogButtonBox::ActionRole);b->setProperty("group_action",pair.second);if(pair.second==1)bindUiText(b,"text","选择","action");connect(b,&QPushButton::clicked,&dialog,[&dialog,&action,list,code=pair.second]{if(list->currentRow()>=0){action=code;dialog.accept();}});}
    layout->addWidget(controls);
    if(runStudioDialog(dialog)!=QDialog::Accepted)return;
    if(!session_.ready()||session_.busy()||session_.model()!=m){showError(uiText("船模已更改，请重新打开此窗口。"));return;}
    const auto& g=m->pointGroups[list->currentRow()];setSelectionKind(HullEntity::Point);selectPoints(QSet<fs::EntityId>(g.points.begin(),g.points.end()));
    if(selection_.size()!=g.points.size()){showError(uiText("点组包含隐藏图层的点。请先显示这些图层，再操作整组。"));return;}
    if(action>1)modelingDialog(action==2?"update_group":"delete_group",{{"index",g.index},{"name",g.name},{"locked",g.locked}});
}

void MainWindow::modelCheckDialog(){
    const auto m=session_.model();if(!m||session_.busy())return;const auto issues=fs::inspectModel(*m);
    QDialog dialog(this);setStudioWindowIcon(&dialog,"ModelCheck");dialog.setWindowTitle(uiText("拓扑检查报告"));dialog.setProperty("modeling_operation","model_check");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);auto* label=new StudioMessage(StudioMessageRole::Feedback,issues.isEmpty()?uiText("未发现本检查覆盖的拓扑问题。"):QString{});layout->addWidget(label);
    auto* list=new QListWidget;for(const auto& issue:issues)list->addItem(issue.message);layout->addWidget(list);
    auto* b=new QDialogButtonBox(&dialog);auto* locate=b->addButton(uiText("定位"),QDialogButtonBox::ActionRole);locate->setProperty("check_action","locate");
    const auto targets=[this,&issues,m](int row){
        if(row<0||row>=issues.size())return QSet<fs::EntityId>{};
        const auto& issue=issues[row];
        if(issue.code=="leaks"||issue.code=="unused"){
            const auto command=issue.code=="leaks"?"select_leaks":"select_points_without_faces";
            return checkSelectionTargets(*m,command,hiddenControlLayers_,layers_->projectedVisibilityChanges());
        }
        return QSet<fs::EntityId>(issue.ids.begin(),issue.ids.end());
    };
    b->setVisible(!issues.isEmpty());
    if(!issues.isEmpty())list->setCurrentRow(0);
    locate->setEnabled(!targets(list->currentRow()).isEmpty());
    connect(list,&QListWidget::currentRowChanged,locate,[locate,targets](int row){locate->setEnabled(!targets(row).isEmpty());});
    connect(locate,&QPushButton::clicked,&dialog,[this,&dialog,list,locate,m,&issues,targets]{
        const int row=list->currentRow();
        if(!session_.ready()||session_.busy()||session_.model()!=m){
            showError(uiText("船模已更改，请重新打开此窗口。"));
            return;
        }
        auto ids=targets(row);
        if(ids.isEmpty()){locate->setEnabled(false);return;}
        const auto& issue=issues[row];
        const auto kind=issue.kind=="point"?HullEntity::Point:issue.kind=="edge"?HullEntity::Edge:HullEntity::Face;
        activateDocument();
        setSelectionKind(kind);
        ensureSelectionView(views_,kind);
        if(kind==HullEntity::Point){
            if(views_.front()->onShowControlNet)views_.front()->onShowControlNet();
            pointSelectionHasBatchOrder_=true;
            selectPoints(std::move(ids));
        }else selectEntities(kind,std::move(ids));
        dialog.accept();
    });
    layout->addWidget(b);
    dialog.resize(600,380);runStudioDialog(dialog);
}

void MainWindow::faceOrientationDialog(){
    if(faceOrientationWindow_){faceOrientationWindow_->raise();faceOrientationWindow_->activateWindow();return;}
    const auto before=session_.model();
    if(!before||session_.busy())return;
    const auto conflicts=orientationConflicts(*before);
    if(conflicts.isEmpty())return;

    auto* dialog=new QDialog(this);
    faceOrientationWindow_=dialog;
    connect(dialog,&QDialog::finished,this,[this,dialog]{if(faceOrientationWindow_==dialog)faceOrientationWindow_.clear();});
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    dialog->setProperty("modeling_operation","repair_normals");
    setStudioWindowIcon(dialog,"InvertFace");
    dialog->setWindowTitle(uiText("修复相邻面朝向"));
    auto* layout=new QVBoxLayout(dialog);
    auto* summary=new StudioMessage(StudioMessageRole::Feedback,QString(uiText("%1 条接缝的面朝向不一致")).arg(conflicts.size()),dialog);
    summary->setObjectName("FaceOrientationSummary");
    layout->addWidget(summary);

    auto* preview=new HullView(dialog);
    preview->setObjectName("FaceOrientationPreview");
    preview->setStudioAppearance(true);
    preview->setAnimationsEnabled(false);
    preview->setEditingEnabled(false);
    preview->setProjection(HullProjection::Perspective);
    preview->setSnapshot(before,{},false,hiddenLayers(*before));
    preview->setFaceOrientationPreview(conflicts);
    preview->setMinimumSize(350,210);
    layout->addWidget(preview,1);

    auto* legend=new QHBoxLayout;
    legend->addWidget(orientationLegend("#578cca",uiText("正面"),dialog));
    legend->addWidget(orientationLegend("#c7584f",uiText("背面"),dialog));
    legend->addWidget(orientationLegend("#ffffff",uiText("无控制面"),dialog));
    auto* seam=orientationLegend("#e39016",uiText("朝向冲突"),dialog);
    auto* changed=orientationLegend("#0b604f",uiText("已翻转"),dialog);
    legend->addWidget(seam);legend->addWidget(changed);legend->addStretch();
    changed->hide();
    layout->addLayout(legend);
    auto* footer=new QHBoxLayout;
    auto* note=new QLabel(uiText("请检查曲面的外侧方向。"),dialog);
    note->setWordWrap(true);
    footer->addWidget(note,1);
    auto* apply=new QPushButton(uiText("修复"),dialog);
    apply->setObjectName("FaceOrientationApply");
    footer->addWidget(apply);
    layout->addLayout(footer);
    dialog->resize(640,460);

    connect(apply,&QPushButton::clicked,dialog,[this,dialog,summary,preview,seam,changed,apply,before,conflicts]{
        if(session_.busy()){
            summary->setMessage(uiText("请等待模型更新完成。"),StudioMessageRole::Warning);
            return;
        }
        if(!session_.ready()||session_.model()!=before){
            apply->setEnabled(false);
            summary->setMessage(uiText("船模已更改，请重新打开此窗口。"),StudioMessageRole::Warning);
            return;
        }
        apply->setEnabled(false);
        summary->setMessage(uiText("正在修复面朝向…"),StudioMessageRole::Feedback,true);
        faceRepairResult_=[this,window=QPointer<QDialog>(dialog),summary,preview,seam,changed,apply,before,count=conflicts.size()](bool success){
            if(!window)return;
            if(!success){
                apply->setEnabled(true);
                summary->setMessage(uiText("面朝向修复失败。"),StudioMessageRole::Warning);
                return;
            }
            const auto after=session_.model();
            QHash<fs::EntityId,const fs::ControlFace*> previous;
            for(const auto& face:before->faces)previous.insert(face.id,&face);
            QSet<fs::EntityId> flipped;
            for(const auto& face:after->faces)
                if(const auto* old=previous.value(face.id);old&&old->points!=face.points)flipped.insert(face.id);
            const auto remaining=orientationConflicts(*after);
            preview->setSnapshot(after,{},false,hiddenLayers(*after));
            preview->setFaceOrientationPreview(remaining,flipped);
            seam->setVisible(!remaining.isEmpty());
            changed->setVisible(!flipped.isEmpty());
            summary->setMessage(QString(uiText("翻转了 %1 个面，朝向冲突 %2 → %3")).arg(flipped.size()).arg(count).arg(remaining.size()),StudioMessageRole::Feedback);
            window->setProperty("face_orientation_repaired",true);
            QTimer::singleShot(3500,preview,[preview,changed,remaining]{preview->setFaceOrientationPreview(remaining);changed->hide();});
        };
        if(!session_.editModeling("repair_normals"))
            if(auto done=std::exchange(faceRepairResult_,{}))done(false);
    });
    showStudioDialog(*dialog);
    preview->fit();
}
