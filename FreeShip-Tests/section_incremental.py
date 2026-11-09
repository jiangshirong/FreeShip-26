"""Compare per-section reuse after actual model edits against full recomputation."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
import freeship_ui_test as common
import native_flicker_test as native
ROOT=Path(__file__).resolve().parent

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
 p.add_argument('--model',type=Path,default=common.FIXTURE)
 p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();source=a.source.resolve();out=a.output.resolve()
 if out==source or source in out.parents:p.error('Output must be outside source')
 out.mkdir(parents=True,exist_ok=False);report={'status':'error','cases':[],'limitations':['Native hidden model operations; no desktop acceptance.']}
 try:
  before=common.fingerprint(source)[0];report['source_sha256']=before
  report['model_sha256']=common.digest_file(a.model)
  names=('section_incremental.py','SectionIncrementalProbe.pas','native_flicker_test.py','freeship_ui_test.py')
  report['test_code_sha256']={n:common.digest_file(ROOT/n) for n in names}
  with common.temporary_workspace('fs-sections-') as td:
   root=Path(td);src=root/'source';src.mkdir();work=root/'build';work.mkdir()
   for name in common.FOLDERS:shutil.copytree(source/name,src/name)
   for f in source.iterdir():
    if f.is_file() and f.suffix.lower() in common.ROOT_SUFFIXES:shutil.copy2(f,src/f.name)
   report['build']=native.build(src,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'SectionIncrementalProbe.pas')
   env=os.environ.copy()
   for key in ('TEMP','TMP','APPDATA','LOCALAPPDATA'):
    d=root/key;d.mkdir();env[key]=str(d)
   fixture=work/'fixture.fbm';shutil.copy2(a.model,fixture);env['FREESHIP_TEST_MODEL']=str(fixture)
   r=subprocess.run([str(work/'NativeLinesplanProbe.exe')],cwd=work,env=env,capture_output=True,timeout=180,creationflags=common.FLAGS)
   (out/'runtime.log').write_bytes(r.stdout+r.stderr)
   report.update(status='pass' if r.returncode==0 else 'fail',exit_code=r.returncode,observations=r.stdout.decode('utf8','replace').splitlines())
   report['cases']=[{'id':'sections.incremental-vs-full','status':report['status']}]
  if common.fingerprint(source)[0]!=before:raise RuntimeError('Source changed during run')
  if common.digest_file(a.model)!=report['model_sha256']:raise RuntimeError('Original model changed')
  if any(common.digest_file(ROOT/n)!=h for n,h in report['test_code_sha256'].items()):raise RuntimeError('Test changed during run')
 except Exception as e:report.update(status='error',error=str(e))
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(report,ensure_ascii=False))
 return 0 if report['status']=='pass' else 1
if __name__=='__main__':raise SystemExit(main())
