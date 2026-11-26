#include "feedback_text.h"
#include "core/platform_process.h"
#include "app/ui_language.h"
#include "preferences_store.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "exchange_dialog.h"
#include "model_session.h"
#include "core/model_exchange.h"
#include "core/cad_export.h"
#include "studio_ui.h"
#include "grid_preview_panel.h"
#include "print_hull_panel.h"
#include "studio_motion.h"
#include "studio_help_hint.h"
#include "obj_import_layer_tree.h"
#include <QButtonGroup>
#include <QPainter>
#include <QVariantAnimation>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QSaveFile>
#include <QSpinBox>
#include <QRegularExpression>
#include <QTabWidget>
#include <QToolButton>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>
#include <cmath>
static QString localizedExchangeError(const QString& source){
    // Translate only the known diagnostic framing; values and file contents
    // after a diagnostic prefix belong to the imported/exported data.
    for(const auto* wrapper:{"模型交接：","曲线导入："}){
        const auto prefix=QString::fromUtf8(wrapper);
        if(source.startsWith(prefix)){
            const auto caption=uiText(prefix);
            return caption+(caption==prefix?QString{}:QStringLiteral(" "))+localizedExchangeError(source.mid(prefix.size()));
        }
    }
    static const QRegularExpression line(QStringLiteral("^(OBJ|Polycad) 第 ([0-9]+) 行：(.*)$"));
    if(const auto match=line.match(source);match.hasMatch())
        return uiText(match.captured(1)+QStringLiteral(" 第 %1 行：%2")).arg(match.captured(2),localizedExchangeError(match.captured(3)));
    static const QRegularExpression fitError(QStringLiteral("^重建未达到容差。最大采样偏差 (\\S+) mm，容差 (\\S+) mm。$"));
    if(const auto match=fitError.match(source);match.hasMatch())
        return QString(uiText("重建未达到容差。最大采样偏差 %1 mm，容差 %2 mm。")).arg(match.captured(1),match.captured(2));
    for(const auto* framing:{"无效数字：","数量或单位标识无效：","无效或超出范围的数字：","无效的索引或数量：",
                              "无效的布尔值：","VRML 缺少 ","VRML 节点暂不支持，未导入部分模型：","VRML 字段重复：","VRML 字段暂不支持："}){
        const auto prefix=QString::fromUtf8(framing);
        if(source.startsWith(prefix)){
            const auto caption=uiText(prefix);
            return caption+(caption==prefix||caption.endsWith(' ')?QString{}:QStringLiteral(" "))+source.mid(prefix.size());
        }
    }
    return uiDiagnosticText(source);
}
namespace {
constexpr std::array objModes{
    std::array{"mesh","保留原网格","保留 OBJ 网格的外形，将多边形面拆成三角面"},
    std::array{"control","作为控制网格导入","用 OBJ 的顶点和多边形面建立控制网格。细分后的曲面可能与原网格有偏差"},
    std::array{"fit","拟合船体曲面","自动调整控制点，让细分后的船体曲面贴合原网格"}
};

QIcon objModeIcon(int mode){
    QPixmap image(224,144);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto point=[mode](qreal u,qreal v){
        const qreal bend=mode==2?std::sin(u*3.141592653589793)*(5+12*v):0;
        return QPointF(8+51*u+43*v,39-27*u+24*v+bend);
    };
    const auto stroke=[&](QPointF a,QPointF b,bool alongU,qreal fixed){
        QPolygonF line{a};
        for(int i=1;i<16;++i)line<< (alongU?point(i/16.,fixed):point(fixed,i/16.));
        line<<b;
        return line;
    };
    QPolygonF face=stroke(point(0,0),point(1,0),true,0)+stroke(point(1,0),point(1,1),false,1);
    const auto back=stroke(point(0,1),point(1,1),true,1);
    for(auto i=back.crbegin();i!=back.crend();++i)face<<*i;
    face<<point(0,0);
    painter.setBrush(QColor(mode==2?"#bcdfd3":"#e3eee9"));
    painter.setPen(QPen(QColor("#447369"),1.5));
    painter.drawPolygon(face);
    painter.setPen(QPen(QColor(mode==2?"#468e80":"#74998e"),.85));
    const int columns=mode==0?5:3;
    for(int x=1;x<columns;++x)painter.drawPolyline(stroke(point(double(x)/columns,0),point(double(x)/columns,1),false,double(x)/columns));
    for(int y=1;y<3;++y)painter.drawPolyline(stroke(point(0,double(y)/3),point(1,double(y)/3),true,double(y)/3));
    if(mode==0)for(int x=0;x<columns;++x)for(int y=0;y<3;++y)
        painter.drawLine(point(double(x)/columns,double(y)/3),point(double(x+1)/columns,double(y+1)/3));
    if(mode==1){
        painter.setPen(QPen(QColor("#3f776b"),1));
        painter.setBrush(QColor("#f5faf7"));
        for(int x=0;x<=columns;++x)for(int y=0;y<=3;++y)
            painter.drawEllipse(point(double(x)/columns,double(y)/3),2,2);
    }
    return QIcon(image);
}

class ObjModeCard final : public QToolButton {
public:
    explicit ObjModeCard(int mode,QWidget* parent) : QToolButton(parent),help_(new StudioHelpHint(this,22)) {
        const auto& copy=objModes[mode];
        setProperty("obj_import_mode",copy[0]);
        setObjectName("ObjModeCard");
        setCheckable(true);
        setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setIcon(objModeIcon(mode));
        setIconSize({112,72});
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        setMinimumHeight(111);
        bindUiText(this,"text",copy[1]);
        bindUiText(help_,"toolTip",copy[2]);
        bindUiText(help_,"accessibleName",copy[2]);
        setStyleSheet("QToolButton#ObjModeCard{background:#f8fbf9;border:1px solid #d4e0da;border-radius:8px;padding:10px 6px 8px;}"
                      "QToolButton#ObjModeCard:hover{border-color:#9fbaae;}"
                      "QToolButton#ObjModeCard:checked{background:#e0f0e8;border-color:#7eaa9a;color:#086d61;}"
                      "QToolButton#ObjModeCard:disabled{color:#8d9994;}");
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QToolButton::resizeEvent(event);
        help_->move(width()-help_->width()-7,7);
    }
private:
    StudioHelpHint* help_;
};

class ObjToleranceRow final : public QWidget {
public:
    ObjToleranceRow(){
        auto* row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);row->setSpacing(6);
        setStyleSheet(
            "QToolButton#ObjToleranceAuto,QLineEdit#ObjToleranceValue {color:#315d57;background:transparent;"
            "border:1px solid #d7e3dd;border-radius:5px;padding:5px 10px;}"
            "QToolButton#ObjToleranceAuto:focus,QLineEdit#ObjToleranceValue:focus {border-color:#739f90;}"
            "QToolButton#ObjToleranceAuto:disabled,QLineEdit#ObjToleranceValue:disabled {color:#8d9994;}");
        automatic=new QToolButton;
        automatic->setObjectName("ObjToleranceAuto");
        automatic->setText(uiText("按船壳大小"));
        bindUiText(automatic,"text","按船壳大小");
        automatic->setToolTip(uiText("船壳外接长方体的对角线长度的 0.1%"));
        automatic->setProperty("obj_tolerance_mode","relative");
        automatic->setCheckable(true);automatic->setChecked(true);
        automatic->setFocusPolicy(Qt::StrongFocus);
        value=new QLineEdit;
        value->setObjectName("ObjToleranceValue");value->setProperty("obj_tolerance_value",true);
        value->setAccessibleName(uiText("最大允许采样偏差 (mm)"));
        value->setPlaceholderText(QStringLiteral("—"));value->setReadOnly(true);value->setMinimumWidth(96);
        value->installEventFilter(this);
        row->addWidget(automatic);row->addWidget(value,1);
        slide_.setDuration(180);slide_.setEasingCurve(QEasingCurve::InOutCubic);
        connect(&slide_,&QVariantAnimation::valueChanged,this,[this](const QVariant& phase){phase_=phase.toReal();update();});
        connect(automatic,&QToolButton::clicked,this,[this]{selectAutomatic(true);});
    }
    QToolButton* automatic;
    QLineEdit* value;
    void setEstimate(double estimate){
        estimate_=estimate;
        if(automatic->isChecked())showEstimate();
    }
    bool manual()const{return !automatic->isChecked();}
protected:
    bool eventFilter(QObject* object,QEvent* event)override{
        if(object==value&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::FocusIn)&&!manual())
            selectAutomatic(false);
        return QWidget::eventFilter(object,event);
    }
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF a=automatic->geometry(),b=value->geometry();
        const QRectF highlight(a.topLeft()+(b.topLeft()-a.topLeft())*phase_,
                              a.size()+(b.size()-a.size())*phase_);
        p.setPen(QColor("#82b3a3"));p.setBrush(QColor("#d5e9df"));
        p.drawRoundedRect(highlight.adjusted(.5,.5,-.5,-.5),5,5);
    }
private:
    void showEstimate(){value->setText(estimate_>0?QString::number(estimate_,'g',12):QString{});}
    void selectAutomatic(bool on){
        automatic->setChecked(on);value->setReadOnly(on);
        if(on)showEstimate();
        const double end=on?0.:1.;
        slide_.stop();
        if(phase_!=end&&isVisible()&&studio_motion::enabled()){
            slide_.setStartValue(phase_);slide_.setEndValue(end);slide_.start();
        }else{phase_=end;update();}
    }
    QVariantAnimation slide_;
    double phase_=0.,estimate_=0.;
};
bool sectionFormat(const QString& f){return f=="dxfcrease"||f=="offset_table"||f=="michlet"||f=="addmass"||f=="ghs"||f=="arch_hll"||f=="arch_app"||f=="pam";}
bool metricFormat(const QString& f){return f=="michlet"||f=="addmass"||f=="arch_hll"||f=="arch_app"||f=="pam";}
bool fileUnits(const QString& f){return f=="part"||f=="waves"||f=="fef"||f=="chines"||f=="surface"||f=="hull";}
bool chineFormat(const QString& f){return f=="chines"||f=="carene"||f=="hull";}
struct ObjFitResult {
    fs::exchange::Document document;
    QString error;
};
}

ExchangeDialog::ExchangeDialog(ModelSession& session,bool importing,QString format,QWidget* parent)
    :QDialog(parent),session_(session),importing_(importing),format_(std::move(format)){
    const auto refreshTitle=[this]{setWindowTitle((importing_?uiText("导入 "):uiText("导出 "))+uiText(fs::cad::title(format_)));};
    refreshTitle();bindUiRefresh(this,refreshTitle);resize(620,460);
    setWindowIcon(studioFormatWindowIcon(fs::cad::extension(format_),importing));setProperty("studio_window_icon","format:"+fs::cad::extension(format_).toUpper());
    confirmOverwrite=[this](const QString& path){return confirmStudioAction(this,uiText("替换文件"),uiText("文件已存在，是否替换？\n")+path);};
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(22,18,22,18);layout->setSpacing(12);
    auto* form=new QFormLayout;
    QWidget* objSidebar=nullptr;
    if(importing_){
        auto* fileRow=new QHBoxLayout;importPath_=new QLineEdit;importPath_->setProperty("exchange_field","path");
        auto* browse=studioFolderButton(uiText("浏览…"));
        fileRow->addWidget(importPath_,1);fileRow->addWidget(browse);form->addRow(uiText("文件"),fileRow);
        connect(browse,&QPushButton::clicked,this,[this]{
            if(busy())return;
            const auto ext=fs::cad::extension(format_);
            const auto filter=uiText(fs::cad::title(format_))+" (*."+ext+(format_=="iges"?" *.iges":"")+")";
            const auto start=importPath_->text().isEmpty()?preferenceFileLocation("import"):importPath_->text();
            const auto path=studioOpenFile(this,windowTitle(),start,filter);
            if(!path.isEmpty()){
                if(objLayerTree_&&path==importPath_->text())scheduleObjLayers();
                else importPath_->setText(path);
            }
        });
    }
    units_=new QComboBox;units_->setProperty("exchange_field","units");for(auto pair:{std::pair{"米 (m)","m"},{"毫米 (mm)","mm"},{"厘米 (cm)","cm"},{"英尺 (ft)","ft"},{"英寸 (in)","in"}})addUiItem(units_,pair.first,pair.second);
    if(!importing&&session.model()&&session.model()->units=="imperial")units_->setCurrentIndex(3);
    form->addRow(uiText("文件中的长度单位"),units_);
    if(format_=="part"){encoding_=new QComboBox;encoding_->setProperty("exchange_field","text_encoding");for(auto pair:{std::pair{"UTF-8","utf8"},{"西欧 Windows-1252","cp1252"},{"俄语 Windows-1251","cp1251"},{"简体中文 GBK","cp936"}})addUiItem(encoding_,pair.first,pair.second);if(importing_){auto current=session.model()?session.model()->textEncoding:QString("cp1252");int i=encoding_->findData(current);if(i>=0)encoding_->setCurrentIndex(i);}form->addRow(uiText("部件文字编码（与来源/接收软件一致）"),encoding_);}
    auto check=[&](QString caption,QString key){auto* c=new QCheckBox(caption);c->setProperty("exchange_field",key);form->addRow(c);return c;};
    axes_=check(uiText("按旧版 FreeShip OBJ 的轴顺序 (Y, Z, X)"),"legacy_axes");
    control_=check(importing?uiText("作为控制网导入，允许细分平滑"):uiText("导出控制网"),"control_net");
    symmetric_=check(importing?uiText("文件仅有半船，按中心线镜像"):uiText("补全对称图层的另一舷"),"symmetric");symmetric_->setChecked(!importing);
    hidden_=check(uiText("包含隐藏图层"),"include_hidden");binary_=check(uiText("二进制 STL"),"binary_stl");binary_->setChecked(true);
    closeDeck_=check(uiText("将敞开甲板按水平线封到中心线"),"close_deck");closeDeck_->setVisible(format_=="ghs"||format_=="arch_hll"||format_=="arch_app"||format_=="pam");
    axes_->setVisible(format_=="obj");control_->setVisible(format_=="obj");hidden_->setVisible(!importing&&format_!="fef");binary_->setVisible(!importing&&format_=="stl");symmetric_->setVisible(format_!="fef");units_->setEnabled(format_!="fef");
    if(importing_&&format_=="obj"){
        control_->hide();
        resize(865,490);
        objImportCards_=new QWidget;
        auto* cards=new QHBoxLayout(objImportCards_);
        cards->setContentsMargins(0,0,0,0);
        objImportMode_=new QButtonGroup(this);
        for(int mode=0;mode<int(objModes.size());++mode){
            auto* card=new ObjModeCard(mode,objImportCards_);
            objImportMode_->addButton(card,mode);
            cards->addWidget(card,1);
        }
        objImportMode_->button(0)->setChecked(true);
        auto* methodLabel=new QLabel;
        bindUiText(methodLabel,"text","导入方式");
        form->addRow(methodLabel);
        form->addRow(objImportCards_);
        objSidebar=new QWidget;
        auto* layerLayout=new QVBoxLayout(objSidebar);
        layerLayout->setContentsMargins(0,0,0,0);
        auto* layerLabel=new QLabel;
        bindUiText(layerLabel,"text","图层","collection");
        layerLayout->addWidget(layerLabel);
        objLayerTree_=new ObjImportLayerTree(objSidebar);
        objLayerTree_->setStyleSheet("QTreeWidget{background:#f9fbfa;border:1px solid #ceddd6;border-radius:5px;padding:3px;}"
                                     "QTreeView::item{min-height:24px;} QTreeView::item:selected{background:#e0eee6;color:#234d42;}");
        layerLayout->addWidget(objLayerTree_,1);
        objLayersTimer_.setSingleShot(true);
        objLayersTimer_.setInterval(100);
        connect(&objLayersTimer_,&QTimer::timeout,this,&ExchangeDialog::readObjLayers);
        connect(importPath_,&QLineEdit::textChanged,this,&ExchangeDialog::scheduleObjLayers);
        objInferCreases_=check(uiText("自动识别折痕"),"obj_infer_creases");
        objInferCreases_->setChecked(true);
        auto* tolerance=new ObjToleranceRow;
        objToleranceRow_=tolerance;
        objTolerance_=tolerance->value;
        objAutomaticTolerance_=tolerance->automatic;
        form->addRow(uiText("最大允许采样偏差 (mm)"),objToleranceRow_);
        const auto showOptions=[this,form]{
            const int mode=objImportMode_->checkedId();
            form->setRowVisible(objInferCreases_,mode!=0);
            form->setRowVisible(objToleranceRow_,mode==2);
        };
        connect(objImportMode_,&QButtonGroup::idClicked,this,showOptions);
        showOptions();
        objToleranceTimer_.setSingleShot(true);
        objToleranceTimer_.setInterval(100);
        connect(&objToleranceTimer_,&QTimer::timeout,this,&ExchangeDialog::estimateObjTolerance);
        const auto schedule=[this,tolerance]{
            if(objToleranceCancel_)objToleranceCancel_->store(true,std::memory_order_relaxed);
            objToleranceTimer_.stop();tolerance->setEstimate(0.);
            if(objImportMode_->checkedId()==2&&!importPath_->text().trimmed().isEmpty())objToleranceTimer_.start();
        };
        connect(importPath_,&QLineEdit::textChanged,this,schedule);
        connect(units_,&QComboBox::currentIndexChanged,this,schedule);
        connect(axes_,&QCheckBox::toggled,this,schedule);
        connect(objImportMode_,&QButtonGroup::idClicked,this,schedule);
    }
    if(format_=="coordinates"){control_->setVisible(true);control_->setText(uiText("导出原控制点（不补镜像，不按图层显隐筛选）"));connect(control_,&QCheckBox::toggled,this,[this](bool b){hidden_->setEnabled(!b);symmetric_->setEnabled(!b);});}
    if(format_=="offset_table"||format_=="offsets"||format_=="dxf3d"){hidden_->hide();symmetric_->hide();}
    if(format_=="ghs"){hidden_->hide();symmetric_->hide();units_->clear();addUiItem(units_,"米 (m)","m");addUiItem(units_,"英尺 (ft)","ft");if(session.model()&&session.model()->units=="imperial")units_->setCurrentIndex(1);}
    if(metricFormat(format_)){
        hidden_->hide();symmetric_->hide();units_->clear();addUiItem(units_,"米 (m)","m");
        const auto model=session.model();const double scale=model&&model->units=="imperial"?.3048:1.;
        const double length=model?model->length*scale:0;
        auto number=[&](const QString& source,const QString& key,double value){
            auto* edit=new QLineEdit(QString::number(value,'g',17));edit->setProperty("exchange_number",key);
            references_.insert(key,edit);auto* caption=new QLabel;caption->setBuddy(edit);form->addRow(caption,edit);
            const auto refresh=[caption,source]{caption->setText(uiText(source)+" (m)");};
            refresh();bindUiRefresh(caption,refresh);
        };
        if(format_=="michlet"||format_=="addmass"){
            QFormLayout* fields=form;QTabWidget* tabs=nullptr;
            auto tab=[&](const QString& title){auto* page=new QWidget;fields=new QFormLayout(page);fields->setContentsMargins(14,16,14,16);fields->setVerticalSpacing(12);tabs->addTab(page,title);};
            tabs=new QTabWidget;tabs->setProperty("exchange_tabs",true);form->addRow(tabs);tab(uiText("船体几何"));
            auto parameter=[&](QString label,QString key,double value){auto* edit=new QLineEdit(QString::number(value,'g',12));edit->setProperty("exchange_number",key);references_.insert(key,edit);fields->addRow(label,edit);};
            auto automatic=[&](QString label,QString key){parameter(label,key,0);auto* edit=references_[key];edit->clear();edit->setProperty("exchange_optional",true);bindUiText(edit,"placeholderText","自动：按实际水下船长");};
            auto choice=[&](QString label,QString key){auto* box=new QComboBox;box->setProperty("exchange_parameter",key);referenceChoices_[key]=box;fields->addRow(label,box);return box;};
            parameter(uiText("横剖站数（奇数）"),"stations",format_=="michlet"?31:21);parameter(uiText("水线数（奇数）"),"waterlines",format_=="michlet"?21:11);
            parameter(uiText("吃水 (m)"),"draft",model?model->draft*scale:0);
            if(format_=="michlet"){
                auto* mode=choice(uiText("船体布置"),"hull_mode");addUiItem(mode,"单体船","monohull");addUiItem(mode,"单体船，输出为双体船","duplicate");addUiItem(mode,"已建好的双体船","catamaran");
                parameter(uiText("片体中心面间距 (m)"),"separation",0);references_["separation"]->setToolTip(uiText("单体复制：设置两片体的间距。已有双体：必须等于模型中两片体中心面的实际间距。"));
                connect(mode,&QComboBox::currentIndexChanged,this,[this]{refreshState();});
                tab(uiText("环境与计算"));parameter(uiText("水深 (m)"),"depth",10000);parameter(uiText("重力加速度 (m/s²)"),"gravity",9.80665);
                parameter(uiText("起始速度 (m/s)"),"start_speed",1);parameter(uiText("结束速度 (m/s)"),"end_speed",5);parameter(uiText("速度数量"),"speeds",50);parameter(uiText("运动黏度 (10⁻⁶ m²/s)"),"viscosity",1.18831);
                auto* friction=choice(uiText("摩擦方法"),"friction");addUiItem(friction,"无",0);friction->addItem("ITTC 1957",1);friction->addItem("Grigson",2);friction->setCurrentIndex(1);
                auto* factor=choice(uiText("形状因子方法"),"form_factor");addUiItem(factor,"无",0);factor->addItem("Dual",3);
                tab(uiText("波形区域"));automatic(uiText("扇形起始半径 R0 (m)"),"r0");automatic(uiText("扇形结束半径 R1 (m)"),"r1");parameter(uiText("扇形半角 β (°)"),"beta",22.5);parameter(uiText("径向采样数"),"nr",100);parameter(uiText("角向采样数"),"nbeta",100);
                automatic(uiText("矩形 X0 (m)"),"wave_x0");automatic(uiText("矩形 X1 (m)"),"wave_x1");automatic(uiText("矩形 Y0 (m)"),"wave_y0");automatic(uiText("矩形 Y1 (m)"),"wave_y1");parameter(uiText("X 采样数"),"wave_nx",100);parameter(uiText("Y 采样数"),"wave_ny",100);
                tabs->setCurrentIndex(0);
            }
            else{parameter(uiText("面元数量"),"panels",1000);parameter(uiText("面积比例 (%)"),"area_percent",100);parameter(uiText("计算精度"),"accuracy",.2);parameter(uiText("面元（1 三角形 / 2 梯形）"),"panel_type",1);parameter(uiText("漂角 (°)"),"drift",0);parameter(uiText("航速 (kn)"),"velocity_knots",10);parameter(uiText("角速度 (1/s)"),"omega",0);parameter(uiText("对象类型编号"),"object_type",1);}
            if(format_=="addmass")parameter(uiText("水深 (m)"),"depth",10000);
            if(!importing_){preview_=new GridPreviewPanel(session_,format_,[this]{return referenceValues();});tabs->addTab(preview_,uiText("型值预览"));
                for(auto* field:references_)connect(field,&QLineEdit::textChanged,preview_,[this]{preview_->invalidate();});
                for(auto* choice:referenceChoices_)connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),preview_,[this]{preview_->invalidate();});}
            resize(680,format_=="michlet"?760:820);
        }else if(format_=="pam"){
            number("中船纵向位置","midship",length/2);
            number("设计水线 Z","waterline",model?model->waterlineZ*scale:0);
        }else{
            number("艉垂线 AP · 模型 X","ap",0);number("艏垂线 FP · 模型 X","fp",length);
            if(format_=="arch_hll"){
                number("艉吃水标志 · 模型 X","aft_mark",0);number("中吃水标志 · 模型 X","mid_mark",length/2);number("艏吃水标志 · 模型 X","fwd_mark",length);
            }
        }
    }
    if((format_=="fef"||format_=="part")&&importing_){units_->clear();addUiItem(units_,"使用文件内记录的单位");}
    if(format_=="polycad"||format_=="surface"||chineFormat(format_))symmetric_->setChecked(true);
    if(chineFormat(format_))symmetric_->hide();
    if(importing&&format_!="fef"&&format_!="part"&&format_!="waves"&&fileUnits(format_)){units_->clear();addUiItem(units_,format_=="hull"?"英寸（转换为英尺，使用英制工程）":"使用文件首行：0 为米，1 为英尺");}
    if(importing&&(format_=="chines"||format_=="carene"||format_=="surface")){
        columns_=new QSpinBox;columns_->setProperty("exchange_count","columns");columns_->setRange(format_=="surface"?0:2,100);columns_->setValue(format_=="surface"?0:8);
        if(format_=="surface")columns_->setSpecialValueText(uiText("按原曲线数量（至少 3 列）"));
        form->addRow(format_=="surface"?uiText("拟合网格列数"):uiText("每条折角线的拟合点数"),columns_);
        if(format_=="surface"){rows_=new QSpinBox;rows_->setProperty("exchange_count","rows");rows_->setRange(3,100);rows_->setValue(10);form->addRow(uiText("拟合网格行数"),rows_);}
    }
    if(importing&&format_=="hull"){intermediate_=check(uiText("使用文件中的 13 个中间肋位（取消后使用 5 个主肋位）"),"intermediate_bulkheads");intermediate_->setChecked(true);}
    if(importing&&format_=="waves"){
        auto model=session.model();double scale=model&&model->units=="imperial"?.3048:1.;
        for(auto key:{QString("origin_x"),QString("origin_y")}){auto* edit=new QLineEdit(QString::number(key=="origin_x"&&model?.5*model->length*scale:0,'g',17));edit->setProperty("exchange_number",key);references_[key]=edit;form->addRow(key=="origin_x"?uiText("波形原点 · 模型 X (m)"):uiText("波形原点 · 模型 Y (m)"),edit);}
    }
    if(objSidebar){
        auto* columns=new QHBoxLayout;
        columns->setSpacing(12);
        columns->addLayout(form,3);
        auto* divider=new QFrame;
        divider->setFixedWidth(1);
        divider->setStyleSheet("background:#c9ddd4;");
        columns->addWidget(divider);
        columns->addWidget(objSidebar,1);
        layout->addLayout(columns,1);
    }else layout->addLayout(form);
    if(!importing_&&format_=="stl"){
        printMode_=check(uiText("3D 打印船壳（敞口）"),"print_shell");
        printHull_=new PrintHullPanel(session_,this);layout->addWidget(printHull_);
        printHull_->changed=[this]{refreshState();};
        connect(printMode_,&QCheckBox::toggled,this,[this](bool on){
            if(on){ordinaryUnits_=units_->currentIndex();units_->setCurrentIndex(units_->findData("mm"));}
            else units_->setCurrentIndex(ordinaryUnits_);
            symmetric_->setVisible(!on);hidden_->setVisible(!on);binary_->setVisible(!on);
            printHull_->setActive(on);refreshState();adjustSize();
        });
    }
    details_=new StudioMessage(StudioMessageRole::Explanation);
    if(!importing_&&format_=="fef")details_->setText(uiText("FEF 不保存剖面线、参考曲线、背景图和约束。"));
    if(format_=="polycad")details_->setText(uiText("曲面将拟合为细分控制网，不能无损还原原 B 样条曲面。"));
    if(chineFormat(format_))details_->setText(uiText("输入 Y≥0 的半船；折角线按龙骨至舷缘排列，方向均为艏至艉。"));
    if(format_=="surface")details_->setText(uiText("曲线将拟合为控制网，不能无损还原原曲面。"));
    if(format_=="iges")details_->setText(uiText("输出 NURBS 曲面片，不是缝合实体。名称仅支持 ASCII。"));
    if(format_=="dxf3d")details_->setText(uiText("输出原侧三维剖面线和控制曲线，不含曲面，不补镜像。"));
    if(format_=="ghs")details_->setText(uiText("仅支持对称单体船，不含舱室。使用全部静水力图层；敞开甲板需选择封顶。"));
    if(format_=="arch_hll"||format_=="arch_app"){
        const auto refreshNotice=[this]{details_->setText(uiText("仅支持对称船体，使用全部静水力图层。")+(format_=="arch_app"?QString(uiText("\nAPP 不支持中文等非 ASCII 文字，将替换为 ?。")):QString{}));};
        refreshNotice();bindUiRefresh(details_,refreshNotice);
    }
    
    if(format_=="part"){symmetric_->hide();units_->setEnabled(false);details_->setText(importing_?uiText(""):uiText("导出所选面；未选面时按图层显隐选项导出。不含背景图、工程参数和跨部件约束。"));}
    if(format_=="waves"){symmetric_->hide();units_->clear();addUiItem(units_,"米 (m)","m");details_->setText(uiText("原点取 Michlet 第一船体中心。波高以设计水线为零，X 方向反转；波形不参与静水力。"));}
    if(format_=="vrml"){control_->show();details_->setText(uiText("材质、纹理和动画不导入。"));}
    if(format_=="addmass")details_->setText(uiText("输入 Y≥0 的对称半船。"));
    if(format_=="michlet")details_->setText(uiText("单体输入 Y≥0 半船；双体输入 Y>0 的完整片体，由图层镜像生成另一片体。"));
    details_->setObjectName("ExchangeNotice");
    const bool formatWarning=(!importing_&&(format_=="fef"||format_=="iges"||format_=="arch_app"||format_=="part"))||format_=="polycad"||format_=="surface"||format_=="vrml";
    details_->setMessage(details_->text(),formatWarning?StudioMessageRole::Warning:StudioMessageRole::Explanation);
    details_->setVisible(!details_->text().isEmpty());
    layout->addWidget(details_);status_=new StudioMessage(StudioMessageRole::Feedback);status_->setProperty("exchange_status",true);layout->addWidget(status_);if(!objSidebar)layout->addStretch();
    auto* buttons=new QDialogButtonBox;run_=buttons->addButton(importing?uiText("导入"):uiText("导出"),QDialogButtonBox::ActionRole);run_->setProperty("exchange_action",importing?"import":"export");
    if(!importing){
        const auto refreshFormat=[this]{setStudioFormatButton(run_,fs::cad::extension(format_),uiText("导出 ")+uiText(fs::cad::title(format_)));};
        refreshFormat();bindUiRefresh(run_,refreshFormat);
    }
    connect(run_,&QPushButton::clicked,this,&ExchangeDialog::submit);layout->addWidget(buttons);
    refreshState();
    exportTimeout_.setSingleShot(true);exportTimeout_.setInterval(90000);
    connect(&exportTimeout_,&QTimer::timeout,this,[this]{cancelled_=true;exportProcess_.kill();showFailure(uiText("导出超时，原文件未被替换。"));});
    connect(&exportProcess_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus){finishSections(code);});
    connect(&exportProcess_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart){exportTimeout_.stop();exporting_=false;showFailure(uiText("无法启动几何导出组件。"));}});
}
ExchangeDialog::~ExchangeDialog(){
    if(objLayersCancel_)objLayersCancel_->store(true,std::memory_order_relaxed);
    if(objToleranceCancel_)objToleranceCancel_->store(true,std::memory_order_relaxed);
    if(importCancel_)importCancel_->store(true,std::memory_order_relaxed);
    exportTimeout_.stop();cancelled_=true;exportProcess_.disconnect(this);if(exportProcess_.state()!=QProcess::NotRunning){exportProcess_.kill();exportProcess_.waitForFinished(2000);}
}
bool ExchangeDialog::busy()const{return session_.busy()||exporting_||importCancel_||importPending_||(printHull_&&printHull_->pending());}
void ExchangeDialog::refreshState(){
    const bool idle=!busy();run_->setEnabled(idle&&!objLayersLoading_&&(importing_||session_.ready()));
    if(importPath_)importPath_->setReadOnly(!idle);
    units_->setEnabled(idle&&!fileUnits(format_)&&!metricFormat(format_));for(auto* box:{axes_,control_,symmetric_,hidden_,binary_,closeDeck_})box->setEnabled(idle);
    if(objImportMode_){
        objImportCards_->setEnabled(idle);
        objLayerTree_->setEnabled(idle&&!objLayersLoading_);
        objInferCreases_->setEnabled(idle);
        objToleranceRow_->setEnabled(idle);
    }
    if(columns_)columns_->setEnabled(idle);if(rows_)rows_->setEnabled(idle);if(intermediate_)intermediate_->setEnabled(idle);
    for(auto* edit:references_)edit->setReadOnly(!idle);
    for(auto* box:referenceChoices_)box->setEnabled(idle);
    if(referenceChoices_.contains("hull_mode"))references_["separation"]->setEnabled(idle&&referenceChoices_["hull_mode"]->currentData().toString()!="monohull");
    if(format_=="coordinates"&&control_->isChecked()){symmetric_->setEnabled(false);hidden_->setEnabled(false);}
    if(printMode_){
        printMode_->setEnabled(idle);printHull_->setEnabled(idle);
        if(printMode_->isChecked()){printHull_->syncSource();units_->setEnabled(false);run_->setEnabled(idle&&session_.ready());}
    }
}
void ExchangeDialog::reject(){
    if(importCancel_){
        importCancel_->store(true,std::memory_order_relaxed);
        QDialog::reject();
    }else if(exporting_){cancelled_=true;exportProcess_.kill();status_->setMessage(uiText("正在取消导出…"),StudioMessageRole::Feedback,true);}else if(!session_.busy()&&!importPending_)QDialog::reject();else if(format_=="iges"){session_.cancelCad();status_->setMessage(uiText("正在取消曲面生成…"),StudioMessageRole::Feedback,true);}
}
QString ExchangeDialog::failure()const{
    const auto error=status_->property("error").toString();
    return error.isEmpty()&&printMode_&&printMode_->isChecked()?printHull_->failure():error;
}
void ExchangeDialog::showFailure(const QString& error){importPending_=false;pendingCadPath_.clear();const auto message=localizedExchangeError(error);status_->setProperty("error",message);status_->setMessage(message,StudioMessageRole::Warning);refreshState();}
void ExchangeDialog::imported(){if(importPending_){importPending_=false;succeeded_=true;accept();}}
void ExchangeDialog::receiveCad(const QJsonObject& data){
    if(pendingCadPath_.isEmpty())return;
    try{fs::cad::write(pendingCadPath_,"iges",*session_.model(),pendingCadOptions_,data);succeeded_=true;status_->setMessage(uiText("已导出：")+pendingCadPath_,StudioMessageRole::Feedback);pendingCadPath_.clear();}
    catch(const std::exception& e){pendingCadPath_.clear();showFailure(QString::fromUtf8(e.what()));}
}
QString ExchangeDialog::chooseExportPath(){
    const auto ext=fs::cad::extension(format_);
    const QFileInfo source(session_.sourcePath());
    const QString base=source.fileName().isEmpty()?uiText("未命名"):source.completeBaseName();
    const QString suggested=preferenceFileLocation("export",source.fileName().isEmpty()?base+"."+ext:source.dir().filePath(base+"."+ext));
    QFileDialog picker(this,windowTitle());
    picker.setOption(QFileDialog::DontConfirmOverwrite); // Confirm once, after validating the destination.
    if(testAttribute(Qt::WA_DontShowOnScreen))picker.setOption(QFileDialog::DontUseNativeDialog);
    picker.setAcceptMode(QFileDialog::AcceptSave);picker.setFileMode(QFileDialog::AnyFile);
    picker.setNameFilter(uiText(fs::cad::title(format_))+" (*."+ext+(format_=="iges"?" *.iges":"")+")");
    picker.setDefaultSuffix(ext);
    picker.setDirectory(QFileInfo(suggested).absolutePath());picker.selectFile(QFileInfo(suggested).fileName());
    return runStudioDialog(picker)==QDialog::Accepted?picker.selectedFiles().value(0):QString{};
}
void ExchangeDialog::submit(){
    if(busy())return;
    status_->setProperty("error",QString{});status_->clear();succeeded_=false;objFitEvidence_={};
    try{
        const auto source=session_.model();
        QString path;
        if(importing_){
            path=importPath_->text().trimmed();
            if(path.isEmpty())throw std::runtime_error(uiText("请先选择文件。"));
        }else{
            if(!session_.ready())return;
            path=chooseExportPath();
            if(path.isEmpty())return;
            if(source!=session_.model()||!session_.ready())throw std::runtime_error(uiText("模型版本已改变，请重新导出。"));
        }
        fs::exchange::Options o;o.units=units_->currentData().toString();o.legacyAxes=format_=="obj"&&axes_->isChecked();o.controlNet=(format_=="obj"||format_=="coordinates"||format_=="vrml")&&control_->isChecked();o.symmetric=symmetric_->isChecked();o.fullHull=symmetric_->isChecked();o.includeHidden=hidden_->isChecked();o.binaryStl=binary_->isChecked();
        o.faceIds=faceIds;o.textEncoding=encoding_?encoding_->currentData().toString():QString("cp1252");
        if(columns_)o.columns=columns_->value();if(rows_)o.rows=rows_->value();if(intermediate_)o.intermediateBulkheads=intermediate_->isChecked();
        if(importing_){
            if((format_=="part"||format_=="waves")&&!session_.ready())
                throw std::runtime_error(uiText("请先打开或新建工程，再追加部件或波形。"));
            if(objImportMode_){
                if(objLayersLoading_)return;
                if(!objLayersError_.isEmpty())throw std::runtime_error(objLayersError_.toUtf8().constData());
                const int mode=objImportMode_->checkedId();
                o.controlNet=mode==1;
                o.objLayerMode=fs::exchange::ObjLayerMode::Assigned;
                o.objAssignedLayers=objLayerTree_->assignments();
                o.inferObjCreases=objInferCreases_->isChecked();
                if(mode==2){
                    o.reconstructObj=true;
                    if(!objAutomaticTolerance_->isChecked()){
                        bool ok=false;
                        o.objToleranceMm=objTolerance_->text().trimmed().toDouble(&ok);
                        if(!ok||!std::isfinite(o.objToleranceMm)||o.objToleranceMm<=0||o.objToleranceMm>1e9)
                            throw std::runtime_error(uiText("请输入大于零的容差（mm）。"));
                    }
                    startObjFit(path,o);
                    return;
                }
            }
            importPrepared(fs::exchange::read(path,format_,o),path);
        }
        else{
            if(!session_.ready())return;
            if(QFileInfo(path).suffix().isEmpty())path+='.'+fs::cad::extension(format_);
            if(!fs::cad::acceptsExtension(format_,QFileInfo(path).suffix()))throw std::runtime_error(uiText("文件扩展名与所选导出格式不一致。"));
            if(!session_.sourcePath().isEmpty()&&QFileInfo(path).absoluteFilePath().compare(QFileInfo(session_.sourcePath()).absoluteFilePath(),Qt::CaseInsensitive)==0)throw std::runtime_error(uiText("请另选导出路径，避免覆盖当前 FBM 工程。"));
            if(QFileInfo::exists(path)&&(!confirmOverwrite||!confirmOverwrite(path)))return;
            if(source!=session_.model()||!session_.ready())throw std::runtime_error(uiText("模型版本已改变，请重新导出。"));
            if(sectionFormat(format_)){startSections(path);return;}
            if(format_=="iges"){pendingCadPath_=path;pendingCadOptions_=o;status_->setMessage(uiText("正在生成曲面…"),StudioMessageRole::Feedback,true);session_.cadPatches();return;}
            if(printMode_&&printMode_->isChecked()){
                printHull_->prepare([this,path]{
                    try{printHull_->save(path);succeeded_=true;status_->setMessage(uiText("已导出：")+path,StudioMessageRole::Feedback);}
                    catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
                });
                return;
            }
            else if(fs::cad::supports(format_))fs::cad::write(path,format_,*session_.model(),o);
            else fs::exchange::write(path,format_,*session_.model(),o);
            succeeded_=true;status_->setMessage(uiText("已导出：")+path,StudioMessageRole::Feedback);
        }
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}

void ExchangeDialog::scheduleObjLayers(){
    if(objLayersCancel_)objLayersCancel_->store(true,std::memory_order_relaxed);
    objLayersTimer_.stop();
    objLayerTree_->clear();
    objLayersError_.clear();
    objLayersLoading_=!importPath_->text().trimmed().isEmpty();
    if(objLayersLoading_)objLayersTimer_.start();
    refreshState();
}

void ExchangeDialog::readObjLayers(){
    const QString path=importPath_->text().trimmed();
    if(path.isEmpty())return;
    using Result=std::pair<QStringList,QString>;
    const auto cancel=std::make_shared<std::atomic_bool>(false);
    objLayersCancel_=cancel;
    auto promise=std::make_shared<QPromise<Result>>();
    auto* watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,cancel]{
        watcher->deleteLater();
        if(cancel->load(std::memory_order_relaxed))return;
        const auto result=watcher->result();
        objLayersLoading_=false;
        objLayersError_=result.second;
        objLayerTree_->setSources(result.first);
        refreshState();
    });
    promise->start();
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([path,cancel,promise]{
        Result result;
        try{result.first=fs::exchange::objLayerSources(path,cancel.get());}
        catch(const std::exception& error){result.second=QString::fromUtf8(error.what());}
        promise->addResult(std::move(result));
        promise->finish();
    });
}

void ExchangeDialog::estimateObjTolerance(){
    const auto path=importPath_->text().trimmed();
    fs::exchange::Options options;
    options.units=units_->currentData().toString();options.legacyAxes=axes_->isChecked();
    const auto cancel=objToleranceCancel_=std::make_shared<std::atomic_bool>(false);
    auto* watcher=new QFutureWatcher<double>(this);
    connect(watcher,&QFutureWatcher<double>::finished,this,[this,watcher,cancel]{
        const auto estimate=watcher->result();
        watcher->deleteLater();
        if(!cancel->load(std::memory_order_relaxed))
            static_cast<ObjToleranceRow*>(objToleranceRow_)->setEstimate(estimate);
    });
    QPromise<double> promise;
    watcher->setFuture(promise.future());
    QThreadPool::globalInstance()->start([promise=std::move(promise),path,options,cancel]()mutable{
        promise.start();double estimate=0.;
        try{estimate=fs::exchange::objAutomaticToleranceMm(path,options,cancel.get());}
        catch(const std::exception&){}
        promise.addResult(estimate);promise.finish();
    });
}

void ExchangeDialog::startObjFit(const QString& path,const fs::exchange::Options& options){
    const auto source=session_.model();
    const auto cancel=std::make_shared<std::atomic_bool>(false);
    importCancel_=cancel;
    status_->setMessage(uiText("正在重建船型并测量偏差…"),StudioMessageRole::Feedback,true);
    refreshState();
    auto promise=std::make_shared<QPromise<ObjFitResult>>();
    auto* watcher=new QFutureWatcher<ObjFitResult>(this);
    connect(watcher,&QFutureWatcher<ObjFitResult>::finished,this,[this,watcher,source,cancel,path]{
        watcher->deleteLater();
        if(cancel->load(std::memory_order_relaxed))return;
        auto result=watcher->result();
        if(!result.error.isEmpty()||!result.document.objFit){
            importCancel_.reset();
            showFailure(result.error.isEmpty()?uiText("船型重建失败。"):result.error);
            return;
        }
        const auto& fit=*result.document.objFit;
        const auto& model=result.document.model;
        objFitEvidence_={{"tolerance_mm",fit.toleranceMm},{"maximum_mm",fit.maximumMm},{"rms_mm",fit.rmsMm},
                         {"samples",qint64(fit.samples)},{"points",qint64(model.points.size())},
                         {"faces",qint64(model.faces.size())},{"precision",model.precision}};
        const auto description=QString(uiText("控制点 %1 个，控制面 %2 个。\n最大采样偏差 %3 mm，均方根偏差 %4 mm。\n允许偏差 %5 mm。导入重建船型？"))
            .arg(model.points.size()).arg(model.faces.size()).arg(fit.maximumMm,0,'g',6)
            .arg(fit.rmsMm,0,'g',6).arg(fit.toleranceMm,0,'g',6);
        const bool proceed=confirmStudioAction(this,uiText("OBJ 船型重建结果"),description);
        importCancel_.reset();
        status_->clear();
        refreshState();
        if(!proceed)return;
        const auto current=session_.model();
        if(bool(source)!=bool(current)||(source&&(source->documentId!=current->documentId||source->revision!=current->revision))){
            showFailure(uiText("当前模型已改变，请重新导入。"));
            return;
        }
        try{importPrepared(std::move(result.document),path);}
        catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
    });
    promise->start();
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise,cancel,path,options]{
        ObjFitResult result;
        try{result.document=fs::exchange::read(path,"obj",options,cancel.get());}
        catch(const std::exception& e){result.error=QString::fromUtf8(e.what());}
        promise->addResult(std::move(result));
        promise->finish();
    });
}

void ExchangeDialog::importPrepared(fs::exchange::Document document,const QString& path){
    const bool append=format_=="part"||format_=="waves";
    if(append&&!session_.ready())throw std::runtime_error(uiText("请先打开或新建工程，再追加部件或波形。"));
    if(document.discardedZeroAreaFaces&&!confirmStudioAction(this,uiText("导入 VRML"),
        QString(uiText("文件含 %1 个零面积面。忽略这些面并导入其余模型？")).arg(document.discardedZeroAreaFaces)))return;
    auto data=document.bridgeData();
    if(format_=="waves"){
        const auto refs=referenceValues();
        data["wave_patch"]=true;
        data["wave_origin_m"]=QJsonObject{{"x",refs["origin_x"]},{"y",refs["origin_y"]}};
    }
    auto apply=[guard=QPointer<ExchangeDialog>(this),data,append,path]{
        if(!guard||!guard->isVisible())return;
        guard->status_->setMessage(uiText("正在生成模型…"),StudioMessageRole::Feedback,true);
        guard->importPending_=true;
        const bool started=guard->importInNewTab
            ?guard->importInNewTab(data,path):guard->session_.importModel(data,append);
        if(!started&&guard->importPending_)guard->showFailure(uiText("无法准备导入数据。"));
        guard->refreshState();
    };
    if(beforeImport&&!append&&!importInNewTab)beforeImport(apply);
    else apply();
}
QJsonObject ExchangeDialog::referenceValues() const {
    QJsonObject refs;for(auto it=references_.cbegin();it!=references_.cend();++it){if(!it.value()->isEnabled())continue;auto text=it.value()->text().trimmed();if(text.isEmpty()&&it.value()->property("exchange_optional").toBool())continue;bool ok=false;double value=text.toDouble(&ok);if(!ok||!std::isfinite(value)||std::abs(value)>1e9)throw std::runtime_error(uiText("参数必须是有限数值，请使用小数点。"));refs[it.key()]=value;}
    for(auto it=referenceChoices_.cbegin();it!=referenceChoices_.cend();++it)refs[it.key()]=QJsonValue::fromVariant(it.value()->currentData());
    return refs;
}
void ExchangeDialog::startSections(const QString& path){
    exportWork_=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/FreeShip26-export-XXXXXX");
    if(!exportWork_->isValid())throw std::runtime_error(uiText("无法创建导出临时目录。"));
    exportPath_=path;exportDocument_=session_.model()->documentId;exportRevision_=session_.model()->revision;
    auto refs=referenceValues();
    refs["include_hidden"]=hidden_->isChecked();refs["full_hull"]=symmetric_->isChecked();
    QJsonObject request{{"format",format_},{"references",refs},{"model",session_.modelPath()},{"input",QFileInfo(session_.modelPath()).dir().filePath("native-input.json")},
        {"document_id",exportDocument_},{"revision",double(exportRevision_)},{"units",units_->currentData().toString()},
        {"close_deck",closeDeck_->isChecked()},{"output",exportWork_->filePath("hull.export")}};
    QSaveFile file(exportWork_->filePath("request.json"));auto data=QJsonDocument(request).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())throw std::runtime_error(uiText("无法准备导出请求。"));
    exportProcess_.setProgram(fs::platform::siblingExecutable("FreeShipGeometryExport"));
    exportProcess_.setArguments({file.fileName(),exportWork_->filePath("report.json"),QString::number(QCoreApplication::applicationPid())});
    exportProcess_.setStandardOutputFile(QProcess::nullDevice());exportProcess_.setStandardErrorFile(exportWork_->filePath("worker.log"));
#ifdef Q_OS_WIN
    exportProcess_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=0x08000000;});
#endif
    cancelled_=false;exporting_=true;status_->setMessage(uiText("正在生成横剖断面…"),StudioMessageRole::Feedback,true);refreshState();exportTimeout_.start();exportProcess_.start();
}
void ExchangeDialog::finishSections(int code){
    if(!exporting_)return;exportTimeout_.stop();exporting_=false;
    if(cancelled_){if(failure().isEmpty())QDialog::reject();else refreshState();return;}
    try{
        QFile reportFile(exportWork_->filePath("report.json"));if(!reportFile.open(QIODevice::ReadOnly)||reportFile.size()>1024*1024)throw std::runtime_error(uiText("几何导出组件未返回有效结果。"));
        auto report=QJsonDocument::fromJson(reportFile.readAll()).object();
        if(code||report["status"]!="pass")throw std::runtime_error(report["error"].toString(uiText("几何导出失败。")).toStdString());
        if(!session_.ready()||session_.model()->documentId!=exportDocument_||session_.model()->revision!=exportRevision_||report["document_id"]!=exportDocument_||report["revision"].toDouble(-1)!=double(exportRevision_))throw std::runtime_error(uiText("模型版本已改变，请重新导出。"));
        QFile result(exportWork_->filePath("hull.export"));if(!result.open(QIODevice::ReadOnly)||result.size()>256*1024*1024)throw std::runtime_error(uiText("无法读取几何导出文件。"));QCryptographicHash hash(QCryptographicHash::Sha256);
        if(!hash.addData(&result)||report["sha256"].toString().toLatin1()!=hash.result().toHex())throw std::runtime_error(uiText("几何导出结果校验失败。"));result.close();
        ModelSession::atomicCopy(result.fileName(),exportPath_);succeeded_=true;
        if(format_=="michlet"||format_=="addmass")status_->setMessage(QString(uiText("已导出 %1 个船体，每个 %2 个断面；每个船体排水体积 %3 m³。\n%4")).arg(report["hulls"].toInt()).arg(report["stations"].toInt()).arg(report["volume_m3"].toDouble(),0,'g',10).arg(exportPath_),StudioMessageRole::Feedback);
        else status_->setMessage(sectionsFeedbackText(report["stations"].toInt(),report["points"].toInt(),report["closed_decks"].toArray().size(),report["empty_stations"].toArray().size(),exportPath_),StudioMessageRole::Feedback);refreshState();
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
