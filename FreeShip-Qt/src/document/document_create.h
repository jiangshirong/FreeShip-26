// GPL-2.0-or-later. Native document construction and version transitions.
#pragma once
#include "topology_edit.h"
namespace fs::document {
Fbm emptyDocument(const QJsonObject &context, int version=43);
// Upgrade only. Existing persistent records are copied, new fields use the
// documented Clear defaults. Opaque extensions are never rewritten.
Fbm promoteDocument(const Fbm &source, int version, const QJsonObject &context);
QJsonObject documentBindings(const Fbm &source, const QJsonObject &context);
TopologyEdit loadDocument(const Fbm &source, const QJsonObject &context);
TopologyEdit newDocument(const QJsonObject &request, const QJsonObject &context);
TopologyEdit importDocument(const QJsonObject &exchange, const QJsonObject &context);
TopologyEdit appendDocument(const Fbm &source,const QJsonObject &bindings,const QJsonObject &exchange);
QJsonArray documentColors(const Fbm &source);
}
