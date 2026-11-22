#pragma once
#include <QPolygonF>
#include <QColor>
struct PlateEdgeHit {
    int plate=-1,edge=-1,seam=-1;
    bool valid()const{return plate>=0;}
    bool sameStroke(PlateEdgeHit other)const{
        return plate==other.plate&&seam==other.seam&&(seam>=0||edge==other.edge);
    }
};
inline const QColor plateSeamColor("#00766a");
inline const QColor plateHoverColor("#239887");
class QWidget;
namespace fs { struct DevelopedPlates; }
struct PlateSeam {
    int left, right;
    QVector<std::pair<int,int>> segments;
    double length[2]{};
    // The two local vertex indices for each physical end, in aft-to-forward order.
    QVector<std::pair<int,int>> ends;
    bool longitudinal=false;
    QString endLabel(int end) const;
};
QVector<PlateSeam> collectPlateSeams(const fs::DevelopedPlates& data);
QString plateSeamMeasurement(const fs::DevelopedPlates& data,const PlateSeam& seam);
int showPlateAssembly(const fs::DevelopedPlates& plates,const QVector<PlateSeam>& seams,int selected,QWidget* parent);
// Projected vertices used by the isolated workflow to detect unintended motion.
QVector<QPolygonF> plateAssemblyScreenGeometry(QWidget* canvas);
