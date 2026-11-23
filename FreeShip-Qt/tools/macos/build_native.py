"""Build FS26 in a private, Qt-dependent development directory on Apple Silicon."""
import os
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/macos-arm64-native"
PROGRAMS = ("FreeShipQt", "FreeShipDocumentService", "FreeShipNativeLinesplan", "FreeShipGeometryExport")


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("Run on Apple Silicon macOS using a native arm64 shell/Python.")
    if not (ROOT.parent / "FreeShip-Tests/qt/fbm_codec_tests.cpp").is_file():
        raise SystemExit("The matching FreeShip-Tests/qt source tree must be beside FreeShip-Qt.")
    # Use only known data subdirectories, never a Windows executable or install manifest.
    bridge = next((p for p in (ROOT / "runtime-data/bridge", ROOT / "dist/bridge")
                   if (p / "Themes/StudioLight/icons/manifest.json").is_file()
                   and (p / "locale").is_dir()), None)
    if not bridge:
        raise SystemExit("Missing development resources: provide runtime-data/bridge/{Themes,locale}.")
    clang = subprocess.check_output(["xcrun", "--find", "clang++"], text=True).strip()
    subprocess.run(["cmake", "-S", ROOT, "-B", OUT, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
                    "-DCMAKE_MACOSX_BUNDLE=OFF", f"-DCMAKE_CXX_COMPILER={clang}"], check=True)
    subprocess.run(["cmake", "--build", OUT, "--parallel", os.environ.get("BUILD_JOBS", "3"),
                    "--target", *PROGRAMS, "qt_document_codec_tests"], check=True)
    for target in (*PROGRAMS, "qt_document_codec_tests"):
        subprocess.run(["xcrun", "lipo", "-verify_arch", "arm64", OUT / target], check=True)
    (OUT / "bridge").mkdir(exist_ok=True)
    resources = {OUT / "languages": ROOT / "src/app/languages"}
    for name in ("Themes", "locale", "Languages"):
        if (bridge / name).is_dir():
            resources[OUT / "bridge" / name] = bridge / name
    for link, source in resources.items():
        if link.is_symlink() and link.resolve() == source.resolve():
            continue
        link.symlink_to(source, target_is_directory=True)
    subprocess.run([OUT / "qt_document_codec_tests", "--output", OUT / "fbm-codec.json",
                    "--model", ROOT / "examples/box-loading-conditions.fbm"], cwd=OUT, check=True)
    print("Development build and FBM codec check passed:", OUT)


if __name__ == "__main__":
    main()
