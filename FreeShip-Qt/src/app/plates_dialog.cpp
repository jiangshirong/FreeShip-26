#include "preferences_store.h"
#include "plates_dialog.h"
#include "model_session.h"
#include "studio_ui.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <cmath>
#include <stdexcept>

namespace {
QPointF plateScreen(QPointF p){return {p.x(),-p.y()};}
class PlateView:public QGraphicsView {
    QPoint last_;bool pan_=false;
public:
    using QGraphicsView::QGraphicsView;
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
    fs::DevelopedPlate& plate;PlateStyle style;QPainterPath fill,under;QVector<QLineF> boundary,inner,positive,negative;
    QColor underwaterColor;
    std::function<void()> changed,selected;
    PlateItem(fs::DevelopedPlate& p,double waterline,QColor waterColor):plate(p),underwaterColor(waterColor){
        setFlags(ItemIsMovable|ItemIsSelectable|ItemSendsGeometryChanges);
        for(const auto& face:p.faces){QPolygonF poly;for(int i:face)poly<<plateScreen(p.points[i]);fill.addPolygon(poly);fill.closeSubpath();
            // Clip each original face at the design waterline in its affine
            // unfolded coordinates. This is display shading only.
            for(int t=2;t<face.size();++t){QVector<QPair<QPointF,double>> polygon;
                for(int i:{face[0],face[t-1],face[t]})polygon.append({p.points[i],p.original[i].z()-waterline});
                QPolygonF clipped;for(int j=0;j<polygon.size();++j){auto a=polygon[j],b=polygon[(j+1)%polygon.size()];
                    if(a.second<=0)clipped<<plateScreen(a.first);
                    if((a.second<0&&b.second>0)||(a.second>0&&b.second<0))clipped<<plateScreen(a.first+(b.first-a.first)*(a.second/(a.second-b.second)));}
                if(clipped.size()>=3){under.addPolygon(clipped);under.closeSubpath();}
            }
        }
        fill.setFillRule(Qt::WindingFill);under.setFillRule(Qt::WindingFill);
        for(const auto&e:p.edges){QLineF line(plateScreen(p.points[e.a]),plateScreen(p.points[e.b]));(e.boundary?boundary:inner).append(line);
            if(e.error>1e-7)positive.append(line);else if(e.error< -1e-7)negative.append(line);}
    }
    QRectF boundingRect()const override{return fill.boundingRect();}
    QPainterPath shape()const override{return fill;}
    void paint(QPainter* painter,const QStyleOptionGraphicsItem*,QWidget*)override{
        painter->setRenderHint(QPainter::Antialiasing);painter->setPen(Qt::NoPen);
        if(style.fill){painter->fillPath(fill,plate.color);if(style.submerged)painter->fillPath(under,underwaterColor);}
        auto lines=[&](const QVector<QLineF>& ls,QColor c,double width){QPen pen(c);pen.setWidthF(width);pen.setCosmetic(true);painter->setPen(pen);painter->drawLines(ls);};
        if(style.interior)lines(inner,QColor(174,183,194),.7);
        lines(boundary,isSelected()?QColor(0,117,190):QColor(39,55,73),isSelected()?2:1);
        if(style.errors){lines(positive,QColor(216,57,44),1.4);lines(negative,QColor(20,100,202),1.4);}
        for(const auto& c:plate.curves)if(style.sections[c.kind-1]){QPolygonF poly;for(auto pt:c.points)poly<<plateScreen(pt);QPen pen(c.color);pen.setCosmetic(true);painter->setPen(pen);painter->drawPolyline(poly);}
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
    setWindowTitle("板材展开");resize(1220,790);if(hidden)setAttribute(Qt::WA_DontShowOnScreen);
    auto* layout=new QVBoxLayout(this);auto* tools=new QHBoxLayout;layout->addLayout(tools);
    auto button=[&](QString text,QString op,std::function<void()> fn){auto*b=new QPushButton(text,this);b->setProperty("plates_action",op);connect(b,&QPushButton::clicked,this,fn);tools->addWidget(b);return b;};
    calculate_=button("生成展开图","calculate",[this]{failure_.clear();status_->setText("正在生成展开图…");session_.developPlates();refreshState();});
    cancel_=button("取消计算","cancel",[this]{session_.cancelPlates();});
    button("适合窗口","fit",[this]{fit();});button("重新排列","arrange",[this]{if(data_){data_->arrange();updateItems();fit();select(list_->currentRow());}});
    for(const auto& pair:QVector<QPair<QString,QString>>{{"DXF","dxf"},{"坐标表","txt"},{"展开数据","json"},{"图片","png"},{"PDF","pdf"}})button(pair.first,pair.second,[this,pair]{chooseExport(pair.second);});
    button("打印预览","print",[this]{if(!data_)return;QPrinter printer(QPrinter::HighResolution);printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageOrientation(QPageLayout::Landscape);
        QPrintPreviewDialog preview(&printer,this);connect(&preview,&QPrintPreviewDialog::paintRequested,this,[this](QPrinter*p){try{renderPages(*p);}catch(const std::exception&e){showFailure(QString::fromUtf8(e.what()));}});preview.exec();});
    tools->addStretch();auto* splitter=new QSplitter(this);layout->addWidget(splitter,1);
    auto* sidebar=new QWidget;auto* side=new QVBoxLayout(sidebar);list_=new QListWidget;list_->setMinimumHeight(110);side->addWidget(list_,1);
    auto* form=new QFormLayout;side->addLayout(form);
    auto spin=[&](QString text,QString field,double low,double high){auto*w=new QDoubleSpinBox;w->setRange(low,high);w->setDecimals(6);w->setProperty("plate_field",field);form->addRow(text,w);connect(w,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{transformActive();});return w;};
    rotation_=spin("旋转 / °","rotation",-36000,36000);x_=spin("水平位置","x",-1e9,1e9);y_=spin("垂直位置","y",-1e9,1e9);
    mirror_=new QCheckBox("镜像板片");mirror_->setProperty("plate_field","mirror");form->addRow(mirror_);connect(mirror_,&QCheckBox::toggled,this,[this]{transformActive();});
    auto* turns=new QHBoxLayout;side->addLayout(turns);for(int angle:{-90,-5,-1,1,5,90}){auto*b=new QPushButton(QString::number(angle)+"°");b->setProperty("plate_rotate",angle);b->setFixedWidth(38);turns->addWidget(b);connect(b,&QPushButton::clicked,this,[this,angle]{rotation_->setValue(rotation_->value()+angle);});}
    auto toggle=[&](QString text,QString field,bool checked){auto*c=new QCheckBox(text);c->setChecked(checked);c->setProperty("plate_display",field);side->addWidget(c);connect(c,&QCheckBox::toggled,this,[this]{updateItems();});return c;};
    fill_=toggle("填色","fill",true);submerged_=toggle("水下着色","submerged",true);interior_=toggle("内部边","interior",false);errors_=toggle("边长误差：红色伸长，蓝色缩短","errors",false);
    names_=toggle("板片名称","names",true);dimensions_=toggle("角点坐标（当前布局）","dimensions",false);boxes_=toggle("包围框","boxes",false);
    const QStringList labels{"横剖线","纵剖线","水线","斜剖线"};for(int i=0;i<4;++i)sections_[i]=toggle(labels[i],"section"+QString::number(i+1),true);
    auto* gridForm=new QFormLayout;side->addLayout(gridForm);
    for(int i=0;i<2;++i){auto*n=new QDoubleSpinBox;n->setRange(0,1e9);n->setDecimals(6);n->setSpecialValueText("不显示");n->setProperty("plate_field",i?"grid_y":"grid_x");gridForm->addRow(i?"网格间距 Y":"网格间距 X",n);if(i)gridY_=n;else gridX_=n;connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{updateItems();});}
    fontSize_=new QSpinBox;fontSize_->setRange(6,36);fontSize_->setValue(10);gridForm->addRow("标注字号",fontSize_);connect(fontSize_,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateItems();});
    auto* printForm=new QFormLayout;side->addLayout(printForm);scale_=new QDoubleSpinBox;scale_->setRange(0,100000);scale_->setDecimals(3);scale_->setSpecialValueText("适合单页");scale_->setValue(0);scale_->setProperty("plate_field","print_scale");printForm->addRow("打印比例 1 :",scale_);
    metrics_=new QLabel;metrics_->setWordWrap(true);metrics_->setTextInteractionFlags(Qt::TextSelectableByMouse);side->addWidget(metrics_);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(sidebar);scroll->setMinimumWidth(300);splitter->addWidget(scroll);
    scene_=new PlateScene(this);view_=new PlateView(scene_,this);view_->setTransformationAnchor(QGraphicsView::NoAnchor);view_->setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);splitter->addWidget(view_);splitter->setSizes({340,860});
    status_=new QLabel("仅展开已勾选“可展开”的图层。展开误差不等于材料成形余量。");status_->setWordWrap(true);layout->addWidget(status_);
    connect(list_,&QListWidget::currentRowChanged,this,&PlatesDialog::select);
    connect(list_,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){if(updating_||!data_)return;data_->plates[list_->row(item)].visible=item->checkState()==Qt::Checked;updateItems();});
    refreshState();
}
void PlatesDialog::reject(){if(session_.busy()){session_.cancelPlates();status_->setText("正在取消，请稍候…");return;}QDialog::reject();}
void PlatesDialog::refreshState(){calculate_->setEnabled(session_.ready()&&!session_.busy());cancel_->setEnabled(session_.busy());}
void PlatesDialog::showFailure(const QString& text){failure_=text;status_->setText(text);refreshState();}
void PlatesDialog::receive(const QJsonObject& object){
    try{auto next=std::make_unique<fs::DevelopedPlates>(fs::DevelopedPlates::parse(object));scene_->clear();items_.clear();data_=std::move(next);buildScene();failure_.clear();status_->setText(QString("已展开 %1 块板片；左键拖动板片，右键平移，滚轮缩放。边长误差为展开长度减原长度。").arg(data_->plates.size()));refreshState();}
    catch(const std::exception&e){showFailure(QString::fromUtf8(e.what()));}
}
void PlatesDialog::buildScene(){
    updating_=true;list_->clear();submerged_->setChecked(data_->shadeSubmerged);
    for(int i=0;i<data_->plates.size();++i){auto&p=data_->plates[i];auto* row=new QListWidgetItem(p.name,list_);row->setFlags(row->flags()|Qt::ItemIsUserCheckable);row->setCheckState(Qt::Checked);
        auto* item=new PlateItem(p,data_->waterline,data_->underwaterColor);scene_->addItem(item);items_.append(item);
        item->selected=[this,i]{if(!updating_)list_->setCurrentRow(i);};
        item->changed=[this,i]{if(!updating_&&list_->currentRow()==i){select(i);if(dimensions_->isChecked())updateItems();}};}
    for(auto*n:{x_,y_,gridX_,gridY_})n->setSuffix(" "+data_->lengthUnit());
    updating_=false;updateItems();list_->setCurrentRow(0);fit();
}
void PlatesDialog::select(int row){
    if(!data_||row<0||row>=data_->plates.size())return;
    updating_=true;auto&p=data_->plates[row];
    rotation_->setValue(p.rotation);x_->setValue(p.offset.x());y_->setValue(p.offset.y());mirror_->setChecked(p.mirrored);
    for(int i=0;i<items_.size();++i)items_[i]->setSelected(i==row);
    const auto m=p.metrics;double multiplier=p.joinedMirror?2:1;QString unit=data_->lengthUnit();
    metrics_->setText(QString("单位：%1\n边长误差：%2 ～ %3 %1\n面积差（有符号）：%4 %1²\n面积差绝对值之和：%5 %1²\n原表面积：%6 %1²\n无量纲误差：%7\n%8")
        .arg(unit).arg(m["min_edge_error"].toDouble(),0,'g',8).arg(m["max_edge_error"].toDouble(),0,'g',8)
        .arg(multiplier*m["signed_area_error"].toDouble(),0,'g',8).arg(multiplier*m["absolute_area_error"].toDouble(),0,'g',8)
        .arg(multiplier*m["reference_area"].toDouble(),0,'g',8).arg(m["normalized_error"].toDouble(),0,'g',8)
        .arg(p.joinedMirror?"本板含对称的两半；DXF 分别保留两侧闭合轮廓。":"面积误差不会抵消为零。"));updating_=false;
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
            l->setBrush(background.lightness()<128?QColor(Qt::white):QColor(44,58,73));l->setPos(pt);};
        if(names_->isChecked())label(p.name,c);
        if(dimensions_->isChecked())for(int k:p.corners){auto v=p.placement().map(p.points[k]);label(QString("%1, %2").arg(v.x(),0,'g',6).arg(v.y(),0,'g',6),plateScreen(p.points[k]));}
        item->update();}
    static_cast<PlateScene*>(scene_)->gridX=gridX_->value();static_cast<PlateScene*>(scene_)->gridY=gridY_->value();scene_->invalidate(scene_->sceneRect(),QGraphicsScene::BackgroundLayer);
    updating_=false;
}
void PlatesDialog::fit(){if(!data_)return;QRectF rect;for(auto*i:items_)if(i->isVisible())rect=rect.united(i->sceneBoundingRect());if(rect.isEmpty())return;
    const auto d=qMax(rect.width(),rect.height())*.04;scene_->setSceneRect(rect.adjusted(-d,-d,d,d));view_->fitInView(scene_->sceneRect(),Qt::KeepAspectRatio);}
QJsonObject PlatesDialog::result()const{return data_?data_->json():QJsonObject{};}
void PlatesDialog::chooseExport(const QString& format){if(!data_)return;auto path=QFileDialog::getSaveFileName(this,"保存展开图",preferenceFileLocation("export",QString("展开图.%1").arg(format)),QString("%1 (*.%1)").arg(format));if(path.isEmpty())return;auto error=exportFile(path,format);status_->setText(error.isEmpty()?"已保存到："+path:error);}
QString PlatesDialog::exportFile(const QString& path,const QString& format){
    try{if(!data_)throw std::runtime_error("尚未生成展开图。");bool visible=false;for(const auto&p:data_->plates)visible|=p.visible;if(!visible)throw std::runtime_error("没有可见的板片可供导出。");
        if(!QStringList{"dxf","txt","json","png","pdf"}.contains(format))throw std::runtime_error("不支持的导出格式。");
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return file.errorString();
        if(format=="pdf"){QPdfWriter pdf(&file);pdf.setPageSize(QPageSize(QPageSize::A4));pdf.setPageOrientation(QPageLayout::Landscape);pdf.setResolution(144);renderPages(pdf);}
        else if(format=="png"){QImage image(2400,1600,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);scene_->clearSelection();QRectF bounds;for(auto*i:items_)if(i->isVisible())bounds=bounds.united(i->sceneBoundingRect());scene_->render(&painter,QRectF(40,40,2320,1520),bounds,Qt::KeepAspectRatio);painter.end();select(list_->currentRow());if(!image.save(&file,"PNG"))throw std::runtime_error("图片写入失败。");}
        else{std::array<bool,4> sections;for(int i=0;i<4;++i)sections[i]=sections_[i]->isChecked();auto bytes=format=="dxf"?data_->dxf(sections):format=="txt"?data_->text():QJsonDocument(data_->json()).toJson();if(file.write(bytes)!=bytes.size())return file.errorString();}
        if(!file.commit())return file.errorString();
        return {};
    }catch(const std::exception&e){return QString::fromUtf8(e.what());}
}
void PlatesDialog::renderPages(QPagedPaintDevice& device){
    if(!data_)return;
    QRectF bounds;for(auto*i:items_)if(i->isVisible())bounds=bounds.united(i->sceneBoundingRect());if(bounds.isEmpty())throw std::runtime_error("没有可见板片。");
    QRect page=device.pageLayout().paintRectPixels(device.logicalDpiX());QRectF target(0,45,page.width(),page.height()-80);
    double pixelsPerUnit=scale_->value()>0?(data_->units==0?1000:304.8)*device.logicalDpiX()/25.4/scale_->value():qMin(target.width()/bounds.width(),target.height()/bounds.height());
    const double nx=scale_->value()==0?1:qMax(1.0,std::ceil(bounds.width()*pixelsPerUnit/target.width()));
    const double ny=scale_->value()==0?1:qMax(1.0,std::ceil(bounds.height()*pixelsPerUnit/target.height()));
    if(!std::isfinite(nx*ny)||nx*ny>500)throw std::runtime_error("当前比例超过 500 页，请调小比例。");
    const int columns=int(nx),rows=int(ny);
    QPainter painter;if(!painter.begin(&device))throw std::runtime_error("无法创建打印页面。");scene_->clearSelection();
    for(int row=0;row<rows;++row)for(int col=0;col<columns;++col){if((row||col)&&!device.newPage())throw std::runtime_error("无法创建下一页。");
        painter.drawText(QRectF(0,0,page.width(),40),Qt::AlignLeft|Qt::AlignVCenter,QString("FreeShip_26 板材展开 · %1 · %2 / %3%4").arg(data_->lengthUnit()).arg(row*columns+col+1).arg(rows*columns).arg(scale_->value()>0?QString(" · 1:%1").arg(scale_->value()):" · 适合单页"));
        QRectF source(bounds.left()+col*target.width()/pixelsPerUnit,bounds.top()+row*target.height()/pixelsPerUnit,target.width()/pixelsPerUnit,target.height()/pixelsPerUnit);
        scene_->render(&painter,target,source,Qt::IgnoreAspectRatio);
        painter.drawText(QRectF(0,page.height()-30,page.width(),30),Qt::AlignLeft|Qt::AlignVCenter,"展开图不包含材料成形余量；请检查边长和面积误差。");
    }painter.end();select(list_->currentRow());
}
