#include "app/main_window.h"
#include "app/studio_ui.h"
#include "app/ui_language.h"
#include "app/preferences_dialog.h"
#include "app/preferences_store.h"
#include "app/studio_caption.h"
#include <QAbstractScrollArea>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStatusBar>
#include <QSpinBox>
#include <QTimer>
#include <QWheelEvent>

namespace {
class NumericWheelFilter final : public QObject {
    bool eventFilter(QObject* receiver,QEvent* event) override {
        if(event->type()!=QEvent::Wheel)return false;
        auto* widget=qobject_cast<QWidget*>(receiver);
        while(widget&&!qobject_cast<QSpinBox*>(widget)&&!qobject_cast<QDoubleSpinBox*>(widget))
            widget=widget->parentWidget();
        if(!widget)return false;

        // Route the original scroll delta to the containing view, including
        // when the wheel arrives at the spin box's internal line editor.
        const auto* wheel=static_cast<QWheelEvent*>(event);
        for(auto* parent=widget->parentWidget();parent;parent=parent->parentWidget()){
            auto* scroll=qobject_cast<QAbstractScrollArea*>(parent);
            if(!scroll)continue;
            auto* viewport=scroll->viewport();
            QWheelEvent forwarded(viewport->mapFromGlobal(wheel->globalPosition()),wheel->globalPosition(),
                wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),
                wheel->phase(),wheel->inverted(),wheel->source(),wheel->pointingDevice());
            QApplication::sendEvent(viewport,&forwarded);
            event->setAccepted(forwarded.isAccepted());
            return true;
        }
        event->ignore();
        return true;
    }
};
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    applyStudioToolTipPolicy();
    NumericWheelFilter numericWheelFilter;
    app.installEventFilter(&numericWheelFilter);
    QCoreApplication::setApplicationName("FreeShip_26 Qt"); // Stable settings identity.
    QApplication::setApplicationDisplayName("FreeShip 26");
    QCoreApplication::setOrganizationName("FreeShip26");
    auto arguments = app.arguments();
    const bool verifyDevelopment=arguments.removeAll("--development")>0;
    const bool verification=arguments.size()>1&&arguments[1].startsWith("--verify-");
    QFile manifest(app.applicationDirPath()+"/build-manifest.json");
    const auto buildInfo=manifest.open(QIODevice::ReadOnly)&&manifest.size()<=1024*1024?
        QJsonDocument::fromJson(manifest.readAll()).object():QJsonObject{};
    const bool developmentInstall=buildInfo["local_installation_role"].toString().startsWith("development;");
#ifdef Q_OS_WIN
    // Installed releases must never clear or reuse development settings on a clean install.
    if(!developmentInstall&&buildInfo["settings_profile"].toString()=="installed")
        QCoreApplication::setOrganizationName("FreeShip26Installed");
#endif
    app.setProperty("freeship_development",developmentInstall||(verification&&verifyDevelopment));
    QString languageIssue;
    // Resolve captions before constructing widgets. Probes never read user settings.
    QString settingsFile;
    const bool windowStartup=verification&&arguments[1]=="--verify-window-startup"&&
        (arguments.size()==3||arguments.size()==4);
    const bool firstLaunchProbe=verification&&arguments[1]=="--verify-first-launch"&&arguments.size()==3;
    if(!verification)settingsFile=PreferencesStore::defaultFilename();
    else if(firstLaunchProbe){
        settingsFile=QDir(arguments[2]).absoluteFilePath("preferences.json");
        if(settingsFile==PreferencesStore::defaultFilename())return 1;
    }
    else if(arguments.size()==4&&arguments[1]=="--verify-preferences"){
        QFile request(arguments[2]);
        if(request.open(QIODevice::ReadOnly)&&request.size()<1024*1024)
            settingsFile=QJsonDocument::fromJson(request.readAll()).object()["settings_file"].toString();
    }
    const bool firstRun=(!verification||firstLaunchProbe)&&!QFileInfo::exists(settingsFile);
    try{
        const auto id=firstRun?QString("en"):windowStartup&&arguments.size()==4?arguments[2]:
            settingsFile.isEmpty()?QString("zh"):PreferencesStore(settingsFile).read().value("ui_language").toString("zh");
        languageIssue=setUiLanguage(id);
        if(!languageIssue.isEmpty()){
            // Missing packs cannot prevent startup or silently overwrite the user's choice.
            if(!setUiLanguage("en").isEmpty())setUiLanguage("zh");
        }
    }catch(const std::exception& e){languageIssue=QString::fromUtf8(e.what());setUiLanguage("zh");}
    QJsonObject initialPreferences;
    if(firstRun){
        // Closing the first-run chooser means the user has not accepted the
        // setup.  Leave the preferences file untouched and end the process.
        if(!chooseInitialLanguages(nullptr,&initialPreferences))return 0;
        PreferencesStore initialStore(settingsFile);
        initialStore.save(initialPreferences);
        const auto selectedLanguage=initialPreferences.value("ui_language").toString("zh");
        if(const auto error=setUiLanguage(selectedLanguage);!error.isEmpty())languageIssue=error;
    }
    MainWindow window(nullptr,!verification);
    if(!languageIssue.isEmpty())window.statusBar()->showMessage(languageIssue);
    if(verification&&arguments[1]!="--verify-window-startup")window.setWindowState(Qt::WindowMaximized);
    if (windowStartup) {
        verifyStudioWindowStartup(&window,arguments.last());
    } else if (arguments.size() == 4 && arguments[1] == "--verify-workflow") {
        QTimer::singleShot(0, &window, [&] { window.verifyWorkflow(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-editing") {
        QTimer::singleShot(0, &window, [&] { window.verifyEditing(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-topology") {
        QTimer::singleShot(0, &window, [&] { window.verifyTopology(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-sections") {
        QTimer::singleShot(0, &window, [&] { window.verifySections(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-overlays") {
        QTimer::singleShot(0, &window, [&] { window.verifyOverlays(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-backgrounds") {
        QTimer::singleShot(0, &window, [&] { window.verifyBackgrounds(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-preferences") {
        QTimer::singleShot(0, &window, [&] { window.verifyPreferences(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-model-ui") {
        QTimer::singleShot(0, &window, [&] { window.verifyModelUi(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-viewport") {
        QTimer::singleShot(0, &window, [&] { window.verifyViewport(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-shading") {
        QTimer::singleShot(0, &window, [&] { window.verifyShading(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-startup") {
        QTimer::singleShot(0, &window, [&] { window.verifyStartup(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-display") {
        QTimer::singleShot(0, &window, [&] { window.verifyDisplay(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-auxiliary") {
        QTimer::singleShot(0, &window, [&] { window.verifyAuxiliary(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-hydro") {
        QTimer::singleShot(0, &window, [&] { window.verifyHydro(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-performance") {
        QTimer::singleShot(0, &window, [&] { window.verifyPerformance(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-plates") {
        QTimer::singleShot(0, &window, [&] { window.verifyPlates(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-exchange") {
        QTimer::singleShot(0, &window, [&] { window.verifyExchange(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-modeling") {
        QTimer::singleShot(0, &window, [&] { window.verifyModeling(arguments[2], arguments[3]); });
    } else if (arguments.size() == 4 && arguments[1] == "--verify-advanced") {
        QTimer::singleShot(0, &window, [&] { window.verifyAdvanced(arguments[2], arguments[3]); });
    } else {
        showStudioMainWindow(&window);
        if(!firstLaunchProbe)
            QTimer::singleShot(0,&window,[&]{window.startSession(arguments.size()==2?arguments[1]:QString{});});
    }
    return app.exec();
}
