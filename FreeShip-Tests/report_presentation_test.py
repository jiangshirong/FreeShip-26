"""Compile and validate Unicode report units and native memo column positions."""
from pathlib import Path
import argparse,json,os,subprocess
import freeship_ui_test as c
import native_flicker_test as n
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--tables',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
r={'status':'error','scope':'Hidden native tables and bitmap render' if a.tables else 'Hidden native memo coordinates, not desktop visual acceptance'}
try:
 before=c.fingerprint(c.DEFAULT_SOURCE)[0]
 with c.temporary_workspace('fs-report-') as td:
  w=Path(td);r['build']=n.build(c.DEFAULT_SOURCE,w,c.DEFAULT_LAZARUS,out,'current','standalone',probe=Path(__file__).with_name('ReportTablesProbe.pas' if a.tables else 'ReportPresentationProbe.pas'))
  result=subprocess.run([str(w/'NativeLinesplanProbe.exe'),str(out/'tables.bmp')],cwd=w,capture_output=True,timeout=20,creationflags=c.FLAGS)
  (out/'result.log').write_bytes(result.stdout+result.stderr)
  r.update(status='pass' if result.returncode==0 else 'fail',exit_code=result.returncode,source_sha256=before)
 if a.tables and r.get('status')=='pass':
  from PIL import Image,ImageStat
  image=Image.open(out/'tables.bmp');variance=max(ImageStat.Stat(image.convert('RGB')).var)
  r['render_variance']=variance
  if variance<10:r.update(status='error',error='Bitmap capture is blank; no visual acceptance evidence')
 r['source_unchanged']=c.fingerprint(c.DEFAULT_SOURCE)[0]==before
 if not r['source_unchanged']:r['status']='error'
except Exception as e:r['error']=str(e)
(out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(r,ensure_ascii=False))
raise SystemExit(0 if r['status']=='pass' else 1)
