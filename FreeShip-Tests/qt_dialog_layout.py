"""StudioLight core dialogs: retained font/window/DPI layout regression.

Runs the production widgets in a private, hidden Windows process. Screenshots
are review evidence, not physical desktop or multi-monitor acceptance.
"""
import argparse
import json
import os
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing

FILES=('qt/dialog_layout_tests.cpp','qt/editing_tests.cpp','qt_dialog_layout.py','qt_migration.py','qt_editing.py','run_tests.py')

def workflows(r,runtime,out):
    original=q.DEFAULT_MODELS[0];digest=q.common.digest_file(original)
    model=out/'input.fbm';shutil.copy2(original,model)
    try:
        with q.Service(runtime/'FreeShipDocumentService.exe',out/'snapshot-service',native_linesplan=True) as service:
            state=q.require_ok(service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'snapshot'))
        r.data['editing_widgets']=q.run_child(r,'editing-widgets',runtime/'qt_editing_tests.exe',[state['model']],out/'editing-widgets',platform='windows')
        for name in ('editing','sections'):
            r.data[name+'_workflow']=q.run_child(r,name+'-workflow',runtime/'FreeShipQt.exe',['--verify-'+name,model],out/name,platform='windows')
    finally:
        r.case('original-model-preserved',q.common.digest_file(original)==digest)
        r.case('private-input-preserved',q.common.digest_file(model)==digest)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic()
    sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    r.data.update(suite='qt-dialog-layout',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,
        test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},runs=[])
    original_scale=os.environ.get('QT_SCALE_FACTOR')
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime sources')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        r.case('runtime-manifest',True)
        base_ratio=None
        for scale in ('1','1.5','2'):
            os.environ['QT_SCALE_FACTOR']=scale
            result=q.run_child(r,'scale-'+scale,runtime/'qt_dialog_layout_tests.exe',[],out/('scale-'+scale),timeout=90,platform='windows')
            # QT_SCALE_FACTOR multiplies the host monitor's native scale (125%
            # here); it does not replace it. Validate the multiplier explicitly.
            if base_ratio is None:base_ratio=result.get('device_pixel_ratio',0)
            r.case('scale-'+scale+'.requested-device-ratio',base_ratio>0 and abs(result.get('device_pixel_ratio',0)-base_ratio*float(scale))<0.01)
            r.data['runs'].append(result)
        workflows(r,runtime,out)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        if original_scale is None:os.environ.pop('QT_SCALE_FACTOR',None)
        else:os.environ['QT_SCALE_FACTOR']=original_scale
        r.case('qt-source-stable',q.current_sources(q.QT)==sources)
        r.case('legacy-source-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.data['elapsed_s']=time.monotonic()-start;editing.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
