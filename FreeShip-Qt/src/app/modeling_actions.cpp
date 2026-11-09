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
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

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
QJsonArray array(const QVector<fs::EntityId>& ids){QJsonArray a;for(auto id:ids)a.append(double(id));return a;}
QJsonArray array(const QSet<fs::EntityId>& ids){auto list=ids.values();std::sort(list.begin(),list.end());return array(list);}
void buttons(QDialog& dialog,QVBoxLayout* layout){
    auto* b=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    b->button(QDialogButtonBox::Ok)->setText("确定");b->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(b,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(b,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(b);
}
}

void MainWindow::buildModelingActions(QMenu* point,QMenu* edge,QMenu* face,QMenu* layer,QMenu* view,QMenu* transform){
    auto add=[this](QMenu* menu,const QString& title,const QString& icon,const QString& op,const QString& kind={},int count=0){
        auto* a=new QAction(studioIcon(icon),title,this);a->setProperty("operation",op);a->setProperty("selection_kind",kind);a->setProperty("minimum_selection",count);
        menu->addAction(a);addAction(a);modelingActions_.append(a);connect(a,&QAction::triggered,this,[this,op]{modelingAction(op);});return a;
    };
    QMenu* fileMenu=nullptr;for(auto* a:menuBar()->actions())if(a->menu()&&a->menu()->actions().contains(open_)){fileMenu=a->menu();break;}
    if(!fileMenu)fileMenu=point;
    auto* fresh=add(fileMenu,"新建船型…","NewFile","new");fresh->setShortcut(QKeySequence::New);
    point->addSeparator();add(point,"控制点属性…","ObjectDetails","point_properties","point",1);
    add(point,"折叠所选点","Collapse","collapse_points","point",1);
    add(point,"挤出所选点…","Point","extrude_points","point",1);
    auto* constraints=point->addMenu("对齐与约束");
    add(constraints,"投影到首尾两点的直线","AlignPoints","align","point",3);
    add(constraints,"建立直线约束","AlignPoints","linear_constraint","point",3);
    add(constraints,"锚定到第一点…","Lock","anchor","point",2);
    add(constraints,"与第一点重合","AlignPoints","coincide","point",2);
    add(constraints,"解除所选点的约束","Unlock","clear_constraints","point",1);
    add(point,"解锁全部点","Unlock","unlock_all");
    add(point,"建立点组…","Point","create_group","point",2);add(point,"管理点组…","Point","point_groups");
    add(point,"用平面插入控制点…","SplitSection","insert_plane");
    add(point,"插入图层交点…","Sections","intersect_layers");
    add(point,"清理无面控制点…","DeleteSelected","remove_unused");
    edge->addSeparator();add(edge,"连接所选控制点","Edge","connect_points","point",2);
    add(edge,"拆分所选边","SplitEdge","split_edges","edge",1);
    add(edge,"合并相邻面","Collapse","collapse_edges","edge",1);
    add(edge,"控制曲线名称…","NewCurve","rename_control","curve",1);
    face->addSeparator();add(face,"翻转所选面的朝向","InvertFace","flip_faces","face",1);
    add(face,"删除负舷的面…","DeleteSelected","delete_negative");add(face,"组装面片…","Face","assemble_faces");
    add(face,"添加网格面板…","Grid","grid_panel");add(face,"添加圆柱…","Face","cylinder");
    add(layer,"按折痕自动分组…","LayerAutoGroup","auto_group_layers");
    add(transform,"绕船中旋转…","RotateCW","rotate_midship","transform",1);
    add(transform,"Lackenby 船型变换…","Transform","lackenby");
    auto* selection=point->addMenu("选择");add(selection,"全选","SelectAll","select_all");add(selection,"反选","SelectAll","invert_selection");
    add(selection,"选择漏点","Point","select_leaks");add(selection,"取消选择","DeselectAll","clear_selection");
    for(const auto& item:{std::array<QString,3>{"法向","Normals","show_normals"},{"标记","Markers","show_markers"}}){auto* a=add(view,item[0],item[1],item[2]);a->setCheckable(true);}
    auto* markers=view->addMenu("标记管理");add(markers,"导入标记…","OpenFile","add_markers");add(markers,"删除全部标记…","DeleteSelected","delete_markers");
    session_.onAnalysis=[this](const QJsonObject& info){modelingDialog("lackenby",info);
        if(modelingWorkflow_&&!session_.busy())QTimer::singleShot(0,this,[this]{modelingStep("analysis_closed");});};
}

void MainWindow::updateModelingActions(){
    const bool ready=session_.ready(),idle=!session_.busy();const auto selectedKind=selectionKind_==HullEntity::Point?"point":selectionKind_==HullEntity::Edge?"edge":selectionKind_==HullEntity::Face?"face":selectionKind_==HullEntity::Curve?"curve":"flow";
    for(auto* a:modelingActions_){const auto op=a->property("operation").toString(),kind=a->property("selection_kind").toString();const int minimum=a->property("minimum_selection").toInt();
        int count=kind=="transform"?transformPoints().size():kind=="point"?selection_.size():entitySelection_.size();
        bool enabled=idle&&(op=="new"||ready)&&(kind.isEmpty()||kind=="transform"||kind==selectedKind)&&count>=minimum;
        if(op=="point_properties"||op=="rename_control")enabled=enabled&&count==1;
        if(ready&&op=="point_groups")enabled=enabled&&!session_.model()->pointGroups.isEmpty();
        if(ready&&op=="delete_markers")enabled=enabled&&!session_.model()->markers.isEmpty();
        a->setEnabled(enabled);
        if(a->isCheckable()){QSignalBlocker b(a);a->setChecked(ready&&(op=="show_normals"?session_.model()->showNormals:session_.model()->showMarkers));}
    }
}

void MainWindow::modelingAction(const QString& op){
    if(session_.busy()||(op!="new"&&!session_.ready()))return;endFlowline();const auto m=session_.model();
    if(op=="new"){withSavedChanges([this]{modelingDialog("new");});return;}
    if(op=="show_normals"||op=="show_markers"){session_.editDisplay({{op,op=="show_normals"?!m->showNormals:!m->showMarkers}});return;}
    if(op=="point_groups"){pointGroupsDialog();return;}
    if(op=="lackenby"){session_.inspectModeling();return;}
    if(op=="rotate_midship"){advancedDialog(op);return;}
    if(op=="clear_selection"){selectEntities(selectionKind_,{});selectPoints({});return;}
    if(op=="select_leaks"){setSelectionKind(HullEntity::Point);QSet<fs::EntityId> ids;for(const auto& p:m->points)if(p.leak)ids.insert(p.id);selectPoints(ids);return;}
    if(op=="select_all"||op=="invert_selection"){
        QSet<fs::EntityId> ids;
        if(selectionKind_==HullEntity::Point)for(const auto& p:m->points)ids.insert(p.id);
        else if(selectionKind_==HullEntity::Edge)for(const auto& e:m->edges)ids.insert(e.id);
        else if(selectionKind_==HullEntity::Face)for(const auto& f:m->faces)ids.insert(f.id);
        else for(const auto& c:selectionKind_==HullEntity::Curve?m->controlCurves:m->flowlines)ids.insert(c.id);
        if(op=="invert_selection")ids.subtract(selectionKind_==HullEntity::Point?selection_:entitySelection_);
        if(selectionKind_==HullEntity::Point)selectPoints(ids);else selectEntities(selectionKind_,ids);return;
    }
    if(op=="add_markers"){
        QFileDialog dialog(this,"导入标记",preferenceFileLocation("import"));dialog.setProperty("modeling_operation",op);dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilter("标记文本 (*.txt);;所有文件 (*)");dialog.setLabelText(QFileDialog::Accept,"打开");dialog.setLabelText(QFileDialog::Reject,"取消");
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().size()!=1)return;const auto filename=dialog.selectedFiles().first();
        QFile file(filename);if(file.size()>8*1024*1024||!file.open(QIODevice::ReadOnly)){showError("无法读取标记文件，或文件超过 8 MB。");return;}
        // Plus markers have a heading and XYZ paths separated by blank lines.
        // Coordinates keep the current model units.
        try{session_.editModeling(op,{{"paths",fs::parseMarkerText(file.readAll(),m->units)}});}catch(const std::exception& e){showError(QString::fromUtf8(e.what()));}return;
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
    QString title;for(auto* a:modelingActions_)if(a->property("operation")==op){title=a->text();break;}
    dialog.setWindowTitle(title.isEmpty()?"编辑点组":title);auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;QMap<QString,QWidget*> fields;
    auto note=[&](QString text){auto* label=new QLabel(text);label->setWordWrap(true);layout->addWidget(label);};
    auto num=[&](QString key,QString label,double value,double low=-1e9,double high=1e9){auto* w=new ModelingNumber;w->setDecimals(12);w->setRange(low,high);w->setValue(initial.value(key).toDouble(value));w->setProperty("field",key);fields[key]=w;form->addRow(label,w);return w;};
    auto integer=[&](QString key,QString label,int value,int low,int high){auto* w=new QSpinBox;w->setRange(low,high);w->setValue(initial.value(key).toInt(value));w->setProperty("field",key);fields[key]=w;form->addRow(label,w);};
    auto text=[&](QString key,QString label,QString value){auto* w=new QLineEdit(initial.value(key).toString(value));w->setMaxLength(4096);w->setProperty("field",key);fields[key]=w;form->addRow(label,w);};
    auto check=[&](QString key,QString label,bool value){auto* w=new QCheckBox(label);w->setChecked(initial.value(key).toBool(value));w->setProperty("field",key);fields[key]=w;form->addRow(w);};
    auto combo=[&](QString key,QString label,QStringList choices,int selected=0){auto* w=new QComboBox;w->addItems(choices);w->setCurrentIndex(selected);w->setProperty("field",key);fields[key]=w;form->addRow(label,w);return w;};
    auto layer=[&](QString key,QString label){auto* w=combo(key,label,{});for(const auto& l:m->layers)w->addItem(l.name,double(l.id));int i=w->findData(double(layers_->selectedLayerId()));if(i>=0)w->setCurrentIndex(i);};
    auto vector=[&](QString key,QString label,fs::Vec3 v={}){num(key+"_x",label+" X",v.x);num(key+"_y",label+" Y",v.y);num(key+"_z",label+" Z",v.z);};
    QJsonObject request;QListWidget* layerList=nullptr;
    if(op=="new"){
        num("length","船长",20,0.001);num("beam","船宽",5,0.001);num("draft","设计吃水",2,0.001);
        integer("columns","纵向控制点数",7,2,100);integer("rows","横向控制点数",5,2,100);combo("units","单位",{"米","英尺"});
    }else if(op=="grid_panel"){
        vector("origin","起点");num("size_a","第一方向尺寸",1,0.000001);num("size_b","第二方向尺寸",1,0.000001);
        integer("columns","第一方向点数",4,2,100);integer("rows","第二方向点数",4,2,100);combo("plane","平面",{"水平面 XY","纵向面 XZ","横向面 YZ"});layer("layer_id","图层");
    }else if(op=="cylinder"){
        vector("origin","轴起点");vector("end","轴终点",{0,1,0});num("radius","半径",1,0.000001);integer("segments","圆周控制点数",12,4,128);
        check("close_start","封闭起端",false);check("close_end","封闭末端",false);layer("layer_id","图层");
    }else if(op=="insert_plane"){
        combo("axis","平面",{"横剖面 X","纵剖面 Y","水平面 Z"});num("distance","坐标",0);check("create_curve","生成控制曲线",true);
    }else if(op=="intersect_layers"){
        layer("layer_id","插入控制点的图层");layer("other_layer_id","用于求交的图层");
    }else if(op=="point_properties"){
        if(selection_.size()!=1)return;const auto* p=m->point(*selection_.begin());request["ids"]=array(selection_);
        text("name","名称",p->name);combo("vertex_type","点类型",{"普通点","折痕点","尖点","角点"},p->vertexType);
        if(p->anchor||p->lineA)note(p->anchor?"该点有锚点约束，可通过“解除所选点的约束”移除。":"该点位于约束直线上，可通过“解除所选点的约束”移除。");
    }else if(op=="rename_control"){
        request["ids"]=array(entitySelection_);QString name;for(const auto& c:m->controlCurves)if(entitySelection_.contains(c.id))name=c.name;text("name","名称",name);
    }else if(op=="extrude_points"){request["ids"]=array(selection_);vector("position","偏移量");
    }else if(op=="anchor"||op=="coincide"||op=="align"||op=="linear_constraint"){
        request["ids"]=array(selectionOrder_);
        if(op=="anchor"){note("第一点作为锚点，其余点保持当前相对位置。移动锚点时，关联点一起移动。");check("hard","禁止单独移动关联点",false);}
        else if(op=="coincide")note("其余所选点移动到第一点的位置。点与面仍分别保留，不合并拓扑。");
        else note(op=="align"?"把中间所选点投影到第一点和最后一点确定的直线上。":"以第一点和最后一点定义直线，其余点持续跟随这条直线。");
        for(int i=0;i<selectionOrder_.size();++i){const auto* p=m->point(selectionOrder_[i]);note(QString("%1：%2，%3，%4").arg(i+1).arg(p->position.x).arg(p->position.y).arg(p->position.z));}
    }else if(op=="create_group"||op=="update_group"){
        request["ids"]=array(selection_);if(initial.contains("index"))request["index"]=initial["index"];
        text("name","组名",QString("Group %1").arg(m->pointGroups.size()+1));check("locked","锁定整组",false);note(QString("包含当前选中的 %1 个控制点。").arg(selection_.size()));
    }else if(op=="lackenby"){
        note(QString("当前排水量 %1，浮心纵坐标 %2，棱形系数 %3。\n迭代改变控制点的纵坐标；未收敛时恢复原模型。").arg(initial["displacement"].toDouble()).arg(initial["lcb"].toDouble()).arg(initial["cp"].toDouble()));
        num("displacement",m->units=="metric"?"目标排水量 (t)":"目标排水量 (long ton)",initial["displacement"].toDouble(),0.000001);
        num("lcb","目标浮心纵坐标",initial["lcb"].toDouble());integer("iterations","最大迭代次数",30,1,100);
        layerList=new QListWidget;layerList->setProperty("field","layer_ids");for(const auto& l:m->layers){auto* item=new QListWidgetItem(l.name,layerList);item->setData(Qt::UserRole,double(l.id));item->setCheckState(l.visible?Qt::Checked:Qt::Unchecked);}form->addRow("参与变换的图层",layerList);
    }else if(op=="remove_unused")note("删除所有不属于任何面的控制点，包括独立控制线上的点。可以撤销。");
    else if(op=="delete_negative")note("删除全部顶点位于 Y ≤ 0 一侧的面，包括中心面，以及负舷未被面使用的点。跨过中心面的面保持完整。可以撤销。");
    else if(op=="delete_markers")note("删除模型中的全部标记。可以撤销。");
    else if(op=="assemble_faces")note("把可组装为规则面片的面分配到各自的图层，保留原图层属性。可以撤销。");
    else if(op=="auto_group_layers"){
        note("沿非折痕边连接的面归入同一组。已选面时处理所选面，否则处理可见图层。可以撤销。");
        if(selectionKind_==HullEntity::Face&&!entitySelection_.isEmpty())request["ids"]=array(entitySelection_);
    }else if(op=="repair_normals")note("统一相邻面的朝向，不改变控制点坐标。开放曲面仍需要人工确认哪一面朝外。可以撤销。");
    else if(op=="delete_group"){request["index"]=initial["index"];note("解除这个点组。组内的控制点和曲面会保留。");}
    else return;
    layout->addLayout(form);buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return;
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
    if(op=="new")session_.newHull(request);else if(op=="rename_control")session_.editAuxiliary(op,request);else session_.editModeling(op,request);
}

void MainWindow::pointGroupsDialog(){
    const auto m=session_.model();if(!m||session_.busy())return;
    QDialog dialog(this);dialog.setWindowTitle("点组");dialog.setProperty("modeling_operation","point_groups");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);auto* list=new QListWidget;
    for(const auto& g:m->pointGroups)list->addItem(QString("%1 · %2 个点%3").arg(g.name).arg(g.points.size()).arg(g.locked?" · 已锁定":""));
    layout->addWidget(list);int action=0;auto* controls=new QDialogButtonBox;
    for(const auto& pair:{std::pair<QString,int>{"选择",1},{"编辑",2},{"解组",3}}){auto* b=controls->addButton(pair.first,QDialogButtonBox::ActionRole);b->setProperty("group_action",pair.second);connect(b,&QPushButton::clicked,&dialog,[&dialog,&action,list,code=pair.second]{if(list->currentRow()>=0){action=code;dialog.accept();}});}
    auto* close=controls->addButton("关闭",QDialogButtonBox::RejectRole);connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);layout->addWidget(controls);
    if(dialog.exec()!=QDialog::Accepted)return;const auto& g=m->pointGroups[list->currentRow()];setSelectionKind(HullEntity::Point);selectPoints(QSet<fs::EntityId>(g.points.begin(),g.points.end()));
    if(selection_.size()!=g.points.size()){showError("点组包含隐藏图层的点。请先显示这些图层，再操作整组。");return;}
    if(action>1)modelingDialog(action==2?"update_group":"delete_group",{{"index",g.index},{"name",g.name},{"locked",g.locked}});
}

void MainWindow::modelCheckDialog(){
    const auto m=session_.model();if(!m||session_.busy())return;const auto issues=fs::inspectModel(*m);
    QDialog dialog(this);dialog.setWindowTitle("模型检查");dialog.setProperty("modeling_operation","model_check");dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
    auto* layout=new QVBoxLayout(&dialog);auto* label=new QLabel(issues.isEmpty()?"未发现本检查覆盖的拓扑问题。":"选择一项可定位相关对象。检查不会修改模型。");layout->addWidget(label);
    auto* list=new QListWidget;for(const auto& issue:issues)list->addItem(issue.message);layout->addWidget(list);
    auto* b=new QDialogButtonBox;auto* locate=b->addButton("定位",QDialogButtonBox::ActionRole);locate->setProperty("check_action","locate");
    connect(locate,&QPushButton::clicked,&dialog,[this,list,issues]{int i=list->currentRow();if(i<0)return;const auto& issue=issues[i];
        const auto kind=issue.kind=="point"?HullEntity::Point:issue.kind=="edge"?HullEntity::Edge:HullEntity::Face;setSelectionKind(kind);
        QSet<fs::EntityId> ids(issue.ids.begin(),issue.ids.end());if(kind==HullEntity::Point)selectPoints(ids);else selectEntities(kind,ids);});
    auto* repair=b->addButton("修复相邻面朝向…",QDialogButtonBox::ActionRole);repair->setProperty("check_action","repair_normals");
    bool fix=false;connect(repair,&QPushButton::clicked,&dialog,[&]{fix=true;dialog.accept();});auto* close=b->addButton("关闭",QDialogButtonBox::RejectRole);connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);layout->addWidget(b);
    dialog.resize(600,380);dialog.exec();if(fix)modelingDialog("repair_normals");
}
