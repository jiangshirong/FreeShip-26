"""Independent sampled-section references for the actual hydrostatic maximum search.

The current model and section integrator are compiled unchanged in isolation.
Only the program entry point and an evaluation counter are instrumented.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import file_safety as build
import freeship_ui_test as common

ROOT=Path(__file__).resolve().parent
CODE=('hydro_search.py','HydroSearchProbe.pas','file_safety.py','freeship_ui_test.py')

def hashes(): return {name:common.digest_file(ROOT/name) for name in CODE}

def prepare(source,work,tools,reference):
    project,exe,snapshot=build.prepare(source,work,tools,instrument=False)
    shutil.copy2(ROOT/'HydroSearchProbe.pas',work/'Units/HydroSearchProbe.pas')
    build.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},",
                  "  HydroSearchProbe in 'Units/HydroSearchProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
    build.replace(work/'FreeShip.dpr','  Application.Run;',
                  '  Application.ShowMainForm := False;\n  HydroSearchProbe.RunHydroSearchProbe(MainForm);')
    build.replace(work/'Units/FreeShipUnit.pas','\nimplementation\n','\nvar HydroSearchAreaCalls:QWord;\n\nimplementation\n')
    hydro=work/'Units/FreeHydrostaticCalc.inc'; text=hydro.read_text(encoding='utf8')
    start=text.index('procedure TFreeHydrostaticCalc.CalculateMidshipLocation;'); end=text.index('\nend;',start)+5
    part=reference.read_text(encoding='utf8') if reference else text[start:end]
    anchor=r'(?m)^([ \t]*)Frame\.CalculateArea\(FData\.WaterlinePlane, Area, COG, MOI\);'
    matches=list(re.finditer(anchor,part))
    if len(matches)!=(3 if reference and len(matches)==3 else 1): raise RuntimeError('Area observation points changed')
    part=re.sub(anchor,lambda m:m[1]+'Inc(HydroSearchAreaCalls);\n'+m[0],part)
    hydro.write_text(text[:start]+part+text[end:],encoding='utf8',newline='\n')
    return project,exe,snapshot

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    parser.add_argument('--output',type=Path,default=ROOT/'reports'/('hydro-search-'+time.strftime('%Y%m%d-%H%M%S')))
    parser.add_argument('--model',type=Path,help='Optional representative real model, copied read-only into isolation')
    parser.add_argument('--reference-function',type=Path,help='Optional saved complete CalculateMidshipLocation function, substituted only in the isolated copy')
    parser.add_argument('--timeout',type=int,default=300)
    args=parser.parse_args(); source=args.source.resolve(); output=args.output.resolve()
    if output.exists() or output==source or source in output.parents: raise SystemExit('Output must be new and outside the source')
    output.mkdir(parents=True)
    report={'status':'error','cases':[],'test_code_sha256':hashes(),
            'limitations':['Finite independent scans are references, not a proof of the global maximum on all geometries.',
                           'Hidden native-code calculations, no desktop acceptance.']}
    before=None
    try:
        before,count=common.fingerprint(source); report.update(source_sha256=before,source_file_count=count)
        if args.model: report['model_sha256']=common.digest_file(args.model.resolve())
        if args.reference_function: report['reference_function_sha256']=common.digest_file(args.reference_function.resolve())
        tools=common.junction_for(args.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-hydro-search-') as run:
            work=run/'source'; work.mkdir(); evidence=run/'evidence'; evidence.mkdir(); profile=run/'profile'; profile.mkdir()
            project,exe,snapshot=prepare(source,work,tools,args.reference_function)
            if before!=snapshot or common.fingerprint(source)[0]!=before: raise RuntimeError('Source changed during snapshot')
            env=os.environ.copy(); env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_HSEARCH_REPORT=str(evidence/'probe.json'))
            if args.model:
                copied=run/('model'+args.model.suffix); shutil.copy2(args.model,copied); env['FS_HSEARCH_MODEL']=str(copied)
            report['instrumented_source_sha256']=common.fingerprint(work)[0]
            report['compiler_sha256']=build.compile_source(project,tools,run,env,output,args.timeout)
            report['executable_sha256']=common.digest_file(exe)
            try:
                process=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=args.timeout,creationflags=common.FLAGS)
                (output/'runtime.log').write_bytes(process.stdout+b'\n'+process.stderr)
            except subprocess.TimeoutExpired as exc:
                (output/'runtime.log').write_bytes((exc.stdout or b'')+b'\n'+(exc.stderr or b''))
                raise
            finally:
                shutil.copytree(evidence,output/'evidence')
            raw=json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig')); report['cases']=raw['cases']
            report['observations']=raw.get('observations',[])
            if raw.get('error') or process.returncode: raise RuntimeError(f'Probe error: exit {process.returncode}: {raw.get("error")}')
            expected_cases=29 if args.model else 22
            if len(report['cases'])!=expected_cases or len({x['id'] for x in report['cases']})!=len(report['cases']): raise RuntimeError(f'Incomplete or duplicate scenarios: expected {expected_cases}')
            if args.model and common.digest_file(copied)!=report['model_sha256']: raise RuntimeError('Isolated model changed')
            report['status']=common.classify_statuses(x['status'] for x in report['cases'])
    except Exception as exc: report.update(status='error',error=f'{type(exc).__name__}: {exc}')
    if before and common.fingerprint(source)[0]!=before: report.update(status='error',error='Source changed during validation')
    if hashes()!=report['test_code_sha256']: report.update(status='error',error='Test code changed')
    if args.model and common.digest_file(args.model.resolve())!=report.get('model_sha256'): report.update(status='error',error='Original model changed')
    if args.reference_function and common.digest_file(args.reference_function.resolve())!=report.get('reference_function_sha256'): report.update(status='error',error='Reference source changed')
    report['counts']=dict(Counter(x['status'] for x in report['cases']))
    (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'counts':report['counts'],'error':report.get('error'),'report':str(output/'report.json')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 2 if report['status']=='partial' else 1

if __name__=='__main__': raise SystemExit(main())
