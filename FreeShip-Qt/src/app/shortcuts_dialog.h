#pragma once
#include "studio_ui.h"
#include "ui_language.h"
#include "studio_read_ui.h"
#include <QAction>
#include <QDialog>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QPainterPath>
#include <QShortcut>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <array>
#include <functional>

enum class ShortcutGesture { None, Click, DoubleClick, Pan, Rotate, Wheel };
enum class ShortcutViewScope { None, All, Current, CurrentOrLines, Perspective };

inline QWidget* shortcutViewScopeIcons(ShortcutViewScope scope,const QString& description,QWidget* owner){
    auto* cell=new QWidget;
    cell->setObjectName("ShortcutViewScope");
    cell->setAccessibleName(description);
    cell->setToolTip(description);
    auto* row=new QHBoxLayout(cell);
    row->setContentsMargins(2,0,8,0);
    row->setSpacing(6);
    auto* views=new QWidget;
    auto* viewRow=new QHBoxLayout(views);
    viewRow->setContentsMargins(0,0,0,0);
    viewRow->setSpacing(6);
    const auto add=[&](QHBoxLayout* layout,const QString& icon,const QString& caption){
        auto* label=new QLabel;
        label->setObjectName("ShortcutViewIcon");
        label->setProperty("shortcut_view_icon",icon);
        label->setAccessibleName(caption);
        label->setToolTip(description+" · "+caption);
        label->setPixmap(studioIcon(icon).pixmap(QSize(20,20),owner->devicePixelRatioF()));
        layout->addWidget(label);
    };
    if(scope!=ShortcutViewScope::Perspective){
        add(viewRow,"ViewBody",uiText("横剖面"));
        add(viewRow,"ViewProfile",uiText("纵剖面"));
        add(viewRow,"ViewPlan",uiText("俯视图"));
    }
    add(viewRow,"ViewPerspective",uiText("透视图"));
    if(scope==ShortcutViewScope::All){
        views->setObjectName("ShortcutAllViews");
        views->setStyleSheet("QWidget#ShortcutAllViews{background:#e7f0ea;border-radius:4px}");
    }
    row->addWidget(views);
    if(scope==ShortcutViewScope::CurrentOrLines){
        auto* separator=new QFrame;
        separator->setFixedSize(1,17);
        separator->setStyleSheet("background:#cbded3");
        row->addWidget(separator);
        add(row,"LinesPlan",uiText("型线图"));
    }
    row->addStretch();
    return cell;
}

class ShortcutGesturePreview final : public QWidget {
public:
    ShortcutGesturePreview(ShortcutGesture gesture,const QString& label)
        : gesture_(gesture){
        setAccessibleName(label);
        setObjectName("ShortcutGesturePreview");
        setFixedSize(100,43);
    }
    void setElapsed(qint64 ms){
        phase_=(ms%1800)/1800.;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor ink("#315b55"), accent("#007e74"), fill("#cae8da");
        const bool dragging=gesture_==ShortcutGesture::Pan||gesture_==ShortcutGesture::Rotate;
        const bool right=gesture_==ShortcutGesture::Pan;
        const bool wheel=gesture_==ShortcutGesture::Wheel;
        const qreal progress=qBound(0.,(phase_-.15)/.58,1.);
        const bool active=dragging?(phase_>=.15&&phase_<.74):
            (phase_>.2&&phase_<.30)||(gesture_==ShortcutGesture::DoubleClick&&phase_>.39&&phase_<.49);
        const qreal x=dragging?22+51*progress:32;
        const qreal y=gesture_==ShortcutGesture::Rotate?8+6*std::sin(progress*3.14159):8;
        if(dragging){
            painter.setPen(QPen(QColor("#9dbdb1"),1,Qt::DashLine));
            painter.drawLine(QPointF(25,37),QPointF(88,37));
        }
        painter.setPen(QPen(ink,1.5));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(x,y,18,26),8,8);
        if(active&&!wheel){
            QPainterPath button;
            const qreal edge=right?x+18:x, middle=x+9;
            button.moveTo(middle,y+1);
            button.quadTo(edge,y+1,edge,y+9);
            button.lineTo(middle,y+9);
            painter.fillPath(button,fill);
        }
        painter.drawLine(QPointF(x+1,y+10),QPointF(x+17,y+10));
        painter.drawLine(QPointF(x+9,y+1),QPointF(x+9,y+10));
        if(wheel){
            painter.setPen(QPen(accent,2,Qt::SolidLine,Qt::RoundCap));
            const qreal offset=3*std::sin(phase_*6.28318);
            painter.drawLine(QPointF(x+9,y+4+offset),QPointF(x+9,y+8+offset));
            painter.drawLine(QPointF(69,11),QPointF(69,31));
            for(int direction:{-1,1}){
                const qreal tip=21+direction*10;
                painter.drawLine(QPointF(69,tip),QPointF(65,tip-direction*4));
                painter.drawLine(QPointF(69,tip),QPointF(73,tip-direction*4));
            }
        }else if(active&&!dragging){
            painter.setPen(QPen(accent,1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QPointF(x+4,y+4),8,8);
        }
    }
private:
    ShortcutGesture gesture_;
    qreal phase_=0;
};

inline bool showShortcutGuide(QWidget* owner,QShortcut* languageShortcut=nullptr,
                              const std::function<void()>& switchLanguage={}){
    QDialog dialog(owner);dialog.setAttribute(Qt::WA_DontShowOnScreen,owner&&owner->testAttribute(Qt::WA_DontShowOnScreen));setStudioWindowIcon(&dialog,"Shortcuts");dialog.setObjectName("ShortcutGuide");dialog.setWindowTitle(uiText("快捷方式"));
    dialog.resize(980,740);dialog.setMinimumSize(620,440);styleStudioReadDialog(&dialog);
    auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);layout->setSpacing(20);
    layout->addWidget(studioReadHeading(uiText("快捷方式"),"Shortcuts",&dialog));
    auto* table=new StudioShortcutTable(0,3);table->setObjectName("ShortcutGuideTable");
    table->setItemDelegate(new StudioWidgetCellDelegate(table));
    table->setHorizontalHeaderLabels({uiText("操作"),uiText("快捷方式","column"),uiText("作用对象")});
    auto* header=table->horizontalHeader();
    header->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    header->setStyleSheet(QString("QHeaderView::section{background:#e4eee7;color:#254c42;border:0;"
        "font-size:%1pt;font-weight:600;padding:10px 11px}").arg(qMax(12.,table->font().pointSizeF()+2)));
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setAlternatingRowColors(false);table->setShowGrid(false);table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->setItemDelegateForColumn(1,new StudioKeycapDelegate(table));
    table->setWordWrap(true);table->setTextElideMode(Qt::ElideNone);applyStudioScrollbars(table);layout->addWidget(table);
    enum Group { Model, Selection, View, Window, Text, Groups };
    struct Entry { QString description,key,scope; ShortcutGesture gesture; ShortcutViewScope viewScope; };
    std::array<QList<Entry>,Groups> entries;
    const auto add=[&](Group group,const QString& description,const QString& key,const QString& scope,
                       ShortcutGesture gesture=ShortcutGesture::None,ShortcutViewScope viewScope=ShortcutViewScope::None){
        if(key.isEmpty()||description.isEmpty())return;
        for(auto& entry:entries[group]){
            if(entry.description!=description||entry.scope!=scope||entry.gesture!=gesture||entry.viewScope!=viewScope)continue;
            if(!entry.key.split(" / ").contains(key))entry.key+=" / "+key;
            return;
        }
        entries[group].append({description,key,scope,gesture,viewScope});
    };
    const auto target=[](const QObject* command){
        const auto source=command->property("shortcut_target").toString();
        return source.isEmpty()?QString(uiText("当前工作区")):uiText(source);
    };
    for(const auto* action:owner->findChildren<QAction*>()){
        if(action->property("coordinate_axis").isValid())continue;
        const auto group=action->property("shortcut_target")=="所选对象"?Selection:Model;
        for(const auto& key:action->shortcuts())add(group,studioMenuCaption(action->text().remove('&')),key.toString(QKeySequence::NativeText),target(action));
    }
    for(const auto* shortcut:owner->findChildren<QShortcut*>()){
        auto* action=qobject_cast<QAction*>(shortcut->property("shortcut_action").value<QObject*>());
        const auto source=shortcut->property("shortcut_source").toString();
        const int axis=shortcut->property("coordinate_shortcut_axis").toInt();
        auto description=action?action->text():QString(uiText(source));
        const auto scope=target(shortcut);
        auto group=Model;
        auto viewScope=ShortcutViewScope::None;
        if(axis>=1&&axis<=3)description=QString(uiText("显示或隐藏 %1 坐标")).arg(QString("XYZ").at(axis-1));
        if(axis){group=View;viewScope=ShortcutViewScope::Current;}
        if(source=="缩放至全图"){group=View;viewScope=ShortcutViewScope::All;}
        if(source=="退出程序"||source=="切换语言 / Language")group=Window;
        for(const auto& key:shortcut->keys())add(group,description,key.toString(QKeySequence::NativeText),scope,ShortcutGesture::None,viewScope);
    }
    add(Selection,uiText("取消当前操作或清空选择"),"Esc",uiText("当前视图"));
    add(Selection,uiText("增选或取消单个对象"),uiText("Ctrl + 左键"),uiText("船模中的对象"),ShortcutGesture::Click);
    add(View,uiText("平移视图"),uiText("右键拖动"),uiText("当前视图或型线图窗口"),ShortcutGesture::Pan,ShortcutViewScope::CurrentOrLines);
    add(View,uiText("旋转视角"),uiText("空白处左键拖动"),uiText("透视图"),ShortcutGesture::Rotate,ShortcutViewScope::Perspective);
    add(View,uiText("缩放视图"),uiText("鼠标滚轮"),uiText("当前视图或型线图窗口"),ShortcutGesture::Wheel,ShortcutViewScope::CurrentOrLines);
    add(View,uiText("缩放表格"),uiText("Ctrl + 鼠标滚轮"),uiText("计算结果表格"),ShortcutGesture::Wheel);
    add(View,uiText("缩放至全图"),uiText("空白处双击左键"),uiText("当前视图"),ShortcutGesture::DoubleClick,ShortcutViewScope::Current);
    add(Window,uiText("关闭当前窗口"),"Esc",uiText("支持 Esc 关闭的窗口"));
    for(const auto& item:QList<QPair<QString,QKeySequence::StandardKey>>{
        {uiText("复制"),QKeySequence::Copy},{uiText("剪切"),QKeySequence::Cut},
        {uiText("粘贴"),QKeySequence::Paste},{uiText("全选文字"),QKeySequence::SelectAll}})
        add(Text,item.first,QKeySequence(item.second).toString(QKeySequence::NativeText),uiText("输入框内的文字"));

    const std::array<QString,Groups> headings{uiText("船模与文件"),uiText("选择与编辑"),uiText("视图操作"),uiText("窗口与语言"),uiText("文字编辑")};
    QList<QPair<int,ShortcutGesturePreview*>> previews;
    for(int group=0;group<Groups;++group){
        if(entries[group].isEmpty())continue;
        int row=table->rowCount();
        table->insertRow(row);
        table->setSpan(row,0,1,3);
        auto* heading=new QTableWidgetItem(headings[group]);
        heading->setData(Qt::UserRole,"group");
        table->setItem(row,0,heading);
        auto* groupLabel=new QLabel(headings[group]);
        groupLabel->setContentsMargins(10,5,10,5);
        groupLabel->setStyleSheet("background:#e4eee7;color:#315d50;border-top:1px solid #d2e2d8");
        auto font=table->font();
        font.setWeight(QFont::DemiBold);
        groupLabel->setFont(font);
        table->setCellWidget(row,0,groupLabel);
        for(const auto& entry:entries[group]){
            row=table->rowCount();
            table->insertRow(row);
            const QStringList cells{entry.description,entry.key,entry.scope};
            for(int col=0;col<3;++col)table->setItem(row,col,new QTableWidgetItem(cells[col]));
            if(entry.viewScope!=ShortcutViewScope::None)
                table->setCellWidget(row,2,shortcutViewScopeIcons(entry.viewScope,entry.scope,&dialog));
            if(entry.gesture==ShortcutGesture::None)continue;
            auto* gestureCell=new QWidget;
            auto* gestureLayout=new QHBoxLayout(gestureCell);
            gestureLayout->setContentsMargins(8,0,8,0);
            auto* preview=new ShortcutGesturePreview(entry.gesture,entry.key);
            gestureLayout->addWidget(preview);
            auto* label=new QLabel(entry.key);
            label->setWordWrap(true);
            gestureLayout->addWidget(label,1);
            table->setCellWidget(row,1,gestureCell);
            table->setItemDelegateForRow(row,new StudioWidgetCellDelegate(table));
            previews.append({row,preview});
        }
    }
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer timer(&dialog);
    QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{
        for(const auto& [row,preview]:previews)
            if(table->visualItemRect(table->item(row,1)).intersects(table->viewport()->rect()))
                preview->setElapsed(elapsed.elapsed());
    });
    timer.start(40);
    table->verticalHeader()->setMinimumSectionSize(36);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    const bool localLanguage=languageShortcut&&languageShortcut->isEnabled()&&switchLanguage;
    constexpr int languageChanged=2;
    if(localLanguage){
        auto* binding=new QShortcut(&dialog);
        binding->setKeys(languageShortcut->keys());
        binding->setAutoRepeat(false);
        QObject::connect(binding,&QShortcut::activated,&dialog,[&]{
            const auto before=uiLanguage();
            switchLanguage();
            if(uiLanguage()!=before)dialog.done(languageChanged);
        });
        languageShortcut->setEnabled(false);
    }
    const int result=dialog.exec();
    if(localLanguage)languageShortcut->setEnabled(true);
    return result==languageChanged;
}
