#include "app/ui_language.h"
#include "preferences_store.h"
#include "hydro_dialog.h"
#include "calculation_input_status.h"
#include "studio_ui.h"
#include "data_motion.h"
#include "studio_table.h"
#include "report_browser.h"
#include "feedback_text.h"
#include "studio_read_ui.h"
#include <limits>
#include <QResizeEvent>
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
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
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
#include <QSplitter>
#include <QTimer>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
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
QString draftValue(double number,const QString& key,double step){
    const bool coefficient=key=="cb"||key=="cp"||key=="cvp"||key=="cm"||key=="cwp";
    const int decimals=coefficient?4:key=="draft"&&step<.001?6:3;
    const double magnitude=std::abs(number);
    if(magnitude>=1e8||(magnitude>0&&magnitude<.5*std::pow(10.,-decimals)))
        return QString::number(number,'e',decimals);
    return QString::number(number==0?0:number,'f',decimals);
}
QString list(const QJsonArray& data,int precision=12){QStringList parts;for(const auto& v:data)parts<<QString::number(v.toDouble(),'g',precision);return parts.join(", ");}
QJsonArray numbers(QString text){
    text.replace(QChar(0xff0c),',');text.replace(';',',');const auto parts=text.split(',',Qt::SkipEmptyParts);QJsonArray out;
    if(parts.isEmpty()||parts.size()>181)throw std::runtime_error(uiText("请输入用逗号分隔的数值列表。"));
    for(auto s:parts){bool ok=false;double v=QLocale::c().toDouble(s.trimmed(),&ok);if(!ok||!std::isfinite(v))throw std::runtime_error(uiText("列表中存在无效数值。"));out.append(v);}return out;
}
QJsonValue value(const QJsonObject& row,const QString& key){
    const auto parts=key.split('.');if(parts.size()!=2)return row[key];const auto a=row[parts[0]].toArray();const int i=parts[1].toInt();return i<a.size()?a[i]:QJsonValue{};
}
QString hydroMessage(const QString& source){
    // The solver may group several independent diagnostics into one notice.
    auto lines=source.split('\n');
    for(auto& line:lines)if(!line.trimmed().isEmpty())line=uiText(line.trimmed());
    return lines.join('\n');
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
    static const QStringList coefficients{"cb","cp","cvp","cm","cwp"};
    const double magnitude=std::abs(value);
    if((key=="buoyancy.1"||key=="waterplane_cog.1")&&magnitude<1e-12)return "0";
    const int decimals=coefficients.contains(key)?6:3;
    if(magnitude>=1e9||(magnitude>0&&magnitude<.5*std::pow(10.,-decimals)))
        return QString::number(value,'g',7);
    auto display=QString::number(value==0?0:value,'f',decimals);
    while(display.endsWith('0'))display.chop(1);
    if(display.endsWith('.'))display.chop(1);
    return display;
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
            for(const auto& row:waterlines_)headers_<<QString(uiText("吃水"))+" = "+reportNumber(row.toObject()["draft"].toDouble())+" "+unit+" ("+unit+"²)";
        }else{
            rowCount_=int(waterlines_.size());
            headers_<<uiText("水线 Z")+length;
            if(file_=="Sterns.txt")headers_<<uiText("尾端 X")+length<<uiText("首端 X")+length;
            else for(const auto& station:result["stations"].toArray())
                headers_<<"X = "+reportNumber(station.toDouble())+" "+unit+" ("+unit+"²)";
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
class HydroPlot final:public QWidget {
public:
    QJsonObject result;QString lengthUnit;
    explicit HydroPlot(QWidget* p):QWidget(p){setMinimumSize(400,280);}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),QColor(250,252,253));p.setRenderHint(QPainter::Antialiasing);
        QRectF area=QRectF(rect()).adjusted(65,35,-25,-55);double xmin=1e300,xmax=-1e300,ymin=0,ymax=0;
        auto rows=result["rows"].toArray();const bool cross=result["mode"]=="cross";
        auto x=[&](QJsonObject r){return r[cross?"heel":"draft"].toDouble();};auto y=[&](QJsonObject r){return r[cross?"gz":"displacement"].toDouble();};
        for(auto v:rows){auto r=v.toObject();if(r["status"]!="ok")continue;xmin=std::min(xmin,x(r));xmax=std::max(xmax,x(r));ymin=std::min(ymin,y(r));ymax=std::max(ymax,y(r));}
        if(xmin>xmax){const QString text=uiText("尚无可绘制的结果");const qreal w=qMin(460,width()-24);const qreal h=studioMessageHeight(p,text,w);paintStudioMessage(p,QRectF((width()-w)/2,(height()-h)/2,w,h),text,StudioMessageRole::Explanation);return;}
        if(xmax==xmin)xmax=xmin+1;
        if(ymax==ymin)ymax=ymin+1;
        auto map=[&](double a,double b){return QPointF(area.left()+(a-xmin)/(xmax-xmin)*area.width(),area.bottom()-(b-ymin)/(ymax-ymin)*area.height());};
        p.setPen(QColor(215,222,229));
        for(int i=0;i<=5;++i){double xx=xmin+(xmax-xmin)*i/5,yy=ymin+(ymax-ymin)*i/5;p.drawLine(map(xx,ymin),map(xx,ymax));p.drawLine(map(xmin,yy),map(xmax,yy));
            p.setPen(QColor(65,75,85));p.drawText(QRectF(map(xx,ymin)+QPointF(-35,5),QSizeF(70,20)),Qt::AlignCenter,QString::number(xx,'g',5));
            p.drawText(QRectF(0,map(xmin,yy).y()-10,60,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(yy,'g',5));p.setPen(QColor(215,222,229));}
        p.setPen(QColor(45,55,65));p.drawText(QRectF(0,5,width(),25),Qt::AlignCenter,cross?"GZ ("+lengthUnit+")":uiText("排水量 (")+(result["units"]=="metric"?QString("t"):QString("long ton"))+")");
        p.drawText(QRectF(0,height()-30,width(),25),Qt::AlignCenter,cross?uiText("横倾角 (°)"):uiText("吃水 (")+lengthUnit+")");
        QPainterPath path;bool connected=false;double mass=0;int curve=cross?-1:0;
        auto flush=[&]{p.setPen(QPen(curveColor(std::max(0,curve)),1.8));p.drawPath(path);path=QPainterPath();};
        for(auto v:rows){auto r=v.toObject();double current=r["target_displacement"].toDouble();
            if(cross&&current!=mass){flush();connected=false;mass=current;++curve;}
            if(r["status"]!="ok"){connected=false;continue;}auto pt=map(x(r),y(r));
            if(connected)path.lineTo(pt);else path.moveTo(pt);connected=true;p.setPen(curveColor(std::max(0,curve)));p.drawEllipse(pt,2,2);
        }flush();
    }
};
}
HydroDialog::HydroDialog(ModelSession& session,const QString& mode,const QJsonObject& defaults,bool hidden,QWidget* parent)
    :QDialog(parent),session_(session),mode_(mode){
    setStudioWindowIcon(this,"Hydrostatics");
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setProperty("hydro_dialog",mode);resize(1180,760);
    const QString titleSource=mode=="single"?"设计静水力":mode=="table"?"静水力吃水表":"稳性曲线与平衡";
    setProperty("calculation_title_source",titleSource);setWindowTitle(uiText(titleSource));
    auto* layout=new QVBoxLayout(this);auto* splitter=new QSplitter;layout->addWidget(splitter,1);
    auto* parameters=new QWidget;auto* left=new QVBoxLayout(parameters);auto* form=new QFormLayout;left->addLayout(form);
    form->setVerticalSpacing(14);form->setHorizontalSpacing(16);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const auto m=session.model();const QString unit=m->units=="metric"?"m":"ft",mass=m->units=="metric"?"t":"long ton";
    auto addField=[&](const QString& key,const QString& title,QWidget* field){
        field->setProperty("field",key);fields_[key]=field;
        if(title.isEmpty())form->addRow(calculationInput::caption(field,field,key));
        else{
            auto* label=new QLabel(title);label->setWordWrap(true);label->setBuddy(field);
            label->setProperty("hydro_field_label",key);
            fieldLabels_[key]=label;
            form->addRow(calculationInput::caption(label,field,key),field);
        }
    };
    auto num=[&](QString key,QString label,double initial,double low=-1e9,double high=1e9){auto* n=new HydroNumber;n->setDecimals(12);n->setRange(low,high);n->setKeyboardTracking(false);n->setSingleStep(.1);n->setValue(initial);addField(key,label,n);};
    auto line=[&](QString key,QString label,QString initial){auto* field=new QLineEdit(initial);field->setMaxLength(8192);addField(key,label,field);};
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
        auto displacements=defaults["displacements"].toArray();if(displacements.isEmpty())displacements.append(defaults["design_displacement"].toDouble(1));
        auto angles=defaults["angles"].toArray();if(angles.isEmpty())angles=QJsonArray{0,5,10,15,20,30,40,50,60};
        const auto savedList=[&](const QString& key,const QJsonArray& fallback){
            const auto v=saved[key];
            return v.isString()?v.toString():v.isArray()?list(v.toArray(),17):list(fallback);
        };
        line("displacements",uiText("排水量 (")+mass+")",savedList("displacements",displacements));
        line("angles",uiText("横倾角 (°)"),savedList("angles",angles));
        num("lcg",uiText("重心 X (")+unit+")",saved["lcg"].toDouble((m->minimum.x+m->maximum.x)/2));
        num("tcg",uiText("重心 Y (")+unit+")",saved["tcg"].toDouble());
        num("vcg",uiText("重心 Z (")+unit+")",saved["vcg"].toDouble(defaults["vcg"].toDouble()));
        check("free_trim",uiText("求解自由纵倾平衡"),saved["free_trim"].toBool(defaults["free_trim"].toBool()));
    }
    run_=new QPushButton(uiText("计算"));run_->setProperty("hydro_action","calculate");cancel_=new QPushButton(uiText("取消计算"));cancel_->setProperty("hydro_action","cancel");
    auto* controls=new QHBoxLayout;controls->addWidget(run_);controls->addWidget(cancel_);left->addLayout(controls);
    left->addStretch();auto* parameterScroll=new QScrollArea;parameterScroll->setWidgetResizable(true);parameterScroll->setFrameShape(QFrame::NoFrame);
    parameterScroll->setWidget(parameters);parameterScroll->setMinimumWidth(380);parameterScroll->setMaximumWidth(500);splitter->addWidget(parameterScroll);
    tabs_=new QTabWidget;splitter->addWidget(tabs_);splitter->setStretchFactor(1,1);splitter->setSizes({430,730});
    splitter->setSizes({360,720});
    table_=mode=="single"?new SingleHydroTable:new QTableWidget;table_->setProperty("hydro_result_table",true);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(false);tabs_->addTab(table_,uiText("结果表"));
    applyStudioScrollbars(table_);
    configureStudioDataTable(table_,mode=="single",mode=="single"?0:-1);
    auto* plotPage=new QWidget;plotPage->setObjectName("HydroCurvePage");
    auto* plotLayout=new QVBoxLayout(plotPage);plotLayout->setContentsMargins(0,0,0,0);
    auto* plot=new HydroPlot(plotPage);plot->lengthUnit=unit;plot_=plot;plotLayout->addWidget(plot,1);
    auto* legendScroll=new QScrollArea;legendScroll->setWidgetResizable(true);legendScroll->setMaximumHeight(72);legendScroll->setFrameShape(QFrame::NoFrame);
    legend_=new QLabel;legend_->setProperty("studio_content_role","plot_legend");legend_->setWordWrap(true);legend_->setMargin(5);legendScroll->setWidget(legend_);legendScroll->setVisible(mode=="cross");plotLayout->addWidget(legendScroll);
    tabs_->addTab(plotPage,uiText("曲线"));
    tabs_->setTabVisible(tabs_->indexOf(plotPage),mode!="single");
    text_=new report::Browser;text_->setObjectName("HydroReport");tabs_->addTab(text_,uiText("报告与基准"));
    if(mode=="single"){
        auxiliaryPage_=new QWidget;auto* box=new QVBoxLayout(auxiliaryPage_);
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
    for(auto* field:fields_){
        calculationInput::watch(field,this,[this]{run_->setEnabled(!session_.busy()&&refreshInputValidity());});
        if(auto* n=qobject_cast<QDoubleSpinBox*>(field))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,edited);
        else if(auto* n=qobject_cast<QLineEdit*>(field))connect(n,&QLineEdit::textEdited,this,edited);
        else if(auto* n=qobject_cast<QCheckBox*>(field))connect(n,&QCheckBox::toggled,this,edited);
    }
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
        setLabel("free_trim",uiText("求解自由纵倾平衡"));
    }
    if(run_)run_->setText(uiText("计算"));
    if(cancel_)cancel_->setText(uiText("取消计算"));
    if(tabs_){
        const int resultIndex=0;
        if(resultIndex<tabs_->count())tabs_->setTabText(resultIndex,uiText("结果表"));
        const int plotIndex=tabs_->indexOf(plot_);
        if(plotIndex>=0)tabs_->setTabText(plotIndex,uiText("曲线"));
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
QJsonObject HydroDialog::inputs()const{
    auto name=session_.model()->name;
    if(name.isEmpty())name=QFileInfo(session_.sourcePath()).completeBaseName();
    if(name.isEmpty())name=uiText("未命名船模");
    QJsonObject out{{"mode",mode_},{"model_name",name}};
    for(auto it=fields_.begin();it!=fields_.end();++it){
        if(auto* n=qobject_cast<QDoubleSpinBox*>(it.value()))out[it.key()]=n->value();
        else if(auto* n=qobject_cast<QLineEdit*>(it.value()))out[it.key()]=numbers(n->text());
        else if(auto* n=qobject_cast<QCheckBox*>(it.value()))out[it.key()]=n->isChecked();
    }return out;
}
QJsonObject HydroDialog::savedInputs()const{
    QJsonObject out;
    for(auto it=fields_.cbegin();it!=fields_.cend();++it){
        if(auto* n=qobject_cast<QDoubleSpinBox*>(it.value()))out[it.key()]=n->value();
        else if(auto* n=qobject_cast<QLineEdit*>(it.value()))out[it.key()]=n->text();
        else if(auto* n=qobject_cast<QCheckBox*>(it.value()))out[it.key()]=n->isChecked();
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
    for(auto it=fields_.cbegin();it!=fields_.cend();++it)if(auto* field=qobject_cast<QDoubleSpinBox*>(it.value())){
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
    if(mode_=="cross"){
        const auto count=[&](const QString& key,double min,double max,int limit){
            try{
                const auto values=numbers(qobject_cast<QLineEdit*>(fields_.value(key))->text());
                if(values.size()>limit)errors[key]=QString(uiText("此列表最多可填 %1 项。")).arg(limit);
                double previous=-std::numeric_limits<double>::infinity();
                for(const auto v:values){
                    if(v.toDouble()<min||v.toDouble()>max||v.toDouble()<=previous){
                        errors[key]=QString(uiText("数值须在 %1 至 %2 之间，并严格递增。")).arg(min).arg(max);
                        break;
                    }
                    previous=v.toDouble();
                }
                return values.size();
            }catch(const std::exception& e){errors[key]=QString::fromUtf8(e.what());return qsizetype(0);}
        };
        const auto displacements=count("displacements",1e-6,1e9,100);
        const auto angles=count("angles",-180,180,181);
        if(errors.isEmpty()&&displacements*angles>500){
            errors["displacements"]=QString(uiText("按当前横倾角项数，排水量最多可填 %1 项（最多 500 个工况）。")).arg(500/angles);
            errors["angles"]=QString(uiText("按当前排水量项数，横倾角最多可填 %1 项（最多 500 个工况）。")).arg(500/displacements);
        }
    }
    return calculationInput::update(fields_,errors,hints,pending);
}
void HydroDialog::refreshState(){
    const bool busy=session_.busy();for(auto* w:fields_)w->setEnabled(!busy);run_->setEnabled(!busy&&refreshInputValidity());
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
        if(!files.isEmpty())status_->setMessage(uiText("计算完成"),StudioMessageRole::Feedback);
        else status_->setMessage(notice.isEmpty()?hydroMessage(data["message"].toString()):notice,StudioMessageRole::Warning);
    }else{result_=result;render();}
    if(!result["parameter_record_error"].toString().isEmpty())status_->setMessage(result["parameter_record_error"].toString(),StudioMessageRole::Warning);
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
    columns_.append(QPair<QString,QString>{"message",uiText("状态")});
    const auto rows=result_["rows"].toArray();int errors=0,partial=0;
    if(mode_!="single"){
        table_->clear();table_->setColumnCount(columns_.size());QStringList labels;for(auto c:columns_)labels<<c.second;table_->setHorizontalHeaderLabels(labels);
        table_->setRowCount(rows.size());
    }
    const double draftStep=result_["inputs"].toObject()["step"].toDouble();
    QStringList legend;double previousMass=0;int curve=0;
    for(auto v:rows){const auto mass=v.toObject()["target_displacement"].toDouble();if(mode_=="cross"&&mass!=previousMass){
        legend<<QString("<span style='color:%1'>● %2 %3</span>").arg(curveColor(curve++).name(),QString::number(mass,'g',9),w);previousMass=mass;
    }}legend_->setText(notation::htmlDocument(legend.join(" &nbsp; &nbsp; ")));
    for(int i=0;i<rows.size();++i){const auto row=rows[i].toObject();const bool valid=row["status"]=="ok";if(!valid)++errors;if(row["partial"].toBool())++partial;
        if(mode_=="single")continue;
        for(int j=0;j<columns_.size();++j){const auto key=columns_[j].first;const auto v=value(row,key);QString s=v.isDouble()?(mode_=="table"?draftValue(v.toDouble(),key,draftStep):QString::number(v.toDouble(),'g',9)):v.toString();
            if(key=="message")s=valid&&s.isEmpty()?QString(uiText("完成")):hydroMessage(s);
            if(v.isNull()&&valid)s=uiText("未提供");
            auto* item=new QTableWidgetItem(s);item->setToolTip(v.isDouble()?full(v.toDouble()):s);if(mode_=="table"&&v.isDouble())item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);if(!valid)item->setForeground(QColor(160,45,30));else if(row["partial"].toBool())item->setForeground(QColor(130,85,10));table_->setItem(i,j,item);}
    }
    if(mode_=="single")renderSingleResult(rows.isEmpty()?QJsonObject{}:rows.first().toObject());
    refreshStudioDataTable(table_);
    auto* plot=static_cast<HydroPlot*>(plot_);plot->result=result_;plot->update();
    text_->setReport(reportText(true));
    data_motion::finishTableUpdate(table_,"hydro:"+mode_);
    status_->setMessage(hydroFeedbackText(rows.size(),errors,partial,result_["elapsed_ms"].toDouble()),
        errors||partial?StudioMessageRole::Warning:StudioMessageRole::Feedback);
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
        {uiText("其他"),{"lateral_area","weight"}},
        {uiText("计算状态"),{"message"}}
    };
    const bool valid=row["status"]=="ok";
    for(const auto& group:groups){
        int heading=-1;
        for(const auto& key:group.second){
            // Routine success is already reported in the completion banner.
            // Preserve row-specific warnings/errors and every existing quantity.
            if(key=="message"&&valid&&!row["partial"].toBool()&&row[key].toString().isEmpty())continue;
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
            QString display=v.isDouble()?QString::number(v.toDouble(),'g',9):v.isString()?v.toString():QStringLiteral("—");
            if(key=="message")display=v.toString().isEmpty()?
                QString(valid?uiText("部分量未提供"):uiText("未得到有效结果")):hydroMessage(v.toString());
            const int at=table_->rowCount();table_->insertRow(at);
            const QStringList cells{name,display,key=="message"?QString{}:unit.isEmpty()?QStringLiteral("—"):unit};
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
        {uiText("水密度"),QString::number(result_["water_density"].toDouble(),'g',7),metric?"t/m³":"lb/ft³"},
        {uiText("附体系数"),QString::number(result_["appendage_coefficient"].toDouble(),'g',7),{}}};
    report::Table parameters{uiText("输入"),headings,{}, {0,1,2},columnWidths,480};
    const QList<QPair<QString,QString>> fields{
        {"draft",uiText("吃水 (")+unit+")"},{"heel",uiText("横倾角 (°)")},{"trim",uiText("纵倾差 (")+unit+")"},
        {"angles",uiText("横倾角 (°)")},{"displacements",uiText("排水量 (")+mass+")"},{"lcg",uiText("重心 X (")+unit+")"},
        {"tcg",uiText("重心 Y (")+unit+")"},{"vcg",uiText("重心 Z (")+unit+")"},{"free_trim",uiText("自由纵倾")}};
    for(const auto& field:fields){
        const auto v=inputs[field.first];if(v.isUndefined()||v.isNull())continue;
        const auto [label,units]=splitUnit(field.second);
        const auto display=v.isBool()?QString(v.toBool()?uiText("是"):uiText("否")):
            v.isArray()?list(v.toArray()):reportNumber(v.toDouble());
        parameters.rows.append({label,display,units});
    }
    QString text=report::heading("FreeShip 26 — "+windowTitle(),html,true)+report::render(conditions,html)+report::render(parameters,html);
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
    const double step=inputs["step"].toDouble();
    const auto summary=[](double number){return QString::number(number,'g',8);};
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
        issues.rows.append({draftValue(row["draft"].toDouble(),"draft",step),message});
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
                    draftValue(field.toDouble(),key,step):QStringLiteral("—"));
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
    if(!auxiliaryStack_||index<0||index>=auxiliaryViews_.size())return false;
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
