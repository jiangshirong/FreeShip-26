#pragma once
class QWidget;
class QToolButton;
class QSize;
class QMenu;
class QAction;
class QTabWidget;

// Approved MotionHarness profile A. Animation is visual only: QAction state,
// selection and tab changes are never delayed by the presentation layer.
namespace studio_motion {
constexpr int MenuMs=140,StateMs=120,TabMs=180;
bool enabled();
QToolButton* toolButton(QWidget* parent);
QSize verticalToolButtonSize(const QToolButton& button);
void installMenu(QMenu* menu);
void installTabs(QTabWidget* tabs);
void dismissMenuPresentations();
double checkProgress(const QMenu* menu,const QAction* action,double fallback);
double menuProgress(const QMenu* menu);
}
