// SPDX-License-Identifier: GPL-2.0-or-later
// Linux counterpart of tools/updates/Updater.cs: same signed feed format,
// same command line and report protocol for StudioUpdateController.
//
//   --check | --download | --install   --report <file>   [--parent <pid>]
//
// The trust key, product family and feed address are compiled in from
// tools/updates/channel.json (generated header). The Linux feed lives next to
// the Windows one: <channel>/linux-amd64/latest.json. Its payload is the .deb.
// A test build (FS_UPDATER_TEST) additionally accepts --endpoint,
// --public-key-file, --root, --current, --install-command and plain HTTP on
// loopback; the production helper accepts none of these.
#include "updater_channel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVersionNumber>
#include <QXmlStreamReader>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <zlib.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace {
using fs::updater::channel::kEndpoint;
using fs::updater::channel::kFamily;
using fs::updater::channel::kPublicKeyXml;
#ifdef FS_UPDATER_TEST
constexpr bool kTest = true;
#else
constexpr bool kTest = false;
#endif

struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};
[[noreturn]] void fail(const QString &message) { throw Failure(message.toStdString()); }

std::atomic<bool> cancelled{false};
void onTerminate(int) { cancelled = true; }
void checkCancel() {
    QCoreApplication::processEvents();
    if (cancelled) throw Failure("cancelled");
}

const QRegularExpression kHash("^[a-f0-9]{64}$");
const QRegularExpression kVersion("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
constexpr qint64 kMaxPackage = 1024LL * 1024 * 1024;
constexpr int kMaxChunk = 262144, kMaxPacked = 263000;
const QString kInstallDir = QStringLiteral("/opt/freeship26");

QJsonObject report;
QString reportPath, root, endpoint, publicKeyXml = QString::fromUtf8(kPublicKeyXml), current = QStringLiteral("0.0.0");
QString installCommand; // test only
std::atomic<qint64> networkBytes{0};

QString sha256(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
QString sha256File(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) fail("无法读取更新文件");
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) fail("无法读取更新文件");
    return QString::fromLatin1(h.result().toHex());
}
// Same rule as Updater.Plain: no symbolic link anywhere on the path.
void plain(const QString &path) {
    for (QFileInfo info(QFileInfo(path).absoluteFilePath());; info = QFileInfo(info.absolutePath())) {
        if (info.isSymLink()) fail("更新路径包含链接");
        if (info.absoluteFilePath() == "/" || info.absoluteFilePath() == info.absolutePath()) break;
    }
}
QByteArray readFile(const QString &path, qint64 limit) {
    plain(path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > limit) fail("无法读取更新文件");
    return f.readAll();
}
void atomicWrite(const QString &path, const QByteArray &bytes) {
    plain(path);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) fail("无法保存更新文件");
}
void saveIfChanged(const QString &path, const QByteArray &bytes) {
    // As Updater.SaveIfChanged: equal length first, then equal bytes.
    if (QFileInfo::exists(path) && QFileInfo(path).size() == bytes.size() && readFile(path, bytes.size()) == bytes) return;
    atomicWrite(path, bytes);
}
void writeReport() {
    if (reportPath.isEmpty()) return;
    report["network_bytes"] = double(networkBytes.load());
    QSaveFile f(reportPath);
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Compact);
    if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size()) f.commit();
}

// ---------------------------------------------------------------- network
QUrl address(const QString &text) {
    const QUrl url(text, QUrl::StrictMode);
    const bool loopback = url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    if (!url.isValid() || url.isRelative() || !url.userInfo().isEmpty() || url.hasFragment() ||
        !(url.scheme() == "https" || (kTest && url.scheme() == "http" && loopback)))
        fail("更新地址必须使用 HTTPS");
    return url;
}
QNetworkAccessManager &network() {
    static QNetworkAccessManager manager;
    return manager;
}
QNetworkRequest request(const QUrl &url) {
    QNetworkRequest r(url);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    r.setTransferTimeout(20000);
    r.setHeader(QNetworkRequest::UserAgentHeader, "FreeShip26-Updater/1");
    r.setRawHeader("Accept-Encoding", "identity");
    return r;
}
// One bounded GET. onRead reports raw network progress.
QByteArray fetch(const QString &url, qint64 limit, int deadlineMs = 0, std::function<void(qint64)> onRead = {}) {
    QNetworkReply *reply = network().get(request(address(url)));
    QByteArray body;
    bool tooLarge = false, timedOut = false;
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&] {
        const auto part = reply->readAll();
        if (body.size() + part.size() > limit) { tooLarge = true; reply->abort(); return; }
        body += part;
        networkBytes += part.size();
        if (onRead) onRead(part.size());
    });
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer poll;
    poll.setInterval(100);
    QElapsedTimer clock;
    clock.start();
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancelled || (deadlineMs > 0 && clock.elapsed() > deadlineMs)) {
            timedOut = !cancelled;
            reply->abort();
        }
    });
    poll.start();
    if (!reply->isFinished()) loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    const qint64 declared = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    reply->deleteLater();
    if (cancelled) throw Failure("cancelled");
    if (timedOut) {
        report["error_code"] = "check_timeout";
        fail("检查更新超时，请重试。");
    }
    if (tooLarge) fail("更新响应超过允许大小");
    if (error != QNetworkReply::NoError || status != 200 || declared > limit) fail("更新服务器返回无效响应");
    return body;
}

// --------------------------------------------------------- signed manifest
struct Chunk {
    QString hash, packedHash;
    int size = 0, packedSize = 0;
};
struct Release {
    QString version, expires, sha256, nextEndpoint;
    qint64 size = 0;
    std::vector<Chunk> chunks;
    QJsonValue notes;
};

QByteArray xmlNumber(const QString &xml, const QString &name) {
    QXmlStreamReader reader(xml);
    while (reader.readNextStartElement() || !reader.atEnd()) {
        if (reader.isStartElement() && reader.name() == name) return QByteArray::fromBase64(reader.readElementText().toLatin1());
        if (reader.hasError()) break;
    }
    fail("更新公钥无效");
}
bool verifySignature(const QByteArray &data, const QByteArray &signature) {
    const auto modulus = xmlNumber(publicKeyXml, "Modulus"), exponent = xmlNumber(publicKeyXml, "Exponent");
    std::unique_ptr<BIGNUM, decltype(&BN_free)> n(BN_bin2bn(reinterpret_cast<const unsigned char *>(modulus.constData()), int(modulus.size()), nullptr), BN_free);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> e(BN_bin2bn(reinterpret_cast<const unsigned char *>(exponent.constData()), int(exponent.size()), nullptr), BN_free);
    if (!n || !e || BN_num_bits(n.get()) < 3072) fail("更新公钥无效");
    std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)> build(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!build || !OSSL_PARAM_BLD_push_BN(build.get(), OSSL_PKEY_PARAM_RSA_N, n.get()) ||
        !OSSL_PARAM_BLD_push_BN(build.get(), OSSL_PKEY_PARAM_RSA_E, e.get())) fail("更新公钥无效");
    std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)> params(OSSL_PARAM_BLD_to_param(build.get()), OSSL_PARAM_free);
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> keyContext(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY *raw = nullptr;
    if (!params || !keyContext || EVP_PKEY_fromdata_init(keyContext.get()) <= 0 ||
        EVP_PKEY_fromdata(keyContext.get(), &raw, EVP_PKEY_PUBLIC_KEY, params.get()) <= 0) fail("更新公钥无效");
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw, EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    EVP_PKEY_CTX *verifyContext = nullptr; // PKCS#1 v1.5 is the RSA default, as RSACryptoServiceProvider.VerifyData.
    if (!digest || EVP_DigestVerifyInit(digest.get(), &verifyContext, EVP_sha256(), nullptr, key.get()) <= 0) fail("更新公钥无效");
    return EVP_DigestVerify(digest.get(), reinterpret_cast<const unsigned char *>(signature.constData()), size_t(signature.size()),
                            reinterpret_cast<const unsigned char *>(data.constData()), size_t(data.size())) == 1;
}
Release verify(const QByteArray &bytes) {
    QJsonParseError error;
    const auto envelope = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError || !envelope["payload"].isString() || !envelope["signature"].isString()) fail("更新清单无效");
    const auto data = QByteArray::fromBase64(envelope["payload"].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    const auto signature = QByteArray::fromBase64(envelope["signature"].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (data.isEmpty() || signature.isEmpty() || !verifySignature(data, signature)) fail("更新签名无效");
    const auto p = QJsonDocument::fromJson(data, &error).object();
    if (error.error != QJsonParseError::NoError) fail("更新清单无效");
    Release r;
    r.version = p["version"].toString();
    r.expires = p["expires"].toString();
    r.sha256 = p["sha256"].toString();
    r.size = p["size"].toInteger(-1);
    r.notes = p["release_notes"];
    if (p.contains("next_endpoint")) r.nextEndpoint = p["next_endpoint"].toString();
    const auto chunks = p["chunks"].toArray();
    if (p["schema"].toInt() != 1 || p["family"].toString() != QString::fromUtf8(kFamily).toLower() || !kVersion.match(r.version).hasMatch() ||
        r.size < 2 || r.size > kMaxPackage || !kHash.match(r.sha256).hasMatch() || chunks.isEmpty() || chunks.size() > 65536)
        fail("更新清单无效");
    auto expires = QDateTime::fromString(r.expires, Qt::ISODateWithMs);
    if (!expires.isValid()) {
        // Python isoformat() writes microseconds; keep milliseconds for Qt.
        QString text = r.expires;
        text.replace(QRegularExpression("(\\.\\d{3})\\d+"), "\\1");
        expires = QDateTime::fromString(text, Qt::ISODateWithMs);
    }
    if (!expires.isValid() || expires <= QDateTime::currentDateTimeUtc()) fail("更新清单已过期");
    qint64 total = 0;
    std::map<QString, Chunk> unique;
    for (const auto &value : chunks) {
        const auto o = value.toObject();
        Chunk c{o["hash"].toString(), o["packed_hash"].toString(), o["size"].toInt(), o["packed_size"].toInt()};
        if (c.size <= 0 || c.size > kMaxChunk || c.packedSize <= 0 || c.packedSize > kMaxPacked || !kHash.match(c.hash).hasMatch() ||
            !kHash.match(c.packedHash).hasMatch()) fail("更新分块无效");
        const auto prior = unique.find(c.hash);
        if (prior != unique.end() && (prior->second.size != c.size || prior->second.packedHash != c.packedHash || prior->second.packedSize != c.packedSize))
            fail("更新分块冲突");
        unique[c.hash] = c;
        total += c.size;
        r.chunks.push_back(c);
    }
    if (total != r.size) fail("更新文件长度不符");
    if (!r.nextEndpoint.isEmpty()) address(r.nextEndpoint);
    return r;
}

// ------------------------------------------------- content-defined base index
struct Source {
    qint64 offset = 0;
    int size = 0;
};
std::array<quint32, 256> gear() {
    std::array<quint32, 256> a{};
    quint32 v = 0x46533236;
    for (auto &x : a) { v = v * 1664525u + 1013904223u; x = v; }
    return a;
}
// Same chunk boundaries as update_release.chunks / Updater.Index.
std::map<QString, Source> indexBase(const QString &path) {
    std::map<QString, Source> out;
    if (path.isEmpty() || !QFileInfo::exists(path)) return out;
    plain(path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxPackage) return out;
    static const auto table = gear();
    QByteArray block;
    block.reserve(kMaxChunk);
    quint32 value = 0;
    qint64 offset = 0;
    while (!f.atEnd()) {
        checkCancel();
        const auto buffer = f.read(65536);
        for (char ch : buffer) {
            const auto b = static_cast<unsigned char>(ch);
            block.append(ch);
            value = (value << 1) + table[b];
            if (block.size() >= 16384 && ((value & 65535) == 0 || block.size() == kMaxChunk)) {
                out[sha256(block)] = Source{offset, int(block.size())};
                offset += block.size();
                block.clear();
                value = 0;
            }
        }
    }
    if (!block.isEmpty()) out[sha256(block)] = Source{offset, int(block.size())};
    return out;
}

// --------------------------------------------------------------- chunks
QByteArray gunzip(const QByteArray &packed, int limit) {
    z_stream s{};
    if (inflateInit2(&s, 15 + 16) != Z_OK) fail("解压分块失败");
    QByteArray out(limit + 1, Qt::Uninitialized);
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(packed.constData()));
    s.avail_in = uInt(packed.size());
    s.next_out = reinterpret_cast<Bytef *>(out.data());
    s.avail_out = uInt(out.size());
    const int rc = inflate(&s, Z_FINISH);
    const auto produced = out.size() - qsizetype(s.avail_out);
    inflateEnd(&s);
    if (rc != Z_STREAM_END || produced > limit) fail("解压分块校验失败");
    out.resize(produced);
    return out;
}
QByteArray unpack(const Chunk &c, const QByteArray &packed) {
    if (packed.size() != c.packedSize || sha256(packed) != c.packedHash) fail("下载分块校验失败");
    const auto raw = gunzip(packed, c.size);
    if (raw.size() != c.size || sha256(raw) != c.hash) fail("解压分块校验失败");
    return raw;
}
QString chunkPath(const Chunk &c) { return root + "/chunks/" + c.hash + ".gz"; }
bool cached(const Chunk &c) {
    const auto path = chunkPath(c);
    if (!QFileInfo::exists(path) || QFileInfo(path).size() != c.packedSize) return false;
    try { unpack(c, readFile(path, c.packedSize)); return true; } catch (const Failure &) { return false; }
}

// --------------------------------------------------------- installation
QString packagePath() { return root + "/package.deb"; }
QString basePath() { return root + "/installed.deb"; }
QJsonObject installedManifest() {
    QFile f(kInstallDir + "/build-manifest.json");
    if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
// Mirrors Updater.Installed(): updates apply only to the packaged installation.
void installed() {
    QProcess q;
    q.start("dpkg-query", {"-W", "-f=${Status}|${Version}", "freeship26"});
    if (!q.waitForFinished(15000) || q.exitCode() != 0) fail("请先使用 FreeShip 26 安装包安装，再使用自动更新");
    const auto out = QString::fromUtf8(q.readAllStandardOutput());
    if (!out.startsWith("install ok installed|")) fail("请先使用 FreeShip 26 安装包安装，再使用自动更新");
    const auto manifest = installedManifest();
    const auto version = manifest["release_version"].toString();
    if (!kVersion.match(version).hasMatch()) fail("安装记录缺少版本号");
    current = version;
}
bool workCopy = false;
// Development/work installations only check, as on Windows.
void localIdentity(const QString &mode) {
    QFile f(QCoreApplication::applicationDirPath() + "/build-manifest.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto m = QJsonDocument::fromJson(f.readAll()).object();
    if (!m["local_installation_role"].toString().startsWith("development;")) return;
    workCopy = true;
    if (mode != "--check") fail("本机工作版仅检查更新，不执行用户版安装");
    current = QVersionNumber::fromString(m["release_version"].toString()).toString();
    root += "/work-check";
}

Release latest() {
    const auto remembered = root + "/endpoint.txt";
    if (QFileInfo::exists(remembered)) endpoint = QString::fromUtf8(readFile(remembered, 4096));
    const auto bytes = fetch(endpoint, 16 * 1024 * 1024, 8000);
    const auto release = verify(bytes);
    const auto seen = root + "/highest.txt";
    if (QFileInfo::exists(seen) && QVersionNumber::fromString(release.version) < QVersionNumber::fromString(QString::fromUtf8(readFile(seen, 64))))
        fail("服务器返回了较旧的更新清单");
    saveIfChanged(seen, release.version.toUtf8());
    saveIfChanged(root + "/latest.json", bytes);
    saveIfChanged(root + "/source.txt", endpoint.toUtf8());
    // Only a signature from the embedded publisher key may move the endpoint.
    if (!release.nextEndpoint.isEmpty()) saveIfChanged(remembered, release.nextEndpoint.toUtf8());
    return release;
}
bool ready(const Release &release) {
    const auto record = root + "/ready.json", package = packagePath();
    if (!QFileInfo::exists(record) || !QFileInfo::exists(package) || QFileInfo(package).size() != release.size) return false;
    try {
        const auto prepared = verify(readFile(record, 16 * 1024 * 1024));
        return prepared.version == release.version && prepared.sha256 == release.sha256 && sha256File(package) == release.sha256;
    } catch (const Failure &) { return false; }
}

void emitProgress(const char *stage, qint64 downloaded, qint64 total, double speed) {
    std::printf("{\"stage\":\"%s\",\"downloaded_bytes\":%lld,\"total_bytes\":%lld,\"bytes_per_second\":%.0f}\n",
                stage, static_cast<long long>(downloaded), static_cast<long long>(total), speed);
    std::fflush(stdout);
}

void prepare(const Release &release, bool download) {
    const auto available = indexBase(basePath());
    std::vector<Chunk> unique;
    QSet<QString> seen;
    for (const auto &c : release.chunks) if (!seen.contains(c.hash)) { seen.insert(c.hash); unique.push_back(c); }
    std::vector<Chunk> required, missing;
    for (const auto &c : unique) if (!available.count(c.hash)) required.push_back(c);
    for (const auto &c : required) { checkCancel(); if (!cached(c)) missing.push_back(c); }
    qint64 bytesToDownload = 0, total = 0, reused = 0, full = 0;
    for (const auto &c : missing) bytesToDownload += c.packedSize;
    for (const auto &c : required) total += c.packedSize;
    for (const auto &c : unique) full += c.packedSize;
    for (const auto &c : release.chunks) if (available.count(c.hash)) reused += c.size;
    const qint64 retained = total - bytesToDownload;
    report["download_bytes"] = double(bytesToDownload);
    report["transfer_total_bytes"] = double(total);
    report["downloaded_bytes"] = double(retained);
    report["reused_bytes"] = double(reused);
    report["full_download_bytes"] = double(full);
    if (!download && ready(release)) { report["status"] = "ready"; report["downloaded_bytes"] = double(total); return; }
    const auto requestFile = root + "/download-request.txt";
    if (!download) {
        if (retained > 0 && QFileInfo::exists(requestFile) && QString::fromUtf8(readFile(requestFile, 128)) == release.sha256)
            report["status"] = "paused";
        else
            report["downloaded_bytes"] = 0;
        return;
    }
    checkCancel();
    if (QStorageInfo(root).bytesAvailable() < release.size + bytesToDownload + 512LL * 1024 * 1024) fail("更新磁盘空间不足");
    saveIfChanged(requestFile, release.sha256.toUtf8());
    QDir().mkpath(root + "/chunks");
    plain(root + "/chunks");
    QSet<QString> keep;
    for (const auto &c : unique) keep.insert(c.hash + ".gz");
    static const QRegularExpression owned("^[a-f0-9]{64}\\.gz$");
    for (const auto &name : QDir(root + "/chunks").entryList({"*.gz"}, QDir::Files))
        if (owned.match(name).hasMatch() && !keep.contains(name)) QFile::remove(root + "/chunks/" + name);

    // Up to four chunks in flight; progress lines every 500 ms as on Windows.
    qint64 committed = retained, received = 0, lastReceived = 0;
    double speed = 0;
    QElapsedTimer clock;
    clock.start();
    qint64 lastTick = 0;
    emitProgress("downloading", committed, total, 0);
    QTimer tick;
    tick.setInterval(500);
    QObject::connect(&tick, &QTimer::timeout, [&] {
        const qint64 now = clock.elapsed(), elapsed = now - lastTick;
        if (elapsed <= 0) return;
        const double currentSpeed = (received - lastReceived) * 1000.0 / elapsed;
        speed = currentSpeed == 0 ? 0 : speed == 0 ? currentSpeed : .35 * currentSpeed + .65 * speed;
        lastReceived = received;
        lastTick = now;
        emitProgress("downloading", committed, total, speed);
    });
    tick.start();
    const QUrl base = address(endpoint);
    try {
        size_t next = 0;
        int inFlight = 0;
        QString firstError;
        std::map<QNetworkReply *, std::pair<Chunk, int>> active; // reply -> chunk, attempt
        QEventLoop loop;
        std::function<void(const Chunk &, int)> start = [&](const Chunk &c, int attempt) {
            auto *reply = network().get(request(base.resolved(QUrl("chunks/" + c.hash + ".gz"))));
            auto body = std::make_shared<QByteArray>();
            ++inFlight;
            active[reply] = {c, attempt};
            QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, body, &received, c] {
                const auto part = reply->readAll();
                if (body->size() + part.size() > c.packedSize) { reply->abort(); return; }
                body->append(part);
                received += part.size();
                networkBytes += part.size();
            });
            QObject::connect(reply, &QNetworkReply::finished, &loop, [&, reply, body, c, attempt] {
                --inFlight;
                active.erase(reply);
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const bool ok = reply->error() == QNetworkReply::NoError && status == 200;
                reply->deleteLater();
                if (cancelled) { loop.quit(); return; }
                try {
                    if (!ok) fail("下载失败，可重试继续下载");
                    unpack(c, *body);
                    atomicWrite(chunkPath(c), *body);
                    committed += c.packedSize;
                } catch (const Failure &) {
                    if (attempt + 1 < 3) { start(c, attempt + 1); return; }
                    if (firstError.isEmpty()) firstError = QStringLiteral("下载失败，可重试继续下载");
                }
                while (firstError.isEmpty() && inFlight < 4 && next < missing.size()) start(missing[next++], 0);
                if (inFlight == 0) loop.quit();
            });
        };
        while (inFlight < 4 && next < missing.size()) start(missing[next++], 0);
        QTimer cancelPoll;
        cancelPoll.setInterval(100);
        QObject::connect(&cancelPoll, &QTimer::timeout, &loop, [&] {
            if (!cancelled) return;
            for (auto &entry : active) entry.first->abort();
        });
        cancelPoll.start();
        if (inFlight > 0) loop.exec();
        report["downloaded_bytes"] = double(committed);
        if (cancelled) throw Failure("cancelled");
        if (!firstError.isEmpty()) fail(firstError);
    } catch (...) {
        report["downloaded_bytes"] = double(committed);
        throw;
    }
    tick.stop();
    emitProgress("verifying", committed, total, 0);

    const auto target = packagePath(), temp = target + ".building";
    plain(temp);
    {
        QFile out(temp);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) fail("无法保存更新文件");
        QFile baseFile(basePath());
        const bool haveBase = !available.empty() && baseFile.open(QIODevice::ReadOnly);
        for (const auto &c : release.chunks) {
            checkCancel();
            QByteArray raw;
            const auto source = available.find(c.hash);
            if (source != available.end() && haveBase) {
                baseFile.seek(source->second.offset);
                raw = baseFile.read(source->second.size);
                if (raw.size() != c.size || sha256(raw) != c.hash) { out.remove(); fail("本地更新基准在准备过程中发生变化"); }
            } else {
                raw = unpack(c, readFile(chunkPath(c), c.packedSize));
            }
            if (out.write(raw) != raw.size()) { out.remove(); fail("无法保存更新文件"); }
        }
        out.close();
    }
    if (QFileInfo(temp).size() != release.size || sha256File(temp) != release.sha256) { QFile::remove(temp); fail("更新安装包完整校验失败"); }
    QFile::remove(target);
    if (!QFile::rename(temp, target)) { QFile::remove(temp); fail("无法保存更新文件"); }
    atomicWrite(root + "/ready.json", readFile(root + "/latest.json", 16 * 1024 * 1024));
    report["status"] = "ready";
}

// An exited but not yet reaped parent (zombie) counts as exited.
bool alive(qint64 pid) {
    if (::kill(pid_t(pid), 0) != 0 && errno != EPERM) return false;
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) return true;
    const auto text = stat.readAll();
    const auto end = text.lastIndexOf(')');
    return !(end > 0 && end + 2 < text.size() && (text[end + 2] == 'Z' || text[end + 2] == 'X'));
}
bool waitForExit(qint64 pid, int timeoutMs) {
    QElapsedTimer clock;
    clock.start();
    while (alive(pid)) {
        if (clock.elapsed() > timeoutMs) return false;
        ::usleep(200000);
    }
    return true;
}

int install(qint64 parent) {
    if (parent <= 0) fail("缺少等待退出的程序标识");
    if (!waitForExit(parent, 30 * 60 * 1000)) fail("程序尚未退出，更新未安装");
    if (!kTest || installCommand.isEmpty()) installed();
    const auto release = verify(readFile(root + "/ready.json", 16 * 1024 * 1024));
    const auto package = packagePath();
    if (QVersionNumber::fromString(release.version) <= QVersionNumber::fromString(current) ||
        QFileInfo(package).size() != release.size || sha256File(package) != release.sha256)
        fail("待安装更新无效或已安装");
    // A private copy readable by apt's sandbox user; the signed bytes are re-checked.
    const auto staged = QDir::tempPath() + "/freeship26-update-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".deb";
    if (!QFile::copy(package, staged) || sha256File(staged) != release.sha256) { QFile::remove(staged); fail("无法准备安装包"); }
    QFile::setPermissions(staged, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    QString program;
    QStringList arguments;
    if (kTest && !installCommand.isEmpty()) {
        arguments = QProcess::splitCommand(installCommand);
        program = arguments.takeFirst();
    } else {
        // System package manager behind the desktop's administrator prompt.
        program = QStandardPaths::findExecutable("pkexec");
        if (program.isEmpty()) { QFile::remove(staged); fail("系统未安装 pkexec，无法请求管理员权限；请手动安装下载的更新包：" + package); }
        arguments = {"/usr/bin/apt-get", "install", "-y"};
    }
    arguments << staged;
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, arguments);
    p.waitForFinished(-1);
    const int code = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    atomicWrite(root + "/install.log", p.readAll().right(256 * 1024));
    QFile::remove(staged);
    report["installer_exit_code"] = code;
    report["status"] = code == 0 ? "installed" : "install_failed";
    if (code == 126 || code == 127) report["error"] = "未获得管理员授权，更新未安装";
    else if (code != 0) report["error"] = "更新安装失败，请查看安装记录";
    if (code == 0) {
        // The installed package becomes the base for the next incremental download.
        QFile::remove(basePath());
        QFile::rename(package, basePath());
        QFile::remove(root + "/ready.json");
    }
    writeReport();
    // Restart only the packaged application whose bytes match its manifest.
    if (!(kTest && !installCommand.isEmpty())) {
        const auto app = kInstallDir + "/FreeShipQt";
        const auto manifest = installedManifest()["files_sha256"].toObject();
        if (QFileInfo::exists(app) && manifest["opt/freeship26/FreeShipQt"].toString() == sha256File(app))
            QProcess::startDetached(app, {}, kInstallDir);
    }
    return code;
}

int run(const QString &mode, qint64 parent) {
    if (endpoint.isEmpty() || publicKeyXml.isEmpty()) { report["status"] = "not_configured"; return 0; }
    plain(root);
    QDir().mkpath(root);
    QFile lockFile(root + "/lock");
    if (!lockFile.open(QIODevice::ReadWrite) || ::flock(lockFile.handle(), LOCK_EX | LOCK_NB) != 0) fail("另一个更新任务正在运行");
    if (mode == "--install") return install(parent);
    const auto release = latest();
    report["version"] = release.version;
    report["current_version"] = current;
    const int comparison = QVersionNumber::compare(QVersionNumber::fromString(release.version), QVersionNumber::fromString(current));
    if (comparison >= 0 && !release.notes.isUndefined() && !release.notes.isNull()) report["release_notes"] = release.notes;
    if (comparison <= 0) { report["status"] = "current"; return 0; }
    report["status"] = "available";
    if (!workCopy) prepare(release, mode == "--download");
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::signal(SIGTERM, onTerminate);
    std::signal(SIGINT, onTerminate);
    int code = 1;
    qint64 parent = 0;
    QString mode = "--check";
    root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/FreeShip26/Updates/" + QString::fromUtf8(kFamily);
    endpoint = QString::fromUtf8(kEndpoint);
    try {
        const auto args = app.arguments();
        // Find the report first, so every argument error is reported to the caller.
        for (int i = 1; i + 1 < args.size(); ++i)
            if (args[i] == "--report") reportPath = QFileInfo(args[i + 1]).absoluteFilePath();
        for (int i = 1; i < args.size(); ++i) {
            const auto &a = args[i];
            auto value = [&] { if (i + 1 >= args.size()) fail("未知更新参数"); return args[++i]; };
            if (a == "--check" || a == "--download" || a == "--install") mode = a;
            else if (a == "--report") reportPath = QFileInfo(value()).absoluteFilePath();
            else if (a == "--parent") parent = value().toLongLong();
            else if (kTest && a == "--endpoint") endpoint = value();
            else if (kTest && a == "--public-key-file") publicKeyXml = QString::fromUtf8(readFile(value(), 65536));
            else if (kTest && a == "--root") root = QFileInfo(value()).absoluteFilePath();
            else if (kTest && a == "--current") current = value();
            else if (kTest && a == "--install-command") installCommand = value();
            else fail("未知更新参数");
        }
        if (!kTest) localIdentity(mode);
        if (!kTest && !workCopy && !endpoint.isEmpty() && mode != "--install") installed();
        code = run(mode, parent);
    } catch (const Failure &e) {
        if (cancelled) { report["status"] = "paused"; code = 0; }
        else { report["status"] = "error"; report["error"] = QString::fromStdString(e.what()); }
    } catch (const std::exception &e) {
        report["status"] = "error";
        report["error"] = QString::fromStdString(e.what());
    }
    writeReport();
    return code;
}
