#pragma once
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <array>
#include <memory>
#include "background_image.h"

namespace fs {
using EntityId=quint64;
struct Vec3 {double x=0,y=0,z=0;};
struct PointMove {EntityId id=0;Vec3 position;};
struct ControlPoint {EntityId id=0;Vec3 position;bool locked=false;bool batchMovable=true;QString name;int vertexType=0;bool leak=false;
    EntityId anchor=0,lineA=0,lineB=0;bool hardAnchor=false;};
struct ControlEdge {EntityId id=0,a=0,b=0;bool crease=false;};
struct ControlFace {EntityId id=0,layer=0;QVector<EntityId> points;};
struct Layer {
    EntityId id=0;QString name;quint32 color=0;bool visible=true,symmetric=true;
    bool developable=false,useInHydrostatics=true,showInLinesplan=true,useForIntersections=true;
    double thickness=0,materialDensity=0;
    int alphaBlend=255;
};
struct HullTriangle {EntityId layer=0;std::array<Vec3,3> vertices;};
// Indexed subdivision polygons, distinct even when positions coincide.
// Display-only data; never serialized back into the editable document.
struct SurfaceTopology {
    QVector<Vec3> points;
    QVector<int> types;
    QVector<QVector<int>> faces;
};
struct SectionSpline {QVector<Vec3> points;QVector<bool> knuckles;};
struct PointGroup {int index=0;QString name;bool locked=false;QVector<EntityId> points;};
struct ReferenceMarker {int index=0;bool visible=true;quint32 color=0;SectionSpline spline;};
struct SurfaceNormal {EntityId face=0;Vec3 position,direction;};
struct AuxiliaryCurve {
    EntityId id=0;QString name;quint32 color=0;SectionSpline spline;
    QVector<EntityId> controls;
    int sourceView=1;std::array<double,2> source{};
};
struct Section {
    int kind=0,index=0;
    double distance=0;
    bool showCurvature=false;
    quint32 color=0;
    std::array<double,4> plane{};
    QVector<QVector<Vec3>> paths;
    QVector<SectionSpline> splines;
};
struct HydroMarker {QString key,text;Vec3 position;};
struct AreaMarker {Vec3 position;double value=0;};
struct OverlayData {
    bool present=false,grid=true,hydro=true,mainParticulars=false,hydroAvailable=false;
    quint32 gridColor=0xa0aa98,gridFontColor=0x636a52,hydroColor=0x000080;
    int pointSize=3,fontSize=9;
    QVector<HydroMarker> markers;
    QVector<AreaMarker> areas;
    QVector<Vec3> areaPath;
};
// Independent of widgets and renderer state. IDs identify editable source
// entities; triangle vertices are derived display geometry, never edit targets.
class ModelSnapshot {
public:
    QString documentId,name,units;
    QString designer,comment,createdBy;
    bool particularsSet=false;
    quint64 revision=0;
    Vec3 minimum,maximum;
    double length=0,beam=0,draft=0,waterlineZ=0;
    double waterDensity=1.025,appendageCoefficient=1;
    QString textEncoding="cp1252";
    double splitSectionLocation=0;
    bool useDefaultSplitSectionLocation=true,simplifyIntersections=false,savePreview=false;
    int precision=0;
    int coefficientBasis=1;
    // FBM visibility order: displacement, lateral area, SAC, metacentric, LCF.
    std::array<bool,5> hydroFeatures{true,true,true,true,true};
    bool shadeUnderwater=true;
    bool bothSides=true,showCurvature=false;
    bool showControlCurves=true,showFlowlines=true;
    bool showNormals=false,showMarkers=true;
    double curvatureScale=1;
    quint32 curvatureColor=0xff00ff;
    quint32 underwaterColor=0;
    int underwaterColorAlpha=255;
    bool generateAuxiliaryReports=false;
    QVector<ControlPoint> points;
    QVector<ControlEdge> edges;
    QVector<ControlFace> faces;
    QVector<Layer> layers;
    QVector<HullTriangle> triangles;
    std::shared_ptr<const SurfaceTopology> surfaceTopology;
    QVector<Section> sections;
    QVector<AuxiliaryCurve> controlCurves,flowlines;
    QVector<PointGroup> pointGroups;
    QVector<ReferenceMarker> markers;
    QVector<SurfaceNormal> surfaceNormals;
    quint32 normalColor=0x788028;
    OverlayData overlay;
    QVector<BackgroundImage> backgrounds;
    const BackgroundImage* background(int view)const;
    void loadSectionDisplay(const QByteArray& data);
    void verifySurfaceDisplay(const QByteArray& data) const;
    void loadSurfaceDisplay(const QByteArray& data);
    static ModelSnapshot load(const QString& filename);
    const ControlPoint* point(EntityId id)const;
    const Layer* layer(EntityId id)const;
    void validate()const;
private:
    QByteArray surfaceDigest_;
};
}
