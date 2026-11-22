// Layout and credits preserved from Forms/FreeAboutDlg.pas (local FreeShip 26).
#pragma once
#include "message_motion.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "studio_read_ui.h"
#include "update_controller.h"
#include <QDialog>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QSysInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QResizeEvent>
#include <QHash>
#include <QClipboard>
#include <QDateTime>
#include <QTimer>
#include <QScreen>
#include <QToolButton>
#include <utility>
class AboutCreditsTable final:public QTableWidget {
public:
    using QTableWidget::QTableWidget;
    void refreshRowHeights(){
        for(int row=0;row<rowCount();++row){
            const bool spacer=item(row,2)&&item(row,2)->text().isEmpty();
            int height=qMax(38,sizeHintForRow(row)+10);
            if(auto* identity=cellWidget(row,2))height=qMax(height,identity->hasHeightForWidth()?identity->heightForWidth(columnWidth(2))+8:identity->sizeHint().height()+8);
            setRowHeight(row,spacer?18:height);
        }
    }
protected:
    void resizeEvent(QResizeEvent* event)override{
        QTableWidget::resizeEvent(event);
        setColumnWidth(0,132);setColumnWidth(1,72);
        setColumnWidth(2,(viewport()->width()-204)*44/100);
        refreshRowHeights();
    }
};
class AboutInformationTable final:public QTableWidget{
public:
    AboutInformationTable(int rows,bool legal):QTableWidget(rows,2),legal_(legal){
        setItemDelegate(new StudioWidgetCellDelegate(this));
        horizontalHeader()->hide();verticalHeader()->hide();setShowGrid(false);setAlternatingRowColors(false);setWordWrap(true);
        setEditTriggers(QAbstractItemView::NoEditTriggers);setSelectionMode(QAbstractItemView::NoSelection);
        horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
        for(int row=0;row<rows;++row)setRowHeight(row,legal?78:44);applyStudioScrollbars(this);
    }
protected:void resizeEvent(QResizeEvent* event)override{
        QTableWidget::resizeEvent(event);setColumnWidth(0,viewport()->width()*(legal_?60:31)/100);
        if(legal_)for(int row=0;row<rowCount();++row){
            int height=78;for(int column=0;column<columnCount();++column)if(auto* widget=cellWidget(row,column)){
                widget->ensurePolished();const auto preferred=widget->hasHeightForWidth()?widget->heightForWidth(columnWidth(column)):widget->sizeHint().height();
                height=qMax(height,preferred+8);
            }setRowHeight(row,height);
        }
    }
private:bool legal_;
};
inline void showFreeShipAbout(QWidget* parent){
    QDialog dialog(parent);dialog.setAttribute(Qt::WA_DontShowOnScreen,parent&&parent->testAttribute(Qt::WA_DontShowOnScreen));setStudioWindowIcon(&dialog,"Info");dialog.setObjectName("FreeShipAbout");dialog.setWindowTitle("FreeShip 26");
    const auto available=(parent?parent->screen():dialog.screen())->availableGeometry().adjusted(12,32,-12,-12);
    dialog.setMinimumSize(QSize(840,620).boundedTo(available.size()));
    dialog.resize(QSize(1040,900).boundedTo(available.size()));
    dialog.move(available.center()-QPoint(dialog.width()/2,dialog.height()/2));
    dialog.setStyleSheet("QDialog{background:#f7f8f4;color:#213b36} QLabel{color:#213b36} QTableWidget{background:white;color:#213b36;border:0;gridline-color:#e1e8e3} QHeaderView::section{background:#ebf0e9;color:#213b36;border:0;padding:12px;font-weight:bold}");
    auto font=QApplication::font();font.setPointSizeF(12);dialog.setFont(font);
    auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto* masthead=new QWidget;auto* mastheadLayout=new QHBoxLayout(masthead);
    masthead->setObjectName("AboutMasthead");masthead->setFixedHeight(112);
    mastheadLayout->setContentsMargins(24,20,24,20);mastheadLayout->setSpacing(16);mastheadLayout->addStretch();
    auto* logo=new QLabel;logo->setObjectName("AboutProductLogo");logo->setFixedSize(56,56);
    logo->setPixmap(QIcon(":/brand/freeship26.png").pixmap(QSize(56,56),dialog.devicePixelRatioF()));
    auto* title=new QLabel("FreeShip 26");title->setObjectName("AboutProductName");title->setStyleSheet("font-size:40px;color:#1e5349");
    mastheadLayout->addWidget(logo);mastheadLayout->addWidget(title);mastheadLayout->addStretch();layout->addWidget(masthead);
    auto* body=new QWidget;auto* bodyLayout=new QVBoxLayout(body);bodyLayout->setContentsMargins(32,0,32,20);bodyLayout->setSpacing(0);layout->addWidget(body,1);
    const QString authorSource=QStringLiteral("蒋世融(Jiang Shirong)");
    const QString deepseekName=QStringLiteral("deepseek-flash 1017");
    const QString linuxCredit=QStringLiteral("Claude Opus 5.5 + GPT-6.2 Astra");
    struct CreditRow {QString year,name,workSource;};
    const QList<CreditRow> rows{
        {"2005","Martijn van Engeland","创建 FREE!ship"},
        {},
        {"2007","Timoshenko Victor F","开发 FREE!ship Plus"},
        {"","Martijn van Engeland","翻译"},
        {},
        {"2015","Mark Malakanov","移植到 FPC"},
        {"","Tom (https://github.com/tvld)","新版可缩放图标"},
        {"","Sönke J. Peters","功能改进与问题修复"},
        {},
        {"2026","GPT-6.1 Sol","界面美化"},
        {"","GPT-6 Astra","迁移到C++ Qt，底层性能优化"},
        {"",deepseekName,"第一版翻译"},
        {"","DeepL + GPT-6.1 Sol","第二版翻译"},
        {"","Qwen-3.8-Max","辅助翻译"},
        {"",linuxCredit,"开发Linux版"},
        {"",authorSource,"为AI写明提示词，一行代码没写的人类"}
    };
    auto* credits=new AboutCreditsTable(rows.size(),4);
    credits->setObjectName("AboutCredits");
    credits->setProperty("credit_year_column",1);credits->setProperty("credit_name_column",2);credits->setProperty("credit_work_column",3);
    credits->setItemDelegate(new StudioWidgetCellDelegate(credits));
    credits->horizontalHeader()->hide();
    credits->verticalHeader()->hide();
    credits->verticalHeader()->setMinimumSectionSize(14);
    credits->setEditTriggers(QAbstractItemView::NoEditTriggers);
    credits->setSelectionMode(QAbstractItemView::NoSelection);
    credits->setShowGrid(false);
    credits->setWordWrap(true);
    credits->setIconSize({28,28});
    credits->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);
    credits->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
    credits->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Fixed);
    credits->setColumnWidth(0,132);credits->setColumnWidth(1,72);
    credits->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    applyStudioScrollbars(credits);
    const QHash<QString,QString> vendors{
        {"GPT-6.1 Sol","openai"},{"GPT-6 Astra","openai"},{deepseekName,"deepseek"},
        {"Qwen-3.8-Max","qwen"}
    };
    struct CreditIdentity {QString vendor,name;};
    const QHash<QString,QList<CreditIdentity>> jointCredits{
        {"DeepL + GPT-6.1 Sol",{{"deepl","DeepL"},{"openai","GPT-6.1 Sol"}}},
        {linuxCredit,{{"claude","Claude Opus 5.5"},{"openai","GPT-6.2 Astra"}}}
    };
    QLabel* authorLabel=nullptr;
    for(int row=0;row<rows.size();++row){
        const auto& entry=rows[row];
        const bool spacer=entry.name.isEmpty(),isAuthor=row==rows.size()-1;
        const auto name=isAuthor?uiText(authorSource):entry.name;
        const auto identities=jointCredits.value(name);
        const bool joint=!identities.isEmpty();
        const QString vendor=joint?identities.first().vendor:vendors.value(name);
        const QStringList cells{QString{},entry.year,name,entry.workSource.isEmpty()?QString{}:uiText(entry.workSource)};
        for(int column=0;column<4;++column){
            auto* cell=new QTableWidgetItem(cells[column]);
            auto face=font;face.setPointSizeF(column==1?14:column==2?11:10.5);
            face.setWeight(column==1?QFont::DemiBold:QFont::Normal);cell->setFont(face);
            cell->setForeground(QColor(column==1?"#1e6255":column==2?"#213b36":"#52675f"));
            cell->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
            if(column==2){
                if(!vendor.isEmpty()){
                    cell->setIcon(studioVendorIcon(vendor));
                    cell->setData(Qt::UserRole,vendor);
                }else if(!spacer){cell->setIcon(studioIcon("Person"));cell->setData(Qt::UserRole+1,"human");}
            }
            cell->setBackground(QColor(spacer?"#f7f8f4":row==rows.size()-1?"#e8f3ee":"#ffffff"));
            credits->setItem(row,column,cell);
        }
        credits->setRowHeight(row,spacer?18:38);
        if(!spacer){
            // All identities use the same left inset and icon slot. Joint
            // credits alternate logo/name pairs rather than collecting logos.
            auto* identity=new QWidget;identity->setAccessibleName(name);
            auto* line=new QHBoxLayout(identity);line->setContentsMargins(6,3,6,3);line->setSpacing(7);
            const auto part=[&](const QString& role,const QString& value,const QIcon& icon=QIcon{}){
                auto* label=new QLabel;label->setProperty("credit_part",role);label->setProperty("credit_value",value);
                if(role=="logo"){
                    label->setFixedSize(28,28);label->setProperty("credit_vendor",value);
                    label->setPixmap(icon.pixmap(QSize(28,28),dialog.devicePixelRatioF()));
                }else{
                    label->setText(value);label->setFont(credits->item(row,2)->font());
                    label->setStyleSheet("color:#213b36");label->setWordWrap(role=="name");
                }
                line->addWidget(label,role=="name"&&!joint?1:0,Qt::AlignVCenter);
                return label;
            };
            if(joint){
                for(int i=0;i<identities.size();++i){
                    if(i>0)part("join","+");
                    const auto& credit=identities.at(i);
                    part("logo",credit.vendor,studioVendorIcon(credit.vendor));
                    part("name",credit.name);
                }
            }else{
                part("logo",vendor.isEmpty()?QString("human"):vendor,vendor.isEmpty()?studioIcon("Person"):studioVendorIcon(vendor));
                auto* label=part("name",name);
                if(isAuthor)authorLabel=label;
            }
            if(joint)line->addStretch();
            // QAbstractItemView reserves decoration space even when the
            // delegate suppresses painting. The widget owns the visible logo.
            credits->item(row,2)->setIcon(QIcon{});
            credits->setCellWidget(row,2,identity);
        }
    }
    bindUiRefresh(credits,[credits,rows,authorSource,authorLabel]{
        for(int row=0;row<rows.size();++row)
            if(!rows[row].workSource.isEmpty())credits->item(row,3)->setText(uiText(rows[row].workSource));
        const auto author=uiText(authorSource);
        const int last=credits->rowCount()-1;
        credits->item(last,2)->setText(author);
        authorLabel->setText(author);
        authorLabel->setProperty("credit_value",author);
        credits->cellWidget(last,2)->setAccessibleName(author);
        credits->refreshRowHeights();
    });
    // Era is a merged leading column, alongside the original dated credits.
    const auto eraCell=[&](int row,int count,bool post){
        credits->setSpan(row,0,count,1);
        auto* band=new QWidget;band->setProperty("era_id",post?"post":"pre");
        band->setStyleSheet(post?"background:#1e6255;color:#f5fff9;border-radius:6px":"background:#eaf0eb;color:#49685b;border-radius:6px");
        auto* line=new QVBoxLayout(band);line->setContentsMargins(12,16,12,16);
        auto* title=new QLabel(uiText(post?"后 AI 时代":"前 AI 时代"));title->setProperty("era_title",true);
        auto face=font;face.setPointSizeF(12);face.setWeight(QFont::DemiBold);title->setFont(face);title->setWordWrap(true);title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(post?"color:#f5fff9;background:transparent":"color:#49685b;background:transparent");
        band->setAccessibleName(title->text());line->addStretch();line->addWidget(title);line->addStretch();
        credits->setCellWidget(row,0,band);
    };
    int transition=-1;
    for(int row=0;row<credits->rowCount();++row)if(credits->item(row,1)->text()=="2026"){transition=row;break;}
    if(transition>0){eraCell(0,transition-1,false);eraCell(transition,credits->rowCount()-transition,true);}
    bodyLayout->addWidget(credits,1);
    auto* footer=new QWidget;footer->setStyleSheet("background:#ebf0e9");auto* buttons=new QHBoxLayout(footer);buttons->setContentsMargins(24,16,24,16);footer->setFixedHeight(72);
    auto* version=new QPushButton(studioIcon("Info"),uiText("版本信息"));version->setObjectName("AboutVersionToggle");auto* legal=new QPushButton(studioIcon("Legal"),uiText("许可证信息"));legal->setObjectName("AboutCopyrightToggle");
    for(auto* b:{version,legal}){
        b->setMinimumSize(qMin(220,qMax(1,(available.width()-80)/3)),40);b->setAutoDefault(false);b->setDefault(false);
        b->setStyleSheet("QPushButton{background:#f7f9f5;color:#244d43;border:1px solid #c4d6ce;border-radius:5px;padding:6px 14px}"
            "QPushButton:hover{background:#edf4ef;border-color:#9cbdb0}"
            "QPushButton:pressed{background:#e1eee6;border-color:#79a895}"
            "QPushButton:focus{background:#edf5f0;border-color:#4d8f7d}"
            "QPushButton:disabled{color:#84958c;border-color:#d8e2db}");
    }
    auto* update=new QPushButton(footer);
    bindAboutUpdateButton(parent,update);
    buttons->addWidget(version);buttons->addStretch();buttons->addWidget(update);buttons->addStretch();buttons->addWidget(legal);layout->addWidget(footer);
    const auto info=[&](bool isVersion){
        const auto authorName=uiText(authorSource);
        QDialog child(&dialog);child.setAttribute(Qt::WA_DontShowOnScreen,dialog.testAttribute(Qt::WA_DontShowOnScreen));setStudioWindowIcon(&child,isVersion?"Info":"Legal");child.setObjectName("AboutInformationWindow");
        child.setProperty("information_kind",isVersion?"version":"license");child.setWindowTitle(isVersion?uiText("版本信息"):uiText("许可证信息"));
        auto readFont=font;readFont.setPointSizeF(10.5);child.setFont(readFont);styleStudioReadDialog(&child);
        child.resize(800,isVersion?500:570);child.setMinimumSize(560,340);
        auto* l=new QVBoxLayout(&child);l->setContentsMargins(24,24,24,24);l->setSpacing(20);
        l->addWidget(studioReadHeading(child.windowTitle(),isVersion?"Info":"Legal",&child));
        QList<QStringList> values;
        if(isVersion){QFile f(QCoreApplication::applicationDirPath()+"/build-manifest.json");QJsonObject m;if(f.open(QIODevice::ReadOnly))m=QJsonDocument::fromJson(f.readAll()).object();
            if(m["release_version"].toString().isEmpty()){
                const auto cache=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/FreeShip26/InstallerCache/ad4aa28d-89c6-5dad-8ead-46843a810838/";
                QFile state(cache+"installed.json");
                if(state.open(QIODevice::ReadOnly)){
                    const auto installed=QJsonDocument::fromJson(state.readAll()).object();
                    const auto product=installed["product"].toString();
                    if(QDir(installed["directory"].toString()).canonicalPath()==QDir(QCoreApplication::applicationDirPath()).canonicalPath() && !product.contains('/') && !product.contains('\\')){
                        QFile record(cache+product+".json");if(record.open(QIODevice::ReadOnly))m["release_version"]=QJsonDocument::fromJson(record.readAll()).object()["version"];
                    }
                }
            }
            auto built=m["created_utc"].toString(QStringLiteral(__DATE__ " " __TIME__));const auto time=QDateTime::fromString(built,Qt::ISODateWithMs);if(time.isValid())built=time.toLocalTime().toString("yyyy-MM-dd  HH:mm:ss");
#if defined(Q_OS_WIN)
            const auto targetSystem=QStringLiteral("Windows");
#elif defined(Q_OS_LINUX)
            const auto targetSystem=QStringLiteral("Linux");
#else
            const auto targetSystem=QSysInfo::kernelType();
#endif
            values={{uiText("产品版本"),"FreeShip 26"},{uiText("程序版本"),m["release_version"].toString(QCoreApplication::applicationVersion().isEmpty()?QString(uiText("未标注")):QCoreApplication::applicationVersion())},
                {uiText("构建日期"),built},
                {uiText("编译器版本"),QStringLiteral("C++ " __VERSION__)},{uiText("目标 CPU"),QSysInfo::buildCpuArchitecture()},{uiText("目标操作系统"),targetSystem}};
        }else values={{"Copyright © 2005, by Martijn van Engeland",""},{"Copyright © 2007-2012, by Timoshenko Victor F.","vftim@rambler.ru"},{"Copyright © 2015, by Mark Malakanov","markmal@github.com"},{"Copyright © 2026 "+authorName,"1296811180@qq.com"}};
        auto* table=new AboutInformationTable(values.size(),!isVersion);table->setObjectName("InformationGrid");
        for(int r=0;r<values.size();++r)for(int c=0;c<2;++c){auto* item=new QTableWidgetItem(values[r][c]);item->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
            item->setForeground(QColor(isVersion&&c==0?"#49695c":"#213f37"));item->setToolTip(values[r][c]);table->setItem(r,c,item);}
        l->addWidget(table,1);
        if(!isVersion){
            auto* footer=new QHBoxLayout;auto* license=new QLabel("GPL v3+");license->setStyleSheet("background:#e0eee5;color:#245c48;border-radius:6px;padding:8px 12px;font-weight:500");footer->addWidget(license);footer->addStretch();
            auto* feedback=new StudioMessage(StudioMessageRole::Feedback);feedback->setObjectName("LicenseCopyFeedback");feedback->setMinimumWidth(150);footer->addWidget(feedback);l->addLayout(footer);
            const QStringList years{"2005","2007–2012","2015","2026"};
            const QStringList names{"Martijn van Engeland","Timoshenko Victor F.","Mark Malakanov",authorName};
            for(int row=0;row<values.size();++row){
                auto* identity=new QWidget;auto* content=new QVBoxLayout(identity);content->setContentsMargins(12,8,12,8);content->setSpacing(5);
                auto* notice=new QLabel("Copyright © "+years[row]);notice->setStyleSheet("color:#49695c");content->addWidget(notice);
                auto* person=new QHBoxLayout;person->setSpacing(9);auto* mark=new QLabel;mark->setProperty("person_icon",true);mark->setFixedSize(20,20);mark->setPixmap(studioIcon("Person").pixmap(QSize(20,20),child.devicePixelRatioF()));
                auto* name=new QLabel(names[row]);name->setFont(readFont);name->setProperty("person_name",true);name->setWordWrap(true);name->setMinimumHeight(name->fontMetrics().height()+2);
                name->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);name->setTextInteractionFlags(Qt::TextSelectableByMouse);person->addWidget(mark);person->addWidget(name,1);content->addLayout(person);
                table->setCellWidget(row,0,identity);
                if(values[row][1].isEmpty())continue;
                const auto email=values[row][1];auto* contact=new QWidget;auto* line=new QHBoxLayout(contact);line->setContentsMargins(12,8,12,8);line->setSpacing(10);
                auto* mail=new QLabel(QString("<a style='color:#315f50' href='mailto:%1'>%1</a>").arg(email.toHtmlEscaped()));
                mail->setTextInteractionFlags(Qt::TextBrowserInteraction);mail->setOpenExternalLinks(true);mail->setWordWrap(true);line->addWidget(mail,1);
                auto* copy=new QToolButton;copy->setProperty("copy_email",email);copy->setIcon(studioIcon("Copy"));copy->setIconSize({18,18});copy->setFixedSize(34,34);
                copy->setAccessibleName(uiText("复制邮箱"));copy->setToolTip(uiText("复制邮箱"));line->addWidget(copy);table->setCellWidget(row,1,contact);
                QObject::connect(copy,&QToolButton::clicked,&child,[feedback,email,copy,&child]{
                    QApplication::clipboard()->setText(email);feedback->setMessage(uiText("复制成功"),StudioMessageRole::Feedback);
                    message_motion::confirmation(copy);
                    const int generation=feedback->property("copy_generation").toInt()+1;feedback->setProperty("copy_generation",generation);
                    QTimer::singleShot(1800,&child,[feedback,generation]{if(feedback->property("copy_generation").toInt()==generation){feedback->clear();}});
                });
            }
        }
        runStudioDialog(child);
    };
    QObject::connect(version,&QPushButton::clicked,&dialog,[&]{info(true);});QObject::connect(legal,&QPushButton::clicked,&dialog,[&]{info(false);});runStudioDialog(dialog);
}
