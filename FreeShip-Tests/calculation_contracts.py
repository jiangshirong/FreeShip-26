"""Exercise isolated calculation files, rejected output, cache publication and real solver byte parity."""
import argparse
from collections import Counter
import json, os
from pathlib import Path
import shutil, subprocess, tempfile, time
import freeship_ui_test as common
import file_safety

ROOT=Path(__file__).resolve().parent
FILES=('calculation_contracts.py','solver_protocol_samples.json','CalculationContractsProbe.pas','ProcessSafetyHelper.pas','file_safety.py','freeship_ui_test.py')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    p.add_argument('--output',type=Path,default=ROOT/'reports'/('calculation-contracts-'+time.strftime('%Y%m%d-%H%M%S')))
    p.add_argument('--solvers',type=Path,help='Optional trusted historical Exec directory; run six planing-solver parity checks')
    p.add_argument('--timeout',type=int,default=240)
    a=p.parse_args();src=a.source.resolve();out=a.output.resolve()
    if out.exists() or out==src or src in out.parents:raise SystemExit('Output must be new and outside source')
    out.mkdir(parents=True)
    r={'status':'error','cases':[],'test_code_sha256':{n:common.digest_file(ROOT/n) for n in FILES},
       'limitations':['Native methods only; no desktop operation.','Protocol validation does not certify physical solver accuracy.','Old variable-length protocols without a record count or checksum cannot detect truncation exactly at a legal row boundary.']}
    try:
        before,count=common.fingerprint(src);r.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-contracts-') as run:
            work=run/'source';work.mkdir();evidence=run/'evidence';evidence.mkdir();profile=run/'profile';profile.mkdir()
            project,exe,snapshot=file_safety.prepare(src,work,tools,False)
            if snapshot!=before or common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during copy')
            shutil.copy2(ROOT/'CalculationContractsProbe.pas',work/'Units/CalculationContractsProbe.pas')
            file_safety.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},", "  CalculationContractsProbe in 'Units/CalculationContractsProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            file_safety.replace(work/'FreeShip.dpr','  Application.Run;','  CalculationContractsProbe.RunCalculationContracts(MainForm);')
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),
              FS_CONTRACT_ROOT=str(evidence/'中文 calculation'),FS_CONTRACT_REPORT=str(evidence/'probe.json'))
            compiler=tools/'fpc/3.2.2/bin/x86_64-win64/fpc.exe';helper=evidence/'process-helper.exe'
            h=subprocess.run([str(compiler),'-FU'+str(evidence),'-o'+str(helper),str(ROOT/'ProcessSafetyHelper.pas')],cwd=evidence,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (out/'helper-build.log').write_bytes(h.stdout+b'\n'+h.stderr)
            if h.returncode:raise RuntimeError('Helper compilation failed')
            env['FS_CONTRACT_HELPER']=str(helper)
            if a.solvers:
                solver=a.solvers.resolve();real=evidence/'real';real.mkdir()
                samples=json.loads((ROOT/'solver_protocol_samples.json').read_text('utf8'))['cases']
                r['real_solver_sha256']={case['exe']:common.digest_file(solver/case['exe']) for case in samples}
                for case in samples:
                    control=real/case['id'];control.mkdir();inp=case['input'];result=case['output']
                    (control/inp).write_text(''.join(f'{x:.9e}\n' for x in case['values']),encoding='ascii')
                    shutil.copy2(control/inp,real/inp)
                    c=subprocess.run([str(solver/case['exe'])],cwd=control,env=env,stdin=subprocess.DEVNULL,capture_output=True,timeout=15,creationflags=common.FLAGS)
                    if c.returncode or not (control/result).exists():raise RuntimeError(f"Real control {case['exe']} failed: {c.returncode}; {c.stderr!r}")
                    shutil.copy2(control/result,real/result)
                (real/'manifest.json').write_text(json.dumps(samples),encoding='utf8')
                shutil.copytree(real,out/'real-control')
                env.update(FS_CONTRACT_SOLVERS=str(solver),FS_CONTRACT_REAL=str(real))
            r['compiler_sha256']=file_safety.compile_source(project,tools,run,env,out,a.timeout)
            r['instrumented_source_sha256']=common.fingerprint(work)[0];r['exe_sha256']=common.digest_file(exe)
            c=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (out/'runtime.log').write_bytes(c.stdout+b'\n'+c.stderr)
            n=json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig'));r['cases']=n['cases']
            if c.returncode or n.get('error'):raise RuntimeError(f'exit={c.returncode}; {n.get("error")}')
            if not a.solvers:r['cases'].append({'id':'11.contract.real-solvers','status':'not_run','reason':'Pass --solvers to run six real solver byte-parity checks.'})
            if common.fingerprint(src)[0]!=before or {n:common.digest_file(ROOT/n) for n in FILES}!=r['test_code_sha256']:raise RuntimeError('Source or tests changed during run')
            r['source_unchanged_during_run']=True;r['status']=common.classify_statuses([x['status'] for x in r['cases']])
    except Exception as e:r.update(status='error',error=f'{type(e).__name__}: {e}')
    r['summary']=dict(Counter(c['status'] for c in r['cases']))
    (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':r['status'],'summary':r['summary'],'error':r.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if r['status']=='pass' else 2 if r['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
