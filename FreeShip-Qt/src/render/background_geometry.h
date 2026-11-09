// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/background_image.h"
namespace fs {
struct BackgroundCalibration {QPoint origin;double scale=1;};
// Sides: 0 interior, 1 west, 2 east, 3 north, 4 south.
int backgroundSide(QRectF rect,QPointF at,double margin=7);
bool moveBackground(const BackgroundImage&,QPointF delta,double pixelsPerUnit,BackgroundCalibration&);
bool resizeBackground(const BackgroundImage&,QPointF zero,double pixelsPerUnit,int side,QPointF delta,BackgroundCalibration&);
bool calibrateBackground(const BackgroundImage&,QPointF pixelOffset,QPointF desiredCoordinate,double& scale);
}
