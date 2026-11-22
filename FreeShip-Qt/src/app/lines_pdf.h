#pragma once
#include <QString>
class Scene;
class QWidget;

// Empty result means success or user cancellation.
QString exportLinesPdf(QWidget* parent,const Scene& scene,bool imperial,bool fill,const QString& suggestedPath);
