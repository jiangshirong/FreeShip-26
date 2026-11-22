// Actual guide/about dialogs. Clipboard clicks use the offscreen platform's
// private clipboard only; Windows desktop clipboard is never accessed by this probe.
#include "studio_ui.h"
#include "main_window.h"
#include "about_dialog.h"
#include "shortcuts_dialog.h"
#include "tutorial_dialog.h"
#include <QToolButton>
#include <QMouseEvent>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextDocument>

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

void MainWindow::auditTutorialLanguage(const QJsonArray& settings){
    if(!startupProbe_){finishWorkflow("Tutorial language check requires an isolated startup run");return;}
    const auto oldLanguage=uiLanguage();
    const bool hidden=testAttribute(Qt::WA_DontShowOnScreen);
    setAttribute(Qt::WA_DontShowOnScreen);
    QDir().mkpath(workflowOutput_+"/views");
    const auto settle=[]{
        QApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    const auto shortcut=[&](QDialog* dialog){
        QApplication::setActiveWindow(dialog);
        auto* focus=dialog->focusWidget()?dialog->focusWidget():dialog;
        QKeyEvent press(QEvent::KeyPress,Qt::Key_L,Qt::ControlModifier|Qt::ShiftModifier,"l");
        QApplication::sendEvent(focus,&press);
        QKeyEvent release(QEvent::KeyRelease,Qt::Key_L,Qt::NoModifier);
        QApplication::sendEvent(focus,&release);
        settle();
    };
    const auto tabs=[](const QTabBar* bar){
        QJsonArray values;
        for(int i=0;i<bar->count();++i)
            values.append(QJsonArray{bar->tabText(i),bar->tabToolTip(i),bar->accessibleTabName(i)});
        return values;
    };
    const auto snapshot=[&](QDialog* dialog,int section,int index,bool select){
        auto* sections=dialog->findChild<QTabWidget*>("TutorialSections");
        auto* menu=dialog->findChild<QTabBar*>("TutorialDemoTabs");
        auto* topics=dialog->findChild<QTabBar*>("TutorialFeatureSubjects");
        auto* steps=dialog->findChild<QWidget*>("TutorialFeatureSteps");
        auto* frame=dialog->findChild<QWidget*>("TutorialFeatureFrame");
        auto* play=dialog->findChild<QToolButton*>("TutorialFeaturePlay");
        auto* replay=dialog->findChild<QToolButton*>("TutorialFeatureReplay");
        auto* menuReplay=dialog->findChild<QPushButton*>("TutorialMenuReplay");
        if(!sections||!menu||!topics||!steps||!frame||!play||!replay||!menuReplay)
            return QJsonObject{};
        auto* bar=section==0?menu:topics;
        if(index>=bar->count())return QJsonObject{};
        if(select){sections->setCurrentIndex(section);bar->setCurrentIndex(index);}
        settle();
        QJsonObject value{{"title",dialog->windowTitle()},{"sections",tabs(sections->tabBar())},
            {"menu_tabs",tabs(menu)},{"feature_tabs",tabs(topics)},
            {"section",sections->currentIndex()},{"index",bar->currentIndex()}};
        if(section==0){
            value["replay"]=menuReplay->text();
            for(auto* label:dialog->findChildren<QLabel*>("TutorialDemoExplanation")){
                if(!label->isVisible())continue;
                QTextDocument text;
                if(label->textFormat()==Qt::RichText)text.setHtml(label->text());
                else text.setPlainText(label->text());
                value["explanation"]=text.toPlainText();
                break;
            }
            if(value["explanation"].toString().isEmpty())return QJsonObject{};
        }else{
            const auto nodes=steps->findChildren<QToolButton*>("TutorialFeatureNode");
            if(nodes.size()<2)return QJsonObject{};
            if(select)nodes[1]->click();
            QJsonArray tips;
            for(const auto* node:nodes)tips.append(QJsonArray{node->toolTip(),node->accessibleName()});
            value["steps"]=tips;
            value["timeline"]=steps->accessibleName();
            value["introduction"]=frame->property("introduction").toString();
            value["frame_title"]=frame->accessibleName();
            value["step"]=frame->property("tutorialStep").toInt();
            value["frame"]=frame->property("frameNumber").toInt();
            value["playback"]=QJsonArray{play->toolTip(),play->accessibleName(),replay->toolTip(),replay->accessibleName()};
        }
        return value;
    };
    for(const auto& item:settings){
        const auto pair=item.toObject();
        const auto from=pair["from"].toString(),to=pair["to"].toString();
        loadPreferences(pair["file"].toString());
        if(uiLanguage()!=from||!setUiLanguage(to).isEmpty()){
            finishWorkflow("Cannot configure tutorial language shortcut");return;
        }
        auto* reference=showTutorialGuide(this);
        QJsonArray expected;
        for(int section=0;section<2;++section)
            for(int index=0;index<6;++index)expected.append(snapshot(reference,section,index,true));
        reference->reject();
        if(!setUiLanguage(from).isEmpty()){finishWorkflow("Cannot restore tutorial language");return;}
        auto* dialog=showTutorialGuide(this);
        QJsonArray observations;
        for(int section=0;section<2;++section)for(int index=0;index<6;++index){
            const auto id=from+"-"+to+(section==0?".menu-":".feature-")+QString::number(index+1);
            const auto before=snapshot(dialog,section,index,true);
            shortcut(dialog);
            const auto switched=snapshot(dialog,section,index,false);
            workflowCase(id+".switch",uiLanguage()==to&&dialog->isVisible()&&!before.isEmpty()&&
                         switched!=before&&switched==expected[section*6+index].toObject()&&
                         (section==0||before["step"].toInt()==1));
            if(to=="ru"&&((from=="zh"&&section==0&&index==0)||(from=="ja"&&section==1&&index==1)))
                dialog->grab().save(workflowOutput_+"/views/tutorial-"+id+".png");
            shortcut(dialog);
            const auto restored=snapshot(dialog,section,index,false);
            workflowCase(id+".restore",uiLanguage()==from&&!before.isEmpty()&&restored==before);
            observations.append(QJsonObject{{"id",id},{"before",before},{"switched",switched},
                                           {"expected",expected[section*6+index]},{"restored",restored}});
        }
        dialog->reject();
        workflowArtifacts_["tutorial_"+from+"_"+to]=observations;
    }
    setAttribute(Qt::WA_DontShowOnScreen,hidden);
    setUiLanguage(oldLanguage);
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
                auto* sections=dialog->findChild<QTabWidget*>("TutorialSections");
                auto* topics=dialog->findChild<QTabBar*>("TutorialFeatureSubjects");
                auto* steps=dialog->findChild<QWidget*>("TutorialFeatureSteps");
                auto* frame=dialog->findChild<QWidget*>("TutorialFeatureFrame");
                auto* play=dialog->findChild<QToolButton*>("TutorialFeaturePlay");
                auto* replay=dialog->findChild<QToolButton*>("TutorialFeatureReplay");
                QJsonArray chapters;
                if(sections&&topics&&steps&&frame&&play&&replay){
                    sections->setCurrentIndex(1);
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    frame->grab();
                    const auto picture=frame->property("pictureRect").toRect();
                    row["tutorial_picture_width"]=picture.width();
                    row["tutorial_playback_icons"]=!play->icon().isNull()&&!replay->icon().isNull()&&
                        play->text().isEmpty()&&replay->text().isEmpty()&&!play->accessibleName().isEmpty()&&!replay->accessibleName().isEmpty();
                    row["tutorial_stage_fraction"]=double(frame->height())/topics->parentWidget()->height();
                    row["tutorial_timeline_below_stage"]=steps->mapTo(dialog,QPoint{}).y()>=frame->mapTo(dialog,QPoint{}).y()+frame->height();
                    for(int chapter=0;chapter<topics->count();++chapter){
                        topics->setCurrentIndex(chapter);
                        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                        const auto nodes=steps->findChildren<QToolButton*>("TutorialFeatureNode");
                        if(chapter==3&&nodes.size()==9){
                            row["tutorial_a4_modes_in_order"]=nodes[5]->toolTip()==uiText("A4 拼页")&&
                                nodes[6]->toolTip()==uiText("无需裁剪")&&
                                nodes[7]->toolTip()==uiText("剪下第 2 页的裁切边");
                        }
                        const auto stem=workflowOutput_+"/views/tutorial-features-"+language+(compact?"-compact":"");
                        const auto firstFrameImage=frame->grab();
                        const auto introduction=frame->property("introduction").toString();
                        const auto introRect=frame->property("introductionRect").toRect();
                        const auto pictureRect=frame->property("pictureRect").toRect();
                        const bool introAbove=!introduction.isEmpty()&&frame->rect().contains(introRect)&&
                            introRect.bottom()<pictureRect.top();
                        const auto initialCaption=frame->grab(introRect).toImage();
                        auto blank=initialCaption;
                        if(!blank.isNull())blank.fill(initialCaption.pixel(0,0));
                        const auto captionIsFixed=[&]{
                            return frame->grab(introRect).toImage()==initialCaption&&
                                frame->property("introduction").toString()==introduction&&
                                frame->property("introductionRect").toRect()==introRect&&
                                frame->property("pictureRect").toRect()==pictureRect;
                        };
                        if(!compact&&language=="zh"&&(chapter==1||chapter==2)){
                            dialog->grab().save(stem+(chapter==1?"-rebuild-intro.png":"-compare-intro.png"));
                        }
                        bool sought=true;
                        int lastX=-1;
                        for(int step=0;step<nodes.size();++step){
                            const auto x=nodes[step]->pos().x();
                            nodes[step]->click();
                            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                            sought=sought&&x>lastX&&nodes[step]->isChecked()&&
                                frame->property("tutorialStep").toInt()==step&&
                                frame->property("frameNumber").toInt()==90*step&&
                                frame->property("introduction").toString()==introduction;
                            lastX=x;
                        }
                        {
                            const auto seekAt=[&](QEvent::Type type,QPoint position,Qt::MouseButton button,Qt::MouseButtons buttons){
                                QMouseEvent event(type,position,steps->mapToGlobal(position),button,buttons,Qt::NoModifier,QPointingDevice::primaryPointingDevice());
                                QApplication::sendEvent(steps,&event);
                            };
                            const auto track=[&](int step,double progress){
                                const int a=nodes[step]->geometry().center().x()+1;
                                const int b=step+1<nodes.size()?nodes[step+1]->geometry().center().x()+1:steps->width()-4;
                                return QPoint(a+qRound((b-a)*progress),steps->height()/2);
                            };
                            const auto select=[&](QPoint position){
                                seekAt(QEvent::MouseButtonPress,position,Qt::LeftButton,Qt::LeftButton);
                                seekAt(QEvent::MouseButtonRelease,position,Qt::LeftButton,Qt::NoButton);
                            };
                            if(chapter==2&&nodes.size()==5){
                                const auto first=track(0,.73),far=track(3,.82),back=track(1,.75);
                                seekAt(QEvent::MouseButtonPress,first,Qt::LeftButton,Qt::LeftButton);
                                const auto firstFrame=frame->property("frameNumber").toInt();
                                seekAt(QEvent::MouseMove,far,Qt::NoButton,Qt::LeftButton);
                                const auto farFrame=frame->property("frameNumber").toInt();
                                seekAt(QEvent::MouseMove,back,Qt::NoButton,Qt::LeftButton);
                                seekAt(QEvent::MouseButtonRelease,back,Qt::LeftButton,Qt::NoButton);
                                const auto returned=frame->property("frameNumber").toInt();
                                row["tutorial_scrub_both_directions"]=firstFrame>57&&firstFrame<90&&farFrame>333&&farFrame<360&&
                                    returned>150&&returned<170&&frame->property("tutorialStep").toInt()==1&&
                                    captionIsFixed();
                                QApplication::setActiveWindow(dialog);
                                play->setFocus(Qt::OtherFocusReason);
                                const auto unfocusedTrack=steps->grab().toImage();
                                steps->setFocus(Qt::MouseFocusReason);
                                const bool trackUnchanged=steps->hasFocus()&&steps->grab().toImage()==unfocusedTrack;
                                nodes[1]->setFocus(Qt::MouseFocusReason);
                                row["tutorial_mouse_focus_preserves_timeline_appearance"]=trackUnchanged&&
                                    nodes[1]->hasFocus()&&steps->grab().toImage()==unfocusedTrack;
                                steps->setFocus(Qt::TabFocusReason);
                                QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
                                QApplication::sendEvent(steps,&right);
                                row["tutorial_timeline_keeps_keyboard_seek"]=steps->hasFocus()&&
                                    frame->property("frameNumber").toInt()>returned;
                                select(back);
                                const auto playIcon=play->icon().cacheKey();
                                play->click();
                                const auto pauseIcon=play->icon().cacheKey();
                                seekAt(QEvent::MouseButtonPress,far,Qt::LeftButton,Qt::LeftButton);
                                seekAt(QEvent::MouseMove,back,Qt::NoButton,Qt::LeftButton);
                                seekAt(QEvent::MouseButtonRelease,back,Qt::LeftButton,Qt::NoButton);
                                QEventLoop progress;
                                QTimer::singleShot(235,&progress,&QEventLoop::quit);
                                progress.exec(QEventLoop::ExcludeUserInputEvents);
                                row["tutorial_scrub_resumes_playback"]=frame->property("frameNumber").toInt()>returned&&
                                    play->icon().cacheKey()==pauseIcon&&playIcon!=pauseIcon;
                                select(track(0,.97));
                                QEventLoop next;
                                QTimer::singleShot(280,&next,&QEventLoop::quit);
                                next.exec(QEventLoop::ExcludeUserInputEvents);
                                row["tutorial_next_step_keeps_caption"]=frame->property("tutorialStep").toInt()==1&&
                                    frame->property("frameNumber").toInt()>90&&captionIsFixed();
                                play->click();
                                row["tutorial_paused_keeps_caption"]=captionIsFixed();
                            }
                            if((language=="zh"&&!compact)||language=="ru"){
                                const auto saveArtwork=[&](const QString& name){
                                    const auto prefix=language=="ru"?stem:workflowOutput_+"/views/tutorial-features";
                                    dialog->grab().save(prefix+"-"+name+".png");
                                };
                                if(chapter==0&&nodes.size()==3){
                                    select(track(2,.97));
                                    saveArtwork("crease-colour");
                                }else if(chapter==2&&nodes.size()==5){
                                    select(track(1,.97));
                                    saveArtwork("compare-widen");
                                    select(track(2,.96));
                                    saveArtwork("compare-draft");
                                    select(track(4,.98));
                                    saveArtwork("compare-load");
                                }else if(chapter==1&&nodes.size()==3){
                                    select(track(1,.50));
                                    saveArtwork("rebuild-transition");
                                    select(track(2,.97));
                                    saveArtwork("rebuild-before-after");
                                }else if(chapter==3&&nodes.size()==9){
                                    select(track(3,.97));
                                    dialog->grab().save(workflowOutput_+"/views/tutorial-features-template-assembly.png");
                                    steps->setFocus(Qt::MouseFocusReason);
                                    const auto left=play->mapTo(dialog,QPoint{});
                                    const auto right=steps->mapTo(dialog,QPoint(steps->width(),steps->height()));
                                    dialog->grab(QRect(left,right)).save(workflowOutput_+"/views/tutorial-features-player-controls.png");
                                }else if(chapter==4&&nodes.size()==3){
                                    select(track(0,.93));
                                    saveArtwork("reference-import");
                                    select(track(1,.97));
                                    saveArtwork("reference-legend");
                                    select(track(2,.97));
                                    saveArtwork("reference-overlay");
                                }else if(chapter==5){
                                    select(track(0,.83));
                                    dialog->grab().save(workflowOutput_+"/views/tutorial-features-print-shell-separate.png");
                                    select(track(3,.96));
                                    dialog->grab().save(workflowOutput_+"/views/tutorial-features-print-shell.png");
                                }
                            }
                            if(chapter==3&&nodes.size()==9&&(!compact||language=="en")){
                                const QList<QPair<int,QString>> drawings={
                                    {0,"section"},{1,"mould"},{2,"kit"},
                                    {5,"tile-modes"},{6,"edge"},{7,"trim"}
                                };
                                for(const auto& [step,label]:drawings){
                                    select(track(step,.56));
                                    dialog->grab().save(stem+"-template-"+label+".png");
                                }
                            }
                        }
                        const bool captionAfterSeek=captionIsFixed();
                        replay->click();
                        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                        const bool replayed=!nodes.isEmpty()&&nodes.first()->isChecked()&&
                            frame->property("frameNumber").toInt()==0&&captionIsFixed();
                        if(chapter==0&&!compact&&language=="zh"){
                            QEventLoop firstMotion;
                            QTimer::singleShot(210,&firstMotion,&QEventLoop::quit);
                            firstMotion.exec(QEventLoop::ExcludeUserInputEvents);
                            row["tutorial_playback_starts_without_caption_delay"]=frame->property("frameNumber").toInt()>0&&
                                frame->grab().toImage()!=firstFrameImage.toImage()&&captionIsFixed();
                        }
                        play->click();
                        chapters.append(QJsonObject{{"name",topics->tabText(chapter)},
                            {"introduction",introduction},{"seek",sought},{"intro_above",introAbove},{"replay",replayed},
                            {"caption_painted_from_first_frame",initialCaption!=blank},{"caption_fixed_after_seek",captionAfterSeek},
                            {"icon",!topics->tabIcon(chapter).isNull()},{"node_count",nodes.size()}});
                    }
                    topics->setCurrentIndex(0);
                    sections->setCurrentIndex(0);
                }
                row["feature_chapters"]=chapters;
                for(auto* scroll:dialog->findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(0);
            }
            surfaces.append(row);
        }
    };
    for(const auto& language:QStringList{"zh","en"}){
        setUiLanguage(language);
        if(!copyOnly){
            auto* tutorial=showTutorialGuide(this);
            capture(tutorial,"tutorial",language);
            tutorial->close();
            QTimer::singleShot(0,this,[&]{auto* dialog=qobject_cast<QDialog*>(frontStudioDialog());if(dialog){capture(dialog,"shortcuts",language);dialog->reject();}});showShortcutGuide(this);
        }
        QTimer::singleShot(0,this,[&]{
            auto* about=qobject_cast<QDialog*>(frontStudioDialog());if(!about)return;
            if(!copyOnly)capture(about,"about",language);
            for(bool version:{true,false}){
                if(copyOnly&&version)continue;
                auto* button=about->findChild<QPushButton*>(version?"AboutVersionToggle":"AboutCopyrightToggle");if(!button)continue;
                QTimer::singleShot(0,this,[&,version]{
                    auto* child=qobject_cast<QDialog*>(frontStudioDialog());if(!child)return;
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
    if(!copyOnly){
        setUiLanguage("zh");
        auto* tutorial=showTutorialGuide(this);
        setUiLanguage("ru");
        capture(tutorial,"tutorial","ru");
        tutorial->close();
    }
    workflowArtifacts_["read_surfaces"]=surfaces;workflowArtifacts_["email_copy"]=copies;
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setUiLanguage(oldLanguage);
}
