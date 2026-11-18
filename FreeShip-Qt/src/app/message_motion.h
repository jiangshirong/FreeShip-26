// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
class StudioMessage;
class QWidget;
namespace message_motion {
bool visibility(StudioMessage* message,bool visible);
void updated(StudioMessage* message,bool working);
void confirmation(QWidget* owner);
}
