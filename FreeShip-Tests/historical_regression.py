"""Run maintained historical probes; audit every original script by hash."""
from pathlib import Path
import argparse, json, os, shutil, subprocess, sys
from collections import Counter
import freeship_ui_test as common
import native_flicker_test as native
ROOT = Path(__file__).resolve().parent

def validate_catalog(root, entries):
 expected={e['source'] for e in entries}
 actual={p.relative_to(root).as_posix() for p in (root/'historical').rglob('*') if p.suffix in ('.pas','.py','.ps1')}
 if expected!=actual or len(expected)!=len(entries):raise RuntimeError('Historical inventory changed or duplicated')
 for e in entries:
  if e['disposition'] not in ('integrated','replaced','retired','non_test_tool') or not e['reason']:
   raise RuntimeError('Missing reviewed disposition')
  if common.digest_file(root/e['source'])!=e['sha256']:raise RuntimeError('Historical source changed: '+e['source'])
  if e['disposition']=='integrated':
   probe=(root/e['path']).resolve()
   if not probe.is_relative_to((root/'probes/history').resolve()) or not probe.is_file():raise RuntimeError('Missing maintained probe')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,required=True)
 p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
 p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
 p.add_argument('--model',type=Path,default=ROOT.parent/'FreeShip-MCP/examples/box.fbm')
 p.add_argument('--run-reviewed',action='store_true')
 p.add_argument('--probe',action='append')
 a=p.parse_args();out=a.output.resolve();repo=a.source.resolve()
 if out==repo or repo in out.parents: p.error('Output must be outside product source')
 out.mkdir(parents=True,exist_ok=False)
 entries=json.loads((ROOT/'historical_manifest.json').read_text('utf8'))['entries']
 report={'status':'error','entries':entries,'counts':dict(Counter(e['disposition'] for e in entries)), 'cases':[],
 'limitations':['Hidden methods only; no desktop input or visual acceptance.',
 'Retirement is a review decision, not a test pass. Historical layout/timing preferences remain observations.']}
 def save():
  (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 try:
  validate_catalog(ROOT, entries)
  probes=[e for e in entries if e['disposition']=='integrated']
  probes.append({'probe':'historical-ui','path':'historical_ui_probe.pas','mode':'observation'})
  probes.append({'probe':'historical-icons','path':'historical_icons_probe.pas','mode':'assertion'})
  probes.append({'probe':'historical-progress','path':'historical_progress_probe.pas','mode':'assertion'})
  if a.probe:
   unknown=set(a.probe)-{e['probe'] for e in probes}
   if unknown:raise ValueError('Unknown probes: '+str(unknown))
   probes=[e for e in probes if e['probe'] in a.probe]
  deps=['historical_regression.py','historical_manifest.json','native_flicker_test.py','freeship_ui_test.py']+[e['path'] for e in probes]
  hashes={n:common.digest_file(ROOT/n) for n in deps}
  before=common.fingerprint(repo)[0]; model_hash=common.digest_file(a.model)
  report.update(source_sha256=before,test_code_sha256=hashes,model_sha256=model_hash)
  for e in probes:
   case={'id':e['probe'],'status':'not_run','mode':e['mode']};report['cases'].append(case)
   if not a.run_reviewed:continue
   folder=out/e['probe'];folder.mkdir();case['status']='error'
   try:
    with common.temporary_workspace('fs-history-') as td:
     work=Path(td)
     case['build']=native.build(repo,work,a.lazarus.resolve(),folder,'current','standalone',probe=ROOT/e['path'])
     env=os.environ.copy()
     for key in ('TEMP','TMP','APPDATA','LOCALAPPDATA'):
      d=work/'profile'/key;d.mkdir(parents=True,exist_ok=True);env[key]=str(d)
     (Path(env['TEMP'])/'FreeShipLabelIntegration').mkdir(exist_ok=True)
     fixture=work/'fixture.fbm';shutil.copy2(a.model,fixture);env['FREESHIP_TEST_MODEL']=str(fixture)
     result=subprocess.run([str(work/'NativeLinesplanProbe.exe')],cwd=work,env=env,capture_output=True,timeout=35,creationflags=common.FLAGS)
     (folder/'result.log').write_bytes(result.stdout+result.stderr)
     case.update(exit_code=result.returncode, status=('pass' if result.returncode==0 else 'fail'))
     if e['mode']=='observation' and result.returncode in (0,1,2,3,4,5):
      case.update(status='review',historical_expectation_met=result.returncode==0)
     for base in (work,Path(env['TEMP'])/'FreeShipLabelIntegration'):
      for artifact in base.iterdir():
       if artifact.suffix.lower() in ('.bmp','.png','.csv') or artifact.name=='observations.json':shutil.copy2(artifact,folder/artifact.name)
   except Exception as exc:case['error']=str(exc)
   save();print(json.dumps(case,ensure_ascii=False),flush=True)
  if common.fingerprint(repo)[0]!=before:raise RuntimeError('Product source changed during run')
  if any(common.digest_file(ROOT/n)!=h for n,h in hashes.items()):raise RuntimeError('Test source changed during run')
  if common.digest_file(a.model)!=model_hash:raise RuntimeError('Original fixture changed')
  statuses=[c['status'] for c in report['cases']]
  report['status']=common.classify_statuses(statuses)
  report['source_unchanged']=True
 except Exception as exc:report.update(status='error',error=str(exc))
 report['summary']=dict(Counter(c['status'] for c in report['cases']));save()
 print(json.dumps({'status':report['status'],'summary':report['summary'],'counts':report['counts']},ensure_ascii=False))
 return {'pass':0,'partial':2,'review':2,'not_run':2,'fail':1,'error':3}.get(report['status'],3)
if __name__=='__main__':raise SystemExit(main())
