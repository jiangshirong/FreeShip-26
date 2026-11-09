"""Viewport camera/light: independent CPU/GPU math and real hidden UI callbacks.

No desktop control. Only private models/settings are used. Physical mouse, native
dialogs and multi-monitor appearance stay separate from these reproducible checks.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing


def read(path):return json.loads(Path(path).read_text('utf8'))


def run(r,runtime,out):
    model=out/'private-model.fbm';shutil.copy2(q.DEFAULT_MODELS[1],model)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service',native_linesplan=True) as s:
        response=q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'load'))
        for platform in ('offscreen','windows'):
            result=q.run_child(r,platform,runtime/'qt_camera_lighting_tests.exe',[response['model']],out/platform,platform=platform)
            r.data.setdefault('render_runs',[]).append(result)
    for platform in ('offscreen','windows'):
        prefix='production' if platform=='offscreen' else 'production-windows'
        result=q.run_child(r,prefix,runtime/'FreeShipQt.exe',['--verify-viewport',model],out/prefix,platform=platform);r.data[prefix]=result
        artifacts=result.get('artifacts',{})
        if 'saved_file' not in artifacts:
            r.case(prefix+'.postconditions',status='not_run',reason='Production workflow did not complete; inspect action discovery and earlier errors.');continue
        for name in ('model','scene','dxf'):r.compare(prefix+'.view-settings-keep-'+name,artifacts['after_'+name],artifacts['before_'+name])
        r.compare(prefix+'.fbm-byte-identical',artifacts['saved_file'],model)
    # These existing checks exercise overlap, alpha, hit-testing, animation and
    # navigation through the same renderer, without rebuilding an old baseline.
    for prefix,exe in [('model-ui','qt_model_ui_tests.exe'),('overlays','qt_overlay_tests.exe')]:
        q.run_child(r,prefix,runtime/exe,[response['model']],out/prefix,platform='offscreen')
    requests=out/'body-only.json';requests.write_text('[]',encoding='utf8')
    q.run_child(r,'body-display',runtime/'qt_display_tests.exe',[requests],out/'body-display',platform='offscreen')
    r.case('curvature-oracle',status='not_run',reason='This suite reuses body projection/selection/cache cases only; the independent curvature oracle belongs to qt-display.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for protected in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==protected or protected in out.parents:p.error('Use private output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-viewport',runtime=str(runtime),limitations=[
        'Hidden production actions and renderers do not establish desktop input or visual acceptance.',
        'Image timings include readback; they are not display frame rate.',
        'Camera and light settings are view-local and are not saved to FBM or across restart.',
        'Gaussian curvature, zebra stripes and developability shading are outside this suite.']);sources=q.current_sources(q.QT);start=time.perf_counter()
    protected={path:q.common.digest_file(path) for path in q.DEFAULT_MODELS};r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in ('qt_viewport.py','qt/camera_lighting_tests.cpp','qt_migration.py','run_tests.py')}
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.case('runtime-manifest',True);run(r,runtime,out)
        r.case('protected-models-unchanged',all(q.common.digest_file(path)==h for path,h in protected.items()));r.case('source-stable',q.current_sources(q.QT)==sources)
        r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
    except Exception as error:r.case('execution',status='error',reason=f'{type(error).__name__}: {error}')
    r.case('desktop-acceptance',status='not_run',reason='Hidden windows and OpenGL contexts do not establish desktop input or visual acceptance.');r.data['elapsed_s']=time.perf_counter()-start;editing.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
