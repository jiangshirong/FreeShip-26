// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QPolygonF>
#include <array>
#include <vector>
#include <functional>
namespace fs {
struct OffsetGrid {
    using Point=std::array<double,3>;
    std::vector<Point> points;
    std::vector<std::vector<size_t>> stations,waterlines;
    Point minimum{},maximum{};
    static OffsetGrid read(const QJsonObject& data);
};
}
class GridPreviewView final:public QWidget {
public:
    explicit GridPreviewView(QWidget* parent=nullptr);
    void setGrid(const QJsonObject& data);
    void setProjection(int index);
    void fit();
    QJsonObject evidence()const;
    std::function<void()> onOrbit;
protected:
    void paintEvent(QPaintEvent*)override;
    void mousePressEvent(QMouseEvent*)override;
    void mouseMoveEvent(QMouseEvent*)override;
    void mouseReleaseEvent(QMouseEvent*)override;
    void wheelEvent(QWheelEvent*)override;
    void mouseDoubleClickEvent(QMouseEvent*)override;
private:
    QPointF direction(const fs::OffsetGrid::Point& p)const;
    fs::OffsetGrid grid_;QByteArray dataHash_;QPointF pan_,last_;double yaw_=30,pitch_=20,zoom_=1;
    Qt::MouseButton drag_=Qt::NoButton;quint64 cameraRevision_=0,gridRevision_=0;
    double pixelsPerMetre_=1,lastPaintMs_=0;int projection_=3;
};
