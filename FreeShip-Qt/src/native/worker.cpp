// FreeShip_26 isolated native lines-plan worker. GPL-2.0-or-later.
#include "scene_builder.h"
#include <QGuiApplication>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <stdexcept>
#include <thread>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
QByteArray read(const QString &path, qint64 limit = 256 * 1024 * 1024) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > limit)
        throw std::runtime_error("Cannot read native input/artifact");
    return f.readAll();
}
void write(const QString &path, const QByteArray &data) {
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        throw std::runtime_error("Cannot commit native artifact");
}
QByteArray hash(const QByteArray &data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}
QJsonObject json(const QByteArray &data) {
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(data, &e);
    if (e.error != QJsonParseError::NoError || !d.isObject())
        throw std::runtime_error("Invalid native JSON");
    return d.object();
}
QJsonObject labelWidths(const Scene& scene){
    QFont font("Arial");font.setPixelSize(10);QFontMetrics metrics(font);QJsonObject widths;
    for(const auto& primitive:scene.primitives)if(primitive.kind==3)widths[primitive.text]=metrics.horizontalAdvance(primitive.text);
    return widths;
}
bool validLabelWidths(const QJsonObject& metadata){
    if(!metadata["label_widths"].isObject())return false;
    QFont font("Arial");font.setPixelSize(10);QFontMetrics metrics(font);
    const auto widths=metadata["label_widths"].toObject();
    for(auto i=widths.begin();i!=widths.end();++i)if(i.value().toInt(-1)!=metrics.horizontalAdvance(i.key()))return false;
    return true;
}
void trimCache(const QString &active) {
    QDir base = QFileInfo(active).dir();
    auto entries = base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);
    qint64 bytes = 0;
    int count = 0;
    QList<QFileInfo> owned;
    for (const auto &entry : entries) {
        auto name = entry.fileName();
        if (entry.isSymLink() || name.size() != 64 || !std::all_of(name.begin(), name.end(), [](QChar c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            }))
            continue;
        try {
            auto metadata = json(read(entry.filePath() + "/entry.json", 1024 * 1024));
            if (metadata["format"] != "FreeShipNativeLinesCache" || metadata["key"] != name)
                continue;
        } catch (...) {
            continue;
        }
        owned.append(entry);
        ++count;
        for (const auto *file : {"scene.fslp", "reference.dxf", "sections.json", "surface.json", "entry.json"})
            bytes += QFileInfo(entry.filePath() + "/" + file).size();
    }
    for (const auto &entry : owned) {
        if (count <= 16 && bytes <= 256 * 1024 * 1024)
            break;
        if (entry.filePath() == active)
            continue;
        for (const auto *file : {"scene.fslp", "reference.dxf", "sections.json", "surface.json", "entry.json"}) {
            QFileInfo info(entry.filePath() + "/" + file);
            if (info.isSymLink())
                continue;
            auto size = info.size();
            if (QFile::remove(info.filePath()))
                bytes -= size;
        }
        if (base.rmdir(entry.fileName()))
            --count;
    }
}
} // namespace
int main(int argc, char **argv) {
    // No window, GPU context, or user preferences. Qt supplies portable text metrics.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    if (app.arguments().size() < 3 || app.arguments().size() > 5)
        return 3;
#ifdef Q_OS_WIN
    if (app.arguments().size() == 5) {
        bool valid = false;
        const auto pid = app.arguments()[4].toUInt(&valid);
        if (!valid || pid == 0)
            return 3;
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!parent)
            return 3;
        // A crashed/terminated model service must not leave a computing orphan.
        std::thread([parent] {
            if (WaitForSingleObject(parent, INFINITE) == WAIT_OBJECT_0)
                TerminateProcess(GetCurrentProcess(), 1);
            CloseHandle(parent);
        }).detach();
    }
#endif
    QElapsedTimer elapsed;
    elapsed.start();
    const auto output = QDir(app.arguments()[2]);
    QJsonObject report{{"status", "error"}, {"engine", "cpp-native"}};
    try {
        if (!output.exists())
            throw std::runtime_error("Output directory does not exist");
        auto root = json(read(app.arguments()[1]));
        auto input = fs::native_geometry::DocumentInput::read(root);
        auto keyData = root;
        keyData.remove("document_id");
        keyData.remove("revision");
        auto engine = hash(read(QCoreApplication::applicationFilePath()));
        auto key = hash(engine + QJsonDocument(keyData).toJson(QJsonDocument::Compact));
        QString cache;
        QJsonObject outputHashes;
        bool hit = false;
        if (app.arguments().size() >= 4 && !app.arguments()[3].isEmpty()) {
            QDir base(app.arguments()[3]);
            if(!QFileInfo(base.absolutePath()).isSymLink()&&base.mkpath("."))cache = base.filePath(QString::fromLatin1(key));
            try {
                if(cache.isEmpty()||QFileInfo(cache).isSymLink())throw std::runtime_error("Unavailable cache directory");
                auto meta = json(read(cache + "/entry.json", 1024 * 1024));
                auto scene = read(cache + "/scene.fslp"), dxf = read(cache + "/reference.dxf");
                auto sections = read(cache + "/sections.json");
                auto surface = input.surfaceDisplay ? read(cache + "/surface.json") : QByteArray{};
                outputHashes={{"scene",QString::fromLatin1(hash(scene))},{"dxf",QString::fromLatin1(hash(dxf))},
                    {"sections",QString::fromLatin1(hash(sections))},{"surface",QString::fromLatin1(hash(surface))}};
                if (meta["format"] == "FreeShipNativeLinesCache" &&
                    meta["key"].toString().toLatin1() == key &&
                    meta["scene"] == outputHashes["scene"] && meta["dxf"] == outputHashes["dxf"] &&
                    meta["sections"] == outputHashes["sections"] &&
                    (!input.surfaceDisplay || meta["surface"] == outputHashes["surface"])) {
                    if(!validLabelWidths(meta))
                        throw std::runtime_error("Cached text metrics changed");
                    if (!dxf.trimmed().endsWith("EOF"))
                        throw std::runtime_error("Incomplete cached DXF");
                    write(output.filePath("scene.fslp"), scene);
                    write(output.filePath("reference.dxf"), dxf);
                    write(output.filePath("sections.json"), sections);
                    if (input.surfaceDisplay) write(output.filePath("surface.json"), surface);
                    report = meta["diagnostics"].toObject();
                    hit = true;
                }
            } catch (const std::exception &) { /* A missing/corrupt derived cache is regenerated. */
            }
        }
        if (!hit) {
            auto result = fs::native_geometry::buildLinesplan(input,
                cache.isEmpty()?QString{}:QDir(app.arguments()[3]).filePath("sections"),engine);
            result.scene.save(output.filePath("scene.fslp"));
            write(output.filePath("reference.dxf"), result.dxf);
            write(output.filePath("sections.json"), result.sectionDisplay);
            if (input.surfaceDisplay) write(output.filePath("surface.json"), result.surfaceDisplay);
            report = result.diagnostics;
            auto scene = read(output.filePath("scene.fslp"));
            outputHashes={{"scene",QString::fromLatin1(hash(scene))},{"dxf",QString::fromLatin1(hash(result.dxf))},
                {"sections",QString::fromLatin1(hash(result.sectionDisplay))},{"surface",QString::fromLatin1(hash(result.surfaceDisplay))}};
            // The document transaction and GUI both parse/validate the result.
            // Repeating Scene::load here needlessly compiles every drawing path.
            if (!cache.isEmpty())
                try {
                    if(scene.size()+result.dxf.size()+result.sectionDisplay.size()+result.surfaceDisplay.size()>255LL*1024*1024)
                        throw std::runtime_error("Derived result exceeds the cache budget");
                    QDir().mkpath(cache);
                    write(cache + "/scene.fslp", scene);
                    write(cache + "/reference.dxf", result.dxf);
                    write(cache + "/sections.json", result.sectionDisplay);
                    if (input.surfaceDisplay) write(cache + "/surface.json", result.surfaceDisplay);
                    QJsonObject meta{{"format", "FreeShipNativeLinesCache"},
                                     {"key", QString::fromLatin1(key)},
                                     {"scene", outputHashes["scene"]},
                                     {"dxf", outputHashes["dxf"]},
                                     {"sections", outputHashes["sections"]},
                                     {"surface", outputHashes["surface"]},
                                     {"label_widths",labelWidths(result.scene)},
                                     {"diagnostics", report}};
                    write(cache + "/entry.json", QJsonDocument(meta).toJson());
                    trimCache(cache);
                } catch (const std::exception
                             &) { /* Caching is optional; the requested output is already complete. */
                }
        }
        report.insert("status", "pass");
        report.insert("document_id", input.documentId);
        report.insert("revision", double(input.revision));
        report.insert("cache_hit", hit);
        if(hit){
            report.insert("section_cache_hits",0);report.insert("section_cache_misses",0);
            report.insert("local_geometry_cache_hit",false);
            for(const auto* field:{"local_geometry_load_ms","local_geometry_compute_ms","local_geometry_save_ms","subdivision_evaluated","subdivision_changed_points","subdivision_changed_roots","fill_roots_reused","fill_roots_rebuilt","fill_prepare_ms","fill_sort_project_ms"})report.insert(field,0);
            report.insert("section_cache_timings",QJsonObject{});
            for(const auto* field:{"generation_ms","subdivision_ms","section_ms","section_index_ms","section_cache_prepare_ms","surface_display_ms"})report.insert(field,0);
        }
        report.insert("cache_key", QString::fromLatin1(key));
        report.insert("total_ms", elapsed.nsecsElapsed() / 1e6);
        report.insert("scene_sha256",outputHashes["scene"]);
        report.insert("dxf_sha256",outputHashes["dxf"]);
        report.insert("sections_sha256",outputHashes["sections"]);
        if (input.surfaceDisplay)
            report.insert("surface_sha256",outputHashes["surface"]);
        write(output.filePath("native-report.json"), QJsonDocument(report).toJson());
        return 0;
    } catch (const std::exception &e) {
        report.insert("error", QString::fromUtf8(e.what()));
        try {
            write(output.filePath("native-report.json"), QJsonDocument(report).toJson());
        } catch (...) {
        }
        return 1;
    }
}
