// Actual guide/about dialogs. Clipboard clicks use the offscreen platform's
// private clipboard only; Windows desktop clipboard is never accessed by this probe.
#include "main_window.h"
#include "about_dialog.h"
#include "shortcuts_dialog.h"
#include "tutorial_dialog.h"
#include <QEventLoop>
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>

namespace {
QJsonObject observeReadCellFocus(QTableWidget* table,const QString& directory,const QString& stem){
    QJsonArray columns;QStyledItemDelegate standard;
    for(int column=0;column<table->columnCount();++column){
        const auto index=table->model()->index(0,column);
        QStyleOptionViewItem option;option.initFrom(table);option.widget=table;option.font=table->font();option.palette=table->palette();
        option.rect=QRect(0,0,qMax(180,table->columnWidth(column)),qMax(44,table->rowHeight(0)));
        option.state=QStyle::State_Enabled|QStyle::State_Active|QStyle::State_KeyboardFocusChange;
        const auto draw=[&](QAbstractItemDelegate* delegate,bool focused){
            auto state=option;if(focused)state.state|=QStyle::State_HasFocus;
            QImage result(option.rect.size(),QImage::Format_ARGB32_Premultiplied);result.fill(Qt::white);
            QPainter painter(&result);delegate->paint(&painter,state,index);return result;
        };
        auto* delegate=table->itemDelegateForIndex(index);
        const auto plain=draw(delegate,false),focused=draw(delegate,true);
        focused.save(directory+"/"+stem+"-focused-"+QString::number(column)+".png");
        const auto standardPlain=draw(&standard,false),standardFocused=draw(&standard,true);
        standardFocused.save(directory+"/"+stem+"-standard-focus-"+QString::number(column)+".png");
        columns.append(QJsonObject{{"column",column},{"focus_changes_cell_pixels",plain!=focused},{"standard_focus_changes_pixels",standardPlain!=standardFocused}});
    }
    const auto current=table->currentIndex();table->setCurrentCell(0,0);
    QKeyEvent down(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QCoreApplication::sendEvent(table,&down);
    const bool navigates=table->currentRow()!=0||table->currentColumn()!=0;table->setCurrentIndex(current);
    return {{"columns",columns},{"keyboard_navigation_retained",navigates}};
}
}

void MainWindow::auditReadSurfaces(bool copyOnly){
    if(!startupProbe_){finishWorkflow("Read-surface observer requires an isolated startup run");return;}
    qApp->setQuitOnLastWindowClosed(false);const auto oldLanguage=uiLanguage();const bool hidden=testAttribute(Qt::WA_DontShowOnScreen);
    setAttribute(Qt::WA_DontShowOnScreen);QDir().mkpath(workflowOutput_+"/views");QJsonArray surfaces,copies;
    const auto capture=[&](QDialog* dialog,const QString& kind,const QString& language){
        for(bool compact:{false,true}){
            if(compact)dialog->resize(kind=="about"?QSize(840,620):kind=="tutorial"?QSize(660,500):QSize(620,380));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            QJsonObject row{{"kind",kind},{"language",language},{"compact",compact},{"width",dialog->width()},{"height",dialog->height()}};
            const auto path=workflowOutput_+"/views/"+kind+"-"+language+(compact?"-compact":"")+".png";dialog->grab().save(path);row["image"]=path;
            if(auto* table=dialog->findChild<QTableWidget*>())
                row["cell_focus"]=observeReadCellFocus(table,workflowOutput_+"/views",kind+"-"+language+(compact?"-compact":""));
            if(auto* table=dialog->findChild<QTableWidget*>("ShortcutGuideTable")){
                row["keycap_delegate"]=dynamic_cast<StudioKeycapDelegate*>(table->itemDelegateForColumn(1))!=nullptr;QJsonArray wraps;bool inside=true;
                for(int at=0;at<table->rowCount();++at){
                    const auto* item=table->item(at,1);
                    if(!item)continue; // Group headers span the table and have no key cell.
                    const auto key=item->text();auto parts=studioKeyParts(key,table->font(),table->columnWidth(1));QJsonArray tokens;
                    for(const auto& part:parts){inside=inside&&QRectF(0,0,table->columnWidth(1),table->rowHeight(at)).contains(part.rect);if(part.key)tokens.append(part.text);}
                    wraps.append(QJsonObject{{"source",key},{"tokens",tokens},{"row_height",table->rowHeight(at)}});
                }row["keycaps_fit_cells"]=inside;row["keys"]=wraps;
            }
            if(auto* table=dialog->findChild<QTableWidget*>("AboutCredits")){
                const int yearColumn=table->property("credit_year_column").toInt(),nameColumn=table->property("credit_name_column").toInt();
                row["era_column"]=0;row["year_column"]=yearColumn;row["column_count"]=table->columnCount();
                int humans=0,icons=0;for(int at=0;at<table->rowCount();++at)if(auto* cell=table->item(at,nameColumn);cell&&cell->data(Qt::UserRole+1)=="human"){
                    ++humans;bool hasIcon=!cell->icon().isNull();
                    if(auto* identity=table->cellWidget(at,nameColumn))for(auto* mark:identity->findChildren<QLabel*>())
                        if(mark->property("credit_part")=="logo"&&mark->property("credit_value")=="human")hasIcon|=!mark->pixmap().isNull();
                    icons+=hasIcon;
                }row["humans"]=humans;row["human_icons"]=icons;row["scroll_max"]=table->verticalScrollBar()->maximum();
                QJsonArray contributions;for(int at=0;at<table->rowCount();++at){
                    const auto* name=table->item(at,nameColumn);const auto* work=table->item(at,table->property("credit_work_column").toInt());if(!name||name->text().isEmpty()||!work)continue;
                    QJsonArray logos,parts;int iconLeft=-1;
                    if(auto* identity=table->cellWidget(at,nameColumn))for(auto* mark:identity->findChildren<QLabel*>()){
                        if(mark->property("credit_vendor").isValid())logos.append(mark->property("credit_vendor").toString());
                        if(mark->property("credit_part").isValid()){
                            const QPoint origin=mark->mapTo(identity,QPoint{});
                            parts.append(QJsonObject{{"role",mark->property("credit_part").toString()},{"value",mark->property("credit_value").toString()},{"x",origin.x()},{"right",origin.x()+mark->width()},{"width",mark->width()}});
                            if(iconLeft<0&&mark->property("credit_part")=="logo")iconLeft=mark->mapTo(table->viewport(),QPoint{}).x()-table->columnViewportPosition(nameColumn);
                        }
                    }
                    contributions.append(QJsonObject{{"name",name->text()},{"work",work->text()},{"joint_vendor_logos",logos},{"identity_parts",parts},{"first_icon_left",iconLeft}});
                }row["contributions"]=contributions;
                QJsonArray eras;QJsonObject era;QJsonArray years;int members=0;
                const auto flushEra=[&]{if(!era.isEmpty()){era["years"]=years;era["members"]=members;eras.append(era);}};
                for(int at=0;at<table->rowCount();++at){
                    if(auto* band=table->cellWidget(at,0);band&&band->property("era_id").isValid()){
                        flushEra();era=QJsonObject{{"id",band->property("era_id").toString()},{"row",at},{"span",table->columnSpan(at,0)},{"row_span",table->rowSpan(at,0)}};years={};members=0;
                        for(auto* label:band->findChildren<QLabel*>())if(label->property("era_title").toBool())era["title"]=label->text();
                    }
                    if(auto* year=table->item(at,yearColumn);year&&!year->text().isEmpty())years.append(year->text());
                    if(auto* member=table->item(at,nameColumn);member&&!member->text().isEmpty())++members;
                }flushEra();row["eras"]=eras;
            }
            if(auto* table=dialog->findChild<QTableWidget*>("InformationGrid")){
                row["label_column"]=table->columnWidth(0);row["table_width"]=table->viewport()->width();row["scroll_max"]=table->verticalScrollBar()->maximum();
                int people=0;bool namesFit=true;for(auto* label:table->findChildren<QLabel*>()){
                    if(label->property("person_icon").toBool())++people;
                    if(label->property("person_name").toBool())namesFit=namesFit&&label->height()>=label->fontMetrics().boundingRect(QRect(0,0,label->contentsRect().width(),1000),Qt::TextWordWrap,label->text()).height();
                }
                row["human_icons"]=people;row["person_names_not_clipped"]=namesFit;
                const auto before=table->viewport()->grab().toImage();QList<QPair<QTableWidgetItem*,QString>> underlying;
                for(int r=0;r<table->rowCount();++r)for(int c=0;c<table->columnCount();++c)if(table->cellWidget(r,c)){
                    auto* item=table->item(r,c);if(item){underlying.append({item,item->text()});item->setText({});}
                }
                const auto without=table->viewport()->grab().toImage();row["no_underlying_text_painted"]=before==without;
                for(const auto& entry:underlying)entry.first->setText(entry.second);
            }
            if(kind=="tutorial"){
                const auto* tabs=dialog->findChild<QTabBar*>("TutorialDemoTabs");
                const auto* example=dialog->findChild<QWidget*>("TutorialActionDemo");
                const auto* scroll=dialog->findChild<QScrollArea*>();
                row["example_tabs"]=tabs?tabs->count():0;
                row["example_stage_reachable"]=scroll&&example&&example->isVisible()&&
                    scroll->viewport()->rect().contains(QRect(example->mapTo(scroll->viewport(),QPoint{}),example->size()));
                for(auto* scroll:dialog->findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(0);
            }
            surfaces.append(row);
        }
    };
    for(const auto& language:QStringList{"zh","en"}){
        setUiLanguage(language);
        if(!copyOnly){
            QTimer::singleShot(0,this,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog){capture(dialog,"tutorial",language);dialog->reject();}});showTutorialGuide(this);
            QTimer::singleShot(0,this,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog){capture(dialog,"shortcuts",language);dialog->reject();}});showShortcutGuide(this);
        }
        QTimer::singleShot(0,this,[&]{
            auto* about=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!about)return;
            if(!copyOnly)capture(about,"about",language);
            for(bool version:{true,false}){
                if(copyOnly&&version)continue;
                auto* button=about->findChild<QPushButton*>(version?"AboutVersionToggle":"AboutCopyrightToggle");if(!button)continue;
                QTimer::singleShot(0,this,[&,version]{
                    auto* child=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!child)return;
                    if(!copyOnly)capture(child,version?"version":"license",language);
                    if(!version){
                        for(auto* copy:child->findChildren<QToolButton*>())if(copy->property("copy_email").isValid()){
                            QJsonObject row{{"language",language},{"email",copy->property("copy_email").toString()},{"icon_present",!copy->icon().isNull()},
                                {"tooltip",copy->toolTip()},{"button_inside",child->rect().contains(QRect(copy->mapTo(child,QPoint()),copy->size()))}};
                            if(QGuiApplication::platformName()=="offscreen"){
                                const bool keyboard=copy->property("copy_email")=="1296811180@qq.com";
                                row["keyboard_focusable"]=copy->focusPolicy()!=Qt::NoFocus;row["activation"]=keyboard?"keyboard":"click";
                                if(keyboard){
                                    QKeyEvent press(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier),release(QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier);
                                    QCoreApplication::sendEvent(copy,&press);QCoreApplication::sendEvent(copy,&release);
                                }else copy->click();
                                row["copied"]=QApplication::clipboard()->text();
                                auto* feedback=child->findChild<QLabel*>("LicenseCopyFeedback");row["feedback"]=feedback?feedback->text():QString{};row["feedback_role"]=feedback?feedback->property("studio_message_role").toString():QString{};
                            }else row["clipboard_skipped"]=true;
                            copies.append(row);
                        }
                        if(QGuiApplication::platformName()=="offscreen"){
                            QEventLoop timer;QTimer::singleShot(1950,&timer,&QEventLoop::quit);timer.exec();
                            auto* feedback=child->findChild<QLabel*>("LicenseCopyFeedback");workflowCase("copy-feedback-expires-"+language,feedback&&feedback->text().isEmpty());
                        }
                    }child->reject();
                });button->click();
            }about->reject();
        });showFreeShipAbout(this);
    }
    workflowArtifacts_["read_surfaces"]=surfaces;workflowArtifacts_["email_copy"]=copies;
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setUiLanguage(oldLanguage);
}
