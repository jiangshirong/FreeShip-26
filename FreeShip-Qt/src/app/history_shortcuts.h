#pragma once
#include <QAction>
#include <QShortcut>
#include <QVariant>
#include <QWidget>

// Explicit window scope keeps document history available in modeless editors
// without taking over unrelated dialogs or native text-editor undo.
inline QShortcut* bindHistoryShortcut(QWidget* window, QAction* action,
                                     const QKeySequence& key) {
    auto* shortcut = new QShortcut(key, window);
    shortcut->setContext(Qt::WindowShortcut);
    shortcut->setProperty("shortcut_action", QVariant::fromValue<QObject*>(action));
    shortcut->setProperty("shortcut_target","当前船模");
    shortcut->setEnabled(action->isEnabled());
    QObject::connect(shortcut, &QShortcut::activated, action, &QAction::trigger);
    QObject::connect(action, &QAction::changed, shortcut, [action, shortcut] {
        shortcut->setEnabled(action->isEnabled());
    });
    return shortcut;
}
