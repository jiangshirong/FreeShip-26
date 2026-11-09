#include "app/main_window.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("FreeShip_26 Qt");
    QCoreApplication::setOrganizationName("FreeShip26");
    const auto arguments = app.arguments();
    const bool verification=arguments.size()>1&&arguments[1].startsWith("--verify-");
    MainWindow window(nullptr,!verification);
    if (arguments.size() == 4 && arguments[1] == "--verify-workflow") {
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
        window.show();
        QTimer::singleShot(0,&window,[&]{window.startSession(arguments.size()==2?arguments[1]:QString{});});
    }
    return app.exec();
}
