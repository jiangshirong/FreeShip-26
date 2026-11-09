"""Verify calculation callers restore cwd/cursor when the real plugin runner fails.

Only the disposable method bodies receive a fault at the start of their outer
try block, before calculation input is used. The wrapper, exception and finally
blocks are the production code. Maneuv.Calculate also runs its normal input
export and process launch against a zero-exit helper producing no output file.
This is not a numerical calculation test.
"""
import argparse
from collections import Counter
import json, os
from pathlib import Path
import shutil, subprocess, tempfile, time
import freeship_ui_test as common
import file_safety

ROOT=Path(__file__).resolve().parent
FILES=('process_callers.py','ProcessCallerProbe.pas','ProcessSafetyHelper.pas','file_safety.py','freeship_ui_test.py')
def hashes():return {n:common.digest_file(ROOT/n) for n in FILES}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    p.add_argument('--output',type=Path,default=ROOT/'reports'/('process-callers-'+time.strftime('%Y%m%d-%H%M%S')))
    p.add_argument('--timeout',type=int,default=240)
    a=p.parse_args();src=a.source.resolve();out=a.output.resolve()
    if out.exists() or out==src or src in out.parents:raise SystemExit('Output must be new and outside source')
    out.mkdir(parents=True)
    r={'status':'error','cases':[],'test_code_sha256':hashes(),'limitations':['Fault injected before calculation inputs in three actual caller methods; Maneuv.Calculate uses normal input export and a zero-exit helper. Other callers reviewed structurally.','No desktop interaction or numerical plugin acceptance.']}
    try:
        before,count=common.fingerprint(src);r.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-process-callers-') as run:
            work=run/'source';work.mkdir();evidence=run/'evidence';evidence.mkdir();profile=run/'profile';profile.mkdir()
            project,exe,snapshot=file_safety.prepare(src,work,tools,False)
            if snapshot!=before or common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during copy')
            shutil.copy2(ROOT/'ProcessCallerProbe.pas',work/'Units'/'ProcessCallerProbe.pas')
            file_safety.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},", "  ProcessCallerProbe in 'Units/ProcessCallerProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            file_safety.replace(work/'FreeShip.dpr','  Application.Run;','  ProcessCallerProbe.RunProcessCallerProbe(MainForm);')
            for name,title in [('Forms/FreePropeller_Task1Dlg.pas','procedure TFreePropeller_Task1.CalculateInWorkspace;'),('Forms/FreePropeller_Task4Dlg.pas','procedure TFreePropeller_Task4.CalculateInWorkspace;'),('Units/FreeEdit.inc','procedure TFreeEdit.File_Execute_AddMass;')]:
                q=work/name;t=q.read_text(encoding='utf8');start=t.index(title)
                anchor='  ProcessCallerCursor := Screen.Cursor;\n  try\n';at=t.index(anchor,start)+len(anchor)
                inject="    if SysUtils.GetEnvironmentVariable('FS_PROCESS_CALLER_FAULT')<>'' then begin\n      SetCurrentDirUTF8(SysUtils.GetEnvironmentVariable('FS_PROCESS_CALLER_DIRECTORY'));\n      Screen.Cursor:=crHourGlass;\n      ExecuteFreePlugin(GetCurrentDirUTF8, IncludeTrailingPathDelimiter(GetCurrentDirUTF8)+SysUtils.GetEnvironmentVariable('FS_PROCESS_CALLER_FAULT'),150);\n    end;\n"
                q.write_text(t[:at]+inject+t[at:],encoding='utf8')
            for filename in ('FreePropeller_Task1Dlg.pas','FreePropeller_Task4Dlg.pas'):
                q=work/'Forms'/filename
                text=q.read_text(encoding='utf8')
                import re
                text,n=re.subn(r'(public\s*\{\s*Public declarations\s*\})',r'\1\n    property ProbeShip: TFreeShip read FFreeship write FFreeship;\n  public',text,count=1)
                if n!=1:raise RuntimeError('No test property insertion point: '+filename)
                q.write_text(text,encoding='utf8')
            q=work/'Forms/FreeHydrodyn_ManeuvDlg.pas'
            file_safety.replace(q,'public { Public declarations }','public { Public declarations }\n    property ProbeShip: TFreeShip read FFreeship write FFreeship;\n  public')
            file_safety.replace(q,'  ProcessCallerCursor := Screen.Cursor;\n  try\n',"  ProcessCallerCursor := Screen.Cursor;\n  try\n    Screen.Cursor:=crHourGlass;\n")
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_PROCESS_CALLER_DIRECTORY=str(evidence),FS_PROCESS_CALLER_REPORT=str(evidence/'probe.json'))
            compiler=tools/'fpc/3.2.2/bin/x86_64-win64/fpc.exe'
            helper=evidence/'process-fail.exe'
            h=subprocess.run([str(compiler),'-FU'+str(evidence),'-o'+str(helper),str(ROOT/'ProcessSafetyHelper.pas')],cwd=evidence,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (out/'helper-build.log').write_bytes(h.stdout+b'\n'+h.stderr)
            if h.returncode:raise RuntimeError('Helper compilation failed')
            shutil.copy2(helper,evidence/'process-wait.exe')
            shutil.copy2(helper,evidence/'ManeuvPP.EXE')
            r['compiler_sha256']=file_safety.compile_source(project,tools,run,env,out,a.timeout)
            r['instrumented_source_sha256']=common.fingerprint(work)[0];r['exe_sha256']=common.digest_file(exe)
            c=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (out/'runtime.log').write_bytes(c.stdout+b'\n'+c.stderr)
            n=json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig'));r['cases']=n['cases']
            if c.returncode or n.get('error'):raise RuntimeError(f'exit={c.returncode}; {n.get("error")}')
            if len(r['cases'])!=23 or len({x['id'] for x in r['cases']})!=23:raise RuntimeError('Incomplete probe results')
            if common.fingerprint(src)[0]!=before or hashes()!=r['test_code_sha256']:raise RuntimeError('Source or tests changed during run')
            r['source_unchanged_during_run']=True;r['status']=common.classify_statuses([x['status'] for x in r['cases']])
    except Exception as e:r.update(status='error',error=f'{type(e).__name__}: {e}')
    r['summary']=dict(Counter(c['status'] for c in r['cases']))
    (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':r['status'],'summary':r['summary'],'error':r.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if r['status']=='pass' else 1
if __name__=='__main__':raise SystemExit(main())
