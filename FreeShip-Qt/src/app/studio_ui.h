#pragma once
#include <QWidget>
#include <QIcon>
#include <QColor>
#include <QVector>
class QAction;
class QToolButton;
class QMenu;
void applyStudioTheme();
QIcon studioIcon(const QString& key);
// Layout and assets follow the user's existing StudioLight shell.
class StudioBar final : public QWidget {
public:
    explicit StudioBar(QWidget* parent = nullptr);
    void addGroup(const QList<QAction*>& actions, QColor background, bool views = false);
    void setAbout(QAction* action);
    void setIconSize(int size);
protected:
    void resizeEvent(QResizeEvent*) override;
private:
    void layoutButtons();
    int iconSize_=20;
    struct Group { QWidget* panel; QVector<QToolButton*> buttons; bool views; };
    QVector<Group> groups_;
    QToolButton* more_;
    QToolButton* about_ = nullptr;
    QMenu* overflow_;
};
