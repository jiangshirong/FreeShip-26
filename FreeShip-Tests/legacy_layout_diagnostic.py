"""Read-only issue 17 investigation of a legacy_compatibility.py corpus report.

This is a diagnostic for the current record schema, not a format detector or
acceptance test. EOF matching alone cannot distinguish an old layout from a
truncated current-layout file. It never patches, opens in FreeShip, or rewrites
the model files. The native offset trace is required; no byte-pattern scanning
is used to guess where model geometry ends.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct


# Ordinals and sizes from FreeVersionUnit.pas and the current FreeTypes records.
# These are explicitly a diagnostic hypothesis for the records after Planing.
SUFFIX_RECORDS = (
    (24, "Rvrs", 68), (25, "Hollen", 176), (26, "Task4", 64),
    (30, "Task5", 24), (33, "Oortmer", 176), (35, "Fung", 176),
    (38, "Maneuv", 80), (38, "HydrodynTask1", 48),
    (40, "RBHS", 212), (40, "MH", 212),
)
PLANING_FIELDS = (
    "Draft", "Lwl", "Bwl", "Cp", "Displacement", "LCB", "WettedSurface",
    "At_Ax", "EntranceAngle", "Sa", "Caa", "Angle", "K",
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inspect_case(case: dict, report_dir: Path) -> dict:
    result = {"id": case["id"], "native_status": case["status"],
              "loaded": case.get("loaded"), "roundtrip": case.get("roundtrip"),
              "status": "review", "fixture": case["fixture"]}
    trace_path = report_dir / (case["id"].removeprefix("17.") + "-offsets.txt")
    trace_bytes = trace_path.read_bytes()
    trace = trace_bytes.decode("utf-8-sig")
    result["offset_trace_sha256"] = digest(trace_bytes)
    version = re.search(r"version end=\d+ ordinal=(\d+)", trace)
    if not version:
        result["reason"] = "No native binary version trace (for example FTM)."
        return result
    ordinal = int(version[1])
    result["file_version_ordinal"] = ordinal
    raw = Path(case["fixture"]).read_bytes()
    result["fixture_sha256"] = digest(raw)
    if result["fixture_sha256"] != case["fixture_sha256"]:
        raise ValueError("Fixture changed since native report: " + case["fixture"])
    failure = re.search(r"大小 (\d+)、位置 (\d+).*文件大小 (\d+)", case.get("error", ""))
    if failure:
        size, position, count = map(int, failure.groups())
        result["failure_required_bytes"] = size
        result["failure_available_bytes"] = count - position
        result["failure_shortfall"] = position + size - count
    start = re.search(r"LoadTFreePlaningResistanceData start=(\d+)", trace)
    if not start:
        result["reason"] = "Native reader did not reach a Planing record."
        return result
    start = int(start[1])
    result["planing_start"] = start
    result["fields_in_current_layout"] = []
    for index, name in enumerate(PLANING_FIELDS):
        offset = start + index * 4
        blob = raw[offset:offset + 4]
        value = struct.unpack("<f", blob)[0] if len(blob) == 4 else None
        result["fields_in_current_layout"].append({
            "name": name, "offset": offset, "hex": blob.hex(),
            "value": value if value is None or math.isfinite(value) else str(value),
        })
    suffix = [(name, size) for minimum, name, size in SUFFIX_RECORDS if ordinal >= minimum]
    suffix_size = sum(size for _, size in suffix)
    result["assumed_suffix_records"] = suffix
    result["layout_candidates"] = []
    for float_count in (9, 11, 13):
        width = float_count * 4 + 4  # Boolean plus Delphi record tail alignment.
        bool_offset = start + float_count * 4
        expected_end = start + width + suffix_size
        result["layout_candidates"].append({
            "float_count": float_count, "record_width": width,
            "extract_offset": bool_offset,
            "extract_byte": raw[bool_offset] if bool_offset < len(raw) else None,
            "bool_and_padding_hex": raw[bool_offset:bool_offset + 4].hex(),
            "expected_end": expected_end, "actual_end": len(raw),
            "end_difference": len(raw) - expected_end,
            "matches_eof": expected_end == len(raw),
        })
    result["reason"] = "Candidate sizes are hypotheses; an EOF match is not proof of a historical layout."
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True, help="Native corpus report.json")
    parser.add_argument("--output", type=Path, required=True, help="New diagnostic JSON file")
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit("Output must be a new file")
    input_bytes = args.report.read_bytes()
    source = json.loads(input_bytes)
    cases = [inspect_case(case, args.report.parent) for case in source["cases"]]
    groups = Counter((c.get("file_version_ordinal"), c["native_status"],
                      c.get("failure_shortfall")) for c in cases)
    report = {
        "status": "review", "diagnostic_only": True,
        "input_report_sha256": digest(input_bytes),
        "diagnostic_code_sha256": digest(Path(__file__).read_bytes()),
        "limitations": [
            "Current-schema suffix sizes are an explicit hypothesis, not a historical specification.",
            "A truncated file can have the same remaining size as an older layout.",
            "Same file-version ordinal can contain different record layouts; version alone is insufficient.",
            "No production code changes and no native import or round-trip verification are performed here.",
        ],
        "groups": [{"version": version, "native_status": status, "shortfall": shortfall, "count": count}
                   for (version, status, shortfall), count in groups.items()],
        "cases": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    print(json.dumps({"status": "review", "cases": len(cases), "groups": report["groups"]}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
