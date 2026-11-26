// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QList>
class MainWindow;
class QStackedWidget;
class QTabBar;

class DocumentWorkspace final : public QMainWindow {
public:
    explicit DocumentWorkspace(bool persistent);
    MainWindow* addDocument();
    void addDocument(MainWindow* doc);
    MainWindow* active() const;
    void activate(MainWindow* doc);
    void accepted(MainWindow* doc);
    void cancelled();
    QList<MainWindow*> documents() const;
    QTabBar* tabBar() const {return tabs_;}
protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    void closeNext();
    void refresh(MainWindow* doc);
    QTabBar* tabs_;
    QStackedWidget* pages_;
    bool persistent_, closing_=false;
};
