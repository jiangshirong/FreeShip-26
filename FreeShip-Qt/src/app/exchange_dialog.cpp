#include "preferences_store.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "exchange_dialog.h"
#include "model_session.h"
#include "core/model_exchange.h"
#include "core/cad_export.h"
#include "studio_ui.h"
#include "grid_preview_panel.h"
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
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QSaveFile>
#include <QSpinBox>
#include <QTabWidget>
#include <cmath>
namespace {
bool sectionFormat(const QString& f){return f=="dxfcrease"||f=="offset_table"||f=="michlet"||f=="addmass"||f=="ghs"||f=="arch_hll"||f=="arch_app"||f=="pam";}
bool metricFormat(const QString& f){return f=="michlet"||f=="addmass"||f=="arch_hll"||f=="arch_app"||f=="pam";}
bool fileUnits(const QString& f){return f=="part"||f=="waves"||f=="fef"||f=="chines"||f=="surface"||f=="hull";}
bool chineFormat(const QString& f){return f=="chines"||f=="carene"||f=="hull";}
}

ExchangeDialog::ExchangeDialog(ModelSession& session,bool importing,QString format,QWidget* parent)
    :QDialog(parent),session_(session),importing_(importing),format_(std::move(format)){
    setWindowTitle((importing?"导入 ":"导出 ")+fs::cad::title(format_));resize(620,460);
    confirmOverwrite=[this](const QString& path){return QMessageBox::question(this,"替换文件","文件已存在，是否替换？\n"+path,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;};
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(22,18,22,18);layout->setSpacing(12);
    auto* form=new QFormLayout;auto* fileRow=new QHBoxLayout;path_=new QLineEdit;path_->setProperty("exchange_field","path");
    auto* browse=new QPushButton(studioIcon(importing?"Open":"ExportFile"),"浏览…");fileRow->addWidget(path_,1);fileRow->addWidget(browse);form->addRow("文件",fileRow);
    connect(browse,&QPushButton::clicked,this,[this]{if(busy())return;auto ext=fs::cad::extension(format_);auto filter=fs::cad::title(format_)+" (*."+ext+(format_=="iges"?" *.iges":"")+")";auto p=importing_?QFileDialog::getOpenFileName(this,windowTitle(),path_->text().isEmpty()?preferenceDirectory(importing_?"import":"export"):path_->text(),filter):QFileDialog::getSaveFileName(this,windowTitle(),path_->text().isEmpty()?preferenceDirectory(importing_?"import":"export"):path_->text(),filter,nullptr,QFileDialog::DontConfirmOverwrite);if(!p.isEmpty()){if(!importing_&&QFileInfo(p).suffix().isEmpty())p+='.'+ext;path_->setText(p);}});
    units_=new QComboBox;units_->setProperty("exchange_field","units");for(auto pair:{std::pair{"米 (m)","m"},{"毫米 (mm)","mm"},{"厘米 (cm)","cm"},{"英尺 (ft)","ft"},{"英寸 (in)","in"}})units_->addItem(pair.first,pair.second);
    if(!importing&&session.model()&&session.model()->units=="imperial")units_->setCurrentIndex(3);
    form->addRow("文件中的长度单位",units_);
    if(format_=="part"){encoding_=new QComboBox;encoding_->setProperty("exchange_field","text_encoding");for(auto pair:{std::pair{"UTF-8","utf8"},{"西欧 Windows-1252","cp1252"},{"俄语 Windows-1251","cp1251"},{"简体中文 GBK","cp936"}})encoding_->addItem(pair.first,pair.second);if(importing_){auto current=session.model()?session.model()->textEncoding:QString("cp1252");int i=encoding_->findData(current);if(i>=0)encoding_->setCurrentIndex(i);}form->addRow("部件文字编码（与来源/接收软件一致）",encoding_);}
    auto check=[&](QString caption,QString key){auto* c=new QCheckBox(caption);c->setProperty("exchange_field",key);form->addRow(c);return c;};
    axes_=check("按旧版 FreeShip OBJ 的轴顺序 (Y, Z, X)","legacy_axes");
    control_=check(importing?"作为控制网导入，允许细分平滑":"导出控制网","control_net");
    symmetric_=check(importing?"文件仅有半船，按中心线镜像":"补全对称图层的另一舷","symmetric");symmetric_->setChecked(!importing);
    hidden_=check("包含隐藏图层","include_hidden");binary_=check("二进制 STL","binary_stl");binary_->setChecked(true);
    closeDeck_=check("将敞开甲板按水平线封到中心线","close_deck");closeDeck_->setVisible(format_=="ghs"||format_=="arch_hll"||format_=="arch_app"||format_=="pam");
    axes_->setVisible(format_=="obj");control_->setVisible(format_=="obj");hidden_->setVisible(!importing&&format_!="fef");binary_->setVisible(!importing&&format_=="stl");symmetric_->setVisible(format_!="fef");units_->setEnabled(format_!="fef");
    if(format_=="coordinates"){control_->setVisible(true);control_->setText("导出原控制点（不补镜像，不按图层显隐筛选）");connect(control_,&QCheckBox::toggled,this,[this](bool b){hidden_->setEnabled(!b);symmetric_->setEnabled(!b);});}
    if(format_=="offset_table"||format_=="offsets"||format_=="dxf3d"){hidden_->hide();symmetric_->hide();}
    if(format_=="ghs"){hidden_->hide();symmetric_->hide();units_->clear();units_->addItem("米 (m)","m");units_->addItem("英尺 (ft)","ft");if(session.model()&&session.model()->units=="imperial")units_->setCurrentIndex(1);}
    if(metricFormat(format_)){
        hidden_->hide();symmetric_->hide();units_->clear();units_->addItem("米 (m)","m");
        const auto model=session.model();const double scale=model&&model->units=="imperial"?.3048:1.;
        const double length=model?model->length*scale:0;
        auto number=[&](const QString& label,const QString& key,double value){
            auto* edit=new QLineEdit(QString::number(value,'g',17));edit->setProperty("exchange_number",key);
            references_.insert(key,edit);form->addRow(label+" (m)",edit);
        };
        if(format_=="michlet"||format_=="addmass"){
            QFormLayout* fields=form;QTabWidget* tabs=nullptr;
            auto tab=[&](const QString& title){auto* page=new QWidget;fields=new QFormLayout(page);fields->setContentsMargins(14,16,14,16);fields->setVerticalSpacing(12);tabs->addTab(page,title);};
            tabs=new QTabWidget;tabs->setProperty("exchange_tabs",true);form->addRow(tabs);tab("船体几何");
            auto parameter=[&](QString label,QString key,double value){auto* edit=new QLineEdit(QString::number(value,'g',12));edit->setProperty("exchange_number",key);references_.insert(key,edit);fields->addRow(label,edit);};
            auto automatic=[&](QString label,QString key){parameter(label,key,0);auto* edit=references_[key];edit->clear();edit->setProperty("exchange_optional",true);edit->setPlaceholderText("自动：按实际水下船长");};
            auto choice=[&](QString label,QString key){auto* box=new QComboBox;box->setProperty("exchange_parameter",key);referenceChoices_[key]=box;fields->addRow(label,box);return box;};
            parameter("横剖站数（奇数）","stations",format_=="michlet"?31:21);parameter("水线数（奇数）","waterlines",format_=="michlet"?21:11);
            parameter("吃水 (m)","draft",model?model->draft*scale:0);
            if(format_=="michlet"){
                auto* mode=choice("船体布置","hull_mode");mode->addItem("单体船","monohull");mode->addItem("单体船，输出为双体船","duplicate");mode->addItem("已建好的双体船","catamaran");
                parameter("片体中心面间距 (m)","separation",0);references_["separation"]->setToolTip("单体复制：设置两片体的间距。已有双体：必须等于模型中两片体中心面的实际间距。");
                connect(mode,&QComboBox::currentIndexChanged,this,[this]{refreshState();});
                tab("环境与计算");parameter("水深 (m)","depth",10000);parameter("重力加速度 (m/s²)","gravity",9.80665);
                parameter("起始速度 (m/s)","start_speed",1);parameter("结束速度 (m/s)","end_speed",5);parameter("速度数量","speeds",50);parameter("运动黏度 (10⁻⁶ m²/s)","viscosity",1.18831);
                auto* friction=choice("摩擦方法","friction");friction->addItem("无",0);friction->addItem("ITTC 1957",1);friction->addItem("Grigson",2);friction->setCurrentIndex(1);
                auto* factor=choice("形状因子方法","form_factor");factor->addItem("无",0);factor->addItem("Dual",3);
                tab("波形区域");automatic("扇形起始半径 R0 (m)","r0");automatic("扇形结束半径 R1 (m)","r1");parameter("扇形半角 β (°)","beta",22.5);parameter("径向采样数","nr",100);parameter("角向采样数","nbeta",100);
                automatic("矩形 X0 (m)","wave_x0");automatic("矩形 X1 (m)","wave_x1");automatic("矩形 Y0 (m)","wave_y0");automatic("矩形 Y1 (m)","wave_y1");parameter("X 采样数","wave_nx",100);parameter("Y 采样数","wave_ny",100);
                tabs->setCurrentIndex(0);
            }
            else{parameter("面元数量","panels",1000);parameter("面积比例 (%)","area_percent",100);parameter("计算精度","accuracy",.2);parameter("面元（1 三角形 / 2 梯形）","panel_type",1);parameter("漂角 (°)","drift",0);parameter("航速 (kn)","velocity_knots",10);parameter("角速度 (1/s)","omega",0);parameter("对象类型编号","object_type",1);}
            if(format_=="addmass")parameter("水深 (m)","depth",10000);
            if(!importing_){preview_=new GridPreviewPanel(session_,format_,[this]{return referenceValues();});tabs->addTab(preview_,"型值预览");
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
    if((format_=="fef"||format_=="part")&&importing_){units_->clear();units_->addItem("使用文件内记录的单位");}
    if(format_=="polycad"||format_=="surface"||chineFormat(format_))symmetric_->setChecked(true);
    if(chineFormat(format_))symmetric_->hide();
    if(importing&&format_!="fef"&&format_!="part"&&format_!="waves"&&fileUnits(format_)){units_->clear();units_->addItem(format_=="hull"?"英寸（转换为英尺，使用英制工程）":"使用文件首行：0 为米，1 为英尺");}
    if(importing&&(format_=="chines"||format_=="carene"||format_=="surface")){
        columns_=new QSpinBox;columns_->setProperty("exchange_count","columns");columns_->setRange(format_=="surface"?0:2,100);columns_->setValue(format_=="surface"?0:8);
        if(format_=="surface")columns_->setSpecialValueText("按原曲线数量（至少 3 列）");
        form->addRow(format_=="surface"?"拟合网格列数":"每条折角线的拟合点数",columns_);
        if(format_=="surface"){rows_=new QSpinBox;rows_->setProperty("exchange_count","rows");rows_->setRange(3,100);rows_->setValue(10);form->addRow("拟合网格行数",rows_);}
    }
    if(importing&&format_=="hull"){intermediate_=check("使用文件中的 13 个中间肋位（取消后使用 5 个主肋位）","intermediate_bulkheads");intermediate_->setChecked(true);}
    if(importing&&format_=="waves"){
        auto model=session.model();double scale=model&&model->units=="imperial"?.3048:1.;
        for(auto key:{QString("origin_x"),QString("origin_y")}){auto* edit=new QLineEdit(QString::number(key=="origin_x"&&model?.5*model->length*scale:0,'g',17));edit->setProperty("exchange_number",key);references_[key]=edit;form->addRow(key=="origin_x"?"波形原点 · 模型 X (m)":"波形原点 · 模型 Y (m)",edit);}
    }
    layout->addLayout(form);details_=new QLabel;details_->setWordWrap(true);
    if(format_=="fef")details_->setText("FEF 保留原控制网、折痕、图层属性、单位及主尺度。不会按显示状态截取或补齐镜像。\n不包含剖面线、标记、背景图和约束；完整工程请保存为 FBM。");
    else details_->setText(importing?"OBJ / STL 没有统一的单位约定，请选择来源文件使用的单位。默认保留多边形外形；完整船体不要再次镜像。OBJ 保留分组，不读取外部材质或纹理。":"导出整个模型的几何，不受缩放或视角影响。默认输出可见图层的完整曲面；OBJ 可改为控制网。OBJ / STL 不保存完整工程信息。\n单位和坐标方向请与接收软件保持一致。");
    if(format_=="polycad")details_->setText("读取 Polycad 的 B 样条网格、曲面列表及船型坐标网格，作为细分控制网；保留坐标、曲面名称和已给出的主尺度。请核对来源单位及是否只有半船。\n沿用旧版导入方式，节点与阶次不用于重建原 B 样条曲面，因此不是原曲面的无损转换。Gmsh .geo 文件不适用。");
    if(chineFormat(format_))details_->setText("根据折角线拟合半船，保留板带分层、折痕、控制曲线及原始曲线标记；艏艉和船底补到中心线，甲板保持开放。\n输入应位于 Y≥0 一侧，折角线按龙骨至舷缘排列，每条方向一致、从艏至艉。沿用旧半船规则，拟合越过中心线时收回至 Y=0。\n"+(format_=="hull"?QString("读取 Hull 的 Y/Z/X 英寸记录、站位、船名和设计者；统一换算为英尺，保留英制工程和淡水密度默认值。"):format_=="carene"?QString("Carene XYZ 使用 LINE 段落，单位由上方选择。"):QString("文本首行为单位 0/1，后续每行 X Y Z，曲线之间用空行分隔，可用 EOF 结束。"))+"\n拟合结果需要对照曲线标记检查；文件不提供设计吃水，请在工程设置中填写。");
    if(format_=="surface")details_->setText("用一组方向一致的横向曲线拟合平滑曲面，保留原曲线作为标记。文本首行为单位 0（米）或 1（英尺），每行 X Y Z，空行分隔曲线，可用 EOF 结束。\n行数决定每条曲线的采样数，列数决定沿曲线排列方向的拟合点数；沿用旧版双向拟合和中心线限制规则。\n生成的是拟合控制网，不能把它当作原曲面的无损副本。导入后请检查原曲线标记、对称选项和工程吃水。");
    if(format_=="iges")details_->setText("将控制网转换为 NURBS 曲面片，保留旧版曲面转换算法。可选择原侧或补全对称图层，不受屏幕半船显示影响。\nIGES 曲面片不等于缝合实体；特殊顶点处的曲面连续性需在接收软件中检查。名称等说明字段使用 ASCII，几何坐标不截短。");
    if(format_=="dxfmesh")details_->setText("导出完整细分三角面（3DFACE），保留图层、朝向和单位。不受镜头和半船显示影响。精度沿用当前模型的细分档位。");
    if(format_=="dxf3d")details_->setText("导出全部已定义剖面和控制曲线的原侧几何，使用三维样条实体保留曲线和折角，不按屏幕显隐筛选，也不额外镜像。\n不是型线图的二维排版，也不包含曲面或折痕边线。");
    if(format_=="offsets")details_->setText("输出全部已定义剖面和控制曲线的原侧型值点、分段与折角标记，不额外镜像。坐标保留完整数值，不按当前视图裁切。\n这是三维点列，不是站线与水线交点组成的矩形型值表。");
    if(format_=="coordinates")details_->setText("默认输出细分曲面的坐标点，可改为原控制点。完全重合的坐标只写一次，不按距离合并。每行 X Y Z，单位由上方选择。\n点列不包含面连接关系；完整模型交接请使用 OBJ 或 FEF。");
    if(format_=="ghs")details_->setText("按现有横剖站导出对称单体船，使用全部参与静水力的图层，包含隐藏图层；不含舱室和附体部件划分。\n保留模型原点，纵向取 −X；曲线离散弦误差不超过 0.1 mm，保留原型值点和折角。横剖站间距仍由剖面线设置决定。\n敞开甲板需明确选择水平封顶；上下分离且收在中心线的轮廓可保留；横向缺口和非对称船体会停止导出。");
    if(format_=="arch_hll"||format_=="arch_app")details_->setText("导出已定义横剖站的完整对称船体，包含全部静水力图层；保留原点、高度、折角，曲线弦误差 0.1 mm。敞开甲板需明确选择水平封顶。\n垂线与吃水标志均为纵向位置，不是吃水深度。默认按 X=0 至船长填写，请与实际工程核对。"+(format_=="arch_hll"?QString("\nHLL 使用 UTF-8，保留中文名称。"):QString("\nAPP 说明文字使用 ASCII；非 ASCII 字符以 ? 代替。")));
    if(format_=="pam")details_->setText("输出旧版 PAM 几何模板，使用米和 kg/m³；纵向为中船位置减模型 X，垂向为模型 Z 减设计水线 Z，横向为半宽。\n保留全部型值点和折角；不再压缩成 10 个采样点。包含全部静水力图层，敞开甲板需明确选择水平封顶。\n模板中的计算参数沿用旧版，使用前需按接收程序核对；此入口只输出文件，不运行附加质量计算。");
    if(format_=="part"){symmetric_->hide();units_->setEnabled(false);details_->setText(importing_?"追加到当前工程，自动换算长度、材料密度和板厚；保留图层、折痕、锁定点及控制曲线，可撤销。文字编码须与来源文件一致。":"导出已选中的面；未选择面时导出可见图层（可勾选包含隐藏图层）。保留控制网、折痕、锁定点和完整控制曲线；不含背景图、工程参数和跨部件约束。");}
    if(format_=="waves"){symmetric_->hide();units_->clear();units_->addItem("米 (m)","m");details_->setText("读取矩形或扇形波高文件，完整追加为非静水力、非型线图图层。填写 Michlet 第一船体中心在本模型中的坐标；单体默认 X 为半船长、Y 为 0，已有双体通常 Y 为负半间距。\n波高叠加到设计水线，X 方向反转；不丢弃零波高区域，不自动镜像，不平滑网格。可撤销。");}
    if(format_=="vrml"){control_->show();details_->setText("读取 VRML 1/2 的静态 IndexedFaceSet，支持层级变换、DEF/USE 和面朝向。默认保留多边形外形，可选择作为平滑控制网。保留文件 XYZ 轴，请核对单位与船艏方向。材质和纹理不转换；未知几何或动态节点会拒绝整个导入，保留当前工程。");}
    if(format_=="addmass")details_->setText("按当前精度的水下曲面生成等距型值表，艏至艉、船底至水线，使用米。支持 Y≥0 的对称单体半船；水下敞开边会拒绝导出。水密度取工程设置。此入口只写计算输入文件，不运行求解器。对象类型编号须按所用 AddMass 版本核对。");
    if(format_=="michlet")details_->setText("单体模式读取 Y≥0 的半船；已有双体模式读取 Y>0 的一个完整片体，其图层须镜像生成另一片体。每个片体须关于自身中心面对称。\n按当前精度生成型值，不改动模型。Michlet 要求艏端半宽为零；平艏、片体相交或水下敞开边会停止输出。波形区域以第一片体中心为原点，X 向艉、Y 向右舷。\n水密度取工程设置。此入口只写计算输入，不运行求解器。");
    if(format_=="offset_table")details_->setText("以已定义横剖站为行、水线为列，保存交点的 Y 型值及所选单位。空单元格表示没有交点；同格多个交点以分号保留，不只取最外点。使用完整原侧曲线，不受镜头裁切影响。");
    if(format_=="dxfcrease")details_->setText("导出当前精度细分曲面的折痕和边界线段，可包含隐藏图层并补全对称另一舷。保留全部线段，不按屏幕简化。不是原控制多边形，也不包含剖面线。");
    layout->addWidget(details_);status_=new QLabel;status_->setWordWrap(true);status_->setProperty("exchange_status",true);layout->addWidget(status_);layout->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close);buttons->button(QDialogButtonBox::Close)->setText("关闭");run_=buttons->addButton(importing?"导入":"导出",QDialogButtonBox::ActionRole);run_->setProperty("exchange_action",importing?"import":"export");
    connect(run_,&QPushButton::clicked,this,&ExchangeDialog::submit);connect(buttons,&QDialogButtonBox::rejected,this,&ExchangeDialog::reject);layout->addWidget(buttons);
    refreshState();
    exportTimeout_.setSingleShot(true);exportTimeout_.setInterval(90000);
    connect(&exportTimeout_,&QTimer::timeout,this,[this]{cancelled_=true;exportProcess_.kill();showFailure("导出超时，原文件未被替换。");});
    connect(&exportProcess_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus){finishSections(code);});
    connect(&exportProcess_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart){exportTimeout_.stop();exporting_=false;showFailure("无法启动几何导出组件。");}});
}
ExchangeDialog::~ExchangeDialog(){if(exportProcess_.state()!=QProcess::NotRunning){exportProcess_.kill();exportProcess_.waitForFinished(2000);}}
bool ExchangeDialog::busy()const{return session_.busy()||exporting_;}
void ExchangeDialog::refreshState(){
    const bool idle=!busy();run_->setEnabled(idle&&(importing_||session_.ready()));path_->setReadOnly(!idle);
    units_->setEnabled(idle&&!fileUnits(format_)&&!metricFormat(format_));for(auto* box:{axes_,control_,symmetric_,hidden_,binary_,closeDeck_})box->setEnabled(idle);
    if(columns_)columns_->setEnabled(idle);if(rows_)rows_->setEnabled(idle);if(intermediate_)intermediate_->setEnabled(idle);
    for(auto* edit:references_)edit->setReadOnly(!idle);
    for(auto* box:referenceChoices_)box->setEnabled(idle);
    if(referenceChoices_.contains("hull_mode"))references_["separation"]->setEnabled(idle&&referenceChoices_["hull_mode"]->currentData().toString()!="monohull");
    if(format_=="coordinates"&&control_->isChecked()){symmetric_->setEnabled(false);hidden_->setEnabled(false);}
}
void ExchangeDialog::reject(){if(exporting_){cancelled_=true;exportProcess_.kill();status_->setText("正在取消导出…");}else if(!session_.busy())QDialog::reject();else if(format_=="iges"){session_.cancelCad();status_->setText("正在取消曲面生成…");}}
QString ExchangeDialog::failure()const{return status_->property("error").toString();}
void ExchangeDialog::showFailure(const QString& error){status_->setProperty("error",error);status_->setText(error);status_->setStyleSheet("color:#9b3030");refreshState();}
void ExchangeDialog::imported(){succeeded_=true;accept();}
void ExchangeDialog::receiveCad(const QJsonObject& data){
    if(pendingCadPath_.isEmpty())return;
    try{fs::cad::write(pendingCadPath_,"iges",*session_.model(),pendingCadOptions_,data);succeeded_=true;status_->setText("已导出："+pendingCadPath_);pendingCadPath_.clear();}
    catch(const std::exception& e){pendingCadPath_.clear();showFailure(QString::fromUtf8(e.what()));}
}
void ExchangeDialog::submit(){
    if(busy())return;
    status_->setProperty("error",QString{});status_->clear();status_->setStyleSheet({});succeeded_=false;
    try{
        auto path=path_->text().trimmed();if(path.isEmpty())throw std::runtime_error("请先选择文件。");
        fs::exchange::Options o;o.units=units_->currentData().toString();o.legacyAxes=format_=="obj"&&axes_->isChecked();o.controlNet=(format_=="obj"||format_=="coordinates"||format_=="vrml")&&control_->isChecked();o.symmetric=symmetric_->isChecked();o.fullHull=symmetric_->isChecked();o.includeHidden=hidden_->isChecked();o.binaryStl=binary_->isChecked();
        o.faceIds=faceIds;o.textEncoding=encoding_?encoding_->currentData().toString():QString("cp1252");
        if(columns_)o.columns=columns_->value();if(rows_)o.rows=rows_->value();if(intermediate_)o.intermediateBulkheads=intermediate_->isChecked();
        if(importing_){const bool append=format_=="part"||format_=="waves";if(append&&!session_.ready())throw std::runtime_error("请先打开或新建工程，再追加部件或波形。");auto d=fs::exchange::read(path,format_,o);auto data=d.bridgeData();if(format_=="waves"){auto refs=referenceValues();data["wave_patch"]=true;data["wave_origin_m"]=QJsonObject{{"x",refs["origin_x"]},{"y",refs["origin_y"]}};}auto submit=[this,data,append]{status_->setText("正在生成模型…");session_.importModel(data,append);refreshState();};if(beforeImport&&!append)beforeImport(submit);else submit();}
        else{
            if(!session_.ready())return;
            if(QFileInfo(path).suffix().isEmpty()){path+='.'+fs::cad::extension(format_);path_->setText(path);}
            if(!fs::cad::acceptsExtension(format_,QFileInfo(path).suffix()))throw std::runtime_error("文件扩展名与所选导出格式不一致。");
            if(!session_.sourcePath().isEmpty()&&QFileInfo(path).absoluteFilePath().compare(QFileInfo(session_.sourcePath()).absoluteFilePath(),Qt::CaseInsensitive)==0)throw std::runtime_error("请另选导出路径，避免覆盖当前 FBM 工程。");
            if(QFileInfo::exists(path)&&(!confirmOverwrite||!confirmOverwrite(path)))return;
            if(sectionFormat(format_)){startSections(path);return;}
            if(format_=="iges"){pendingCadPath_=path;pendingCadOptions_=o;status_->setText("正在生成曲面…");session_.cadPatches();return;}
            if(fs::cad::supports(format_))fs::cad::write(path,format_,*session_.model(),o);
            else fs::exchange::write(path,format_,*session_.model(),o);
            succeeded_=true;status_->setText("已导出："+path);
        }
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
QJsonObject ExchangeDialog::referenceValues() const {
    QJsonObject refs;for(auto it=references_.cbegin();it!=references_.cend();++it){if(!it.value()->isEnabled())continue;auto text=it.value()->text().trimmed();if(text.isEmpty()&&it.value()->property("exchange_optional").toBool())continue;bool ok=false;double value=text.toDouble(&ok);if(!ok||!std::isfinite(value)||std::abs(value)>1e9)throw std::runtime_error("参数必须是有限数值，请使用小数点。");refs[it.key()]=value;}
    for(auto it=referenceChoices_.cbegin();it!=referenceChoices_.cend();++it)refs[it.key()]=QJsonValue::fromVariant(it.value()->currentData());
    return refs;
}
void ExchangeDialog::startSections(const QString& path){
    exportWork_=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/FreeShip26-export-XXXXXX");
    if(!exportWork_->isValid())throw std::runtime_error("无法创建导出临时目录。");
    exportPath_=path;exportDocument_=session_.model()->documentId;exportRevision_=session_.model()->revision;
    auto refs=referenceValues();
    refs["include_hidden"]=hidden_->isChecked();refs["full_hull"]=symmetric_->isChecked();
    QJsonObject request{{"format",format_},{"references",refs},{"model",session_.modelPath()},{"input",QFileInfo(session_.modelPath()).dir().filePath("native-input.json")},
        {"document_id",exportDocument_},{"revision",double(exportRevision_)},{"units",units_->currentData().toString()},
        {"close_deck",closeDeck_->isChecked()},{"output",exportWork_->filePath("hull.export")}};
    QSaveFile file(exportWork_->filePath("request.json"));auto data=QJsonDocument(request).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())throw std::runtime_error("无法准备导出请求。");
    exportProcess_.setProgram(QCoreApplication::applicationDirPath()+"/FreeShipGeometryExport.exe");
    exportProcess_.setArguments({file.fileName(),exportWork_->filePath("report.json"),QString::number(QCoreApplication::applicationPid())});
    exportProcess_.setStandardOutputFile(QProcess::nullDevice());exportProcess_.setStandardErrorFile(exportWork_->filePath("worker.log"));
#ifdef Q_OS_WIN
    exportProcess_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=0x08000000;});
#endif
    cancelled_=false;exporting_=true;status_->setText("正在生成横剖断面，可随时关闭并取消…");refreshState();exportTimeout_.start();exportProcess_.start();
}
void ExchangeDialog::finishSections(int code){
    if(!exporting_)return;exportTimeout_.stop();exporting_=false;
    if(cancelled_){if(failure().isEmpty())QDialog::reject();else refreshState();return;}
    try{
        QFile reportFile(exportWork_->filePath("report.json"));if(!reportFile.open(QIODevice::ReadOnly)||reportFile.size()>1024*1024)throw std::runtime_error("几何导出组件未返回有效结果。");
        auto report=QJsonDocument::fromJson(reportFile.readAll()).object();
        if(code||report["status"]!="pass")throw std::runtime_error(report["error"].toString("几何导出失败。").toStdString());
        if(!session_.ready()||session_.model()->documentId!=exportDocument_||session_.model()->revision!=exportRevision_||report["document_id"]!=exportDocument_||report["revision"].toDouble(-1)!=double(exportRevision_))throw std::runtime_error("模型版本已改变，请重新导出。");
        QFile result(exportWork_->filePath("hull.export"));if(!result.open(QIODevice::ReadOnly)||result.size()>256*1024*1024)throw std::runtime_error("无法读取几何导出文件。");QCryptographicHash hash(QCryptographicHash::Sha256);
        if(!hash.addData(&result)||report["sha256"].toString().toLatin1()!=hash.result().toHex())throw std::runtime_error("几何导出结果校验失败。");result.close();
        ModelSession::atomicCopy(result.fileName(),exportPath_);succeeded_=true;
        if(format_=="michlet"||format_=="addmass")status_->setText(QString("已导出 %1 个船体，每个 %2 个断面；每个船体排水体积 %3 m³。\n%4").arg(report["hulls"].toInt()).arg(report["stations"].toInt()).arg(report["volume_m3"].toDouble(),0,'g',10).arg(exportPath_));
        else status_->setText(QString("已导出 %1 个断面、%2 个点；水平封顶 %3 个，未与船体相交 %4 个。\n%5").arg(report["stations"].toInt()).arg(report["points"].toInt()).arg(report["closed_decks"].toArray().size()).arg(report["empty_stations"].toArray().size()).arg(exportPath_));refreshState();
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
