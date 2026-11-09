"""Native document publication: geometry/export parity, metadata and independence.

Uses owned hidden backend processes, not desktop automation. --oracle-runtime
can point to a frozen pre-migration package; otherwise the isolated Pascal
bridge is the independent publication oracle for versions it supports.
"""
import argparse
import copy
import json
import time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_document as document
import qt_native_project as project
import fbm_preview_bytes as preview_bytes


def load_json(path):
    return json.loads(Path(path).read_text('utf8'))


def differences(a,b,path='',result=None):
    result=[] if result is None else result
    if len(result)>=24:return result
    if isinstance(a,dict) and isinstance(b,dict):
        for key in sorted(a.keys()|b.keys()):
            if key not in a or key not in b:result.append(dict(path=path+'/'+key,native=a.get(key),oracle=b.get(key)))
            else:differences(a[key],b[key],path+'/'+key,result)
    elif isinstance(a,list) and isinstance(b,list):
        if len(a)!=len(b):result.append(dict(path=path,length_native=len(a),length_oracle=len(b)))
        else:
            for i,(x,y) in enumerate(zip(a,b)):differences(x,y,path+'/'+str(i),result)
    elif a!=b:result.append(dict(path=path,native=a,oracle=b))
    return result


def input_point_order(value,ids):
    """Use the oracle's recorded stable IDs, never nearest-coordinate matching.

    Its SaveProject sorts point addresses before serialization. Reindex every
    point reference while retaining exact coordinates, types and connectivity.
    """
    result=copy.deepcopy(value);g=result['geometry'];order=sorted(range(len(ids)),key=lambda i:ids[i])
    assert len(ids)==len(g['points']) and len(set(ids))==len(ids)
    positions={old:new for new,old in enumerate(order)}
    g['points']=[g['points'][i] for i in order]
    for kind in ('edges','faces','curves'):
        for item in g[kind]:item['points']=[positions[i] for i in item['points']]
    return result


def paired(r,runtime,oracle_runtime,model,out):
    identity=str(q.uuid.uuid4());records=[];timings=[]
    oracle=oracle_runtime/'FreeShipDocumentService.exe' if oracle_runtime else runtime/'bridge/FreeShipGeometryBridge.exe'
    for tag,exe in [('native',runtime/'FreeShipDocumentService.exe'),('oracle',oracle)]:
        with q.Service(exe,out/tag/'process',native_linesplan=True) as service:
            start=time.perf_counter()
            response=q.require_ok(service.command('load',path=str(model),document_id=identity,native_surface=True,output=out/tag/'load'))
            timings.append(dict(kind=tag,load_ms=(time.perf_counter()-start)*1000,prepare_ms=response.get('publication_prepare_ms')))
            if tag=='oracle' and not oracle_runtime:
                before=dict(service.state)
                response=q.require_ok(service.command('snapshot',external_document=True,checkpoint=True,native_surface=True,output=out/tag/'bound-snapshot'))
                r.case(out.name+'.oracle-checkpoint-readonly',service.state==before)
            value=load_json(Path(response['model']).parent/'native-input.json')
            if tag=='oracle' and not oracle_runtime:value=input_point_order(value,load_json(response['binding_file'])['points'])
            records.append((response,load_json(response['model']),value))
            if tag=='native':
                r.case(out.name+'.native-engine',response.get('snapshot_engine')=='cpp-publication' and response.get('adapter_operation')=='none')
                saved=q.require_ok(service.command('save',output=out/tag/'save'))
                r.compare(out.name+'.source-exact-save',saved['saved_file'],model)
                state=dict(service.state)
                repeat=q.require_ok(service.command('snapshot',output=out/tag/'repeat'))
                r.case(out.name+'.repeat-readonly',state==service.state)
                r.case(out.name+'.reuse-prepared-publication',repeat.get('publication_reused') is True)
                r.compare(out.name+'.repeat-model',repeat['model'],response['model'])
                r.compare(out.name+'.repeat-scene',repeat['scene'],response['scene'])
                r.compare(out.name+'.repeat-dxf',repeat['dxf'],response['dxf'])
    a,b=records
    # Only implementation diagnostics and the explanatory unavailable reason
    # differ. Every geometry, setting, label, material and ID remains compared.
    for record in (a,b):
        record[1].pop('diagnostics',None)
        record[1]['overlay']['hydro'].pop('reason',None)
    # The historical snapshot writer omits layer alpha, but its independent
    # geometry writer includes the serialized value. Use that source only for
    # this declared schema extension; never remove a native value from comparison.
    layer_ids=load_json(b[0]['binding_file'])['layers'] if b[0].get('binding_file') else [layer['id'] for layer in b[1]['layers']]
    legacy_layers=b[2]['geometry']['layers']
    if len(layer_ids)!=len(legacy_layers) or len(set(layer_ids))!=len(layer_ids):raise ValueError('Ambiguous oracle layer bindings')
    expected_alpha={key:layer['alpha_blend'] for key,layer in zip(layer_ids,legacy_layers)}
    r.case(out.name+'.layer-alpha-from-pascal',all(layer.get('alpha_blend',255)==expected_alpha[layer['id']] for layer in a[1]['layers']))
    for layer in b[1]['layers']:
        if 'alpha_blend' not in layer and expected_alpha[layer['id']]!=255:layer['alpha_blend']=expected_alpha[layer['id']]
    # Plus's historical JSON writer omits this persisted flag. Independently
    # decode its original FBM bytes to extend the expected snapshot schema.
    persisted=preview_bytes.envelope(model.read_bytes())
    b[1]['settings'].update(save_preview=persisted['enabled'],generate_auxiliary_reports=persisted['generate_auxiliary_reports'],underwater_color_alpha=persisted['underwater_color_alpha'],coefficient_basis=persisted['coefficient_basis'],**persisted['hydro_fields'])
    diff=differences(a[1],b[1]);r.case(out.name+'.complete-snapshot-exact',not diff,differences=diff)
    diff=differences(a[2],b[2]);r.case(out.name+'.worker-input-exact',not diff,differences=diff)
    r.compare(out.name+'.scene-exact',a[0]['scene'],b[0]['scene'])
    r.compare(out.name+'.dxf-exact',a[0]['dxf'],b[0]['dxf'])
    r.data.setdefault('timings',[]).append(dict(model=str(model),measurements=timings))


def independence(r,runtime,model,out):
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        initial=q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'load'))
        m=load_json(initial['model']);p=next(p for p in m['control_points'] if p['batch_movable'])
        request_count=initial.get('adapter_requests')
        r.case('independent.no-pascal-bootstrap',request_count==0 and initial.get('adapter_pid')==0 and initial.get('preferences_engine')=='cpp-preferences')
        moves=list(p['p']);moves[2]+=.125
        requests=[('snapshot',{}),('move_point',dict(point_id=p['id'],position=moves)),
            ('update_project',dict(changes={'name':'Native publication'})),('undo',{}),('redo',{}),
            ('update_project',dict(changes={'name':'Native publication'}))]
        for i,(method,fields) in enumerate(requests):
            response=q.require_ok(s.command(method,output=out/f'{i}-{method}',**fields))
            r.case(f'independent.{i}-{method}',response.get('snapshot_engine')=='cpp-publication' and
                   response.get('adapter_operation')=='none' and response.get('adapter_requests')==request_count)
        before=dict(s.state);saved=q.require_ok(s.command('save',output=out/'saved'))
        blocker=out/'blocked';blocker.write_text('preserve','ascii')
        rejected=s.command('update_project',changes={'name':'Must not commit'},output=blocker)
        r.case('transaction.failed-output-keeps-state',not rejected['ok'] and s.state==before and blocker.read_text('ascii')=='preserve')
        stale=s.command('move_point',point_id=p['id'],position=moves,expected_revision=1,output=out/'stale')
        r.case('transaction.stale-rejected',not stale['ok'] and s.state==before)
        after=q.require_ok(s.command('save',output=out/'saved-after'));r.compare('transaction.failed-edits-byte-identical',saved['saved_file'],after['saved_file'])
        recovered=q.require_ok(s.command('snapshot',output=out/'recovered'))
        r.case('transaction.recovers-without-adapter',recovered.get('snapshot_engine')=='cpp-publication' and recovered.get('adapter_requests')==request_count)


def new_hulls(r,runtime,oracle_runtime,out):
    oracle=oracle_runtime/'FreeShipDocumentService.exe' if oracle_runtime else runtime/'bridge/FreeShipGeometryBridge.exe'
    for name,parameters in [('metric',dict(length=10,beam=4,draft=1,columns=7,rows=5,units=0)),
                            ('rounding',dict(length=20,beam=5,draft=2,columns=7,rows=5,units=0)),
                            ('imperial',dict(length=32.5,beam=12.5,draft=3.25,columns=9,rows=6,units=1))]:
        identity=str(q.uuid.uuid4());records=[]
        for tag,exe in [('native',runtime/'FreeShipDocumentService.exe'),('oracle',oracle)]:
            with q.Service(exe,out/name/tag/'process',native_linesplan=True) as s:
                response=q.require_ok(s.command('new',document_id=identity,output=out/name/tag/'new',native_surface=True,**parameters))
                m=load_json(response['model']);m.pop('diagnostics',None);m['overlay']['hydro'].pop('reason',None)
                record=dict(response=response,model=m)
                if tag=='oracle' and oracle_runtime:
                    with q.Service(oracle_runtime/'bridge/FreeShipGeometryBridge.exe',out/name/tag/'hydro-process') as original:
                        q.require_ok(original.command('new',document_id=str(q.uuid.uuid4()),output=out/name/tag/'hydro-new',**parameters))
                        hydro=q.require_ok(original.command('hydrostatics',mode='single',draft=parameters['draft'],heel=0,trim=0,output=out/name/tag/'hydro'))
                else:hydro=q.require_ok(s.command('hydrostatics',mode='single',draft=parameters['draft'],heel=0,trim=0,output=out/name/tag/'hydro'))
                record['hydro']=load_json(hydro['hydro'])['rows'][0];records.append(record)
        a,b=records
        # FreeProjectSettings.Clear: FSavePreview := True for new hulls.
        b['model']['settings']['save_preview']=True
        b['model']['settings'].update(coefficient_basis=1,generate_auxiliary_reports=False,underwater_color_alpha=255,**dict.fromkeys(preview_bytes.HYDRO_FIELDS,True))
        diff=differences(a['model'],b['model'])
        r.case('new.'+name+'.snapshot-exact',not diff,differences=diff)
        r.compare('new.'+name+'.scene-exact',a['response']['scene'],b['response']['scene'])
        r.compare('new.'+name+'.dxf-exact',a['response']['dxf'],b['response']['dxf'])
        r.case('new.'+name+'.km-exact',a['hydro']['kmt']==b['hydro']['kmt'],native=a['hydro']['kmt'],oracle=b['hydro']['kmt'])


def run(r,runtime,oracle_runtime,models,out):
    for i,model in enumerate(models):paired(r,runtime,oracle_runtime,model,out/f'model-{i+1}')
    fixtures=out/'fixtures';fixtures.mkdir()
    for version in (0,1,2,6,8,11,16,20,41,43,44):
        model=fixtures/f'v{version}.fbm';project.fixture(model,version=version,relations=version>=43)
        if oracle_runtime or version>=7:paired(r,runtime,oracle_runtime,model,out/f'version-{version}')
        else:
            with q.Service(runtime/'FreeShipDocumentService.exe',out/f'version-{version}/process',native_linesplan=True) as s:
                response=q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/f'version-{version}/load'))
                m=load_json(response['model']);r.case(f'v{version}.historical-control-net',len(m['control_points'])==8 and len(m['control_faces'])==5)
                saved=q.require_ok(s.command('save',output=out/f'version-{version}/saved'));r.compare(f'v{version}.exact-save',saved['saved_file'],model)
    new_hulls(r,runtime,oracle_runtime,out/'new-hulls')
    independence(r,runtime,models[0],out/'independence')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path)
    p.add_argument('--oracle-runtime',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,action='append')
    a=p.parse_args();runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    r=q.Report();r.data.update(suite='qt-publication',runtime=str(runtime),oracle_runtime=str(a.oracle_runtime) if a.oracle_runtime else 'isolated-pascal-bridge',limitations=[
        'Backend processes and hidden Qt callbacks are not desktop mouse/focus/visual acceptance.',
        'Missing legacy executable is exercised by qt-preferences; this suite also compares the independent Pascal oracle.'])
    models=[m.resolve() for m in (a.model or q.DEFAULT_MODELS)]
    protected={str(m):q.common.digest_file(m) for m in models};sources=q.current_sources(q.QT);start=time.perf_counter()
    r.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in ('qt_publication.py','qt_document.py','qt_native_project.py','fbm_preview_bytes.py','run_tests.py')}
    try:
        manifest=load_json(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Runtime source manifest stale')
        for name,h in manifest['binaries_sha256'].items():
            if q.common.digest_file(runtime/name)!=h:raise RuntimeError('Runtime binary mismatch: '+name)
        run(r,runtime,a.oracle_runtime,models,out)
        r.case('protected-models-unchanged',all(q.common.digest_file(Path(f))==h for f,h in protected.items()))
        r.case('sources-unchanged-during-test',q.current_sources(q.QT)==sources)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.case('desktop-acceptance',status='not_run',reason='No computer-use acceptance was performed.')
    r.data['elapsed_s']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps({'counts':r.data['counts'],'report':str(out/'report.json')}))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
