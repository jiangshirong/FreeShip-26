#pragma once
#include <QWidget>
#include <QLabel>
#include <QIcon>
#include <QColor>
#include <QVector>
#include <QPointer>
#include "message_style.h"
class QAction;
class QAbstractButton;
class QAbstractScrollArea;
class QToolButton;
class QMenu;
class QPushButton;
class QScrollArea;
class QToolBar;
class QTimer;
void applyStudioTheme();
bool confirmStudioAction(QWidget* parent,const QString& title,const QString& question);
void applyStudioScrollbars(QAbstractScrollArea* area);
QIcon studioIcon(const QString& key);
QPushButton* studioFolderButton(const QString& label);
QIcon studioVendorIcon(const QString& vendor);
QIcon studioFormatIcon(const QString& format,bool importing=false);
QIcon studioFormatWindowIcon(const QString& format,bool importing=false);
void setStudioWindowIcon(QWidget* window,const QString& key);
void setStudioFormatButton(QAbstractButton* button, const QString& format, const QString& description);
void setStudioFormatMenuAction(QMenu* menu, QAction* action, const QString& format,bool importing=false);
QString studioMenuCaption(const QString& text);
bool studioMenuOpensDialog(const QAction* action);
class StudioMessage final : public QLabel {
public:
    explicit StudioMessage(StudioMessageRole role, const QString& text = {}, QWidget* parent = nullptr);
    void setMessage(const QString& text, StudioMessageRole role, bool working = false);
    void enableMathNotation(){mathNotation_=true;}
    void setText(const QString& text);
    void clear();
    void setVisible(bool visible) override;
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    StudioMessageRole role_;
    bool working_ = false;
    bool mathNotation_ = false;
    QColor ink_;
};
// Layout and assets follow the user's existing StudioLight shell.
class StudioBar final : public QWidget {
public:
    explicit StudioBar(QWidget* parent = nullptr);
    void addGroup(const QList<QAction*>& actions, QColor background);
    QToolBar* sideBar() const { return sideBar_; }
    QList<QToolButton*> buttons() const;
    void setIconSize(int size);
    void setStartupLayoutWidth(int width);
protected:
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
private:
    void layoutButtons();
    void watchMenuButton(QToolButton* button);
    void queueHoverMenu(QToolButton* button);
    void showHoverMenu();
    void openButtonMenu(QToolButton* button);
    bool insideHoverMenu(const QPoint& global) const;
    void trackHoverMenu(const QPoint& global,bool dragging);
    QToolButton* buttonAt(const QPoint& global) const;
    QTimer* hoverMenuTimer_;
    QTimer* closeMenuTimer_;
    QPoint lastMenuPointer_;
    bool hasMenuPointer_=false,keepMenuRelease_=false;
    QPointer<QToolButton> pendingMenuButton_,openMenuButton_;
    QPointer<QMenu> openMenu_;
    int iconSize_=20;
    int startupLayoutWidth_=0;
    QToolBar* sideBar_;
    QScrollArea* sideScroll_;
    QWidget* sideContents_;
    struct Group { QWidget* top; QWidget* side; QVector<QToolButton*> buttons; };
    QVector<Group> groups_;
};
