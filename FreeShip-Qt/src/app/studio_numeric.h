// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
class QWidget;
// Numeric fields share a measured width within each form/grid. Text, paths,
// choices and note editors retain their own sizing policies.
void configureStudioNumericForms(QWidget* root);
