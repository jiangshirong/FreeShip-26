"""Current native FBM/FTM reader compatibility and field-offset evidence, without desktop input."""
import argparse
from collections import Counter
import json
import re
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import freeship_ui_test as common
import file_safety

ROOT = Path(__file__).resolve().parent
FILES = ('legacy_compatibility.py', 'LegacyCompatibilityProbe.pas', 'FileSafetyProbe.pas',
         'SurfaceLoadingProbe.pas', 'GeometrySafetyProbe.pas', 'file_safety.py', 'freeship_ui_test.py')

def calculation_evidence(data, trace, original):
    if not data.get('loaded'): return
    records={
      'HoltrSeriesResistance':(43,2),'OSTSeriesResistance':(52,2),
      'Task1Propeller':(15,1),'Task2Propeller':(25,1),'Task3Propeller':(16,1),
      'PlaningResistance':(13,1),'RvrsPropeller':(16,1),'HollenSeriesResistance':(43,2),
      'Task4Propeller':(15,1),'Task5Propeller':(5,1),'OortmerSeriesResistance':(43,2),
      'FungSeriesResistance':(43,2),'HydrodynManeuv':(19,1),'HydrodynTask1':(11,1),
      'RBHSSeriesResistance':(52,2),'MHSeriesResistance':(52,2)}
    versions=list(re.finditer(r'^version start=',trace,re.M))
    source_trace=trace[:versions[1].start()] if len(versions)>1 else ''
    layout=data.get('legacy_calculation_layout',''); equal=True; originals=True; checked=[]
    for name,(floats,flags) in records.items():
        method='LoadTFree'+name+'Data'
        values=[bytes.fromhex(v) for v in re.findall(r'values '+method+r'=([0-9A-F]+)',trace)]
        if not values: continue
        width=floats*4+flags
        if len(values)>1: equal &= values[0][:width]==values[-1][:width]
        start=re.search(method+r' start=(\d+)',source_trace)
        end=re.search(method+r' end=(\d+)',source_trace)
        if start and end and original.suffix.lower()=='.fbm':
            b=original.read_bytes();a,z=int(start[1]),int(end[1]);disk=b[a:z]
            stored_floats=len(disk)//4-1
            if name=='Task5Propeller' and 'invalid Propeller Task5' in layout:
                originals &= values[0][:width]==bytes(width)
            elif 0<=stored_floats<=floats:
                originals &= values[0][:stored_floats*4]==disk[:stored_floats*4]
                originals &= values[0][stored_floats*4:floats*4]==bytes((floats-stored_floats)*4)
                originals &= values[0][floats*4:width]==disk[stored_floats*4:stored_floats*4+flags]
            else: originals=False
            checked.append(name)
    data['calculation_parameters_roundtrip_equal']=equal
    data['calculation_parameters_match_source_fields']=originals
    data['calculation_records_checked']=checked
    if not equal or not originals:data['status']='fail'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output', type=Path, default=ROOT/'reports'/('legacy-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--model', type=Path, action='append')
    ap.add_argument('--all-ships', action='store_true')
    ap.add_argument('--timeout', type=int, default=240)
    ap.add_argument('--diagnostic', action='store_true', help='Allow compilation of a changing checkout snapshot; results cannot be acceptance evidence')
    a = ap.parse_args(); src = a.source.resolve(); out = a.output.resolve()
    if out.exists() or out == src or src in out.parents:
        raise SystemExit('Output must be new and outside source')
    out.mkdir(parents=True)
    cases = []; report = {'status':'error', 'cases':cases, 'limitations':['Native model methods only; not a desktop open-dialog test.', 'A readable bundled sample does not prove compatibility with every historical writer.', 'Roundtrip compares geometry, project settings and calculation-parameter fields; this does not certify physical correctness. Invalid historical Task5 bytes are explicitly reported and cleared, not recovered.', 'Background JPEG recompression differences are review, never a claim of lossless image preservation.']}
    try:
        code = {p: common.digest_file(ROOT/p) for p in FILES}; report['test_code_sha256'] = code
        before = common.fingerprint(src)[0]; report['source_sha256'] = before
        models = [src/'Ships'/'Database'/'Motorboat3.fbm', src/'Ships'/'board.fbm', src/'Ships'/'board.ftm']
        if a.all_ships:
            models += sorted(p for p in (src/'Ships').rglob('*') if p.suffix.lower() in ('.fbm','.ftm'))
        models += [p.resolve() for p in (a.model or [])]
        models = list(dict.fromkeys(models)); fixture_hashes = {str(p):common.digest_file(p) for p in models}
        fixture_hashes[str(common.FIXTURE)] = common.digest_file(common.FIXTURE)
        report['fixture_sha256'] = fixture_hashes
        tools = common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-legacy-') as run:
            work=run/'source'; work.mkdir(); profile=run/'profile'; profile.mkdir()
            project,exe,snapshot=file_safety.prepare(src,work,tools,False)
            report['source_snapshot_sha256']=snapshot
            if not a.diagnostic and (snapshot!=before or common.fingerprint(src)[0]!=before):
                raise RuntimeError('Source changed during snapshot')
            for p in FILES:
                if p.endswith('.pas'): shutil.copy2(ROOT/p,work/'Units'/p)
            file_safety.replace(work/'FreeShip.dpr', "  Main in 'Forms/Main.pas' {MainForm},", "  LegacyCompatibilityProbe in 'Units/LegacyCompatibilityProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            file_safety.replace(work/'FreeShip.dpr', '  Application.Run;', '  LegacyCompatibilityProbe.RunLegacyProbe(MainForm);')
            # Do not display an integrity-warning dialog while processing a bad fixture.
            surface=work/'Units'/'FreeSubdivisionSurface.inc'; s=surface.read_text(encoding='utf8')
            s=s.replace('ShowMessage(rsLocLoadedModelIntegrity)', 'raise EReadError.Create(rsLocLoadedModelIntegrity)')
            surface.write_text(s,encoding='utf8')
            env=os.environ.copy(); env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile))
            report['instrumented_source_sha256']=common.fingerprint(work)[0]
            report['compiler_sha256']=file_safety.compile_source(project,tools,run,env,out,a.timeout)
            report['exe_sha256']=common.digest_file(exe)
            trials=[(p,False) for p in models]+[(common.FIXTURE,True)]
            for i,(model,alpha) in enumerate(trials):
                trial=run/f'trial-{i}'; trial.mkdir(); local=trial/('model'+model.suffix); shutil.copy2(model,local)
                subenv=env.copy(); subenv.update(FS_LEGACY_MODEL=str(local),FS_LEGACY_OUTPUT=str(trial),FS_LEGACY_ALPHA='1' if alpha else '')
                label=f'{i:03d}-'+('known-fv500' if alpha else model.stem)
                try:
                    child=subprocess.run([str(exe)],cwd=work,env=subenv,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
                    (out/(label+'.log')).write_bytes(child.stdout+b'\n'+child.stderr)
                    data=json.loads((trial/'probe.json').read_text(encoding='utf-8-sig'))
                    data.update(id='17.'+label, fixture=str(model), fixture_sha256=fixture_hashes[str(model)])
                    if child.returncode: raise RuntimeError('Probe process exited '+str(child.returncode))
                    calculation_evidence(data,(trial/'offsets.txt').read_text(encoding='utf-8-sig'),model)
                    shutil.copy2(trial/'offsets.txt',out/(label+'-offsets.txt'))
                    (out/(label+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
                    cases.append(data)
                except Exception as exc:
                    cases.append({'id':'17.'+label,'status':'error','fixture':str(model),'actual':str(exc)})
            unchanged=common.fingerprint(src)[0]==before and snapshot==before
            report['source_unchanged_during_run']=unchanged
            if {p:common.digest_file(ROOT/p) for p in FILES}!=code: raise RuntimeError('Test code changed during run')
            if {p:common.digest_file(Path(p)) for p in fixture_hashes}!=fixture_hashes: raise RuntimeError('Original fixture changed during run')
            if a.diagnostic:
                report['status']='review'; report['diagnostic_only']=True
            elif not unchanged: raise RuntimeError('Source changed during run')
            else: report['status']='error' if any(c['status']=='error' for c in cases) else common.classify_statuses([c['status'] for c in cases])
    except Exception as exc: report['status']='error'; report['error']=str(exc)
    report['summary']=dict(Counter(c['status'] for c in cases))
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:report.get(k) for k in ('status','summary','error')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 2 if report['status'] in ('review','partial') else 1

if __name__=='__main__': raise SystemExit(main())
