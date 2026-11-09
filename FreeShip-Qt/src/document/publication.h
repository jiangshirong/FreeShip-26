// GPL-2.0-or-later. Immutable FBM -> UI/lines-plan publication, without a
// legacy ship object. Binary/display provenance is recorded in NATIVE-PUBLICATION.md.
#pragma once
#include "fbm_codec.h"
namespace fs::document {
class Publication {
public:
    Publication(const Fbm &source, const QJsonObject &bindings);
    // Writes into a private transaction directory owned by the caller. The
    // caller validates every artifact before returning it or committing history.
    QJsonObject generate(const QString &directory,const QString &cacheDirectory,
                         const QString &documentId,quint64 revision) const;
private:
    QJsonObject model_,input_;
};
}
