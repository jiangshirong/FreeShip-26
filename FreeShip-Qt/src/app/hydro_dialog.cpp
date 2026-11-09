#include "preferences_store.h"
#include "hydro_dialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
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
QString list(const QJsonArray& data){QStringList parts;for(const auto& v:data)parts<<QString::number(v.toDouble(),'g',12);return parts.join(", ");}
QJsonArray numbers(QString text){
    text.replace(QChar(0xff0c),',');text.replace(';',',');const auto parts=text.split(',',Qt::SkipEmptyParts);QJsonArray out;
    if(parts.isEmpty()||parts.size()>181)throw std::runtime_error("请输入用逗号分隔的数值列表。");
    for(auto s:parts){bool ok=false;double v=QLocale::c().toDouble(s.trimmed(),&ok);if(!ok||!std::isfinite(v))throw std::runtime_error("列表中存在无效数值。");out.append(v);}return out;
}
QJsonValue value(const QJsonObject& row,const QString& key){
    const auto parts=key.split('.');if(parts.size()!=2)return row[key];const auto a=row[parts[0]].toArray();const int i=parts[1].toInt();return i<a.size()?a[i]:QJsonValue{};
}
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
        if(xmin>xmax){p.drawText(rect(),Qt::AlignCenter,"尚无可绘制的结果");return;}
        if(xmax==xmin)xmax=xmin+1;
        if(ymax==ymin)ymax=ymin+1;
        auto map=[&](double a,double b){return QPointF(area.left()+(a-xmin)/(xmax-xmin)*area.width(),area.bottom()-(b-ymin)/(ymax-ymin)*area.height());};
        p.setPen(QColor(215,222,229));
        for(int i=0;i<=5;++i){double xx=xmin+(xmax-xmin)*i/5,yy=ymin+(ymax-ymin)*i/5;p.drawLine(map(xx,ymin),map(xx,ymax));p.drawLine(map(xmin,yy),map(xmax,yy));
            p.setPen(QColor(65,75,85));p.drawText(QRectF(map(xx,ymin)+QPointF(-35,5),QSizeF(70,20)),Qt::AlignCenter,QString::number(xx,'g',5));
            p.drawText(QRectF(0,map(xmin,yy).y()-10,60,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(yy,'g',5));p.setPen(QColor(215,222,229));}
        p.setPen(QColor(45,55,65));p.drawText(QRectF(0,5,width(),25),Qt::AlignCenter,cross?"GZ ("+lengthUnit+")":"排水量 ("+(result["units"]=="metric"?QString("t"):QString("long ton"))+")");
        p.drawText(QRectF(0,height()-30,width(),25),Qt::AlignCenter,cross?"横倾角 (°)":"吃水 ("+lengthUnit+")");
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
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setProperty("hydro_dialog",mode);resize(1080,690);
    setWindowTitle(mode=="single"?"设计静水力":mode=="table"?"静水力吃水表":"稳性曲线与平衡");
    auto* layout=new QVBoxLayout(this);auto* splitter=new QSplitter;layout->addWidget(splitter,1);
    auto* parameters=new QWidget;auto* left=new QVBoxLayout(parameters);auto* form=new QFormLayout;left->addLayout(form);
    const auto m=session.model();const QString unit=m->units=="metric"?"m":"ft",mass=m->units=="metric"?"t":"long ton";
    auto num=[&](QString key,QString label,double initial,double low=-1e9,double high=1e9){auto* n=new HydroNumber;n->setDecimals(12);n->setRange(low,high);n->setKeyboardTracking(false);n->setSingleStep(.1);n->setValue(initial);n->setProperty("field",key);fields_[key]=n;form->addRow(label,n);};
    auto line=[&](QString key,QString label,QString initial){auto* n=new QLineEdit(initial);n->setProperty("field",key);fields_[key]=n;form->addRow(label,n);};
    auto check=[&](QString key,QString label,bool initial){auto* n=new QCheckBox(label);n->setChecked(initial);n->setProperty("field",key);fields_[key]=n;form->addRow(n);};
    auto note=[&](QString s){auto* n=new QLabel(s);n->setWordWrap(true);left->addWidget(n);};
    if(mode=="single"){num("draft","吃水 ("+unit+")",m->draft,0,1e6);num("heel","横倾角 (°)",0,-85,85);}
    if(mode=="table"){
        num("start","起始吃水 ("+unit+")",std::max(0.,defaults["start"].toDouble()),0,1e6);
        num("end","终止吃水 ("+unit+")",std::max(m->draft,defaults["end"].toDouble()),0,1e6);
        num("step","吃水间隔 ("+unit+")",defaults["step"].toDouble()>0?defaults["step"].toDouble():std::max(.001,m->draft/10),.000001,1e6);
    }
    num("trim","纵倾差 ("+unit+")",defaults[mode=="cross"?"cross_trim":"trim"].toDouble(),-m->length,m->length);
    if(mode=="cross"){
        auto displacements=defaults["displacements"].toArray();if(displacements.isEmpty())displacements.append(defaults["design_displacement"].toDouble(1));
        auto angles=defaults["angles"].toArray();if(angles.isEmpty())angles=QJsonArray{0,5,10,15,20,30,40,50,60};
        line("displacements","排水量 ("+mass+")",list(displacements));line("angles","横倾角 (°)",list(angles));
        num("lcg","重心 X ("+unit+")",(m->minimum.x+m->maximum.x)/2);num("tcg","重心 Y ("+unit+")",0);
        num("vcg","重心 Z ("+unit+")",defaults["vcg"].toDouble());check("free_trim","求解自由纵倾平衡",defaults["free_trim"].toBool());
        note("列表按递增顺序填写，以逗号分隔。重心 X/Y/Z 均使用模型坐标。");
        note("KN 以船体最低点为基准；GZ 已计入重心。GZ 按输入角度做梯形积分，从 0° 起算，遇到错误工况后停止累计。");
        note("重心 X/Y 仅保留在本次报告；其余参数可保存到船模。");
    }else note("吃水从参与静水力计算船体的最低点量起。纵倾差为 X=船长 与 X=0 处的水线高度差；横倾时沿用原版计算约定。");
    note(QString("水密度：%1 %2\n附体系数：%3\n参与计算：图层的静水力开关；隐藏图层仍可参与。\n系数基准：%4")
        .arg(m->waterDensity,0,'g',6).arg(m->units=="metric"?"t/m³":"lb/ft³").arg(m->appendageCoefficient,0,'g',6)
        .arg(defaults["coefficient_basis"].toInt()==0?"工程主尺度":"实际水下尺度"));
    run_=new QPushButton("计算");run_->setProperty("hydro_action","calculate");cancel_=new QPushButton("取消计算");cancel_->setProperty("hydro_action","cancel");
    auto* controls=new QHBoxLayout;controls->addWidget(run_);controls->addWidget(cancel_);left->addLayout(controls);
    saveSettings_=new QPushButton("将计算范围保存到船模");saveSettings_->setProperty("hydro_action","save_settings");left->addWidget(saveSettings_);saveSettings_->setVisible(mode!="single");
    left->addStretch();auto* parameterScroll=new QScrollArea;parameterScroll->setWidgetResizable(true);parameterScroll->setFrameShape(QFrame::NoFrame);
    parameterScroll->setWidget(parameters);parameterScroll->setMinimumWidth(340);parameterScroll->setMaximumWidth(440);splitter->addWidget(parameterScroll);
    tabs_=new QTabWidget;splitter->addWidget(tabs_);splitter->setStretchFactor(1,1);
    splitter->setSizes({360,720});
    table_=new QTableWidget;table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(true);tabs_->addTab(table_,"结果表");
    auto* plotPage=new QWidget;auto* plotLayout=new QVBoxLayout(plotPage);plotLayout->setContentsMargins(0,0,0,0);
    auto* plot=new HydroPlot(plotPage);plot->lengthUnit=unit;plot_=plot;plotLayout->addWidget(plot,1);
    auto* legendScroll=new QScrollArea;legendScroll->setWidgetResizable(true);legendScroll->setMaximumHeight(72);legendScroll->setFrameShape(QFrame::NoFrame);
    legend_=new QLabel;legend_->setWordWrap(true);legend_->setMargin(5);legendScroll->setWidget(legend_);legendScroll->setVisible(mode=="cross");plotLayout->addWidget(legendScroll);
    tabs_->addTab(plotPage,"曲线");
    text_=new QPlainTextEdit;text_->setReadOnly(true);tabs_->addTab(text_,"报告与基准");
    if(mode=="single"){
        auto* page=new QWidget;auto* box=new QVBoxLayout(page);
        auxiliaryNotice_=new QLabel("可在工程设置中启用附加表，并定义水线与肋位后重新计算。");auxiliaryNotice_->setWordWrap(true);box->addWidget(auxiliaryNotice_);
        auxiliaryTabs_=new QTabWidget;box->addWidget(auxiliaryTabs_);tabs_->addTab(page,"附加表");
        connect(auxiliaryTabs_,&QTabWidget::currentChanged,this,[this]{renderAuxiliaryPage();});
        connect(tabs_,&QTabWidget::currentChanged,this,[this]{renderAuxiliaryPage();});
        auxiliarySave_=new QPushButton("保存当前附加表…");auxiliarySave_->setProperty("hydro_action","save_auxiliary");box->addWidget(auxiliarySave_);
        connect(auxiliarySave_,&QPushButton::clicked,this,[this,hidden]{
            const int index=auxiliaryTabs_->currentIndex();const auto files=result_["auxiliary"].toObject()["files"].toArray();
            if(index<0||index>=files.size())return;
            // Only the explicitly hidden regression flow can supply a path;
            // ordinary use always asks which file to save and confirms overwrite.
            QString path=hidden?auxiliarySave_->property("probe_save_path").toString():QString{};
            if(path.isEmpty())path=QFileDialog::getSaveFileName(this,"保存附加表",QDir(preferenceFileLocation("export")).filePath(files[index].toObject()["name"].toString()),"文本文件 (*.txt)");
            if(path.isEmpty())return;const auto error=exportAuxiliary(path,index);status_->setText(error.isEmpty()?"报告已保存："+path:error);auxiliarySave_->setProperty("export_error",error);
        });
    }
    status_=new QLabel("设置工况后点击“计算”。");status_->setWordWrap(true);layout->addWidget(status_);
    auto edited=[this]{if(!result_.isEmpty()&&!session_.busy())status_->setText("参数已更改；当前表格和报告仍是上一次结果，请重新计算。");};
    for(auto* field:fields_){
        if(auto* n=qobject_cast<QDoubleSpinBox*>(field))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,edited);
        else if(auto* n=qobject_cast<QLineEdit*>(field))connect(n,&QLineEdit::textEdited,this,edited);
        else if(auto* n=qobject_cast<QCheckBox*>(field))connect(n,&QCheckBox::toggled,this,edited);
    }
    auto* footer=new QHBoxLayout;layout->addLayout(footer);
    for(const auto& format:{QString("csv"),QString("txt"),QString("json"),QString("pdf")}){
        auto* b=new QPushButton("导出 "+format.toUpper());b->setProperty("hydro_export",format);exports_.append(b);footer->addWidget(b);
        connect(b,&QPushButton::clicked,this,[this,format]{const auto path=QFileDialog::getSaveFileName(this,"保存计算报告",preferenceFileLocation("export"),format.toUpper()+" (*."+format+")");if(path.isEmpty())return;
            const auto error=exportReport(path,format);status_->setText(error.isEmpty()?"报告已保存："+path:error);});
    }
    footer->addStretch();auto* close=new QPushButton("关闭");close->setProperty("hydro_action","close");footer->addWidget(close);connect(close,&QPushButton::clicked,this,&HydroDialog::reject);
    connect(run_,&QPushButton::clicked,this,&HydroDialog::calculate);connect(cancel_,&QPushButton::clicked,this,[this]{session_.cancelHydrostatics();status_->setText("正在取消，等待当前积分结束…");});
    connect(saveSettings_,&QPushButton::clicked,this,[this]{try{if(session_.saveHydroSettings(inputs()))status_->setText("正在保存计算范围；可通过主窗口撤销。");}catch(const std::exception& ex){showFailure(QString::fromUtf8(ex.what()));}});
    refreshState();
}
QJsonObject HydroDialog::inputs()const{
    auto name=session_.model()->name;
    if(name.isEmpty())name=QFileInfo(session_.sourcePath()).completeBaseName();
    if(name.isEmpty())name="未命名船模";
    QJsonObject out{{"mode",mode_},{"model_name",name}};
    for(auto it=fields_.begin();it!=fields_.end();++it){
        if(auto* n=qobject_cast<QDoubleSpinBox*>(it.value()))out[it.key()]=n->value();
        else if(auto* n=qobject_cast<QLineEdit*>(it.value()))out[it.key()]=numbers(n->text());
        else if(auto* n=qobject_cast<QCheckBox*>(it.value()))out[it.key()]=n->isChecked();
    }return out;
}
void HydroDialog::calculate(){
    try{if(session_.calculateHydrostatics(inputs())){setProperty("hydro_finished",false);setProperty("hydro_cancelled",false);setProperty("hydro_error",QString{});status_->setText("正在计算，可取消。编辑文档保持不变。");refreshState();}}
    catch(const std::exception& ex){showFailure(QString::fromUtf8(ex.what()));}
}
void HydroDialog::refreshState(){
    const bool busy=session_.busy();for(auto* w:fields_)w->setEnabled(!busy);run_->setEnabled(!busy);cancel_->setEnabled(busy);saveSettings_->setEnabled(!busy&&mode_!="single");
    for(auto* b:exports_)b->setEnabled(!busy&&!result_.isEmpty());
    if(auxiliarySave_)auxiliarySave_->setEnabled(!busy&&auxiliaryCount()>0);
    if(closing_&&!busy)QTimer::singleShot(0,this,[this]{QDialog::reject();});
}
void HydroDialog::showFailure(const QString& error){status_->setText(error);setProperty("hydro_error",error);setProperty("hydro_finished",true);refreshState();if(closing_)QDialog::reject();}
void HydroDialog::settingsSaved(){status_->setText("计算范围已写入当前文档，可撤销。保存船模后，下次打开仍可使用。");refreshState();}
void HydroDialog::receive(const QJsonObject& result){
    setProperty("hydro_finished",true);setProperty("hydro_cancelled",result["cancelled"].toBool());
    if(result["cancelled"].toBool())status_->setText(result["message"].toString()+" 已保留上一次完整结果。");
    else{result_=result;render();}refreshState();if(closing_)QDialog::reject();
}
void HydroDialog::reject(){if(session_.busy()){closing_=true;session_.cancelHydrostatics();status_->setText("取消计算后关闭…");return;}QDialog::reject();}
void HydroDialog::render(){
    const QString u=result_["units"]=="metric"?"m":"ft",w=result_["units"]=="metric"?"t":"long ton";
    columns_={{"draft","吃水 ("+u+")"},{"heel","横倾 (°)"},{"trim","纵倾 ("+u+")"},{"displacement","排水量 ("+w+")"},{"volume","排水体积 ("+u+"³)"}};
    if(mode_=="cross")columns_={{"target_displacement","目标排水量 ("+w+")"},{"heel","横倾 (°)"},{"trim","纵倾 ("+u+")"},{"displacement","排水量 ("+w+")"},{"kn","KN ("+u+")"},{"gz","GZ ("+u+")"},{"gz_integral","GZ 积分 ("+u+"·rad)"},{"mass_residual","排水量残差 ("+w+")"},{"longitudinal_residual","纵向平衡残差 ("+u+")"}};
    else for(auto pair:QVector<QPair<QString,QString>>{{"buoyancy.0","浮心 X"},{"buoyancy.1","浮心 Y"},{"buoyancy.2","浮心 Z"},{"lwl","水线长"},{"bwl","水线宽"},{"midship_x","中剖面 X"},{"midship_area","中剖面积"},{"waterplane_area","水线面面积"},{"wetted_area","湿表面积"},{"cb","Cb"},{"cp","Cp"},{"cvp","Cvp"},{"cm","Cm"},{"cwp","Cwp"},{"waterplane_cog.0","漂心 X"},{"waterplane_cog.1","漂心 Y"},{"waterplane_ix","水线面 Ix"},{"waterplane_iy","水线面 Iy"},{"kmt","横稳心高度 KMt"},{"kml","纵稳心高度 KMl"},{"lateral_area","侧投影面积"},{"weight","船壳材料重量"}}){
        const auto k=pair.first;QString suffix;
        if(k=="weight")suffix=w;else if(k.contains("area"))suffix=u+"²";else if(k=="waterplane_ix"||k=="waterplane_iy")suffix=u+"⁴";else if(!QStringList{"cb","cp","cvp","cm","cwp"}.contains(k))suffix=u;
        columns_.append({k,pair.second+(suffix.isEmpty()?QString{}:" ("+suffix+")")});
    }
    columns_.append(QPair<QString,QString>{"message","状态"});table_->clear();table_->setColumnCount(columns_.size());QStringList labels;for(auto c:columns_)labels<<c.second;table_->setHorizontalHeaderLabels(labels);
    const auto rows=result_["rows"].toArray();table_->setRowCount(rows.size());int errors=0,partial=0;
    QStringList legend;double previousMass=0;int curve=0;
    for(auto v:rows){const auto mass=v.toObject()["target_displacement"].toDouble();if(mode_=="cross"&&mass!=previousMass){
        legend<<QString("<span style='color:%1'>● %2 %3</span>").arg(curveColor(curve++).name(),QString::number(mass,'g',9),w);previousMass=mass;
    }}legend_->setText(legend.join(" &nbsp; &nbsp; "));
    for(int i=0;i<rows.size();++i){const auto row=rows[i].toObject();const bool valid=row["status"]=="ok";if(!valid)++errors;if(row["partial"].toBool())++partial;
        for(int j=0;j<columns_.size();++j){const auto key=columns_[j].first;const auto v=value(row,key);QString s=v.isDouble()?QString::number(v.toDouble(),'g',9):v.toString();if(key=="message"&&valid&&s.isEmpty())s="完成";
            if(v.isNull()&&valid)s="未提供";
            auto* item=new QTableWidgetItem(s);item->setToolTip(v.isDouble()?full(v.toDouble()):s);if(!valid)item->setForeground(QColor(160,45,30));else if(row["partial"].toBool())item->setForeground(QColor(130,85,10));table_->setItem(i,j,item);}
    }table_->resizeColumnsToContents();
    auto* plot=static_cast<HydroPlot*>(plot_);plot->result=result_;plot->update();text_->setPlainText(reportText());
    if(auxiliaryTabs_){
        while(auxiliaryTabs_->count()){auto* page=auxiliaryTabs_->widget(0);auxiliaryTabs_->removeTab(0);delete page;}
        const auto auxiliary=result_["auxiliary"].toObject();
        auxiliaryNotice_->setText(auxiliary.isEmpty()?"未启用附加表。可在工程设置中启用后重新计算。":auxiliary["message"].toString());
        for(const auto& entry:auxiliary["files"].toArray()){const auto file=entry.toObject();auto* text=new QPlainTextEdit;text->setReadOnly(true);text->setLineWrapMode(QPlainTextEdit::NoWrap);text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));auxiliaryTabs_->addTab(text,file["name"].toString());}
        renderAuxiliaryPage();
    }
    status_->setText(QString("完成 %1 个工况，其中 %2 个未得到有效结果%3。耗时 %4 s。").arg(rows.size()).arg(errors)
        .arg(partial?QString("，%1 个工况的部分量未提供（见状态列）").arg(partial):QString{}).arg(result_["elapsed_ms"].toDouble()/1000,0,'f',3));
}
QByteArray HydroDialog::csv()const{
    auto quote=[](QString s){return '"'+s.replace('"',"\"\"")+'"';};QStringList lines,heads;
    for(auto c:columns_)heads<<quote(c.second);
    lines<<heads.join(',');
    for(auto v:result_["rows"].toArray()){QStringList row;for(auto c:columns_){auto x=value(v.toObject(),c.first);row<<(x.isDouble()?full(x.toDouble()):quote(x.toString()));}lines<<row.join(',');}
    return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();
}
QString HydroDialog::reportText()const{
    QString text="FreeShip_26 — "+windowTitle()+"\n";
    const bool metric=result_["units"]=="metric";
    const QString unit=metric?"m":"ft",mass=metric?"t":"long ton";
    const auto inputs=result_["inputs"].toObject();
    text+="模型："+inputs["model_name"].toString()+"\n";
    text+="单位："+QString(metric?"米、吨":"英尺、长吨")+"；最低点 Z="+full(result_["keel_z"].toDouble())+" "+unit+"\n";
    text+="水密度："+full(result_["water_density"].toDouble())+QString(metric?" t/m³":" lb/ft³")+"；附体系数："+full(result_["appendage_coefficient"].toDouble())+"\n";
    text+="吃水与 KN 的基准为参与计算船体的最低点。浮心为按原算法旋转后的坐标；重心输入为模型坐标。KM 为旧核心的最低点基准值。\n";
    text+="数值表保留原积分精度；未收敛和进水行不作为有效结果。此报告未执行天气衡准或规范判定。\n\n输入：\n";
    if(mode_=="cross")text+="GZ 面积按相邻输入角度做梯形积分，使用弧度；减小角度间隔可减少离散积分误差。\n";
    const QVector<QPair<QString,QString>> parameters{
        {"draft","吃水 ("+unit+")"},{"heel","横倾角 (°)"},{"start","起始吃水 ("+unit+")"},
        {"end","终止吃水 ("+unit+")"},{"step","吃水间隔 ("+unit+")"},{"trim","纵倾差 ("+unit+")"},
        {"angles","横倾角 (°)"},{"displacements","排水量 ("+mass+")"},{"lcg","重心 X ("+unit+")"},
        {"tcg","重心 Y ("+unit+")"},{"vcg","重心 Z ("+unit+")"},{"free_trim","自由纵倾"}};
    for(const auto& c:parameters){
        const auto v=inputs[c.first];if(v.isUndefined())continue;
        QString display;if(v.isBool())display=v.toBool()?"是":"否";
        else if(v.isArray()){QStringList parts;for(auto n:v.toArray())parts<<full(n.toDouble());display=parts.join(", ");}
        else display=full(v.toDouble());
        text+=c.second+"："+display+"\n";
    }
    text+="\n结果：\n";
    for(auto v:result_["rows"].toArray()){auto row=v.toObject();QStringList values;for(auto c:columns_){auto x=value(row,c.first);if(!x.isUndefined()&&!x.isNull())values<<c.second+"="+(x.isDouble()?full(x.toDouble()):x.toString());}text+=values.join("；")+"\n";}
    return text;
}
void HydroDialog::renderAuxiliaryPage(){
    if(!auxiliaryTabs_||tabs_->currentWidget()!=auxiliaryTabs_->parentWidget())return;
    auto* text=qobject_cast<QPlainTextEdit*>(auxiliaryTabs_->currentWidget());const int index=auxiliaryTabs_->currentIndex();
    const auto files=result_["auxiliary"].toObject()["files"].toArray();
    if(!text||index<0||index>=files.size()||text->property("loaded").toBool())return;
    text->setPlainText(files[index].toObject()["text"].toString());text->setProperty("loaded",true);
}
int HydroDialog::auxiliaryCount()const{return result_["auxiliary"].toObject()["files"].toArray().size();}
bool HydroDialog::selectAuxiliary(int index){
    if(!auxiliaryTabs_||index<0||index>=auxiliaryCount())return false;
    tabs_->setCurrentWidget(auxiliaryTabs_->parentWidget());auxiliaryTabs_->setCurrentIndex(index);return true;
}
QString HydroDialog::exportAuxiliary(const QString& path,int index)const{
    const auto files=result_["auxiliary"].toObject()["files"].toArray();if(index<0||index>=files.size())return "没有可保存的附加表。";
    const auto bytes=files[index].toObject()["text"].toString().toUtf8();QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return "附加表保存失败，原文件未替换："+file.errorString();
    return {};
}
QString HydroDialog::exportReport(const QString& path,const QString& format)const{
    if(result_.isEmpty())return "尚无完整计算结果。";
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return "无法写入报告："+file.errorString();
    if(format=="pdf"){
        QPdfWriter writer(&file);writer.setPageSize(QPageSize(QPageSize::A4));writer.setResolution(120);
        QTextDocument doc;doc.setPlainText(reportText());doc.print(&writer);
    }else{
        QByteArray bytes;if(format=="csv")bytes=csv();else if(format=="json")bytes=QJsonDocument(result_).toJson(QJsonDocument::Indented);else if(format=="txt")bytes=reportText().toUtf8();else return "未知报告格式。";
        if(file.write(bytes)!=bytes.size())return "报告写入未完成，原文件未替换。";
    }
    if(!file.commit())return "无法完成报告保存："+file.errorString();
    return {};
}
