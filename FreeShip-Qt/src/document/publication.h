// GPL-2.0-or-later. Immutable FBM -> UI/lines-plan publication, without a
// legacy ship object. Binary/display provenance is recorded in NATIVE-PUBLICATION.md.
#pragma once
#include "fbm_codec.h"
#include "core/snapshot_metadata.h"
#include <memory>
#include <future>
#include <functional>
namespace fs {class SnapshotSurface;}
namespace fs::document {
class Hydrostatics;
// Value snapshot of private, validated output. No mutable solver, live file path or worker.
class PublicationFrame {
public:
    bool canReuseMetadata() const {return !artifacts_.empty();}
    qsizetype retainedBytes() const;
    PublicationFrame packedForHistory() const;
    int restoredLayerNumber() const;
    // The destination is a new, unpublished transaction directory.
    QJsonObject replay(const QString &directory,const QString &id,quint64 revision) const;
protected:
    QJsonObject input_;
    bool packed_=false;
    mutable fs::SnapshotMetadata generatedMetadata_;
    std::map<QString,QByteArray> artifacts_;
    void writeArtifacts(const QString &directory,const QString &documentId,quint64 revision) const;
    QJsonObject writeFrame(const QString &directory,const QString &id,quint64 revision,const fs::SnapshotMetadata &model) const;
};
class Publication : public PublicationFrame {
public:
    Publication(const Fbm &source, const QJsonObject &bindings,const Publication* previous=nullptr,bool parallelPresentation=false,bool placement=false);
    // Writes into a private transaction directory owned by the caller. The
    // caller validates every artifact before returning it or committing history.
    QJsonObject generate(const QString &directory,const QString &cacheDirectory,
                         const QString &documentId,quint64 revision,const std::function<void(const QJsonObject&)>& displayReady={},bool displayOnly=false,
                         QJsonObject* timings=nullptr,std::unique_ptr<fs::SnapshotSurface>* parsedSurface=nullptr) const;
    // Retain only artifacts that the service has fully validated. Private bytes
    // avoid trusting old, externally writable transaction directories on reuse.
    void retainValidatedArtifacts(const QString &directory);
    QJsonObject generateMetadata(const QString &directory,const QJsonObject &texts,
                                const QString &documentId,quint64 revision);
    std::optional<QJsonObject> metadataChanges(const Publication &previous) const;
    std::shared_ptr<Hydrostatics> hydrostatics() const {return hydro_;}
    bool meshReused() const {return hydroReused_;}
private:
    mutable QJsonObject model_;
    mutable QJsonObject generatedSurface_;
    std::shared_ptr<Hydrostatics> hydro_;
    mutable std::future<QJsonObject> presentation_;
    bool hydroReused_=false,placementGeometryReused_=false;
};
}
