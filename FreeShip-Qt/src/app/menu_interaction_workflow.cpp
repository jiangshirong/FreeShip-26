// Production menu input and tutorial observations in an explicitly private run.
// No global input, personal files/settings, or user-owned HWND access.
#include "main_window.h"
#include "layer_panel.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "project_dialog.h"
#include "section_dialog.h"
#include <QDialogButtonBox>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QMenu>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QPointer>
#include <QScrollArea>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class ToolbarRepeatMouseObserver final:public QObject{
public:
    QPointer<QMenu> menu;QPoint target;int nativePresses=0;
    bool eventFilter(QObject* object,QEvent* event)override{
        if(object==menu&&event->type()==QEvent::MouseButtonPress&&event->spontaneous()){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton&&(mouse->globalPosition().toPoint()-target).manhattanLength()<3)++nativePresses;
        }return false;
    }
};
class ToolbarNativeMoveObserver final:public QObject{
public:
    QPointer<QToolButton> target;int moves=0,enters=0,totalMoves=0;
    bool eventFilter(QObject* object,QEvent* event)override{
        if(object==target&&event->type()==QEvent::MouseMove)++totalMoves;
        if(object==target&&event->spontaneous()){
            if(event->type()==QEvent::MouseMove)++moves;
            if(event->type()==QEvent::Enter)++enters;
        }return false;
    }
};

void MainWindow::auditMenuInteraction(){
    if(!startupProbe_||!session_.ready()||session_.busy()){finishWorkflow("Private menu input audit needs an idle model");return;}
    qApp->setQuitOnLastWindowClosed(false);QDir().mkpath(workflowOutput_+"/views");
    const auto original=session_.model();const auto selection=selection_;const bool modified=session_.modified(),oldWorkflow=workflow_;workflow_=true;
    const QString oldLanguage=uiLanguage();
    auto findAction=[&](const QString& operation){
        QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("operation")==operation)found.append(action);
        if(found.size()!=1)throw std::runtime_error(("Missing or ambiguous production action: "+operation).toStdString());
        return found.front();
    };
    auto pathFor=[&](QAction* wanted){
        QList<QMenu*> result;QSet<QMenu*> seen;
        std::function<bool(QMenu*,QList<QMenu*>)> visit=[&](QMenu* menu,QList<QMenu*> path){
            if(seen.contains(menu))return false;
            seen.insert(menu);path.append(menu);
            if(menu->actions().contains(wanted)){result=path;return true;}
            for(auto* action:menu->actions())if(action->menu()&&visit(action->menu(),path))return true;
            return false;
        };
        for(auto* button:menuWidget()->findChildren<QToolButton*>())if(button->defaultAction()&&button->defaultAction()->menu())if(visit(button->defaultAction()->menu(),{}))break;
        if(result.isEmpty())throw std::runtime_error("Action has no production menu path");
        return result;
    };
    auto popup=[this](const QList<QMenu*>& path){
        const auto origin=menuWidget()->mapToGlobal(QPoint(10,menuWidget()->height()+10));
        int offset=0;for(auto* menu:path){menu->setAttribute(Qt::WA_DontShowOnScreen);menu->popup(origin+QPoint(offset,0));offset+=menu->width();}
    };
    auto click=[](QMenu* menu,QAction* action,bool ctrl){
        const QPointF local=menu->actionGeometry(action).center(),global=menu->mapToGlobal(local.toPoint());
        const auto modifiers=ctrl?Qt::ControlModifier:Qt::NoModifier;
        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,modifiers);
        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,modifiers);
        QApplication::sendEvent(menu,&press);QApplication::sendEvent(menu,&release);
    };
    try{
        // The user's single selection-mode menu contains the existing actions.
        // Exercise real menu mouse dispatch and observe exclusive checks/views.
        {
        const auto previousKind=selectionKind_;const auto previousEntities=entitySelection_;
        const QList<QPair<QString,HullEntity>> kinds{{"select_point",HullEntity::Point},{"select_edge",HullEntity::Edge},
            {"select_face",HullEntity::Face},{"select_curve",HullEntity::Curve},{"select_flow",HullEntity::Flowline}};
        auto* selector=findAction("selection_modes");auto* menu=selector->menu();
        if(!menu)throw std::runtime_error("Selection toolbar menu unavailable");
        QSet<QMenu*> seen;QJsonArray owners;
        std::function<void(QMenu*)> visit=[&](QMenu* current){
            if(seen.contains(current))return;seen.insert(current);
            for(auto* a:current->actions()){
                for(const auto& kind:kinds)if(a->property("operation")==kind.first)owners.append(QJsonObject{{"operation",kind.first},{"selection_menu",current==menu}});
                if(a->menu())visit(a->menu());
            }
        };
        for(auto* button:menuWidget()->findChildren<QToolButton*>())if(button->defaultAction()&&button->defaultAction()->menu())visit(button->defaultAction()->menu());
        QJsonArray states;
        for(const auto& kind:kinds){
            auto* action=findAction(kind.first);const auto path=pathFor(action);popup(path);QApplication::processEvents();
            click(menu,action,false);
            QJsonArray checked;for(const auto& other:kinds)if(findAction(other.first)->isChecked())checked.append(other.first);
            bool views=true;for(auto* view:views_)views&=view->selectionKind()==kind.second;
            states.append(QJsonObject{{"operation",kind.first},{"checked",checked},{"current_kind",selectionKind_==kind.second},
                {"views",views},{"direct_toolbar_menu",path.size()==1&&path.front()==menu},{"checkable",action->isCheckable()}});
        }
        auto* selectAll=findAction("select_all");auto* invert=findAction("invert_selection");
        const bool commandsDirect=pathFor(selectAll)==QList<QMenu*>{menu}&&pathFor(invert)==QList<QMenu*>{menu};
        bool allPointsSelected=false,complementSelected=false;
        if(commandsDirect){
            setSelectionKind(HullEntity::Edge);
            popup({menu});click(menu,selectAll,false);
            const auto all=selection_;
            allPointsSelected=selectionKind_==HullEntity::Point&&findAction("select_point")->isChecked()&&all.size()==original->points.size();
            if(!all.isEmpty()){
                const auto id=*all.begin();selectPoints({id});auto complement=all;complement.remove(id);
                popup({menu});click(menu,invert,false);
                complementSelected=selectionKind_==HullEntity::Point&&selection_==complement;
            }
        }
        workflowCase("point-commands-in-selection-menu",commandsDirect&&!selectAll->isCheckable()&&!invert->isCheckable());
        workflowCase("select-all-points-from-edge-mode",allPointsSelected);
        workflowCase("invert-one-selected-point",complementSelected);
        popup({menu});QApplication::processEvents();menu->grab().save(workflowOutput_+"/views/selection-menu.png");menu->hide();
        workflowArtifacts_["selection_modes"]=QJsonObject{{"owners",owners},{"states",states},{"entries",menu->actions().size()},
            {"icon",selector->icon().name()},{"model_unchanged",session_.model()==original&&session_.modified()==modified}};
        menuWidget()->grab().save(workflowOutput_+"/views/selection-toolbar.png");
        setSelectionKind(previousKind);if(previousKind==HullEntity::Point)selectPoints(selection);else selectEntities(previousKind,previousEntities);
        }
        // Feed ordinary no-button mouse events through the production toolbar.
        // While a popup owns mouse delivery, send to that popup with the next
        // button's global position. Never move the system cursor.
        {
        const bool wasVisible=isVisible(),wasHidden=testAttribute(Qt::WA_DontShowOnScreen);
        setAttribute(Qt::WA_DontShowOnScreen);show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QList<QToolButton*> hoverButtons;
        for(auto* button:menuWidget()->findChildren<QToolButton*>())
            if(button->isVisible()&&button->isEnabled()&&button->property("studio_hover_popup").toBool())hoverButtons.append(button);
        const auto waitHover=[](){QEventLoop loop;QTimer::singleShot(145,&loop,&QEventLoop::quit);loop.exec();};
        const auto moveTo=[](QWidget* receiver,QToolButton* target){
            const QPointF global=target->mapToGlobal(target->rect().center());
            QMouseEvent move(QEvent::MouseMove,receiver->mapFromGlobal(global.toPoint()),global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(receiver,&move);
        };
        const auto menuFor=[](QToolButton* button){return button->menu()?button->menu():button->defaultAction()->menu();};
        int toolbarCalls=0;QList<QMetaObject::Connection> toolbarConnections;
        for(auto* button:hoverButtons)toolbarConnections.append(connect(menuFor(button),&QMenu::triggered,this,[&]{++toolbarCalls;}));
        QJsonArray toolbarHover;QJsonObject toolbarBehavior;
#ifdef Q_OS_WIN
        {
            // Native Windows delivery on this test's own non-input desktop.
            // No SetCursorPos, SendInput, or access to a user-owned window.
            setAttribute(Qt::WA_DontShowOnScreen,false);show();
            const auto hwnd=reinterpret_cast<HWND>(winId());ShowWindow(hwnd,SW_SHOWNOACTIVATE);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            ToolbarNativeMoveObserver native;qApp->installEventFilter(&native);QJsonArray observations;
            for(auto* button:hoverButtons){
                menuFor(button)->setAttribute(Qt::WA_DontShowOnScreen,false);
                native.target=button;native.moves=native.enters=native.totalMoves=0;
                const auto point=button->mapTo(this,button->rect().center());const qreal ratio=devicePixelRatioF();
                PostMessageW(hwnd,WM_MOUSEMOVE,0,MAKELPARAM(qRound(point.x()*ratio),qRound(point.y()*ratio)));
                waitHover();auto* menu=menuFor(button);waitHover();
                const bool nativeVisible=menu->isVisible();
                const int nativeMoves=native.moves,nativeEnters=native.enters;
                // Also exercise the actual QWindow-to-alien-widget routing,
                // rather than sending directly to a chosen tool button.
                QMouseEvent routed(QEvent::MouseMove,QPointF(point),QPointF(mapToGlobal(point)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(windowHandle(),&routed);waitHover();waitHover();
                BYTE alpha=255;COLORREF key=0;DWORD flags=0;const auto popup=reinterpret_cast<HWND>(menu->winId());
                const bool layered=GetLayeredWindowAttributes(popup,&key,&alpha,&flags);
                observations.append(QJsonObject{{"caption",button->text()},{"moves",native.moves},{"enters",native.enters},
                    {"native_visible_before_routed_event",nativeVisible},{"routed_moves",native.totalMoves},
                    {"native_moves_before_routed",nativeMoves},{"native_enters_before_routed",nativeEnters},
                    {"visible",menu->isVisible()},{"native_visible",bool(IsWindowVisible(popup))},{"opacity",menu->windowOpacity()},
                    {"layered",layered},{"native_alpha",int(alpha)},{"mask_height",menu->mask().isEmpty()?menu->height():menu->mask().boundingRect().height()},
                    {"height",menu->height()},{"x",menu->pos().x()},{"y",menu->pos().y()}});
                menu->close();
            }
            qApp->removeEventFilter(&native);workflowArtifacts_["native_toolbar_hover"]=observations;
            hide();setAttribute(Qt::WA_DontShowOnScreen);show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
#endif
        if(!hoverButtons.isEmpty()){
            auto* first=hoverButtons.front();auto* menu=menuFor(first);
            const QPoint covered=first->mapToGlobal(first->rect().center());
            QDialog modeless(this);
            modeless.setAttribute(Qt::WA_DontShowOnScreen);
            modeless.resize(160,100);modeless.move(covered-QPoint(80,50));
            QPushButton dialogButton(&modeless);
            dialogButton.setGeometry(modeless.rect());
            modeless.show();
            moveTo(&dialogButton,first);waitHover();
            toolbarBehavior["modeless_hover_does_not_open"]=!menu->isVisible();
            menu->close();
            bool clicked=false;connect(&dialogButton,&QPushButton::clicked,&modeless,[&]{clicked=true;});
            const QPoint localClick=dialogButton.mapFromGlobal(covered);
            QMouseEvent press(QEvent::MouseButtonPress,localClick,covered,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease,localClick,covered,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&dialogButton,&press);QApplication::sendEvent(&dialogButton,&release);
            toolbarBehavior["modeless_click_reaches_dialog"]=clicked&&!menu->isVisible();
            menu->close();
            modeless.hide();
            moveTo(first,first);QEvent leave(QEvent::Leave);QApplication::sendEvent(first,&leave);waitHover();
            toolbarBehavior["leave_before_open_cancels"]=!menu->isVisible();
            const QPointF local=first->rect().center(),global=first->mapToGlobal(local.toPoint());
            QEnterEvent enter(local,first->mapTo(window(),local.toPoint()),global);
            QApplication::sendEvent(first,&enter);waitHover();
            toolbarBehavior["enter_event_opens_without_mouse_move"]=menu->isVisible();menu->close();
            first->setEnabled(false);moveTo(first,first);waitHover();toolbarBehavior["disabled_does_not_open"]=!menu->isVisible();first->setEnabled(true);
        }
        // A hover-open menu grabs pointer moves. The anchor can miss Leave;
        // its painted surface must still return to idle after dismissal.
        for(auto* button:hoverButtons){
            auto* menu=menuFor(button);const auto rows=menu->actions();
            if(std::none_of(rows.begin(),rows.end(),[](auto* row){return row->property("operation")=="both_sides";}))continue;
            const auto sample=[button]{const auto image=button->grab().toImage();const auto px=qRound(6*image.devicePixelRatio());return image.pixelColor(px,px);};
            const auto idle=sample();
            const QPointF local=button->rect().center(),global=button->mapToGlobal(local.toPoint());
            QEnterEvent enter(local,button->mapTo(window(),local.toPoint()),global);QApplication::sendEvent(button,&enter);
            waitHover();sample();waitHover();
            const auto lit=sample();const bool open=menu->isVisible()&&button->isDown();
            const auto outside=menu->frameGeometry().bottomRight()+QPoint(60,60);
            QMouseEvent exit(QEvent::MouseMove,menu->mapFromGlobal(outside),outside,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(menu,&exit);
            for(int i=0;i<4;++i){waitHover();sample();}
            const auto after=sample();
            toolbarBehavior["view_popup_highlight"]=QJsonObject{{"opened",open},{"closed",!menu->isVisible()&&!button->isDown()},
                {"idle",idle.name(QColor::HexArgb)},{"open",lit.name(QColor::HexArgb)},{"after",after.name(QColor::HexArgb)}};
            menu->close();break;
        }
        QPointer<QMenu> previous;
        for(auto* button:hoverButtons){
            auto* menu=menuFor(button);QElapsedTimer elapsed;elapsed.start();
            moveTo(previous?static_cast<QWidget*>(previous.data()):button,button);waitHover();
            toolbarHover.append(QJsonObject{{"caption",button->text()},{"opened_without_click",menu->isVisible()},
                {"previous_closed",!previous||!previous->isVisible()},{"button_active",button->isDown()},
                {"activation_count",toolbarCalls},{"elapsed_ms",elapsed.elapsed()}});
            previous=menu;
        }
        if(previous&&!hoverButtons.isEmpty()){
            const auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
            const auto moveGlobal=[](QMenu* menu,QPoint global){QMouseEvent move(QEvent::MouseMove,menu->mapFromGlobal(global),global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(menu,&move);};
            auto* button=hoverButtons.last();auto* menu=previous.data();
            int shows=0,hides=0;const auto onShow=connect(menu,&QMenu::aboutToShow,this,[&]{++shows;});const auto onHide=connect(menu,&QMenu::aboutToHide,this,[&]{++hides;});
            QAction* active=nullptr;for(auto* row:menu->actions())if(row->isEnabled()&&!row->isSeparator()){active=row;break;}menu->setActiveAction(active);
            const QPoint global=button->mapToGlobal(button->rect().center());const QPoint local=menu->mapFromGlobal(global);
            for(int i=0;i<3;++i){
                QMouseEvent press(i==2?QEvent::MouseButtonDblClick:QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(menu,&press);QApplication::sendEvent(menu,&release);
            }wait(35);
            toolbarBehavior["repeat_click_keeps_popup"]=menu->isVisible()&&shows==0&&hides==0&&menu->activeAction()==active&&button->isDown();
            toolbarBehavior["repeat_click_show_count"]=shows;toolbarBehavior["repeat_click_hide_count"]=hides;
#ifdef Q_OS_WIN
            ToolbarRepeatMouseObserver observer;observer.menu=menu;observer.target=global;qApp->installEventFilter(&observer);
            const auto ratio=menu->devicePixelRatioF();
            const auto position=MAKELPARAM(WORD(qRound(local.x()*ratio)),WORD(qRound(local.y()*ratio)));const auto handle=reinterpret_cast<HWND>(menu->winId());
            const bool posted=PostMessageW(handle,WM_LBUTTONDOWN,MK_LBUTTON,position)&&PostMessageW(handle,WM_LBUTTONUP,0,position);wait(60);
            qApp->removeEventFilter(&observer);
            toolbarBehavior["native_repeat_click"]=QJsonObject{{"posted",posted},{"delivered_presses",observer.nativePresses},{"visible",menu->isVisible()},{"show_count",shows},{"hide_count",hides}};
#endif
            disconnect(onShow);disconnect(onHide);
            const auto buttonBottom=button->mapToGlobal(QPoint(button->width()/2,button->height()));
            moveGlobal(menu,buttonBottom+QPoint(0,3));wait(120);moveGlobal(menu,menu->geometry().topLeft()+QPoint(24,10));wait(260);
            toolbarBehavior["button_to_menu_bridge_stays_open"]=menu->isVisible();
            const auto bounds=menu->frameGeometry();
            moveGlobal(menu,QPoint(bounds.right()+6,bounds.center().y()));wait(260);const bool widthMargin=menu->isVisible();
            moveGlobal(menu,QPoint(bounds.center().x(),bounds.bottom()+4));wait(260);
            toolbarBehavior["small_width_and_height_margin"]=widthMargin&&menu->isVisible();
            const auto outside=bounds.bottomRight()+QPoint(60,60);
            moveGlobal(menu,outside);wait(70);toolbarBehavior["exit_has_grace_period"]=menu->isVisible();
            moveGlobal(menu,bounds.center());wait(270);toolbarBehavior["reenter_cancels_dismissal"]=menu->isVisible();
            moveGlobal(menu,outside);wait(280);toolbarBehavior["outside_region_closes"]=!menu->isVisible()&&!button->isDown();
            moveTo(button,button);waitHover();
            QMouseEvent heldPress(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(menu,&heldPress);
            QMouseEvent heldMove(QEvent::MouseMove,menu->mapFromGlobal(outside),outside,Qt::NoButton,Qt::LeftButton,Qt::NoModifier),heldRelease(QEvent::MouseButtonRelease,menu->mapFromGlobal(outside),outside,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(menu,&heldMove);QApplication::sendEvent(menu,&heldRelease);wait(280);
            toolbarBehavior["repeat_press_drag_release_outside_closes"]=!menu->isVisible();
            // Include an actual child popup in the protected region, then leave
            // the complete chain. No action is triggered to open the child.
            // Discover a button that actually owns a populated child menu.
            // Toolbar creation order can start with a flat selection-mode menu.
            QToolButton* parentButton=nullptr;QMenu* root=nullptr;QAction* submenu=nullptr;
            for(auto* candidate:hoverButtons){
                for(auto* row:menuFor(candidate)->actions())if(row->isEnabled()&&row->menu()&&!row->menu()->actions().isEmpty()){
                    parentButton=candidate;root=menuFor(candidate);submenu=row;break;
                }
                if(submenu)break;
            }
            if(submenu){
                moveTo(parentButton,parentButton);waitHover();
                auto* child=submenu->menu();moveGlobal(root,root->mapToGlobal(root->actionGeometry(submenu).center()));wait(40);
                if(child->isVisible()){moveGlobal(child,child->frameGeometry().center());wait(260);}
                toolbarBehavior["submenu_region_stays_open"]=root->isVisible()&&child->isVisible();
                const auto outsideChain=root->frameGeometry().united(child->frameGeometry()).bottomRight()+QPoint(60,60);
                moveGlobal(child,outsideChain);wait(280);toolbarBehavior["outside_closes_submenu_chain"]=!root->isVisible()&&!child->isVisible();
            }
            moveTo(button,button);waitHover();previous=menu;
        }
        if(previous){
            QAction* row=nullptr;for(auto* action:previous->actions())if(!action->isSeparator()&&action->isEnabled()){row=action;break;}
            if(row){const QPointF local=previous->actionGeometry(row).center(),global=previous->mapToGlobal(local.toPoint());
                QMouseEvent move(QEvent::MouseMove,local,global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(previous,&move);waitHover();
                toolbarBehavior["moving_into_menu_keeps_open"]=previous->isVisible();
            }
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(previous,&escape);
            toolbarBehavior["escape_closes"]=!previous->isVisible();
        }
        for(auto* button:menuWidget()->findChildren<QToolButton*>())if(button->isVisible()&&button->isEnabled()&&button->defaultAction()&&!button->defaultAction()->menu()){
            moveTo(button,button);waitHover();toolbarBehavior["plain_button_hover_has_no_popup"]=!QApplication::activePopupWidget()&&!QApplication::activeModalWidget();break;
        }
        QJsonArray persistentClicks;
        const auto waitForClick=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
        for(auto* button:hoverButtons){
            auto* menu=menuFor(button);menu->setAttribute(Qt::WA_DontShowOnScreen,false);
            bool shown=false,forcedClose=false;QTimer safety;safety.setSingleShot(true);
            connect(&safety,&QTimer::timeout,menu,[&]{forcedClose=true;menu->close();});
            auto connection=connect(menu,&QMenu::aboutToShow,this,[&]{shown=true;});
            safety.start(800);
            const QPointF local=button->rect().center(),global=button->mapToGlobal(local.toPoint());
            QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QElapsedTimer elapsed;elapsed.start();QApplication::sendEvent(button,&press);
            const bool returnedWithoutNestedLoop=!forcedClose;const auto dispatchMs=elapsed.elapsed();
            QApplication::sendEvent(button,&release);QJsonArray frames;
            for(int delay:{0,40,120,340}){
                if(delay)waitForClick(delay);
                QJsonObject frame{{"elapsed_ms",double(elapsed.elapsed())},{"visible",menu->isVisible()},
                    {"mask_empty",menu->mask().isEmpty()},{"opacity",menu->windowOpacity()}};
#ifdef Q_OS_WIN
                const auto hwnd=reinterpret_cast<HWND>(menu->winId());
                frame["native_visible"]=bool(IsWindowVisible(hwnd));
                HRGN region=CreateRectRgn(0,0,0,0);RECT regionBounds{},bounds{};const int kind=GetWindowRgn(hwnd,region);
                GetRgnBox(region,&regionBounds);GetWindowRect(hwnd,&bounds);DeleteObject(region);
                frame["native_region_full"]=kind==ERROR||(regionBounds.right-regionBounds.left>=bounds.right-bounds.left&&regionBounds.bottom-regionBounds.top>=bounds.bottom-bounds.top);
#endif
                frames.append(frame);
            }
            int ink=0;if(menu->isVisible()){
                const auto pixels=menu->grab().toImage();
                for(int y=0;y<pixels.height();y+=2)for(int x=0;x<pixels.width();x+=2){const auto c=pixels.pixelColor(x,y);if(c.alpha()>200&&c.green()<210)++ink;}
            }
            persistentClicks.append(QJsonObject{{"button",button->defaultAction()?button->defaultAction()->property("operation").toString():QString("overflow")},
                {"shown",shown},{"nonblocking",returnedWithoutNestedLoop},{"dispatch_ms",double(dispatchMs)},{"ink_samples",ink},{"frames",frames}});
            safety.stop();disconnect(connection);menu->close();waitForClick(30);
        }
        toolbarBehavior["normal_click_still_opens"]=!persistentClicks.isEmpty()&&std::all_of(persistentClicks.cbegin(),persistentClicks.cend(),[](const QJsonValue& row){return row.toObject()["shown"].toBool();});
        workflowArtifacts_["toolbar_persistent_clicks"]=persistentClicks;
        toolbarBehavior["activation_count"]=toolbarCalls;
        workflowArtifacts_["toolbar_hover"]=toolbarHover;workflowArtifacts_["toolbar_hover_behavior"]=toolbarBehavior;
        for(const auto& connection:toolbarConnections)disconnect(connection);
        if(!wasVisible)hide();setAttribute(Qt::WA_DontShowOnScreen,wasHidden);
        }
        QJsonArray backgroundPopups;
        for(auto* view:views_)if(view->backgroundView()<3)for(bool ctrl:{false,true}){
            const auto originalCallback=view->onBackgroundAction;
            bool called=false;int calls=0;
            view->onBackgroundAction=[&](const QString& operation,QPointF){
                if(operation!="import")return;
                called=true;++calls;QDialog modal(this);modal.setAttribute(Qt::WA_DontShowOnScreen);
                QTimer::singleShot(10,&modal,&QDialog::reject);modal.exec();
            };
            QContextMenuEvent event(QContextMenuEvent::Keyboard,{20,20},view->mapToGlobal(QPoint(20,20)));
            QApplication::sendEvent(view,&event);
            QMenu* submenu=nullptr;QAction* import=nullptr;
            for(auto* menu:view->findChildren<QMenu*>())if(menu->isVisible()||menu->property("menu_role")=="background")
                for(auto* action:menu->actions())if(action->property("background_operation")=="import"){submenu=menu;import=action;}
            if(!import){view->onBackgroundAction=originalCallback;throw std::runtime_error("Background import action missing");}
            submenu->setAttribute(Qt::WA_DontShowOnScreen);submenu->popup({60,60});
            click(submenu,import,ctrl);const bool deferred=!called;
            QApplication::processEvents();
            view->onBackgroundAction=originalCallback;
            backgroundPopups.append(QJsonObject{{"view",view->backgroundView()},{"ctrl",ctrl},{"deferred",deferred},{"calls",calls}});
        }
        workflowArtifacts_["background_popup"]=backgroundPopups;
        auto* grid=findAction("overlay_grid");const bool checked=grid->isChecked();auto path=pathFor(grid);
        // Only move over actual production submenu rows. No mouse button or
        // QAction activation is used to obtain the visible child menu.
        QJsonArray hoverRows;auto* root=path.last();int hoverActivations=0;
        const auto hoverBinding=connect(root,&QMenu::triggered,this,[&]{++hoverActivations;});
        popup(path);QPointer<QMenu> previous;
        for(auto* entry:root->actions())if(entry->isEnabled()&&entry->isVisible()&&entry->menu()){
            auto* child=entry->menu();child->setAttribute(Qt::WA_DontShowOnScreen);
            const auto position=root->actionGeometry(entry).center();QElapsedTimer elapsed;elapsed.start();
            for(const auto local:{position+QPoint(-3,0),position}){
                QMouseEvent move(QEvent::MouseMove,QPointF(local),QPointF(root->mapToGlobal(local)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(root,&move);
            }
            QEventLoop loop;QTimer::singleShot(30,&loop,&QEventLoop::quit);loop.exec();
            hoverRows.append(QJsonObject{{"caption",entry->text()},{"opened_without_click",child->isVisible()},{"previous_closed",!previous||!previous->isVisible()},
                {"parent_stays_open",root->isVisible()},{"activation_count",hoverActivations},{"elapsed_ms",elapsed.elapsed()}});previous=child;
        }
        if(previous)previous->hide();for(auto* menu:path)menu->hide();disconnect(hoverBinding);workflowArtifacts_["submenu_hover"]=hoverRows;
        popup(path);
        int calls=0;auto binding=connect(grid,&QAction::triggered,this,[&]{++calls;});QJsonArray toggles;
        for(int i=0;i<3;++i){
            click(path.last(),grid,i<2);QJsonArray views;for(auto* view:views_)views.append(view->gridVisible());
            toggles.append(QJsonObject{{"ctrl",i<2},{"menu_visible",path.last()->isVisible()},{"checked",grid->isChecked()},{"calls",calls},{"views",views}});
        }
        disconnect(binding);if(grid->isChecked()!=checked)grid->trigger();for(auto* menu:path)menu->hide();workflowArtifacts_["ctrl_toggle"]=toggles;

        auto* select=findAction("select_all");path=pathFor(select);popup(path);calls=0;binding=connect(select,&QAction::triggered,this,[&]{++calls;});QJsonArray commands;
        for(int i=0;i<3;++i){
            click(path.last(),select,i<2);bool parents=true;for(auto* menu:path)parents=parents&&menu->isVisible();
            commands.append(QJsonObject{{"ctrl",i<2},{"menu_visible",path.last()->isVisible()},{"all_menus_visible",parents},{"calls",calls},{"selected",int(selection_.size())}});
        }
        disconnect(binding);for(auto* menu:path)menu->hide();selectPoints(selection);workflowArtifacts_["ctrl_command"]=commands;

        path=pathFor(layerProperties_);popup(path);click(path.last(),layerProperties_,true);
        workflowArtifacts_["ctrl_dialog"]=QJsonObject{{"marked_as_dialog",studioMenuOpensDialog(layerProperties_)},{"menu_visible",path.last()->isVisible()},{"window_opened",layerDialog_->isVisible()}};
        layerDialog_->close();for(auto* menu:path)menu->hide();

        QJsonArray guides;auto* tutorial=findAction("tutorial");
        for(const auto& language:QStringList{"zh","en"}){
            const auto error=setUiLanguage(language);if(!error.isEmpty())throw std::runtime_error(error.toStdString());
            bool captured=false;QTimer timer;timer.setSingleShot(true);
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* dialog=qobject_cast<QDialog*>(frontStudioDialog());if(!dialog)return;
                QJsonArray kinds;bool inside=true;
                bool tabsOnlyText=true;
                auto* demoTabs=dialog->findChild<QTabBar*>("TutorialDemoTabs");
                if(demoTabs)for(int i=0;i<demoTabs->count();++i){
                    const auto kind=demoTabs->tabData(i).toString();
                    if(kind!="ctrl"&&kind!="window")kinds.append(kind);
                    tabsOnlyText=tabsOnlyText&&!demoTabs->tabButton(i,QTabBar::RightSide)&&demoTabs->tabIcon(i).isNull();
                }
                for(auto* label:dialog->findChildren<QLabel*>()){
                    if(label->window()!=dialog||!label->isVisible())continue;
                    // Scrolled content may extend below the viewport. Its
                    // actual containing surface, not the dialog, owns bounds.
                    QWidget* surface=dialog;
                    for(auto* scroll:dialog->findChildren<QScrollArea*>())
                        if(scroll->widget()&&(scroll->widget()==label||scroll->widget()->isAncestorOf(label)))surface=scroll->widget();
                    inside=inside&&surface->rect().contains(QRect(label->mapTo(surface,QPoint(0,0)),label->size()));
                }
                const auto image=workflowOutput_+"/views/tutorial-"+language+".png";dialog->grab().save(image);
                auto* demo=dialog->findChild<QWidget*>("TutorialMenuDemo");
                auto* actionDemo=dialog->findChild<QWidget*>("TutorialActionDemo");
                auto* windowDemo=dialog->findChild<QWidget*>("TutorialWindowDemo");
                auto* replay=dialog->findChild<QPushButton*>("TutorialMenuReplay");
                QJsonArray animation,examples,windowAnimation;
                const auto explanation=[](QWidget* demoWidget){
                    const auto* label=demoWidget->parentWidget()->findChild<QLabel*>("TutorialDemoExplanation");
                    return label&&label->isVisible()?label->text():QString{};
                };
                const auto livePopup=[](QWidget* demoWidget){
                    for(auto* menu:demoWidget->findChildren<QMenu*>("TutorialLivePopup"))
                        if(menu->isVisible()&&menu->windowType()==Qt::Popup&&!menu->actions().isEmpty()&&
                           menu->actionGeometry(menu->actions().first()).isValid())return menu;
                    return static_cast<QMenu*>(nullptr);
                };
                const auto windowExample=[](QWidget* demoWidget){
                    auto* view=demoWidget->findChild<QGraphicsView*>("TutorialWindowExample");
                    if(view&&view->isVisible())for(auto* item:view->scene()->items()){
                        auto* proxy=qgraphicsitem_cast<QGraphicsProxyWidget*>(item);
                        if(!proxy)continue;
                        if(auto* area=qobject_cast<QMdiArea*>(proxy->widget());area&&!area->subWindowList().isEmpty())
                            return area->subWindowList().first();
                    }
                    return static_cast<QMdiSubWindow*>(nullptr);
                };
                const auto staysInPreview=[dialog](QMdiSubWindow* sample){
                    return sample&&sample->isVisible()&&sample->windowType()==Qt::SubWindow&&
                        sample->widget()&&!sample->widget()->isWindow()&&sample->parentWidget()->rect().contains(sample->geometry())&&
                        dialog&&!dialog->isModal();
                };
                QString ctrlExplanation,windowExplanation;
                bool compactFits=false,compactTabsFit=false;
                if(demo&&replay&&demoTabs&&actionDemo&&windowDemo){
                    const bool startsWithCommand=demoTabs->currentIndex()==0&&actionDemo->isVisible();
                    demoTabs->setCurrentIndex(4);
                    ctrlExplanation=explanation(demo);
                    const auto capture=[&](const QString& phase){
                        const auto frame=workflowOutput_+"/views/tutorial-"+language+"-"+phase+".png";
                        demo->grab().save(frame);
                        const auto* menu=livePopup(demo);
                        int switchedOff=0;
                        if(menu)for(const auto* entry:menu->actions())
                            if(entry->isCheckable()&&!entry->isChecked())++switchedOff;
                        animation.append(QJsonObject{{"phase",phase},{"open",demo->property("tutorial_menu_open").toBool()},
                            {"ctrl",demo->property("tutorial_ctrl_down").toBool()},
                            {"clicks",demo->property("tutorial_click_count").toInt()},{"live_popup",bool(menu)},
                            {"switched_off",switchedOff},{"image",frame}});
                        if(auto* popup=livePopup(demo))popup->grab().save(frame.chopped(4)+"-popup.png");
                    };
                    const auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
                    wait(1650);capture("first");
                    wait(920);capture("second");
                    dialog->grab().save(image);
                    wait(800);capture("released");
                    replay->click();capture("replay");
                    dialog->resize(660,500);
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    compactTabsFit=true;
                    for(int i=0;i<demoTabs->count();++i)
                        compactTabsFit=compactTabsFit&&demoTabs->rect().contains(demoTabs->tabRect(i));
                    if(auto* guideScroll=dialog->findChild<QScrollArea*>()){
                        const auto* viewport=guideScroll->viewport();
                        const auto box=QRect(demo->mapTo(viewport,QPoint{}),demo->size());
                        compactFits=viewport->rect().contains(box);
                    }
                    dialog->grab().save(workflowOutput_+"/views/tutorial-"+language+"-compact.png");
                    for(int i=0;i<4;++i){
                        demoTabs->setCurrentIndex(i);
                        const bool visible=actionDemo->isVisible()&&!demo->isVisible();
                        const bool startsEmpty=actionDemo->property("tutorial_action_result").toString().isEmpty();
                        const auto before=actionDemo->grab().toImage();
                        wait(520);
                        auto* popup=livePopup(actionDemo);
                        const bool showsActualMenu=popup&&popup->testAttribute(Qt::WA_DontShowOnScreen)==
                            dialog->testAttribute(Qt::WA_DontShowOnScreen);
                        if(popup)popup->grab().save(workflowOutput_+"/views/tutorial-"+language+"-"+demoTabs->tabData(i).toString()+"-popup.png");
                        wait(710);
                        const auto result=actionDemo->property("tutorial_action_result").toString();
                        const auto after=actionDemo->grab().toImage();
                        const auto resultImage=workflowOutput_+"/views/tutorial-"+language+"-"+demoTabs->tabData(i).toString()+".png";
                        after.save(resultImage);
                        dialog->grab().save(resultImage.chopped(4)+"-guide.png");
                        const bool menuOpen=actionDemo->property("tutorial_action_menu_open").toBool();
                        auto* submenu=actionDemo->findChild<QMenu*>("TutorialLiveSubmenu");
                        const bool liveSubmenu=submenu&&submenu->isVisible()&&submenu->windowType()==Qt::Popup&&
                            submenu->actions().size()>1;
                        if(liveSubmenu)submenu->grab().save(workflowOutput_+"/views/tutorial-"+language+"-live-submenu.png");
                        const auto copy=explanation(actionDemo);
                        auto* sample=windowExample(actionDemo);
                        const bool openedSchematic=staysInPreview(sample)&&
                            sample->widget()->objectName()=="TutorialWindowSchematic"&&!layerDialog_->isVisible();
                        replay->click();
                        examples.append(QJsonObject{{"kind",demoTabs->tabData(i).toString()},{"result",result},
                            {"menu_open",menuOpen},{"changed",before!=after},{"visible",visible},
                            {"live_popup",showsActualMenu},{"live_submenu",liveSubmenu},
                            {"schematic_opened",openedSchematic},
                            {"shows_inline_mark",copy.contains("<img src=\"data:image/png;base64,")},
                            {"started_clear",startsWithCommand&&startsEmpty},
                            {"restarts",actionDemo->property("tutorial_action_result").toString().isEmpty()&&
                                        !actionDemo->property("tutorial_action_menu_open").toBool()},
                            {"image",resultImage}});
                    }
                    demoTabs->setCurrentIndex(5);
                    windowExplanation=explanation(windowDemo);
                    const auto windowFrame=[&](const QString& name){
                        const auto path=workflowOutput_+"/views/tutorial-"+language+"-window-"+name+".png";
                        auto* sample=windowExample(windowDemo);
                        const auto geometry=sample?sample->geometry():QRect{};
                        dialog->grab().save(path);
                        windowAnimation.append(QJsonObject{{"phase",name},{"maximized",sample&&sample->isMaximized()},
                            {"image",path},{"inside_preview",staysInPreview(sample)},
                            {"schematic",sample&&sample->widget()&&sample->widget()->objectName()=="TutorialWindowSchematic"},
                            {"x",geometry.x()},{"y",geometry.y()},{"width",geometry.width()},{"height",geometry.height()}});
                    };
                    windowFrame("normal");
                    wait(1240);windowFrame("maximized");
                    wait(1450);windowFrame("restored");
                    dialog->grab().save(workflowOutput_+"/views/tutorial-"+language+"-window.png");
                    replay->click();windowFrame("replay");
                }
                guides.append(QJsonObject{{"language",language},{"kinds",kinds},{"tabs_only_text",tabsOnlyText},{"window_animation",windowAnimation},
                    {"window_explanation",windowExplanation},{"ctrl_explanation",ctrlExplanation},
                    {"content_inside",inside},{"image",image},{"title",dialog->windowTitle()},{"icon",dialog->windowIcon().name()},
                    {"animation",animation},{"examples",examples},{"replay",replay?replay->text():QString{}},{"compact_fits",compactFits},
                    {"compact_tabs_fit",compactTabsFit},
                    {"old_hint",dialog->findChild<QLabel*>("TutorialCtrlClick")!=nullptr}});
                captured=true;dialog->reject();
            });timer.start(0);tutorial->trigger();if(!captured)throw std::runtime_error("Tutorial production callback was not observed");
        }
        workflowArtifacts_["tutorial_guides"]=guides;
        workflowCase("menu-input-preserves-document",session_.model()==original&&session_.modified()==modified);

        // Actual outstanding production request, not a synthetic busy flag.
        // This is a private startup instance with a copied model; never touches
        // the user's live windows. Suppress startup command advancement only
        // while these nested event loops observe this request's completion.
        const auto completed=session_.onCompleted;
        session_.onCompleted=[](const QString&){};
        try{
            const QString fresh=original->comment+"\nPrivate busy navigation probe";
            if(!session_.updateProject({{"comment",fresh},{"draft",original->draft*.99}}))throw std::runtime_error("Private project update rejected");
            QJsonObject navigation{{"request_busy",session_.busy()},{"project_enabled",project_->isEnabled()},{"sections_enabled",sectionsAction_->isEnabled()}};
            sectionsAction_->trigger();auto* table=sections_?sections_->findChild<QTableWidget*>():nullptr;
            navigation["sections_open_waiting"]=sections_&&sections_->isVisible()&&table&&!table->isEnabled();
            bool waiting=false,refreshed=false;QElapsedTimer elapsed;elapsed.start();QTimer poll;poll.setInterval(5);
            connect(&poll,&QTimer::timeout,this,[&]{
                if(!activeProject_)return;
                auto* editor=activeProject_->fieldEditor("comment");
                if(session_.busy())waiting=waiting||(activeProject_->property("project_waiting").toBool()&&editor&&!editor->isEnabled());
                else{
                    refreshed=!activeProject_->property("project_waiting").toBool()&&editor&&editor->isEnabled()&&
                        static_cast<QPlainTextEdit*>(activeProject_->fieldEditor("comment"))->toPlainText()==fresh&&activeProject_->changes().isEmpty();
                    activeProject_->reject();
                }
                if(elapsed.elapsed()>15000)activeProject_->reject();
            });poll.start();project_->trigger();poll.stop();
            navigation["project_open_waiting"]=waiting;navigation["project_refreshed_after_completion"]=refreshed;
            navigation["sections_ready_after_completion"]=table&&table->isEnabled();navigation["elapsed_ms"]=elapsed.elapsed();
            if(sections_)sections_->reject();
            workflowArtifacts_["busy_navigation"]=navigation;
        }catch(...){session_.onCompleted=completed;throw;}
        session_.onCompleted=completed;
    }catch(const std::exception& error){workflow_=oldWorkflow;setUiLanguage(oldLanguage);finishWorkflow(QString::fromUtf8(error.what()));return;}
    workflow_=oldWorkflow;setUiLanguage(oldLanguage);
}
