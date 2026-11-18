// SPDX-License-Identifier: GPL-2.0-or-later
#include "model_preview.h"
#include "hull_view.h"
#include <QBuffer>
#include <QPainter>
#include <QElapsedTimer>
#include <array>
#include <stdexcept>

QByteArray modelPreview(std::shared_ptr<const fs::ModelSnapshot> model,const QJsonObject& colors,QJsonObject* timings) {
    QElapsedTimer clock;clock.start();qint64 stamp=0;
    auto stage=[&](const char* name){const auto now=clock.nsecsElapsed();if(timings)(*timings)[name]=(now-stamp)/1e6;stamp=now;};
    if(!model)throw std::invalid_argument("No model for preview");
    QImage result(400,300,QImage::Format_RGB32);result.fill(Qt::white);
    // Hold both normal and split-body display data throughout the render. A
    // temporary view per quadrant would release and rebuild the shared mesh.
    std::array<HullView,2> views;
    stage("construct_ms");
    for(int n=0;n<2;++n){
        auto& view=views[n];view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(800,600);
        view.setAnimationsEnabled(false);view.setStudioAppearance(true);view.setGraphicPreferences(colors);
        view.setProjection(n?HullProjection::Body:HullProjection::Profile);view.setSnapshot(model);view.setFillVisible(true);
        view.setControlNetVisible(false);view.setGridVisible(false);view.setHydroVisible(false);
    }
    stage("prepare_ms");
    QPainter painter(&result);int i=0;
    for(auto projection:{HullProjection::Profile,HullProjection::Body,HullProjection::Plan,HullProjection::Perspective}) {
        auto& view=views[projection==HullProjection::Body?1:0];view.setProjection(projection);
        view.fit();view.finishPresentation();
        // Render directly to 200x150 pixels; the larger logical camera keeps
        // normal view margins from consuming a tiny thumbnail's drawing area.
        auto image=view.thumbnailImage(.25);image.setDevicePixelRatio(1);
        if(image.isNull())throw std::runtime_error("Cannot render model preview");
        painter.drawImage(QPoint((i%2)*200,(i/2)*150),image);++i;
    }
    stage("render_ms");
    painter.end();QByteArray jpeg;QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);
    if(!result.save(&buffer,"JPEG",90))throw std::runtime_error("Cannot encode model preview");
    stage("encode_ms");
    return jpeg;
}
