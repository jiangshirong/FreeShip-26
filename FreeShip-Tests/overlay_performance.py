"""Isolated four-pane overlay draw workload; not desktop FPS acceptance."""
from pathlib import Path
import argparse,json,subprocess,re
import freeship_ui_test as c
import native_flicker_test as n
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--compare',type=Path);p.add_argument('--cache-check',action='store_true');p.add_argument('--label-reveal',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
r={'status':'error','scope':('Label width reveal across all label kinds; no physical pan acceptance' if a.label_reveal else ('Animation base cache semantics; no desktop acceptance' if a.cache_check else 'Four sequential 960x540 synthetic overlay frames; QPC draw timings, not native displayed FPS'))}
try:
 before=c.fingerprint(c.DEFAULT_SOURCE)[0]
 with c.temporary_workspace('fs-overlay-') as td:
  w=Path(td);r['build']=n.build(c.DEFAULT_SOURCE,w,c.DEFAULT_LAZARUS,out,'current','standalone',probe=Path(__file__).with_name('LabelRevealProbe.pas' if a.label_reveal else ('AnimationBaseProbe.pas' if a.cache_check else 'OverlayPerformanceProbe.pas')))
  result=subprocess.run([str(w/'NativeLinesplanProbe.exe'),str(out)],cwd=w,capture_output=True,timeout=60,creationflags=c.FLAGS)
  text=(result.stdout+result.stderr).decode(errors='replace');(out/'result.log').write_text(text)
  r['timings_ms']={key:float(value) for key,value in re.findall(r'(cold_ms|p50_ms|p95_ms|max_ms)=([0-9.]+)',text)}
  r['status']='pass' if result.returncode==0 and (a.cache_check or a.label_reveal or len(r['timings_ms'])==4) else 'fail'
  r['source_sha256']=before
 r['source_unchanged']=c.fingerprint(c.DEFAULT_SOURCE)[0]==before
 if not r['source_unchanged']:r['status']='error'
 if a.compare:
  old=json.loads((a.compare/'report.json').read_text());r['baseline_timings_ms']=old.get('timings_ms')
except Exception as e:r['error']=str(e)
(out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(r,ensure_ascii=False));raise SystemExit(0 if r['status']=='pass' else 1)
