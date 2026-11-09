"""Native plate production protocols; called by the unified qt-plates suite.

Independent geometry and original-core comparisons live in qt_plates.py. This
module verifies document ownership, recomputation, failure isolation and the
patched legacy intersection contract. All processes/files are private.
"""
import json
import time
import struct
import threading
from pathlib import Path
import qt_migration as q
import qt_document


def data(response):
    q.require_ok(response)
    return json.loads(Path(response['plates']).read_text('utf8'))


def geometry(value):
    return {k:v for k,v in value.items() if k not in ('document_id','revision','calculation_ms','analysis_engine')}


def run(r,runtime,models,out):
    out.mkdir(parents=True,exist_ok=False)
    timings=[]
    # Native geometry must remain reproducible after reloading another file;
    # allocation addresses cannot select a different unfolding.
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native-process',native_linesplan=True) as s:
        for name in ('plane','saddle','cylinder','mirror-bottom','bent-centre'):
            q.require_ok(s.command('load',path=str(models/(name+'.fbm')),document_id=str(q.uuid.uuid4()),output=out/name/'load'))
            saved=q.require_ok(s.command('save',output=out/name/'save-before'))
            before=dict(s.state)
            a=data(s.command('develop_plates',output=out/name/'a'))
            b=data(s.command('develop_plates',output=out/name/'b'))
            r.case('native.'+name+'.repeat-identical',geometry(a)==geometry(b))
            after=q.require_ok(s.command('save',output=out/name/'save-after'))
            r.compare('native.'+name+'.fbm-readonly',after['saved_file'],saved['saved_file'])
            r.case('native.'+name+'.history-readonly',before==s.state)
            q.require_ok(s.command('load',path=str(models/'feet.fbm'),document_id=str(q.uuid.uuid4()),output=out/name/'other'))
            q.require_ok(s.command('load',path=str(models/(name+'.fbm')),document_id=str(q.uuid.uuid4()),output=out/name/'reload'))
            c=data(s.command('develop_plates',output=out/name/'c'))
            r.case('native.'+name+'.reload-identical',geometry(a)==geometry(c))
            timings.append(dict(model=name,first_ms=a['calculation_ms'],repeat_ms=b['calculation_ms'],reload_ms=c['calculation_ms']))

        load=q.require_ok(s.command('load',path=str(models/'plane.fbm'),document_id=str(q.uuid.uuid4()),output=out/'edit-load'))
        model=json.loads(Path(load['model']).read_text('utf8'));point=model['control_points'][0]
        a=data(s.command('develop_plates',output=out/'edit-before'))
        position=list(point['p']);position[2]+=.25
        q.require_ok(s.command('move_point',point_id=point['id'],position=position,output=out/'edit'))
        b=data(s.command('develop_plates',output=out/'edit-after'))
        r.case('native.edit-recomputed',b['revision']>a['revision'] and b['plates'][0]['reference_area']!=a['plates'][0]['reference_area'])
        q.require_ok(s.command('undo',output=out/'undo'))
        c=data(s.command('develop_plates',output=out/'undo-result'))
        r.case('native.undo-restores',geometry(c)==geometry(a))
        q.require_ok(s.command('redo',output=out/'redo'))
        d=data(s.command('develop_plates',output=out/'redo-result'))
        r.case('native.redo-restores',geometry(d)==geometry(b))
        snap=q.require_ok(s.command('snapshot',output=out/'snapshot'))
        r.case('native.no-pascal-bootstrap',snap.get('adapter_pid')==0 and snap.get('adapter_requests')==0)
        e=data(s.command('develop_plates',output=out/'detached'))
        r.case('native.without-pascal',geometry(e)==geometry(d))

    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'guard') as s:
        q.require_ok(s.command('load',path=str(models/'plane.fbm'),document_id=str(q.uuid.uuid4()),output=out/'guard-load'))
        response=s.command('develop_plates',external_document=True,output=out/'forbid-old')
        r.case('native.forbid-old',not response['ok'] and 'must execute in C++' in response.get('error',''))

    # Both routes include the established fixes for coplanar faces/edges. A
    # section in the flat panel's own plane has no invented interior contour.
    for label,kind,positions in (('coplanar',3,[0.0]),('edge-sections',1,[0.0,.5,1.,1.5,2.,2.5,3.])):
        results=[]
        for engine,exe in (('native',runtime/'FreeShipDocumentService.exe'),('legacy',runtime/'bridge/FreeShipGeometryBridge.exe')):
            folder=out/label/engine
            with q.Service(exe,folder/'process') as s:
                q.require_ok(s.command('load',path=str(models/'plane.fbm'),document_id=str(q.uuid.uuid4()),output=folder/'load'))
                q.require_ok(s.command('sections',operation='add',kind=kind,positions=positions,output=folder/'sections'))
                value=data(s.command('develop_plates',output=folder/'plates'))
                results.append(value['plates'][0]['curves'])
        if label=='coplanar':r.case('native.coplanar-no-invented-contour',results==[[],[]])
        else:
            for engine,curves in zip(('native','legacy'),results):
                r.case('native.edge-sections.'+engine,len(curves)==len(positions) and all(c['kind']==1 for c in curves),count=len(curves))
            # Rotation/translation are layout choices. Lengths and section
            # styles are independent of these choices.
            import math
            signature=lambda cs:sorted((c['kind'],c['color'],c['dxf_color'],len(c['points']),sum(math.dist(a,b) for a,b in zip(c['points'],c['points'][1:]))) for c in cs)
            x,y=signature(results[0]),signature(results[1])
            r.case('native.section-style-and-length',len(x)==len(y) and all(a[:4]==b[:4] and abs(a[4]-b[4])<2e-5 for a,b in zip(x,y)))
    r.data['native_plate_timings']=timings


def precision_cases(r,runtime,models,out):
    """All supported subdivision settings, without quadratic distance tables."""
    from qt_plates import metrics
    out.mkdir(parents=True,exist_ok=False)
    measurements=[]
    for precision in (1,2,3):
        for name in ('plane','saddle','mirror-bottom'):
            folder=out/(name+'-'+str(precision));folder.mkdir()
            raw=bytearray((models/(name+'.fbm')).read_bytes())
            count=struct.unpack_from('<i',raw)[0]
            assert bytes(raw[4:4+count])==b'FREE!ship'
            struct.pack_into('<i',raw,4+count+1,precision)
            fixture=folder/'fixture.fbm';fixture.write_bytes(raw)
            results=[];times=[]
            for engine,exe in (('native',runtime/'FreeShipDocumentService.exe'),('legacy',runtime/'bridge/FreeShipGeometryBridge.exe')):
                with q.Service(exe,folder/engine/'process',native_linesplan=True) as s:
                    q.require_ok(s.command('load',path=str(fixture),document_id=str(q.uuid.uuid4()),output=folder/engine/'load'))
                    started=time.perf_counter();result=data(s.command('develop_plates',output=folder/engine/'plates'));times.append((time.perf_counter()-started)*1000);results.append(result)
            a,b=results;prefix=f'precision.{name}.{precision}'
            r.case(prefix+'.count-and-units',len(a['plates'])==len(b['plates']) and a['units']==b['units'] and a['waterline']==b['waterline'])
            for i,(pa,pb) in enumerate(zip(a['plates'],b['plates'])):
                metrics(r,prefix+'.'+str(i),pa)
                r.case(prefix+f'.{i}.geometry-counts',all(len(pa[k])==len(pb[k]) for k in ('points','original','faces','edges','corners')))
                r.case(prefix+f'.{i}.legacy-metrics',all(abs(pa[k]-pb[k])<2e-5*max(1,abs(pb[k])) for k in ('reference_area','absolute_area_error','normalized_error','min_edge_error','max_edge_error')),native={k:pa[k] for k in ('reference_area','absolute_area_error','normalized_error')},legacy={k:pb[k] for k in ('reference_area','absolute_area_error','normalized_error')})
            if name=='plane':r.case(prefix+'.analytic-full-resolution',len(a['plates'][0]['faces'])==9*4**(precision+1) and abs(a['plates'][0]['reference_area']-9)<1e-5)
            measurements.append(dict(model=name,precision=precision,faces=len(a['plates'][0]['faces']),native_ms=a['calculation_ms'],native_request_ms=times[0],legacy_request_ms=times[1]))
    r.data['native_plate_precision_timings']=measurements


def failures(r,runtime,model,out):
    out.mkdir(parents=True,exist_ok=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'load'))
        q.require_ok(s.command('sections',operation='add',kind=1,positions=[3*i/101 for i in range(1,101)],output=out/'sections'))
        before=dict(s.state)
        # Output-directory creation is an observable request boundary. Cancel
        # after calculation has started, while the service command is pending.
        folder=out/'cancel-running';observed=threading.Event()
        def cancel():
            deadline=time.perf_counter()+5
            while not folder.exists() and time.perf_counter()<deadline:time.sleep(.002)
            if folder.exists():
                time.sleep(.03);(folder/'cancel.flag').write_text('cancel','ascii');observed.set()
        worker=threading.Thread(target=cancel);worker.start();started=time.perf_counter()
        response=s.command('develop_plates',output=folder);elapsed=time.perf_counter()-started;worker.join(timeout=6)
        r.case('native.cancel-in-flight',observed.is_set() and not response['ok'] and '取消' in response.get('error','') and elapsed<5,seconds=elapsed,error=response.get('error'))
        r.case('native.cancel-no-partial-result',not (folder/'plates.json').exists() and s.state==before)
        blocked=out/'blocked';(blocked/'plates.json').mkdir(parents=True);marker=blocked/'plates.json/keep.txt';marker.write_text('keep','ascii')
        response=s.command('develop_plates',output=blocked)
        r.case('native.write-failure-isolated',not response['ok'] and marker.read_text('ascii')=='keep' and s.state==before,error=response.get('error'))
        retry=data(s.command('develop_plates',output=out/'retry'))
        r.case('native.recover-after-failures',retry['analysis_engine']=='cpp-plates' and len(retry['plates'][0]['curves'])==100 and s.state==before)


def old_versions(r,runtime,out):
    """A 10 by 2 roof panel in independently written historical FBM layouts."""
    from qt_native_project import fixture
    out.mkdir(parents=True,exist_ok=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        for version in (0,1,2,6,8,9,44):
            file=out/f'v{version}.fbm';fixture(file,version=version,rich=False)
            load=q.require_ok(s.command('load',path=str(file),document_id=str(q.uuid.uuid4()),output=out/f'v{version}-load'))
            model=json.loads(Path(load['model']).read_text('utf8'))
            updated=q.require_ok(s.command('update_layer',layer_id=model['layers'][1]['id'],changes={'developable':True,'name':''},output=out/f'v{version}-enable'))
            display=json.loads(Path(updated['model']).read_text('utf8'))['layers'][1]['name']
            value=data(s.command('develop_plates',output=out/f'v{version}-plates'))
            p=value['plates'][0]
            r.case(f'native.version-{version}.analytic-panel',len(value['plates'])==1 and p['joined_mirror'] and abs(p['reference_area']-20)<1e-5)
            r.case(f'native.version-{version}.settings',value['units']==0 and value['underwater_color']==model['settings']['underwater_color'] and value['waterline']==model['settings']['waterline_z'])
            r.case(f'native.version-{version}.default-layer-name',bool(display) and p['name']==display)
