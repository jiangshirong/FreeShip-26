#include "studio_motion.h"
#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QStyleOptionToolButton>
#include <QTextBoundaryFinder>
#include <QVariantAnimation>
#include <cmath>
#include <functional>
#include <algorithm>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace studio_motion {
namespace {
double clamp(double x){return std::max(0.,std::min(1.,x));}
constexpr int VerticalButtonHorizontalPadding=8,VerticalButtonVerticalPadding=5,VerticalButtonIconGap=6;
constexpr int BadgeDiameter=9,BadgeRightInset=5,BadgeVerticalInset=4;

bool isVertical(const QToolButton& button){return button.property("studio_vertical_toolbar_label").toBool();}
int verticalBadgeReserve(const QToolButton& button){
    // Keep space for the update dot even when idle, so its arrival never moves the label.
    return button.defaultAction()&&button.defaultAction()->property("studio_update_badge").toBool()?
        BadgeDiameter+BadgeVerticalInset:0;
}

struct VerticalGlyph {
    QString text;
    QRectF bounds;
    bool rotated=false;
    qreal nudge=0;
};
struct VerticalLabel {
    QList<VerticalGlyph> glyphs;
    QSizeF size;
    qreal step=0;
};

VerticalLabel verticalLabel(const QString& text,const QFontMetricsF& metrics){
    const auto codePoints=text.toUcs4();
    const bool eastAsian=std::any_of(codePoints.begin(),codePoints.end(),[](char32_t codePoint){
        const auto script=QChar::script(codePoint);
        return script==QChar::Script_Han||script==QChar::Script_Hiragana||script==QChar::Script_Katakana||script==QChar::Script_Hangul;
    });
    const auto bounds=[&](const QString& line){
        return metrics.boundingRect(line).united(QRectF(0,-metrics.ascent(),metrics.horizontalAdvance(line),metrics.height()));
    };
    VerticalLabel result;
    if(!eastAsian){
        // Shape an entire word together, preserving bearings at both ends.
        const auto line=bounds(text);
        result.glyphs.append({text,line,true});
        result.size={line.height(),line.width()};
        result.step=line.width();
        return result;
    }
    QTextBoundaryFinder characters(QTextBoundaryFinder::Grapheme,text);
    qsizetype start=0;
    qreal width=metrics.height();
    result.step=metrics.height();
    for(qsizetype end=characters.toNextBoundary();end>=0;end=characters.toNextBoundary()){
        const auto character=text.mid(start,end-start);
        const bool rotated=character==QStringLiteral("ー");
        const auto line=bounds(character);
        const qreal nudge=QStringView(u"ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ").contains(character)?metrics.height()*.1:0;
        result.glyphs.append({character,line,rotated,nudge});
        width=std::max(width,(rotated?line.height():line.width())+2*nudge);
        result.step=std::max(result.step,(rotated?line.width():line.height())+2*nudge);
        start=end;
    }
    result.size={width,result.glyphs.size()*result.step};
    return result;
}

void paintVerticalLabel(QPainter& painter,const QStyleOptionToolButton& option,int badgeReserve){
    const auto label=verticalLabel(option.text,QFontMetricsF(painter.font(),painter.device()));
    const bool enabled=option.state.testFlag(QStyle::State_Enabled);
    const auto iconMode=!enabled?QIcon::Disabled:option.state.testFlag(QStyle::State_MouseOver)?QIcon::Active:QIcon::Normal;
    const auto iconState=option.state.testFlag(QStyle::State_On)?QIcon::On:QIcon::Off;
    const qreal contentWidth=option.iconSize.width()+VerticalButtonIconGap+label.size.width();
    const QRectF content=QRectF(option.rect).adjusted(0,0,0,-badgeReserve);
    const int x=qRound(content.center().x()-contentWidth/2);
    const QRect iconRect(x,qRound(content.center().y()-option.iconSize.height()/2.0),option.iconSize.width(),option.iconSize.height());
    option.icon.paint(&painter,iconRect,Qt::AlignCenter,iconMode,iconState);
    painter.setPen(option.palette.color(enabled?QPalette::Active:QPalette::Disabled,QPalette::ButtonText));
    QPointF center(x+option.iconSize.width()+VerticalButtonIconGap+label.size.width()/2,
                   content.center().y()-(label.size.height()-label.step)/2);
    for(const auto& glyph:label.glyphs){
        painter.save();
        painter.translate(center+QPointF(glyph.nudge,-glyph.nudge));
        if(glyph.rotated)painter.rotate(90);
        painter.drawText(-glyph.bounds.center(),glyph.text);
        painter.restore();
        center.ry()+=label.step;
    }
}
// QAbstractAnimation uses Qt's shared animation driver. Retargets preserve the
// currently displayed value and velocity; there is no queue or extra idle timer.
class Spring final:public QAbstractAnimation {
public:
    explicit Spring(QObject* parent,std::function<void(double)> update):QAbstractAnimation(parent),update_(std::move(update)){}
    int duration()const override{return duration_;}
    double value()const{return value_;}
    double target()const{return target_;}
    void reset(double value){stop();from_=target_=value_=value;velocity_=fromVelocity_=0;update_(value_);}
    void to(double target,int ms){
        if(target==target_&&state()==Running)return;
        const double velocity=velocity_,value=value_;stop();
        from_=value;fromVelocity_=velocity;target_=target;duration_=ms;
        if(!enabled()||(std::abs(from_-target_)<.00001&&std::abs(velocity)<.00001)){reset(target);return;}
        start();
    }
protected:
    void updateCurrentTime(int ms)override{
        if(ms>=duration_){value_=target_;velocity_=0;}
        else{
            const double t=double(ms)/duration_,delta=from_-target_;
            const double carry=fromVelocity_*duration_+9*delta,decay=std::exp(-9*t);
            value_=target_+(delta+carry*t)*decay;
            velocity_=(carry-9*(delta+carry*t))*decay/duration_;
        }
        update_(value_);
    }
private:
    std::function<void(double)> update_;
    double from_=0,target_=0,value_=0,velocity_=0,fromVelocity_=0;
    int duration_=StateMs;
};

class MotionButton final:public QToolButton {
public:
    explicit MotionButton(QWidget* parent):QToolButton(parent),checked_(this,[this](double p){setProperty("studio_motion_checked",p);update();}),hover_(this,[this](double p){hoverProgress_=clamp(p);update();}),badge_(this){
        setProperty("studio_motion_button",true);
        connect(this,&QToolButton::toggled,this,[this](bool){sync();});
        connect(this,&QToolButton::pressed,this,[this]{sync();});
        connect(this,&QToolButton::released,this,[this]{sync();});
        badge_.setDuration(2300);
        badge_.setLoopCount(-1);
        badge_.setStartValue(0.0);
        badge_.setEndValue(1.0);
        connect(&badge_,&QVariantAnimation::valueChanged,this,[this]{update(badgeRect().toAlignedRect());});
    }
    void setHovered(bool hovered){hover_.to(hovered?1:0,StateMs);}
protected:
    bool event(QEvent* event)override{
        const bool result=QToolButton::event(event);
        if(!property("studio_hover_popup").toBool()){
            if(event->type()==QEvent::Enter)setHovered(true);
            if(event->type()==QEvent::Leave)setHovered(false);
        }
        if(event->type()==QEvent::Hide){hover_.reset(0);sync(true);}
        if(event->type()==QEvent::Show)sync(true);
        if(event->type()==QEvent::ActionChanged||event->type()==QEvent::Show||event->type()==QEvent::Hide)syncBadge();
        return result;
    }
    void paintEvent(QPaintEvent*)override{
        sync();
        syncBadge();
        {
            QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
            const auto paint=[&](QColor fill,QColor border,double opacity){
                if(opacity<=.0001)return;p.setOpacity(opacity*(isEnabled()?1:.35));
                p.setPen(border);p.setBrush(fill);p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),2,2);
            };
            paint(QColor("#e7f2fc"),QColor("#abd5f5"),hoverProgress_);
            paint(QColor("#cce8ff"),QColor("#8dcaff"),clamp(checked_.value()));
        }
        // Paint only the native label. Painting the whole tool button here can
        // cover the interpolated surface with the stylesheet's final checked
        // fill, making an active animation appear to jump instantly.
        QStyleOptionToolButton option;initStyleOption(&option);QPainter label(this);
        label.setFont(font());
        label.save();
        if(isVertical(*this))paintVerticalLabel(label,option,verticalBadgeReserve(*this));
        else style()->drawControl(QStyle::CE_ToolButtonLabel,&option,&label,this);
        label.restore();
        if(hasUpdate()){
            const auto opacity=badge_.state()==QAbstractAnimation::Running?
                .72+.28*std::cos(6.283185307179586*badge_.currentValue().toDouble()):1.;
            label.setRenderHint(QPainter::Antialiasing);
            label.setOpacity(opacity);
            label.setPen(Qt::NoPen);label.setBrush(QColor("#25806b"));
            label.drawEllipse(badgeRect());
        }
    }
private:
    bool hasUpdate()const{return defaultAction()&&defaultAction()->property("update_available").toBool();}
    QRectF badgeRect()const{
        const int y=isVertical(*this)?height()-BadgeVerticalInset-BadgeDiameter:BadgeVerticalInset;
        return QRectF(width()-BadgeRightInset-BadgeDiameter,y,BadgeDiameter,BadgeDiameter);
    }
    void syncBadge(){
        const bool animate=isVisible()&&hasUpdate()&&enabled();
        if(animate&&badge_.state()!=QAbstractAnimation::Running)badge_.start();
        else if(!animate)badge_.stop();
    }
    void sync(bool snap=false){
        auto* popup=property("studio_hover_popup").toBool()&&defaultAction()?defaultAction()->menu():nullptr;
        const bool target=isChecked()||(popup?popup->isVisible():isDown());
        if(target==checkedTarget_&&!snap)return;
        checkedTarget_=target;
        if(snap||!isVisible())checked_.reset(target?1:0);else checked_.to(target?1:0,StateMs);
    }
    Spring checked_,hover_;
    QVariantAnimation badge_;
    double hoverProgress_=0;
    bool checkedTarget_=false;
};

QList<QPointer<QWidget>>& exitPresentations(){static QList<QPointer<QWidget>> windows;return windows;}
class MenuGhost final:public QWidget {
public:
    MenuGhost(QWidget* canvas,const QPixmap& pixels,const QRect& geometry,double start):
        QWidget(canvas),pixels_(pixels),fade_(this,[this](double p){progress_=clamp(p);setProperty("studio_motion_progress",progress_);update();}){
        setObjectName("StudioMenuExitPresentation");setAttribute(Qt::WA_TransparentForMouseEvents);
        if(qApp->property("studio_motion_probe").toBool())setProperty("studio_motion_source_pixels",pixels_);
        exitPresentations().append(this);
        // A child presentation cannot become another native popup or disturb
        // QMenu's mouse grab/activation. The parent clips any outside portion.
        setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::NoFocus);
        setGeometry(QRect(canvas->mapFromGlobal(geometry.topLeft()),geometry.size()));fade_.reset(start);
        connect(&fade_,&QAbstractAnimation::finished,this,&QObject::deleteLater);
    }
    void begin(){show();raise();fade_.to(0,MenuMs);}
    double progress()const{return progress_;}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setClipRect(QRectF(0,0,width(),height()*progress_));
        p.setOpacity(progress_);p.drawPixmap(QPointF(),pixels_);
    }
private:
    QPixmap pixels_;
    Spring fade_;
    double progress_=0;
};

class MenuMotion final:public QObject {
public:
    explicit MenuMotion(QMenu* menu):QObject(menu),menu_(menu),reveal_(this,[this](double p){apply(p);}){
        menu->setProperty("studio_motion_menu",true);menu->setProperty("studio_motion_surface","paint-only");menu->installEventFilter(this);
        connect(menu,&QMenu::aboutToShow,this,[this]{
            const auto generation=++revealGeneration_;
            refreshChecks();
            double from=0;
            if(ghost_){from=ghost_->progress();delete ghost_;ghost_=nullptr;}
            // A retiring popup must never cover the new menu/dialog's text.
            dismissMenuPresentations();
            opening_=enabled();
            if(opening_){
                // Never animate native HWND opacity or its input/visible region.
                // The style paints the entrance inside an ordinary complete menu.
                reveal_.reset(from);
                QTimer::singleShot(0,this,[this]{startReveal();});
                QTimer::singleShot(MenuMs+80,this,[this,generation]{
                    if(generation==revealGeneration_&&menu_&&menu_->isVisible()){opening_=false;reveal_.reset(1);queueCapture();}
                });
            }else reveal_.reset(1);
        });
        connect(menu,&QMenu::aboutToHide,this,[this]{leave();});
        connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){if(state!=Qt::ApplicationActive&&!qApp->property("studio_motion_probe").toBool()&&ghost_){delete ghost_;ghost_=nullptr;}});
        connect(&reveal_,&QAbstractAnimation::finished,this,[this]{queueCapture();});
        refreshChecks();
    }
    ~MenuMotion()override{delete ghost_;}
    double check(const QAction* action,double fallback){
        if(!enabled()||!menu_||!menu_->isVisible())return fallback;
        const auto it=checks_.constFind(const_cast<QAction*>(action));
        if(it==checks_.cend())return fallback;
        // Controllers can legitimately refresh QAction state under a signal
        // blocker. Reconcile that state without waiting for another popup.
        if(it.value()->target()!=fallback)it.value()->to(fallback,StateMs);
        return clamp(it.value()->value());
    }
protected:
    bool eventFilter(QObject*,QEvent* event)override{
        if(event->type()==QEvent::Show&&opening_){
            // Qt has now positioned/sized its popup. No animated HWND movement.
            QTimer::singleShot(0,this,[this]{startReveal();});
        }else if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress){
            // User input can immediately reach any real menu row while opening.
            if(reveal_.state()==QAbstractAnimation::Running)reveal_.reset(1);
            // Capture at an input boundary, never recursively on every Paint.
            if(menu_&&menu_->isVisible())cached_=menu_->grab();
        }else if(event->type()==QEvent::ActionAdded)QTimer::singleShot(0,this,[this]{refreshChecks();});
        return false;
    }
private:
    void startReveal(){if(menu_&&menu_->isVisible()&&opening_){opening_=false;reveal_.to(1,MenuMs);queueCapture();}}
    void queueCapture(){
        if(capturing_||captureQueued_||!menu_||!menu_->isVisible())return;
        captureQueued_=true;
        QTimer::singleShot(0,this,[this]{
            captureQueued_=false;if(!menu_||!menu_->isVisible())return;
            capturing_=true;
            cached_=menu_->grab();capturing_=false;
        });
    }
    void apply(double p){
        if(!menu_)return;p=clamp(p);menu_->setProperty("studio_motion_progress",p);
        if(menu_->isVisible())menu_->update();
    }
    void refreshChecks(){
        if(!menu_)return;
        for(auto* action:menu_->actions()){
            if(action->menu())installMenu(action->menu());
            if(!action->isCheckable())continue;
            if(!checks_.contains(action)){
                QPointer<QAction> guard(action);
                auto* tween=new Spring(this,[this](double){if(menu_&&menu_->isVisible())menu_->update();});
                checks_.insert(action,tween);tween->reset(action->isChecked()?1:0);
                connect(action,&QAction::toggled,this,[this,guard,tween](bool checked){
                    if(!guard)return;if(menu_->isVisible())tween->to(checked?1:0,StateMs);else tween->reset(checked?1:0);
                });
                connect(action,&QObject::destroyed,this,[this,action]{delete checks_.take(action);});
            }else if(!menu_->isVisible())checks_[action]->reset(action->isChecked()?1:0);
        }
    }
    void leave(){
        if(!menu_)return;
        ++revealGeneration_;
        const double from=opening_?reveal_.value():(reveal_.state()==QAbstractAnimation::Running?reveal_.value():1);
        reveal_.stop();opening_=false;
        QWidget* owner=menu_->parentWidget();while(owner&&qobject_cast<QMenu*>(owner))owner=owner->parentWidget();
        const bool active=qApp->applicationState()==Qt::ApplicationActive||qApp->property("studio_motion_probe").toBool();
        if(enabled()&&active&&owner&&owner->isVisible()&&from>.002&&!cached_.isNull()&&!QCoreApplication::closingDown()){
            const auto size=menu_->size();
            // Capture once at native DPR. The passive exit cannot grab input,
            // change QAction state, delay an action, or stretch text.
            // QMenu is already hidden inside aboutToHide on Windows. Retain
            // its last completed paint instead of trying to render a hidden HWND.
            delete ghost_;ghost_=new MenuGhost(owner->window(),cached_,QRect(menu_->pos(),size),from);ghost_->begin();
        }
    }
    QPointer<QMenu> menu_;
    QPointer<MenuGhost> ghost_;
    Spring reveal_;
    QHash<QAction*,Spring*> checks_;
    QPixmap cached_;
    quint64 revealGeneration_=0;
    bool opening_=false,capturing_=false,captureQueued_=false;
};

class TabIndicator final:public QWidget {
public:
    explicit TabIndicator(QTabBar* bar):QWidget(bar),bar_(bar),position_(this,[this](double p){positionValue_=p;update();}){
        setObjectName("StudioTabSelectionMotion");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);resize(bar->size());show();
    }
    void select(int index,bool animate){
        if(!bar_)return;
        const auto target=bar_->tabRect(index);if(!target.isValid())return;
        if(!initialized_||!animate){position_.reset(target.left());initialized_=true;}else position_.to(target.left(),TabMs);
        width_=target.width();setGeometry(bar_->rect());raise();update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        if(!initialized_)return;QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(QColor("#007e74"));
        p.drawRoundedRect(QRectF(positionValue_+12,bar_?bar_->tabRect(bar_->currentIndex()).bottom()-9:height()-11,std::max(10,width_-24),2.5),1.25,1.25);
    }
private:
    QPointer<QTabBar> bar_;
    Spring position_;
    double positionValue_=0;
    int width_=0;
    bool initialized_=false;
};

class TabPresentation final:public QWidget {
public:
    explicit TabPresentation(QWidget* parent):QWidget(parent),phase_(this,[this](double p){phaseValue_=clamp(p);setProperty("studio_motion_phase",phaseValue_);update();}){
        setObjectName("StudioTabContentMotion");setAttribute(Qt::WA_TransparentForMouseEvents);setFocusPolicy(Qt::NoFocus);
        connect(&phase_,&QAbstractAnimation::finished,this,[this]{hide();old_=QPixmap();fresh_=QPixmap();});
    }
    void begin(QPixmap old,QPixmap fresh,QColor paper){
        old_=std::move(old);fresh_=std::move(fresh);paper_=paper;phase_.reset(0);setGeometry(parentWidget()->rect());show();raise();phase_.to(1,TabMs);
    }
    void finish(){phase_.stop();hide();old_=QPixmap();fresh_=QPixmap();}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),paper_);
        // One text layer at a time, matching the approved sample's fade-through.
        const bool old=phaseValue_<.4;const double a=old?1-phaseValue_/.4:(phaseValue_-.4)/.6;
        p.setOpacity(clamp(a));p.drawPixmap(QPointF(0,old?-4*(1-a):4*(1-a)),old?old_:fresh_);
    }
private:
    QPixmap old_,fresh_;
    QColor paper_;
    Spring phase_;
    double phaseValue_=0;
};

class TabMotion final:public QObject {
public:
    explicit TabMotion(QTabWidget* tabs):QObject(tabs),tabs_(tabs),bar_(tabs->tabBar()),previous_(tabs->currentWidget()),indicator_(new TabIndicator(tabs->tabBar())){
        tabs->setProperty("studio_motion_tabs",true);tabs->installEventFilter(this);tabs->tabBar()->installEventFilter(this);
        tabs->setProperty("studio_tab_navigation","flat");
        bar_->setProperty("studio_tab_motion",true);
        stack_=tabs->findChild<QStackedWidget*>();if(stack_)stack_->installEventFilter(this);
        connect(tabs,&QTabWidget::currentChanged,this,[this](int index){change(index);});
        indicator_->select(tabs->currentIndex(),false);
    }
    ~TabMotion()override{if(qApp)qApp->removeEventFilter(this);}
protected:
    bool eventFilter(QObject* watched,QEvent* event)override{
        if(!tabs_||!bar_||!indicator_)return false;
        if((event->type()==QEvent::Resize||event->type()==QEvent::Hide)&&
           (watched==tabs_||watched==stack_||watched==bar_)){
            if(presentation_)presentation_->finish();
            indicator_->select(bar_->currentIndex(),false);
        }
        if(presentation_&&presentation_->isVisible()&&
           (event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress||event->type()==QEvent::Wheel)){
            const auto* widget=qobject_cast<QWidget*>(watched);
            if(widget&&stack_&&(widget==stack_||stack_->isAncestorOf(widget)))presentation_->finish();
        }
        return false;
    }
private:
    void change(int index){
        if(!tabs_||!bar_||!indicator_)return;
        QPointer<QWidget> old=previous_,fresh=tabs_->widget(index);previous_=fresh;
        indicator_->select(index,enabled()&&tabs_->isVisible());
        if(!enabled()&&presentation_)presentation_->finish();
        if(!old||!fresh||old==fresh||!tabs_->isVisible()||!enabled()||!stack_)return;
        // A large/live surface keeps its live renderer. Its tab indicator still
        // animates, without capturing GPU data or a whole table on every frame.
        const qreal dpr=stack_->devicePixelRatioF();
        const auto live=[](QWidget* page){
            for(auto* view:page->findChildren<QAbstractItemView*>())
                if(view->window()->windowType()!=Qt::Popup&&view->window()->windowType()!=Qt::ToolTip)return true;
            for(auto* child:page->findChildren<QWidget*>())if(child->inherits("QOpenGLWidget"))return true;
            return false;
        };
        if(stack_->width()*stack_->height()*dpr*dpr>2200000||live(fresh)||live(old)){
            if(presentation_)presentation_->finish();tabs_->setProperty("studio_motion_content","live-surface");return;
        }
        QPixmap before=(presentation_&&presentation_->isVisible())?presentation_->grab():old->grab();
        if(presentation_)presentation_->finish();
        if(!presentation_){presentation_=new TabPresentation(stack_);qApp->installEventFilter(this);}
        const QPixmap after=fresh->grab();
        if(before.size()!=after.size()||before.devicePixelRatio()!=after.devicePixelRatio())return;
        tabs_->setProperty("studio_motion_content","native-dpr");
        presentation_->begin(std::move(before),after,fresh->palette().color(QPalette::Window));
    }
    QPointer<QTabWidget> tabs_;
    QPointer<QTabBar> bar_;
    QPointer<QStackedWidget> stack_;
    QPointer<QWidget> previous_;
    QPointer<TabPresentation> presentation_;
    QPointer<TabIndicator> indicator_;
};

QHash<const QMenu*,QPointer<MenuMotion>>& menuMotions(){static QHash<const QMenu*,QPointer<MenuMotion>> map;return map;}
}

bool enabled(){
    const auto override=qApp->property("studio_motion_enabled");if(override.isValid())return override.toBool();
#ifdef Q_OS_WIN
    BOOL animate=TRUE;if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0))return animate;
#endif
    return true;
}
QToolButton* toolButton(QWidget* parent){return new MotionButton(parent);}
void setMenuButtonHovered(QToolButton* button,bool hovered){
    if(auto* motion=dynamic_cast<MotionButton*>(button))motion->setHovered(hovered);
}
QSize verticalToolButtonSize(const QToolButton& button){
    const auto label=verticalLabel(button.text(),QFontMetricsF(button.font(),&button));
    const auto icon=button.iconSize();
    return {icon.width()+VerticalButtonIconGap+int(std::ceil(label.size.width()))+2*VerticalButtonHorizontalPadding,
            int(std::ceil(std::max(qreal(icon.height()),label.size.height())))+2*VerticalButtonVerticalPadding+verticalBadgeReserve(button)};
}
void installMenu(QMenu* menu){
    if(!menu||menuMotions().contains(menu))return;
    auto* motion=new MenuMotion(menu);menuMotions().insert(menu,motion);
    QObject::connect(menu,&QObject::destroyed,qApp,[menu]{menuMotions().remove(menu);});
}
void installTabs(QTabWidget* tabs){if(tabs&&!tabs->property("studio_motion_tabs").toBool())new TabMotion(tabs);}
void dismissMenuPresentations(){
    const auto windows=exitPresentations();exitPresentations().clear();
    for(const auto& window:windows)if(window)delete window.data();
}
double checkProgress(const QMenu* menu,const QAction* action,double fallback){
    const auto it=menuMotions().constFind(menu);return it!=menuMotions().cend()&&it.value()?it.value()->check(action,fallback):fallback;
}
double menuProgress(const QMenu* menu){
    if(!menu||!enabled())return 1.;
    const auto progress=menu->property("studio_motion_progress");
    return progress.isValid()?clamp(progress.toDouble()):1.;
}
}
