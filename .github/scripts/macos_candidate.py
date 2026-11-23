"""Build-branch tooling for the frozen FreeShip 26 0.1.21 macOS test candidates."""

import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import uuid
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "FreeShip-Qt"
VERSION = "0.1.21"
FEED_BASE = "https://raw.githubusercontent.com/jiangshirong/FreeShip-26/refs/heads/build/macos-v0.1.21-20261123/macos-test-feed"
UPDATE_PATCH = ROOT / ".github/patches/fs26-macos-updater.patch"
UPDATE_FILES = ("CMakeLists.txt", "src/app/update_controller.cpp", "src/app/document_workspace.cpp",
                *(f"src/app/languages/{lang}.json" for lang in ("de", "en", "es", "fr", "ja", "ru", "zh")))
QT_VERSION = "6.8.3"
MIN_MACOS = "15.0"
SOURCE_ZIP_SHA256 = "de34d43f300b274d8f5825a0ce1994ec63cf24c3180b778c4813af4907e80993"
MANIFEST_SHA256 = "d6ab6b5e9679ec825398f5196bf6a389ea325fa1622693b4fc33179c010cbaa7"
# Only in a CI copy after the original bytes have been checked. Apple Clang
# rejects copy-list-init of QPicture's explicit default-argument constructor.
COMPATIBILITY = (
    ("FreeShip-Qt/src/render/hull_view.cpp",
     b"if(!std::isfinite(rasterRatio)||rasterRatio<=0)return {};",
     b"if(!std::isfinite(rasterRatio)||rasterRatio<=0)return QPicture{};",
     "Apple Clang: directly construct the same empty QPicture"),
    ("FreeShip-Qt/src/app/startup_workflow.cpp",
     b'fs::PointMove{p["point_id"].toInt(),',
     b'fs::PointMove{static_cast<fs::EntityId>(p["point_id"].toInt()),',
     "Apple Clang: spell out the existing integer conversion in the startup probe"),
    ("FreeShip-Tests/qt/fbm_codec_tests.cpp",
     b'"paths",QJsonArray{QJsonArray{QJsonArray{0.1,2,3},QJsonArray{4,5,6}}}',
     b'"paths",QJsonArray{QJsonValue(QJsonArray{QJsonArray{0.1,2,3},QJsonArray{4,5,6}})}',
     "Apple Clang: use one JSON value to represent the inner path, not QJsonArray's copy constructor"),
)
PROGRAMS = ("FreeShipQt", "FreeShipDocumentService", "FreeShipNativeLinesplan", "FreeShipGeometryExport",
            "FreeShipUpdater")
LANGUAGES = ("de", "en", "es", "fr", "ja", "ru", "zh")
MACHO_MAGICS = {bytes.fromhex(x) for x in ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def run(*args, capture=False, timeout=120, **kwargs):
    if capture:
        kwargs["stdout"] = subprocess.PIPE
    result = subprocess.run(args, check=True, text=True, timeout=timeout, **kwargs)
    return result.stdout.strip() if capture else None


def dump(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def verify_source():
    manifest = ROOT / "FS26-BUILD-SNAPSHOT.json"
    require(digest(manifest) == MANIFEST_SHA256, "Frozen manifest hash mismatch")
    record = json.loads(manifest.read_bytes())
    files = record["sha256"]
    require(record["version"] == VERSION and len(files) == record["source_entries"] == 3373,
            "Unexpected snapshot version or inventory")
    for name, sha in files.items():
        parts = PurePosixPath(name)
        p = ROOT.joinpath(*parts.parts)
        require(not parts.is_absolute() and ".." not in parts.parts and p.is_file() and not p.is_symlink(), name)
        require(digest(p) == sha, f"Frozen file mismatch: {name}")
    print(f"Verified {len(files)} frozen source files, manifest {MANIFEST_SHA256}", flush=True)
    return record


def compatibility_overlay(source_root):
    entries = []
    for name, old, new, reason in COMPATIBILITY:
        original = (ROOT / name).read_bytes()
        require(original.count(old) == 1, f"Compatibility anchor mismatch: {name}")
        patched = original.replace(old, new, 1)
        target = source_root / name
        require(target.read_bytes() == original, f"Staged source mismatch: {name}")
        target.write_bytes(patched)
        entries.append({"file": name, "before": hashlib.sha256(original).hexdigest(),
                        "after": hashlib.sha256(patched).hexdigest(), "reason": reason,
                        "original": old.decode(), "replacement": new.decode()})
    return entries


def stage_source():
    verify_source()
    temp = Path(os.environ["RUNNER_TEMP"])
    workspace = temp / "fs26-source"
    workspace.mkdir()
    for name in ("FreeShip-Qt", "FreeShip-Tests"):
        shutil.copytree(ROOT / name, workspace / name)
    entries = compatibility_overlay(workspace)
    dump(temp / "fs26-compatibility.json", entries)
    print(json.dumps(entries, indent=2), flush=True)
    paths = [*(f"FreeShip-Qt/{name}" for name in UPDATE_FILES), "FreeShip-Tests/qt/update_about_tests.cpp"]
    before = {name: digest(workspace / name) for name in paths}
    run("git", "apply", "--check", "--whitespace=nowarn", UPDATE_PATCH, cwd=workspace)
    run("git", "apply", "--whitespace=nowarn", UPDATE_PATCH, cwd=workspace)
    overlay = [{"file": name, "before": before[name], "after": digest(workspace / name)} for name in paths]
    require(all(r["before"] != r["after"] for r in overlay), "Incomplete Mac updater patch")
    for name in ("macos_bundle_location.h", "macos_channel.h.in", "macos_updater.cpp", "macos_updater.cmake"):
        path = workspace / "FreeShip-Qt/src/updater" / name
        overlay.append({"file": path.relative_to(workspace).as_posix(), "after": digest(path)})
    dump(temp / "fs26-macos-update-overlay.json", {"patch_sha256": digest(UPDATE_PATCH), "files": overlay})


def put_resources(resources, commit, arch):
    bridge = SOURCE / "runtime-data/bridge"
    target = resources / "bridge"
    target.mkdir()
    for name in ("legacy-source.zip", "source-id.txt", "report.json"):
        shutil.copyfile(bridge / name, target / name)
    shutil.copytree(bridge / "locale", target / "locale")
    # Keep theme INIs and artwork, not old generator scripts/design sources.
    shutil.copytree(bridge / "Themes", target / "Themes",
                    ignore=shutil.ignore_patterns("*.exe", "*.sh", "*.cmd", "*.bat", "*.odg", ".directory"))
    lang_dir = resources / "languages"
    lang_dir.mkdir()
    for lang in LANGUAGES:
        name = f"{lang}.json"
        patched = Path(os.environ["RUNNER_TEMP"]) / "fs26-source/FreeShip-Qt/src/app/languages"
        shutil.copyfile(patched / name, lang_dir / name)
        require(json.loads((lang_dir / name).read_bytes())["id"] == lang, f"Language ID mismatch: {lang}")
    require((target / "Themes/StudioLight/icons/manifest.json").is_file(), "Missing StudioLight assets")
    dump(resources / "build-manifest.json", {
        "release_version": VERSION,
        "platform": "macos",
        "architecture": arch,
        "deployment_target": MIN_MACOS,
        "local_installation_role": "macos-test-candidate",
        "frozen_source_zip_sha256": SOURCE_ZIP_SHA256,
        "snapshot_manifest_sha256": MANIFEST_SHA256,
        "source_commit": commit,
        "apple_notarized": False,
        "update_channel_configured": True,
        "update_channel_url": f"{FEED_BASE}/{arch}/latest.json",
    })
    for name in ("COPYING", "NOTICE.md"):
        shutil.copyfile(SOURCE / name, resources / name)
    licenses = resources / "licenses"
    licenses.mkdir()
    for name in ("clipper2", "jkqtmathtext", "manifold", "shipcad"):
        dest = licenses / name
        dest.mkdir()
        for src in (SOURCE / "third_party" / name).iterdir():
            if src.is_file() and src.name.startswith(("LICENSE", "DEPENDENCY-NOTICES", "origin", "provenance")):
                shutil.copyfile(src, dest / src.name)
    qt_licenses = licenses / "qt"
    qt_licenses.mkdir()
    for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt", "GPL-2.0-only.txt", "Qt-GPL-exception-1.0.txt"):
        url = f"https://raw.githubusercontent.com/qt/qtbase/v{QT_VERSION}/LICENSES/{name}"
        with urllib.request.urlopen(url, timeout=30) as response:
            (qt_licenses / name).write_bytes(response.read())
    (qt_licenses / "SOURCE-PROVENANCE.txt").write_text(
        f"Qt {QT_VERSION}: Qt Base and Qt SVG, from the Qt distribution archives.\n"
        f"Qt source for this version: https://download.qt.io/archive/qt/6.8/{QT_VERSION}/submodules/\n"
        f"FreeShip 26 corresponding source and packaging: https://github.com/jiangshirong/FreeShip-26/tree/{commit}\n",
        encoding="utf-8",
    )


def create_icon(resources, temp):
    iconset = temp / "FreeShip26.iconset"
    iconset.mkdir()
    png = SOURCE / "src/app/assets/brand/freeship26.png"
    # The original 256px icon is reused; the Finder representation is not a UI redesign.
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = "@2x" if scale == 2 else ""
            run("sips", "-z", str(size * scale), str(size * scale), png,
                "--out", iconset / f"icon_{size}x{size}{suffix}.png", capture=True)
    run("iconutil", "-c", "icns", iconset, "-o", resources / "FreeShip26.icns")


def init_bundle(app, build, qt, temp, arch, commit):
    contents = app / "Contents"
    programs = contents / "MacOS"
    resources = contents / "Resources"
    programs.mkdir(parents=True)
    resources.mkdir()
    info = {
        "CFBundleIdentifier": "io.github.jiangshirong.freeship26",
        "CFBundleName": "FreeShip 26",
        "CFBundleDisplayName": "FreeShip 26",
        "CFBundleExecutable": PROGRAMS[0],
        "CFBundlePackageType": "APPL",
        "CFBundleVersion": VERSION,
        "CFBundleShortVersionString": VERSION,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleIconFile": "FreeShip26.icns",
        "LSMinimumSystemVersion": MIN_MACOS,
        "NSHighResolutionCapable": True,
        "CFBundleDocumentTypes": [{
            "CFBundleTypeName": "FreeShip model", "CFBundleTypeExtensions": ["fbm"],
            "CFBundleTypeRole": "Editor", "LSHandlerRank": "Alternate",
        }],
    }
    (contents / "Info.plist").write_bytes(plistlib.dumps(info))
    put_resources(resources, commit, arch)
    create_icon(resources, temp)
    for name in PROGRAMS:
        shutil.copy2(build / name, programs / name)
    # The frozen product resolves its own data next to the executables. Keep
    # the actual data in Resources and provide local, bundle-contained aliases.
    for name in ("bridge", "languages", "build-manifest.json"):
        (programs / name).symlink_to(Path("../Resources") / name)
    # macdeployqt's desktop allowlist deploys Cocoa, not offscreen. The
    # document service invokes FreeShipNativeLinesplan, which requires it.
    offscreen = contents / "PlugIns/platforms/libqoffscreen.dylib"
    offscreen.parent.mkdir(parents=True)
    shutil.copy2(qt / "plugins/platforms" / offscreen.name, offscreen)
    additional_binaries = [*(programs / name for name in PROGRAMS[1:]), offscreen]
    run(qt / "bin/macdeployqt", app, "-no-strip", "-verbose=2",
        *(f"-executable={path}" for path in additional_binaries), timeout=480)
    require((offscreen.parent / "libqcocoa.dylib").is_file(), "Missing bundled Qt Cocoa plugin")
    # Non-app helper processes may not resolve the main bundle's Resources.
    # Qt also searches beside an executable and appends the platforms suffix.
    (programs / "platforms").symlink_to("../PlugIns/platforms")
    # Non-app updater helper must also discover the native Qt TLS backend.
    require((contents / "PlugIns/tls/libqsecuretransportbackend.dylib").is_file(), "Missing bundled macOS TLS plugin")
    (programs / "tls").symlink_to("../PlugIns/tls")
    return contents


def macho_files(app):
    for p in sorted(app.rglob("*")):
        if p.is_symlink():
            require(p.exists() and p.resolve().is_relative_to(app.resolve()), f"Broken or external symlink: {p}")
        elif p.is_file():
            require(p.suffix.lower() not in (".exe", ".dll"), f"Windows executable in app: {p}")
            with p.open("rb") as f:
                if f.read(4) in MACHO_MAGICS:
                    yield p


def dependencies(path, contents):
    output = run("otool", "-L", path, capture=True)
    names = re.findall(r"^\s+(.+?) \(compatibility version ", output, re.M)
    load_commands = run("otool", "-l", path, capture=True)
    rpaths = re.findall(r"\bcmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)", load_commands)
    for rpath in rpaths:
        require(not rpath.startswith("/") or rpath.startswith(("/usr/lib/", "/System/Library/")),
                f"Non-portable rpath {rpath} in {path}")
    for name in names:
        if name.startswith(("/usr/lib/", "/System/Library/")):
            continue
        if name.startswith("@rpath/"):
            dest = contents / "Frameworks" / name.removeprefix("@rpath/")
        elif name.startswith("@loader_path/"):
            dest = path.parent / name.removeprefix("@loader_path/")
        elif name.startswith("@executable_path/"):
            dest = contents / "MacOS" / name.removeprefix("@executable_path/")
        else:
            raise RuntimeError(f"Non-portable dependency {name} in {path}")
        require(dest.is_file() and dest.resolve().is_relative_to(contents.resolve()), f"Missing dependency {name} in {path}")
    return names


def audit_and_sign(app, contents, arch):
    binaries = list(macho_files(app))
    require(all(contents / "MacOS" / n in binaries for n in PROGRAMS), "Not all native programs are Mach-O")
    info = []
    for path in binaries:
        slices = run("xcrun", "lipo", "-archs", path, capture=True).split()
        require(arch in slices, f"{path} has no {arch} slice: {slices}")
        if len(slices) > 1:
            thin = path.with_name(path.name + ".thin")
            run("xcrun", "lipo", path, "-thin", arch, "-output", thin)
            thin.chmod(path.stat().st_mode & 0o777)
            thin.replace(path)
        exact = run("xcrun", "lipo", "-archs", path, capture=True)
        require(exact == arch, f"Wrong Mach-O architecture in {path}: {exact}")
        build = run("xcrun", "vtool", "-show-build", path, capture=True)
        match = re.search(r"\b(?:minos|version) (\d+(?:\.\d+)+)", build)
        require(match and tuple(map(int, match[1].split(".")[:2])) <= (15, 0), f"Unexpected deployment target: {build}")
        info.append({"file": path.relative_to(app).as_posix(), "arch": exact,
                     "min_macos": match[1], "dependencies": dependencies(path, contents)})
        # Signing the bundle's primary executable seals the rest of the app,
        # so first sign *all* nested Mach-Os, including FreeShipUpdater.
        if path != contents / "MacOS" / PROGRAMS[0]:
            run("codesign", "--force", "--sign", "-", path, capture=True)
    for framework in sorted((contents / "Frameworks").glob("*.framework")):
        run("codesign", "--force", "--sign", "-", framework, capture=True)
    run("codesign", "--force", "--sign", "-", app)
    verify_signature(app)
    return info


def verify_signature(app):
    run("codesign", "--verify", "--deep", "--strict", "--verbose=2", app)


def run_checks(build, contents, sample, evidence, temp):
    # Do not let tests accidentally depend on a Qt/Python shell from the build.
    env = {k: v for k, v in os.environ.items() if not k.startswith(("DYLD_", "QT_", "QML"))}
    for key, path in {"HOME": temp / "test-home", "XDG_CONFIG_HOME": temp / "test-config"}.items():
        path.mkdir()
        env[key] = str(path)
    (build / "languages").symlink_to(contents / "Resources/languages")
    results = {}

    def verify(name, fn):
        try:
            results[name] = {"status": "pass", **fn()}
        except (RuntimeError, OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
            results[name] = {"status": "fail", "error": str(exc)}
        print(f"{name}: {json.dumps(results[name], ensure_ascii=False)}", flush=True)

    def checked_report(path):
        data = json.loads(path.read_bytes())
        require(data.get("status") == "pass" and data.get("cases"), f"Test did not pass: {path.name}")
        return {"cases": len(data["cases"])}

    def fbm_codec():
        path = evidence / "fbm-codec.json"
        run(build / "qt_document_codec_tests", "--output", path, "--model", sample, env=env, timeout=150)
        return checked_report(path)

    def languages():
        directory = evidence / "ui-language"
        run(build / "qt_ui_language_tests", directory, env={**env, "QT_QPA_PLATFORM": "offscreen"})
        return checked_report(directory / "report.json")

    programs = contents / "MacOS"

    def native_worker_boot():
        # With no workload, this worker returns 3 after QGuiApplication has
        # started. An abort/missing platform plugin is not that expected code.
        with (evidence / "native-worker-startup.txt").open("w", encoding="utf-8") as log:
            result = subprocess.run([programs / "FreeShipNativeLinesplan"],
                                    env={**env, "QT_DEBUG_PLUGINS": "1"}, cwd=temp,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=45)
        require(result.returncode == 3, f"Worker failed before entering its command handler: {result.returncode}")
        text = (evidence / "native-worker-startup.txt").read_text(encoding="utf-8", errors="replace")
        require(any(str(contents / "PlugIns/platforms/libqoffscreen.dylib") in line and "loaded library" in line
                    or str(contents / "MacOS/platforms/libqoffscreen.dylib") in line and "loaded library" in line
                    for line in text.splitlines()), "Worker did not load the packaged offscreen platform plugin")
        return {"platform": "offscreen"}

    def native_model_load():
        out = temp / "rpc-output"
        out.mkdir()
        docid = str(uuid.uuid4())
        messages = [{"id": 1, "method": "load", "path": str(sample), "output": str(out),
                     "document_id": docid, "native_linesplan": True}, {"id": 2, "method": "quit"}]
        with (evidence / "document-service-stderr.txt").open("w", encoding="utf-8") as stderr:
            output = run(programs / "FreeShipDocumentService", "--serve", capture=True, stderr=stderr,
                         input="\n".join(json.dumps(r) for r in messages) + "\n", env=env, timeout=120)
        responses = [json.loads(line) for line in output.splitlines() if line.strip()]
        dump(evidence / "document-service.json", responses)
        require(len(responses) == 2 and all(r.get("ok") is True for r in responses)
                and responses[0].get("document_id") == docid and responses[0].get("document_engine") == "cpp-document"
                and responses[0].get("adapter_requests") == 0 and responses[0].get("revision") == 1,
                f"Packaged native sample-model load failed: {responses}")
        return {"sample": sample.name}

    def cocoa_startup():
        # Automated Qt event-loop probe, not a Finder/Gatekeeper or manual test.
        window_report = evidence / "window-startup"
        with (evidence / "window-startup-log.txt").open("w", encoding="utf-8") as log:
            run(programs / "FreeShipQt", "--verify-window-startup", window_report,
                env={**env, "QT_QPA_PLATFORM": "cocoa"}, cwd=temp, stdout=log, stderr=subprocess.STDOUT, timeout=75)
        window = json.loads((window_report / "report.json").read_bytes())
        require(any(c["id"] == "private-startup-observed" and c["status"] == "pass" for c in window["cases"]),
                "Packaged Cocoa startup probe did not reach its event loop")
        return {"frames": len(window["frames"])}

    verify("fbm_codec", fbm_codec)
    verify("ui_language", languages)
    verify("packaged_native_worker_boot", native_worker_boot)
    verify("packaged_native_model_load", native_model_load)
    verify("automated_cocoa_window_startup", cocoa_startup)
    return results


def image_instructions(path, arch, commit):
    chip = "Apple Silicon (M-series)" if arch == "arm64" else "Intel Mac"
    path.write_text(
        f"FreeShip 26 {VERSION} — macOS 测试版 / test build\n"
        f"{chip} · {arch} · macOS {MIN_MACOS}+\n\n"
        "打开 DMG，将 FreeShip 26 拖入「应用程序」，再从那里打开。不要直接在磁盘映像里运行。\n"
        "「关于 → 检查更新」使用独立 macOS 测试通道；有新版时手动下载，确认后重启安装。\n"
        "仅临时签名，未经 Apple 公证。只在确认来源可信时打开。可复制附带的 FBM 示例测试。\n\n"
        "Open the DMG, drag FreeShip 26 to Applications and launch the installed app.\n"
        "About → Check for Updates uses a separate macOS test feed. Downloads and installs require confirmation.\n"
        "Ad hoc signed, not Apple notarized. Only open if you trust the source. Test with a copy of the sample.\n"
        "Apple's guidance: https://support.apple.com/102445\n\n"
        f"Source, license and packaging: https://github.com/jiangshirong/FreeShip-26/tree/{commit}\n",
        encoding="utf-8",
    )


def export_image(stage, dmg, folder, arch):
    stage.joinpath("Applications").symlink_to("/Applications", target_is_directory=True)
    chip = "Apple Silicon" if arch == "arm64" else "Intel"
    run("hdiutil", "create", "-fs", "HFS+", "-format", "UDZO", "-srcfolder", stage,
        "-volname", f"FreeShip 26 {VERSION} ({chip})", dmg, timeout=240)
    run("hdiutil", "verify", dmg, timeout=180)
    mount = folder / "mounted-image"
    mount.mkdir()
    mount = mount.resolve()
    run("hdiutil", "attach", "-readonly", "-nobrowse", "-noautoopen", "-mountpoint", mount, dmg, timeout=90)
    try:
        require(os.readlink(mount / "Applications") == "/Applications", "Wrong Applications link")
        require({p.name for p in mount.iterdir() if not p.name.startswith(".")} ==
                {"FreeShip 26.app", "Applications", "README.txt", "box-loading-conditions.fbm"}, "Unexpected DMG root items")
        require(os.statvfs(mount).f_flag & os.ST_RDONLY, "DMG was not mounted read-only")
        exported = folder / "Applications" / "FreeShip 26.app"
        exported.parent.mkdir()
        run("ditto", mount / exported.name, exported, timeout=120)
        sample = exported.parent / "box-loading-conditions.fbm"
        shutil.copy2(mount / sample.name, sample)
    finally:
        run("hdiutil", "detach", mount, timeout=45)
    verify_signature(exported)
    return exported, sample


def package():
    sys.modules.setdefault("macos_candidate", sys.modules[__name__])
    from macos_updater_tests import distribution_test, production_checks
    record = verify_source()
    arch = os.environ["FS26_ARCH"]
    require(arch in ("arm64", "x86_64") and run("uname", "-m", capture=True) == arch, "Wrong native runner")
    qt = Path(os.environ["FS26_QT"])
    tmp = Path(os.environ["RUNNER_TEMP"])
    build = tmp / "fs26-build"
    delivery = tmp / "fs26-delivery"
    evidence = delivery / "evidence"
    evidence.mkdir(parents=True)
    commit = run("git", "-C", ROOT, "rev-parse", "HEAD", capture=True)
    name = f"FreeShip-26-{VERSION}-macos-{arch}"
    with tempfile.TemporaryDirectory(prefix="fs26-mac-", dir=tmp) as td:
        folder = Path(td)
        payload = folder / name
        payload.mkdir()
        app = payload / "FreeShip 26.app"
        contents = init_bundle(app, build, qt, folder, arch, commit)
        binaries = audit_and_sign(app, contents, arch)
        dump(evidence / "macho-inventory.json", binaries)
        sample = payload / "box-loading-conditions.fbm"
        shutil.copyfile(SOURCE / "examples" / sample.name, sample)
        image_instructions(payload / "README.txt", arch, commit)
        bundle_dmg = delivery / f"{name}.dmg"
        exported, sample_copy = export_image(payload, bundle_dmg, folder, arch)
        exported_binaries = list(macho_files(exported))
        require(len(exported_binaries) == len(binaries), "Mach-O inventory changed inside the final image")
        for path in exported_binaries:
            require(run("xcrun", "lipo", "-archs", path, capture=True) == arch and
                    digest(path) == digest(app / path.relative_to(exported)), f"DMG Mach-O mismatch: {path}")
        require(not (exported / "Contents/MacOS/FreeShipUpdaterTest").exists(), "Test updater must never ship")
        checksum = digest(bundle_dmg)
        (evidence / "SHA256SUMS.txt").write_text(f"{checksum}  {bundle_dmg.name}\n", encoding="ascii")
        checks = run_checks(build, exported / "Contents", sample_copy, evidence, folder)
        receipts = {"status": "fail", "version": VERSION, "architecture": arch, "deployment_target": MIN_MACOS,
            "qt_version": QT_VERSION, "runner_macos": run("sw_vers", "-productVersion", capture=True),
            "runner_os_build": run("sw_vers", "-buildVersion", capture=True),
            "xcode": run("xcodebuild", "-version", capture=True), "git_commit": commit,
            "snapshot_version": record["version"], "frozen_source_files_verified": record["source_entries"],
            "frozen_zip_sha256": SOURCE_ZIP_SHA256, "frozen_manifest_sha256": MANIFEST_SHA256,
            "package": bundle_dmg.name, "package_bytes": bundle_dmg.stat().st_size, "sha256": checksum,
            "ci_compatibility_overlay": json.loads((tmp / "fs26-compatibility.json").read_bytes()),
            "macos_updater_overlay": json.loads((tmp / "fs26-macos-update-overlay.json").read_bytes()),
            "mach_o_files_checked": len(binaries), "ad_hoc_signature_verification": "pass",
            "developer_id_signed": False, "apple_notarized": False,
            "update_channel_url": f"{FEED_BASE}/{arch}/latest.json", "update_channel_configured": True,
            "dmg_readonly_mount_and_applications_link": "pass", "languages_bundled": LANGUAGES, "checks": checks,
            "finder_gatekeeper_acceptance": "not_run", "manual_macos_desktop_acceptance": "not_run",
        }
        report = evidence / "verification.json"
        dump(report, receipts)
        if all(result["status"] == "pass" for result in checks.values()):
            receipts["live_update_checks"] = production_checks(build, exported, evidence, folder)
            dump(report, receipts)
            receipts["update_install_checks"] = distribution_test(build, exported, bundle_dmg, evidence, folder, arch)
            receipts["status"] = "pass"
            dump(report, receipts)
        require(receipts["status"] == "pass", "Package probes did not pass. See uploaded validation evidence.")
        print(f"Verified {bundle_dmg.name}: {checksum}", flush=True)


if __name__ == "__main__":
    actions = {"verify-source": verify_source, "stage-source": stage_source, "package": package}
    require(len(sys.argv) == 2 and sys.argv[1] in actions, "Use verify-source, stage-source, or package")
    actions[sys.argv[1]]()
