#pragma once

#include "core/model_snapshot.h"
#include "view_lighting.h"
#include "surface_analysis.h"
#include "drag_section_compare.h"
#include <QImage>
#include <QIcon>
#include <QPicture>
#include <QPointF>
#include <QSet>
#include <QWidget>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <QJsonObject>

class QMenu;
class QToolButton;

enum class HullProjection { Profile, Plan, Body, Perspective };
namespace fs::curveEdit {class Motion;}
enum class HullEntity { Point, Edge, Face, Curve, Flowline };
enum class BackgroundMode { None, Origin, Scale, Frame, TransparentColor };

// A camera onto an immutable model snapshot. Editing is requested through the
// callback; the view never changes the model or regenerates its derived mesh.
class HullView : public QWidget {
public:
    explicit HullView(QWidget* parent = nullptr);
    ~HullView() override;
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot,
                     std::shared_ptr<const fs::ModelSnapshot> presentation = {},bool drawingUnchanged=false,
                     const QSet<fs::EntityId>& visibilityChanges={});
    void setLayerVisibilityPreview(const QSet<fs::EntityId>& changes);
    void setDisplayPreview(std::shared_ptr<const fs::ModelSnapshot> presentation);
    void setMoveSectionPreview(std::shared_ptr<const QVector<fs::Section>> sections);
    std::shared_ptr<const QVector<fs::Section>> moveSectionPreview()const;
    struct DragShellRaster;
    using DragRasterLayer=std::shared_ptr<const DragShellRaster>;
    using DragRasterJob=std::function<DragRasterLayer(const DragShell&,const DragRasterLayer&,const std::atomic_bool*)>;
    void setDragSectionComparison(DragSections pairs,DragShell shell={},std::shared_ptr<const DragShellRaster> raster={});
    DragRasterJob dragRasterJob(bool originalOnly=false)const;
    bool verifyDragRaster()const;
    void retainReleasedCreases();
    DragShell dragShellComparison()const;
    DragSections dragSectionComparison()const;
    fs::Vec3 pointMoveAnchor()const;
    bool axisPointMove()const;
    void setPendingPointMoves(const QVector<fs::PointMove>& moves);
    void setPlateDiagnosticFaces(const QString& document, quint64 revision, QVector<QVector<fs::Vec3>> faces);
    void setFaceOrientationPreview(QSet<fs::EntityId> conflicts, QSet<fs::EntityId> flipped = {});
    QSet<fs::EntityId> selectedMissingFace(const QSet<fs::EntityId>& selection) const;
    bool curveEditMotionEnabled() const;
    void setCurveEditMotion(std::shared_ptr<fs::curveEdit::Motion> motion);
    std::shared_ptr<const fs::ModelSnapshot> snapshot() const;
    void setProjection(HullProjection projection);
    HullProjection projection() const;
    void setSelectedPoint(fs::EntityId id);
    fs::EntityId selectedPoint() const;
    void setSelectedPoints(QSet<fs::EntityId> ids);
    QSet<fs::EntityId> selectedPoints() const;
    void setSelectionKind(HullEntity kind);
    HullEntity selectionKind() const;
    void setSelectedEntities(QSet<fs::EntityId> ids);
    QSet<fs::EntityId> selectedEntities() const;
    fs::EntityId entityAt(QPointF pixel) const;
    void setStudioAppearance(bool enabled);
    void setGraphicPreferences(const QJsonObject& colors);
    void setFillVisible(bool enabled);
    bool setShadeMode(HullShade mode);
    HullShade shadeMode()const;
    bool setFocalLength(int millimeters);
    int focalLength()const;
    bool setLight(const HullLight& light);
    HullLight light()const;
    QString displayModeKey()const;
    void restoreDisplayMode(const QString& mode);
    bool selectDisplayMode(const QString& mode);
    void installDisplayModeButton(const std::function<QIcon(const QString&)>& icon);
    void setDisplayModeButtonCorner(Qt::Corner corner);
    QJsonObject viewSettingsEvidence()const;
    // Deterministic probe of the production fallback, without changing settings.
    QImage softwareImage(qreal devicePixelRatio=1)const;
    // Small file previews use the same complete depth-tested mesh, without
    // constructing editing annotations, font fallbacks or window captions.
    QImage thumbnailImage(qreal devicePixelRatio=1)const;
    void setControlNetVisible(bool enabled);
    void setHiddenControlLayers(const QSet<fs::EntityId>& ids);
    void setSectionVisible(int kind,bool enabled);
    void highlightSections(int kind,QSet<int> indices);
    void setGridVisible(bool enabled);
    void setHydroVisible(bool enabled);
    bool gridVisible()const;
    bool hydroVisible()const;
    void setCoordinateAxis(int axis);
    int coordinateAxis()const;
    void setAnimationsEnabled(bool enabled);
    void restartPresentation();
    void finishPresentation();
    void freezePresentation(bool frozen);
    bool presentationActive()const;
    // Deterministic renderer probe; ordinary image export always renders the final state.
    QImage presentationImageAt(qint64 milliseconds);
    int verifyResizeSectionPixels() const; // -1: ordinary path; 0/1: unequal/equal layer
    QJsonObject overlayEvidence()const;
    QJsonObject interactionMotionEvidence()const;
    void setEditingEnabled(bool enabled);
    bool editingEnabled() const;
    void setFlowPlacement(bool enabled);
    bool flowPlacement()const;
    void setPointPlacement(bool enabled);
    void setPlacementUndoPreview(const QSet<fs::EntityId>& ids);
    void setPendingPlacements(const QVector<fs::Vec3>& positions);
    bool pointPlacement()const;
    QPointF flowSource(QPointF screen)const;
    int backgroundView()const;
    void populateBackgroundMenu(QMenu* menu);
    const fs::BackgroundImage* background()const;
    QRectF backgroundRect()const;
    double backgroundImportScale(int imageWidth)const;
    bool setBackgroundMode(BackgroundMode);
    BackgroundMode backgroundMode()const;
    QPointF backgroundReference(QPointF screen)const;
    bool calibrateBackgroundAt(QPointF screen,QPointF coordinate);
    void fit();
    void setViewDividerDragging(bool dragging);
    void panBy(QPointF delta);
    void zoomAt(QPointF anchor, double factor);
    QPointF project(const fs::Vec3& point) const;
    std::optional<fs::Vec3> displayedControlPoint(fs::EntityId id)const;
    QPointF projectControlPoint(fs::EntityId id)const;
    QImage image(qreal devicePixelRatio = 1) const;
    // Immutable printer commands. Text/lines remain vector; only shaded fills
    // and imported background images are raster layers.
    QPicture printPicture(qreal rasterRatio=1)const;
    QString projectionLabel() const;
    bool gpuReady() const;

    double lastPaintMs = 0;
    quint64 paintCount = 0;
    // Only active during the isolated frame-by-frame drag diagnostic.
    bool measureDragPaint = false;
    mutable QJsonObject dragPaintTimings;
    std::function<void()> onDragRasterTargetChanged;
    std::function<void()> onHullFrame;
    std::function<void()> onHullVisibleFrame;
    std::function<void(fs::EntityId)> onPointSelected,onRepeatPointClick;
    std::function<void(fs::EntityId, fs::Vec3)> onPointMoved;
    std::function<void(QSet<fs::EntityId>)> onSelectionChanged;
    std::function<void(QVector<fs::PointMove>)> onPointsMoved;
    std::function<void(const QVector<fs::PointMove>&)> onTransformPreview;
    std::function<void()> onPointMovePreviewChanged;
    std::function<void(QString)> onEditRejected;
    std::function<void(HullEntity,QSet<fs::EntityId>)> onEntitySelectionChanged;
    std::function<void(HullEntity,fs::EntityId,QPoint)> onRepeatEntityClick;
    std::function<void(QString,QPointF)> onBackgroundAction;
    std::function<void(QJsonObject)> onBackgroundChanged;
    std::function<void(int,QPointF)> onFlowRequested;
    std::function<void()> onFlowPlacementEnded;
    std::function<void(fs::Vec3)> onPointCreationRequested;
    std::function<void()> onPointPlacementEnded;
    std::function<void(QString)> onViewAction;
    std::function<void(const QString&)> onViewSettingsChanged;
    std::function<bool(QMenu&)> onPopulateSelectionMenu;
    std::function<void(QMenu&)> onPopulateContextMenu;
    std::function<void()> onShowControlNet;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void hideEvent(QHideEvent*) override;
    void showEvent(QShowEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    struct State;
    std::unique_ptr<State> state_;
    QToolButton* displayModeButton_=nullptr;
    Qt::Corner displayModeCorner_=Qt::TopRightCorner;
    void refreshDisplayModeButton();
    void positionDisplayModeButton();
    fs::EntityId repeatPoint_=0;
    QString projectionSourceKey() const;
    void draw(QPainter&, qreal devicePixelRatio,bool final=true,bool forceSoftware=false,int* verifySections=nullptr,
              bool deferFaceLegend=false) const;
    void fitToViewport(bool resizing);
    bool showsTransformHandles()const;
    bool canTransform()const;
    bool beginTransform(int axis,QPointF origin,bool handle);
    void rebuildInfluence();void previewTransform(QPointF cursor);void finishTransform();
    QVector<fs::PointMove> transformMoves()const;void drawTransform(QPainter&)const;
    void cancelGesture(bool committingPointMove=false);
    void selectWithLeftButton(QPointF position,bool additive);
    void notifySelectionChanged();
    void chooseCoordinateAxis(int axis);
    void updateInteractionHover(fs::EntityId id);
    void emphasizeSelection(const QSet<fs::EntityId>& previous,const QSet<fs::EntityId>& current,HullEntity kind);
    void advanceInteractionMotion();
    void finishInteractionCamera();
};
