"""Compile current FreeShip in isolation and exercise its actual save transaction.

Only modal chooser answers and explicit fault sites are adapted in the disposable
copy. No desktop input, user settings or original model files are changed.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

import freeship_ui_test as common

ROOT = Path(__file__).resolve().parent
FILES = ('save_safety.py', 'SaveSafetyHooks.pas', 'UiSaveSafetyProbe.pas', 'freeship_ui_test.py')


def hashes():
    return {name: common.digest_file(ROOT / name) for name in FILES}


def prepare(source, work):
    for name in common.FOLDERS:
        shutil.copytree(source / name, work / name)
    for path in source.iterdir():
        if path.is_file() and path.suffix.lower() in common.ROOT_SUFFIXES:
            shutil.copy2(path, work / path.name)
    snapshot = common.fingerprint(work)[0]
    for name in ('Ships', 'Manuals', 'Import'):
        (work / name).mkdir(exist_ok=True)
    for name in FILES[1:3]:
        shutil.copy2(ROOT / name, work / 'Units' / name)
    project = work / 'FreeShip.dpr'
    text = project.read_text(encoding='utf8')
    text, n = re.subn(r"(?m)^(\s*Main\s+in\s+'Forms/Main\.pas'\s+\{MainForm\},)$",
                     "  UiSaveSafetyProbe in 'Units/UiSaveSafetyProbe.pas',\n\\1", text)
    if n != 1:
        raise RuntimeError('Main program uses anchor changed')
    text, n = re.subn(r'(?m)^\s*Application\.Run\s*;\s*$',
                     '  Application.ShowMainForm := False;\n  UiSaveSafetyProbe.RunSaveSafety(MainForm);', text)
    if n != 1:
        raise RuntimeError('Program entry anchor changed')
    project.write_text(text, encoding='utf8')
    main_unit = work / 'Units' / 'FreeShipUnit.pas'
    common.replace_once(main_unit, 'uses Math,', 'uses SaveSafetyHooks, Math,')
    edit = work / 'Units' / 'FreeEdit.inc'
    text = edit.read_text(encoding='utf8')
    start = text.index('function TFreeEdit.File_Save:boolean;')
    end = text.index('end;{TFreeEdit.File_Save}', start)
    part = text[start:end]
    if not part.count('MessageDlg('):
        raise RuntimeError('Save messages were not discovered')
    part = part.replace('MessageDlg(', 'SaveSafetyHooks.SaveMessage(')
    anchor = '      FreeShip.SaveProject(Destination);'
    if part.count(anchor) != 1:
        raise RuntimeError('Serialization injection site changed')
    part = part.replace(anchor, "      SaveSafetyHooks.Trip('serialize');\n" + anchor)
    text = text[:start] + part + text[end:]
    start = text.index('function TFreeEdit.File_SaveAs:boolean;')
    end = text.index('end;{TFreeEdit.FileDestroy_SaveAs}', start)
    part = text[start:end]
    anchor = '    if not SaveDialog.Execute then Exit;'
    if part.count(anchor) != 1:
        raise RuntimeError('SaveAs chooser site changed')
    part = part.replace(anchor, "    SaveDialog.FileName := SaveSafetyHooks.SelectedName;\n    if SaveDialog.FileName='' then Exit;")
    part = part.replace('MessageDlg(', 'SaveSafetyHooks.SaveMessage(')
    edit.write_text(text[:start] + part + text[end:], encoding='utf8')
    safe = work / 'Units' / 'FreeSafeFile.pas'
    common.replace_once(safe, 'uses\n  {$IFDEF MSWINDOWS}', 'uses SaveSafetyHooks,\n  {$IFDEF MSWINDOWS}')
    common.replace_once(safe, '  if not FlushFileBuffers(FHandle) then', "  SaveSafetyHooks.Trip('flush');\n  if not FlushFileBuffers(FHandle) then")
    common.replace_once(safe, '    Buffer.SaveToStream(Pending.Stream);',
                        "    if SaveSafetyHooks.Fault='write' then begin Pending.Stream.WriteBuffer(PChar('partial')[0],7); SaveSafetyHooks.Trip('write') end;\n    Buffer.SaveToStream(Pending.Stream);")
    common.replace_once(safe, '            Backup.Stream.CopyFrom(Original, 0);',
                        "            SaveSafetyHooks.Trip('backup-copy');\n            Backup.Stream.CopyFrom(Original, 0);")
    common.replace_once(safe, "        Backup.Install(ChangeFileExt(Target, '.Bak'), True);",
                        "        SaveSafetyHooks.Trip('backup-install');\n        Backup.Install(ChangeFileExt(Target, '.Bak'), True);")
    common.replace_once(safe, '    Pending.Install(Target, HadOriginal);',
                        "    SaveSafetyHooks.Trip('replace');\n    Pending.Install(Target, HadOriginal);")
    lpi = work / 'FreeShip.lpi'
    tree = ET.parse(lpi)
    for parent in tree.iter():
        for child in list(parent):
            if child.tag in ('ExecuteBefore', 'ExecuteAfter'):
                parent.remove(child)
    return snapshot, tree, lpi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    parser.add_argument('--output', type=Path, default=ROOT / 'reports' / ('save-safety-' + time.strftime('%Y%m%d-%H%M%S')))
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output.exists() or output == source or source in output.parents:
        raise SystemExit('Output must be new and outside the production source')
    output.mkdir(parents=True)
    report = {'status': 'error', 'cases': [], 'test_code_sha256': hashes(),
              'fixture_sha256': common.digest_file(common.FIXTURE),
              'limitations': ['Hidden native-code execution, not desktop interaction acceptance.',
                              'Windows filesystem tested; POSIX and power loss are not simulated.']}
    before = None
    try:
        before, count = common.fingerprint(source)
        report.update(source_sha256=before, source_file_count=count)
        tools = common.junction_for(args.lazarus.resolve(), Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-save-safety-') as run:
            work = run / 'source'; work.mkdir()
            evidence = run / 'evidence'; evidence.mkdir()
            profile = run / 'profile'; profile.mkdir()
            snapshot, tree, lpi = prepare(source, work)
            if snapshot != before or common.fingerprint(source)[0] != before:
                raise RuntimeError('Source changed during snapshot')
            runtime = tools / 'fpc' / '3.2.2' / 'units' / 'x86_64-win64'
            for item in tree.iter('OtherUnitFiles'):
                item.set('Value', item.get('Value', '') + ';' + ';'.join(str(p) for p in sorted(runtime.iterdir()) if p.is_dir() and p.name != 'fv'))
            for item in tree.findall('.//CompilerOptions/Target/Filename'):
                item.set('Value', 'save-safety-probe')
            tree.write(lpi, encoding='utf8', xml_declaration=True)
            fixture = run / 'fixture.fbm'; shutil.copy2(common.FIXTURE, fixture)
            compiler = tools / 'fpc' / '3.2.2' / 'bin' / 'x86_64-win64' / 'fpc.exe'
            report['compiler_sha256'] = common.digest_file(compiler)
            report['instrumented_source_sha256'] = common.fingerprint(work)[0]
            env = os.environ.copy()
            env.update(TEMP=str(run), TMP=str(run), APPDATA=str(profile), LOCALAPPDATA=str(profile),
                       FS_SAVE_OUTPUT=str(evidence), FS_SAVE_FIXTURE=str(fixture))
            cmd = [str(tools / 'lazbuild.exe'), f'--lazarusdir={tools}', f'--compiler={compiler}',
                   f'--pcp={run / "config"}', '--build-mode=Win64 Release', '--ws=win32',
                   '--cpu=x86_64', '--os=win64', str(lpi)]
            built = subprocess.run(cmd, cwd=work, env=env, capture_output=True, timeout=args.timeout, creationflags=common.FLAGS)
            (output / 'build.log').write_bytes(built.stdout + b'\n' + built.stderr)
            if built.returncode:
                raise RuntimeError('Build failed; see build.log')
            exe = work / 'save-safety-probe.exe'
            report['executable_sha256'] = common.digest_file(exe)
            tested = subprocess.run([str(exe)], cwd=work, env=env, capture_output=True, timeout=90, creationflags=common.FLAGS)
            (output / 'runtime.log').write_bytes(tested.stdout + b'\n' + tested.stderr)
            shutil.copytree(evidence, output / 'evidence')
            raw = json.loads((evidence / 'probe.json').read_text(encoding='utf-8-sig'))
            report['cases'] = raw['cases']
            if tested.returncode or raw.get('error'):
                raise RuntimeError(f'Probe error: exit={tested.returncode}, {raw.get("error")}')
            if len(report['cases']) != 13:
                raise RuntimeError('Incomplete save scenarios')
            if common.digest_file(fixture) != report['fixture_sha256']:
                raise RuntimeError('Isolated fixture changed')
            report['status'] = common.classify_statuses([c['status'] for c in report['cases']])
    except Exception as exc:
        report['status'] = 'error'; report['error'] = f'{type(exc).__name__}: {exc}'
    if before and common.fingerprint(source)[0] != before:
        report['status'] = 'error'; report['error'] = 'Source changed during validation'
    if hashes() != report['test_code_sha256'] or common.digest_file(common.FIXTURE) != report['fixture_sha256']:
        report['status'] = 'error'; report['error'] = 'Test code or original fixture changed'
    report['counts'] = dict(Counter(c['status'] for c in report['cases']))
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'status': report['status'], 'counts': report['counts'], 'error': report.get('error'), 'report': str(output / 'report.json')}, ensure_ascii=False))
    return 0 if report['status'] == 'pass' else 1


if __name__ == '__main__':
    raise SystemExit(main())
