// SPDX-License-Identifier: GPL-2.0-or-later
// The UI protocol is shared with FreeShip's other native updaters. The macOS
// feed is deliberately separate: signed product, architecture, version, OS
// floor, DMG size/digest and expiry. Neither Apple Developer ID nor the system
// security policy is replaced by the manifest signature.
#include "macos_bundle_location.h"
#include "macos_channel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <QXmlStreamReader>
#include <Security/Security.h>
#include <mach-o/loader.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <functional>
#include <stdexcept>

namespace {
using namespace fs26::macos_update;
#if defined(Q_PROCESSOR_ARM_64)
constexpr char arch[] = "arm64";
constexpr cpu_type_t cpu = CPU_TYPE_ARM64;
#elif defined(Q_PROCESSOR_X86_64)
constexpr char arch[] = "x86_64";
constexpr cpu_type_t cpu = CPU_TYPE_X86_64;
#else
#error Unsupported FreeShip macOS architecture
#endif
constexpr char bundleId[] = "io.github.jiangshirong.freeship26";
constexpr char appName[] = "FreeShip 26.app";
constexpr char schema[] = "fs26-macos-dmg-v1";
constexpr qint64 maxImage = 1024LL * 1024 * 1024;
constexpr qint64 maxManifest = 1024 * 1024;
const QRegularExpression versionPattern("^(?:0|[1-9][0-9]{0,7})\\.(?:0|[1-9][0-9]{0,7})\\.(?:0|[1-9][0-9]{0,7})\\z");
const QRegularExpression osPattern("^[0-9]{1,3}\\.[0-9]{1,3}(?:\\.[0-9]{1,3})?\\z");
const QRegularExpression hashPattern("^[a-f0-9]{64}\\z");

volatile std::sig_atomic_t stopping = 0;
void onStop(int) { stopping = 1; }
[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
void require(bool ok, const QString &message) { if (!ok) fail(message); }
const QString installFailure = QStringLiteral("更新安装失败");
const QString badManifest = QStringLiteral("更新清单无效");
const QString invalidPayload = QStringLiteral("更新安装包完整校验失败");

QByteArray decoded(const QString &text) {
    return QByteArray::fromBase64(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
}
QByteArray boundedRead(const QString &path, qint64 limit) {
    QFileInfo info(path);
    QFile file(path);
    require(info.isFile() && !info.isSymLink() && file.open(QIODevice::ReadOnly) && file.size() <= limit,
            QStringLiteral("无法读取更新文件"));
    const auto bytes = file.read(limit + 1);
    require(bytes.size() <= limit && file.atEnd() && file.error() == QFileDevice::NoError, QStringLiteral("无法读取更新文件"));
    return bytes;
}
void save(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit(),
            QStringLiteral("无法保存更新文件"));
}
QString process(const QString &program, const QStringList &arguments, int timeout = 30000) {
    QProcess p;
    p.start(program, arguments);
    if (!p.waitForStarted(5000) || !p.waitForFinished(timeout)) {
        p.kill(); p.waitForFinished(5000);
        fail(installFailure);
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        const QByteArray error = p.readAllStandardError().right(4096);
        std::fprintf(stderr, "%s: %s\n", qPrintable(program), error.constData());
        fail(installFailure);
    }
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}
QString plist(const QString &app, const QString &name) {
    return process("/usr/libexec/PlistBuddy", {"-c", "Print :" + name, app + "/Contents/Info.plist"});
}
int compareVersions(const QString &a, const QString &b) {
    return QVersionNumber::compare(QVersionNumber::fromString(a).normalized(), QVersionNumber::fromString(b).normalized());
}
bool hostSupports(const QString &floor) {
    const auto os = QOperatingSystemVersion::current();
    return QVersionNumber(os.majorVersion(), os.minorVersion(), std::max(0, os.microVersion())) >= QVersionNumber::fromString(floor);
}

// Security.framework imports RSA public keys as DER PKCS#1, while the
// existing release config stores the same two public integers in XML.
QByteArray derField(char tag, const QByteArray &value) {
    QByteArray out(1, tag);
    if (value.size() < 128) out += char(value.size());
    else { out += char(0x82); out += char(value.size() >> 8); out += char(value.size()); }
    return out + value;
}
QByteArray positiveInteger(QByteArray value) {
    if (static_cast<unsigned char>(value.at(0)) >= 128) value.prepend('\0');
    return derField(2, value);
}
QByteArray releaseKey() {
    QXmlStreamReader xml(QString::fromUtf8(keyXml));
    QByteArray modulus, exponent;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("Modulus")) modulus = decoded(xml.readElementText());
        else if (xml.isStartElement() && xml.name() == QStringLiteral("Exponent")) exponent = decoded(xml.readElementText());
    }
    require(!xml.hasError() && modulus.size() >= 384 && modulus.size() <= 1024 && !exponent.isEmpty() && exponent.size() <= 8,
            QStringLiteral("更新公钥无效"));
    return derField(0x30, positiveInteger(modulus) + positiveInteger(exponent));
}
bool verifySignature(const QByteArray &key, const QByteArray &data, const QByteArray &signature) {
    const void *keys[] = {kSecAttrKeyClass, kSecAttrKeyType};
    const void *values[] = {kSecAttrKeyClassPublic, kSecAttrKeyTypeRSA};
    const auto attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const auto asCF = [](const QByteArray &value) {
        return CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(value.constData()), value.size());
    };
    const auto encodedKey = asCF(key);
    const auto publicKey = SecKeyCreateWithData(encodedKey, attributes, nullptr);
    CFRelease(encodedKey); CFRelease(attributes);
    if (!publicKey) return false;
    const auto clear = asCF(data), sig = asCF(signature);
    const bool verified = SecKeyGetBlockSize(publicKey) >= 384 &&
        SecKeyVerifySignature(publicKey, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, clear, sig, nullptr);
    CFRelease(clear); CFRelease(sig); CFRelease(publicKey);
    return verified;
}

QUrl address(const QString &text) {
    const QUrl url(text, QUrl::StrictMode);
    require(url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment() &&
#ifdef FS26_MACOS_UPDATE_TEST
            url.scheme() == "http" && url.host() == "127.0.0.1",
#else
            url.scheme() == "https",
#endif
            QStringLiteral("更新地址必须使用 HTTPS"));
    return url;
}

struct Release {
    QString version, floor, url, sha256;
    qint64 size = 0;
    QJsonObject notes;
};
Release parse(const QByteArray &envelope, const QByteArray &trustKey, const QString &endpoint) {
    const auto top = QJsonDocument::fromJson(envelope).object();
    const auto data = decoded(top["payload"].toString()), signature = decoded(top["signature"].toString());
    require(!data.isEmpty() && !signature.isEmpty() && verifySignature(trustKey, data, signature), QStringLiteral("更新签名无效"));
    const auto payload = QJsonDocument::fromJson(data).object();
    const auto package = payload["package"].toObject();
    Release r{payload["version"].toString(), payload["minimum_macos"].toString(), package["url"].toString(),
              package["sha256"].toString(), package["size"].toInteger(-1), payload["release_notes"].toObject()};
    require(payload["schema"].toString() == schema && payload["family"].toString() == family &&
            payload["channel"].toString() == endpoint && payload["arch"].toString() == arch &&
            versionPattern.match(r.version).hasMatch() && osPattern.match(r.floor).hasMatch(), badManifest);
    const auto expires = QDateTime::fromString(payload["expires"].toString(), Qt::ISODate);
    require(expires.isValid() && expires.timeSpec() != Qt::LocalTime && expires > QDateTime::currentDateTimeUtc(),
            QStringLiteral("更新清单已过期"));
    // Initial feed can announce the installed version without a newer download.
    if (payload.contains("package")) {
        require(r.size > 0 && r.size <= maxImage && hashPattern.match(r.sha256).hasMatch(), badManifest);
        address(r.url);
    }
    return r;
}

using Consume = std::function<void(const QByteArray &)>;
qint64 get(QNetworkAccessManager &network, QUrl url, qint64 limit, int deadline, const Consume &consume) {
    QElapsedTimer elapsed; elapsed.start();
    for (int hop = 0; hop <= 5; ++hop) {
        require(!stopping && elapsed.elapsed() < deadline, QStringLiteral("更新服务器返回无效响应"));
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(20000);
        request.setRawHeader("Accept-Encoding", "identity");
        request.setHeader(QNetworkRequest::UserAgentHeader, "FreeShip26-MacUpdater/1");
        auto *reply = network.get(request);
        QEventLoop loop;
        QTimer poll;
        qint64 received = 0;
        QString failure;
        const auto read = [&] {
            const auto bytes = reply->readAll();
            if (bytes.isEmpty() || !failure.isEmpty() || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) return;
            received += bytes.size();
            try {
                require(received <= limit, QStringLiteral("更新文件长度不符"));
                consume(bytes);
            } catch (const std::exception &e) {
                failure = QString::fromUtf8(e.what()); reply->abort();
            }
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
            if (stopping || elapsed.elapsed() >= deadline) reply->abort();
        });
        reply->setReadBufferSize(512 * 1024);
        poll.start(100);
        if (!reply->isFinished()) loop.exec();
        poll.stop(); read();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const auto error = reply->error();
        reply->deleteLater();
        require(failure.isEmpty(), failure);
        if (stopping) fail(QStringLiteral("下载已暂停"));
        if (elapsed.elapsed() >= deadline && deadline < 10000) fail(QStringLiteral("检查更新超时，请重试。"));
        if (status >= 300 && status < 400 && !redirect.isEmpty()) { url = address(url.resolved(redirect).toString()); continue; }
        require(status == 200 && error == QNetworkReply::NoError, QStringLiteral("更新服务器返回无效响应"));
        return received;
    }
    fail(QStringLiteral("更新服务器返回无效响应"));
}

bool validImage(const QString &path, const Release &r) {
    const QFileInfo info(path);
    QFile file(path);
    if (!info.isFile() || info.isSymLink() || info.size() != r.size || !file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && QString::fromLatin1(hash.result().toHex()) == r.sha256;
}
void progress(const char *stage, qint64 received, qint64 size, qint64 rate) {
    const auto notice = QJsonDocument(QJsonObject{{"stage", stage}, {"total_bytes", size}, {"downloaded_bytes", received},
        {"bytes_per_second", rate}}).toJson(QJsonDocument::Compact);
    std::fputs(notice.constData(), stdout); std::fputc('\n', stdout); std::fflush(stdout);
}

void validateNative(const QString &path) {
    QFile file(path);
    const QFileInfo info(path);
    mach_header_64 header{};
    require(!info.isSymLink() && info.isExecutable() && file.open(QIODevice::ReadOnly) &&
            file.read(reinterpret_cast<char *>(&header), sizeof(header)) == sizeof(header) &&
            header.magic == MH_MAGIC_64 && header.cputype == cpu, invalidPayload);
}
QString installedVersion(const QString &app) {
    require(!app.isEmpty(), installFailure);
    const auto manifest = QJsonDocument::fromJson(boundedRead(app + "/Contents/Resources/build-manifest.json", 65536)).object();
    const auto version = manifest["release_version"].toString();
    require(manifest["platform"].toString() == "macos" && manifest["architecture"].toString() == arch &&
            versionPattern.match(version).hasMatch() && version == plist(app, "CFBundleShortVersionString") &&
            plist(app, "CFBundleIdentifier") == bundleId && plist(app, "CFBundleExecutable") == "FreeShipQt", installFailure);
    return version;
}
void waitForExit(pid_t pid) {
    require(pid > 1 && pid != getpid(), installFailure);
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 45000) {
        if (kill(pid, 0) != 0 && errno == ESRCH) return;
        require(!stopping, installFailure);
        usleep(100000);
    }
    fail(installFailure);
}
void verifyApp(const QString &app, const Release &r) {
    require(QFileInfo(app).isDir() && !QFileInfo(app).isSymLink() && installedVersion(app) == r.version &&
            compareVersions(plist(app, "LSMinimumSystemVersion"), r.floor) == 0, invalidPayload);
    validateNative(app + "/Contents/MacOS/FreeShipQt");
    validateNative(app + "/Contents/MacOS/FreeShipUpdater");
    process("/usr/bin/codesign", {"--verify", "--deep", "--strict", app});
}
void stageApp(const QString &dmg, const QString &target) {
    QTemporaryDir mount;
    require(mount.isValid(), installFailure);
    const QString path = QFileInfo(mount.path()).canonicalFilePath();
    try {
        process("/usr/bin/hdiutil", {"attach", "-readonly", "-nobrowse", "-noautoopen", "-mountpoint", path, dmg}, 90000);
        const QFileInfo source(path + "/" + appName);
        require(source.isDir() && !source.isSymLink(), invalidPayload);
        process("/usr/bin/ditto", {source.absoluteFilePath(), target}, 120000);
        process("/usr/bin/hdiutil", {"detach", path}, 30000);
    } catch (...) {
        if (QStorageInfo(path).rootPath() == path) {
            try { process("/usr/bin/hdiutil", {"detach", path}); }
            catch (...) { mount.setAutoRemove(false); } // never remove a mounted tree
        }
        throw;
    }
}
void swapApps(const QString &live, const QString &staged) {
    require(renamex_np(QFile::encodeName(live).constData(), QFile::encodeName(staged).constData(), RENAME_SWAP) == 0, installFailure);
}
void install(const QString &app, const QString &image, const Release &r, bool relaunch) {
    require(writableInstall(app), QString::fromUtf8(installLocationError));
    require(hostSupports(r.floor) && validImage(image, r), invalidPayload);
    QTemporaryDir stage(QFileInfo(app).absolutePath() + "/.fs26-update-XXXXXX");
    require(stage.isValid(), installFailure);
    const QString stagedApp = stage.filePath(appName);
    stageApp(image, stagedApp);
    verifyApp(stagedApp, r);
    require(!stopping && writableInstall(app) && compareVersions(r.version, installedVersion(app)) > 0, installFailure);
    swapApps(app, stagedApp); // single same-volume atomic rename; old app still exists at stagedApp
    try {
        if (relaunch) process("/usr/bin/open", {"-a", app});
    } catch (...) {
        try { swapApps(app, stagedApp); }
        catch (...) { stage.setAutoRemove(false); std::fprintf(stderr, "Original app retained in %s\n", qPrintable(stage.path())); }
        throw;
    }
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication program(argc, argv);
    std::signal(SIGTERM, onStop); std::signal(SIGINT, onStop);
    QJsonObject report;
    QString mode, reportPath, app, stage = "arguments";
    qint64 parent = 0;
    bool relaunch = true, parentGone = false;
    int result = 1;
    try {
        const auto args = program.arguments();
        for (qsizetype i = 1; i + 1 < args.size(); ++i) if (args[i] == "--report") reportPath = args[i + 1];
        for (qsizetype i = 1; i < args.size(); ++i) {
            const auto arg = args[i];
            if (arg == "--check" || arg == "--download" || arg == "--install") {
                require(mode.isEmpty(), badManifest); mode = arg;
            } else if ((arg == "--parent" || arg == "--report") && i + 1 < args.size()) {
                const auto value = args[++i];
                if (arg == "--parent") parent = value.toLongLong();
            } else fail(badManifest);
        }
        require(!mode.isEmpty() && !reportPath.isEmpty() && (mode != "--install" || (parent > 1 && parent <= INT_MAX)), badManifest);
        stage = "bundle_location";
        app = containingApp(QCoreApplication::applicationDirPath());
        require(!app.isEmpty(), installFailure);
        bool fixtureRelaunch = true;
#ifdef FS26_MACOS_UPDATE_TEST
        // Scoped fixtures must state whether they may launch a real GUI.
        const auto fixture = QJsonDocument::fromJson(boundedRead(app + "/Contents/Resources/macos-update-fixture.json", 65536)).object();
        fixtureRelaunch = fixture["relaunch"].toBool();
#endif
        relaunch = fixtureRelaunch;
        if (mode == "--install") {
            // Only a normal application quit may release the installation.
            waitForExit(pid_t(parent)); parentGone = true;
        }
        stage = "bundle_identity";
        const QString current = installedVersion(app);
        report["current_version"] = current;
        stage = "channel_config";
        QString endpoint = QString::fromUtf8(feedBase) + "/" + arch + "/latest.json";
        QByteArray trustKey = releaseKey();
        QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/FreeShip26/Updates/macos-" + arch +
            "-" + QString::fromLatin1(QCryptographicHash::hash(endpoint.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
#ifdef FS26_MACOS_UPDATE_TEST
        endpoint = fixture["endpoint"].toString(); trustKey = decoded(fixture["public_key_der"].toString());
        cache = fixture["cache"].toString();
#endif
        address(endpoint);
        require(QDir::isAbsolutePath(cache) && QDir().mkpath(cache), QStringLiteral("无法保存更新文件"));
        struct stat state{};
        const auto cacheNative = QFile::encodeName(cache);
        require(lstat(cacheNative.constData(), &state) == 0 && S_ISDIR(state.st_mode) && state.st_uid == getuid() &&
                chmod(cacheNative.constData(), 0700) == 0, QStringLiteral("更新路径包含链接"));
        const QString lockPath = cache + "/lock";
        QFile lock(lockPath);
        require(!QFileInfo(lockPath).isSymLink() && lock.open(QIODevice::ReadWrite) && flock(lock.handle(), LOCK_EX | LOCK_NB) == 0,
                QStringLiteral("另一个更新任务正在运行"));
        const QString image = cache + "/update.dmg", ready = cache + "/ready.json";
        if (mode == "--install") {
            stage = "install";
            const auto r = parse(boundedRead(ready, maxManifest), trustKey, endpoint);
            require(compareVersions(r.version, current) > 0 && r.size > 0, QStringLiteral("待安装更新无效或已安装"));
            install(app, image, r, relaunch);
            QFile::remove(ready); QFile::remove(image);
            report["version"] = r.version;
            report["status"] = "installed";
        } else {
            QNetworkAccessManager network;
            QByteArray bytes;
            stage = "read_feed";
            get(network, address(endpoint), maxManifest, 7500, [&](const QByteArray &part) { bytes += part; });
            stage = "verify_feed";
            const auto r = parse(bytes, trustKey, endpoint);
            report["version"] = r.version;
            const int comparison = compareVersions(r.version, current);
            if (comparison >= 0) report["release_notes"] = r.notes;
            if (comparison <= 0) report["status"] = "current";
            else {
                require(r.size > 0, badManifest);
                require(hostSupports(r.floor), QStringLiteral("此更新需要更高版本的 macOS。"));
                report["transfer_total_bytes"] = r.size;
                report["downloaded_bytes"] = 0;
                if (validImage(image, r)) { save(ready, bytes); report["status"] = "ready"; }
                else if (mode == "--check") report["status"] = "available";
                else {
                    require(QStorageInfo(cache).bytesAvailable() >= r.size, QStringLiteral("无法保存更新文件"));
                    QFile::remove(ready);
                    QSaveFile file(image);
                    require(file.open(QIODevice::WriteOnly), QStringLiteral("无法保存更新文件"));
                    QElapsedTimer timer; timer.start();
                    qint64 received = 0, announced = -250;
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    stage = "download";
                    get(network, address(r.url), r.size, 20 * 60 * 1000, [&](const QByteArray &part) {
                        require(file.write(part) == part.size(), QStringLiteral("无法保存更新文件"));
                        hash.addData(part); received += part.size();
                        const qint64 elapsed = timer.elapsed();
                        if (elapsed - announced >= 250) {
                            progress("downloading", received, r.size, received * 1000 / std::max<qint64>(elapsed, 1)); announced = elapsed;
                        }
                    });
                    progress("verifying", received, r.size, 0);
                    require(received == r.size && QString::fromLatin1(hash.result().toHex()) == r.sha256, invalidPayload);
                    require(file.commit(), QStringLiteral("无法保存更新文件"));
                    save(ready, bytes); report["status"] = "ready";
                }
                if (report["status"].toString() == "ready") report["downloaded_bytes"] = r.size;
            }
        }
        result = 0;
    } catch (const std::exception &e) {
        if (stopping && mode == "--download") { report["status"] = "paused"; result = 0; }
        else {
            report["status"] = mode == "--install" ? "install_failed" : "error";
            report["error"] = QString::fromUtf8(e.what());
            report["error_stage"] = stage;
        }
    }
    bool reportSaved = true;
    if (!reportPath.isEmpty()) {
        try { save(reportPath, QJsonDocument(report).toJson(QJsonDocument::Compact)); }
        catch (...) { reportSaved = false; }
    }
    if (result && parentGone && relaunch && !app.isEmpty()) {
        try { process("/usr/bin/open", {"-a", app}); } catch (...) { /* Finder relaunch can be tried manually. */ }
    }
    return reportSaved ? result : 1;
}
