"""Load transaction evidence using an isolated build; never operates the desktop.

The only runtime adapters replace Application.Run with the in-process probe,
record the error dialog, and optionally fail the tested model rebuild. The
original File_Load, LoadProject, serializer and undo/redo methods are exercised.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

import freeship_ui_test as common

ROOT = Path(__file__).resolve().parent
CODE = ('file_safety.py', 'FileSafetyProbe.pas', 'SurfaceLoadingProbe.pas',
        'GeometrySafetyProbe.pas', 'freeship_ui_test.py')


def hashes():
    return {name: common.digest_file(ROOT / name) for name in CODE}


def replace(path, old, new):
    text = path.read_text(encoding='utf8')
    if text.count(old) != 1:
        raise RuntimeError(f'{path.name}: expected exactly one instrumentation anchor: {old}')
    path.write_text(text.replace(old, new), encoding='utf8', newline='\n')


def prepare(src, work, tools, instrument=True):
    for folder in common.FOLDERS:
        shutil.copytree(src / folder, work / folder)
    for p in src.iterdir():
        if p.is_file() and p.suffix.lower() in common.ROOT_SUFFIXES:
            shutil.copy2(p, work / p.name)
    snapshot = common.fingerprint(work)[0]
    for folder in ('Ships', 'Manuals', 'Import'):
        (work / folder).mkdir(exist_ok=True)
    if instrument:
        for name in ('FileSafetyProbe.pas', 'SurfaceLoadingProbe.pas', 'GeometrySafetyProbe.pas'):
            shutil.copy2(ROOT / name, work / 'Units' / name)
        dpr = work / 'FreeShip.dpr'
        replace(dpr, "  Main in 'Forms/Main.pas' {MainForm},",
                "  FileSafetyProbe in 'Units/FileSafetyProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
        replace(dpr, '  Application.Run;', '  FileSafetyProbe.RunFileSafetyProbe(MainForm);')
        edit = work / 'Units' / 'FreeEdit.inc'
        replace(edit, '        ExceptionDlg.ShowModal;',
                "        Windows.SetEnvironmentVariable('FS_FILE_MODAL','shown');\n"
                "        Windows.SetEnvironmentVariable('FS_FILE_ERROR',PChar(ErrMsg));")
        surface = work / 'Units' / 'FreeSubdivisionSurface.inc'
        replace(surface, '  FIsBuilding:=true;',
                "  if SysUtils.GetEnvironmentVariable('FS_FILE_REBUILD_FAULT')='1' then\n"
                "    raise Exception.Create('injected model rebuild failure');\n  FIsBuilding:=true;")
    project = work / 'FreeShip.lpi'
    tree = ET.parse(project)
    for parent in tree.iter():
        for child in list(parent):
            if child.tag in ('ExecuteBefore', 'ExecuteAfter'):
                parent.remove(child)
    runtime = tools / 'fpc' / '3.2.2' / 'units' / 'x86_64-win64'
    for node in tree.iter('OtherUnitFiles'):
        node.set('Value', node.get('Value', '') + ';' + ';'.join(
            str(p) for p in sorted(runtime.iterdir()) if p.is_dir() and p.name != 'fv'))
    target = 'file-safety-probe' if instrument else 'FreeShip-file-safety'
    for node in tree.findall('.//CompilerOptions/Target/Filename'):
        node.set('Value', target)
    tree.write(project, encoding='utf8', xml_declaration=True)
    return project, work / (target + '.exe'), snapshot


def compile_source(project, tools, run, env, out, timeout):
    compiler = tools / 'fpc' / '3.2.2' / 'bin' / 'x86_64-win64' / 'fpc.exe'
    cmd = [str(tools / 'lazbuild.exe'), f'--lazarusdir={tools}', f'--compiler={compiler}',
           f'--pcp={run / "config"}', '--build-mode=Win64 Release', '--ws=win32',
           '--cpu=x86_64', '--os=win64', str(project)]
    process = subprocess.run(cmd, cwd=project.parent, env=env, capture_output=True,
                             timeout=timeout, creationflags=common.FLAGS)
    (out / 'build.log').write_bytes(process.stdout + b'\n' + process.stderr)
    if process.returncode:
        raise RuntimeError(f'Build failed ({process.returncode}); see build.log')
    return common.digest_file(compiler)


def bad_fixtures(original):
    if len(original) < 64 or struct.unpack_from('<I', original)[0] != 9 or original[4:13] != b'FREE!ship':
        raise RuntimeError('fixture does not have the expected documented binary string header')
    invalid = bytearray(original)
    invalid[4:13] = b'NOT!model'
    version = bytearray(original)
    version[13] = 255
    return {
        'missing': None,
        'wrong-header': bytes(invalid),
        'unknown-version': bytes(version),
        'truncated-string': original[:8],
        'truncated-settings': original[:32],
        'truncated-middle': original[:len(original) // 2],
        'truncated-tail': original[:-1],
        'rebuild-fault': original,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output', type=Path, default=ROOT / 'reports' / ('file-safety-' + time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout', type=int, default=240)
    ap.add_argument('--model', type=Path, action='append', help='Additional valid model for successful-load and roundtrip checks')
    ap.add_argument('--case', action='append', choices=('load', 'surface', 'geometry'),
                    help='Run selected groups; default runs all groups')
    ap.add_argument('--build-release-only', action='store_true', help='Build an uninstrumented GUI candidate into the report directory; do not install or launch it')
    a = ap.parse_args()
    src = a.source.resolve()
    out = a.output.resolve()
    if out.exists() or out == src or src in out.parents:
        raise SystemExit('Output must be new and outside the source tree')
    out.mkdir(parents=True)
    cases = []
    report = {'status': 'error', 'cases': cases, 'test_code_sha256': hashes(),
              'limitations': ['Hidden in-process load operations; no native dialogs, desktop input, display or power-loss acceptance.']}
    try:
        before, count = common.fingerprint(src)
        report.update(source_sha256=before, source_file_count=count)
        models = [common.FIXTURE, src / 'Ships' / 'board.fbm', src / 'Ships' / 'Ferries+deck.fbm', src / 'Ships' / 'board.ftm']
        models.extend(p.resolve() for p in (a.model or []))
        fixture_hashes = {str(p): common.digest_file(p) for p in models}
        report['fixture_sha256'] = fixture_hashes
        tools = common.junction_for(a.lazarus.resolve(), Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-file-safety-') as run:
            work = run / 'source'; work.mkdir()
            profile = run / 'profile'; profile.mkdir()
            project, exe, snapshot = prepare(src, work, tools, not a.build_release_only)
            if before != snapshot or common.fingerprint(src)[0] != before:
                raise RuntimeError('Source changed during snapshot')
            report['source_snapshot_sha256'] = snapshot
            report['instrumented_source_sha256'] = common.fingerprint(work)[0]
            env = os.environ.copy()
            env.update(TEMP=str(run), TMP=str(run), APPDATA=str(profile), LOCALAPPDATA=str(profile))
            report['compiler_sha256'] = compile_source(project, tools, run, env, out, a.timeout)
            report['exe_sha256'] = common.digest_file(exe)
            if a.build_release_only:
                data = exe.read_bytes(); pe = struct.unpack_from('<I', data, 60)[0]
                subsystem = struct.unpack_from('<H', data, pe + 24 + 68)[0]
                report['pe_subsystem'] = subsystem
                if subsystem != 2:
                    raise RuntimeError('Candidate is not a Windows GUI executable')
            else:
                original = common.FIXTURE.read_bytes()
                trials = [(mode, key, data, common.FIXTURE) for mode in ('direct', 'edit')
                          for key, data in bad_fixtures(original).items()]
                trials += [('success', str(i) + '-' + p.stem, p.read_bytes(), p) for i, p in enumerate(models)]
                trials += [('success', 'unicode-path', original, common.FIXTURE),
                           ('surface', 'loading-and-events', original, common.FIXTURE),
                           ('geometry', 'indices-and-rebuild', original, common.FIXTURE)]
                if a.case:
                    trials = [trial for trial in trials if
                              (trial[0] if trial[0] in ('surface', 'geometry') else 'load') in a.case]
                for index, (mode, key, data, model) in enumerate(trials):
                    trial = run / f'trial-{index}'; trial.mkdir()
                    trial_profile = trial / 'profile'; trial_profile.mkdir()
                    base = trial / 'original.fbm'; base.write_bytes(original)
                    target = trial / ('模型-Женя.fbm' if key == 'unicode-path' else 'target' + (model.suffix if mode == 'success' else '.fbm'))
                    if data is not None:
                        target.write_bytes(data)
                    subenv = env.copy()
                    subenv.update(APPDATA=str(trial_profile), LOCALAPPDATA=str(trial_profile),
                                  FS_FILE_MODEL=str(base), FS_FILE_TARGET=str(target),
                                  FS_FILE_OUTPUT=str(trial), FS_FILE_MODE=mode, FS_FILE_CASE=key,
                                  FS_FILE_REBUILD_FAULT='', FS_FILE_MODAL='', FS_FILE_ERROR='')
                    label = f'{index:02d}-{mode}-{key}'
                    try:
                        p = subprocess.run([str(exe)], cwd=work, env=subenv, capture_output=True,
                                           timeout=a.timeout, creationflags=common.FLAGS)
                        (out / (label + '.log')).write_bytes(p.stdout + b'\n' + p.stderr)
                        native = json.loads((trial / 'probe.json').read_text(encoding='utf-8-sig'))
                        (out / (label + '.json')).write_text(json.dumps(native, ensure_ascii=False, indent=2), encoding='utf8')
                        evidence = out / label
                        evidence.mkdir()
                        for pattern in ('*.fbm', 'geometry-*.txt'):
                            for saved in trial.glob(pattern):
                                shutil.copy2(saved, evidence / saved.name)
                        if p.returncode or native.get('error'):
                            raise RuntimeError(f'exit={p.returncode}; {native.get("error")}')
                        expected = 11 if mode == 'success' else 35 if mode == 'surface' else 62 if mode == 'geometry' else 7
                        if len(native['cases']) != expected or len({c['id'] for c in native['cases']}) != expected:
                            raise RuntimeError(f'incomplete probe output: expected {expected} cases')
                        for case in native['cases']:
                            if mode == 'success':
                                case['id'] = case['id'].replace('02.success.', '02.success.' + key + '.')
                            case['fixture'] = str(model)
                            cases.append(case)
                    except Exception as exc:
                        cases.append({'id': f'02.{mode}.{key}.execution', 'status': 'error',
                                      'expected': 'probe executes and emits all required evidence', 'actual': str(exc)})
                cases.append({'id': '02.native-interaction', 'status': 'not_run',
                              'expected': 'native desktop menu/dialog and visible viewport acceptance',
                              'actual': 'AGENTS.md prohibits desktop-operation acceptance; in-process actions only'})
            if common.fingerprint(src)[0] != before:
                raise RuntimeError('Source changed during run; results are invalid')
            if hashes() != report['test_code_sha256']:
                raise RuntimeError('Test code changed during run; results are invalid')
            if {str(p): common.digest_file(p) for p in models} != fixture_hashes:
                raise RuntimeError('Original model changed during run')
            report['source_unchanged_during_run'] = True
            if a.build_release_only:
                shutil.copy2(exe, out / exe.name)
                report['candidate'] = str(out / exe.name)
                report['status'] = 'pass'
            else:
                report['status'] = common.classify_statuses([c['status'] for c in cases])
    except Exception as exc:
        report['status'] = 'error'; report['error'] = f'{type(exc).__name__}: {exc}'
    report['summary'] = dict(Counter(c['status'] for c in cases))
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'status': report['status'], 'summary': report['summary'], 'report': str(out / 'report.json'),
                      'error': report.get('error')}, ensure_ascii=False))
    return 0 if report['status'] == 'pass' else 2 if report['status'] == 'partial' else 1


if __name__ == '__main__':
    raise SystemExit(main())
