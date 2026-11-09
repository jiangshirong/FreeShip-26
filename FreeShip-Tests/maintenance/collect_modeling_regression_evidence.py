"""Preserve compact failure evidence before retiring superseded 2026-11-02 output.

Run from any directory: python maintenance/collect_modeling_regression_evidence.py
This is report maintenance, not a test: it does not execute product behavior.
Reproducible assertions remain in qt_modeling*, qt_advanced and qt_auxiliary.
"""
import argparse
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
reports = root / 'reports'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rotation-report', help='Optional superseded rotation regression directory name')
args = parser.parse_args()
record = {
    'purpose': 'Retain failures and provenance; remove only superseded generated outputs separately.',
    'product_fixes': [
        'Rectangular grid allocation, endpoints and fixed-axis origin are checked independently.',
        'Persist group locks through member locks; validate FBM text encoding before mutation.',
        'Detach/rebind control curves during point collapse; split curves before point deletion.',
        'Copy every layer participation flag, not only AssignProperties fields.',
        'Use representable stored layer names during auto-grouping and undo.',
        'Pivot rotation uses signed addition; original AddPoint clamps negative Y.',
    ],
    'test_corrections': [
        'Ordered point selection must use ordered callbacks, not QSet iteration.',
        'FBM record IDs change on reload; constraints compare exact target coordinates.',
        'Normal pointer traversal order is unspecified; compare exact vector multisets.',
        'Box integral retains Single arithmetic; bound rounding over 82 samples explicitly.',
    ],
    'runs': [],
}
for folder in sorted(reports.iterdir()):
    if not folder.name.startswith(('qt-modeling-', 'qt-advanced-modeling-')):
        continue
    path = folder / 'report.json'
    if not path.is_file():
        continue
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    failed = [case for case in data.get('cases', []) if case.get('status') in ('fail', 'error')]
    if not failed:
        continue
    record['runs'].append({
        'retired_run': folder.name,
        'report_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'counts': data.get('counts'),
        'failures': failed,
        'test_sources_sha256': data.get('test_sources_sha256'),
        'bridge_build': data.get('bridge_build'),
    })
rotation = reports / args.rotation_report / 'model-1/whole-model' if args.rotation_report else None
if rotation and rotation.is_dir():
    record['rotation_negative_control'] = {
        'expected': json.loads((rotation / 'marker-math/state.json').read_text())['points'],
        'before_fix': json.loads((rotation / 'rotated-markers-inspect/state.json').read_text())['markers'],
        'regressions': ['whole.rotate90-analytic', 'whole.checked-markers-rotate-exactly',
                        'extra.rotation-pivot-signed-coordinates'],
    }
destination = Path(__file__).with_name('modeling-fixed-regressions-20261102.json')
if not record['runs']:
    raise SystemExit('No source failure reports found; existing evidence left intact.')
destination.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'path': str(destination), 'failed_runs': len(record['runs'])}))
