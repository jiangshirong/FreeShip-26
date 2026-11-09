"""Measure hidden dialog preparation without desktop interaction."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
import freeship_ui_test as common
import native_flicker_test as native
ROOT=Path(__file__).resolve().parent
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=ROOT.parent/'FreeShip-MCP/examples/box.fbm');p.add_argument('--compare',type=Path);a=p.parse_args()
 out=a.output.resolve();source=a.source.resolve()
 if out==source or source in out.parents:p.error('Output must be outside source')
 out.mkdir(parents=True,exist_ok=False);report={'status':'error','limitations':['Hidden preparation timing; no native click-to-visible acceptance. Modal presentation is replaced by handle creation and cancellation.']}
 try:
  before=common.fingerprint(source)[0];report['source_sha256']=before
  test_hashes={n:common.digest_file(ROOT/n) for n in ('dialog_opening.py','DialogOpeningProbe.pas','native_flicker_test.py','freeship_ui_test.py')}
  model_hash=common.digest_file(a.model);report.update(test_code_sha256=test_hashes,model_sha256=model_hash)
  with common.temporary_workspace('fs-opening-') as td:
   root=Path(td);src=root/'source';src.mkdir();work=root/'build';work.mkdir()
   for name in common.FOLDERS:shutil.copytree(source/name,src/name)
   for f in source.iterdir():
    if f.is_file() and f.suffix.lower() in common.ROOT_SUFFIXES:shutil.copy2(f,src/f.name)
   for name in ('FreeProjectSettingsDlg.pas','FreeIntersectionDlg.pas'):
    path=src/'Forms'/name;s=path.read_text('utf-8-sig');anchor='  ShowModal;';assert s.count(anchor)==1,(name,s.count(anchor))
    replacement='  HandleNeeded; Realign; ModalResult:=mrCancel;'
    if name=='FreeProjectSettingsDlg.pas':replacement="  if UnitBox.Items[0]<>rsChoiceMetric then raise Exception.Create('Stale cached choice language');\n  if Edit1.Text<>GlobalFreeship.ProjectSettings.ProjectName then raise Exception.Create('Stale cached settings');\n  Edit1.Text:='cancelled draft';\n"+replacement
    path.write_text(s.replace(anchor,replacement),encoding='utf8')
   path=src/'Forms/FreeLinesplanFrm.pas';s=path.read_text('utf-8-sig');anchor='  Show;';assert s.count(anchor)==1;s=s.replace(anchor,'  HandleNeeded; Realign;');path.write_text(s,encoding='utf8')
   path=src/'Forms/FreeLinesplanFrme.pas';s=path.read_text('utf-8-sig')
   stages=[('  PaintStarted := GetTickCount64;', 'begin'),('  Mainplane.a := 1.0;', 'hydro-ready'),('      PrepareFillAtlas;', 'triangles-ready'),('      DrawFillTriangles(Above, [lvPlan]);', 'fill-ready'),('        FFillCacheKey := CacheKey;', 'cache-ready'),('    DrawPendingLabels;', 'labels-begin')]
   for anchor,stage in stages:
    assert s.count(anchor)==1,(anchor,s.count(anchor))
    s=s.replace(anchor,anchor+"\n  WriteLn('stage."+stage+"=',GetTickCount64-PaintStarted);")
   anchor='      PrepareFillAtlas;'
   s=s.replace(anchor,anchor+"\n      WriteLn('fill.triangles=',Below.Count+Above.Count);")
   for axis in (1,2,3):
    anchor='      SortTriangles(Above, '+str(axis)+');'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+"\n      WriteLn('stage.sort"+str(axis)+"=',GetTickCount64-PaintStarted);")
   path.write_text(s,encoding='utf8')
   report['build']=native.build(src,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'DialogOpeningProbe.pas')
   env=os.environ.copy()
   for key in ('TEMP','TMP','APPDATA','LOCALAPPDATA'):
    d=root/key;d.mkdir();env[key]=str(d)
   fixture=work/'fixture.fbm';shutil.copy2(a.model,fixture);env['FREESHIP_TEST_MODEL']=str(fixture)
   r=subprocess.run([str(work/'NativeLinesplanProbe.exe')],cwd=work,env=env,capture_output=True,timeout=180,creationflags=common.FLAGS)
   (out/'runtime.log').write_bytes(r.stdout+r.stderr);report['exit_code']=r.returncode
   report['timings']=r.stdout.decode('utf8','replace').splitlines()
   report['status']='partial' if r.returncode==0 else 'fail'
   report['cases']=[{'id':'dialog.behavior','status':'pass' if r.returncode==0 else 'fail'},{'id':'dialog.timing','status':'review'},{'id':'dialog.native-visible','status':'not_run'}]
   if r.returncode==0:
    from PIL import Image,ImageChops
    for n in range(1,4):
     cold=work/('cold-'+str(n)+'.bmp');warm=work/('warm-'+str(n)+'.bmp')
     equal=ImageChops.difference(Image.open(cold).convert('RGB'),Image.open(warm).convert('RGB')).getbbox() is None
     report['cases'].append({'id':'dialog.cache-pixels.'+str(n),'status':'pass' if equal else 'fail'})
     shutil.copy2(cold,out/cold.name);shutil.copy2(warm,out/warm.name)
     if not equal:report['status']='fail'
     if a.compare:
      old=a.compare/cold.name
      same=old.exists() and ImageChops.difference(Image.open(old).convert('RGB'),Image.open(cold).convert('RGB')).getbbox() is None
      report['cases'].append({'id':'dialog.reference-pixels.'+str(n),'status':'pass' if same else 'fail'})
      if not same:report['status']='fail'
   if r.returncode==0:
    cache_files=list((root/'LOCALAPPDATA'/'FreeShip26'/'LinesplanCache-v2').glob('*.fsc'))
    if not cache_files:raise RuntimeError('No persistent cache was saved')
    report['restart_runs']=[]
    for phase in ('restart','corrupt'):
     if phase=='corrupt':
      for cache in cache_files:
       data=bytearray(cache.read_bytes());data[len(data)//2]^=1;cache.write_bytes(data)
     again=subprocess.run([str(work/'NativeLinesplanProbe.exe')],cwd=work,env=env,capture_output=True,timeout=180,creationflags=common.FLAGS)
     lines=again.stdout.decode('utf8','replace').splitlines()
     (out/(phase+'.log')).write_bytes(again.stdout+again.stderr)
     had_build=any(x.startswith('fill.triangles=') for x in lines)
     ok=again.returncode==0 and (had_build==(phase=='corrupt'))
     report['restart_runs'].append({'phase':phase,'exit_code':again.returncode,'timings':lines})
     for n in range(1,4):
      old=out/('cold-'+str(n)+'.bmp');new=work/('cold-'+str(n)+'.bmp')
      ok=ok and ImageChops.difference(Image.open(old).convert('RGB'),Image.open(new).convert('RGB')).getbbox() is None
     report['cases'].append({'id':'dialog.disk.'+phase,'status':'pass' if ok else 'fail'})
     if not ok:report['status']='fail'
  if common.fingerprint(source)[0]!=before:raise RuntimeError('Source changed during run')
  if common.digest_file(a.model)!=model_hash:raise RuntimeError('Original model changed')
  if any(common.digest_file(ROOT/n)!=h for n,h in test_hashes.items()):raise RuntimeError('Test code changed during run')
 except Exception as e:report.update(status='error',error=str(e))
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(report,ensure_ascii=False));return 2 if report['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
