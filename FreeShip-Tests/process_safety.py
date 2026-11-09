"""Exercise real FreeProcess with bounded external helper programs, without desktop input."""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import freeship_ui_test as common

ROOT = Path(__file__).resolve().parent
DEPENDENCIES = ('Units/freeprocess.pas', 'Units/FreeLogger.pas',
                'Forms/FreeExceptionDlg.pas', 'Forms/FreeExceptionDlg.lfm')
TEST_FILES = ('process_safety.py', 'ProcessSafetyProbe.pas', 'ProcessSafetyHelper.pas', 'freeship_ui_test.py')


def hashes(root, names):
    return {name: common.digest_file(root / name) for name in names}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    parser.add_argument('--output', type=Path, default=ROOT/'reports'/('process-safety-'+time.strftime('%Y%m%d-%H%M%S')))
    parser.add_argument('--timeout', type=int, default=90)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output.exists() or output == source or source in output.parents:
        raise SystemExit('Output must be new and outside the source tree')
    output.mkdir(parents=True)
    report = {'status': 'error', 'cases': [], 'limitations': [
        'Real Windows processes and FreeProcess unit are exercised; no desktop interaction.',
        'Cancellation callback is tested; existing dialogs do not yet expose a cancel control.',
        'Plugin-specific per-run work directories and output-file completeness checks are not covered.',
        'Timeout terminates the directly launched process; arbitrary descendant process trees are not covered.',
    ]}
    try:
        report['source_dependencies_sha256'] = hashes(source, DEPENDENCIES)
        report['test_code_sha256'] = hashes(ROOT, TEST_FILES)
        toolchain = common.junction_for(args.lazarus.resolve(), Path(tempfile.gettempdir()))
        compiler = toolchain/'fpc/3.2.2/bin/x86_64-win64/fpc.exe'
        report['compiler_sha256'] = common.digest_file(compiler)
        with common.temporary_workspace('fs-process-safety-') as run:
            work = run/'source'; work.mkdir()
            for name in DEPENDENCIES:
                destination = work/name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source/name, destination)
            if hashes(work, DEPENDENCIES) != report['source_dependencies_sha256']:
                raise RuntimeError('Source changed while copying')
            for name in TEST_FILES[1:3]:
                shutil.copy2(ROOT/name, work/name)
            units = run/'units'; units.mkdir()
            evidence = run/'evidence'; evidence.mkdir()
            paths = [work/'Units', work/'Forms', toolchain/'lcl/units/x86_64-win64/win32',
                     toolchain/'lcl/units/x86_64-win64', toolchain/'components/lazutils/lib/x86_64-win64',
                     toolchain/'packager/units/x86_64-win64']
            paths += [p for p in (toolchain/'fpc/3.2.2/units/x86_64-win64').iterdir() if p.is_dir() and p.name != 'fv']
            env = os.environ.copy()
            env.update(TEMP=str(run), TMP=str(run), APPDATA=str(run/'profile'), LOCALAPPDATA=str(run/'profile'))
            helper = evidence/'process-helper.exe'
            build = subprocess.run([str(compiler), '-FU'+str(units), '-o'+str(helper), str(work/'ProcessSafetyHelper.pas')],
                                   cwd=work, env=env, capture_output=True, timeout=args.timeout, creationflags=common.FLAGS)
            (output/'helper-build.log').write_bytes(build.stdout+b'\n'+build.stderr)
            if build.returncode:
                raise RuntimeError('Helper compile failed')
            for name in ('process-fail.exe', 'process-wait.exe'):
                shutil.copy2(helper, evidence/name)
            unicode_dir=evidence/'中文 Женя';unicode_dir.mkdir()
            shutil.copy2(helper,unicode_dir/'计算 helper.exe')
            report['helper_sha256'] = common.digest_file(helper)
            viewer = evidence/'process-viewer.exe'
            build = subprocess.run([str(compiler), '-WG', '-FU'+str(units), '-o'+str(viewer), str(work/'ProcessSafetyHelper.pas')],
                                   cwd=work, env=env, capture_output=True, timeout=args.timeout, creationflags=common.FLAGS)
            (output/'viewer-build.log').write_bytes(build.stdout+b'\n'+build.stderr)
            if build.returncode:
                raise RuntimeError('Viewer helper compile failed')
            report['viewer_helper_sha256'] = common.digest_file(viewer)
            exe = work/'process-safety-probe.exe'
            cmd = [str(compiler), '-dLCL', '-gl', '-Cr', '-Co', '-Ci', '-Sa', '-FU'+str(units), '-o'+str(exe)]
            cmd += ['-Fu'+str(p) for p in paths]
            cmd.append(str(work/'ProcessSafetyProbe.pas'))
            build = subprocess.run(cmd, cwd=work, env=env, capture_output=True, timeout=args.timeout, creationflags=common.FLAGS)
            (output/'build.log').write_bytes(build.stdout+b'\n'+build.stderr)
            if build.returncode:
                raise RuntimeError('Probe compile failed; see build.log')
            report['exe_sha256'] = common.digest_file(exe)
            child = subprocess.run([str(exe), str(evidence), str(evidence/'probe.json')], cwd=work, env=env,
                                   capture_output=True, timeout=args.timeout, creationflags=common.FLAGS)
            (output/'runtime.log').write_bytes(child.stdout+b'\n'+child.stderr)
            if not (evidence/'probe.json').exists():
                raise RuntimeError(f'No report: exit {child.returncode}')
            native = json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig'))
            shutil.copy2(evidence/'probe.json', output/'probe.json')
            report['cases'] = native['cases']
            if native.get('error'):
                raise RuntimeError(native['error'])
            if len(report['cases']) != 30 or len({x['id'] for x in report['cases']}) != 30:
                raise RuntimeError('Incomplete or duplicate probe results')
            if child.returncode and not any(x['status'] == 'fail' for x in report['cases']):
                raise RuntimeError(f'Unexpected process exit {child.returncode}')
            report['status'] = 'fail' if any(x['status'] == 'fail' for x in report['cases']) else 'pass'
    except Exception as exc:
        report.update(status='error', error=f'{type(exc).__name__}: {exc}')
    finally:
        if report.get('source_dependencies_sha256') != hashes(source, DEPENDENCIES):
            report.update(status='error', error='Compiled source dependencies changed during the run')
        if report.get('test_code_sha256') != hashes(ROOT, TEST_FILES):
            report.update(status='error', error='Test code changed during the run')
        report['summary'] = dict(Counter(x['status'] for x in report['cases']))
        (output/'report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf8')
    print(json.dumps({'status': report['status'], 'summary': report['summary'], 'report': str(output/'report.json'), 'error': report.get('error')}, ensure_ascii=False))
    return 0 if report['status'] == 'pass' else 1


if __name__ == '__main__':
    raise SystemExit(main())
