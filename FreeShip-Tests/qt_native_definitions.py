"""Native layer/section transactions, exact Plus output and independent rules.

Uses production document requests, stable IDs, history, save/reopen and real
format fixtures. No physical desktop input or automatic visual acceptance.
"""
import argparse,json,math,struct,subprocess,uuid
from collections import Counter
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_editing as e
import qt_native_project as project
import native_linesplan_app as nla


def open_hydro_boundary(model):
    """Independent control-polyhedron check for the planar box fixture.

    Include mirrored faces exactly when the layer requests them. A submerged
    edge owned by one face cannot bound a watertight displacement volume.
    """
    points={p['id']:tuple(p['p']) for p in model['control_points']}
    layers={l['id']:l for l in model['layers']};edges=Counter()
    for face in model['control_faces']:
        layer=layers[face['layer']]
        if not layer['use_in_hydrostatics']:continue
        for side in ((1,-1) if layer['symmetric'] else (1,)):
            polygon=[(points[p][0],side*points[p][1],points[p][2]) for p in face['points']]
            for a,b in zip(polygon,polygon[1:]+polygon[:1]):edges[tuple(sorted((a,b)))]+=1
    z=model['settings']['waterline_z']
    return any(n==1 and min(a[2],b[2])<z-1e-7 for (a,b),n in edges.items())


def probe(r,runtime,out):
    out.mkdir(parents=True);index=0;env=q.private_environment(out/'env')
    bindings=dict(fbm_encoding='utf8',layers=[1,2],layer_faces=[[1,3,4,5],[2]],high_water={'layers':2},last_used_layer_id=8,layer_name_prefix='Layer',default_layer_color=123456,resolved_colors={'-2147483643':0xfefefe})
    def invoke(path,request,minimum_only=False,state=bindings):
        nonlocal index
        index+=1;req=out/f'{index}-request.json';target=out/f'{index}-result.fbm'
        req.write_text(json.dumps(dict(request=request,bindings=state,minimum_only=minimum_only)),'utf8')
        process=subprocess.run([str(runtime/'native_definition_edit_probe.exe'),str(path),str(req),str(target)],env=env,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
        result=json.loads(process.stdout);return result,target
    for version in (0,1,2,6,7,9,10,11,12,13,15,16,18,20,21,44):
        path=out/f'v{version}.fbm';before=project.fixture(path,version=version,rich=True)
        def patch(**changes):return dict(method='update_layer',layer_id=1,changes=changes)
        response,target=invoke(path,patch());r.case(f'definitions.probe.v{version}.noop',response['ok'] and not response['changed'] and target.read_bytes()==path.read_bytes())
        for label,request,minimum in [
            ('material',patch(thickness=.006),11),('linesplan',patch(show_in_linesplan=False),15),('hydro',patch(use_in_hydrostatics=False),9),
            ('diagonal',dict(method='sections',kind=4,operation='add',positions=[2.125]),9),
            ('curvature',dict(method='sections',kind=1,operation='update',index=0,show_curvature=True),11)]:
            response,_=invoke(path,request,True)
            r.case(f'definitions.probe.v{version}.{label}-version',response['ok'] and response['minimum_version']==max(version,minimum))
        response,target=invoke(path,dict(method='sections',kind=1,operation='add',positions=[2.125,2.125,5,6.75]))
        after=project.observe(target)
        r.case(f'definitions.probe.v{version}.planes',response['changed'] and [s['plane'] for s in after['sections']['stations']]==[[1,0,0,-2.125],[1,0,0,-5],[1,0,0,-6.75]])
        r.case(f'definitions.probe.v{version}.section-other-bytes',all(v==after['blocks'][k] for k,v in before['blocks'].items() if k not in before['sections']))
        response,target=invoke(path,dict(method='manage_layers',operation='create',changes={}))
        after=project.observe(target)
        r.case(f'definitions.probe.v{version}.create',response['changed'] and after['active']==2 and after['layers'][-1]['id']==9 and after['layers'][-1]['color']==123456 and response['bindings']['layers']==[1,2,3])
        r.case(f'definitions.probe.v{version}.layer-other-bytes',all(v==after['blocks'][k] for k,v in before['blocks'].items() if k!='layers' and k not in before['sections'] and k!='flowlines'))
        removed,dest=invoke(target,dict(method='manage_layers',operation='delete',layer_id=3),state=response['bindings'])
        r.case(f'definitions.probe.v{version}.delete-active',removed['changed'] and project.observe(dest)['layers']==before['layers'] and project.observe(dest)['active']==0 and removed['bindings']['high_water']['layers']==3)
    path=out/'system-color.fbm';before=project.fixture(path);data=bytearray(path.read_bytes());at=before['spans']['layers'][0]+4;size=struct.unpack_from('<i',data,at)[0];struct.pack_into('<i',data,at+4+size+4,-2147483643);path.write_bytes(data)
    response,target=invoke(path,dict(method='update_layer',layer_id=1,changes={'color':0xfefefe}))
    r.case('definitions.probe.system-color-noop',response['ok'] and not response['changed'] and target.read_bytes()==path.read_bytes())
    response,target=invoke(path,dict(method='update_layer',layer_id=1,changes={'color':123}))
    r.case('definitions.probe.system-color-change',response['ok'] and response['changed'] and project.observe(target)['layers'][0]['color']==123)
    response,_=invoke(path,dict(method='update_layer',layer_id=1,changes={'name':'A\0B'}));r.case('definitions.probe.nul-reject',not response['ok'])
    # Count limit precedes duplicate filtering, exactly as the public adapter.
    path=out/'limit.fbm';info=project.fixture(path);data=path.read_bytes();start,size=info['spans']['stations'];record=struct.pack('<iBffffBi',1,0,1,0,0,-5,0,0)
    path.write_bytes(data[:start]+struct.pack('<i',4093)+record*4093+data[start+size:])
    response,_=invoke(path,dict(method='sections',kind=1,operation='add',positions=[5]));r.case('definitions.probe.limit-before-dedup',not response['ok'])
    response,_=invoke(path,dict(method='sections',kind=1,operation='delete',indices=[0,0]));r.case('definitions.probe.duplicate-selection-rejected',not response['ok'])


def run(r,runtime,source,out):
    out.mkdir(parents=True);timings=[]
    probe(r,runtime,out/'probe')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None;last=None
        def both(label,method='snapshot',**fields):
            nonlocal step,current,last
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                a=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                if method=='save':
                    saved=a['saved_file']
                    a=q.require_ok(service.command('snapshot',output=out/f'{step:03}-{label}'/(tag+'-snapshot')))
                    a['saved_file']=saved
                results.append((a,json.loads(Path(a['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'definitions.{step:03}-{label}'
            ga,gb=modeling.geometry(am),modeling.geometry(bm)
            ah,bh=am['overlay']['hydro'],bm['overlay']['hydro']
            suppressed=(ah=={'available':False,'markers':[],'areas':[],'path':[]} and bh['available'] and
                open_hydro_boundary(am) and {k:v for k,v in ga.items() if k!='overlay'}=={k:v for k,v in gb.items() if k!='overlay'} and
                {k:v for k,v in am['overlay'].items() if k!='hydro'}=={k:v for k,v in bm['overlay'].items() if k!='hydro'})
            r.case(prefix+'.model',status='review' if suppressed else 'pass' if ga==gb else 'fail',
                different_fields=[k for k in ga if ga[k]!=gb.get(k)],
                reason='Intentional: original overlay reports hydrostatics for a half hull whose mirror was disabled, leaving a submerged open boundary.' if suppressed else '')
            if suppressed:r.case(prefix+'.open-hull-has-no-hydro-labels',open_hydro_boundary(am) and not ah['available'] and not ah['markers'])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method in ('update_layer','manage_layers','sections'):
                r.case(prefix+'.native-owner',a.get('edit_engine')=='cpp-definitions' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append(dict(method=method,operation=fields.get('operation'),edit_ms=a['definition_edit_ms'],total_ms=a['document_total_ms']))
            current=am;last=a;return am
        def load(path=source):return both('load','load',path=str(path),document_id=str(uuid.uuid4()))
        def reject(label,method,**fields):
            for tag,service in [('native',native),('oracle',oracle)]:
                state=dict(service.state);a=service.command(method,output=out/f'reject-{label}-{tag}',**fields)
                r.case('definitions.reject-'+label+'.'+tag,not a['ok'] and service.state==state,error=a.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('definitions.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('definitions.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        def layer(identifier):return next(l for l in current['layers'] if l['id']==identifier)
        def patch(identifier,**changes):return both('layer','update_layer',layer_id=identifier,changes=changes)
        def section(kind,operation,**fields):return both('section-'+operation,'sections',kind=kind,operation=operation,**fields)

        base=load();lid=base['layers'][0]['id']
        changed=patch(lid,name='Hull Case',color=123456,visible=False,symmetric=False,developable=True,
                      use_in_hydrostatics=False,use_for_intersections=False,show_in_linesplan=False,thickness=.0123,material_density=7.85)
        l=layer(lid)
        r.case('definitions.layer-values',l['name']=='Hull Case' and l['color']==123456 and not l['visible'] and not l['symmetric'] and l['developable'] and not l['use_in_hydrostatics'] and not l['use_for_intersections'] and not l['show_in_linesplan'] and l['thickness']==e.f32(.0123) and l['material_density']==e.f32(7.85))
        r.case('definitions.layer-preserves-control-net',e.points(changed)==e.points(base) and changed['control_faces']==base['control_faces'] and changed['control_edges']==base['control_edges'])
        history('layer-properties',base,changed)
        before=dict(native.state);patch(lid,**{k:v for k,v in l.items() if k in ('name','color','visible','symmetric','developable','use_in_hydrostatics','use_for_intersections','show_in_linesplan','thickness','material_density')})
        r.case('definitions.layer-noop',native.state==before)
        patch(lid,use_in_hydrostatics=True);r.case('definitions.hydro-enables-symmetry',layer(lid)['symmetric'])
        patch(lid,use_in_hydrostatics=False,symmetric=False)
        patch(lid,use_in_hydrostatics=True,symmetric=False);r.case('definitions.explicit-asymmetric-hydro',not layer(lid)['symmetric'] and layer(lid)['use_in_hydrostatics'])
        patch(lid,name='hULL cASE');r.case('definitions.name-case-sensitive',layer(lid)['name']=='hULL cASE')
        for label,changes in [('unknown',{'bogus':1}),('negative',{'name':'rollback','thickness':-1}),('fraction-color',{'color':1.5}),('huge-color',{'color':0x1000000}),('bool-number',{'material_density':True}),('wrong-flag',{'visible':1}),('wrong-name',{'name':None})]:
            reject(label,'update_layer',layer_id=lid,changes=changes)
        reject('layer-fraction','update_layer',layer_id=1.5,changes={})
        reject('layer-missing','update_layer',layer_id=2000000,changes={})

        base=load();added=both('layer-create','manage_layers',operation='create',changes={});new=added['layers'][-1];created=new['id']
        r.case('definitions.new-layer-defaults',new['symmetric'] and new['visible'] and new['use_in_hydrostatics'] and new['use_for_intersections'] and new['show_in_linesplan'] and not new['developable'] and new['thickness']==new['material_density']==0)
        old=dict(native.state);patch(created,name=new['name']);r.case('definitions.generated-name-noop',old==native.state)
        before=both('create-save','save');observed=project.observe(Path(last['saved_file']))
        r.case('definitions.generated-name-remains-empty',observed['layers'][-1]['name']==b'')
        reject('occupied','manage_layers',operation='delete',layer_id=base['control_faces'][0]['layer'])
        reject('invalid-create','manage_layers',operation='create',changes={'name':'Ghost','thickness':-1})
        same=both('after-invalid-create');r.case('definitions.no-ghost-layer',same['layers']==added['layers'])
        removed=both('layer-delete','manage_layers',operation='delete',layer_id=created);history('layer-delete',same,removed)
        restored=both('restore-for-branch','undo')
        branch=both('branch-create','manage_layers',operation='create',changes={'name':'Branch'})
        r.case('definitions.branch-never-reuses-stable-id',branch['layers'][-1]['id']>created)
        cleared=both('delete-empty','manage_layers',operation='delete_empty')
        r.case('definitions.empty-delete-keeps-all-faces',cleared['control_faces']==base['control_faces'] and len(cleared['layers'])>=1)
        state=dict(native.state);both('empty-delete-noop','manage_layers',operation='delete_empty');r.case('definitions.empty-delete-noop',state==native.state)
        for kind in range(1,5):
            before=current;position=1.234567+kind*.01
            after=section(kind,'add',positions=[position,position+.5,position+1,position])
            items=[s for s in after['sections'] if s['kind']==kind]
            item=next(s for s in items if abs(s['distance']-e.f32(position))<1e-6)
            n=e.f32(1/math.sqrt(2));p=e.f32(position)
            plane=[1,0,0,-p] if kind==1 else [0,1,0,-p] if kind==2 else [0,0,1,-p] if kind==3 else [0,n,n,e.f32(-p*n)]
            r.case(f'definitions.kind-{kind}.plane',item['plane']==plane)
            state=dict(native.state);section(kind,'add',positions=[position]);r.case(f'definitions.kind-{kind}.duplicate-noop',state==native.state)
            changed=section(kind,'update',index=item['index'],distance=position+.125,show_curvature=True)
            edited=next(s for s in changed['sections'] if s['kind']==kind and abs(s['distance']-e.f32(position+.125))<1e-6)
            r.case(f'definitions.kind-{kind}.curvature',edited['show_curvature'])
            reject(f'duplicate-{kind}','sections',kind=kind,operation='update',index=edited['index'],distance=position+.5)
            removed=section(kind,'delete',indices=[edited['index']]);history(f'section-{kind}',changed,removed)
            outside=section(kind,'add',positions=[-10000-kind,10000+kind])
            r.case(f'definitions.kind-{kind}.outside-hull-retained',len(outside['sections'])==len(removed['sections'])+2)
        for label,fields in [('bad-kind',dict(kind=5,operation='clear')),('fraction-kind',dict(kind=1.5,operation='clear')),('duplicate-index',dict(kind=1,operation='delete',indices=[0,0])),('fraction-index',dict(kind=1,operation='update',index=.5,distance=3)),('bad-distance',dict(kind=1,operation='add',positions=[True])),('bad-flag',dict(kind=1,operation='update',index=0,distance=12345,show_curvature=1)),('too-many',dict(kind=1,operation='add',positions=list(range(513)))),('empty-array',dict(kind=1,operation='add',positions=[])),('stale',dict(kind=1,operation='clear',expected_revision=1))]:reject(label,'sections',**fields)
        before=current;after=section(4,'clear');r.case('definitions.clear-one-kind',[s for s in before['sections'] if s['kind']!=4]==after['sections'])
        state=dict(native.state);section(4,'clear');r.case('definitions.empty-clear-noop',state==native.state)
        both('undo-clear','undo');state=dict(native.state);both('noop-preserves-redo','update_layer',layer_id=current['layers'][0]['id'],changes={})
        r.case('definitions.noop-keeps-redo',native.state==state and native.state['can_redo'])
        both('redo-clear','redo')
        original=current
        for tag,service in [('native',native),('oracle',oracle)]:
            saved=q.require_ok(service.command('save',output=out/('final-save-'+tag)))
            response=q.require_ok(service.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),output=out/('final-reopen-'+tag)))
            actual=json.loads(Path(response['model']).read_text('utf8'))
            r.case('definitions.save-reopen-'+tag,nla.reopened_geometry(actual)==nla.reopened_geometry(original))
        # File formats predate curvature, material data and lines-plan flags.
        # Accepted edits must promote only when the new value needs that field.
        for version in (9,11,15,21):
            path=out/f'old-{version}.fbm';project.fixture(path,version=version,rich=False);load(path)
            # A planar deck is valid; the renderer now computes nonempty display
            # bounds for it. Changing visibility must preserve geometry/history.
            before=current
            after=patch(current['layers'][0]['id'],show_in_linesplan=False,use_for_intersections=False)
            visible={l['id'] for l in after['layers'] if l['show_in_linesplan']}
            points=e.points(after);z={points[p][2] for f in after['control_faces'] if f['layer'] in visible for p in f['points']}
            r.case(f'definitions.planar-{version}.fixture',len(z)==1)
            r.case(f'definitions.planar-{version}.geometry-fixed',all(after[k]==before[k] for k in ('control_points','control_edges','control_faces')))
            data=Path(last['scene']).read_bytes();x0,y0,x1,y1=struct.unpack_from('<dddd',data,8)
            r.case(f'definitions.planar-{version}.valid-display-bounds',data[:8]==b'FSLP0001' and all(math.isfinite(v) for v in (x0,y0,x1,y1)) and x1>x0 and y1>y0)
            history(f'planar-{version}',before,after);both(f'planar-{version}-restore','undo')
            lid=current['layers'][1]['id'];patch(lid,thickness=.004,show_in_linesplan=False,use_for_intersections=False)
            section(4,'add',positions=[2.125]);section(1,'update',index=0,show_curvature=True)
            both('old-save','save');stored=project.observe(Path(last['saved_file']))
            r.case(f'definitions.old-{version}.promoted',stored['version']==max(version,15) and stored['layers'][1]['thickness']==e.f32(.004))
        base=load();state=dict(native.state);blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        response=native.command('sections',kind=1,operation='add',positions=[12345],output=blocked)
        r.case('definitions.publication-rollback',not response['ok'] and native.state==state)
        after=both('after-publication-failure');r.case('definitions.publication-restores-model',modeling.geometry(after)==modeling.geometry(base))
        # QJsonObject otherwise keeps only the final duplicate key, bypassing
        # the old patch validation. Exercise the actual UTF-8 protocol bytes.
        for method in ('update_project','update_layer'):
            native.counter+=1
            request=dict(method=method,id=native.counter,expected_document_id=native.state['document_id'],expected_revision=native.state['revision'],output=str(out/('duplicate-'+method)),layer_id=current['layers'][0]['id'],changes={})
            payload=json.dumps(request).replace('"changes": {}','"changes": {"na\\u006de":"First","name":"Last"}')
            native.process.stdin.write((payload+'\n').encode());native.process.stdin.flush();response=native.events.get(timeout=30)
            (out/('duplicate-'+method+'.json')).write_text(json.dumps(dict(wire_request=payload,response=response),ensure_ascii=False),'utf8')
            r.case('definitions.duplicate-fields-'+method,not response['ok'] and 'Duplicate' in response.get('error',''))
        after=both('after-duplicate-fields');r.case('definitions.duplicate-fields-atomic',modeling.geometry(after)==modeling.geometry(base) and native.state==state)
        for method in ('update_layer','manage_layers','sections'):
            response=oracle.command(method,external_document=True,output=out/('forbid-'+method))
            r.case('definitions.forbid-old-'+method,not response['ok'] and 'C++' in response.get('error',''))
    r.data['native_definition_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('definitions.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
