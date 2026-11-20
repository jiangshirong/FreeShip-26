#include "app/ui_language.h"
#include "preferences_store.h"
#include "hydro_dialog.h"
#include "table_number.h"
#include "loading_book_editor.h"
#include "hydro_number_list.h"
#include "studio_numeric.h"
#include "core/loading_cases.h"
#include "calculation_input_status.h"
#include "calculation_plot.h"
#include "studio_ui.h"
#include "studio_motion.h"
#include "data_motion.h"
#include "studio_table.h"
#include "report_browser.h"
#include "feedback_text.h"
#include "studio_read_ui.h"
#include <limits>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QStyleOptionTab>
#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTableView>
#include <QTabBar>
#include <QTabWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QTimer>
#include <QVariantAnimation>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
class StabilityTabBar final:public QTabBar {
public:
    StabilityTabBar(){
        setExpanding(false);
        setStyleSheet("QTabBar::tab:selected {border-bottom-color:transparent;}");
        motion_.setObjectName("HydroLoadingTabMotion");
        motion_.setEasingCurve(QEasingCurve::InOutCubic);
        connect(&motion_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){
            extension_=value.toReal();update();
        });
        connect(&motion_,&QVariantAnimation::finished,this,[this]{settle();});
    }
    void selectParameters(int index){selectedParameters_=index;update();}
    void showEditorTabs(bool visible){
        if(expanded_==visible&&isTabVisible(firstEditor)==visible)return;
        expanded_=visible;
        motion_.stop();
        const qreal target=visible?1.0:0.0;
        if(!isVisible()||!studio_motion::enabled()||qFuzzyCompare(extension_,target)){settle();return;}
        // Keep the expanded geometry until the last label has retracted.
        // Otherwise Qt shrinks the tab bar before the right tabs can move.
        setEditorTabsVisible(true);
        motion_.setDuration(qMax(1,qRound(280*std::abs(target-extension_))));
        motion_.setStartValue(extension_);motion_.setEndValue(target);
        motion_.setCurrentTime(0);motion_.start();
    }
protected:
    void mousePressEvent(QMouseEvent* event)override{
        // The two mode tabs stay under the pointer throughout the transition.
        if(motion_.state()==QAbstractAnimation::Running&&
           event->position().x()>tabRect(firstEditor-1).right()){
            event->accept();return;
        }
        QTabBar::mousePressEvent(event);
    }
    void keyPressEvent(QKeyEvent* event)override{
        if(motion_.state()==QAbstractAnimation::Running){motion_.stop();settle();}
        QTabBar::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);
        const bool moving=motion_.state()==QAbstractAnimation::Running;
        const int anchor=tabRect(firstEditor-1).right()+1;
        int edge=anchor;
        for(int index=firstEditor;moving&&index<pastEditors;++index){
            const auto rect=tabRect(index);
            const auto amount=std::clamp((extension_-.22*(index-firstEditor))/.78,0.0,1.0);
            const int exposed=qRound(rect.width()*amount);
            if(exposed>0)
                drawTab(painter,index,rect.translated(edge+exposed-rect.right()-1,0),QRect(edge,0,exposed,height()),true);
            edge+=exposed;
        }
        const int offset=moving?edge-tabRect(pastEditors-1).right()-1:0;
        for(int index=0;index<count();++index){
            if(!isTabVisible(index)||(moving&&index>=firstEditor&&index<pastEditors))continue;
            auto rect=tabRect(index);
            if(index>=pastEditors)rect.translate(offset,0);
            drawTab(painter,index,rect,this->rect(),moving&&index>=pastEditors);
        }
    }
private:
    static constexpr int firstEditor=2;
    static constexpr int pastEditors=firstEditor+1;
    void setEditorTabsVisible(bool visible){
        auto* tabs=static_cast<QTabWidget*>(parentWidget());
        for(int index=firstEditor;index<pastEditors;++index)tabs->setTabVisible(index,visible);
    }
    void settle(){
        extension_=expanded_?1.0:0.0;
        setEditorTabsVisible(expanded_);update();
    }
    void drawTab(QPainter& painter,int index,const QRect& rect,const QRect& clip,bool moving){
        QStyleOptionTab option;initStyleOption(&option,index);
        if(index<firstEditor){
            const bool selected=index==selectedParameters_;
            option.state.setFlag(QStyle::State_Selected,selected);
            if(!selected)option.state&=~(QStyle::State_Enabled|QStyle::State_MouseOver);
        }
        if(moving)option.state&=~QStyle::State_MouseOver;
        painter.save();painter.setClipRect(clip,Qt::IntersectClip);
        painter.translate(rect.topLeft()-option.rect.topLeft());
        style()->drawControl(QStyle::CE_TabBarTab,&option,&painter,this);
        painter.restore();
    }
    int selectedParameters_=0;
    bool expanded_=false;
    qreal extension_=0.0;
    QVariantAnimation motion_{this};
};

class StabilityTabs final:public QTabWidget {
public:
    StabilityTabs(){setTabBar(new StabilityTabBar);}
    void selectParameters(int index){
        auto* bar=static_cast<StabilityTabBar*>(tabBar());
        bar->selectParameters(index);bar->showEditorTabs(index==1);
    }
};

class AuxiliaryNoticePlacement final:public QWidget {
public:
    AuxiliaryNoticePlacement(QTabWidget* tabs,QWidget* page,QVBoxLayout* pageLayout,StudioMessage* notice)
        :QWidget(tabs),tabs_(tabs),page_(page),pageLayout_(pageLayout),notice_(notice){
        setObjectName("AuxiliaryReportBar");auto* layout=new QVBoxLayout(this);layout->setContentsMargins(12,0,0,0);
        layout->addWidget(notice_);notice_->setMinimumWidth(0);
        auto policy=notice_->sizePolicy();policy.setHorizontalPolicy(QSizePolicy::Ignored);notice_->setSizePolicy(policy);
        auto barPolicy=sizePolicy();barPolicy.setHeightForWidth(true);setSizePolicy(barPolicy);
        tabs_->installEventFilter(this);notice_->installEventFilter(this);installEventFilter(this);
        connect(tabs_,&QTabWidget::currentChanged,this,[this]{queue();});hide();queue();
    }
protected:
    int heightForWidth(int width)const override{return std::max(notice_->minimumHeight(),notice_->heightForWidth(std::max(1,width-12)));}
    QSize sizeHint()const override{return {preferredWidth_,heightForWidth(preferredWidth_)};}
    QSize minimumSizeHint()const override{return {0,notice_->fontMetrics().height()+20};}
    bool eventFilter(QObject*,QEvent* event)override{
        if(event->type()==QEvent::Resize||event->type()==QEvent::LayoutRequest||event->type()==QEvent::Show||event->type()==QEvent::Hide)queue();
        return false;
    }
private:
    void queue(){if(queued_)return;queued_=true;QTimer::singleShot(0,this,[this]{queued_=false;arrange();});}
    void arrange(){
        if(tabs_->currentWidget()!=page_||notice_->text().isEmpty()){hide();return;}
        const int room=tabs_->width()-tabs_->tabBar()->sizeHint().width()-16;
        const auto oldHint=sizeHint();const bool wasVisible=isVisible();
        const int headerWidth=std::min(560,room);
        const int singleLineHeight=std::max(40,notice_->fontMetrics().height()+22);
        const bool inHeader=room>=270&&heightForWidth(headerWidth)<=std::max(singleLineHeight,tabs_->tabBar()->sizeHint().height());
        if(inHeader){
            pageLayout_->removeWidget(this);preferredWidth_=headerWidth;setMinimumWidth(0);setMaximumWidth(preferredWidth_);
            if(tabs_->cornerWidget(Qt::TopRightCorner)!=this)tabs_->setCornerWidget(this,Qt::TopRightCorner);
        }else{
            if(tabs_->cornerWidget(Qt::TopRightCorner)==this)tabs_->setCornerWidget(nullptr,Qt::TopRightCorner);
            setMinimumWidth(0);setMaximumWidth(QWIDGETSIZE_MAX);
            if(pageLayout_->indexOf(this)<0)pageLayout_->insertWidget(0,this);
        }
        const auto margins=pageLayout_->contentsMargins();
        const int available=inHeader?headerWidth:page_->contentsRect().width()-margins.left()-margins.right();
        setMinimumHeight(heightForWidth(available));
        setProperty("studio_warning_layout",inHeader?"tab-row":"wrapped");show();
        if(oldHint!=sizeHint()||!wasVisible)updateGeometry();
    }
    QTabWidget* tabs_;QWidget* page_;QVBoxLayout* pageLayout_;StudioMessage* notice_;bool queued_=false;int preferredWidth_=360;
};
constexpr const char* AboveWaterFile="AboveWaterGeometry.txt";
constexpr const char* AuxiliaryFiles[]{"Bonjean.txt","SACs.txt","Weights.txt","Sterns.txt",AboveWaterFile};
QStringList auxiliaryTitles(){
    return {"Bonjean",uiText("横剖面面积"),uiText("重量"),uiText("首尾水线位置"),uiText("水线上方几何")};
}
QJsonObject aboveWaterGeometry(const QJsonObject& result){
    const auto rows=result["rows"].toArray();
    return rows.isEmpty()?QJsonObject{}:rows.first().toObject()["above_water_geometry"].toObject();
}
// Keep the three reading columns together even in a maximized window.
// Unused canvas stays outside the columns; units are not stretched across it.
class SingleHydroTable final:public QTableWidget {
public:
    SingleHydroTable(){
        setObjectName("HydroSingleResult");setProperty("studio_content_role","result_data");
        setColumnCount(3);setHorizontalHeaderLabels({uiText("参数"),uiText("数值"),uiText("单位")});
        auto face=font();face.setPointSizeF(13);setFont(face);
        verticalHeader()->hide();setFrameShape(QFrame::NoFrame);
        setShowGrid(false);setWordWrap(false);
        setItemDelegate(new StudioWidgetCellDelegate(this,QColor("#e2e9e5"),0));
        auto* number=horizontalHeaderItem(1);number->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        horizontalHeaderItem(2)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    }
};
class HydroNumber final:public QDoubleSpinBox {
    QString textFromValue(double v)const override{
        QString text=QDoubleSpinBox::textFromValue(v);
        if(text.contains(locale().decimalPoint())){
            while(text.endsWith('0'))text.chop(1);
            if(text.endsWith(locale().decimalPoint()))text.chop(locale().decimalPoint().size());
        }return text;
    }
};
QColor curveColor(int index){return QColor::fromHsl((205+137*index)%360,165,115);}
QString full(double value){return QString::number(value,'g',17);}
QString list(const QJsonArray& data){
    QStringList parts;for(const auto& v:data)parts<<tableNumber::display(v.toDouble());return parts.join(", ");
}
QJsonValue value(const QJsonObject& row,const QString& key){
    if(key=="gz_assessment"&&row.contains("required_gz")){
        const auto margin=row["gz_margin"];
        return !margin.isDouble()?uiText("未评估"):margin.toDouble()>=0?uiText("满足"):uiText("不满足");
    }
    const auto parts=key.split('.');if(parts.size()!=2)return row[key];const auto a=row[parts[0]].toArray();const int i=parts[1].toInt();return i<a.size()?a[i]:QJsonValue{};
}
QString hydroMessage(const QString& source){
    // The solver may group several independent diagnostics into one notice.
    auto lines=source.split('\n');
    for(auto& line:lines)if(!line.trimmed().isEmpty())line=uiText(line.trimmed());
    return lines.join('\n');
}
QString singleHydroIssue(const QJsonObject& result){
    const auto rows=result["rows"].toArray();
    const auto row=rows.isEmpty()?QJsonObject{}:rows.first().toObject();
    const auto issue=hydroMessage(row["message"].toString());
    if(!issue.isEmpty())return issue;
    if(row["status"]!="ok")return uiText("未得到有效结果");
    if(row["partial"].toBool())return uiText("部分量未提供");
    return {};
}
struct DraftTable {
    QString title;
    QStringList keys;
};
QPair<QString,QString> splitUnit(const QString& heading){
    const auto pos=heading.lastIndexOf(" (");
    if(pos<0||!heading.endsWith(')'))return {heading,{}};
    return {heading.left(pos),heading.mid(pos+2,heading.size()-pos-3)};
}
QString reportNumber(double value,const QString& key={}){
    if((key=="buoyancy.1"||key=="waterplane_cog.1")&&std::abs(value)<1e-12)return "0";
    return tableNumber::display(value);
}
QString skippedWaterlineNotice(const QJsonObject& data,const QString& unit){
    QStringList elevations;
    for(const auto& z:data["skipped_waterlines"].toArray())elevations.append(QString::number(z.toDouble(),'g',8));
    if(elevations.isEmpty())return {};
    const auto leak=QString::number(data["waterline_leak_z"].toDouble(),'g',8);
    const auto message=data["status"]=="error"?
        uiText("所有水线均高于船体最低漏点 Z = %1 %2。水线 Z = %3 %2。"):
        uiText("船体最低漏点 Z = %1 %2。已跳过水线 Z = %3 %2。");
    return QString(message).arg(leak,unit,elevations.join(", "));
}
report::Table aboveWaterTable(const QJsonObject& geometry,bool metric){
    const QString unit=metric?"m":"ft";
    report::Table table{uiText("水线上方几何"),{uiText("参数"),uiText("数值"),uiText("单位")},{},{0,1,2},{56,25,19},520};
    auto add=[&](const char* key,const QString& label,const QString& units){
        if(const auto value=geometry[key];value.isDouble())table.rows.append({label,reportNumber(value.toDouble()),units});
    };
    add("wind_projection_area",uiText("横向受风投影面积"),unit+"²");
    add("wind_centroid_height",uiText("投影形心距水线高度"),unit);
    add("deck_edge_min_height",uiText("甲板边缘距水线的最小高度"),unit);
    add("wind_centroid_x",uiText("投影形心 X（水线坐标系）"),unit);
    add("wind_centroid_z",uiText("投影形心 Z（水线坐标系）"),unit);
    add("wind_centroid_distance_from_fp",uiText("投影形心距艏垂线的纵向距离"),unit);
    return table;
}
class AuxiliaryTable final:public QAbstractTableModel {
public:
    AuxiliaryTable(const QJsonObject& result,const QString& file,bool metric,const QStringList& layerLabels,QObject* parent)
        :QAbstractTableModel(parent),file_(file),waterlines_(result["waterlines"].toArray()),stations_(result["stations"].toArray()){
        const QString unit=metric?"m":"ft";
        const auto length=" ("+unit+")";
        if(file_==AboveWaterFile){
            auto table=aboveWaterTable(result,metric);
            headers_=std::move(table.headers);geometryRows_=std::move(table.rows);
            rowCount_=int(geometryRows_.size());
        }else if(file_=="Weights.txt"){
            headers_={uiText("图层"),uiText("面"),uiText("面积")+QString(" (")+unit+"²)",
                uiText("重量")+QString(metric?" (t)":" (long ton)"),"X"+length,"Y"+length,"Z"+length,"X min"+length,"X max"+length};
            for(const auto& entry:result["layers"].toArray()){
                auto layer=entry.toObject();
                const auto ordinal=layer["index"].toInt(-1);
                if(layer["name_generated"].toBool())
                    layer["name"]=QString(uiText("图层"))+" "+QString::number(ordinal);
                else if(layer["name"].toString().trimmed().isEmpty())
                    layer["name"]=layerLabels.value(ordinal,QString(uiText("图层"))+" "+QString::number(ordinal));
                const auto faces=layer["faces"].toArray();
                layers_.append({layer,faces,rowCount_});
                rowCount_+=1+int(faces.size());
            }
        }else if(file_=="Bonjean.txt"){
            rowCount_=int(stations_.size());headers_<<"X"+length;
            for(const auto& row:waterlines_)headers_<<QString(uiText("吃水"))+" = "+reportNumber(row.toObject()["draft"].toDouble())+" "+unit+QStringLiteral(" (")+unit+"²)";
        }else{
            rowCount_=int(waterlines_.size());
            headers_<<uiText("水线 Z")+length;
            if(file_=="Sterns.txt")headers_<<uiText("尾端 X")+length<<uiText("首端 X")+length;
            else for(const auto& station:result["stations"].toArray())
                headers_<<"X = "+reportNumber(station.toDouble())+" "+unit+QStringLiteral(" (")+unit+"²)";
        }
    }
    int rowCount(const QModelIndex& parent={})const override{return parent.isValid()?0:rowCount_;}
    int columnCount(const QModelIndex& parent={})const override{return parent.isValid()?0:int(headers_.size());}
    QVariant headerData(int index,Qt::Orientation orientation,int role)const override{
        return role==Qt::DisplayRole&&orientation==Qt::Horizontal?QVariant(headers_.value(index)):QVariant{};
    }
    QVariant data(const QModelIndex& index,int role)const override{
        if(!index.isValid())return {};
        const int col=index.column();
        if(role==Qt::TextAlignmentRole)return int(Qt::AlignVCenter|((file_=="Weights.txt"||file_==AboveWaterFile)&&col==0?Qt::AlignLeft:Qt::AlignRight));
        if(role!=Qt::DisplayRole&&role!=Qt::ToolTipRole)return {};
        if(file_==AboveWaterFile)return geometryRows_.at(index.row()).value(col);
        QJsonValue cell;
        if(file_=="Weights.txt"){
            const auto row=std::upper_bound(layers_.cbegin(),layers_.cend(),index.row(),[](int row,const Layer& layer){return row<layer.start;})-1;
            const bool summary=index.row()==row->start;
            const auto item=summary?row->data:row->faces[index.row()-row->start-1].toObject();
            if(col==0){
                const auto name=row->data["name"].toString();
                return name.trimmed().isEmpty()?QString(uiText("图层"))+" "+QString::number(row->data["index"].toInt()):name;
            }
            if(summary&&col==1)return QString(uiText("合计"));
            if(col>=4&&col<=6)cell=item["cog"].toArray().at(col-4);
            else{
                static const char* keys[]{"","child","area","weight","","","","xmin","xmax"};
                cell=item[summary&&col==2?"surface_area":keys[col]];
            }
        }else if(file_=="Bonjean.txt"){
            cell=col==0?stations_[index.row()]:waterlines_[col-1].toObject()["bonjean_areas"].toArray()[index.row()];
        }else{
            const auto row=waterlines_[index.row()].toObject();
            if(col==0)cell=row["waterline_z"];
            else if(file_=="Sterns.txt")cell=row[col==1?"aft_x":"fore_x"];
            else cell=row[file_=="SACs.txt"?"sac_areas":"bonjean_areas"].toArray().at(col-1);
        }
        if(!cell.isDouble())return QStringLiteral("—");
        return role==Qt::ToolTipRole?full(cell.toDouble()):reportNumber(cell.toDouble());
    }
private:
    struct Layer{QJsonObject data;QJsonArray faces;int start;};
    QString file_;QJsonArray waterlines_,stations_;QStringList headers_;QList<Layer> layers_;QList<QStringList> geometryRows_;int rowCount_=0;
};
class SolveStatusDelegate final:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        QStyleOptionViewItem cell(option);initStyleOption(&cell,index);
        const auto icon=cell.icon;cell.icon={};cell.features&=~QStyleOptionViewItem::HasDecoration;
        const auto* style=cell.widget?cell.widget->style():QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&cell,painter,cell.widget);
        const int side=std::min({cell.rect.width(),cell.rect.height(),std::max(12,cell.fontMetrics.height())});
        icon.paint(painter,QStyle::alignedRect(cell.direction,Qt::AlignCenter,QSize(side,side),cell.rect));
    }
};
class HydroStatusTable final:public QTableWidget {
public:
    HydroStatusTable(){
        setMouseTracking(true);viewport()->setMouseTracking(true);
        setItemDelegateForColumn(0,new SolveStatusDelegate(this));
        for(auto* scroll:{horizontalScrollBar(),verticalScrollBar()})
            connect(scroll,&QScrollBar::valueChanged,this,[this]{clearStatusTip();});
        connect(model(),&QAbstractItemModel::modelAboutToBeReset,this,[this]{clearStatusTip();});
    }
protected:
    bool viewportEvent(QEvent* event)override{
        if(event->type()==QEvent::MouseMove){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const auto index=indexAt(mouse->position().toPoint());
            if(index!=hovered_){
                clearStatusTip();
                if(index.isValid()&&index.column()==0){
                    hovered_=index;
                    shown_=index.data(Qt::ToolTipRole).toString();
                    if(!shown_.isEmpty())QToolTip::showText(mouse->globalPosition().toPoint(),shown_,viewport(),visualRect(index));
                }
            }
        }else if(event->type()==QEvent::Leave||event->type()==QEvent::Hide)clearStatusTip();
        return QTableWidget::viewportEvent(event);
    }
private:
    QPersistentModelIndex hovered_;
    QString shown_;
    void clearStatusTip(){
        if(!shown_.isEmpty()&&QToolTip::text()==shown_)QToolTip::hideText();
        hovered_=QPersistentModelIndex{};shown_.clear();
    }
};
class HydroPlot final:public QWidget {
public:
    QJsonObject result;QString lengthUnit;
    explicit HydroPlot(QWidget* p):QWidget(p){setMinimumSize(400,280);}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),QColor(250,252,253));p.setRenderHint(QPainter::Antialiasing);
        QRectF area=QRectF(rect()).adjusted(65,35,-25,-55);double xmin=1e300,xmax=-1e300,ymin=0,ymax=0;
        const auto rows=result["rows"].toArray();
        const auto x=[](const QJsonObject& row){return row["draft"].toDouble();};
        const auto y=[](const QJsonObject& row){return row["displacement"].toDouble();};
        for(auto v:rows){auto r=v.toObject();if(r["status"]!="ok")continue;xmin=std::min(xmin,x(r));xmax=std::max(xmax,x(r));ymin=std::min(ymin,y(r));ymax=std::max(ymax,y(r));}
        if(xmin>xmax){const QString text=uiText("尚无可绘制的结果");const qreal w=qMin(460,width()-24);const qreal h=studioMessageHeight(p,text,w);paintStudioMessage(p,QRectF((width()-w)/2,(height()-h)/2,w,h),text,StudioMessageRole::Explanation);return;}
        if(xmax==xmin)xmax=xmin+1;
        if(ymax==ymin)ymax=ymin+1;
        auto map=[&](double a,double b){return QPointF(area.left()+(a-xmin)/(xmax-xmin)*area.width(),area.bottom()-(b-ymin)/(ymax-ymin)*area.height());};
        p.setPen(QColor(215,222,229));
        for(int i=0;i<=5;++i){double xx=xmin+(xmax-xmin)*i/5,yy=ymin+(ymax-ymin)*i/5;p.drawLine(map(xx,ymin),map(xx,ymax));p.drawLine(map(xmin,yy),map(xmax,yy));
            p.setPen(QColor(65,75,85));p.drawText(QRectF(map(xx,ymin)+QPointF(-35,5),QSizeF(70,20)),Qt::AlignCenter,QString::number(xx,'g',5));
            p.drawText(QRectF(0,map(xmin,yy).y()-10,60,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(yy,'g',5));p.setPen(QColor(215,222,229));}
        p.setPen(QColor(45,55,65));p.drawText(QRectF(0,5,width(),25),Qt::AlignCenter,uiText("排水量 (")+(result["units"]=="metric"?QString("t"):QString("long ton"))+")");
        p.drawText(QRectF(0,height()-30,width(),25),Qt::AlignCenter,uiText("吃水 (")+lengthUnit+")");
        QPainterPath path;bool connected=false;
        p.setPen(QPen(curveColor(0),1.8));
        for(const auto& value:rows){
            const auto row=value.toObject();
            if(row["status"]!="ok"){connected=false;continue;}
            const auto point=map(x(row),y(row));
            if(connected)path.lineTo(point);else path.moveTo(point);
            connected=true;p.drawEllipse(point,2,2);
        }
        p.drawPath(path);
    }
};
}
HydroDialog::HydroDialog(ModelSession& session,const QString& mode,const QJsonObject& defaults,bool hidden,QWidget* parent)
    :QDialog(parent),session_(session),mode_(mode){
    setStudioWindowIcon(this,"Hydrostatics");
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setProperty("hydro_dialog",mode);resize(1180,760);
    const QString titleSource=mode=="single"?"设计静水力":mode=="table"?"静水力吃水表":"稳性曲线与平衡";
    setProperty("calculation_title_source",titleSource);setWindowTitle(uiText(titleSource));
    auto* layout=new QVBoxLayout(this);
    const auto makeForm=[](QVBoxLayout* layout){
        auto* form=new QFormLayout;layout->addLayout(form);
        form->setVerticalSpacing(14);form->setHorizontalSpacing(16);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return form;
    };
    auto* parameters=new QWidget;auto* left=new QVBoxLayout(parameters);auto* form=makeForm(left);
    QWidget* loadingParameters=nullptr;QVBoxLayout* loadingLayout=nullptr;
    auto* fieldSet=&fields_;auto* labelSet=&fieldLabels_;
    const auto m=session.model();const QString unit=m->units=="metric"?"m":"ft",mass=m->units=="metric"?"t":"long ton";
    auto addField=[&](const QString& key,const QString& title,QWidget* field){
        field->setProperty("field",key);fieldSet->insert(key,field);
        if(title.isEmpty())form->addRow(calculationInput::caption(field,field,key));
        else{
            auto* label=new QLabel(title);label->setWordWrap(true);label->setBuddy(field);
            label->setProperty("hydro_field_label",key);
            labelSet->insert(key,label);
            form->addRow(calculationInput::caption(label,field,key),field);
        }
    };
    auto num=[&](QString key,QString label,double initial,double low=-1e9,double high=1e9){auto* n=new HydroNumber;n->setDecimals(12);n->setRange(low,high);n->setKeyboardTracking(false);n->setSingleStep(.1);n->setValue(initial);addField(key,label,n);};
    auto check=[&](QString key,QString label,bool initial){auto* n=new QCheckBox(label);n->setChecked(initial);addField(key,{},n);};
    const auto saved=defaults[QString("saved_")+mode+"_inputs"].toObject();
    if(mode=="single"){num("draft",uiText("吃水 (")+unit+")",saved["draft"].toDouble(m->draft),0,1e6);num("heel",uiText("横倾角 (°)"),saved["heel"].toDouble(),-85,85);}
    const auto range=mode=="table"&&saved.isEmpty()?defaults["saved_table_range"].toObject():saved;
    if(mode=="table"){
        num("start",uiText("起始吃水 (")+unit+")",range["start"].toDouble(std::max(0.,defaults["start"].toDouble())),0,1e6);
        num("end",uiText("终止吃水 (")+unit+")",range["end"].toDouble(std::max(m->draft,defaults["end"].toDouble())),0,1e6);
        const double step=defaults["step"].toDouble();
        num("step",uiText("吃水间隔 (")+unit+")",range["step"].toDouble(step>0?step:std::max(.001,m->draft/10)),.000001,1e6);
    }
    const double trim=defaults[mode=="cross"?"cross_trim":"trim"].toDouble();
    num("trim",uiText("纵倾差 (")+unit+")",range["trim"].toDouble(trim),-m->length,m->length);
    if(mode=="cross"){
        auto* history=new QUndoStack(this);
        bindDraftHistory(this,*history,[this]{return !session_.busy();});
        loadingBook_=saved["loading_book"].toObject();
        if(saved.contains("gz_criteria"))loadingBook_["criteria"]=saved["gz_criteria"];
        useLoadingBook_=saved["use_loading_book"].toBool();
        auto displacements=defaults["displacements"].toArray();if(displacements.isEmpty())displacements.append(defaults["design_displacement"].toDouble(1));
        auto angles=defaults["angles"].toArray();if(angles.isEmpty())angles=QJsonArray{0,5,10,15,20,30,40,50,60};
        const auto numberList=[&](const QString& key,const QString& title,const QJsonValue& value,const QJsonArray& fallback,int limit,double min,double max){
            auto* field=new HydroNumberList(limit,min,max,*history);
            field->setValues(value.isString()||value.isArray()?value:QJsonValue(fallback));
            field->setProperty("field",key);fieldSet->insert(key,field);
            auto* label=new QLabel(title);label->setWordWrap(true);label->setBuddy(field);
            label->setProperty("hydro_field_label",key);labelSet->insert(key,label);
            field->setCaption(calculationInput::caption(label,field,key));
            form->addRow(field);
        };
        numberList("displacements",uiText("排水量 (")+mass+")",saved["displacements"],displacements,100,1e-6,1e9);
        numberList("angles",uiText("横倾角 (°)"),saved["angles"],angles,181,-180,180);
        num("lcg",uiText("重心 X (")+unit+")",saved["lcg"].toDouble((m->minimum.x+m->maximum.x)/2));
        num("tcg",uiText("重心 Y (")+unit+")",saved["tcg"].toDouble());
        num("vcg",uiText("重心 Z (")+unit+")",saved["vcg"].toDouble(defaults["vcg"].toDouble()));
        const auto freeTrim=saved["free_trim"].toBool(defaults["free_trim"].toBool());
        check("free_trim",uiText("求解自由纵倾平衡"),freeTrim);

        loadingParameters=new QWidget;loadingLayout=new QVBoxLayout(loadingParameters);form=makeForm(loadingLayout);
        fieldSet=&loadingFields_;labelSet=&loadingFieldLabels_;
        const auto loadingSaved=saved["loading_parameters"].isObject()?saved["loading_parameters"].toObject():saved;
        num("trim",uiText("纵倾差 (")+unit+")",loadingSaved["trim"].toDouble(range["trim"].toDouble(trim)),-m->length,m->length);
        loadingEditor_=new loadingEditor::Workspace(this,loadingBook_,*history,m,defaults,
            [this](const std::optional<QJsonObject>& weights){
                bool changed=false;
                if(weights){
                    auto book=*weights;
                    book["criteria"]=loadingBook_.value("criteria").toArray();
                    changed=book!=loadingBook_;
                    if(changed)loadingBook_=book;
                }
                refreshLoadingSummary();refreshState();
                if(changed&&useLoadingBook_&&!result_.isEmpty())
                    status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);
            },
            [this](const std::optional<QJsonArray>& criteria){
                const bool changed=criteria&&*criteria!=loadingBook_.value("criteria").toArray();
                if(changed){
                    if(loadingBook_.contains("cases")||!criteria->isEmpty())loadingBook_["criteria"]=*criteria;
                    else loadingBook_.remove("criteria");
                }
                refreshState();
                if(changed&&!result_.isEmpty())
                    status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);
            });
        loadingSummary_=new QWidget;auto* summaryLayout=new QVBoxLayout(loadingSummary_);
        summaryLayout->setContentsMargins(0,0,0,0);summaryLayout->setSpacing(6);
        loadingSummaryTitle_=new QLabel;loadingSummaryTitle_->setWordWrap(true);
        QFont titleFont=loadingSummaryTitle_->font();titleFont.setBold(true);loadingSummaryTitle_->setFont(titleFont);
        summaryLayout->addWidget(loadingSummaryTitle_);
        loadingValues_=new QTableWidget;loadingValues_->setObjectName("HydroLoadingValues");
        loadingValues_->setColumnCount(5);loadingValues_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        loadingValues_->setSelectionMode(QAbstractItemView::SingleSelection);
        loadingValues_->verticalHeader()->hide();loadingValues_->setMinimumWidth(0);
        loadingValues_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        applyStudioScrollbars(loadingValues_);configureStudioDataTable(loadingValues_);
        summaryLayout->addWidget(loadingValues_);form->addRow(loadingSummary_);
        loadingNotice_=new StudioMessage(StudioMessageRole::Warning,{},this);
        loadingNotice_->setObjectName("HydroLoadingNotice");
        numberList("angles",uiText("横倾角 (°)"),loadingSaved["angles"],angles,181,-180,180);
        check("free_trim",uiText("求解自由纵倾平衡"),loadingSaved["free_trim"].toBool(freeTrim));
        translateLoadingInputs();refreshLoadingSummary();
    }
    run_=new QPushButton(uiText("计算"));run_->setProperty("hydro_action","calculate");cancel_=new QPushButton(uiText("取消计算"));cancel_->setProperty("hydro_action","cancel");
    auto* controlsPanel=new QWidget;
    auto* controls=new QHBoxLayout(controlsPanel);controls->setContentsMargins(0,0,0,0);
    controls->addWidget(run_);controls->addWidget(cancel_);controls->addStretch();
    (useLoadingBook_?loadingLayout:left)->addWidget(controlsPanel);
    left->addStretch();
    const auto parameterScroll=[](QWidget* content){
        auto* scroll=new QScrollArea;
        scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(content);applyStudioScrollbars(scroll);
        return scroll;
    };
    const auto withCriteria=[](QWidget* inputs,QVBoxLayout*& criteriaHost){
        auto* canvas=new QWidget;
        auto* columns=new QHBoxLayout(canvas);
        columns->setContentsMargins(0,0,0,0);columns->setSpacing(14);
        columns->addWidget(inputs,1);
        auto* slot=new QWidget;
        criteriaHost=new QVBoxLayout(slot);
        criteriaHost->setContentsMargins(0,0,0,0);
        columns->addWidget(slot,1);
        return canvas;
    };
    configureStudioNumericForms(parameters);
    QVBoxLayout *manualCriteria=nullptr,*namedCriteria=nullptr;
    parameterPage_=parameterScroll(loadingParameters?withCriteria(parameters,manualCriteria):parameters);
    parameterPage_->setProperty("calculation_page","general");
    tabs_=loadingParameters?new StabilityTabs:new QTabWidget;layout->addWidget(tabs_,1);
    tabs_->addTab(parameterPage_,uiText("参数表"));
    if(loadingParameters){
        loadingLayout->addStretch();
        configureStudioNumericForms(loadingParameters);
        loadingPage_=new QWidget;
        loadingPage_->setProperty("calculation_page","loading_parameters");
        auto* namedLayout=new QVBoxLayout(loadingPage_);
        namedLayout->setContentsMargins(0,0,0,0);
        namedLayout->addWidget(parameterScroll(withCriteria(loadingParameters,namedCriteria)),1);
        namedLayout->addWidget(loadingNotice_);
        auto* criterionHost=useLoadingBook_?namedCriteria:manualCriteria;
        criterionHost->addWidget(loadingEditor_->criteriaPanel());
        loadingEditor_->criteriaPanel()->show();
        tabs_->addTab(loadingPage_,uiText("参数表（装载工况）"));
        tabs_->addTab(loadingEditor_->casesPage(),uiText("装载工况"));
        if(useLoadingBook_)tabs_->setCurrentWidget(loadingPage_);
        static_cast<StabilityTabs*>(tabs_)->selectParameters(tabs_->currentIndex());
    }
    if(mode=="single")table_=new SingleHydroTable;
    else table_=new HydroStatusTable;
    table_->setProperty("hydro_result_table",true);table_->setProperty("calculation_page","results");
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(false);tabs_->addTab(table_,uiText("结果表"));
    applyStudioScrollbars(table_);
    configureStudioDataTable(table_,mode=="single",mode=="single"?0:-1);
    auto* plotPage=new QWidget;plotPage->setObjectName("HydroCurvePage");plotPage->setProperty("calculation_page","curve");
    auto* plotLayout=new QVBoxLayout(plotPage);plotLayout->setContentsMargins(0,0,0,0);
    if(mode=="cross"){
        plot_=new CalculationPlot(plotPage);
        plot_->setObjectName("HydroStabilityChart");
    }else{
        auto* plot=new HydroPlot(plotPage);plot->lengthUnit=unit;plot_=plot;
    }
    plotLayout->addWidget(plot_,1);
    tabs_->addTab(plotPage,uiText("曲线"));
    tabs_->setTabVisible(tabs_->indexOf(plotPage),mode!="single");
    text_=new report::Browser;text_->setObjectName("HydroReport");text_->setProperty("calculation_page","report");tabs_->addTab(text_,uiText("报告与基准"));
    if(mode=="cross"){
        comparison_=new QTableWidget;comparison_->setObjectName("LoadingComparison");comparison_->setProperty("calculation_page","comparison");
        comparison_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        applyStudioScrollbars(comparison_);configureStudioDataTable(comparison_,true,-1,true);
        tabs_->addTab(comparison_,uiText("工况对比"));tabs_->setTabVisible(tabs_->indexOf(comparison_),false);
    }
    if(mode=="single"){
        auxiliaryPage_=new QWidget;auxiliaryPage_->setProperty("calculation_page","auxiliary");auto* box=new QVBoxLayout(auxiliaryPage_);
        box->setSpacing(12);tabs_->addTab(auxiliaryPage_,uiText("附加表"));
        // One warning belongs beside the top-level tabs; narrow layouts wrap.
        auxiliaryNotice_=new StudioMessage(StudioMessageRole::Warning);
        auxiliaryNotice_->setObjectName("AuxiliaryReportNotice");
        auto* selector=new QWidget;selector->setObjectName("AuxiliaryReportSelector");
        selector->setStyleSheet(
            "QWidget#AuxiliaryReportSelector {background:#eef3f0;border:1px solid #d7e3dd;border-radius:6px;}"
            "QToolButton {color:#315d57;background:transparent;border:1px solid transparent;border-radius:4px;padding:7px 14px;}"
            "QToolButton:hover {background:#e3eee8;}"
            "QToolButton:checked {color:#124f47;background:#d5e9df;border-color:#adcfc0;}"
            "QToolButton:disabled {color:#8d9994;}");
        auto* choices=new QHBoxLayout(selector);choices->setContentsMargins(3,3,3,3);choices->setSpacing(3);
        auxiliaryChoices_=new QButtonGroup(this);auxiliaryChoices_->setExclusive(true);
        auxiliaryStack_=new QStackedWidget;auxiliaryStack_->setObjectName("AuxiliaryReportStack");
        const auto titles=auxiliaryTitles();
        for(int index=0;index<titles.size();++index){
            auto* choice=new QToolButton(selector);choice->setText(titles[index]);choice->setCheckable(true);
            choice->setProperty("auxiliary_index",index);choice->setAccessibleName(titles[index]);
            auxiliaryChoices_->addButton(choice,index);choices->addWidget(choice);
            auto* view=new QTableView;
            view->setObjectName("AuxiliaryReportTable");view->setAlternatingRowColors(false);view->setWordWrap(false);
            view->setEditTriggers(QAbstractItemView::NoEditTriggers);
            applyStudioScrollbars(view);
            view->verticalHeader()->hide();configureStudioDataTable(view);
            auxiliaryViews_.append(view);auxiliaryStack_->addWidget(view);
        }
        auxiliaryChoices_->button(0)->setChecked(true);
        box->addWidget(selector,0,Qt::AlignLeft);
        new AuxiliaryNoticePlacement(tabs_,auxiliaryPage_,box,auxiliaryNotice_);
        box->addWidget(auxiliaryStack_,1);
        connect(auxiliaryChoices_,&QButtonGroup::idClicked,this,[this](int index){
            auxiliaryStack_->setCurrentIndex(index);renderAuxiliaryPage();refreshState();
        });
        connect(tabs_,&QTabWidget::currentChanged,this,[this]{renderAuxiliaryPage();});
        auxiliarySave_=new QPushButton;setStudioFormatButton(auxiliarySave_,"txt",uiText("保存当前附加表"));
        auxiliarySave_->setProperty("hydro_action","save_auxiliary");box->addWidget(auxiliarySave_,0,Qt::AlignLeft);
        connect(auxiliarySave_,&QPushButton::clicked,this,[this,hidden]{
            const int index=currentAuxiliaryIndex();const auto file=auxiliaryFile(index);
            if(file.isEmpty())return;
            // Only the explicitly hidden regression flow can supply a path;
            // ordinary use always asks which file to save and confirms overwrite.
            QString path=hidden?auxiliarySave_->property("probe_save_path").toString():QString{};
            if(path.isEmpty())path=QFileDialog::getSaveFileName(this,uiText("保存附加表"),QDir(preferenceFileLocation("export")).filePath(file["name"].toString()),uiText("文本文件 (*.txt)"));
            if(path.isEmpty())return;const auto error=exportAuxiliary(path,index);
            status_->setMessage(error.isEmpty()?uiText("报告已保存：")+path:error,error.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);
            auxiliarySave_->setProperty("export_error",error);
        });
    }
    status_=new StudioMessage(StudioMessageRole::Feedback);status_->enableMathNotation();status_->setObjectName("HydroStatus");layout->addWidget(status_);
    auto edited=[this]{if(!result_.isEmpty()&&!session_.busy())status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);};
    if(loadingPage_){
        connect(tabs_,&QTabWidget::currentChanged,this,[this,left,loadingLayout,controlsPanel,manualCriteria,namedCriteria,edited](int index){
            auto* page=tabs_->widget(index);
            if(page!=parameterPage_&&page!=loadingPage_)return;
            const bool use=page==loadingPage_;
            if(use==useLoadingBook_)return;
            useLoadingBook_=use;
            static_cast<StabilityTabs*>(tabs_)->selectParameters(index);
            auto* target=use?loadingLayout:left;
            target->insertWidget(1,controlsPanel);
            controlsPanel->show();
            auto* criterionHost=use?namedCriteria:manualCriteria;
            criterionHost->addWidget(loadingEditor_->criteriaPanel());
            loadingEditor_->criteriaPanel()->show();
            refreshState();edited();
        });
    }
    const auto watchFields=[this,edited](const QMap<QString,QWidget*>& fields,bool loading){
        const auto changed=[this,edited,loading]{
            if(useLoadingBook_!=loading)return;
            run_->setEnabled(!session_.busy()&&refreshInputValidity());edited();
        };
        for(auto* field:fields){
            calculationInput::watch(field,this,[this,loading]{
                if(useLoadingBook_==loading)run_->setEnabled(!session_.busy()&&refreshInputValidity());
            });
            if(auto* n=qobject_cast<QDoubleSpinBox*>(field))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,changed);
            else if(auto* n=dynamic_cast<HydroNumberList*>(field))n->onChanged=[this,loading,edited](bool committed){
                if(useLoadingBook_!=loading)return;
                run_->setEnabled(!session_.busy()&&refreshInputValidity());
                if(committed)edited();
            };
            else if(auto* n=qobject_cast<QCheckBox*>(field))connect(n,&QCheckBox::toggled,this,changed);
        }
    };
    watchFields(fields_,false);
    watchFields(loadingFields_,true);
    auto* footer=new QHBoxLayout;layout->addLayout(footer);
    for(const auto& format:{QString("csv"),QString("txt"),QString("json"),QString("pdf")}){
        auto* b=new QPushButton;
        const auto refreshFormat=[b,format]{setStudioFormatButton(b,format,uiText("导出 ")+format.toUpper());};
        refreshFormat();bindUiRefresh(b,refreshFormat);
        b->setProperty("hydro_export",format);exports_.append(b);footer->addWidget(b);
        connect(b,&QPushButton::clicked,this,[this,format]{const auto path=QFileDialog::getSaveFileName(this,uiText("保存计算报告"),preferenceFileLocation("export"),format.toUpper()+" (*."+format+")");if(path.isEmpty())return;
            const auto error=exportReport(path,format);
            status_->setMessage(error.isEmpty()?uiText("报告已保存：")+path:error,error.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);});
    }
    footer->addStretch();
    connect(run_,&QPushButton::clicked,this,[this]{calculate();});connect(cancel_,&QPushButton::clicked,this,[this]{session_.cancelHydrostatics();if(session_.busy())status_->setMessage(uiText("正在取消…"),StudioMessageRole::Feedback,true);});
    refreshOutputTabs();
    refreshState();
}
void HydroDialog::changeEvent(QEvent* event){
    QDialog::changeEvent(event);
    if(event->type()!=QEvent::LanguageChange)return;
    const auto model=session_.model();
    const QString unit=model&&model->units=="metric"?"m":"ft";
    const QString mass=model&&model->units=="metric"?"t":"long ton";
    auto setLabel=[this](const QString& key,const QString& value){
        if(fieldLabels_.contains(key))fieldLabels_[key]->setText(value);
    };
    if(mode_=="single"){
        setLabel("draft",uiText("吃水 (")+unit+")");
        setLabel("heel",uiText("横倾角 (°)"));
    }
    if(mode_=="table"){
        setLabel("start",uiText("起始吃水 (")+unit+")");
        setLabel("end",uiText("终止吃水 (")+unit+")");
        setLabel("step",uiText("吃水间隔 (")+unit+")");
    }
    setLabel("trim",uiText("纵倾差 (")+unit+")");
    if(mode_=="cross"){
        setLabel("displacements",uiText("排水量 (")+mass+")");
        setLabel("angles",uiText("横倾角 (°)"));
        setLabel("lcg",uiText("重心 X (")+unit+")");
        setLabel("tcg",uiText("重心 Y (")+unit+")");
        setLabel("vcg",uiText("重心 Z (")+unit+")");
        loadingFieldLabels_["trim"]->setText(uiText("纵倾差 (")+unit+")");
        loadingFieldLabels_["angles"]->setText(uiText("横倾角 (°)"));
        translateLoadingInputs();refreshLoadingSummary();
        if(!session_.busy())run_->setEnabled(refreshInputValidity());
    }
    if(run_)run_->setText(uiText("计算"));
    if(cancel_)cancel_->setText(uiText("取消计算"));
    if(tabs_){
        tabs_->setTabText(tabs_->indexOf(parameterPage_),uiText("参数表"));
        if(loadingPage_){
            tabs_->setTabText(tabs_->indexOf(loadingPage_),uiText("参数表（装载工况）"));
            tabs_->setTabText(tabs_->indexOf(loadingEditor_->casesPage()),uiText("装载工况"));
        }
        tabs_->setTabText(tabs_->indexOf(table_),uiText("结果表"));
        const int plotIndex=tabs_->indexOf(plot_->parentWidget());
        if(plotIndex>=0)tabs_->setTabText(plotIndex,uiText("曲线"));
        if(comparison_)tabs_->setTabText(tabs_->indexOf(comparison_),uiText("工况对比"));
        if(text_){
            const int reportIndex=tabs_->indexOf(text_);
            if(reportIndex>=0)tabs_->setTabText(reportIndex,uiText("报告与基准"));
        }
        if(auxiliaryPage_){
            const int auxiliaryIndex=tabs_->indexOf(auxiliaryPage_);
            if(auxiliaryIndex>=0)tabs_->setTabText(auxiliaryIndex,uiText("附加表"));
        }
    }
    if(auxiliaryChoices_){
        const auto titles=auxiliaryTitles();
        for(int index=0;index<titles.size();++index)
            if(auto* button=qobject_cast<QToolButton*>(auxiliaryChoices_->button(index)))button->setText(titles[index]);
    }
    if(table_){
        table_->setHorizontalHeaderLabels({uiText("参数"),uiText("数值"),uiText("单位")});
        if(!result_.isEmpty())render();
    }
    for(auto* view:auxiliaryViews_){
        if(view->model()){auto* old=view->model();view->setModel(nullptr);old->deleteLater();}
    }
    renderAuxiliaryPage();
}
static QJsonObject fieldValues(const QMap<QString,QWidget*>& fields){
    QJsonObject values;
    for(auto it=fields.cbegin();it!=fields.cend();++it){
        if(auto* n=qobject_cast<QDoubleSpinBox*>(it.value()))values[it.key()]=n->value();
        else if(auto* n=dynamic_cast<HydroNumberList*>(it.value()))values[it.key()]=n->values();
        else if(auto* n=qobject_cast<QCheckBox*>(it.value()))values[it.key()]=n->isChecked();
    }
    return values;
}
QJsonObject HydroDialog::inputs()const{
    auto name=session_.model()->name;
    if(name.isEmpty())name=QFileInfo(session_.sourcePath()).completeBaseName();
    if(name.isEmpty())name=uiText("未命名船模");
    auto out=fieldValues(useLoadingBook_?loadingFields_:fields_);
    out["mode"]=mode_;out["model_name"]=name;
    if(loadingPage_)out["use_loading_book"]=useLoadingBook_;
    if(useLoadingBook_)out["loading_book"]=loadingBook_;
    else if(!loadingBook_["criteria"].toArray().isEmpty())out["gz_criteria"]=loadingBook_["criteria"];
    return out;
}
QJsonObject HydroDialog::savedInputs()const{
    auto out=fieldValues(fields_);
    if(loadingPage_){
        out["use_loading_book"]=useLoadingBook_;
        out["loading_parameters"]=fieldValues(loadingFields_);
        if(loadingBook_.contains("cases"))out["loading_book"]=loadingBook_;
        else if(loadingBook_.contains("criteria"))out["gz_criteria"]=loadingBook_["criteria"];
    }
    return out;
}
void HydroDialog::calculate(const QString& report){
    if(report.isEmpty()&&!refreshInputValidity())return;
    try{auto request=report.isEmpty()?inputs():QJsonObject{{"mode","auxiliary"},{"report",report}};
        if(report.isEmpty()&&mode_=="single")request["include_auxiliary"]=true;
        if(session_.calculateHydrostatics(request)){setProperty("hydro_finished",false);setProperty("hydro_cancelled",false);setProperty("hydro_error",QString{});status_->setMessage(uiText("正在计算…"),StudioMessageRole::Feedback,true);refreshState();}}
    catch(const std::exception& ex){showFailure(QString::fromUtf8(ex.what()));}
}
bool HydroDialog::refreshInputValidity(){
    QMap<QString,double> numeric;
    QMap<QString,QString> errors;
    QMap<QString,QString> hints;
    QSet<QString> pending;
    const auto model=session_.model();
    if(!model||!(model->length>0)){
        pending.insert("trim");
        hints["trim"]=uiText("待校验：请先设置船长。");
    }
    const bool named=useLoadingBook_;
    const auto& active=named?loadingFields_:fields_;
    for(auto it=active.cbegin();it!=active.cend();++it)if(auto* field=qobject_cast<QDoubleSpinBox*>(it.value())){
        bool valid=false;
        numeric[it.key()]=calculationInput::number(*field,valid);
        if(!valid)errors[it.key()]=uiText("请输入范围内的数值。");
    }
    if(mode_=="table"&&!errors.contains("start")&&!errors.contains("end")&&!errors.contains("step")){
        const auto draftAt=[&](double v){return QString::number(v,'g',10)+" "+(model&&model->units=="imperial"?"ft":"m");};
        const auto start=numeric["start"],end=numeric["end"],step=numeric["step"],span=end-start;
        if(span<0){
            errors["start"]=QString(uiText("须不大于当前%1（%2）。")).arg(uiText("终止吃水"),draftAt(end));
            errors["end"]=QString(uiText("须不小于当前%1（%2）。")).arg(uiText("起始吃水"),draftAt(start));
        }
        else if(span/step>199+1e-8){
            errors["start"]=QString(uiText("按当前终止吃水和间隔，须不低于 %1（最多 200 行）。")).arg(draftAt(end-199*step));
            errors["end"]=QString(uiText("按当前起始吃水和间隔，须不高于 %1（最多 200 行）。")).arg(draftAt(start+199*step));
            errors["step"]=QString(uiText("按当前起止吃水，须不小于 %1（最多 200 行）。")).arg(draftAt(span/199));
        }
    }
    bool editingList=false;
    if(mode_=="cross"){
        const auto count=[&](const QString& key,double min,double max,int limit){
            auto* list=static_cast<HydroNumberList*>(active.value(key));
            editingList|=list->isEditing();
            const auto values=list->validationValues();
            if(values.isEmpty()){errors[key]=uiText("请至少添加一个数值。");return values;}
            if(values.size()>limit){errors[key]=QString(uiText("此列表最多可填 %1 项。")).arg(limit);return values;}
            double previous=-std::numeric_limits<double>::infinity();
            for(int i=0;i<values.size();++i){
                const auto v=values[i];
                if(!v.isDouble()){
                    errors[key]=QString(uiText("第 %1 项：请输入一个数值。")).arg(i+1);break;
                }
                if(v.toDouble()<min||v.toDouble()>max||v.toDouble()<=previous){
                    errors[key]=QString(uiText("第 %1 项：数值须在 %2 至 %3 之间，并按顺序严格递增。"))
                        .arg(i+1).arg(min).arg(max);break;
                }
                previous=v.toDouble();
            }
            return values;
        };
        const auto displacements=named?loadingValues_->rowCount():count("displacements",1e-6,1e9,100).size();
        const auto angles=count("angles",-180,180,181);
        if(named&&!loadingError_.isEmpty())errors["use_loading_book"]=loadingError_;
        if(errors.isEmpty()){
            QSet<double> calculatedAngles;
            for(auto v:angles)calculatedAngles.insert(v.toDouble());
            if(named)calculatedAngles.insert(0);
            for(auto v:loadingBook_.value("criteria").toArray())calculatedAngles.insert(v.toArray()[0].toDouble());
            if(displacements*calculatedAngles.size()>500){
                if(!named)errors["displacements"]=QString(uiText("按当前横倾角项数，排水量最多可填 %1 项（最多 500 个工况）。")).arg(500/calculatedAngles.size());
                errors["angles"]=named
                    ?QString(uiText("按当前装载工况，计算用横倾角总数不得超过 %1 项（含 0° 和衡准角）。")).arg(500/displacements)
                    :QString(uiText("按当前排水量项数，横倾角最多可填 %1 项（最多 500 个工况）。")).arg(500/displacements);
            }
        }
    }
    return calculationInput::update(active,errors,hints,pending)&&!editingList&&(!loadingEditor_||loadingEditor_->criteriaValid());
}
void HydroDialog::translateLoadingInputs(){
    static_cast<QCheckBox*>(fields_.value("free_trim"))->setText(uiText("求解自由纵倾平衡"));
    static_cast<QCheckBox*>(loadingFields_.value("free_trim"))->setText(uiText("求解自由纵倾平衡"));
    loadingSummaryTitle_->setText(uiText("装载工况计算值 · 只读"));
    loadingValues_->setAccessibleName(loadingSummaryTitle_->text());
    for(auto key:{"displacements","angles"})static_cast<HydroNumberList*>(fields_.value(key))->setAccessibleLabel(fieldLabels_[key]->text());
    static_cast<HydroNumberList*>(loadingFields_["angles"])->setAccessibleLabel(loadingFieldLabels_["angles"]->text());
}
void HydroDialog::refreshLoadingSummary(){
    const bool metric=session_.model()->units=="metric";
    const QString length=metric?"m":"ft",mass=metric?"t":"long ton";
    const auto column=[&](const char* key,const QString& unit){return QString(uiText(key))+" ("+unit+")";};
    loadingValues_->setHorizontalHeaderLabels({uiText("工况名称"),column("排水量",mass),
        column("重心 X",length),column("重心 Y",length),column("重心 Z",length)});
    loadingValues_->setRowCount(0);loadingError_.clear();
    if(!loadingEditor_->weightError().isEmpty())loadingError_=uiText(loadingEditor_->weightError());
    else if(loadingBook_.value("cases").toArray().isEmpty())loadingError_=uiText("请先编辑装载工况。");
    else try{
        const auto cases=fs::loading::parse(loadingBook_);
        const double lengthUnit=metric?1.:.3048,massUnit=metric?1.:1.0160469088;
        for(const auto& load:cases){
            const auto g=load.gravity({0,0,1});const int row=loadingValues_->rowCount();loadingValues_->insertRow(row);
            auto* name=new QTableWidgetItem(load.name);name->setData(Qt::UserRole,load.name);
            name->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);loadingValues_->setItem(row,0,name);
            const double values[]{load.mass/massUnit,g.x/lengthUnit,g.y/lengthUnit,g.z/lengthUnit};
            for(int i=0;i<4;++i){
                auto* item=new QTableWidgetItem(tableNumber::display(values[i]));item->setData(Qt::UserRole,values[i]);item->setToolTip(full(values[i]));
                item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
                loadingValues_->setItem(row,i+1,item);
            }
        }
    }catch(const std::exception& e){loadingError_=uiText(QString::fromUtf8(e.what()));}
    auto* header=loadingValues_->horizontalHeader();
    loadingValues_->setVisible(loadingValues_->rowCount()>0);
    loadingSummary_->setVisible(loadingValues_->rowCount()>0);
    loadingNotice_->setMessage(loadingError_,StudioMessageRole::Warning);
    loadingValues_->setFixedHeight(header->sizeHint().height()+qMin(4,loadingValues_->rowCount())*loadingValues_->verticalHeader()->defaultSectionSize()+16);
    refreshStudioDataTable(loadingValues_);
}
void HydroDialog::refreshState(){
    const bool busy=session_.busy();for(auto* w:fields_)w->setEnabled(!busy);
    for(auto* field:loadingFields_)field->setEnabled(!busy);
    if(loadingEditor_)loadingEditor_->setEnabled(!busy);
    run_->setEnabled(!busy&&refreshInputValidity());
    cancel_->setEnabled(busy&&!session_.recordingHydroRange()&&(session_.isCalculationSession()||mode_!="cross"||(session_.hydroRunning()&&session_.hydroRunningMode()==mode_)));
    for(auto* b:exports_)b->setEnabled(!busy&&!result_.isEmpty());
    if(auxiliarySave_)auxiliarySave_->setEnabled(!busy&&!auxiliaryFile(currentAuxiliaryIndex()).isEmpty());
    if(session_.calculationQueued())status_->setMessage(uiText("排队中")+QString(" · %1").arg(session_.calculationQueuePosition()),StudioMessageRole::Feedback);
    else if(busy&&property("calculation_was_queued").toBool())status_->setMessage(uiText("正在计算…"),StudioMessageRole::Feedback,true);
    setProperty("calculation_was_queued",session_.calculationQueued());
    if(closing_&&!busy)QTimer::singleShot(0,this,[this]{QDialog::reject();});
}
void HydroDialog::showProgress(const QJsonObject& progress){
    setProperty("hydro_progress",progress);
    if(session_.hydroRunning()&&session_.hydroRunningMode()==mode_&&!session_.calculationQueued())
        status_->setMessage(uiText("正在计算…")+QString(" %1 / %2").arg(progress["completed"].toInt()).arg(progress["total"].toInt()),StudioMessageRole::Feedback,true);
}
void HydroDialog::showFailure(const QString& error,bool finished){status_->setMessage(error,StudioMessageRole::Warning);if(finished){setProperty("hydro_error",error);setProperty("hydro_finished",true);}refreshState();if(closing_&&finished)QDialog::reject();}
void HydroDialog::receive(const QJsonObject& result){
    setProperty("hydro_finished",true);setProperty("hydro_cancelled",result["cancelled"].toBool());
    if(result["cancelled"].toBool())status_->setMessage(hydroMessage(result["message"].toString())+
        (!result_.isEmpty()||!auxiliaryResults_.isEmpty()?uiText(" 已保留上一次完整结果。"):QString{}),StudioMessageRole::Feedback);
    else if(result["mode"]=="auxiliary"||result.contains("auxiliary")){
        const bool combined=result["mode"]!="auxiliary";
        if(combined){result_=result;render();auxiliaryResults_.clear();auxiliaryWarnings_.clear();for(auto* view:auxiliaryViews_){data_motion::beginTableUpdate(view);delete view->model();}}
        const auto data=result["auxiliary"].toObject();
        auto notice=skippedWaterlineNotice(data,result["units"]=="metric"?"m":"ft");
        if(data["status"]=="error"||(data["status"]=="partial"&&(notice.isEmpty()||data["message"]!="已跳过高于船体最低漏点的水线。"))){
            const auto message=hydroMessage(data["message"].toString());
            if(!message.isEmpty()&&!notice.contains(message))notice+=(notice.isEmpty()?QString():QString("\n"))+message;
        }
        const auto files=data["files"].toArray();
        for(const auto& entry:files){
            const auto name=entry.toObject()["name"].toString();
            auxiliaryResults_[name]=result;
            for(int index=0;index<auxiliaryViews_.size();++index){
                if(name!=AuxiliaryFiles[index])continue;
                if(auxiliaryViews_[index]->model())data_motion::beginTableUpdate(auxiliaryViews_[index]);
                delete auxiliaryViews_[index]->model();
                auxiliaryWarnings_[name]=notice;
            }
        }
        
        // Keep failed/all-skipped family warnings even when no export was produced.
        const auto report=result["inputs"].toObject()["report"].toString();
        const QStringList affected=combined?QStringList{"Bonjean.txt","SACs.txt","Weights.txt","Sterns.txt"}:report=="sections"?QStringList{"Bonjean.txt","SACs.txt"}:
            report=="weights"?QStringList{"Weights.txt"}:report=="sterns"?QStringList{"Sterns.txt"}:QStringList{};
        for(const auto& name:affected)auxiliaryWarnings_[name]=name=="Weights.txt"&&auxiliaryResults_.contains(name)?QString():notice;
        renderAuxiliaryPage();
        if(!combined||singleHydroIssue(result_).isEmpty()){
            if(!files.isEmpty())status_->setMessage(uiText("计算完成"),StudioMessageRole::Feedback);
            else status_->setMessage(notice.isEmpty()?hydroMessage(data["message"].toString()):notice,StudioMessageRole::Warning);
        }
    }else{result_=result;render();}
    const auto recordError=result["parameter_record_error"].toString();
    if(!recordError.isEmpty()){
        const auto issue=mode_=="single"&&!result_.isEmpty()?singleHydroIssue(result_):QString{};
        status_->setMessage(issue.isEmpty()?recordError:issue+"\n"+recordError,StudioMessageRole::Warning);
    }
    refreshOutputTabs();
    const bool changedMode=loadingPage_&&result["inputs"].toObject()["use_loading_book"].toBool()!=useLoadingBook_;
    if(!result["cancelled"].toBool()&&!changedMode&&(tabs_->currentWidget()==parameterPage_||tabs_->currentWidget()==loadingPage_)){
        const auto rows=result["rows"].toArray();
        if(std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.toObject()["status"]=="ok";}))
            tabs_->setCurrentWidget(table_);
    }
    if(!result["cancelled"].toBool()&&changedMode&&!result_.isEmpty())
        status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);
    refreshState();if(closing_)QDialog::reject();
}
void HydroDialog::reject(){
    if(mode_=="cross"||session_.isCalculationSession()){
        closing_=false;
        if(onBackground)onBackground();
        QDialog::reject();return;
    }
    if(!session_.busy()){QDialog::reject();return;}
    closing_=true;
    if(session_.recordingHydroRange())return;
    session_.cancelHydrostatics();
    status_->setMessage(uiText("取消计算后关闭…"),StudioMessageRole::Feedback,true);
}
void HydroDialog::refreshOutputTabs(){
    const auto enable=[this](QWidget* page,bool ready){tabs_->setTabEnabled(tabs_->indexOf(page),ready);};
    const auto rows=result_.value("rows").toArray();
    enable(table_,mode_=="single"?!rows.isEmpty()&&rows.first().toObject()["status"]=="ok":!rows.isEmpty());
    enable(text_,!result_.isEmpty());
    const bool chart=mode_=="cross"?stabilityChart_&&stabilityChart_->data&&!stabilityChart_->data->curves.isEmpty()
        :mode_=="table"&&std::any_of(rows.begin(),rows.end(),[](const auto& entry){return entry.toObject()["status"]=="ok";});
    enable(plot_->parentWidget(),chart);
    if(comparison_)enable(comparison_,tabs_->isTabVisible(tabs_->indexOf(comparison_))&&comparison_->columnCount()>0);
    if(auxiliaryPage_)enable(auxiliaryPage_,!auxiliaryResults_.isEmpty()||!aboveWaterGeometry(result_).isEmpty());
}
void HydroDialog::render(){
    data_motion::beginTableUpdate(table_);
    const QString u=result_["units"]=="metric"?"m":"ft",w=result_["units"]=="metric"?"t":"long ton";
    columns_={{"draft",uiText("吃水 (")+u+")"},{"heel",uiText("横倾 (°)")},{"trim",uiText("纵倾 (")+u+")"},{"displacement",uiText("排水量 (")+w+")"},{"volume",uiText("排水体积 (")+u+"³)"}};
    if(mode_=="cross")columns_={{"target_displacement",uiText("目标排水量 (")+w+")"},{"heel",uiText("横倾 (°)")},{"trim",uiText("纵倾 (")+u+")"},{"displacement",uiText("排水量 (")+w+")"},{"kn","KN ("+u+")"},{"gz","GZ ("+u+")"},{"gz_integral",uiText("GZ 积分 (")+u+"·rad)"},{"mass_residual",uiText("排水量残差 (")+w+")"},{"longitudinal_residual",uiText("纵向平衡残差 (")+u+")"}};
    else for(auto pair:QVector<QPair<QString,QString>>{{"buoyancy.0",uiText("浮心 X")},{"buoyancy.1",uiText("浮心 Y")},{"buoyancy.2",uiText("浮心 Z")},{"lwl",uiText("水线长")},{"bwl",uiText("水线宽")},{"midship_x",uiText("中剖面 X")},{"midship_area",uiText("中剖面积")},{"waterplane_area",uiText("水线面面积")},{"wetted_area",uiText("湿表面积")},{"cb","Cb"},{"cp","Cp"},{"cvp","Cvp"},{"cm","Cm"},{"cwp","Cwp"},{"waterplane_cog.0",uiText("漂心 X")},{"waterplane_cog.1",uiText("漂心 Y")},{"waterplane_ix",uiText("水线面 Ix")},{"waterplane_iy",uiText("水线面 Iy")},{"kmt",uiText("横稳心高度 KMt")},{"kml",uiText("纵稳心高度 KMl")},{"lateral_area",uiText("侧投影面积")},{"weight",uiText("船壳材料重量")}}){
        const auto k=pair.first;QString suffix;
        if(k=="weight")suffix=w;else if(k.contains("area"))suffix=u+"²";else if(k=="waterplane_ix"||k=="waterplane_iy")suffix=u+"⁴";else if(!QStringList{"cb","cp","cvp","cm","cwp"}.contains(k))suffix=u;
        columns_.append({k,pair.second+(suffix.isEmpty()?QString{}:" ("+suffix+")")});
    }
    const auto rows=result_["rows"].toArray();
    const auto reported=[&](const QString& key){return std::any_of(rows.begin(),rows.end(),[&](const QJsonValue& r){return r.toObject().value(key).isDouble();});};
    if(mode_=="cross"&&reported("pitch_deg"))columns_.insert(3,{"pitch_deg",uiText("纵倾角 (°)")});
    const bool named=result_["inputs"].toObject()["use_loading_book"].toBool();
    const QVector<QPair<QString,QString>> details={{"lcg","LCG ("+u+")"},{"tcg","TCG ("+u+")"},{"vcg","VCG ("+u+")"},{"gg0",uiText("初稳性自由液面修正")+QStringLiteral(" (")+u+")"},{"aft_draft",uiText("艉垂线吃水")+QStringLiteral(" (")+u+")"},{"forward_draft",uiText("艏垂线吃水")+QStringLiteral(" (")+u+")"},{"required_gz",uiText("要求 GZ")+QStringLiteral(" (")+u+")"},{"gz_margin",uiText("GZ 裕量")+QStringLiteral(" (")+u+")"}};
    if(named){columns_.prepend({"case_name",uiText("工况名")});columns_+=details;}
    else if(reported("required_gz"))columns_+=details.mid(details.size()-2);
    if(reported("required_gz"))columns_.append(QPair<QString,QString>{"gz_assessment",uiText("判定")});
    if(comparison_){
        tabs_->setTabVisible(tabs_->indexOf(comparison_),named);
        if(named){
            auto summary=details;
            summary.prepend({"target_displacement",uiText("重量")+QStringLiteral(" (")+w+")"});
            summary.resize(summary.size()-2);
            comparison_->clear();comparison_->setRowCount(summary.size());comparison_->setColumnCount(0);
            QStringList labels;for(const auto& field:summary)labels<<field.second;
            comparison_->setVerticalHeaderLabels(labels);
            for(const auto& entry:rows){
                const auto row=entry.toObject();
                if(!row["heel"].isDouble()||row["heel"].toDouble()!=0)continue;
                const int column=comparison_->columnCount();comparison_->insertColumn(column);
                comparison_->setHorizontalHeaderItem(column,new QTableWidgetItem(row["case_name"].toString()));
                for(int j=0;j<summary.size();++j){
                    const auto val=row[summary[j].first];
                    auto* item=new QTableWidgetItem(val.isDouble()?tableNumber::display(val.toDouble()):QStringLiteral("—"));
                    item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
                    if(val.isDouble())item->setToolTip(full(val.toDouble()));
                    comparison_->setItem(j,column,item);
                }
            }
            refreshStudioDataTable(comparison_);
        }
    }
    columns_.append(QPair<QString,QString>{"message",mode_=="cross"?uiText("求解状态"):uiText("状态")});
    auto displayColumns=columns_;
    int errors=0,partial=0;
    if(mode_!="single"){
        const auto status=displayColumns.takeLast();displayColumns.prepend(status);
        table_->clear();table_->setColumnCount(displayColumns.size());
        QStringList labels;for(const auto& c:displayColumns)labels<<c.second;
        table_->setHorizontalHeaderLabels(labels);
        const QStringList optional={"aft_draft","forward_draft","required_gz","gz_margin"};
        for(int j=0;j<displayColumns.size();++j){const auto& k=displayColumns[j].first;
            table_->setColumnHidden(j,named&&optional.contains(k)&&!reported(k=="gz_margin"?QString("required_gz"):k));}
        table_->setRowCount(rows.size());
    }
    for(int i=0;i<rows.size();++i){const auto row=rows[i].toObject();const bool valid=row["status"]=="ok";if(!valid)++errors;if(row["partial"].toBool())++partial;
        if(mode_=="single")continue;
        for(int j=0;j<displayColumns.size();++j){const auto key=displayColumns[j].first;const auto v=value(row,key);
            if(key=="message"){
                auto* item=new QTableWidgetItem;
                item->setIcon(studioIcon(valid?"MessageSuccess":"MessageCritical"));
                item->setData(Qt::UserRole,valid);
                const auto message=hydroMessage(v.toString());
                const bool incomplete=row["partial"].toBool();
                const auto status=message.isEmpty()
                    ?QString(!valid?uiText("未得到有效结果"):incomplete?uiText("部分量未提供"):
                        mode_=="cross"?uiText("已求解"):uiText("完成"))
                    :message;
                item->setData(Qt::AccessibleTextRole,status);
                if(!valid||incomplete||!message.isEmpty()){
                    item->setToolTip(notation::contains(status)
                        ?QStringLiteral("<p>%1</p>").arg(notation::html(status,table_->font().pointSizeF()).replace('\n',"<br>"))
                        :status);
                }
                table_->setItem(i,j,item);continue;
            }
            QString s=v.isDouble()?reportNumber(v.toDouble(),key):v.toString();
            if(v.isNull()&&valid)s=(key=="trim"&&row.contains("pitch_deg"))?uiText("不适用"):uiText("未提供");
            auto* item=new QTableWidgetItem(s);item->setToolTip(v.isDouble()?full(v.toDouble()):s);if(mode_=="table"&&v.isDouble())item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);if(!valid)item->setForeground(QColor(160,45,30));else if(row["partial"].toBool())item->setForeground(QColor(130,85,10));table_->setItem(i,j,item);}
    }
    const auto singleRow=mode_=="single"&&!rows.isEmpty()?rows.first().toObject():QJsonObject{};
    if(mode_=="single")renderSingleResult(singleRow);
    refreshStudioDataTable(table_);
    renderCurve(rows,u,w);
    text_->setReport(reportText(true));
    data_motion::finishTableUpdate(table_,"hydro:"+mode_);
    const auto issue=mode_=="single"?singleHydroIssue(result_):QString{};
    if(!issue.isEmpty()){
        status_->setMessage(issue,StudioMessageRole::Warning);
        return;
    }
    status_->setMessage(hydroFeedbackText(rows.size(),errors,partial,result_["elapsed_ms"].toDouble()),
        errors||partial?StudioMessageRole::Warning:StudioMessageRole::Feedback);
}
void HydroDialog::renderCurve(const QJsonArray& rows,const QString& lengthUnit,const QString& massUnit){
    if(mode_!="cross"){
        auto* plot=static_cast<HydroPlot*>(plot_);plot->result=result_;plot->update();return;
    }
    auto data=std::make_shared<CalculationChartData>();
    data->method="hydro_cross";data->captionLanguage=uiLanguage();
    QMap<QString,int> groups;
    QVector<QJsonArray> points;
    const QString xTitle=uiText("横倾角 (°)"),yTitle=QStringLiteral("GZ (")+lengthUnit+")";
    for(const auto& entry:rows){
        const auto row=entry.toObject();
        const auto mass=row["target_displacement"];
        if(!mass.isDouble())continue;
        const auto name=row["case_name"].toString();
        const auto key=full(mass.toDouble())+"|"+name;
        auto found=groups.constFind(key);
        int index;
        if(found==groups.cend()){
            index=data->curves.size();groups.insert(key,index);
            CalculationCurve curve;
            curve.source=key;
            const auto displacement=tableNumber::display(mass.toDouble())+" "+massUnit;
            if(!name.isEmpty())curve.legendSuffix=" · "+displacement;
            curve.name=name.isEmpty()?displacement:name+curve.legendSuffix;
            curve.xTitle=xTitle;curve.yTitle=yTitle;curve.axis=yTitle;
            curve.color=curveColor(index);curve.width=1.8;curve.markers=true;
            data->curves.append(std::move(curve));
            points.append(QJsonArray{});
        }else index=found.value();
        const auto y=row["gz"];
        points[index].append(QJsonArray{row["heel"],row["status"]=="ok"&&y.isDouble()?y:QJsonValue(QJsonValue::Null)});
    }
    QJsonArray series;
    for(const auto& values:points)series.append(QJsonObject{{"points",values}});
    data->decodeNumeric(QJsonObject{{"series",series}});
    data->curves.removeIf([](const auto& curve){return curve.finitePoints==0;});
    auto next=std::make_shared<CalculationChartState>();next->data=data;next->selected=-1;
    QMap<QString,bool> previous;
    if(stabilityChart_&&stabilityChart_->data){
        for(int i=0;i<stabilityChart_->data->curves.size();++i)
            previous[stabilityChart_->data->curves[i].source]=stabilityChart_->visible.value(i,true);
    }
    for(const auto& curve:data->curves)next->visible.append(previous.value(curve.source,true));
    stabilityChart_=next;
    static_cast<CalculationPlot*>(plot_)->setState(std::move(next));
}
void HydroDialog::renderSingleResult(const QJsonObject& row){
    table_->clearSpans();table_->setRowCount(0);
    const QList<QPair<QString,QStringList>> groups{
        {uiText("浮态"),{"draft","heel","trim"}},
        {uiText("排水与船型系数"),{"displacement","volume","wetted_area","cb","cp","cvp","cm"}},
        {uiText("浮心"),{"buoyancy.0","buoyancy.1","buoyancy.2"}},
        {uiText("水线面"),{"lwl","bwl","waterplane_area","cwp","waterplane_cog.0","waterplane_cog.1","waterplane_ix","waterplane_iy"}},
        {uiText("初稳性"),{"kmt","kml"}},
        {uiText("中横剖面"),{"midship_x","midship_area"}},
        {uiText("其他"),{"lateral_area","weight"}}
    };
    const bool valid=row["status"]=="ok";
    for(const auto& group:groups){
        int heading=-1;
        for(const auto& key:group.second){
            const auto field=std::find_if(columns_.cbegin(),columns_.cend(),[&](const auto& field){return field.first==key;});
            if(field==columns_.cend())continue;
            if(heading<0){
                heading=table_->rowCount();table_->insertRow(heading);auto* label=new QTableWidgetItem(group.first);
                label->setData(Qt::UserRole,"section");label->setFlags(Qt::ItemIsEnabled);
                QFont face;face.setWeight(QFont::DemiBold);label->setFont(face);
                label->setForeground(QColor("#225e55"));label->setBackground(QColor("#e5efea"));
                table_->setItem(heading,0,label);table_->setSpan(heading,0,1,3);
            }
            const auto [name,unit]=splitUnit(field->second);const auto v=value(row,key);
            const QString display=v.isDouble()?reportNumber(v.toDouble(),key):v.isString()?v.toString():QStringLiteral("—");
            const int at=table_->rowCount();table_->insertRow(at);
            const QStringList cells{name,display,unit.isEmpty()?QStringLiteral("—"):unit};
            for(int col=0;col<3;++col){
                auto* cell=new QTableWidgetItem(cells[col]);cell->setData(Qt::UserRole,key);
                cell->setTextAlignment(Qt::AlignVCenter|(col==1&&v.isDouble()?Qt::AlignRight:Qt::AlignLeft));
                if(col==1){cell->setToolTip(v.isDouble()?full(v.toDouble()):display);if(v.isDouble())cell->setData(Qt::UserRole+1,v.toDouble());}
                if(!valid||row["partial"].toBool())cell->setForeground(QColor(!valid?"#a02d1e":"#82550a"));
                table_->setItem(at,col,cell);
            }
        }
    }
}
QByteArray HydroDialog::csv()const{
    auto quote=[](QString s){return '"'+s.replace('"',"\"\"")+'"';};QStringList lines,heads;
    for(auto c:columns_)heads<<quote(c.second);
    lines<<heads.join(',');
    for(const auto& entry:result_["rows"].toArray()){
        const auto values=entry.toObject();QStringList row;
        for(const auto& column:columns_){
            const auto field=value(values,column.first);
            row<<(field.isDouble()?full(field.toDouble()):
                  quote(column.first=="message"?hydroMessage(field.toString()):field.toString()));
        }
        lines<<row.join(',');
    }
    return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();
}
QString HydroDialog::reportText(bool html)const{
    if(mode_=="table")return draftReport(html);
    const bool metric=result_["units"]=="metric";
    const QString unit=metric?"m":"ft",mass=metric?"t":"long ton";
    const auto inputs=result_["inputs"].toObject();
    const QStringList headings{uiText("参数"),uiText("数值"),uiText("单位")};
    const QList<int> columnWidths{42,34,24};
    report::Table conditions{uiText("计算基准"),headings,{}, {0,1,2},columnWidths,480};
    conditions.rows={{uiText("模型"),inputs["model_name"].toString(),{}},
        {uiText("船体最低点 Z"),reportNumber(result_["keel_z"].toDouble()),unit},
        {uiText("水密度"),tableNumber::display(result_["water_density"].toDouble()),metric?"t/m³":"lb/ft³"},
        {uiText("附体系数"),tableNumber::display(result_["appendage_coefficient"].toDouble()),{}}};
    report::Table parameters{uiText("输入"),headings,{}, {0,1,2},columnWidths,480};
    const QList<QPair<QString,QString>> fields{
        {"draft",uiText("吃水 (")+unit+")"},{"heel",uiText("横倾角 (°)")},{"trim",uiText("纵倾差 (")+unit+")"},
        {"angles",uiText("横倾角 (°)")},{"displacements",uiText("排水量 (")+mass+")"},{"lcg",uiText("重心 X (")+unit+")"},
        {"tcg",uiText("重心 Y (")+unit+")"},{"vcg",uiText("重心 Z (")+unit+")"},{"free_trim",uiText("自由纵倾")}};
    for(const auto& field:fields){
        if(inputs["use_loading_book"].toBool()&&QStringList{"displacements","lcg","tcg","vcg"}.contains(field.first))continue;
        const auto v=inputs[field.first];if(v.isUndefined()||v.isNull())continue;
        const auto [label,units]=splitUnit(field.second);
        const auto display=v.isBool()?QString(v.toBool()?uiText("是"):uiText("否")):
            v.isArray()?list(v.toArray()):reportNumber(v.toDouble());
        parameters.rows.append({label,display,units});
    }
    QString text=report::heading("FreeShip 26 — "+windowTitle(),html,true)+report::render(conditions,html)+report::render(parameters,html);
    if(inputs["use_loading_book"].toBool()){
        using namespace fs::loading;
        const auto book=unifiedBook(inputs["loading_book"].toObject(),uiText("货物"));
        const auto appendWeights=[&](const QString& title,const QJsonArray& items){
            if(items.isEmpty())return;
            report::Table table{title,{},{},{}};
            std::vector<int> columns;
            for(int col=Name;col<ColumnCount;++col){
                if(std::none_of(items.begin(),items.end(),[col](const auto& item){
                    return usesItemColumn(item.toObject()["kind"].toString(),ItemColumn(col));
                }))continue;
                table.headers.append(uiText(itemFields[col].title));
                columns.push_back(col);
            }
            for(auto entry:items){
                const auto item=entry.toObject();const auto kind=item["kind"].toString();
                QStringList cells;
                for(int col:columns){
                    const auto v=item[itemFields[col].key];
                    if(col==Kind)cells<<uiText(itemTitle(kind));
                    else if(!usesItemColumn(kind,ItemColumn(col)))cells<<QStringLiteral("—");
                    else cells<<(v.isDouble()?tableNumber::display(v.toDouble()):v.toString());
                }
                table.rows.append(cells);
            }
            text+=report::render(table,html);
        };
        appendWeights(uiText("所有工况都具有的重量项"),book["shared_items"].toArray());
        for(auto entry:book["cases"].toArray()){
            const auto row=entry.toArray();
            appendWeights(row.at(0).toString()+" · "+uiText("重量项"),row.at(1).toArray());
        }
    }
    const auto rows=result_["rows"].toArray();
    const auto cell=[](const QJsonObject& row,const QString& key){
        const auto v=value(row,key);
        if(v.isDouble())return reportNumber(v.toDouble(),key);
        if(key=="message"){
            if(!v.toString().isEmpty())return hydroMessage(v.toString());
            return QString(row["status"]=="ok"&&!row["partial"].toBool()?uiText("完成"):uiText("未得到有效结果"));
        }
        return v.isString()?v.toString():QStringLiteral("—");
    };
    if(mode_=="single"){
        const auto row=rows.isEmpty()?QJsonObject{}:rows.first().toObject();
        report::Table results{uiText("结果"),headings,{}, {0,1,2},columnWidths,480};
        for(const auto& field:columns_){
            const auto [label,units]=splitUnit(field.second);
            if(row["status"]!="ok"&&value(row,field.first).isUndefined()&&field.first!="message")continue;
            results.rows.append({label,cell(row,field.first),units});
        }
        text+=report::render(results,html);
    }else{
        if(inputs["use_loading_book"].toBool()){
            const auto book=inputs["loading_book"].toObject();
            for(auto c:book["cases"].toArray()){
                const auto name=c.toArray()[0].toString().trimmed();
                report::Table eq{name+" · "+uiText("正浮角 0°"),headings,{}, {}};
                report::Table curve{name+" · GZ",{uiText("横倾角 (°)"),"GZ ("+unit+")",uiText("状态")},{},{}};
                report::Table criteria{uiText("自定义 GZ 衡准"),{uiText("横倾角 (°)"),uiText("要求值")+QString(" (")+unit+")",uiText("计算值")+QString(" (")+unit+")",uiText("裕量")+QString(" (")+unit+")",uiText("判定")},{},{}};
                for(auto item:rows){auto r=item.toObject();if(r["case_name"]!=name)continue;
                    if(r["heel"].toDouble()==0)for(auto col:columns_)if(QStringList{"target_displacement","lcg","tcg","vcg","gg0","aft_draft","forward_draft"}.contains(col.first)){auto [title,u]=splitUnit(col.second);eq.rows.append({title,cell(r,col.first),u});}
                    curve.rows.append({cell(r,"heel"),cell(r,"gz"),cell(r,"message")});
                    if(r.contains("required_gz")){auto margin=r["gz_margin"];criteria.rows.append({cell(r,"heel"),cell(r,"required_gz"),cell(r,"gz"),cell(r,"gz_margin"),!margin.isDouble()?uiText("未评估"):margin.toDouble()>=0?uiText("满足"):uiText("不满足")});}
                }
                text+=report::render(eq,html)+report::render(curve,html);if(!criteria.rows.isEmpty())text+=report::render(criteria,html);
            }return text;
        }
        report::Table results{uiText("结果"),{},{},{int(columns_.size()-1)}};
        for(const auto& field:columns_)results.headers.append(field.second);
        for(const auto& entry:rows){
            QStringList cells;const auto row=entry.toObject();
            for(const auto& field:columns_)cells.append(cell(row,field.first));
            results.rows.append(cells);
        }
        text+=report::render(results,html);
    }
    return text;
}
QString HydroDialog::draftReport(bool html)const{
    const bool metric=result_["units"]=="metric";
    const QString unit=metric?"m":"ft";
    const auto inputs=result_["inputs"].toObject();
    const auto summary=tableNumber::display;
    const auto length=[&](const char* key){return summary(inputs[key].toDouble());};
    const auto rows=result_["rows"].toArray();
    QList<DraftTable> tables{
        {uiText("排水量与水线尺度"),{"draft","displacement","volume","lwl","bwl","weight"}},
        {uiText("浮心与稳心"),{"draft","buoyancy.0","buoyancy.1","buoyancy.2","kmt","kml"}},
        {uiText("剖面与面积"),{"draft","midship_x","midship_area","waterplane_area","wetted_area","lateral_area"}},
        {uiText("水线面特性"),{"draft","waterplane_cog.0","waterplane_cog.1","waterplane_ix","waterplane_iy"}},
        {uiText("船型系数"),{"draft","cb","cp","cvp","cm","cwp"}},
    };
    const auto columnHeading=[this](const QString& key){
        for(const auto& column:columns_)if(column.first==key)return column.second;
        return key;
    };
    report::Table conditions{uiText("计算基准"),{uiText("参数"),uiText("数值"),uiText("单位")},{},{0,1,2},{42,34,24},480};
    conditions.rows={{uiText("模型"),inputs["model_name"].toString().simplified()},
        {uiText("起始吃水"),length("start"),unit},{uiText("终止吃水"),length("end"),unit},{uiText("吃水间隔"),length("step"),unit},
        {uiText("横倾"),"0","°"},{uiText("纵倾差"),length("trim"),unit},
        {uiText("水密度"),summary(result_["water_density"].toDouble()),metric?"t/m³":"lb/ft³"},
        {uiText("附体系数"),summary(result_["appendage_coefficient"].toDouble())}};
    QString report=report::heading("FreeShip 26 — "+windowTitle(),html,true)+report::render(conditions,html);
    report::Table issues{uiText("需查看的工况"),{columnHeading("draft"),columnHeading("message")},{},{1}};
    for(const auto& entry:rows){
        const auto row=entry.toObject();
        if(row["status"]=="ok"&&!row["partial"].toBool())continue;
        auto message=hydroMessage(row["message"].toString()).simplified();
        if(message.isEmpty())message=row["status"]=="ok"?uiText("部分量未提供"):uiText("未得到有效结果");
        issues.rows.append({reportNumber(row["draft"].toDouble(),"draft"),message});
    }
    report+=report::render(issues,html);
    for(const auto& group:tables){
        report::Table table{group.title,{},{},{}};
        for(const auto& key:group.keys)table.headers.append(columnHeading(key));
        for(const auto& entry:rows){
            const auto row=entry.toObject();QStringList cells;
            for(const auto& key:group.keys){
                const auto field=value(row,key);
                cells.append(field.isDouble()&&(key=="draft"||row["status"]=="ok")?
                    reportNumber(field.toDouble(),key):QStringLiteral("—"));
            }
            table.rows.append(cells);
        }
        report+=report::render(table,html);
    }
    return report;
}
int HydroDialog::currentAuxiliaryIndex()const{
    return auxiliaryStack_?auxiliaryStack_->currentIndex():-1;
}
void HydroDialog::renderAuxiliaryPage(){
    if(!auxiliaryPage_)return;
    if(tabs_->currentWidget()!=auxiliaryPage_){auxiliaryNotice_->hide();return;}
    const int index=currentAuxiliaryIndex();
    if(index<0||index>=auxiliaryViews_.size())return;
    auto* table=auxiliaryViews_[index];
    const QString name=AuxiliaryFiles[index];
    if(name==AboveWaterFile){
        auxiliaryNotice_->setText({});auxiliaryNotice_->hide();
        const auto geometry=aboveWaterGeometry(result_);
        if(geometry.isEmpty()||table->model())return;
        table->setModel(new AuxiliaryTable(geometry,name,result_["units"]=="metric",{},table));
        data_motion::finishTableUpdate(table,"auxiliary:"+name);
        refreshStudioDataTable(table);
        return;
    }
    const auto notice=auxiliaryWarnings_.value(name);
    auxiliaryNotice_->setText(notice);auxiliaryNotice_->setVisible(!notice.isEmpty());
    const auto data=auxiliaryResults_.value(name);
    if(data.isEmpty()||table->model())return;
    QStringList layerLabels;for(const auto& layer:session_.model()->layers)layerLabels.append(layer.name);
    table->setModel(new AuxiliaryTable(data["auxiliary"].toObject(),name,data["units"]=="metric",layerLabels,table));
    data_motion::finishTableUpdate(table,"auxiliary:"+name);
    refreshStudioDataTable(table);
}
QJsonObject HydroDialog::auxiliaryFile(int index)const{
    if(index<0||index>=auxiliaryViews_.size())return {};
    const QString name=AuxiliaryFiles[index];
    if(name==AboveWaterFile){
        const auto table=aboveWaterTable(aboveWaterGeometry(result_),result_["units"]=="metric");
        if(table.rows.isEmpty())return {};
        return {{"name",name},{"text",report::render(table,false)}};
    }
    for(const auto& entry:auxiliaryResults_.value(name)["auxiliary"].toObject()["files"].toArray())
        if(entry.toObject()["name"]==name)return entry.toObject();
    return {};
}
bool HydroDialog::selectAuxiliary(int index){
    if(!auxiliaryStack_||index<0||index>=auxiliaryViews_.size()||
        !tabs_->isTabEnabled(tabs_->indexOf(auxiliaryPage_)))return false;
    auxiliaryStack_->setCurrentIndex(index);auxiliaryChoices_->button(index)->setChecked(true);
    tabs_->setCurrentWidget(auxiliaryPage_);renderAuxiliaryPage();refreshState();return true;
}
QString HydroDialog::exportAuxiliary(const QString& path,int index)const{
    const auto entry=auxiliaryFile(index);if(entry.isEmpty())return uiText("没有可保存的附加表。");
    const auto bytes=entry["text"].toString().toUtf8();QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return uiText("附加表保存失败，原文件未替换：")+file.errorString();
    return {};
}
QString HydroDialog::exportReport(const QString& path,const QString& format)const{
    if(result_.isEmpty())return uiText("尚无完整计算结果。");
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return uiText("无法写入报告：")+file.errorString();
    if(format=="pdf"){
        QPdfWriter writer(&file);writer.setPageSize(QPageSize(QPageSize::A4));writer.setResolution(120);
        QTextDocument doc;
        if(mode_!="single")writer.setPageOrientation(QPageLayout::Landscape);
        doc.setDefaultFont(QFont("Microsoft YaHei UI",9));
        report::setHtml(&doc,reportText(true));
        doc.print(&writer);
    }else{
        QByteArray bytes;if(format=="csv")bytes=csv();else if(format=="json")bytes=QJsonDocument(result_).toJson(QJsonDocument::Indented);else if(format=="txt")bytes=reportText(false).toUtf8();else return uiText("未知报告格式。");
        if(file.write(bytes)!=bytes.size())return uiText("报告写入未完成，原文件未替换。");
    }
    if(!file.commit())return uiText("无法完成报告保存：")+file.errorString();
    return {};
}
