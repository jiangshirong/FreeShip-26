// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "app/studio_ui.h"
#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonObject>
// Installed only during the explicitly requested isolated startup audit.
class MessageLifecycleProbe final:public QObject {
public:
 QJsonArray unexpected;
 MessageLifecycleProbe(){qApp->installEventFilter(this);}
 ~MessageLifecycleProbe(){qApp->removeEventFilter(this);}
 bool eventFilter(QObject* object,QEvent* event)override{
  if(event->type()==QEvent::Show)if(auto* message=dynamic_cast<StudioMessage*>(object);message&&message->isWindow())
   unexpected.append(QJsonObject{{"text",message->text()},{"width",message->width()},{"height",message->height()}});
  return false;
 }
};
