#include "app/ui_language.h"
#include "preferences_store.h"
#include "plates_dialog.h"
#include "text_style.h"
#include "model_session.h"
#include "studio_ui.h"
#include "studio_read_ui.h"
#include "render/winding_contours.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "feedback_text.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include "print_preview_style.h"
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QApplication>
#include <QJsonArray>
#include <cmath>
#include <stdexcept>

namespace {
QPointF plateScreen(QPointF p){return {p.x(),-p.y()};}
class PlateView:public QGraphicsView {
    QPoint last_;bool pan_=false;
public:
    explicit PlateView(QGraphicsScene* scene,QWidget* parent):QGraphicsView(scene,parent){
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setProperty("studio_canvas_kind","plates");
    }
protected:
    void wheelEvent(QWheelEvent* e)override{
        const auto before=mapToScene(e->position().toPoint());double factor=std::pow(1.0015,e->angleDelta().y());
        double next=transform().m11()*factor;if(next>1e-6&&next<1e7){scale(factor,factor);const auto after=mapToScene(e->position().toPoint());translate(after.x()-before.x(),after.y()-before.y());}e->accept();
    }
    void mousePressEvent(QMouseEvent*e)override{if(e->button()==Qt::RightButton){pan_=true;last_=e->pos();setCursor(Qt::ClosedHandCursor);e->accept();}else QGraphicsView::mousePressEvent(e);}
    void mouseMoveEvent(QMouseEvent*e)override{if(pan_){auto delta=mapToScene(e->pos())-mapToScene(last_);translate(delta.x(),delta.y());last_=e->pos();e->accept();}else QGraphicsView::mouseMoveEvent(e);}
    void mouseReleaseEvent(QMouseEvent*e)override{if(e->button()==Qt::RightButton){pan_=false;unsetCursor();e->accept();}else QGraphicsView::mouseReleaseEvent(e);}
};
struct PlateStyle {bool fill=true,interior=false,errors=false,dimensions=false,names=true,submerged=true,boxes=false;std::array<bool,4> sections{true,true,true,true};};
// Device caches accelerate interactive translation only. Exports and print
// render the original paths at the destination resolution, including PDF vectors.
class UncachedPlateRender {
    QVector<QPair<QGraphicsItem*,QGraphicsItem::CacheMode>> saved_;
public:
    explicit UncachedPlateRender(const QVector<QGraphicsItem*>& items){for(auto* item:items){saved_.append({item,item->cacheMode()});item->setCacheMode(QGraphicsItem::NoCache);}}
    ~UncachedPlateRender(){for(const auto& entry:saved_)entry.first->setCacheMode(entry.second);}
};
class PlateScene:public QGraphicsScene {
public:
    using QGraphicsScene::QGraphicsScene;
    double gridX=0,gridY=0;
protected:
    void drawBackground(QPainter* painter,const QRectF& rect)override{
        painter->fillRect(rect,Qt::white);QPen pen(QColor(220,225,232));pen.setCosmetic(true);painter->setPen(pen);
        if(gridX>0&&rect.width()/gridX<500){double first=std::ceil(rect.left()/gridX)*gridX;for(int i=0;i<501;++i){double x=first+i*gridX;if(x>rect.right())break;painter->drawLine(QPointF(x,rect.top()),QPointF(x,rect.bottom()));}}
        if(gridY>0&&rect.height()/gridY<500){double first=std::ceil(rect.top()/gridY)*gridY;for(int i=0;i<501;++i){double y=first+i*gridY;if(y>rect.bottom())break;painter->drawLine(QPointF(rect.left(),y),QPointF(rect.right(),y));}}
    }
};
class PlateItem:public QGraphicsItem {
public:
    quint64 paintCalls=0;
    fs::DevelopedPlate& plate;PlateStyle style;QPainterPath fill,under,screenFill,screenUnder;QVector<QLineF> boundary,inner,positive,negative;
    QColor underwaterColor;QVector<fs::PlateCurve> screenCurves;
    std::function<void()> changed,selected;
    PlateItem(fs::DevelopedPlate& p,double waterline,QColor waterColor):plate(p),underwaterColor(waterColor){
        setFlags(ItemIsMovable|ItemIsSelectable|ItemSendsGeometryChanges);
        setCacheMode(DeviceCoordinateCache);
        screenCurves=p.curves;for(auto& curve:screenCurves)for(auto& point:curve.points)point=plateScreen(point);
        fs::WindingContours outline,wetOutline;QVector<QPointF> vertices;vertices.reserve(p.points.size());
        for(auto point:p.points)vertices.append(plateScreen(point));
        std::map<std::pair<int,int>,int> crossings;
        auto crossing=[&](int a,int b){if(a>b)std::swap(a,b);const std::pair<int,int> key{a,b};
            auto found=crossings.find(key);if(found!=crossings.end())return found->second;
            const double za=p.original[a].z()-waterline,zb=p.original[b].z()-waterline;
            const int id=vertices.size();vertices.append(plateScreen(p.points[a]+(p.points[b]-p.points[a])*(za/(za-zb))));crossings.emplace(key,id);return id;};
        for(const auto& face:p.faces){QPolygonF poly;for(int i:face)poly<<plateScreen(p.points[i]);fill.addPolygon(poly);fill.closeSubpath();
            outline.add(face);
            // Clip each original face at the design waterline in its affine
            // unfolded coordinates. This is display shading only.
            for(int t=2;t<face.size();++t){QVector<QPair<QPointF,double>> polygon;
                const QVector<int> triangle{face[0],face[t-1],face[t]};QVector<int> wet;
                for(int j=0;j<3;++j){const int a=triangle[j],b=triangle[(j+1)%3];const double za=p.original[a].z()-waterline,zb=p.original[b].z()-waterline;
                    if(za<=0)wet.append(a);if((za<0&&zb>0)||(za>0&&zb<0))wet.append(crossing(a,b));}
                if(wet.size()>=3)wetOutline.add(wet);
                for(int i:{face[0],face[t-1],face[t]})polygon.append({p.points[i],p.original[i].z()-waterline});
                QPolygonF clipped;for(int j=0;j<polygon.size();++j){auto a=polygon[j],b=polygon[(j+1)%polygon.size()];
                    if(a.second<=0)clipped<<plateScreen(a.first);
                    if((a.second<0&&b.second>0)||(a.second>0&&b.second<0))clipped<<plateScreen(a.first+(b.first-a.first)*(a.second/(a.second-b.second)));}
                if(clipped.size()>=3){under.addPolygon(clipped);under.closeSubpath();}
            }
        }
        fill.setFillRule(Qt::WindingFill);under.setFillRule(Qt::WindingFill);
        screenFill=outline.path(vertices);screenUnder=wetOutline.path(vertices);
        for(const auto&e:p.edges){QLineF line(plateScreen(p.points[e.a]),plateScreen(p.points[e.b]));(e.boundary?boundary:inner).append(line);
            if(e.error>1e-7)positive.append(line);else if(e.error< -1e-7)negative.append(line);}
    }
    QRectF boundingRect()const override{return fill.boundingRect();}
    QPainterPath shape()const override{return fill;}
    void paint(QPainter* painter,const QStyleOptionGraphicsItem*,QWidget*)override{
        ++paintCalls;painter->setRenderHint(QPainter::Antialiasing);painter->setPen(Qt::NoPen);
        // NoCache is used for exact export/reference rendering. Screen contours
        // retain identical winding while omitting cancelling internal edges.
        const bool original=cacheMode()==NoCache;
        if(style.fill){painter->fillPath(original?fill:screenFill,plate.color);if(style.submerged)painter->fillPath(original?under:screenUnder,underwaterColor);}
        auto lines=[&](const QVector<QLineF>& ls,QColor c,double width){QPen pen(c);pen.setWidthF(width);pen.setCosmetic(true);painter->setPen(pen);painter->drawLines(ls);};
        if(style.interior)lines(inner,QColor(174,183,194),.7);
        lines(boundary,isSelected()?QColor(0,117,190):QColor(39,55,73),isSelected()?2:1);
        if(style.errors){lines(positive,QColor(216,57,44),1.4);lines(negative,QColor(20,100,202),1.4);}
        for(const auto& c:screenCurves)if(style.sections[c.kind-1]){QPen pen(c.color);pen.setCosmetic(true);painter->setPen(pen);painter->drawPolyline(c.points);}
        if(style.boxes){QPen pen(QColor(140,150,165),1,Qt::DashLine);pen.setCosmetic(true);painter->setPen(pen);painter->setBrush(Qt::NoBrush);painter->drawRect(boundingRect());}
    }
    QVariant itemChange(GraphicsItemChange change,const QVariant& value)override{
        if(change==ItemPositionHasChanged){plate.offset={pos().x(),-pos().y()};if(changed)changed();}
        if(change==ItemSelectedHasChanged&&value.toBool()&&selected)selected();
        return QGraphicsItem::itemChange(change,value);
    }
};
}
PlatesDialog::PlatesDialog(ModelSession& session,bool hidden,QWidget* parent):QDialog(parent),session_(session),hidden_(hidden){
    setStudioWindowIcon(this,"Face");
    setProperty("calculation_title_source",QStringLiteral("板材展开"));setWindowTitle(uiText("板材展开"));resize(1320,850);if(hidden)setAttribute(Qt::WA_DontShowOnScreen);
    auto* layout=new QVBoxLayout(this);auto* tools=new QHBoxLayout;layout->addLayout(tools);
    auto button=[&](QString text,QString op,std::function<void()> fn){auto*b=new QPushButton(text,this);b->setProperty("plates_action",op);connect(b,&QPushButton::clicked,this,fn);tools->addWidget(b);return b;};
    calculate_=button(uiText("生成展开图"),"calculate",[this]{failure_.clear();status_->setMessage(uiText("正在生成展开图…"),StudioMessageRole::Feedback,true);session_.developPlates();refreshState();});
    cancel_=button(uiText("取消计算"),"cancel",[this]{session_.cancelPlates();});
    button(uiText("适合窗口"),"fit",[this]{fit();});button(uiText("重新排列"),"arrange",[this]{if(data_){data_->arrange();updateItems();fit();select(list_->currentRow());}});
    for(const auto& pair:QVector<QPair<QString,QString>>{{"展开图","dxf"},{"坐标表","txt"},{"展开数据","json"},{"图片","png"},{"展开图","pdf"}}){
        auto* control=button(uiText(pair.first),pair.second,[this,format=pair.second]{chooseExport(format);});
        const auto refreshFormat=[control,pair]{setStudioFormatButton(control,pair.second,uiText("导出 ")+uiText(pair.first)+" ("+pair.second.toUpper()+")");};
        refreshFormat();bindUiRefresh(control,refreshFormat);
    }
    button(uiText("打印预览"),"print",[this]{if(!data_)return;QPrinter printer(QPrinter::HighResolution);printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageOrientation(QPageLayout::Landscape);
        QPrintPreviewDialog preview(&printer,this);styleStudioPrintPreview(&preview,true);preview.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));connect(&preview,&QPrintPreviewDialog::paintRequested,this,[this](QPrinter*p){try{renderPages(*p);}catch(const std::exception&e){showFailure(QString::fromUtf8(e.what()));}});preview.exec();});
    tools->addStretch();auto* splitter=new QSplitter(this);layout->addWidget(splitter,1);
    auto* sidebar=new QWidget;auto* side=new QVBoxLayout(sidebar);list_=new QListWidget;list_->setMinimumHeight(110);side->addWidget(list_,1);
    auto* form=new QFormLayout;side->addLayout(form);
    auto spin=[&](QString text,QString field,double low,double high){auto*w=new QDoubleSpinBox;w->setRange(low,high);w->setDecimals(6);w->setProperty("plate_field",field);form->addRow(text,w);connect(w,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{transformActive();});return w;};
    rotation_=spin(uiText("旋转 / °"),"rotation",-36000,36000);x_=spin(uiText("水平位置"),"x",-1e9,1e9);y_=spin(uiText("垂直位置"),"y",-1e9,1e9);
    mirror_=new QCheckBox(uiText("镜像板片"));mirror_->setProperty("plate_field","mirror");form->addRow(mirror_);connect(mirror_,&QCheckBox::toggled,this,[this]{transformActive();});
    auto* turns=new QHBoxLayout;side->addLayout(turns);for(int angle:{-90,-5,-1,1,5,90}){auto*b=new QPushButton(QString::number(angle)+"°");b->setProperty("plate_rotate",angle);b->setMinimumWidth(44);turns->addWidget(b);connect(b,&QPushButton::clicked,this,[this,angle]{rotation_->setValue(rotation_->value()+angle);});}
    auto toggle=[&](QString text,QString field,bool checked){auto*c=new QCheckBox(text);c->setChecked(checked);c->setProperty("plate_display",field);side->addWidget(c);connect(c,&QCheckBox::toggled,this,[this]{updateItems();});return c;};
    fill_=toggle(uiText("填色"),"fill",true);submerged_=toggle(uiText("水下着色"),"submerged",true);interior_=toggle(uiText("内部边"),"interior",false);errors_=toggle(uiText("边长误差：红色伸长，蓝色缩短"),"errors",false);
    names_=toggle(uiText("板片名称"),"names",true);dimensions_=toggle(uiText("角点坐标（当前布局）"),"dimensions",false);boxes_=toggle(uiText("包围框"),"boxes",false);
    const QStringList labels{uiText("横剖线"),uiText("纵剖线"),uiText("水线"),uiText("斜剖线")};for(int i=0;i<4;++i)sections_[i]=toggle(labels[i],"section"+QString::number(i+1),true);
    auto* gridForm=new QFormLayout;side->addLayout(gridForm);
    for(int i=0;i<2;++i){auto*n=new QDoubleSpinBox;n->setRange(0,1e9);n->setDecimals(6);n->setSpecialValueText(uiText("不显示"));n->setProperty("plate_field",i?"grid_y":"grid_x");gridForm->addRow(i?uiText("网格间距 Y"):uiText("网格间距 X"),n);if(i)gridY_=n;else gridX_=n;connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{updateItems();});}
    fontSize_=new QSpinBox;fontSize_->setRange(6,36);fontSize_->setValue(10);gridForm->addRow(uiText("标注字号"),fontSize_);connect(fontSize_,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateItems();});
    auto* printForm=new QFormLayout;side->addLayout(printForm);scale_=new QDoubleSpinBox;scale_->setRange(0,100000);scale_->setDecimals(3);scale_->setSpecialValueText(uiText("适合单页"));scale_->setValue(0);scale_->setProperty("plate_field","print_scale");printForm->addRow(uiText("打印比例 1 :"),scale_);
    metrics_=new QTableWidget(6,3);metrics_->setObjectName("PlateMetrics");metrics_->setProperty("studio_content_role","result_data");
    const auto refreshHeaders=[this]{metrics_->setHorizontalHeaderLabels({uiText("参数"),uiText("数值"),uiText("单位")});};
    refreshHeaders();bindUiRefresh(metrics_,refreshHeaders);metrics_->verticalHeader()->hide();
    metrics_->setEditTriggers(QAbstractItemView::NoEditTriggers);metrics_->setSelectionMode(QAbstractItemView::NoSelection);
    metrics_->setItemDelegate(new StudioWidgetCellDelegate(metrics_));metrics_->setAlternatingRowColors(false);metrics_->setShowGrid(false);
    metrics_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);metrics_->horizontalHeader()->setStretchLastSection(false);
    metrics_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);metrics_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    metrics_->setStyleSheet("QTableWidget#PlateMetrics{background:#ffffff;border:1px solid #d8e2dc;border-radius:6px;color:#24463d} QTableWidget#PlateMetrics::item{border:0;border-bottom:1px solid #edf1ee;padding:5px 7px;} QTableWidget#PlateMetrics QHeaderView::section{background:#eef3f0;border:0;padding:6px;color:#365e50;}");
    metrics_->hide();side->addWidget(metrics_);
    plateNote_=new StudioMessage(StudioMessageRole::Explanation,uiText("仅展开已勾选“可展开曲面”的图层。展开误差不等于材料成形余量。"));side->addWidget(plateNote_);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(sidebar);scroll->setMinimumWidth(360);splitter->addWidget(scroll);
    scene_=new PlateScene(this);view_=new PlateView(scene_,this);view_->setTransformationAnchor(QGraphicsView::NoAnchor);view_->setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);splitter->addWidget(view_);splitter->setSizes({400,900});
    status_=new StudioMessage(StudioMessageRole::Feedback);status_->enableMathNotation();status_->setObjectName("PlateStatus");layout->addWidget(status_);
    connect(list_,&QListWidget::currentRowChanged,this,&PlatesDialog::select);
    connect(list_,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){if(updating_||!data_)return;data_->plates[list_->row(item)].visible=item->checkState()==Qt::Checked;updateItems();});
    refreshState();
}
void PlatesDialog::reject(){if(session_.isCalculationSession()){QDialog::reject();return;}if(session_.busy()){session_.cancelPlates();status_->setMessage(uiText("正在取消…"),StudioMessageRole::Feedback,true);return;}QDialog::reject();}
void PlatesDialog::refreshState(){
    const bool idle=!session_.busy(),result=data_&&!data_->plates.isEmpty();
    const bool selected=result&&list_->currentRow()>=0&&list_->currentRow()<data_->plates.size();
    calculate_->setEnabled(session_.ready()&&idle);cancel_->setEnabled(!idle);
    if(session_.calculationQueued())status_->setMessage(uiText("排队中")+QString(" · %1").arg(session_.calculationQueuePosition()),StudioMessageRole::Feedback);
    else if(!idle&&property("calculation_was_queued").toBool())status_->setMessage(uiText("正在生成展开图…"),StudioMessageRole::Feedback,true);
    setProperty("calculation_was_queued",session_.calculationQueued());
    for(auto* button:findChildren<QPushButton*>()){
        const auto action=button->property("plates_action").toString();
        if(!action.isEmpty()&&action!="calculate"&&action!="cancel")button->setEnabled(idle&&result);
        if(button->property("plate_rotate").isValid())button->setEnabled(idle&&selected);
    }
    for(auto* widget:findChildren<QWidget*>()){
        const auto field=widget->property("plate_field").toString();
        if(!field.isEmpty())widget->setEnabled(idle&&((field=="rotation"||field=="x"||field=="y"||field=="mirror")?selected:result));
        if(widget->property("plate_display").isValid())widget->setEnabled(idle&&result);
    }
    fontSize_->setEnabled(idle&&result);list_->setEnabled(idle&&result);
}
void PlatesDialog::showFailure(const QString& text){failure_=text;status_->setMessage(uiText(text),StudioMessageRole::Warning);refreshState();}
void PlatesDialog::receive(const QJsonObject& object){
    try{auto next=std::make_unique<fs::DevelopedPlates>(fs::DevelopedPlates::parse(object));scene_->clear();items_.clear();list_->clear();data_=std::move(next);translatePlateNames();buildScene();failure_.clear();completionMessage_=completionText(data_->plates.size(),"已展开 %1 块板片。");status_->setMessage(completionMessage_,StudioMessageRole::Feedback);refreshState();}
    catch(const std::exception&e){showFailure(QString::fromUtf8(e.what()));}
}
void PlatesDialog::translatePlateNames(){
    const QSignalBlocker blockList(list_);
    for(qsizetype i=0;i<data_->plates.size();++i){
        auto& plate=data_->plates[i];
        if(plate.layerName.isEmpty()||plate.partNumber<=0)continue;
        plate.name=QString(uiText("%1 部件 %2")).arg(plate.layerName,QString::number(plate.partNumber))+plate.sideSuffix;
        if(auto* row=list_->item(int(i)))row->setText(plate.name);
    }
}
void PlatesDialog::changeEvent(QEvent* event){
    QDialog::changeEvent(event);
    if(event->type()!=QEvent::LanguageChange||!data_)return;
    translatePlateNames();
    updateItems();
    if(!completionMessage_.isEmpty()&&status_->text()==completionMessage_){
        completionMessage_=completionText(data_->plates.size(),"已展开 %1 块板片。");
        status_->setMessage(completionMessage_,StudioMessageRole::Feedback);
    }
}
void PlatesDialog::buildScene(){
    updating_=true;list_->clear();submerged_->setChecked(data_->shadeSubmerged);
    for(int i=0;i<data_->plates.size();++i){auto&p=data_->plates[i];auto* row=new QListWidgetItem(p.name,list_);row->setFlags(row->flags()|Qt::ItemIsUserCheckable);row->setCheckState(Qt::Checked);
        auto* item=new PlateItem(p,data_->waterline,data_->underwaterColor);scene_->addItem(item);items_.append(item);
        item->selected=[this,i]{if(!updating_)list_->setCurrentRow(i);};
        item->changed=[this,i]{positionChanged(i);};}
    for(auto*n:{x_,y_,gridX_,gridY_})n->setSuffix(" "+data_->lengthUnit());
    updating_=false;updateItems();list_->setCurrentRow(0);fit();
}
void PlatesDialog::select(int row){
    if(!data_||row<0||row>=data_->plates.size()){refreshState();return;}
    updating_=true;auto&p=data_->plates[row];
    rotation_->setValue(p.rotation);x_->setValue(p.offset.x());y_->setValue(p.offset.y());mirror_->setChecked(p.mirrored);
    for(int i=0;i<items_.size();++i)items_[i]->setSelected(i==row);
    const auto m=p.metrics;double multiplier=p.joinedMirror?2:1;QString unit=data_->lengthUnit();
    const QStringList keys{"min_edge_error","max_edge_error","signed_area_error","absolute_area_error","reference_area","normalized_error"};
    const QStringList labels{uiText("最小边长误差"),uiText("最大边长误差"),uiText("面积差（有符号）"),uiText("面积差绝对值之和"),uiText("原表面积"),uiText("无量纲误差")};
    for(int row=0;row<keys.size();++row){
        const double value=m[keys[row]].toDouble()*(row>=2&&row<=4?multiplier:1);
        const QStringList cells{labels[row],QString::number(value,'g',8),row<2?unit:row<5?unit+QString(QChar(0x00b2)):QString{}};
        for(int col=0;col<3;++col){auto* item=new QTableWidgetItem(cells[col]);item->setTextAlignment(col==1?Qt::AlignRight|Qt::AlignVCenter:Qt::AlignLeft|Qt::AlignVCenter);metrics_->setItem(row,col,item);}
        metrics_->setRowHeight(row,44);
    }
    metrics_->setFixedHeight(6*44+metrics_->horizontalHeader()->height()+4);metrics_->show();
    plateNote_->setText(QString(uiText("仅展开已勾选“可展开曲面”的图层。展开误差不等于材料成形余量。"))+"\n\n"+uiText("边长误差为展开长度减原长度。")+(p.joinedMirror?"\n"+QString(uiText("本板含对称的两半；DXF 分别保留两侧闭合轮廓。")):QString{}));updating_=false;refreshState();
}
void PlatesDialog::positionChanged(int row){
    if(updating_||!data_||row<0||row>=data_->plates.size())return;
    updating_=true;const auto& plate=data_->plates[row];
    if(list_->currentRow()==row){x_->setValue(plate.offset.x());y_->setValue(plate.offset.y());}
    if(dimensions_->isChecked())for(auto* child:items_[row]->childItems())if(child->data(0).isValid()){
        auto* label=static_cast<QGraphicsSimpleTextItem*>(child);const auto v=plate.placement().map(plate.points[child->data(0).toInt()]);
        label->setText(QString("%1, %2").arg(v.x(),0,'g',6).arg(v.y(),0,'g',6));
    }
    updating_=false;
}
void PlatesDialog::transformActive(){if(updating_||!data_||list_->currentRow()<0)return;auto&p=data_->plates[list_->currentRow()];const auto field=sender()?sender()->property("plate_field").toString():QString{};
    if(field=="rotation")p.rotation=rotation_->value();else if(field=="x")p.offset.setX(x_->value());else if(field=="y")p.offset.setY(y_->value());else if(field=="mirror")p.mirrored=mirror_->isChecked();
    updateItems();}
void PlatesDialog::updateItems(){if(!data_||updating_)return;updating_=true;
    for(int i=0;i<items_.size();++i){auto* item=static_cast<PlateItem*>(items_[i]);auto&p=data_->plates[i];item->setVisible(p.visible);auto c=plateScreen(p.bounds.center());QTransform tr;tr.translate(c.x(),c.y());tr.rotate(-p.rotation);tr.scale(1,p.mirrored?-1:1);tr.translate(-c.x(),-c.y());item->setTransform(tr);item->setPos(plateScreen(p.offset));
        item->style={fill_->isChecked(),interior_->isChecked(),errors_->isChecked(),dimensions_->isChecked(),names_->isChecked(),submerged_->isChecked(),boxes_->isChecked(),{}};
        for(int k=0;k<4;++k)item->style.sections[k]=sections_[k]->isChecked();
        for(auto* child:item->childItems())delete child;
        auto label=[&](QString text,QPointF pt){auto* l=new QGraphicsSimpleTextItem(text,item);QFont font=this->font();font.setPointSize(fontSize_->value());l->setFont(font);l->setFlag(QGraphicsItem::ItemIgnoresTransformations);
            QColor background=fill_->isChecked()&&item->fill.contains(pt)?(submerged_->isChecked()&&item->under.contains(pt)?data_->underwaterColor:p.color):QColor(Qt::white);
            l->setBrush(background.lightness()<128?QColor(Qt::white):QColor(44,58,73));l->setPos(pt);return l;};
        if(names_->isChecked())label(p.name,c);
        if(dimensions_->isChecked())for(int k:p.corners){auto v=p.placement().map(p.points[k]);label(QString("%1, %2").arg(v.x(),0,'g',6).arg(v.y(),0,'g',6),plateScreen(p.points[k]))->setData(0,k);}
        item->update();}
    static_cast<PlateScene*>(scene_)->gridX=gridX_->value();static_cast<PlateScene*>(scene_)->gridY=gridY_->value();scene_->invalidate(scene_->sceneRect(),QGraphicsScene::BackgroundLayer);
    updating_=false;
}
void PlatesDialog::fit(){if(!data_)return;QRectF rect;for(auto*i:items_)if(i->isVisible())rect=rect.united(i->sceneBoundingRect());if(rect.isEmpty())return;
    const auto d=qMax(rect.width(),rect.height())*.04;scene_->setSceneRect(rect.adjusted(-d,-d,d,d));view_->fitInView(scene_->sceneRect(),Qt::KeepAspectRatio);}
QJsonObject PlatesDialog::result()const{return data_?data_->json():QJsonObject{};}
QJsonObject PlatesDialog::measureDrag(){
    // Only called by the private --verify-plates workflow on an isolated desktop.
    if(!data_||items_.isEmpty())return {};
    QApplication::processEvents();fit();select(0);
    const auto before=result();const auto dxf=data_->dxf({true,true,true,true}),text=data_->text();
    auto* item=static_cast<PlateItem*>(items_[0]);const auto position=item->pos(),modelOffset=item->plate.offset;
    auto* viewport=view_->viewport();QImage image(viewport->size(),QImage::Format_ARGB32_Premultiplied);
    auto render=[&]{image.fill(Qt::white);QPainter painter(&image);viewport->render(&painter);};
    QElapsedTimer timer;timer.start();render();const auto first=timer.nsecsElapsed()/1e6;
    QJsonArray modes;
    for(bool coordinates:{false,true}){
        dimensions_->setChecked(coordinates);render();const auto paints=item->paintCalls;
        const QPoint start=view_->mapFromScene(item->mapToScene(item->boundingRect().center()));
        auto mouse=[&](QEvent::Type type,QPoint pt,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,QPointF(pt),QPointF(viewport->mapToGlobal(pt)),button,buttons,Qt::NoModifier);QApplication::sendEvent(viewport,&event);
        };
        mouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        QJsonArray frames,events;bool moved=false;
        for(int i=0;i<32;++i){const QPoint pt=start+QPoint(i<16?i+1:31-i,(i<16?i+1:31-i)/2);
            timer.restart();mouse(QEvent::MouseMove,pt,Qt::NoButton,Qt::LeftButton);const auto eventMs=timer.nsecsElapsed()/1e6;
            moved|=item->pos()!=position;render();frames.append(timer.nsecsElapsed()/1e6);events.append(eventMs);
        }
        mouse(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);item->setPos(position);
        // QGraphicsItem suppresses near-equal setPos calls. Restore the test
        // model exactly as well, so a sub-ULP pointer round trip is not an export change.
        item->plate.offset=modelOffset;
        modes.append(QJsonObject{{"coordinates",coordinates},{"frames_ms",frames},{"events_ms",events},{"paint_calls",qint64(item->paintCalls-paints)},{"moved",moved}});
    }
    dimensions_->setChecked(false);
    QJsonArray navigation;
    fit();
    for(double factor:{1.,20.,.2,40.}){
        fit();view_->scale(factor,factor);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);render();
        const auto transformBefore=view_->transform();const auto plateBefore=result();
        const auto anchor=viewport->rect().center();const auto point=view_->mapToScene(anchor);const auto start=view_->mapFromScene(point);
        auto mouse=[&](QEvent::Type type,QPoint pt,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,QPointF(pt),QPointF(viewport->mapToGlobal(pt)),button,buttons,Qt::NoModifier);QApplication::sendEvent(viewport,&event);};
        mouse(QEvent::MouseButtonPress,anchor,Qt::RightButton,Qt::RightButton);
        mouse(QEvent::MouseMove,anchor+QPoint(35,22),Qt::NoButton,Qt::RightButton);
        mouse(QEvent::MouseButtonRelease,anchor+QPoint(35,22),Qt::RightButton,Qt::NoButton);render();
        const auto moved=view_->mapFromScene(point)-start;
        navigation.append(QJsonObject{{"factor",factor},{"horizontal_hidden",!view_->horizontalScrollBar()->isVisibleTo(view_)},
            {"vertical_hidden",!view_->verticalScrollBar()->isVisibleTo(view_)},{"horizontal_policy_off",view_->horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff},
            {"vertical_policy_off",view_->verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff},{"dx",moved.x()},{"dy",moved.y()},
            {"zoom_unchanged_by_pan",std::abs(view_->transform().m11()-transformBefore.m11())<1e-10},
            {"model_unchanged_by_pan",result()==plateBefore},{"viewport_width",viewport->width()},{"view_width",view_->contentsRect().width()},
            {"viewport_height",viewport->height()},{"view_height",view_->contentsRect().height()}});
    }
    fit();
    QJsonArray cameras;
    for(bool wheel:{false,true}){
        fit();render();QJsonArray frames;const QPoint anchor=viewport->rect().center();
        auto mouse=[&](QEvent::Type type,QPoint pt,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,QPointF(pt),QPointF(viewport->mapToGlobal(pt)),button,buttons,Qt::NoModifier);QApplication::sendEvent(viewport,&event);};
        if(!wheel)mouse(QEvent::MouseButtonPress,anchor,Qt::RightButton,Qt::RightButton);
        for(int i=0;i<12;++i){timer.restart();
            if(wheel){QWheelEvent event(QPointF(anchor),QPointF(viewport->mapToGlobal(anchor)),QPoint{},QPoint(0,i<6?120:-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(viewport,&event);}
            else mouse(QEvent::MouseMove,anchor+QPoint((i<6?i+1:11-i)*3,2),Qt::NoButton,Qt::RightButton);
            render();frames.append(timer.nsecsElapsed()/1e6);
        }
        if(!wheel)mouse(QEvent::MouseButtonRelease,anchor,Qt::RightButton,Qt::NoButton);
        cameras.append(QJsonObject{{"kind",wheel?"wheel":"pan"},{"frames_ms",frames}});
    }
    fit();
    QTemporaryDir images;const auto a=images.filePath("cached.png"),b=images.filePath("uncached.png");
    const auto firstError=exportFile(a,"png");
    qint64 interiorPixels=0,missingPixels=0;
    for(double factor:{.3,1.,6.,30.,1.}){
        fit();view_->scale(factor,factor);view_->translate(25,-15);render();const QImage cachedScreen=image.copy();
        {UncachedPlateRender reference(items_);render();}
        for(int y=1;y+1<image.height();++y)for(int x=1;x+1<image.width();++x){
            const auto color=image.pixel(x,y);if(color!=data_->underwaterColor.rgb()&&color!=item->plate.color.rgb()&&color!=qRgb(255,255,255))continue;
            bool interior=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)interior&=image.pixel(x+dx,y+dy)==color;
            if(interior){++interiorPixels;if(cachedScreen.pixel(x,y)!=color)++missingPixels;}
        }
    }
    fit();
    QString secondError;{UncachedPlateRender reference(items_);secondError=exportFile(b,"png");}
    const bool rasterEqual=firstError.isEmpty()&&secondError.isEmpty()&&QImage(a)==QImage(b);
    return {{"canvas_navigation",navigation},{"screen_interior_pixels",interiorPixels},{"screen_missing_pixels",missingPixels},{"png_cache_and_camera_independent",rasterEqual},{"first_render_ms",first},{"modes",modes},{"faces",item->plate.faces.size()},{"viewport_width",viewport->width()},{"viewport_height",viewport->height()},
        {"camera_modes",cameras},{"before_layout",before["layout"]},{"after_layout",result()["layout"]},{"data_restored",result()==before},{"dxf_restored",data_->dxf({true,true,true,true})==dxf},{"text_restored",data_->text()==text}};
}
void PlatesDialog::chooseExport(const QString& format){if(!data_)return;auto path=QFileDialog::getSaveFileName(this,uiText("保存展开图"),preferenceFileLocation("export",QString(uiText("展开图.%1")).arg(format)),QString("%1 (*.%1)").arg(format));if(path.isEmpty())return;auto error=exportFile(path,format);status_->setMessage(error.isEmpty()?uiText("已保存到：")+path:error,error.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);}
QString PlatesDialog::exportFile(const QString& path,const QString& format){
    try{if(!data_)throw std::runtime_error(uiText("尚未生成展开图。"));bool visible=false;for(const auto&p:data_->plates)visible|=p.visible;if(!visible)throw std::runtime_error(uiText("没有可见的板片可供导出。"));
        if(!QStringList{"dxf","txt","json","png","pdf"}.contains(format))throw std::runtime_error(uiText("不支持的导出格式。"));
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return file.errorString();
        if(format=="pdf"){QPdfWriter pdf(&file);pdf.setPageSize(QPageSize(QPageSize::A4));pdf.setPageOrientation(QPageLayout::Landscape);pdf.setResolution(144);renderPages(pdf);}
        else if(format=="png"){UncachedPlateRender vectorRender(items_);QImage image(2400,1600,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);scene_->clearSelection();QRectF bounds;for(auto*i:items_)if(i->isVisible())bounds=bounds.united(i->sceneBoundingRect());scene_->render(&painter,QRectF(40,40,2320,1520),bounds,Qt::KeepAspectRatio);painter.end();select(list_->currentRow());if(!image.save(&file,"PNG"))throw std::runtime_error(uiText("图片写入失败。"));}
        else{std::array<bool,4> sections;for(int i=0;i<4;++i)sections[i]=sections_[i]->isChecked();auto bytes=format=="dxf"?data_->dxf(sections):format=="txt"?data_->text():QJsonDocument(data_->json()).toJson();if(file.write(bytes)!=bytes.size())return file.errorString();}
        if(!file.commit())return file.errorString();
        return {};
    }catch(const std::exception&e){return QString::fromUtf8(e.what());}
}
void PlatesDialog::renderPages(QPagedPaintDevice& device){
    if(!data_)return;
    UncachedPlateRender vectorRender(items_);
    QRectF bounds;for(auto*i:items_)if(i->isVisible())bounds=bounds.united(i->sceneBoundingRect());if(bounds.isEmpty())throw std::runtime_error(uiText("没有可见板片。"));
    QRect page=device.pageLayout().paintRectPixels(device.logicalDpiX());
    QPainter painter;if(!painter.begin(&device))throw std::runtime_error(uiText("无法创建打印页面。"));
    QFont printFont=studioTextFont(QFont("Microsoft YaHei UI",10));painter.setFont(printFont);
    const qreal dpiScale=device.logicalDpiX()/96.;
    const QString warning=uiText("展开图不包含材料成形余量；请检查边长和面积误差。");
    const qreal headerHeight=QFontMetricsF(printFont,&device).height()+16*dpiScale;
    const qreal footerHeight=studioMessageHeight(painter,warning,page.width(),dpiScale);
    QRectF target(0,headerHeight,page.width(),qMax(1.,page.height()-headerHeight-footerHeight-12*dpiScale));
    double pixelsPerUnit=scale_->value()>0?(data_->units==0?1000:304.8)*device.logicalDpiX()/25.4/scale_->value():qMin(target.width()/bounds.width(),target.height()/bounds.height());
    const double nx=scale_->value()==0?1:qMax(1.0,std::ceil(bounds.width()*pixelsPerUnit/target.width()));
    const double ny=scale_->value()==0?1:qMax(1.0,std::ceil(bounds.height()*pixelsPerUnit/target.height()));
    if(!std::isfinite(nx*ny)||nx*ny>500)throw std::runtime_error(uiText("当前比例超过 500 页，请调小比例。"));
    const int columns=int(nx),rows=int(ny);
    scene_->clearSelection();
    for(int row=0;row<rows;++row)for(int col=0;col<columns;++col){if((row||col)&&!device.newPage())throw std::runtime_error(uiText("无法创建下一页。"));
        painter.drawText(QRectF(0,0,page.width(),headerHeight),Qt::AlignLeft|Qt::AlignVCenter,QString(uiText("FreeShip 26 板材展开 · %1 · %2 / %3%4")).arg(data_->lengthUnit()).arg(row*columns+col+1).arg(rows*columns).arg(scale_->value()>0?QString(" · 1:%1").arg(scale_->value()):uiText(" · 适合单页")));
        QRectF source(bounds.left()+col*target.width()/pixelsPerUnit,bounds.top()+row*target.height()/pixelsPerUnit,target.width()/pixelsPerUnit,target.height()/pixelsPerUnit);
        scene_->render(&painter,target,source,Qt::IgnoreAspectRatio);
        paintStudioMessage(painter,QRectF(0,page.height()-footerHeight,page.width(),footerHeight),warning,StudioMessageRole::Warning,dpiScale);
    }painter.end();select(list_->currentRow());
}
