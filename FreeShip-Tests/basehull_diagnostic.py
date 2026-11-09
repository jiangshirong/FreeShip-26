"""Isolated FBM hydrostatic report diagnostic; no desktop or physical validation."""
import argparse, json, os, shutil, subprocess, tempfile, time
from pathlib import Path
import freeship_ui_test as common
import file_safety as build

ROOT=Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model',required=True,type=Path)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    p.add_argument('--output',type=Path,default=ROOT/'reports'/('basehull-'+time.strftime('%Y%m%d-%H%M%S')))
    a=p.parse_args(); src=a.source.resolve(); out=a.output.resolve(); model=a.model.resolve()
    if out.exists() or out==src or src in out.parents: raise SystemExit('Output must be new and outside source')
    out.mkdir(parents=True); result={'status':'error','limitations':['Isolated native calls, not a desktop test or physical validation.']}
    try:
        before=common.fingerprint(src)[0]; mh=common.digest_file(model)
        result.update(source_sha256=before,model_sha256=mh)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-basehull-') as run:
            work=run/'source';work.mkdir();profile=run/'profile';profile.mkdir()
            project,exe,snapshot=build.prepare(src,work,tools,False)
            if snapshot!=before: raise RuntimeError('Source changed during snapshot')
            shutil.copy2(ROOT/'BaseHullProbe.pas',work/'Units/BaseHullProbe.pas')
            d=work/'FreeShip.dpr'
            build.replace(d,"  Main in 'Forms/Main.pas' {MainForm},","  BaseHullProbe in 'Units/BaseHullProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            build.replace(d,'{$IFDEF WINDOWS}{$R AppIcon26.res}{$ENDIF}','')
            build.replace(d,"   Application.Icon.LoadFromResourceName(HInstance,'APPICON26');",'')
            build.replace(d,'   Main.GShowSplash := ShowSplash;','   BaseHullProbe.Run; Halt(0);\n   Main.GShowSplash := ShowSplash;')
            copy=run/'model.fbm';shutil.copy2(model,copy)
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_BASEHULL_MODEL=str(copy),FS_BASEHULL_REPORT=str(run/'evidence.json'))
            result['compiler_sha256']=build.compile_source(project,tools,run,env,out,240)
            result['executable_sha256']=common.digest_file(exe)
            r=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=60,creationflags=common.FLAGS)
            (out/'runtime.log').write_bytes(r.stdout+b'\n'+r.stderr)
            if r.returncode: raise RuntimeError(f'Probe exit {r.returncode}')
            result.update(json.loads((run/'evidence.json').read_text(encoding='utf-8-sig')))
            if common.digest_file(copy)!=mh or common.digest_file(model)!=mh or common.fingerprint(src)[0]!=before: raise RuntimeError('Model or source changed')
            result['status']='completed' if all(c['status']=='completed' for c in result['cases']) else 'exception'
    except Exception as e: result['error']=f'{type(e).__name__}: {e}'
    (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':result['status'],'cases':result.get('cases'),'error':result.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if result['status']=='completed' else 1
if __name__=='__main__': raise SystemExit(main())
