// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production dialogs, rendered without desktop input or user settings.
#include "app/project_dialog.h"
#include "app/preferences_dialog.h"
#include "app/section_dialog.h"
#include "app/studio_ui.h"
#include "app/studio_menu_icons.h"
#include <QPainter>
#include <QPlainTextEdit>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <stdexcept>
#include "core/water_viscosity.h"
#include <limits>

static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(); QApplication::processEvents(); }
static QRect bounds(QWidget* w, QWidget* in) { return {w->mapTo(in, QPoint()), w->size()}; }
static int pageOf(QTabWidget* tabs, QWidget* field) {
    for (int i=0;i<tabs->count();++i) if (tabs->widget(i)->isAncestorOf(field)) return i;
    return -1;
}

int main(int argc,char** argv) {
    QApplication app(argc,argv); if(argc!=2) return 3;
    applyStudioTheme(); const QString out=app.arguments()[1]; QDir().mkpath(out);
    QJsonArray cases,images; bool failed=false; QJsonObject report;
    auto check=[&](const QString& id,bool ok,QJsonObject detail=QJsonObject{}) {
        detail["id"]=id;detail["status"]=ok?"pass":"fail";cases.append(detail);failed|=!ok;
    };
    auto capture=[&](QWidget& w,const QString& name) {
        const QString path=out+"/"+name+".png";check(name+".render",w.grab().save(path));images.append(path);
    };
    try {
        // User requirement 2026-11-12: wider, consistent scrollbars, including
        // scroll areas that do not explicitly call applyStudioScrollbars.
        QScrollArea scrollbarSample;scrollbarSample.setAttribute(Qt::WA_DontShowOnScreen);
        auto* large=new QWidget;large->setFixedSize(1000,1000);scrollbarSample.setWidget(large);
        scrollbarSample.resize(240,180);scrollbarSample.show();settle();
        for(auto* bar:{scrollbarSample.horizontalScrollBar(),scrollbarSample.verticalScrollBar()})
            check("scrollbars.global-polish-20-dip",bar->property("studio_scrollbar").toBool()&&
                (bar->orientation()==Qt::Horizontal?bar->height():bar->width())==20);
        capture(scrollbarSample,"scrollbar-style");
        const auto iconKeys=studioSupplementIconKeys();QImage sheet(560,iconKeys.size()*48+28,QImage::Format_ARGB32_Premultiplied);sheet.fill(QColor("#f5f8f5"));QPainter icons(&sheet);
        icons.setPen(QColor("#315f56"));icons.drawText(10,18,"StudioLight menu vectors — normal / disabled, 16 / 20 / 24 / 32 DIP");
        int row=0;for(const auto& key:iconKeys){
            const auto icon=studioIcon(key);icons.drawText(10,60+row*48,key);int column=0;
            for(auto mode:{QIcon::Normal,QIcon::Disabled})for(int side:{16,20,24,32}){
                const auto pixmap=icon.pixmap(QSize(side,side),mode);auto image=pixmap.toImage();int ink=0;QRect bounds;
                for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).alpha()>0){++ink;bounds=bounds.united(QRect(x,y,1,1));}
                check("icon."+key+"."+QString::number(int(mode))+"."+QString::number(side),!icon.isNull()&&ink>side&&image.rect().contains(bounds),{{"ink_pixels",ink}});
                icons.drawPixmap(200+column++*42,34+row*48,pixmap);
            }++row;
        }icons.end();const auto iconPath=out+"/menu-vector-icons.png";check("icon-sheet",sheet.save(iconPath));images.append(iconPath);
        check("water-table.endpoints",fs::waterViscosity(0)==1.82f && fs::waterViscosity(30)==.817f);
        check("water-table.default",fs::waterViscosity(15)==1.189f);
        check("water-table.interpolation",std::abs(fs::waterViscosity(16.1f)-1.157f)<1e-6f);
        for(float invalid:{-1.f,30.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            bool rejected=false;try{fs::waterViscosity(invalid);}catch(const std::invalid_argument&){rejected=true;}
            check("water-table.domain",rejected);
        }
        fs::ModelSnapshot m;m.units="metric";m.name="9a — 工程设置";m.designer="FreeShip_26";
        m.createdBy="Original author";
        {
            ProjectDialog dialog(m);
            auto* author=qobject_cast<QLineEdit*>(dialog.fieldEditor("created_by"));
            if(!author)throw std::runtime_error("Created-by editor absent");
            check("project.author-original",author->text()==m.createdBy && dialog.changes().isEmpty());
            author->setText("New author");check("project.author-patch",dialog.changes()==QJsonObject{{"created_by","New author"}});
            author->setText(QString(65537,'a'));check("project.author-length",!dialog.validationError().isEmpty());
            author->setText(QString("A")+QChar(0));check("project.author-null",!dialog.validationError().isEmpty());
        }
        m.comment="保留原有工程信息。\n尺寸、精度与单位修改仍由文档服务统一提交。";
        m.length=181.256;m.beam=28;m.draft=10.85;m.waterDensity=1.025;m.appendageCoefficient=1.0005;
        m.useDefaultSplitSectionLocation=true;m.shadeUnderwater=true;m.underwaterColor=0x800000;
        m.underwaterColorAlpha=234;
        m.sections={{1,0,2,false,0,{1,0,0,-2},{}},{2,0,3,false,0,{0,1,0,-3},{}}};
        for (int pointSize:{9,14,18}) {
            const QString size=QString::number(pointSize);QFont font=app.font();font.setPointSize(pointSize);
            ProjectDialog project(m);PreferencesDialog preferences({});SectionDialog sections;
            for (auto* dialog:QList<QDialog*>{&project,&preferences,&sections}) {
                dialog->setAttribute(Qt::WA_DontShowOnScreen);dialog->setFont(font);dialog->show();
            }
            sections.setSnapshot(std::make_shared<fs::ModelSnapshot>(m));sections.setEditingEnabled(true);settle();
            auto* tabs=project.findChild<QTabWidget*>();auto* buttons=project.findChild<QDialogButtonBox*>();
            if(!tabs||!buttons)throw std::runtime_error("Project actions/tabs absent");
            // User changed this explicit layout policy on 2026-11-12.
            check("project."+size+".footer-actions",bounds(buttons,&project).top()>bounds(tabs,&project).bottom());
            check("project."+size+".original-action-icons",!buttons->button(QDialogButtonBox::Ok)->icon().isNull()&&!buttons->button(QDialogButtonBox::Cancel)->icon().isNull());
            for(auto* button:buttons->buttons())
                check("project."+size+".action-text-fits."+button->text(),button->contentsRect().width()>=button->fontMetrics().horizontalAdvance(button->text())+button->iconSize().width() && button->contentsRect().height()>=button->fontMetrics().height());
            // The LFM groups units/precision with General, split with Dimensions,
            // and water properties with Hydrostatics. Independent layout authority.
            for (auto pair:{std::pair{"units",0},{"precision",0},{"shade_underwater",0},{"length",1},{"split_section_location",1},{"water_density",2},{"appendage_coefficient",2}})
                check("project."+size+".group."+pair.first,pageOf(tabs,project.fieldEditor(pair.first))==pair.second);
            for (auto pair:{std::pair<QDialog*,QString>{&project,"project"},{&preferences,"preferences"}}) {
                auto* dialog=pair.first;auto* pages=dialog->findChild<QTabWidget*>();
                for (bool compact:{false,true}) {
                    const QSize requested=compact?QSize(600,420):(pair.second=="project"?QSize(800,600):QSize(638,687));
                    dialog->resize(requested);settle();
                    const QString prefix=pair.second+"."+size+(compact?".compact":".normal");
                    check(prefix+".fits-requested-window",dialog->size()==requested,{{"width",dialog->width()},{"height",dialog->height()}});
                    for(int i=0;i<pages->count();++i) {
                        pages->setCurrentIndex(i);settle();auto* page=pages->widget(i);
                        // Every field must be fully reachable, including the last
                        // row at a large font / short window. Not just instantiated.
                        QStringList inaccessible;
                        for(auto* field:page->findChildren<QWidget*>()) {
                            QString key=field->property(pair.second=="project"?"modelField":"preference_field").toString();
                            if(key.isEmpty())continue;
                            auto* scroll=qobject_cast<QScrollArea*>(page);
                            if(scroll){scroll->ensureWidgetVisible(field,8,8);settle();}
                            QWidget* viewport=scroll?scroll->viewport():page;
                            if(!viewport->rect().contains(bounds(field,viewport))||field->height()<field->minimumSizeHint().height())inaccessible<<key;
                        }
                        for(auto* label:page->findChildren<QLabel*>())if(!label->wordWrap()&&label->isVisible()&&label->width()<label->fontMetrics().horizontalAdvance(label->text()))inaccessible<<label->text();
                        check(prefix+".page"+QString::number(i)+".fields-reachable",inaccessible.isEmpty(),{{"unreachable",QJsonArray::fromStringList(inaccessible)}});
                        if(auto* scroll=qobject_cast<QScrollArea*>(page))scroll->verticalScrollBar()->setValue(0);
                        settle();capture(*dialog,prefix+".page"+QString::number(i));
                    }
                }
            }
            check("project."+size+".layout-does-not-edit-model",project.changes().isEmpty());
            project.setEditingEnabled(false);settle();
            check("project."+size+".waiting-guards-submit",!tabs->isEnabled()&&!buttons->button(QDialogButtonBox::Ok)->isEnabled()&&buttons->button(QDialogButtonBox::Cancel)->isEnabled());
            project.accept();check("project."+size+".waiting-cannot-accept",project.result()!=QDialog::Accepted&&project.isVisible());
            auto latest=m;latest.name="Latest revision";latest.comment="Fresh result";latest.draft=11.125;
            project.refreshSnapshot(latest);project.setEditingEnabled(true);settle();
            check("project."+size+".refresh-latest-snapshot",qobject_cast<QLineEdit*>(project.fieldEditor("name"))->text()==latest.name&&
                qobject_cast<QPlainTextEdit*>(project.fieldEditor("comment"))->toPlainText()==latest.comment&&
                qobject_cast<QDoubleSpinBox*>(project.fieldEditor("draft"))->value()==latest.draft&&tabs->isEnabled()&&project.changes().isEmpty());
            auto imperial=latest;imperial.units="imperial";imperial.length=600;imperial.draft=35;imperial.waterDensity=64;
            project.refreshSnapshot(imperial);check("project."+size+".refresh-imperial-no-unwanted-patch",project.changes().isEmpty()&&qobject_cast<QDoubleSpinBox*>(project.fieldEditor("draft"))->value()==35);
            project.refreshSnapshot(m);check("project."+size+".refresh-original-no-unwanted-patch",project.changes().isEmpty());
            auto* opacity=qobject_cast<QSpinBox*>(project.fieldEditor("underwater_opacity_percent"));
            if(!opacity)throw std::runtime_error("Underwater opacity editor unavailable");
            check("project."+size+".opacity-display",opacity->value()==91);
            opacity->setValue(50);check("project."+size+".opacity-conversion",project.changes()==QJsonObject{{"underwater_color_alpha",127}});
            opacity->setValue(91);check("project."+size+".opacity-restored-byte-no-op",project.changes().isEmpty());
            auto* shade=qobject_cast<QCheckBox*>(project.fieldEditor("shade_underwater"));shade->setChecked(false);
            check("project."+size+".opacity-disabled-retains-value",!opacity->isEnabled()&&opacity->value()==91&&project.changes()==QJsonObject{{"shade_underwater",false}});
            shade->setChecked(true);
            auto* preview=qobject_cast<QCheckBox*>(project.fieldEditor("save_preview"));
            if(!preview)throw std::runtime_error("Save preview editor unavailable");
            preview->setChecked(!m.savePreview);
            check("project."+size+".preview-toggle-command",project.changes()==QJsonObject{{"save_preview",!m.savePreview}});
            preview->setChecked(m.savePreview);
            check("project."+size+".preview-restored-no-op",project.changes().isEmpty());
            QJsonObject hydroPatch{{"coefficient_basis",0}};
            auto* basis=qobject_cast<QComboBox*>(project.fieldEditor("coefficient_basis"));
            if(!basis)throw std::runtime_error("Coefficient basis editor unavailable");
            basis->setCurrentIndex(0);
            for(auto* key:{"hydro_displacement","hydro_lateral_area","hydro_sectional_areas","hydro_metacentric_height","hydro_lcf"}){
                auto* box=qobject_cast<QCheckBox*>(project.fieldEditor(key));
                if(!box)throw std::runtime_error("Hydrostatic feature editor unavailable");
                box->setChecked(false);hydroPatch[key]=false;
            }
            check("project."+size+".hydro-options-single-patch",project.changes()==hydroPatch);
            basis->setCurrentIndex(1);
            for(auto* key:{"hydro_displacement","hydro_lateral_area","hydro_sectional_areas","hydro_metacentric_height","hydro_lcf"})qobject_cast<QCheckBox*>(project.fieldEditor(key))->setChecked(true);
            check("project."+size+".hydro-options-restored-no-op",project.changes().isEmpty());
            auto* auxiliary=qobject_cast<QCheckBox*>(project.fieldEditor("generate_auxiliary_reports"));
            if(auxiliary){
                auxiliary->setChecked(true);check("project."+size+".auxiliary-command",project.changes()==QJsonObject{{"generate_auxiliary_reports",true}});
                auxiliary->setChecked(false);check("project."+size+".auxiliary-restored-no-op",project.changes().isEmpty());
            }else cases.append(QJsonObject{{"id","project."+size+".auxiliary-command"},{"status","not_run"},{"reason","Legacy generate_auxiliary_reports editor is absent in the current project dialog; no callback exercised."}});
            auto* kinds=sections.findChild<QTabWidget*>();auto* table=sections.findChild<QTableWidget*>();
            if(!kinds||!table)throw std::runtime_error("Section selector/table absent");
            auto* tabBar=kinds->tabBar();bool tabsFit=true;
            for(int i=0;i<kinds->count();++i)tabsFit &= kinds->rect().contains(bounds(tabBar,kinds))&&tabBar->tabRect(i).height()>=tabBar->fontMetrics().height()+8;
            check("sections."+size+".tabs-not-clipped",tabsFit);
            const int headerMargin=table->horizontalHeader()->style()->pixelMetric(QStyle::PM_HeaderMargin);
            check("sections."+size+".curvature-header-readable",table->columnWidth(2)>=table->horizontalHeader()->fontMetrics().horizontalAdvance(table->horizontalHeaderItem(2)->text())+2*headerMargin);
            kinds->setCurrentIndex(1);settle();check("sections."+size+".switch-keeps-category",sections.kind()==2&&table->rowCount()==1&&table->item(0,1)->text()=="3");
            int edits=0;sections.onEdit=[&](const QString& op,int kind,QJsonObject update){if(op=="update"&&kind==2&&update["distance"].toDouble()==4.25)++edits;return true;};
            table->item(0,1)->setText("4.25");check("sections."+size+".edit-keeps-dispatch",edits==1);
            sections.discardPending();
            sections.onEdit=[](const QString&,int,QJsonObject){return false;};
            table->item(0,1)->setText("5.25");
            check("sections."+size+".rejected-edit-restores-confirmed-list",table->item(0,1)->text()=="3"&&sections.property("section_pending_count").toInt()==0);
            sections.onEdit=[&](const QString&,int,QJsonObject){sections.discardPending();return false;};
            table->item(0,1)->setText("6.25");
            check("sections."+size+".synchronous-failure-clears-pending",table->item(0,1)->text()=="3"&&sections.property("section_pending_count").toInt()==0);
            capture(sections,"sections."+size);sections.setEditingEnabled(false);
            check("sections."+size+".busy-keeps-controls-disabled",!table->isEnabled()&&!kinds->isEnabled());
            project.reject();check("project."+size+".cancel-keeps-document",project.result()==QDialog::Rejected&&m.length==181.256);
        }
        cases.append(QJsonObject{{"id","physical-desktop-dpi-focus-and-dialog-acceptance"},{"status","not_run"}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;report["images"]=images;report["device_pixel_ratio"]=app.devicePixelRatio();
    report["scope"]="Production widgets, StudioLight icons, font/window matrix and saved renders; no desktop automation.";
    QFile file(out+"/report.json");if(!file.open(QIODevice::WriteOnly))return 3;file.write(QJsonDocument(report).toJson());
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
