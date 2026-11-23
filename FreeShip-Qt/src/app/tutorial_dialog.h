// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "studio_ui.h"
#include "ui_language.h"
#include "studio_read_ui.h"
#include "studio_motion.h"
#include "tutorial_features.h"
#include <QApplication>
#include <QBuffer>
#include <QDialog>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QPainterPath>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyleOptionMenuItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <array>
#include <functional>

namespace tutorial {
inline void box(QPainter& p,const QRectF& rect,const QColor& fill,const QColor& border,int radius=7){
    p.setPen(border);p.setBrush(fill);p.drawRoundedRect(rect,radius,radius);
}
inline void menuRow(QPainter& p,QMenu& style,const QRect& row,const QString& label,const QIcon& icon,
                    const QFont& font,bool selected=false,bool checkable=false,bool checked=false,bool submenu=false){
    QStyleOptionMenuItem option;option.initFrom(&style);option.rect=row;option.font=font;
    option.text=label;option.icon=icon;option.state|=QStyle::State_Enabled;
    if(selected)option.state|=QStyle::State_Selected;
    option.menuItemType=submenu?QStyleOptionMenuItem::SubMenu:QStyleOptionMenuItem::Normal;
    option.checkType=checkable?QStyleOptionMenuItem::NonExclusive:QStyleOptionMenuItem::NotCheckable;
    option.checked=checked;
    style.style()->drawControl(QStyle::CE_MenuItem,&option,&p,&style);
}
inline void cursor(QPainter& p,QPointF point,int age){
    if(age>=0&&age<240){
        QColor pulse("#007a69");
        const double progress=age/240.;
        pulse.setAlphaF(.8*(1-progress));p.setPen(QPen(pulse,2));p.setBrush(Qt::NoBrush);
        const double radius=studio_motion::enabled()?6+9*progress:8;
        p.drawEllipse(point,radius,radius);
    }
    p.save();p.translate(point);
    p.setPen(QPen(QColor("#143d34"),1.3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::white);
    p.drawPolygon(QPolygonF{{0,0},{0,17},{4.5,13},{7.5,19},{11,17.5},{8,11.8},{14,11.8}});p.restore();
}
inline QPixmap menuMark(int kind,qreal scale){
    const QSize size(kind==2?38:20,20);
    QPixmap result((QSizeF(size)*scale).toSize());
    result.setDevicePixelRatio(scale);
    result.fill(Qt::transparent);
    QMenu menuStyle;
    menuStyle.ensurePolished();
    QPainter p(&result);
    const auto mark=[&](int x,bool checked){
        // Render the menu's trailing gutter with the same style as a real row.
        menuRow(p,menuStyle,QRect(x-86,0,101,20),kind==1?QString(QChar(0x2026)):QString{},
                {},menuStyle.font(),false,kind==2,checked,kind==3);
    };
    if(kind==2){mark(10,true);mark(28,false);}
    else mark(10,false);
    return result;
}
inline QString markedExplanation(const QString& explanation,int kind,qreal scale){
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    const auto mark=menuMark(kind,scale);
    mark.save(&buffer,"PNG");
    const auto size=mark.deviceIndependentSize().toSize();
    const auto image=QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%3\" style=\"vertical-align:middle\"/>")
        .arg(QString::fromLatin1(encoded.toBase64())).arg(size.width()).arg(size.height());
    return explanation.toHtmlEscaped()+" "+QString(uiText("这类操作在下拉栏中用%1标示出来。")).toHtmlEscaped().arg(image);
}

class PopupCursor final : public QWidget {
public:
    explicit PopupCursor(QWidget* parent):QWidget(parent){
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    void movePointer(QPointF point,int age){
        point_=point;age_=age;
        resize(parentWidget()->size());
        raise();show();update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        tutorial::cursor(p,point_,age_);
    }
private:
    QPointF point_;
    int age_=-1;
};

class DemoPopup final : public QMenu {
public:
    explicit DemoPopup(QWidget* parent):QMenu(parent),pointer_(new PopupCursor(this)){
        setObjectName("TutorialLivePopup");
        setAttribute(Qt::WA_DontShowOnScreen,parent->window()->testAttribute(Qt::WA_DontShowOnScreen));
        studio_motion::installMenu(this);
    }
    void openAt(const QPoint& origin){
        setFont(parentWidget()->font());
        popup(parentWidget()->mapToGlobal(origin));
    }
    QPointF rowPoint(QAction* action)const{
        return parentWidget()->mapFromGlobal(mapToGlobal(actionGeometry(action).center()));
    }
    void click(QAction* action,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
        if(!isVisible()||!action)return;
        const QPoint local=actionGeometry(action).center(),global=mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,modifiers);
        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,modifiers);
        QApplication::sendEvent(this,&press);
        if(isVisible())QApplication::sendEvent(this,&release);
        if(isVisible()&&action->menu()&&!action->menu()->isVisible()){
            setActiveAction(action);
            QKeyEvent open(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
            QApplication::sendEvent(this,&open);
        }
    }
    bool paintPointer(QPointF point,int age){
        const QPointF local=point+QPointF(mapFromGlobal(parentWidget()->mapToGlobal(QPoint{})));
        if(isVisible()&&rect().contains(local.toPoint())){
            pointer_->movePointer(local,age);return true;
        }
        pointer_->hide();return false;
    }
private:
    PopupCursor* pointer_;
};

inline QWidget* demoPanel(QWidget* animation,QLabel* explanation){
    auto* panel=new QFrame;
    panel->setObjectName("TutorialDemoPanel");
    panel->setStyleSheet("QFrame#TutorialDemoPanel{background:#eef3f0;border:1px solid #d4e1da;border-radius:9px}"
                         "QLabel#TutorialDemoExplanation{color:#34554b;border-top:1px solid #d4e1da;padding:9px 0}");
    auto* layout=new QVBoxLayout(panel);
    layout->setContentsMargins(18,0,18,4);
    layout->setSpacing(0);
    layout->addWidget(animation);
    explanation->setObjectName("TutorialDemoExplanation");
    explanation->setWordWrap(true);
    explanation->setTextFormat(Qt::PlainText);
    layout->addWidget(explanation);
    return panel;
}

class ExampleSubWindow final : public QMdiSubWindow {
public:
    int titleHeight()const{return style()->pixelMetric(QStyle::PM_TitleBarHeight,nullptr,this);}
protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override{
        if(e->button()==Qt::LeftButton&&e->position().y()>=0&&e->position().y()<titleHeight()){
            if(isMaximized())showNormal();else showMaximized();
            e->accept();
            return;
        }
        QMdiSubWindow::mouseDoubleClickEvent(e);
    }
};

class ExampleWindowBody final : public QWidget {
public:
    ExampleWindowBody(){setObjectName("TutorialWindowSchematic");}
protected:
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);
        p.fillRect(rect(),QColor("#f4f8f5"));
        p.setRenderHint(QPainter::Antialiasing);
        const auto viewport=QRectF(rect());
        const QSizeF form(qMin(460.,viewport.width()*.78),qMin(232.,viewport.height()*.65));
        const QRectF panel(viewport.center()-QPointF(form.width()/2,form.height()/2),form);
        box(p,panel,Qt::white,QColor("#d4e1da"),12);
        const qreal inset=panel.width()*.09;
        for(int row=0;row<3;++row){
            const qreal top=panel.top()+panel.height()*(.14+.19*row);
            const QRectF label(panel.left()+inset,top+9,panel.width()*.24,9);
            const QRectF field(panel.left()+panel.width()*.43,top,panel.width()*.48,27);
            box(p,label,QColor("#b3cfc2"),Qt::transparent,4);
            box(p,field,QColor("#f4f8f5"),QColor("#d4e1da"),5);
        }
        const QRectF affordance(panel.right()-inset-74,panel.bottom()-43,74,24);
        box(p,affordance,QColor("#d9ebe2"),QColor("#bdd7ca"),6);
    }
};

class WindowExample final : public QGraphicsView {
public:
    explicit WindowExample(QWidget* parent):QGraphicsView(parent){
        setObjectName("TutorialWindowExample");
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setInteractive(false);
        setFocusPolicy(Qt::NoFocus);
        auto* scene=new QGraphicsScene(this);
        scene->setSceneRect(QRectF(0,0,640,470));
        setScene(scene);
        setBackgroundBrush(QColor("#eef3f0"));
        area_=new QMdiArea;
        area_->setFixedSize(sceneRect().size().toSize());
        area_->setBackground(QColor("#dae7e1"));
        scene->addWidget(area_);
        window_=new ExampleSubWindow;
        window_->setWidget(new ExampleWindowBody);
        area_->addSubWindow(window_,Qt::SubWindow|Qt::CustomizeWindowHint|Qt::WindowTitleHint|
                                   Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint);
        window_->setObjectName("TutorialExampleSubWindow");
        bindUiText(window_,"windowTitle",QStringLiteral("图层属性"));
        window_->setWindowIcon(studioIcon("Layers"));
        pointer_=new PopupCursor(area_->viewport());
        window_->hide();
    }

    void open(){
        show();
        hidePointer();
        window_->showNormal();
        QRect normal(QPoint{},QSize(416,300));
        normal.moveCenter(area_->viewport()->rect().center());
        window_->setGeometry(normal);
        area_->setActiveSubWindow(window_);
    }
    void reset(){hidePointer();window_->hide();hide();}
    void doubleClickTitle(){
        const auto point=titlePoint();
        QMouseEvent event(QEvent::MouseButtonDblClick,point,window_->mapToGlobal(point),
                          Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(window_,&event);
    }
    void showPointer(int age){
        pointer_->movePointer(window_->mapTo(area_->viewport(),titlePoint()),age);
    }
protected:
    void resizeEvent(QResizeEvent* e) override{
        QGraphicsView::resizeEvent(e);
        fitInView(sceneRect(),Qt::KeepAspectRatio);
    }
private:
    QPoint titlePoint()const{
        return {window_->width()/2,window_->titleHeight()/2};
    }
    void hidePointer(){pointer_->hide();}
    QMdiArea* area_;
    ExampleSubWindow* window_;
    PopupCursor* pointer_;
};
}

class TutorialActionDemo final : public QWidget {
public:
    explicit TutorialActionDemo(QWidget* parent):QWidget(parent),windowExample_(new tutorial::WindowExample(this)){
        setObjectName("TutorialActionDemo");setFixedHeight(212);
        windowExample_->reset();
        for(auto& menu:menus_)menu=new tutorial::DemoPopup(this);
        entries_[0]=menus_[0]->addAction(studioIcon("SelectAll"),uiText("全选点"));
        entries_[1]=menus_[1]->addAction(studioIcon("Layers"),uiText("图层属性"));
        entries_[1]->setProperty("studio_opens_dialog",true);
        entries_[2]=menus_[2]->addAction(studioIcon("Sections"),uiText("水线"));
        entries_[2]->setCheckable(true);
        submenu_=menus_[3]->addMenu(studioIcon("Resistance"),uiText("阻力计算"));
        submenu_->setObjectName("TutorialLiveSubmenu");
        submenu_->setAttribute(Qt::WA_DontShowOnScreen,window()->testAttribute(Qt::WA_DontShowOnScreen));
        submenu_->addAction(studioIcon("Resistance"),uiText("Delft 帆船阻力"))->setProperty("studio_opens_dialog",true);
        submenu_->addAction(studioIcon("Resistance"),uiText("Holtrop 海船阻力"))->setProperty("studio_opens_dialog",true);
        entries_[3]=submenu_->menuAction();
        for(int i=0;i<3;++i)connect(entries_[i],&QAction::triggered,this,[this,i]{setResult(i);});
        connect(entries_[1],&QAction::triggered,this,[this]{
            QTimer::singleShot(0,this,[this]{
                if(!isVisible()||example_!=1||!done_)return;
                windowExample_->setGeometry(rect());
                windowExample_->open();
            });
        });
        connect(submenu_,&QMenu::aboutToShow,this,[this]{setResult(3);});
        motion_.setStartValue(0);motion_.setEndValue(2450);motion_.setDuration(2450);
        connect(&motion_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){showFrame(value.toInt());});
    }
    void play(int example){
        stop();example_=example;time_=0;done_=false;
        setFixedHeight(example==1?previewHeight_:212);
        pointer_=toolbarRect().center();pointerInPopup_=false;
        entries_[2]->setChecked(true);setProperty("tutorial_action_result",QString{});
        setProperty("tutorial_action_menu_open",false);
        motion_.start();update();
    }
    void stop(){motion_.stop();submenu_->hide();for(auto* menu:menus_)menu->hide();windowExample_->reset();}
    void setPreviewHeight(int height){
        previewHeight_=height;
        if(example_==1)setFixedHeight(height);
    }
protected:
    void resizeEvent(QResizeEvent* e) override{
        QWidget::resizeEvent(e);
        windowExample_->setGeometry(rect());
    }
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto toolbar=toolbarRect();
        const QRect outcome(toolbar.right()+13,65,qMin(274,width()-toolbar.right()-24),120);
        const QColor ink("#143d34");
        tutorial::box(p,toolbar,menus_[example_]->isVisible()?QColor("#dcede6"):Qt::white,QColor("#c6dad1"));
        studioIcon(groupIcons_[example_]).paint(&p,QRect(toolbar.left()+10,toolbar.top()+8,18,18));
        p.setPen(ink);p.drawText(toolbar.adjusted(35,0,-8,0),Qt::AlignVCenter,uiText(groups_[example_]));
        if(example_==0||example_==2){
            tutorial::box(p,outcome,Qt::white,QColor("#c9dad2"));
            p.save();p.translate(outcome.center());
            p.scale(qMin(1.,(outcome.width()-24)/174.),1.);
            QPainterPath section;
            section.moveTo(-82,-30);section.cubicTo(-68,9,-49,35,0,35);section.cubicTo(49,35,68,9,82,-30);
            p.setPen(QPen(QColor("#276756"),2));p.setBrush(Qt::NoBrush);p.drawPath(section);
            if(example_==0){
                p.setPen(QPen(QColor("#87a799"),1,Qt::DashLine));
                const QPolygonF points{{-82,-30},{-57,9},{0,35},{57,9},{82,-30}};
                p.drawPolyline(points);
                p.setPen(QPen(QColor("#006c60"),1.3));p.setBrush(done_?QColor("#006c60"):Qt::white);
                for(const auto& point:points)p.drawEllipse(point,4.5,4.5);
            }else if(entries_[2]->isChecked()){
                p.setPen(QPen(QColor("#3975ad"),2));p.drawLine(QPointF(-88,0),QPointF(88,0));
            }
            p.restore();
        }
        if(!pointerInPopup_)tutorial::cursor(p,pointer_,time_>=Activate?time_-Activate:time_-Open);
    }
private:
    static constexpr int Open=280,Activate=1050,Finish=2280;
    static constexpr const char* groups_[]{"选择","图层","视图","计算"};
    static constexpr const char* groupIcons_[]{"SelectAll","Layers","Observe","Calculate"};
    static constexpr const char* results_[]{"points-selected","window-open","waterline-off","submenu-open"};
    QRect toolbarRect()const{
        const int left=qMax(14,(width()-524)/2);
        return QRect(left,24,qMin(236,(width()-40)/2),34);
    }
    void setResult(int example){done_=true;setProperty("tutorial_action_result",results_[example]);update();}
    void showFrame(int time){
        const int previous=time_;time_=time;
        auto* menu=menus_[example_];
        const QRect toolbar=toolbarRect();
        if(previous<Open&&time>=Open){
            menu->openAt(toolbar.bottomLeft()+QPoint(0,4));
            target_=menu->rowPoint(entries_[example_]);
        }
        if(previous<Activate&&time>=Activate)menu->click(entries_[example_]);
        if(previous<Finish&&time>=Finish&&menu->isVisible()){submenu_->hide();menu->hide();}
        const QPointF start=toolbar.center(),end=time<Open?start:target_;
        const auto phase=qBound(0.,double(time_-Open)/490.,1.);
        pointer_=start+(end-start)*(studio_motion::enabled()?QEasingCurve(QEasingCurve::OutCubic).valueForProgress(phase):qreal(phase>=1));
        pointerInPopup_=menu->paintPointer(pointer_,time_-Activate);
        setProperty("tutorial_action_menu_open",menu->isVisible());
        update();
    }
    std::array<tutorial::DemoPopup*,4> menus_{};
    std::array<QAction*,4> entries_{};
    QMenu* submenu_=nullptr;
    tutorial::WindowExample* windowExample_;
    QVariantAnimation motion_;
    QPointF target_,pointer_;
    bool done_=false,pointerInPopup_=false;
    int example_=0,time_=0,previewHeight_=360;
};

class TutorialMenuDemo final : public QWidget {
public:
    explicit TutorialMenuDemo(QWidget* parent):QWidget(parent),menu_(new tutorial::DemoPopup(this)){
        setObjectName("TutorialMenuDemo");
        bindUiText(this,"accessibleName",QStringLiteral("按住 Ctrl 连续点击，菜单保持展开。"));
        setFixedHeight(212);
        const std::array<const char*,3> names{"横剖线","纵剖线","水线"};
        for(size_t i=0;i<names.size();++i){
            actions_[i]=menu_->addAction(studioIcon("Sections"),uiText(names[i]));
            actions_[i]->setCheckable(true);
            connect(actions_[i],&QAction::triggered,this,[this]{++clicks_;});
        }
        playback_.setStartValue(0);playback_.setEndValue(3850);playback_.setDuration(3850);
        connect(&playback_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){showFrame(value.toInt());});
        playReset();
    }
    void play(){stop();playReset();playback_.start();}
    void stop(){playback_.stop();menu_->hide();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRect button=buttonRect();
        const QRect ctrl(button.left()-82,98,62,37);
        const QColor ink("#143d34");
        tutorial::box(p,button,menu_->isVisible()?QColor("#dcede6"):Qt::white,QColor("#c6dad1"),6);
        studioIcon("Observe").paint(&p,QRect(button.left()+10,button.top()+10,18,18));
        p.setPen(ink);p.drawText(button.adjusted(35,0,-8,0),Qt::AlignVCenter,uiText("视图"));
        const bool down=ctrlDown();
        const auto key=ctrl.translated(0,down?2:0);
        tutorial::box(p,key,down?QColor("#006c60"):Qt::white,down?QColor("#006c60"):QColor("#a8c0b2"),6);
        p.setPen(down?Qt::white:ink);p.drawText(key,Qt::AlignCenter,QStringLiteral("Ctrl"));
        if(!pointerInPopup_)tutorial::cursor(p,pointer_,time_-lastClick());
    }
private:
    static constexpr int Open=520,Hold=1000,FirstClick=1510,SecondClick=2350,Release=2850,Finish=3650;
    bool ctrlDown()const{return time_>=Hold&&time_<Release;}
    int lastClick()const{return time_>=SecondClick?SecondClick:time_>=FirstClick?FirstClick:Open;}
    QRect buttonRect()const{return QRect((width()-qMin(300,width()-120))/2+32,14,110,38);}
    void playReset(){
        for(auto* action:actions_)action->setChecked(true);
        time_=0;clicks_=0;
        showFrame(0);
    }
    void showFrame(int time){
        const int previous=time_;time_=time;
        const QRect button=buttonRect();
        if(previous<Open&&time>=Open){
            menu_->openAt(button.bottomLeft()+QPoint(0,5));
            first_=menu_->rowPoint(actions_[0]);second_=menu_->rowPoint(actions_[2]);
        }
        if(previous<FirstClick&&time>=FirstClick)menu_->click(actions_[0],Qt::ControlModifier);
        if(previous<SecondClick&&time>=SecondClick)menu_->click(actions_[2],Qt::ControlModifier);
        if(previous<Finish&&time>=Finish)menu_->hide();
        const auto move=[&](QPointF from,QPointF to,int start,int finish){
            const auto progress=qBound(0.,double(time_-start)/(finish-start),1.);
            return from+(to-from)*(studio_motion::enabled()?QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress):qreal(progress>=1));
        };
        const QPointF opening=button.center();
        pointer_=move(opening+QPointF(76,28),opening,140,Open);
        if(time_>=Hold)pointer_=move(opening,first_,Hold,FirstClick-60);
        if(time_>=SecondClick-430)pointer_=move(first_,second_,SecondClick-430,SecondClick-60);
        pointerInPopup_=menu_->paintPointer(pointer_,time_-lastClick());
        setProperty("tutorial_menu_open",menu_->isVisible());
        setProperty("tutorial_ctrl_down",ctrlDown());
        setProperty("tutorial_click_count",clicks_);
        update();
    }
    tutorial::DemoPopup* menu_;
    QVariantAnimation playback_;
    std::array<QAction*,3> actions_{};
    QPointF first_,second_,pointer_;
    bool pointerInPopup_=false;
    int time_=0,clicks_=0;
};

class TutorialWindowDemo final : public QWidget {
public:
    explicit TutorialWindowDemo(QWidget* parent):QWidget(parent),windowExample_(new tutorial::WindowExample(this)){
        setObjectName("TutorialWindowDemo");
        setFixedHeight(360);
        bindUiText(this,"accessibleName",QStringLiteral("双击标题栏可最大化窗口，再双击可还原。"));
        auto* layout=new QVBoxLayout(this);
        layout->setContentsMargins(0,0,0,0);
        layout->addWidget(windowExample_);
        playback_.setStartValue(0);
        playback_.setEndValue(3300);
        playback_.setDuration(3300);
        connect(&playback_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){showFrame(value.toInt());});
    }
    void play(){
        stop();time_=0;
        windowExample_->open();
        playback_.start();
    }
    void stop(){playback_.stop();windowExample_->reset();}
private:
    static constexpr int Maximize=830,Restore=2300;
    void showFrame(int time){
        const int previous=time_;time_=time;
        if(!windowExample_->isVisible())return;
        if((previous<Maximize&&time>=Maximize)||(previous<Restore&&time>=Restore))
            windowExample_->doubleClickTitle();
        if(time>=Maximize-300)windowExample_->showPointer(time-(time>=Restore?Restore:Maximize));
    }
    tutorial::WindowExample* windowExample_;
    QVariantAnimation playback_;
    int time_=0;
};

class TutorialGuideDialog final : public QDialog {
public:
    using QDialog::QDialog;
    std::function<void()> resizeExamples;
protected:
    void resizeEvent(QResizeEvent* e) override{
        QDialog::resizeEvent(e);
        if(resizeExamples)resizeExamples();
    }
};

inline QDialog* showTutorialGuide(QWidget* owner){
    if(auto* existing=owner?owner->findChild<QDialog*>("TutorialGuide",Qt::FindDirectChildrenOnly):nullptr){
        if(existing->isVisible()){
            existing->raise();existing->activateWindow();return existing;
        }
    }
    auto& dialog=*new TutorialGuideDialog(owner);
    dialog.setAttribute(Qt::WA_DeleteOnClose);
    setStudioWindowIcon(&dialog,"Tutorial");dialog.setObjectName("TutorialGuide");dialog.setWindowTitle(uiText("教程"));
    dialog.setAttribute(Qt::WA_DontShowOnScreen,owner&&owner->testAttribute(Qt::WA_DontShowOnScreen));
    dialog.resize(1120,740);dialog.setMinimumSize(660,500);styleStudioReadDialog(&dialog);
    auto face=dialog.font();face.setPointSizeF(qMax(10.5,face.pointSizeF()));dialog.setFont(face);
    auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(28,24,28,24);layout->setSpacing(24);
    auto* sections=new QTabWidget(&dialog);
    sections->setObjectName("TutorialSections");
    layout->addWidget(sections,1);
    auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);applyStudioScrollbars(scroll);
    auto* content=new QWidget;auto* body=new QVBoxLayout(content);body->setContentsMargins(0,0,0,0);body->setSpacing(20);scroll->setWidget(content);
    sections->addTab(scroll,uiText("菜单与窗口"));
    sections->addTab(createFeatureTutorial(sections),uiText("新功能"));
    auto* gallery=new QVBoxLayout;gallery->setSpacing(8);body->addLayout(gallery);
    auto* tabs=new QTabBar;tabs->setObjectName("TutorialDemoTabs");tabs->setExpanding(false);
    tabs->setStyleSheet("QTabBar#TutorialDemoTabs::tab{padding:9px 9px;margin-right:2px}");
    const std::array<const char*,6> captions{"命令项","对话框项","切换项","子菜单项","连续操作","窗口操作"};
    const auto kinds=QStringList{"command","dialog","toggle","submenu","ctrl","window"};
    for(int i=0;i<int(captions.size());++i)tabs->setTabData(tabs->addTab(uiText(captions[i])),kinds[i]);
    gallery->addWidget(tabs);
    auto* stage=new QStackedWidget;
    auto* actionExplanation=new QLabel;
    actionExplanation->setProperty("ui_user_text",true);
    auto* actionDemo=new TutorialActionDemo(stage);auto* actionPanel=tutorial::demoPanel(actionDemo,actionExplanation);stage->addWidget(actionPanel);
    actionExplanation->setTextFormat(Qt::RichText);
    auto* ctrlDemo=new TutorialMenuDemo(stage);
    auto* ctrlPanel=tutorial::demoPanel(ctrlDemo,new QLabel(uiText("按住 Ctrl 连续点击，菜单保持展开。")));stage->addWidget(ctrlPanel);
    auto* windowDemo=new TutorialWindowDemo(stage);
    auto* windowPanel=tutorial::demoPanel(windowDemo,new QLabel(uiText("双击标题栏可最大化窗口，再双击可还原。")));stage->addWidget(windowPanel);
    dialog.resizeExamples=[&dialog,actionDemo,windowDemo]{
        const int height=qBound(236,dialog.height()-264,420);
        actionDemo->setPreviewHeight(height);
        windowDemo->setFixedHeight(height);
    };
    dialog.resizeExamples();
    const std::array<const char*,4> descriptions{"点击后立即执行命令。","点击后打开对话框。",
        "点击可切换状态。对勾表示开启，空心圆表示关闭。","点击或悬停可展开子菜单。"};
    const auto refreshExplanation=[=]{
        const int index=tabs->currentIndex();
        if(index<0||index>=int(descriptions.size()))return;
        actionExplanation->setText(tutorial::markedExplanation(uiText(descriptions[index]),index,actionExplanation->devicePixelRatioF()));
        actionDemo->setAccessibleName(uiText(captions[index]));
    };
    refreshExplanation();
    bindUiRefresh(tabs,[=]{
        for(int i=0;i<int(captions.size());++i)tabs->setTabText(i,uiText(captions[i]));
        refreshExplanation();
        actionDemo->update();
        ctrlDemo->update();
        windowDemo->update();
    });
    gallery->addWidget(stage);
    auto* replay=new QPushButton(uiText("重播演示"));replay->setObjectName("TutorialMenuReplay");
    gallery->addWidget(replay,0,Qt::AlignRight);
    const auto play=[=]{
        actionDemo->stop();ctrlDemo->stop();windowDemo->stop();
        const int index=tabs->currentIndex();
        if(index==4){stage->setCurrentWidget(ctrlPanel);ctrlDemo->play();}
        else if(index==5){stage->setCurrentWidget(windowPanel);windowDemo->play();}
        else{
            refreshExplanation();
            stage->setCurrentWidget(actionPanel);actionDemo->play(index);
        }
    };
    QObject::connect(replay,&QPushButton::clicked,stage,play);
    QObject::connect(tabs,&QTabBar::currentChanged,stage,play);
    QObject::connect(sections,&QTabWidget::currentChanged,stage,[=](int index){
        if(index==0)play();
        else{actionDemo->stop();ctrlDemo->stop();windowDemo->stop();}
    });
    QTimer::singleShot(0,stage,play);
    body->addStretch();
    dialog.setWindowModality(Qt::NonModal);
    dialog.show();
    return &dialog;
}
