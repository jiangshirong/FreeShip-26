"""One-off protocol probe; retained for diagnosing the original ADD_MASS binary.

Usage: python maintenance/probe_addmass.py --input INA --solver ADD_MASS.EXE --output DIR
Run only on an isolated copy of a generated input. Regression coverage lives in
qt_added_mass.py; this probe captures otherwise undocumented raw solver output.
"""
import argparse
import json
import os
import math
import pathlib
import subprocess
import time

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=pathlib.Path)
    p.add_argument("--ellipsoid", action="store_true")
    for name in ("solver", "output"):
        p.add_argument("--" + name, required=True, type=pathlib.Path)
    p.add_argument("--panel-type", type=int, choices=(1, 2))
    p.add_argument("--convert-unit", choices=("native", "little_endian", "big_endian"))
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    if list(a.output.iterdir()):
        raise SystemExit("Probe output must be empty")
    if a.ellipsoid:
        rows = ["Ellipsoid L=10 B=4 T=1.5", "1025 400 100 .2 1 10000 0 10 0 1",
                "0 0 0 0 0 0 0 0 0 0", "10 1.5", "21 1. 0.", "11"]
        for i in range(21):
            rows.append(" ".join(format(2*math.sqrt(max(0,1-(-1+i/10)**2))*
                                     math.sqrt(max(0,1-(-1+j/10)**2)), ".17g") for j in range(11)))
        raw = ("\r\n".join(rows)+"\r\n\\*\r\n").encode("ascii")
    else:
        if not a.input: p.error("Use --input or --ellipsoid")
        raw = a.input.read_bytes()
    if a.panel_type:
        lines = raw.decode("utf-8").splitlines()
        fields = lines[1].split()
        fields[4] = str(a.panel_type)
        lines[1] = " ".join(fields)
        raw = ("\r\n".join(lines) + "\r\n").encode("utf-8")
    (a.output / "INA").write_bytes(raw)
    (a.output / "input-copy.txt").write_bytes(raw)
    started = time.monotonic()
    env = dict(os.environ)
    if a.convert_unit:
        env["GFORTRAN_CONVERT_UNIT"] = a.convert_unit
    try:
        r = subprocess.run([str(a.solver.resolve())], cwd=a.output, input=b"\r\n",
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           env=env, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), timeout=110)
        (a.output / "solver.log").write_bytes(r.stdout)
        result = {"exit_code": r.returncode, "seconds": time.monotonic()-started}
    except subprocess.TimeoutExpired as e:
        (a.output / "solver.log").write_bytes(e.stdout or b"")
        result = {"timeout": True, "seconds": time.monotonic()-started}
    (a.output / "probe.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result))

if __name__ == "__main__":
    main()
