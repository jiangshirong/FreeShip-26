"""Model editing UI: layer order/opacity, placement, labels and current-view export.

Private FBM copies and hidden production callbacks only. Expected shape, ownership,
compositing and projected coordinates are independent of widget layout rules.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing
import qt_native_project as project


def read(path):
    return json.loads(Path(path).read_text('utf8'))


def spatial(model):
    return {k:model[k] for k in ('units','bounds','control_points','control_edges','control_faces','triangles')}


def order(model):
    layers={layer['id']:layer for layer in model['layers']}
    return [layers[i] for i in model.get('layer_order',list(layers))]


def named_faces(model):
    layers={l['id']:l['name'] for l in model['layers']}
    return [dict(f,layer=layers[f['layer']]) for f in model['control_faces']]


def native(r,runtime,out,model):
    out.mkdir(parents=True)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        initial=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()));base=read(initial['model']);layer=base['layers'][0]['id']
        original=call('initial-save','save')['saved_file']
        changed=call('alpha','update_layer',layer_id=layer,changes={'alpha_blend':128});alpha=read(changed['model'])
        r.case('native.opacity-preserves-model-geometry',spatial(base)==spatial(alpha))
        r.case('native.opacity-is-published',alpha['layers'][0]['alpha_blend']==128)
        r.compare('native.opacity-preserves-dxf',initial['dxf'],changed['dxf'])
        saved=call('alpha-save','save')['saved_file'];call('alpha-undo','undo');r.compare('native.alpha-undo-exact-fbm',call('alpha-undo-save','save')['saved_file'],original)
        call('alpha-redo','redo');r.compare('native.alpha-redo-exact-fbm',call('alpha-redo-save','save')['saved_file'],saved)
        call('new-layer','manage_layers',operation='create',changes={'name':'Second layer','alpha_blend':64})
        before=call('before-order','snapshot');before_data=read(before['model']);before_save=call('before-order-save','save')['saved_file']
        moved=call('order','manage_layers',operation='move',layer_id=layer,direction=1);moved_data=read(moved['model'])
        r.case('native.order-preserves-stable-geometry',spatial(before_data)==spatial(moved_data))
        r.case('native.order-is-published',[l['id'] for l in order(moved_data)]==[l['id'] for l in order(before_data)][::-1])
        r.compare('native.reorder-preserves-dxf',before['dxf'],moved['dxf'])
        moved_save=call('moved-save','save')['saved_file'];call('order-undo','undo');r.compare('native.order-undo-exact-fbm',call('order-undo-save','save')['saved_file'],before_save)
        call('order-redo','redo');r.compare('native.order-redo-exact-fbm',call('order-redo-save','save')['saved_file'],moved_save)
        state=dict(s.state);call('edge-move','manage_layers',operation='move',layer_id=layer,direction=1)
        r.case('native.boundary-move-does-not-create-history',s.state==state)
        for name,method,fields in [('invalid-alpha','update_layer',dict(layer_id=layer,changes={'alpha_blend':256})),('fractional-alpha','update_layer',dict(layer_id=layer,changes={'alpha_blend':12.5})),('invalid-direction','manage_layers',dict(operation='move',layer_id=layer,direction=0))]:
            result=s.command(method,output=out/name,**fields);r.case(name+'.rejected-without-mutation',not result['ok'] and s.state==state)
        blocked=out/'blocked';blocked.write_text('preserve',encoding='utf8');result=s.command('update_layer',layer_id=layer,changes={'alpha_blend':37},output=blocked)
        r.case('native.failed-publication-rolls-back-alpha',not result['ok'] and s.state==state and blocked.read_text('utf8')=='preserve')
        r.compare('native.failed-publication-preserves-fbm',call('recovered-save','save')['saved_file'],moved_save)
        reopened=call('reopen','load',path=str(moved_save),document_id=str(q.uuid.uuid4()));reopened_data=read(reopened['model'])
        r.case('native.reopen-restores-layer-order-and-opacity',[(l['name'],l['alpha_blend']) for l in order(reopened_data)]==[(l['name'],l['alpha_blend']) for l in order(moved_data)])
        r.case('native.reopen-preserves-face-layer-ownership',named_faces(reopened_data)==named_faces(moved_data))
        r.compare('native.reopen-preserves-dxf',moved['dxf'],reopened['dxf'])
        q.run_child(r,'independent-ui',runtime/'qt_model_ui_tests.exe',[reopened['model']],out/'independent',platform='offscreen')
        return initial['model']


def production(r,runtime,out,model):
    result=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-model-ui',model],out,platform='offscreen')
    r.data['production']=result
    artifacts=result.get('artifacts',{})
    if 'redo-point_model' not in artifacts:
        r.case('production.postconditions',status='not_run',reason='Workflow did not reach final stage; inspect action observations and earlier errors.');return
    models={k:read(artifacts[k+'_model']) for k in ('original','alpha','reordered','undo-reorder','redo-reorder','reopened','point','undo-point','redo-point')}
    r.case('production.alpha-keeps-geometry',spatial(models['original'])==spatial(models['alpha']))
    r.case('production.undo-redo-keeps-face-ownership',models['reordered']['control_faces']==models['undo-reorder']['control_faces']==models['redo-reorder']['control_faces'])
    r.case('production.reopen-keeps-layer-order',[l['name'] for l in order(models['reordered'])]==[l['name'] for l in order(models['reopened'])])
    r.case('production.reopen-keeps-face-ownership',named_faces(models['reordered'])==named_faces(models['reopened']))
    r.case('production.point-undo-restores-model',spatial(models['undo-point'])==spatial(models['reopened']))
    r.case('production.point-redo-restores-model',spatial(models['redo-point'])==spatial(models['point']))
    measurement=artifacts['measurements'];r.case('production.box-independent-measurements',abs(measurement['area']-536)<1e-6 and abs(measurement['weight']-10.72)<1e-6 and abs(measurement['center'][0]-10)<1e-6 and abs(measurement['center'][1])<1e-6 and abs(measurement['center'][2]-832/268)<1e-6)
    for phase in ('alpha','reordered','undo-reorder','redo-reorder','reopened'):
        r.compare('production.dxf-unchanged-'+phase,artifacts['original_dxf'],artifacts[phase+'_dxf'])


def legacy_alpha(r,runtime,out):
    out.mkdir();model=out/'v20.fbm';project.fixture(model,version=20)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service',native_linesplan=True) as s:
        before=q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'before'));original=read(before['model'])
        after=q.require_ok(s.command('update_layer',layer_id=original['layers'][0]['id'],changes={'alpha_blend':137},output=out/'after'))
        r.case('legacy-v20.alpha-upgrade-keeps-geometry',spatial(read(after['model']))==spatial(original))
        r.compare('legacy-v20.alpha-keeps-dxf',before['dxf'],after['dxf'])
        saved=q.require_ok(s.command('save',output=out/'save'))['saved_file'];reopened=q.require_ok(s.command('load',path=str(saved),document_id=str(q.uuid.uuid4()),output=out/'reopened'))
        r.case('legacy-v20.alpha-upgrade-persists-exact-value',read(reopened['model'])['layers'][0]['alpha_blend']==137)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for protected in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==protected or protected in out.parents:p.error('Use private test output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-model-ui',runtime=str(runtime));start=time.perf_counter();sources=q.current_sources(q.QT)
    originals={path:q.common.digest_file(path) for path in q.DEFAULT_MODELS};r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in ('qt_model_ui.py','qt/model_ui_tests.cpp','qt_migration.py','run_tests.py')}
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Runtime source manifest is stale')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file hash mismatch')
        r.case('runtime-manifest',True);model=out/'模型-Женя.fbm';shutil.copy2(q.DEFAULT_MODELS[0],model)
        snapshot=native(r,runtime,out/'native',model);production(r,runtime,out/'production',model);legacy_alpha(r,runtime,out/'legacy-alpha')
        for prefix,exe in [('overlays','qt_overlay_tests.exe'),('auxiliary','qt_auxiliary_tests.exe'),('editing','qt_editing_tests.exe')]:
            q.run_child(r,prefix,runtime/exe,[snapshot],out/prefix,platform='offscreen')
        auxiliary=out/'flow-hull.fbm';shutil.copy2(q.DEFAULT_MODELS[1],auxiliary)
        q.run_child(r,'production-flow',runtime/'FreeShipQt.exe',['--verify-auxiliary',auxiliary],out/'production-flow',platform='offscreen')
        r.case('protected-models-unchanged',all(q.common.digest_file(path)==digest for path,digest in originals.items()))
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
    except Exception as e:r.case('execution',status='error',reason=f'{type(e).__name__}: {e}')
    r.case('desktop-native-print-acceptance',status='not_run',reason='No physical mouse, native dialogs, printer or multi-monitor acceptance.');r.data['elapsed_s']=time.perf_counter()-start;editing.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
