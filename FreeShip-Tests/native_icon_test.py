"""Read actual WM_GETICON pixels in a private full application, including handle recreation."""
import argparse,json,os,subprocess,time,shutil
from pathlib import Path
import freeship_ui_test as c
import native_flicker_test as n
from PIL import Image,ImageChops
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--blank-icon-control',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r={'status':'error','scope':'Only newly launched isolated instance; reads actual window icon pixels before and after handle recreation'}
try:
 before=c.fingerprint(c.DEFAULT_SOURCE)[0]
 with c.temporary_workspace('fs-icon-') as td:
  def prepare(work):
   if a.blank_icon_control:
    p=work/'FreeShip.dpr';s=p.read_text(encoding='utf-8-sig').replace('{$IFDEF WINDOWS}{$R AppIcon26.res}{$ENDIF}','').replace("   Application.Icon.LoadFromResourceName(HInstance,'APPICON26');",'');p.write_text(s,encoding='utf8')
  w=Path(td);r['blank_icon_control']=a.blank_icon_control;r['build']=n.build(c.DEFAULT_SOURCE,w,c.DEFAULT_LAZARUS,out,'current','full-app',app_probe=Path(__file__).with_name('NativeIconProbe.pas'),prepare=prepare)
  env=os.environ.copy()
  for k in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
   d=w/'profile'/k;d.mkdir(parents=True,exist_ok=True);env[k]=str(d)
  shutil.copy2(c.FIXTURE,w/'input.fbm');ready=w/'ready.json'
  with (out/'runtime.log').open('wb') as log:
   proc=subprocess.Popen([str(w/'NativeLinesplanProbe.exe'),str(w/'input.fbm'),str(ready),'private-icon-test'],cwd=w,env=env,stdout=log,stderr=log,creationflags=c.FLAGS)
   try:proc.wait(timeout=45)
   finally:
    if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
  if not ready.exists():raise RuntimeError('Private process did not produce icon evidence')
  r['owned_process']=json.loads(ready.read_text());assert r['owned_process']['pid']==proc.pid
  for f in w.glob('ready.json.*.bmp'):shutil.copy2(f,out/f.name)
  expected=Image.open(out/'ready.json.expected.bmp').convert('RGB');blank=Image.new('RGB',(32,32),'white');assert ImageChops.difference(expected,blank).getbbox(),'expected icon blank'
  r['checks']=[]
  for f in out.glob('ready.json.*-*.bmp'):
   if 'resource-main' in f.name:continue
   im=Image.open(f).convert('RGB');r['checks'].append({'image':f.name,'matches_embedded_logo':ImageChops.difference(im,expected).getbbox() is None,'nonblank':ImageChops.difference(im,blank).getbbox() is not None})
  r['status']='pass' if len(r['checks'])==10 and all(x['matches_embedded_logo'] for x in r['checks']) else 'fail'
 r['source_sha256']=before;r['source_unchanged']=c.fingerprint(c.DEFAULT_SOURCE)[0]==before
 if not r['source_unchanged']:r['status']='error'
except Exception as e:r['error']=str(e)
(out/'report.json').write_text(json.dumps(r,indent=2),encoding='utf8');print(json.dumps(r));raise SystemExit(r['status']!='pass')
