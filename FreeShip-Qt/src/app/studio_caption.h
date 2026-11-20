// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>

// Only the main window keeps an enabled minimize control. Other
// application dialogs use a native close-only caption. Both support double click.
void enableStudioMinimizeCaption(QWidget* window);
QWidget* createStudioCaption(QWidget* window,QWidget* tabs=nullptr);
void installStudioCaptionEvents();
void showStudioMainWindow(QWidget* window);
// Isolated startup observer; only called by --verify-window-startup.
void verifyStudioWindowStartup(QWidget* window,const QString& output);
