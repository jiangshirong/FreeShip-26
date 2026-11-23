#include "app/ui_language.h"
#include "preferences_store.h"
#include "plates_dialog.h"
#include "pdf_scale_inputs.h"
#include "pdf_export_dialog.h"
#include "plate_assembly.h"
#include "table_number.h"
#include "text_style.h"
#include "model_session.h"
#include "studio_ui.h"
#include "studio_read_ui.h"
#include "render/winding_contours.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "feedback_text.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QBuffer>
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
#include <QStyle>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QApplication>
#include <QJsonArray>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <limits>
#include <QtMath>

namespace {
QPointF plateScreen(QPointF p){return {p.x(),-p.y()};}
class PlateView:public QGraphicsView {
    QPoint last_,press_;bool pan_=false,moved_=false;
    std::optional<QPointF> pivot_;
    QPointF previousArm_;
    void turnTo(QPoint pos){
        const auto arm=QPointF(pos)-viewportTransform().map(*pivot_);
        if(QLineF({},arm).length()<4){previousArm_={};return;}
        if(!previousArm_.isNull()){
            const double cross=previousArm_.y()*arm.x()-previousArm_.x()*arm.y();
            rotateBy(qRadiansToDegrees(std::atan2(cross,QPointF::dotProduct(previousArm_,arm))));
        }
        previousArm_=arm;
    }
    PlateEdgeHit pressedEdge_;
public:
    std::function<PlateEdgeHit(QPoint)> hitTest;
    std::function<void(PlateEdgeHit)> hover,picked;
    std::function<void(QPainter*)> overlay;
    std::function<std::optional<QPointF>(QPoint)> rotationCenter;
    std::function<void(double)> rotateBy;
    std::function<void(QPoint,QPoint)> plateMenu;
    void cancelRotation(){pivot_.reset();}
    explicit PlateView(QGraphicsScene* scene,QWidget* parent):QGraphicsView(scene,parent){
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setProperty("studio_canvas_kind","plates");
        setMouseTracking(true);
    }
protected:
    void drawForeground(QPainter* painter,const QRectF& rect)override{
        QGraphicsView::drawForeground(painter,rect);
        if(overlay)overlay(painter);
    }
    void wheelEvent(QWheelEvent* e)override{
        const auto before=mapToScene(e->position().toPoint());double factor=std::pow(1.0015,e->angleDelta().y());
        double next=transform().m11()*factor;if(next>1e-6&&next<1e7){scale(factor,factor);const auto after=mapToScene(e->position().toPoint());translate(after.x()-before.x(),after.y()-before.y());hover(hitTest(e->position().toPoint()));}e->accept();
    }
    void leaveEvent(QEvent* e)override{if(hover)hover({});QGraphicsView::leaveEvent(e);}
    void mousePressEvent(QMouseEvent* e)override{
        press_=e->pos();moved_=false;
        if(e->button()==Qt::RightButton){pan_=true;last_=press_;hover({});setCursor(Qt::ClosedHandCursor);e->accept();return;}
        if(e->button()==Qt::LeftButton){
            if(e->modifiers().testFlag(Qt::ControlModifier)){
                pivot_=rotationCenter(press_);
                if(pivot_){previousArm_={};turnTo(press_);hover({});e->accept();return;}
            }
            pressedEdge_=hitTest(press_);
            if(pressedEdge_.valid()){hover(pressedEdge_);e->accept();return;}
            picked({});hover({});
        }
        QGraphicsView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e)override{
        if(pivot_&&e->buttons().testFlag(Qt::LeftButton)){turnTo(e->pos());e->accept();return;}
        if(e->buttons()&&(e->pos()-press_).manhattanLength()>=QApplication::startDragDistance())moved_=true;
        if(pan_){if(!moved_){e->accept();return;}auto delta=mapToScene(e->pos())-mapToScene(last_);translate(delta.x(),delta.y());last_=e->pos();e->accept();return;}
        if(pressedEdge_.valid()){if(moved_)hover({});e->accept();return;}
        if(!e->buttons())hover(hitTest(e->pos()));
        QGraphicsView::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e)override{
        if(e->button()==Qt::LeftButton&&pivot_){turnTo(e->pos());cancelRotation();hover(hitTest(e->pos()));e->accept();return;}
        if(e->button()==Qt::RightButton&&pan_){
            pan_=false;unsetCursor();hover(hitTest(e->pos()));
            if(!moved_&&(e->pos()-press_).manhattanLength()<QApplication::startDragDistance()&&plateMenu)
                plateMenu(e->pos(),e->globalPosition().toPoint());
            e->accept();return;
        }
        if(e->button()==Qt::LeftButton&&pressedEdge_.valid()){
            const auto hit=hitTest(e->pos());
            if(!moved_&&(e->pos()-press_).manhattanLength()<QApplication::startDragDistance()&&pressedEdge_.sameStroke(hit))picked(hit);
            pressedEdge_={};hover(hit);e->accept();return;
        }
        QGraphicsView::mouseReleaseEvent(e);
    }
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
void paintPlateBackground(QPainter& painter,const QRectF& rect,double gridX,double gridY){
    painter.fillRect(rect,Qt::white);QPen pen(QColor(220,225,232));pen.setCosmetic(true);painter.setPen(pen);
    if(gridX>0&&rect.width()/gridX<500){double first=std::ceil(rect.left()/gridX)*gridX;for(int i=0;i<501;++i){double x=first+i*gridX;if(x>rect.right())break;painter.drawLine(QPointF(x,rect.top()),QPointF(x,rect.bottom()));}}
    if(gridY>0&&rect.height()/gridY<500){double first=std::ceil(rect.top()/gridY)*gridY;for(int i=0;i<501;++i){double y=first+i*gridY;if(y>rect.bottom())break;painter.drawLine(QPointF(rect.left(),y),QPointF(rect.right(),y));}}
}
class PlateScene:public QGraphicsScene {
public:
    using QGraphicsScene::QGraphicsScene;
    double gridX=0,gridY=0;
    bool recordGeometry=false;
protected:
    void drawBackground(QPainter* painter,const QRectF& rect)override{
        if(!recordGeometry)paintPlateBackground(*painter,rect,gridX,gridY);
    }
};
class PlateItem:public QGraphicsItem {
public:
    quint64 paintCalls=0;
    fs::DevelopedPlate& plate;PlateStyle style;QPainterPath fill,under,screenFill,screenUnder;QVector<QLineF> boundary,inner,positive,negative;
    QColor underwaterColor;QVector<fs::PlateCurve> screenCurves;
    QVector<int> boundaryIndices;
    QHash<int,int> seamByEdge;
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
        for(int i=0;i<p.edges.size();++i){const auto& e=p.edges[i];if(e.boundary)boundaryIndices.append(i);QLineF line(plateScreen(p.points[e.a]),plateScreen(p.points[e.b]));(e.boundary?boundary:inner).append(line);
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
    for(const auto& pair:QVector<QPair<QString,QString>>{{"展开图","dxf"},{"坐标表","txt"},{"展开数据","json"},{"图片","png"},{"展开图","pdf"}}){
        auto* control=button(uiText(pair.first),pair.second,[this,format=pair.second]{chooseExport(format);});
        const auto refreshFormat=[control,pair]{setStudioFormatButton(control,pair.second,uiText("导出 ")+uiText(pair.first)+" ("+pair.second.toUpper()+")");};
        refreshFormat();bindUiRefresh(control,refreshFormat);
    }
    tools->addStretch();
    rotationPresets_=new QWidget(this);
    auto* presetLayout=new QHBoxLayout(rotationPresets_);presetLayout->setContentsMargins(0,0,0,0);
    for(int angle:{-90,-5,-1,1,5,90}){
        auto* preset=new QPushButton(QString::number(angle)+QChar(0x00b0),rotationPresets_);
        preset->setFixedWidth(preset->fontMetrics().horizontalAdvance(QStringLiteral("-90°"))+20);
        preset->setAutoDefault(false);preset->setProperty("plate_angle",angle);
        presetLayout->addWidget(preset);
        connect(preset,&QPushButton::clicked,this,[this,angle]{
            const int row=list_->currentRow();
            if(!data_||row<0||session_.busy()||!data_->plates[row].visible)return;
            data_->plates[row].rotation=angle;placePlate(row);
        });
    }
    tools->addWidget(rotationPresets_);rotationPresets_->hide();tools->addStretch();
    connect(this,&QDialog::finished,rotationPresets_,&QWidget::hide);
    auto* splitter=new QSplitter(this);layout->addWidget(splitter,1);
    auto* sidebar=new QWidget;auto* side=new QVBoxLayout(sidebar);list_=new QListWidget;list_->setMinimumHeight(110);side->addWidget(list_,1);
    auto toggle=[&](QString text,QString field,bool checked){auto*c=new QCheckBox(text);c->setChecked(checked);c->setProperty("plate_display",field);side->addWidget(c);connect(c,&QCheckBox::toggled,this,[this]{updateItems();});return c;};
    fill_=toggle(uiText("填色"),"fill",true);submerged_=toggle(uiText("水下着色"),"submerged",true);interior_=toggle(uiText("内部边"),"interior",false);errors_=toggle(uiText("边长误差：红色伸长，蓝色缩短"),"errors",false);
    names_=toggle(uiText("板片名称"),"names",true);dimensions_=toggle(uiText("角点坐标（当前布局）"),"dimensions",false);boxes_=toggle(uiText("包围框"),"boxes",false);
    const QStringList labels{uiText("横剖线"),uiText("纵剖线"),uiText("水线"),uiText("斜剖线")};for(int i=0;i<4;++i)sections_[i]=toggle(labels[i],"section"+QString::number(i+1),true);
    auto* gridForm=new QFormLayout;side->addLayout(gridForm);
    for(int i=0;i<2;++i){auto*n=new QDoubleSpinBox;n->setRange(0,1e9);n->setDecimals(6);n->setSpecialValueText(uiText("不显示"));n->setProperty("plate_field",i?"grid_y":"grid_x");gridForm->addRow(i?uiText("网格间距 Y"):uiText("网格间距 X"),n);if(i)gridY_=n;else gridX_=n;connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{updateItems();});}
    fontSize_=new QSpinBox;fontSize_->setRange(6,36);fontSize_->setValue(10);gridForm->addRow(uiText("标注字号"),fontSize_);connect(fontSize_,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateItems();});
    metrics_=new QTableWidget(7,3);metrics_->setObjectName("PlateMetrics");metrics_->setProperty("studio_content_role","result_data");
    const auto refreshHeaders=[this]{metrics_->setHorizontalHeaderLabels({uiText("参数"),uiText("数值"),uiText("单位")});};
    refreshHeaders();bindUiRefresh(metrics_,refreshHeaders);metrics_->verticalHeader()->hide();
    metrics_->setEditTriggers(QAbstractItemView::NoEditTriggers);metrics_->setSelectionMode(QAbstractItemView::NoSelection);
    metrics_->setItemDelegate(new StudioWidgetCellDelegate(metrics_));metrics_->setAlternatingRowColors(false);metrics_->setShowGrid(false);
    metrics_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);metrics_->horizontalHeader()->setStretchLastSection(false);
    metrics_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);metrics_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    metrics_->setStyleSheet("QTableWidget#PlateMetrics{background:#ffffff;border:1px solid #d8e2dc;border-radius:6px;color:#24463d} QTableWidget#PlateMetrics::item{border:0;border-bottom:1px solid #edf1ee;padding:5px 7px;} QTableWidget#PlateMetrics QHeaderView::section{background:#eef3f0;border:0;padding:6px;color:#365e50;}");
    metrics_->hide();side->addWidget(metrics_);
    diagnosticNote_=new StudioMessage(StudioMessageRole::Warning);diagnosticNote_->hide();side->addWidget(diagnosticNote_);
    plateNote_=new StudioMessage(StudioMessageRole::Explanation,uiText("仅展开已勾选“可展开曲面”的图层。展开误差不等于材料成形余量。"));side->addWidget(plateNote_);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(sidebar);scroll->setMinimumWidth(360);splitter->addWidget(scroll);
    scene_=new PlateScene(this);view_=new PlateView(scene_,this);view_->setTransformationAnchor(QGraphicsView::NoAnchor);view_->setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);splitter->addWidget(view_);splitter->setSizes({400,900});
    status_=new StudioMessage(StudioMessageRole::Feedback);status_->enableMathNotation();status_->setObjectName("PlateStatus");layout->addWidget(status_);
    auto* sheetView=static_cast<PlateView*>(view_);
    const auto openPlateMenu=[this](int row,QPoint global){
        if(!data_||session_.busy())return;
        if(row<0)row=list_->currentRow();
        if(row<0||row>=data_->plates.size()||!data_->plates[row].visible)return;
        list_->setCurrentRow(row);
        const QPersistentModelIndex target(list_->model()->index(row,0));
        auto* menu=new QMenu(view_);
        menu->setObjectName("PlateContextMenu");
        auto* mirror=menu->addAction(studioIcon("Mirror"),uiText("镜像翻转"));
        mirror->setProperty("plate_operation","mirror");
        connect(mirror,&QAction::triggered,this,[this,target]{
            const int row=target.row();
            if(!target.isValid()||!data_||session_.busy()||row<0||row>=data_->plates.size()||!data_->plates[row].visible)return;
            auto& plate=data_->plates[row];
            plate.mirrored=!plate.mirrored;placePlate(row);
        });
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        menu->popup(global);
    };
    sheetView->plateMenu=[this,openPlateMenu](QPoint pos,QPoint global){openPlateMenu(plateAt(pos),global);};
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_,&QListWidget::customContextMenuRequested,this,[this,openPlateMenu](QPoint pos){
        openPlateMenu(list_->row(list_->itemAt(pos)),list_->viewport()->mapToGlobal(pos));
    });
    sheetView->rotationCenter=[this](QPoint pos)->std::optional<QPointF>{
        if(!data_||session_.busy())return {};
        const int row=plateAt(pos);
        if(row<0)return {};
        list_->setCurrentRow(row);
        return items_[row]->mapToScene(plateScreen(data_->plates[row].bounds.center()));
    };
    sheetView->rotateBy=[this](double degrees){
        const int row=list_->currentRow();
        if(!data_||row<0||degrees==0)return;
        data_->plates[row].rotation=std::remainder(data_->plates[row].rotation+degrees,360.);
        placePlate(row);
        rotationPresets_->show();
    };
    sheetView->hitTest=[this](QPoint pos){return edgeAt(pos);};
    sheetView->hover=[this](PlateEdgeHit hit){
        if(hovered_.sameStroke(hit))return;
        hovered_=hit;view_->viewport()->update();
    };
    sheetView->picked=[this](PlateEdgeHit hit){
        rotationPresets_->hide();
        if(!hit.valid()&&seam_<0&&!singleEdge_.valid())return;
        highlightSeam(hit.seam,hit.valid()&&hit.seam<0);
        if(hit.valid()&&hit.seam<0)singleEdge_=hit;
    };
    sheetView->overlay=[this](QPainter* painter){
        const int row=list_->currentRow();
        if(data_&&row>=0&&row<items_.size()&&items_[row]->isVisible()){
            const auto& plate=data_->plates[row];
            if(plate.metrics["max_face_area_error"].toDouble()>plate.metrics["reference_area"].toDouble()*1e-7){
                painter->save();
                QPen pen(QColor(189,100,11),2);pen.setCosmetic(true);
                painter->setPen(pen);painter->setBrush(QColor(246,155,41,75));
                for(int face:plate.worstFaces){
                    QPolygonF outline;
                    for(int vertex:plate.faces[face])outline.append(items_[row]->mapToScene(plateScreen(plate.points[vertex])));
                    painter->drawPolygon(outline);
                }
                painter->restore();
            }
        }
        if(seam_<0&&!hovered_.valid()&&!singleEdge_.valid())return;
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        auto draw=[&](PlateEdgeHit hit,QColor color,double width){
            const int index=hit.plate;
            auto* item=items_[index];if(!item->isVisible())return;
            const auto& p=data_->plates[index];QVector<QLineF> lines;
            auto append=[&](int edge){
                const auto& e=p.edges[edge];
                lines.append(QLineF(item->mapToScene(plateScreen(p.points[e.a])),item->mapToScene(plateScreen(p.points[e.b]))));
            };
            if(hit.seam<0)append(hit.edge);
            else {
                const auto& s=seams_[hit.seam];for(auto [a,b]:s.segments)append(index==s.left?a:b);
            }
            QPen pen(Qt::white,width+2,Qt::SolidLine,Qt::RoundCap);pen.setCosmetic(true);
            painter->setPen(pen);painter->drawLines(lines);
            pen.setColor(color);pen.setWidthF(width);
            painter->setPen(pen);painter->drawLines(lines);
        };
        if(seam_>=0){
            for(int plate:{seams_[seam_].left,seams_[seam_].right})
                draw({plate,-1,seam_},plateSeamColor,3);
        }
        if(singleEdge_.valid())draw(singleEdge_,plateSeamColor,3);
        if(hovered_.valid()){
            bool selected=(hovered_.seam>=0&&seam_==hovered_.seam)||hovered_.sameStroke(singleEdge_);
            draw(hovered_,selected?plateSeamColor:plateHoverColor,selected?4:2.5);
        }
        if(seam_>=0){
            const auto& seam=seams_[seam_];
            const auto sceneToView=painter->worldTransform();
            painter->resetTransform();
            const QRectF viewport=view_->viewport()->rect();
            QVector<QRectF> occupied;
            for(int plate:{seam.left,seam.right}){
                if(!items_[plate]->isVisible())continue;
                for(int end=0;end<seam.ends.size();++end){
                    const auto [left,right]=seam.ends[end];
                    const int vertex=plate==seam.left?left:right;
                    const auto local=plateScreen(data_->plates[plate].points[vertex]);
                    const auto anchor=sceneToView.map(items_[plate]->mapToScene(local));
                    if(!viewport.contains(anchor))continue;
                    painter->setBrush(plateSeamColor);
                    painter->setPen(QPen(Qt::white,1));
                    painter->drawEllipse(anchor,3.5,3.5);
                    const QString label=seam.endLabel(end);
                    const QFontMetricsF metrics(painter->font());
                    const QSizeF size(metrics.horizontalAdvance(label)+8,metrics.height()+2);
                    auto freeSpace=[&](const QRectF& box){
                        const auto margin=box.adjusted(-2,-2,2,2);
                        if(!viewport.contains(margin))return false;
                        for(const auto& used:occupied)if(used.intersects(margin))return false;
                        for(auto* entry:items_){
                            const auto* item=static_cast<PlateItem*>(entry);
                            if(!item->isVisible())continue;
                            const auto t=item->deviceTransform(sceneToView);
                            if(!t.mapRect(item->boundingRect()).intersects(margin))continue;
                            QPainterPath rect;rect.addRect(margin);
                            if(item->screenFill.intersects(t.inverted().map(rect)))return false;
                        }
                        return true;
                    };
                    QRectF chosen;
                    // Try nearby white space first, then the nearest free cell on the sheet.
                    for(double gap:{6.,14.,24.,40.,64.}){
                        for(const QPointF dir:{QPointF(0,-1),{0,1},{-1,0},{1,0},{-1,-1},{1,-1},{-1,1},{1,1}}){
                            QRectF box({},size);
                            box.moveCenter(anchor+QPointF(dir.x()*(gap+size.width()/2),dir.y()*(gap+size.height()/2)));
                            if(freeSpace(box)){chosen=box;break;}
                        }
                        if(!chosen.isEmpty())break;
                    }
                    if(chosen.isEmpty()){
                        double nearest=std::numeric_limits<double>::infinity();
                        for(double y=3;y+size.height()+2<viewport.height();y+=size.height()+2)
                            for(double x=3;x+size.width()+2<viewport.width();x+=size.width()+2){
                                const QRectF box(QPointF(x,y),size);
                                const double distance=QLineF(anchor,box.center()).length();
                                if(distance<nearest&&freeSpace(box)){nearest=distance;chosen=box;}
                            }
                    }
                    if(chosen.isEmpty())continue;
                    occupied.append(chosen);
                    painter->setPen(QPen(plateSeamColor,1));
                    painter->drawLine(anchor,chosen.center());
                    painter->setBrush(Qt::white);
                    painter->drawRoundedRect(chosen,3,3);
                    painter->drawText(chosen,Qt::AlignCenter,label);
                }
            }
        }
        painter->restore();
    };
    connect(list_,&QListWidget::currentRowChanged,this,&PlatesDialog::select);
    connect(list_,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){if(updating_||!data_)return;data_->plates[list_->row(item)].visible=item->checkState()==Qt::Checked;if(item==list_->currentItem()&&item->checkState()!=Qt::Checked)rotationPresets_->hide();updateItems();refreshDiagnosticFaces();});
    refreshState();
}
void PlatesDialog::generateIfNeeded(){
    if(session_.busy()||(hasResult()&&failure_.isEmpty()&&session_.calculationSnapshotCurrent()))return;
    failure_.clear();
    status_->setMessage(uiText("正在生成展开图…"),StudioMessageRole::Feedback,true);
    session_.developPlates();
    refreshState();
}
void PlatesDialog::previewAssembly(){
    if(!data_)return;
    auto* snapshot=data_.get();
    const int chosen=showPlateAssembly(*data_,seams_,seam_,this);
    if(snapshot==data_.get())highlightSeam(chosen);
}
void PlatesDialog::reject(){if(session_.isCalculationSession()){QDialog::reject();return;}if(session_.busy()){session_.cancelPlates();status_->setMessage(uiText("正在取消…"),StudioMessageRole::Feedback,true);return;}QDialog::reject();}
void PlatesDialog::refreshState(){
    const bool idle=!session_.busy(),result=data_&&!data_->plates.isEmpty();
    const bool selected=result&&list_->currentRow()>=0&&list_->currentRow()<data_->plates.size();
    if(!idle||!selected)rotationPresets_->hide();
    if(session_.calculationQueued())status_->setMessage(uiText("排队中")+QString(" · %1").arg(session_.calculationQueuePosition()),StudioMessageRole::Feedback);
    else if(!idle&&property("calculation_was_queued").toBool())status_->setMessage(uiText("正在生成展开图…"),StudioMessageRole::Feedback,true);
    setProperty("calculation_was_queued",session_.calculationQueued());
    for(auto* button:findChildren<QPushButton*>()){
        if(button->property("plates_action").isValid())button->setEnabled(idle&&result);
    }
    for(auto* widget:findChildren<QWidget*>()){
        const auto field=widget->property("plate_field").toString();
        if(!field.isEmpty())widget->setEnabled(idle&&result);
        if(widget->property("plate_display").isValid())widget->setEnabled(idle&&result);
    }
    fontSize_->setEnabled(idle&&result);list_->setEnabled(idle&&result);
}
void PlatesDialog::showFailure(const QString& text){failure_=text;status_->setMessage(uiText(text),StudioMessageRole::Warning);refreshState();}
void PlatesDialog::receive(const QJsonObject& object){
    try{auto next=std::make_unique<fs::DevelopedPlates>(fs::DevelopedPlates::parse(object));static_cast<PlateView*>(view_)->cancelRotation();scene_->clear();items_.clear();list_->clear();data_=std::move(next);resultModel_=session_.model();translatePlateNames();buildScene();failure_.clear();completionMessage_=completionText(data_->plates.size(),"已展开 %1 块板片。");status_->setMessage(completionMessage_,StudioMessageRole::Feedback);refreshState();refreshDiagnosticFaces();}
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
    select(list_->currentRow());
    if(!completionMessage_.isEmpty()&&status_->text()==completionMessage_){
        completionMessage_=completionText(data_->plates.size(),"已展开 %1 块板片。");
        status_->setMessage(completionMessage_,StudioMessageRole::Feedback);
    }
}
void PlatesDialog::hideEvent(QHideEvent* event){
    QDialog::hideEvent(event);
    if(onDiagnosticFaces)onDiagnosticFaces({},0,{});
}
void PlatesDialog::showEvent(QShowEvent* event){
    QDialog::showEvent(event);
    refreshDiagnosticFaces();
}
void PlatesDialog::refreshDiagnosticFaces(){
    view_->viewport()->update();
    if(!onDiagnosticFaces)return;
    QVector<QVector<fs::Vec3>> polygons;
    const int row=list_->currentRow();
    if(isVisible()&&resultModel_&&row>=0&&row<items_.size()&&items_[row]->isVisible()){
        const auto& plate=data_->plates[row];
        if(plate.metrics["max_face_area_error"].toDouble()>plate.metrics["reference_area"].toDouble()*1e-7){
            for(int face:plate.worstFaces){
                QVector<fs::Vec3> polygon;
                for(int vertex:plate.faces[face]){
                    const auto p=plate.original[vertex];
                    polygon.append({p.x(),p.y(),p.z()});
                }
                polygons.append(std::move(polygon));
            }
        }
    }
    if(polygons.isEmpty())onDiagnosticFaces({},0,{});
    else onDiagnosticFaces(resultModel_->documentId,resultModel_->revision,polygons);
}
void PlatesDialog::buildScene(){
    seam_=-1;hovered_={};singleEdge_={};seams_=collectPlateSeams(*data_);
    updating_=true;list_->clear();submerged_->setChecked(data_->shadeSubmerged);
    for(int i=0;i<data_->plates.size();++i){auto&p=data_->plates[i];auto* row=new QListWidgetItem(p.name,list_);row->setFlags(row->flags()|Qt::ItemIsUserCheckable);row->setCheckState(Qt::Checked);
        auto* item=new PlateItem(p,data_->waterline,data_->underwaterColor);scene_->addItem(item);items_.append(item);
        item->selected=[this,i]{if(!updating_)list_->setCurrentRow(i);};
        item->changed=[this,i]{positionChanged(i);};}
    for(int i=0;i<seams_.size();++i){
        const auto& s=seams_[i];for(auto [a,b]:s.segments){
            static_cast<PlateItem*>(items_[s.left])->seamByEdge.insert(a,i);
            static_cast<PlateItem*>(items_[s.right])->seamByEdge.insert(b,i);
        }
    }
    for(auto*n:{gridX_,gridY_})n->setSuffix(" "+data_->lengthUnit());
    updating_=false;updateItems();list_->setCurrentRow(0);fit();
}
void PlatesDialog::highlightSeam(int index,bool unpaired){
    seam_=index;singleEdge_={};
    if(index>=0||unpaired){
        scene_->clearSelection();list_->setCurrentRow(-1);list_->clearSelection();metrics_->hide();
        refreshState();
    }
    if(index>=0)status_->setMessage(plateSeamMeasurement(*data_,seams_[index]),StudioMessageRole::Feedback);
    else status_->setMessage(unpaired?uiText("这条边没有唯一的配对板片"):completionMessage_,StudioMessageRole::Feedback);
    view_->viewport()->update();
}
PlateEdgeHit PlatesDialog::edgeAt(QPoint pos)const{
    if(!data_)return {};
    double best=36.;PlateEdgeHit hit;
    const auto scenePoint=view_->mapToScene(pos);
    const QRectF region=view_->mapToScene(QRect(pos-QPoint(8,8),QSize(17,17))).boundingRect();
    for(int i=items_.size()-1;i>=0;--i){
        auto* item=static_cast<PlateItem*>(items_[i]);if(!item->isVisible()||!item->sceneBoundingRect().intersects(region))continue;
        const bool inside=item->screenFill.contains(item->mapFromScene(scenePoint));
        const auto& p=data_->plates[i];const auto transform=item->deviceTransform(view_->viewportTransform());
        for(int j:item->boundaryIndices){
            const auto& e=p.edges[j];
            const QPointF a=transform.map(plateScreen(p.points[e.a])),v=transform.map(plateScreen(p.points[e.b]))-a;
            const double t=std::clamp(QPointF::dotProduct(pos-a,v)/std::max(1e-15,QPointF::dotProduct(v,v)),0.,1.);
            const QPointF delta=pos-(a+t*v);const double d=QPointF::dotProduct(delta,delta);
            if(d<best&&(!inside||d<=9.)){best=d;hit={i,j,item->seamByEdge.value(j,-1)};}
        }
        if(inside)break; // The visible plate interior occludes boundaries below it.
    }
    return hit;
}
int PlatesDialog::plateAt(QPoint pos)const{
    for(auto* hit:view_->items(pos)){
        while(hit->parentItem())hit=hit->parentItem();
        const int row=items_.indexOf(hit);
        if(row>=0)return row;
    }
    return edgeAt(pos).plate;
}
void PlatesDialog::select(int row){
    rotationPresets_->hide();
    if(!data_||row<0||row>=data_->plates.size()){metrics_->hide();diagnosticNote_->hide();refreshState();refreshDiagnosticFaces();return;}
    if(seam_>=0||singleEdge_.valid())highlightSeam(-1);
    updating_=true;auto&p=data_->plates[row];
    for(int i=0;i<items_.size();++i)items_[i]->setSelected(i==row);
    const auto m=p.metrics;double multiplier=p.joinedMirror?2:1;QString unit=data_->lengthUnit();
    const QStringList keys{"min_edge_error","max_edge_error","signed_area_error","area_percent","absolute_area_error","reference_area","normalized_error"};
    const QStringList labels{uiText("最小边长误差"),uiText("最大边长误差"),uiText("展开面积 − 原表面积"),uiText("面积差占原表面积"),uiText("面积差绝对值之和"),uiText("原表面积"),uiText("无量纲误差")};
    const QStringList units{unit,unit,unit+QChar(0x00b2),"%",unit+QChar(0x00b2),unit+QChar(0x00b2),{}};
    for(int row=0;row<keys.size();++row){
        const double value=row==3?100*m["signed_area_error"].toDouble()/m["reference_area"].toDouble():
            m[keys[row]].toDouble()*(row>=2&&row<=5?multiplier:1);
        const QStringList cells{labels[row],tableNumber::display(value),units[row]};
        for(int col=0;col<3;++col){auto* item=new QTableWidgetItem(cells[col]);item->setTextAlignment(col==1?Qt::AlignRight|Qt::AlignVCenter:Qt::AlignLeft|Qt::AlignVCenter);if(col==1)item->setToolTip(QString::number(value,'g',17));metrics_->setItem(row,col,item);}
        metrics_->setRowHeight(row,44);
    }
    const int scrollHeight=metrics_->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,metrics_);
    metrics_->setFixedHeight(metrics_->rowCount()*44+metrics_->horizontalHeader()->height()+scrollHeight+4);metrics_->show();
    QStringList warnings;
    if(p.flippedTriangles)warnings.append(uiText("展开图中有三角面翻转。"));
    if(p.collapsedTriangles)warnings.append(uiText("展开图中有三角面退化。"));
    if(p.overlapArea>0)warnings.append(uiText("展开图中有区域重叠。"));
    diagnosticNote_->setText(warnings.join('\n'));diagnosticNote_->setVisible(!warnings.isEmpty());
    plateNote_->setText(QString(uiText("仅展开已勾选“可展开曲面”的图层。展开误差不等于材料成形余量。"))+"\n\n"+uiText("边长误差为展开长度减原长度。")+(p.joinedMirror?"\n"+QString(uiText("本板含对称的两半；DXF 分别保留两侧闭合轮廓。")):QString{}));updating_=false;refreshState();refreshDiagnosticFaces();
}
void PlatesDialog::positionChanged(int row){
    if(updating_||!data_||row<0||row>=data_->plates.size())return;
    updating_=true;const auto& plate=data_->plates[row];
    if(dimensions_->isChecked())for(auto* child:items_[row]->childItems())if(child->data(0).isValid()){
        auto* label=static_cast<QGraphicsSimpleTextItem*>(child);const auto v=plate.placement().map(plate.points[child->data(0).toInt()]);
        label->setText(QString("%1, %2").arg(v.x(),0,'g',6).arg(v.y(),0,'g',6));
    }
    updating_=false;
}
void PlatesDialog::placePlate(int row){
    const auto& p=data_->plates[row];auto* item=items_[row];
    const auto c=plateScreen(p.bounds.center());QTransform tr;
    tr.translate(c.x(),c.y());tr.rotate(-p.rotation);tr.scale(1,p.mirrored?-1:1);tr.translate(-c.x(),-c.y());
    item->setTransform(tr);item->setPos(plateScreen(p.offset));positionChanged(row);
}
void PlatesDialog::updateItems(){if(!data_||updating_)return;hovered_={};updating_=true;
    for(int i=0;i<items_.size();++i){auto* item=static_cast<PlateItem*>(items_[i]);auto&p=data_->plates[i];item->setVisible(p.visible);auto c=plateScreen(p.bounds.center());placePlate(i);
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
void PlatesDialog::fit(){if(!data_)return;hovered_={};QRectF rect;for(auto*i:items_)if(i->isVisible())rect=rect.united(i->sceneBoundingRect());if(rect.isEmpty())return;
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
struct PlatesDialog::PdfSheet {
    struct Ink { QPainterPath path; QColor color; double width=0; bool dashed=false; };
    QVector<Ink> inks;
    struct Part {
        QTransform transform;
        QRectF bounds;
        PlateStyle style;
        QColor color,waterColor;
        QPainterPath fill,under;
        QVector<QLineF> boundary,inner,positive,negative;
        QVector<fs::PlateCurve> curves;
    };
    QVector<Part> parts;
    struct Label { QPointF anchor; QString text; QFont font; QColor color; };
    QVector<Label> labels;
    QRectF bounds,target;
    QRect page,physicalPaper;
    QFont font;
    QString warning,unit;
    double footerHeight=0,dpiScale=1,unitsToPixels=1,gridX=0,gridY=0;

    static void configure(QPdfWriter& pdf) {
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageOrientation(QPageLayout::Landscape);
        pdf.setResolution(144);
    }
    double scale(double numerator,double denominator) const {
        return denominator==0?qMin(target.width()/bounds.width(),target.height()/bounds.height()):
            unitsToPixels*numerator/denominator;
    }
    QPair<int,int> tiles(double k) const {
        const double cols=qMax(1.,std::ceil(bounds.width()*k/target.width()-1e-12));
        const double rows=qMax(1.,std::ceil(bounds.height()*k/target.height()-1e-12));
        if(!std::isfinite(cols*rows)||cols*rows>500)return {};
        return {int(cols),int(rows)};
    }
    QString caption(double numerator,double denominator) const {
        return denominator==0?uiText(" · 适合单页"):QStringLiteral(" · %1:%2").arg(numerator,0,'g',15).arg(denominator,0,'g',15);
    }
    QVector<Ink> prepareInk() const {
        QVector<Ink> result;
        for(const auto& part:parts){
            const auto ink=[&](const QPainterPath& path,QColor color,double width=0,bool dashed=false){
                if(!path.isEmpty())result.push_back({part.transform.map(path),color,width,dashed});
            };
            const auto lines=[&](const QVector<QLineF>& edges,QColor color,double width){
                QPainterPath path;for(const auto& edge:edges){path.moveTo(edge.p1());path.lineTo(edge.p2());}ink(path,color,width);
            };
            if(part.style.fill){ink(part.fill,part.color);if(part.style.submerged)ink(part.under,part.waterColor);}
            if(part.style.interior)lines(part.inner,QColor(174,183,194),.7);
            lines(part.boundary,QColor(39,55,73),1);
            if(part.style.errors){lines(part.positive,QColor(216,57,44),1.4);lines(part.negative,QColor(20,100,202),1.4);}
            for(const auto& curve:part.curves)if(part.style.sections[curve.kind-1]){QPainterPath path;path.addPolygon(curve.points);ink(path,curve.color,1);}
            if(part.style.boxes){QPainterPath path;path.addRect(part.bounds);ink(path,QColor(140,150,165),1,true);}
        }
        return result;
    }
    void paint(QPainter& p,int col,int row,double k,QPair<int,int> grid,const QString& caption,bool includeWarning=true) {
        const auto [columns,rows]=grid;
        p.setFont(font);p.setPen(Qt::black);
        p.drawText(QRectF(0,0,page.width(),target.top()),Qt::AlignLeft|Qt::AlignVCenter,
            QString(uiText("FreeShip 26 板材展开 · %1 · %2 / %3%4")).arg(unit).arg(row*columns+col+1).arg(rows*columns).arg(caption));
        p.save();p.setClipRect(target,Qt::IntersectClip);
        p.translate(target.left()-col*target.width(),target.top()-row*target.height());p.scale(k,k);
        p.translate(-bounds.topLeft());
        paintPlateBackground(p,QRectF(bounds.left()+col*target.width()/k,bounds.top()+row*target.height()/k,
            target.width()/k,target.height()/k),gridX,gridY);
        p.restore();p.save();p.setClipRect(target,Qt::IntersectClip);
        const QPointF pageOrigin=target.topLeft()-QPointF(col*target.width(),row*target.height())-bounds.topLeft()*k;
        QTransform toPaper(k,0,0,k,pageOrigin.x(),pageOrigin.y());
        const QRectF visible=toPaper.inverted().mapRect(target.adjusted(-2,-2,2,2));
        for(const Ink& ink:inks){
            const auto rect=ink.path.controlPointRect();
            if(rect.right()<visible.left()||rect.left()>visible.right()||rect.bottom()<visible.top()||rect.top()>visible.bottom())continue;
            const auto path=toPaper.map(ink.path);
            if(!ink.width){p.fillPath(path,ink.color);continue;}
            QPen pen(ink.color,ink.width,ink.dashed?Qt::DashLine:Qt::SolidLine,Qt::FlatCap);
            p.strokePath(path,pen);
        }
        for(const auto& label:labels){
            p.setFont(label.font);p.setPen(label.color);
            const QPointF origin=target.topLeft()-QPointF(col*target.width(),row*target.height());
            p.drawText(origin+(label.anchor-bounds.topLeft())*k+QPointF(0,p.fontMetrics().ascent()),label.text);
        }
        p.restore();
        if(includeWarning)paintStudioMessage(p,QRectF(0,page.height()-footerHeight,page.width(),footerHeight),warning,StudioMessageRole::Warning,dpiScale);
    }
    QString write(const QString& path,double numerator,double denominator) {
        const double k=scale(numerator,denominator);
        const auto grid=tiles(k);
        if(!grid.first)return uiText("当前比例超过 500 页，请调小比例。");
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return file.errorString();
        {
            QPdfWriter pdf(&file);configure(pdf);
            QPainter p;if(!p.begin(&pdf))return uiText("无法创建打印页面。");
            const auto title=caption(numerator,denominator);
            for(int row=0;row<grid.second;++row)for(int col=0;col<grid.first;++col){
                if((row||col)&&!pdf.newPage())return uiText("无法创建下一页。");
                paint(p,col,row,k,grid,title);
            }
            if(!p.end())return uiText("无法写入 PDF 页面。");
        }
        return file.commit()?QString():file.errorString();
    }
};

std::shared_ptr<PlatesDialog::PdfSheet> PlatesDialog::pdfSheet(bool prepare) {
    if(!data_)throw std::runtime_error(uiText("尚未生成展开图。"));
    auto sheet=std::make_shared<PdfSheet>();
    for(const auto* item:items_)if(item->isVisible()){
        const auto* plate=static_cast<const PlateItem*>(item);
        const auto transform=plate->sceneTransform();
        // A rotated local bounding box can extend far beyond a tapered plate.
        // Use the placed outline, so empty corners do not reduce the print scale.
        const auto bounds=transform.type()<=QTransform::TxScale?plate->sceneBoundingRect():
            transform.map(plate->fill).boundingRect();
        sheet->bounds=sheet->bounds.united(bounds);
    }
    if(sheet->bounds.isEmpty())throw std::runtime_error(uiText("没有可见板片。"));
    QBuffer buffer;QPdfWriter device(&buffer);PdfSheet::configure(device);
    sheet->page=device.pageLayout().paintRectPixels(device.logicalDpiX());
    sheet->physicalPaper=device.pageLayout().fullRectPixels(device.logicalDpiX());
    sheet->dpiScale=device.logicalDpiX()/96.;
    sheet->unitsToPixels=(data_->units==0?1000.:304.8)*device.logicalDpiX()/25.4;
    sheet->unit=data_->lengthUnit();
    sheet->warning=uiText("展开图不包含材料成形余量；请检查边长和面积误差。");
    sheet->font=studioTextFont(QFont("Microsoft YaHei UI",10));
    if(sheet->font.pointSizeF()>0)sheet->font.setPixelSize(qRound(sheet->font.pointSizeF()*device.logicalDpiY()/72.));
    QImage metrics(1,1,QImage::Format_ARGB32_Premultiplied);
    metrics.setDotsPerMeterX(qRound(device.logicalDpiX()/0.0254));metrics.setDotsPerMeterY(qRound(device.logicalDpiY()/0.0254));
    QPainter mp(&metrics);mp.setFont(sheet->font);
    const double head=mp.fontMetrics().height()+16*sheet->dpiScale;
    sheet->footerHeight=studioMessageHeight(mp,sheet->warning,sheet->page.width(),sheet->dpiScale);
    sheet->target={0,head,double(sheet->page.width()),
        qMax(1.,sheet->page.height()-head-sheet->footerHeight-12*sheet->dpiScale)};
    mp.end();
    auto* plateScene=static_cast<PlateScene*>(scene_);
    sheet->gridX=plateScene->gridX;sheet->gridY=plateScene->gridY;
    // Store model geometry only. Stroke it in paper coordinates, so even a
    // vanishingly short edge at a pointed deck end cannot acquire a model-sized cap.
    for(auto* item:items_)if(item->isVisible()){
        const auto* plate=static_cast<PlateItem*>(item);
        sheet->parts.push_back({plate->sceneTransform(),plate->boundingRect(),plate->style,plate->plate.color,plate->underwaterColor,
            plate->fill,plate->under,plate->boundary,plate->inner,plate->positive,plate->negative,plate->screenCurves});
        for(auto* child:plate->childItems())if(auto* text=qgraphicsitem_cast<QGraphicsSimpleTextItem*>(child);text&&text->isVisible()){
            QFont font=text->font();font.setPixelSize(qMax(1,qRound(font.pointSizeF()*device.logicalDpiY()/72.)));
            sheet->labels.push_back({text->scenePos(),text->text(),font,text->brush().color()});
        }
    }
    if(prepare){sheet->inks=sheet->prepareInk();sheet->parts.clear();}
    return sheet;
}

void PlatesDialog::chooseExport(const QString& format){
    if(!data_)return;
    std::shared_ptr<PdfSheet> sheet;
    if(format=="pdf") {
        if(!selectPdfSettings(sheet))return;
    }
    auto path=studioSaveFile(this,uiText("保存展开图"),preferenceFileLocation("export",QString(uiText("展开图.%1")).arg(format)),QString("%1 (*.%1)").arg(format));
    if(path.isEmpty())return;
    if(sheet&&QFileInfo(path).suffix().isEmpty())path+=".pdf";
    auto error=sheet?sheet->write(path,pdfNumerator_,pdfDenominator_):exportFile(path,format);
    status_->setMessage(error.isEmpty()?uiText("已保存到：")+path:error,error.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);
}

bool PlatesDialog::selectPdfSettings(std::shared_ptr<PdfSheet>& sheet) {
    PdfExportDialog options(this);options.setObjectName("PlatePdfSettings");
    options.setWindowTitle(QString(uiText("板材展开"))+" · PDF");
    if(hasPendingStudioDialog(this))return false;
    try {sheet=pdfSheet(false);}catch(const std::exception& e){status_->setMessage(uiText(e.what()),StudioMessageRole::Warning);return false;}
    PdfScaleInputs fields(QStringLiteral("PlatePdf"),100000);
    fields.numerator.setValue(pdfNumerator_);
    fields.denominator.setValue(pdfDenominator_==0?pdfFixedDenominator_:pdfDenominator_);
    options.fit.setChecked(pdfDenominator_==0);
    options.setPageDimensions(QSizeF(297,210));
    options.addScale(&fields);
    StudioMessage warning(StudioMessageRole::Warning,sheet->warning);
    warning.setObjectName("PlatePdfWarning");options.addPreviewFooter(&warning);
    StudioMessage progress(StudioMessageRole::Feedback);
    progress.setMessage(uiText("正在生成预览…"),StudioMessageRole::Feedback,true);
    options.addPreviewFooter(&progress);
    bool ready=false;
    QLabel pages;pages.setObjectName("PlatePdfPageCount");
    options.scaleOptions.addRow(uiText("页数"),&pages);
    auto refresh=[&]{
        const bool fit=options.fit.isChecked();
        const double numerator=fields.numerator.value(),denominator=fit?0:fields.denominator.value();
        const double k=sheet->scale(numerator,denominator);const auto grid=sheet->tiles(k);
        fields.showFitRatio(fit,sheet->unitsToPixels/k);
        options.confirm()->setEnabled(ready&&grid.first>0);
        pages.setText(grid.first?options.locale().toString(grid.first*grid.second):uiText("超过 500 页"));
        if(!ready||!grid.first){options.preview.clearPage();return;}
        const auto caption=sheet->caption(numerator,denominator);
        options.preview.showPage(sheet->physicalPaper.size(),k,0,[sheet,grid,caption](QPainter& p,QSizeF,double factor){
            p.translate(sheet->page.topLeft()-sheet->physicalPaper.topLeft());
            sheet->paint(p,0,0,factor,grid,caption,false);
        });
    };
    connect(&options.fit,&QCheckBox::toggled,&options,refresh);
    connect(&fields.numerator,&QDoubleSpinBox::valueChanged,&options,refresh);
    connect(&fields.denominator,&QDoubleSpinBox::valueChanged,&options,refresh);
    refresh();
    using Prepared=QVector<PdfSheet::Ink>;
    auto* watcher=new QFutureWatcher<Prepared>(&options);
    connect(watcher,&QFutureWatcher<Prepared>::finished,&options,[&]{
        if(!watcher->future().resultCount()){
            progress.setMessage(uiText("无法创建 PDF。"),StudioMessageRole::Warning);return;
        }
        sheet->inks=watcher->result();sheet->parts.clear();ready=true;
        progress.hide();refresh();
    });
    auto promise=std::make_shared<QPromise<Prepared>>();
    watcher->setFuture(promise->future());
    QTimer::singleShot(0,&options,[snapshot=sheet,promise]{
        QThreadPool::globalInstance()->start([snapshot,promise]{
            promise->start();
            try{promise->addResult(snapshot->prepareInk());}catch(const std::exception&){}
            promise->finish();
        });
    });
    if(runStudioDialog(options)!=QDialog::Accepted||!ready)return false;
    pdfNumerator_=fields.numerator.value();pdfFixedDenominator_=fields.denominator.value();
    pdfDenominator_=options.fit.isChecked()?0:pdfFixedDenominator_;
    return true;
}
QString PlatesDialog::exportFile(const QString& path,const QString& format){
    try{if(!data_)throw std::runtime_error(uiText("尚未生成展开图。"));bool visible=false;for(const auto&p:data_->plates)visible|=p.visible;if(!visible)throw std::runtime_error(uiText("没有可见的板片可供导出。"));
        if(!QStringList{"dxf","txt","json","png","pdf"}.contains(format))throw std::runtime_error(uiText("不支持的导出格式。"));
        if(format=="pdf")return pdfSheet()->write(path,pdfNumerator_,pdfDenominator_);
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return file.errorString();
        if(format=="png"){UncachedPlateRender vectorRender(items_);QImage image(2400,1600,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);scene_->clearSelection();QRectF bounds;for(auto*i:items_)if(i->isVisible())bounds=bounds.united(i->sceneBoundingRect());scene_->render(&painter,QRectF(40,40,2320,1520),bounds,Qt::KeepAspectRatio);painter.end();select(list_->currentRow());if(!image.save(&file,"PNG"))throw std::runtime_error(uiText("图片写入失败。"));}
        else{std::array<bool,4> sections;for(int i=0;i<4;++i)sections[i]=sections_[i]->isChecked();auto bytes=format=="dxf"?data_->dxf(sections):format=="txt"?data_->text():QJsonDocument(data_->json()).toJson();if(file.write(bytes)!=bytes.size())return file.errorString();}
        if(!file.commit())return file.errorString();
        return {};
    }catch(const std::exception&e){return QString::fromUtf8(e.what());}
}
