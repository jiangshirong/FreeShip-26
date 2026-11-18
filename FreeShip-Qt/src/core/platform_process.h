#pragma once
// Platform differences for launching FreeShip's own worker programs and the
// separately supplied legacy Windows solvers. Windows behaviour is unchanged:
// every helper below is the identity / a no-op there. Other platforms run the
// unchanged Windows console solvers through Wine; protocols, files and
// numerical reading stay in the existing callers.
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <stdexcept>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif
#ifdef Q_OS_LINUX
#include <cerrno>
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#endif

namespace fs::platform {

inline QString executableSuffix() {
#ifdef Q_OS_WIN
    return QStringLiteral(".exe");
#else
    return {};
#endif
}

// A FreeShip program installed next to the running one, e.g. "FreeShipDocumentService".
inline QString siblingExecutable(const QString &base) {
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + base + executableSuffix();
}

#ifdef Q_OS_LINUX
// Worker side of the "--parent pid" contract already used on Windows: end this
// worker when the owning process exits. Returns false for an invalid or absent
// parent, matching the Windows OpenProcess failure path.
inline bool terminateWithParent(quint32 pid) {
    if (pid == 0 || ::kill(pid_t(pid), 0) != 0) return false;
    if (::getppid() == pid_t(pid)) {
        if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) return false;
        if (::getppid() != pid_t(pid)) ::_exit(1); // Parent exited before the signal was armed.
        return true;
    }
    // Not our direct parent (for example an intermediate launcher): poll it.
    std::thread([pid] {
        while (::kill(pid_t(pid), 0) == 0 || errno == EPERM)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ::_exit(1);
    }).detach();
    return true;
}
#endif

#ifndef Q_OS_WIN
namespace detail {
// Windows file-name identity: trailing dots/spaces dropped, case-insensitive.
inline QString windowsName(QString name) {
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) name.chop(1);
    return name.toCaseFolded();
}
inline void alias(const QDir &dir, const QString &from, const QString &to) {
    if (::link(QFile::encodeName(dir.filePath(from)).constData(), QFile::encodeName(dir.filePath(to)).constData()) != 0)
        QFile::copy(dir.filePath(from), dir.filePath(to));
}
// The first Wine start creates the prefix (about 10 s). If that start is cut
// short (calculation cancelled, application closed), the prefix stays partial
// and the gfortran solvers (FPPCALC, RNTSP, ...) then exit with code 3 without
// output. FreeShip's own prefix is therefore initialized completely, once,
// under a lock, and marked ready. A complete older prefix is only marked; a
// partial one is moved aside, never deleted.
inline void ensureWinePrefix(const QString &wine, const QProcessEnvironment &env, const QString &prefix) {
    const QString marker = prefix + QStringLiteral("/.freeship26-prefix-ready");
    const QString registry = prefix + QStringLiteral("/system.reg");
    constexpr qint64 completeRegistry = 1024 * 1024; // A complete Wine 9 registry is about 3 MB.
    if (QFileInfo::exists(marker)) return;
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    QLockFile lock(prefix + QStringLiteral(".lock"));
    lock.setStaleLockTime(15 * 60 * 1000);
    if (!lock.tryLock(10 * 60 * 1000)) throw std::runtime_error("Wine 环境正在初始化，请稍后重试。");
    if (QFileInfo::exists(marker)) return;
    auto markReady = [&] {
        QFile f(marker);
        if (!f.open(QIODevice::WriteOnly) || f.write("ready\n") != 6) throw std::runtime_error("无法记录 Wine 环境状态。");
    };
    if (QFileInfo(registry).size() >= completeRegistry) { markReady(); return; }
    if (QFileInfo::exists(prefix)) {
        const auto aside = prefix + QStringLiteral(".incomplete-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QDir().rename(prefix, aside)) throw std::runtime_error("Wine 环境不完整，且无法移开旧目录。");
    }
    auto run = [&](const QString &program, const QStringList &arguments) {
        QProcess p;
        p.setProcessEnvironment(env);
        p.setStandardOutputFile(QProcess::nullDevice());
        p.setStandardErrorFile(QProcess::nullDevice());
        p.start(program, arguments);
        if (!p.waitForFinished(5 * 60 * 1000)) { p.kill(); p.waitForFinished(3000); return false; }
        return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    };
    // wineboot returns before wineserver has written the registry; wait for it too.
    QString server = QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
    if (!QFileInfo(server).isExecutable()) server = QStandardPaths::findExecutable(QStringLiteral("wineserver"));
    if (!run(wine, {QStringLiteral("wineboot"), QStringLiteral("--init")}) || (!server.isEmpty() && !run(server, {QStringLiteral("-w")})) ||
        QFileInfo(registry).size() < completeRegistry)
        throw std::runtime_error("Wine 环境初始化失败，请重新计算。");
    markReady();
}
}
#endif

// Before a legacy solver starts: under Wine it opens "INO." as "INO", because
// Windows drops trailing dots. Give such prepared inputs that name too.
inline void exposeLegacyInputs(const QString &work) {
#ifdef Q_OS_WIN
    Q_UNUSED(work);
#else
    const QDir dir(work);
    for (const auto &name : dir.entryList(QDir::Files | QDir::Hidden)) {
        QString plain = name;
        while (plain.endsWith(QLatin1Char('.')) || plain.endsWith(QLatin1Char(' '))) plain.chop(1);
        if (plain != name && !plain.isEmpty() && !dir.exists(plain)) detail::alias(dir, name, plain);
    }
#endif
}

// After a legacy solver exits: present its outputs under the exact names the
// caller reads ("OUT." written as "OUT", "RVRSRES.dat" written as "rvrsres.dat").
inline void adoptLegacyOutputs(const QString &work, const QStringList &names) {
#ifdef Q_OS_WIN
    Q_UNUSED(work); Q_UNUSED(names);
#else
    const QDir dir(work);
    const auto entries = dir.entryList(QDir::Files | QDir::Hidden);
    for (const auto &name : names) {
        if (dir.exists(name)) continue;
        const auto key = detail::windowsName(name);
        for (const auto &entry : entries)
            if (detail::windowsName(entry) == key) { detail::alias(dir, entry, name); break; }
    }
#endif
}

// Launcher for a legacy Windows console solver. Call after setProgram/
// setArguments/setProcessEnvironment and before start(). On Windows nothing
// changes. Elsewhere the program becomes Wine with the solver as its first
// argument; FREESHIP_WINE selects another Wine executable, and a private
// FreeShip Wine prefix is used unless WINEPREFIX is already set.
inline constexpr char legacySolverWineMissing[]="未找到 Wine。请先安装 Wine，才能运行 %1。";
inline void prepareLegacySolver(QProcess &process, const QString& missingWineMessage) {
#ifdef Q_OS_WIN
    Q_UNUSED(process);
    Q_UNUSED(missingWineMessage);
#else
    auto env = process.processEnvironment();
    if (env.isEmpty()) env = QProcessEnvironment::systemEnvironment();
    QString wine = env.value(QStringLiteral("FREESHIP_WINE"));
    if (wine.isEmpty()) wine = QStandardPaths::findExecutable(QStringLiteral("wine"));
    if (wine.isEmpty())
        throw std::runtime_error(missingWineMessage.arg(QDir(process.program()).dirName()).toStdString());
    QString ownPrefix; // Only FreeShip's private prefix is managed; a user's WINEPREFIX is left alone.
    if (!env.contains(QStringLiteral("WINEPREFIX"))) {
        const auto base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (!base.isEmpty()) {
            ownPrefix = base + QStringLiteral("/FreeShip26/wine");
            env.insert(QStringLiteral("WINEPREFIX"), ownPrefix);
        }
    }
    if (!env.contains(QStringLiteral("WINEDEBUG"))) env.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    // Console solvers need neither .NET nor HTML; skip the first-run installers.
    if (!env.contains(QStringLiteral("WINEDLLOVERRIDES"))) env.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("mscoree,mshtml="));
    if (!ownPrefix.isEmpty()) detail::ensureWinePrefix(wine, env, ownPrefix);
    process.setProcessEnvironment(env);
    if (!process.workingDirectory().isEmpty()) exposeLegacyInputs(process.workingDirectory());
    QStringList arguments{process.program()};
    arguments += process.arguments();
    process.setProgram(wine);
    process.setArguments(arguments);
#ifdef Q_OS_LINUX
    // Equivalent of the Windows kill-on-close job: the solver ends with the thread that launched it.
    process.setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGKILL); });
#endif
#endif
}

} // namespace fs::platform
