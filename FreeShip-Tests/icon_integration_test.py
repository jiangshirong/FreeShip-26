"""Validate embedded icon loading, image slot collisions and actual toolbar control renders."""
from pathlib import Path
import argparse,json,subprocess
import freeship_ui_test as c
import native_flicker_test as n
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
r={'status':'error','scope':'Isolated native resource and toolbar renders; not physical hover or click acceptance'}
try:
 before=c.fingerprint(c.DEFAULT_SOURCE)[0]
 with c.temporary_workspace('fs-icons-') as td:
  w=Path(td);r['build']=n.build(c.DEFAULT_SOURCE,w,c.DEFAULT_LAZARUS,out,'current','standalone',probe=Path(__file__).with_name('IconIntegrationProbe.pas'))
  result=subprocess.run([str(w/'NativeLinesplanProbe.exe'),str(out)],cwd=w,capture_output=True,timeout=60,creationflags=c.FLAGS)
  (out/'result.log').write_bytes(result.stdout+result.stderr);r['exit_code']=result.returncode
  r['status']='pass' if result.returncode==0 else 'fail'
  r['source_sha256']=before
 r['source_unchanged']=c.fingerprint(c.DEFAULT_SOURCE)[0]==before
 if not r['source_unchanged']:r['status']='error'
except Exception as e:r['error']=str(e)
(out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(r,ensure_ascii=False));raise SystemExit(0 if r['status']=='pass' else 1)
