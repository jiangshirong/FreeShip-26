// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QJsonObject>
#include <functional>
class QLabel;
class StudioMessage;
class QPushButton;
class QTabWidget;
class QTextBrowser;
class PreferencesDialog final : public QDialog {
public:
    explicit PreferencesDialog(QJsonObject values,QWidget* parent=nullptr,const QString& solverDefault={});
    QJsonObject values()const{return draft_;}
    void setBusy(bool busy);
    void setLanguageValue(const QString& id);
    void showFailure(const QString& message);
    void reject() override;
    std::function<void(const QJsonObject&)> onApply;
private:
    void buildGraphics();
    void refreshGraphics();
    void importLegacy();
    QJsonObject draft_;
    QTabWidget* tabs_;
    QWidget* graphics_=nullptr;
    StudioMessage* error_;
    QTextBrowser* importSummary_;
    QPushButton *ok_,*cancel_,*import_;
    bool busy_=false;
};

bool chooseInitialLanguages(QWidget* parent,QJsonObject* values);
