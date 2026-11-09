// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production dialogs, rendered without desktop input or user settings.
#include "app/project_dialog.h"
#include "app/preferences_dialog.h"
#include "app/section_dialog.h"
#include "app/studio_ui.h"
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
            check("project."+size+".original-action-position",bounds(buttons,&project).bottom()<bounds(tabs,&project).top());
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
            if(!auxiliary)throw std::runtime_error("Auxiliary reports editor unavailable");
            auxiliary->setChecked(true);check("project."+size+".auxiliary-command",project.changes()==QJsonObject{{"generate_auxiliary_reports",true}});
            auxiliary->setChecked(false);check("project."+size+".auxiliary-restored-no-op",project.changes().isEmpty());
            auto* kinds=sections.findChild<QTabWidget*>();auto* table=sections.findChild<QTableWidget*>();
            if(!kinds||!table)throw std::runtime_error("Section selector/table absent");
            auto* tabBar=kinds->tabBar();bool tabsFit=true;
            for(int i=0;i<kinds->count();++i)tabsFit &= kinds->rect().contains(bounds(tabBar,kinds))&&tabBar->tabRect(i).height()>=tabBar->fontMetrics().height()+8;
            check("sections."+size+".tabs-not-clipped",tabsFit);
            const int headerMargin=table->horizontalHeader()->style()->pixelMetric(QStyle::PM_HeaderMargin);
            check("sections."+size+".curvature-header-readable",table->columnWidth(2)>=table->horizontalHeader()->fontMetrics().horizontalAdvance(table->horizontalHeaderItem(2)->text())+2*headerMargin);
            kinds->setCurrentIndex(1);settle();check("sections."+size+".switch-keeps-category",sections.kind()==2&&table->rowCount()==1&&table->item(0,1)->text()=="3");
            int edits=0;sections.onEdit=[&](const QString& op,int kind,QJsonObject update){if(op=="update"&&kind==2&&update["distance"].toDouble()==4.25)++edits;};
            table->item(0,1)->setText("4.25");check("sections."+size+".edit-keeps-dispatch",edits==1);
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
