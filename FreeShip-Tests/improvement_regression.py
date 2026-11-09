"""Run the reusable upstream-fix probes; preserve failures and untested coverage."""
import argparse
import ast
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
from pathlib import Path
import subprocess
import sys
import time

import freeship_ui_test as common

ROOT = Path(__file__).resolve().parent
GROUPS = {
    'buffer': ('buffer_safety.py', ('03', '04')),
    'save': ('save_safety.py', ('01',)),
    'load': ('file_safety.py', ('02', '04', '08', '09')),
    'units': ('units_conversion.py', ('05', '06')),
    'hydro': ('hydro_corrections.py', ('07', '12', '15')),
    'hydro-search': ('hydro_search.py', ('07',)),
    'undo-background': ('undo_background_safety.py', ('10', '14')),
    'process': ('process_safety.py', ('11',)),
    'process-callers': ('process_callers.py', ('11',)),
    'calculation-contracts': ('calculation_contracts.py', ('11',)),
    'stl': ('stl_symmetry.py', ('13',)),
    'plate': ('plate_errors.py', ('16',)),
    'legacy': ('legacy_compatibility.py', ('17',)),
}


def test_hashes(names):
    paths = {Path(__file__).name}
    for name in names:
        script = GROUPS[name][0]
        tree = ast.parse((ROOT/script).read_text(encoding='utf-8-sig'))
        declared = []
        for node in tree.body:
            if isinstance(node, ast.Assign) and any(
                    isinstance(target, ast.Name) and target.id in ('CODE','FILES','TEST_FILES')
                    for target in node.targets):
                value = ast.literal_eval(node.value)
                if not isinstance(value, (tuple, list)) or not all(isinstance(p, str) for p in value):
                    raise ValueError('Invalid test dependency declaration: '+script)
                declared += list(value)
        if not declared or script not in declared:
            raise ValueError('Runner has no complete test dependency declaration: '+script)
        paths.update(declared)
    # Other test projects may be edited concurrently. Only the selected runners
    # and their declared compiled/imported dependencies belong to this suite.
    return {name: common.digest_file(ROOT/name) for name in sorted(paths)}


def run_group(name, args, output):
    script, issues = GROUPS[name]
    target = output / name
    command = [sys.executable, '-B', str(ROOT/script), '--source', str(args.source),
               '--lazarus', str(args.lazarus), '--output', str(target),
               '--timeout', str(args.timeout)]
    if name == 'calculation-contracts' and args.solvers:
        command += ['--solvers', str(args.solvers.resolve())]
    if name == 'legacy':
        command += ['--all-ships']
    if name in ('load', 'legacy'):
        for model in args.model or []:
            command += ['--model', str(model)]
    if name == 'hydro-search' and args.model:
        command += ['--model', str(args.model[0])]
    entry = {'id': name, 'issues': issues, 'status': 'error', 'command': command,
             'report': str(target/'report.json')}
    try:
        # Individual probes enforce child timeouts. The outer bound also covers
        # compilation and many separate model runs without letting a runner hang.
        result = subprocess.run(command, cwd=ROOT, capture_output=True,
                                timeout=args.group_timeout, creationflags=common.FLAGS)
        (output/(name+'.log')).write_bytes(result.stdout+b'\n'+result.stderr)
        entry['exit_code'] = result.returncode
        report = json.loads((target/'report.json').read_text(encoding='utf-8-sig'))
        status = report['status']
        if status not in ('pass', 'fail', 'partial', 'review', 'not_run', 'error'):
            raise ValueError('Unknown child status: '+str(status))
        if status == 'pass' and result.returncode != 0:
            raise ValueError('Child reported pass but returned an error code')
        entry.update(status=status, summary=report.get('summary', report.get('counts', {})),
                     source_sha256=report.get('source_sha256'), error=report.get('error'))
    except Exception as exc:
        entry['error'] = f'{type(exc).__name__}: {exc}'
    return entry


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--solvers', type=Path, help='Trusted Exec directory for calculation contract controls')
    ap.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output', type=Path, default=ROOT/'reports'/('improvements-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--group', action='append', choices=GROUPS)
    ap.add_argument('--model', type=Path, action='append')
    ap.add_argument('--jobs', type=int, choices=range(1, 5), default=3)
    ap.add_argument('--timeout', type=int, default=240, help='Each probe build/child timeout in seconds')
    ap.add_argument('--group-timeout', type=int, default=1800, help='Outer runner timeout in seconds')
    args = ap.parse_args()
    args.source = args.source.resolve(); args.lazarus = args.lazarus.resolve()
    if args.model:
        args.model = [model.resolve() for model in args.model]
    out = args.output.resolve()
    if out.exists() or out == args.source or args.source in out.parents:
        raise SystemExit('Output must be new and outside product source')
    out.mkdir(parents=True)
    report = {'status': 'error', 'cases': [], 'limitations': [
        'A test group covers its recorded scenarios, not every aspect of its issue.',
        'No desktop input or native visual acceptance is performed.',
        'Known compatibility failures remain fail; they are not waived.']}
    try:
        before = common.fingerprint(args.source)[0]
        names = list(dict.fromkeys(args.group or GROUPS))
        tests = test_hashes(names)
        report.update(source_sha256=before, test_code_sha256=tests)
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(run_group, name, args, out): name for name in names}
            for future in as_completed(futures):
                entry = future.result(); report['cases'].append(entry)
                print(json.dumps(entry, ensure_ascii=False), flush=True)
        report['cases'].sort(key=lambda x: names.index(x['id']))
        if common.fingerprint(args.source)[0] != before:
            raise RuntimeError('Product source changed during suite')
        if test_hashes(names) != tests:
            raise RuntimeError('Probe sources changed during suite')
        # The common classifier accepts leaf outcomes, while child runners may
        # already aggregate them as partial. Preserve that as incomplete coverage.
        report['status'] = common.classify_statuses([
            'review' if c['status']=='partial' else c['status'] for c in report['cases']])
    except Exception as exc:
        report['error'] = f'{type(exc).__name__}: {exc}'
    report['summary'] = dict(Counter(c['status'] for c in report['cases']))
    totals = Counter()
    for group in report['cases']:
        totals.update(group.get('summary', {}))
    report['assertion_summary'] = dict(totals)
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({k:report.get(k) for k in ('status','summary','assertion_summary','error')}, ensure_ascii=False))
    return 0 if report['status']=='pass' else 2 if report['status'] in ('partial','review','not_run') else 1


if __name__ == '__main__':
    raise SystemExit(main())
