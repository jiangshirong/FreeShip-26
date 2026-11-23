"""Compile the Qt-independent FS26 geometry libraries; this does not build a Mac app."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/macos-arm64-core"
GROUPS = {"native_geometry": ("plane", "spline"),
          "native_subdivision": ("subdivision",),
          "native_sections": ("sections", "linesplan")}
TARGET = "aarch64-macos.15.0"


def check_object(data):
    magic, cpu, _, kind, ncmds, *_ = struct.unpack_from("<8I", data)
    if (magic, cpu, kind) != (0xfeedfacf, 0x0100000c, 1):
        raise ValueError("Not an arm64 Mach-O object")
    offset = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", data, offset)
        if cmd == 0x32 and struct.unpack_from("<II", data, offset + 8) == (1, 15 << 16):
            return
        offset += size
    raise ValueError("Missing macOS 15.0 deployment target")


def inputs():
    files = list((ROOT / "src/native").glob("*.h"))
    files += [ROOT / f"src/native/{n}.cpp" for names in GROUPS.values() for n in names]
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(files)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--zig", default="zig", help="Zig executable (tested with 0.14.1)")
    zig = p.parse_args().zig
    if Path(zig).is_file():
        zig = str(Path(zig).resolve())
    OUT.mkdir(parents=True, exist_ok=True)
    before = inputs()
    report = {"target": TARGET, "zig": subprocess.check_output([zig, "version"], text=True).strip(),
              "source_sha256": before, "libraries": {}, "executed_on_macos": False}
    with tempfile.TemporaryDirectory(dir=OUT) as tmp:
        scratch = Path(tmp)
        for group, names in GROUPS.items():
            members = [name + ".o" for name in names]
            for name, member in zip(names, members):
                subprocess.run([zig, "c++", "-target", TARGET, "-std=c++17", "-O2",
                                "-fno-fast-math", "-ffp-contract=off", "-c",
                                ROOT / f"src/native/{name}.cpp", "-o", member], cwd=tmp, check=True)
            archive = f"lib{group}.a"
            subprocess.run([zig, "ar", "rcs", archive, *members], cwd=tmp, check=True)
            actual = subprocess.check_output([zig, "ar", "t", archive], cwd=tmp, text=True).splitlines()
            if actual != members:
                raise ValueError(f"Unexpected members in {archive}: {actual}")
            for member in members:
                check_object(subprocess.check_output([zig, "ar", "p", archive, member], cwd=tmp))
            report["libraries"][archive] = {"members": actual, "sha256":
                                            hashlib.sha256((scratch / archive).read_bytes()).hexdigest()}
        if inputs() != before:
            raise RuntimeError("Sources changed during compilation; rerun the build")
        (scratch / "core-build.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        for name in [*report["libraries"], "core-build.json"]:
            os.replace(scratch / name, OUT / name)
    print("Verified 3 arm64 archives (5 objects); no executable was run:", OUT)


if __name__ == "__main__":
    main()
