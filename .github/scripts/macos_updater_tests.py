"""Exercise the native update transport and transactional install on a disposable Mac app.

The signed input is the final DMG for this runner, not a mock installer. The
old .app and loopback signing key are created only inside RUNNER_TEMP.
"""
import base64
import contextlib
from datetime import datetime, timedelta, timezone
import http.server
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import threading
import time

from macos_candidate import VERSION, MIN_MACOS, digest, dump, require, run, verify_signature


def assert_case(cases, name, value):
    cases[name] = "pass" if value else "fail"
    require(value, f"macOS update test failed: {name}")


def clean_env(home):
    home.mkdir(exist_ok=True)
    return {**{k: v for k, v in os.environ.items() if not k.startswith(("DYLD_", "QT_", "QML"))},
            "HOME": str(home), "XDG_CONFIG_HOME": str(home / "config")}


def production_checks(build, installed, evidence, temp):
    programs = installed / "Contents/MacOS"
    helper = programs / "FreeShipUpdater"
    env = clean_env(temp / "update-test-home")
    command = [helper, "--check", "--report", str(evidence / "live-channel-check.json")]
    run(*command, env=env, timeout=30)
    report = json.loads((evidence / "live-channel-check.json").read_bytes())
    require(report.get("status") == "current" and report.get("current_version") == report.get("version") == VERSION,
            f"The signed HTTPS candidate channel is not usable: {report}")
    denied = evidence / "live-helper-rejects-override.json"
    result = subprocess.run([helper, "--check", "--report", denied, "--endpoint", "http://127.0.0.1:1/"], env=env, timeout=15)
    require(result.returncode != 0 and json.loads(denied.read_bytes()).get("status") == "error",
            "Shipped updater must reject endpoint overrides")
    test_bin = temp / "update-ui-bin"
    test_bin.mkdir()
    shutil.copy2(build / "qt_update_about_tests", test_bin / "qt_update_about_tests")
    (test_bin / helper.name).symlink_to(helper)
    for name in ("bridge", "languages", "build-manifest.json"):
        (test_bin / name).symlink_to(programs / name)
    run(test_bin / "qt_update_about_tests", "--live-macos-check", evidence / "update-about-ui", env={**env, "QT_QPA_PLATFORM": "cocoa"}, timeout=30)
    return {"https_signed_manifest": "pass", "shipped_helper_rejects_runtime_endpoint_override": "pass",
            "about_button_calls_shipped_helper": "pass"}


class Fixture(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        try:
            if self.path == "/latest.json":
                data = self.server.envelope
                self.send_response(200)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            elif self.path == "/redirect":
                self.send_response(302)
                self.send_header("Location", "/download.dmg")
                self.send_header("Content-Length", "0")
                self.end_headers()
            elif self.path == "/download.dmg":
                self.send_response(200)
                self.send_header("Content-Length", str(self.server.image.stat().st_size))
                self.end_headers()
                with self.server.image.open("rb") as source:
                    if self.server.corrupt:
                        self.wfile.write(bytes([source.read(1)[0] ^ 1]))
                    shutil.copyfileobj(source, self.wfile, length=256 * 1024)
            else:
                self.send_error(404)
        except (BrokenPipeError, ConnectionResetError):
            pass


def signed(payload, key):
    data = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode()
    sig = subprocess.run(["/usr/bin/openssl", "dgst", "-sha256", "-sign", key], input=data,
                         stdout=subprocess.PIPE, check=True, timeout=15).stdout
    return json.dumps({"payload": base64.b64encode(data).decode(), "signature": base64.b64encode(sig).decode()}).encode()


def session_started(app, timeout=18):
    prefix = str(app / "Contents/MacOS/FreeShipQt")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = run("/bin/ps", "-axww", "-o", "pid=,command=", capture=True)
        for line in data.splitlines():
            pid, command = line.strip().split(maxsplit=1)
            if command == prefix or command.startswith(prefix + " "):
                return int(pid)
        time.sleep(0.25)
    return None


def distribution_test(build, installed, dmg, evidence, temp, arch):
    cases = {}
    env = clean_env(temp / "update-fixture-home")
    temporary = temp / "update-fixture"
    temporary.mkdir()
    live = temporary / installed.name
    run("ditto", installed, live)
    helper = live / "Contents/MacOS/FreeShipUpdater"
    shutil.copy2(build / "FreeShipUpdaterTest", helper)
    run("codesign", "--force", "--sign", "-", helper)
    manifest_path = live / "Contents/Resources/build-manifest.json"
    manifest = json.loads(manifest_path.read_bytes())
    original = "0.1.20"
    manifest["release_version"] = original
    dump(manifest_path, manifest)
    plist_path = live / "Contents/Info.plist"
    value = plistlib.loads(plist_path.read_bytes())
    value["CFBundleVersion"] = value["CFBundleShortVersionString"] = original
    plist_path.write_bytes(plistlib.dumps(value))
    cache = temporary / "cache"
    test_config = live / "Contents/Resources/macos-update-fixture.json"
    key = temporary / "fixture-key.pem"
    run("/usr/bin/openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:3072", "-out", key,
        stderr=subprocess.DEVNULL)
    public = subprocess.run(["/usr/bin/openssl", "rsa", "-in", key, "-RSAPublicKey_out", "-outform", "DER"],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True, timeout=15).stdout
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture) as srv:
        srv.image = dmg
        srv.corrupt = False
        origin = f"http://127.0.0.1:{srv.server_port}"
        endpoint = origin + "/latest.json"
        fixture = {"endpoint": endpoint, "public_key_der": base64.b64encode(public).decode(), "cache": str(cache), "relaunch": False}
        dump(test_config, fixture)
        run("codesign", "--force", "--sign", "-", live)
        initial_main_sha = digest(live / "Contents/MacOS/FreeShipQt")
        version = lambda: json.loads(manifest_path.read_bytes())["release_version"]
        conf = json.loads((Path(__file__).resolve().parents[2] / "FreeShip-Qt/tools/updates/channel.json").read_bytes())
        payload = {"schema": "fs26-macos-dmg-v1", "channel": endpoint, "family": conf["family"], "arch": arch,
                   "version": VERSION, "minimum_macos": MIN_MACOS,
                   "expires": (datetime.now(timezone.utc) + timedelta(hours=1)).strftime("%Y-%m-%dT%H:%M:%SZ"),
                   "package": {"url": origin + "/redirect", "size": dmg.stat().st_size, "sha256": digest(dmg)}}
        srv.envelope = signed(payload, key)
        thread = threading.Thread(target=srv.serve_forever, daemon=True)
        thread.start()
        log_dir = evidence / "updater-tests"
        log_dir.mkdir()

        def call(name, mode, wanted, parent=None):
            report = log_dir / f"{name}.json"
            args = [helper, mode, "--report", report]
            if parent is not None:
                args += ["--parent", str(parent.pid)]
            with (log_dir / f"{name}.log").open("wb") as log:
                if parent is None:
                    result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=120)
                    code = result.returncode
                else:
                    with subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=env) as p:
                        # Reap the dummy parent; the helper must wait for its exit.
                        time.sleep(.3)
                        require(p.poll() is None and version() == original, "Updater changed a live app before its parent exited")
                        parent.wait(timeout=10)
                        try: code = p.wait(timeout=160)
                        except subprocess.TimeoutExpired:
                            p.kill(); p.wait(); raise
            require(report.is_file(), f"{name} did not produce a report: {code}")
            data = json.loads(report.read_bytes())
            assert_case(cases, name, data.get("status") == wanted and (code == 0) == (wanted not in ("error", "install_failed")))
            return data

        def install_attempt(name, wanted):
            with subprocess.Popen(["/bin/sleep", "1"], env=env) as parent:
                return call(name, "--install", wanted, parent)

        try:
            call("newer-version-found", "--check", "available")
            wrong = "x86_64" if arch == "arm64" else "arm64"
            for name, edits, result in (
                ("wrong-architecture-rejected", {"arch": wrong}, "error"),
                ("wrong-channel-rejected", {"channel": endpoint + "-elsewhere"}, "error"),
                ("wrong-product-rejected", {"family": "different-product"}, "error"),
                ("expired-feed-rejected", {"expires": "2000-01-01T00:00:00Z"}, "error"),
                ("higher-os-required", {"minimum_macos": "99.0"}, "error"),
                ("downgrade-not-offered", {"version": "0.1.19"}, "current"),
            ):
                srv.envelope = signed({**payload, **edits}, key)
                call(name, "--check", result)
            envelope = json.loads(signed(payload, key))
            signature = base64.b64decode(envelope["signature"])
            envelope["signature"] = base64.b64encode(bytes([signature[0] ^ 1]) + signature[1:]).decode()
            srv.envelope = json.dumps(envelope).encode()
            call("bad-signature-rejected", "--check", "error")
            srv.envelope = signed(payload, key)
            srv.corrupt = True
            call("modified-download-rejected", "--download", "error")
            assert_case(cases, "bad-download-not-ready", not (cache / "ready.json").exists() and version() == original)
            srv.corrupt = False
            call("download-final-dmg-and-check-digest", "--download", "ready")
            call("verified-download-reused", "--check", "ready")
            with (cache / "update.dmg").open("r+b") as f:
                first = f.read(1); f.seek(0); f.write(bytes([first[0] ^ 1]))
            invalid = install_attempt("modified-cache-cannot-install", "install_failed")
            assert_case(cases, "install-report-rejected-the-dmg-digest", invalid.get("error") == "更新安装包完整校验失败")
            assert_case(cases, "failed-install-preserved-old-app", version() == original and
                        digest(live / "Contents/MacOS/FreeShipQt") == initial_main_sha)
            verify_signature(live)
            call("redownload-after-failure", "--download", "ready")
            dump(test_config, {**fixture, "relaunch": True})
            run("codesign", "--force", "--sign", "-", live)
            install_attempt("wait-for-exit-then-install-verified-app", "installed")
            assert_case(cases, "installed-app-version-updated", version() == VERSION and not test_config.exists())
            for name in ("FreeShipQt", "FreeShipUpdater"):
                assert_case(cases, "installed-" + name + "-matches-dmg", digest(live / "Contents/MacOS" / name) ==
                            digest(installed / "Contents/MacOS" / name))
            verify_signature(live)
            # Verify Launch Services actually started the new app, not just
            # that the open command returned success. The user has not tested this desktop.
            pid = session_started(live)
            try:
                assert_case(cases, "new-app-process-relaunched", pid is not None)
            finally:
                if pid is not None:
                    with contextlib.suppress(ProcessLookupError):
                        os.kill(pid, signal.SIGTERM)
            return cases
        finally:
            srv.shutdown()
            thread.join(timeout=5)
            key.unlink(missing_ok=True)
            dump(evidence / "update-chain.json", cases)
