"""Real developed-plate geometry and dimensional consistency in an isolated application build."""
from __future__ import annotations
import argparse, json, os, shutil, subprocess, tempfile, time
from collections import Counter
from pathlib import Path
import freeship_ui_test as common
import file_safety as build
ROOT=Path(__file__).resolve().parent
FILES=('plate_errors.py','PlateErrorsProbe.pas','freeship_ui_test.py','file_safety.py')
def hashes():return {p:common.digest_file(ROOT/p) for p in FILES}
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output',type=Path,default=ROOT/'reports'/('plate-errors-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout',type=int,default=240)
    ap.add_argument('--mutation',choices=('none','legacy-area','legacy-score'),default='none',help='Negative control applied only in the disposable copy')
    a=ap.parse_args();src=a.source.resolve();out=a.output.resolve()
    if out.exists() or out==src or src in out.parents:raise SystemExit('Output must be new and outside the source')
    out.mkdir(parents=True);r={'status':'error','cases':[],'test_code_sha256':hashes(),'mutation':a.mutation,'limitations':[
      'Uses actual Unroll, mirror copying and hidden form labels on constructed planar, folded and saddle surfaces.',
      'No material-forming, collision or native screen acceptance; arbitrary scales and meshes are not exhaustively proven.']}
    try:
      before,count=common.fingerprint(src);r.update(source_sha256=before,source_file_count=count)
      tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
      with common.temporary_workspace('fs-plate-errors-') as run:
        work=run/'source';work.mkdir();profile=run/'profile';profile.mkdir()
        project,exe,snapshot=build.prepare(src,work,tools,False)
        if snapshot!=before or common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during snapshot')
        shutil.copy2(ROOT/'PlateErrorsProbe.pas',work/'Units/PlateErrorsProbe.pas')
        build.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},","  PlateErrorsProbe in 'Units/PlateErrorsProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
        build.replace(work/'FreeShip.dpr','  Application.Run;','  Application.ShowMainForm:=False;\n  PlateErrorsProbe.RunPlateErrorsProbe(MainForm);')
        build.replace(work/'Units/FreeGeometry.pas','    property NormalizedError: extended read FNormalizedError;',
          '    property NormalizedError: extended read FNormalizedError;\n    function ProbePointCount:Integer;')
        build.replace(work/'Units/FreeDevelopedPatch.inc','function TFreeDevelopedPatch.FGetMaxError: extended;',
          'function TFreeDevelopedPatch.ProbePointCount:Integer; begin Result:=FPoints.Count end;\n\nfunction TFreeDevelopedPatch.FGetMaxError: extended;')
        form=work/'Forms/FreeExpanedPlatesDlg.pas'
        build.replace(form,'  public    { Public declarations }','  public    { Public declarations }\n    procedure ProbeBind(Ship:TFreeShip; Plates:TFasterListTFreeDevelopedPatch);')
        build.replace(form,'function GetGridSpacing(OveralSize: TFloatType): TFloatType;',
          'procedure TFreeExpanedplatesDialog.ProbeBind(Ship:TFreeShip; Plates:TFasterListTFreeDevelopedPatch);\nbegin\n  FFreeShip:=Ship; FPlates:=Plates; InitViewPort; FUpdateListBox; ActivePatch:=Plates[0];\nend;\n\nfunction GetGridSpacing(OveralSize: TFloatType): TFloatType;')
        patch=work/'Units/FreeDevelopedPatch.inc'
        if a.mutation=='legacy-area':
          build.replace(patch,'    Result := 0.5 * Abs(Ux * Vy - Uy * Vx);',
            '    Result := 0.5*((P1.X-P2.X)*(P1.Y-P2.Y)+(P2.X-P3.X)*(P2.Y-P3.Y)+(P3.X-P1.X)*(P3.Y-P1.Y));')
        elif a.mutation=='legacy-score':
          build.replace(patch,'    MaxError := FNormalizedError;',
            '    FNormalizedError := MaxError + Abs(FTotalAreaError);\n    MaxError := FNormalizedError;')
        r['instrumented_source_sha256']=common.fingerprint(work)[0]
        env=os.environ.copy();env.update(APPDATA=str(profile),LOCALAPPDATA=str(profile),TEMP=str(run),TMP=str(run),FS_PLATE_OUTPUT=str(run))
        r['compiler_sha256']=build.compile_source(project,tools,run,env,out,a.timeout)
        r['exe_sha256']=common.digest_file(exe)
        proc=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
        (out/'runtime.log').write_bytes(proc.stdout+b'\n'+proc.stderr)
        native=json.loads((run/'probe.json').read_text(encoding='utf-8-sig'))
        (out/'native.json').write_text(json.dumps(native,ensure_ascii=False,indent=2),encoding='utf8')
        r['cases']=native['cases']
        if proc.returncode or native.get('error'):raise RuntimeError(f'exit={proc.returncode}; {native.get("error")}')
        if len({c['id'] for c in r['cases']})!=len(r['cases']) or len(r['cases'])!=70:raise RuntimeError('Incomplete or duplicate checks')
        if common.fingerprint(src)[0]!=before or hashes()!=r['test_code_sha256']:raise RuntimeError('Source or test code changed during run')
        r['source_unchanged_during_run']=True
        r['cases'].append({'id':'16.native-display','status':'not_run','expected':'native labels and hints render correctly','actual':'no desktop operation; hidden form captions tested'})
        r['status']=common.classify_statuses([c['status'] for c in r['cases']])
    except Exception as e:r.update(status='error',error=f'{type(e).__name__}: {e}')
    r['summary']=dict(Counter(c['status'] for c in r['cases']))
    (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':r['status'],'summary':r['summary'],'error':r.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if r['status']=='pass' else 2 if r['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
