"""Native display, control-curve and flowline definitions.

Pairs production requests with the isolated Plus oracle, and independently
checks byte preservation, version requirements, graph rules and rollback.
Flowline tracing is now native; the isolated reference initializes the previously
unassigned edge fraction explicitly (see qt_native_flowlines.py).
"""
import argparse,json,struct,subprocess,uuid
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_editing as e
import qt_native_project as project
import native_linesplan_app as nla


def curves(data):
    count=struct.unpack_from('<i',data)[0];at=4;result=[]
    for _ in range(count):
        n=struct.unpack_from('<i',data,at)[0];at+=4
        points=list(struct.unpack_from('<'+'i'*n,data,at));at+=n*4
        result.append(dict(points=points,selected=data[at]));at+=1
    assert at==len(data)
    return result


def probe(r,runtime,out):
    out.mkdir(parents=True);index=0;env=q.private_environment(out/'env')
    def state(version):
        counts=dict(points=8,edges=12,faces=5,layers=2,curves=int(version>=12),flows=int(version>=20))
        return dict(fbm_encoding='utf8',high_water=counts,**{k:list(range(1,n+1)) for k,n in counts.items()})
    def invoke(path,request,minimum_only=False,bindings=None):
        nonlocal index
        index+=1;req=out/f'{index}-request.json';target=out/f'{index}-result.fbm'
        req.write_text(json.dumps(dict(request=request,bindings=bindings or state(44),minimum_only=minimum_only)),'utf8')
        proc=subprocess.run([str(runtime/'native_auxiliary_edit_probe.exe'),str(path),str(req),str(target)],env=env,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
        return json.loads(proc.stdout),target
    for version in (0,1,2,6,9,11,12,13,16,17,18,19,20,21,40,41,44):
        path=out/f'v{version}.fbm';before=project.fixture(path,version=version)
        response,target=invoke(path,dict(method='display',changes={}))
        r.case(f'aux.probe.v{version}.noop',response['ok'] and not response['changed'] and path.read_bytes()==target.read_bytes())
        for flag,minimum in [('show_control_curves',12),('show_flowlines',20)]:
            # Before the persisted field existed, both options defaulted true.
            response,_=invoke(path,dict(method='display',changes={flag:False}),True)
            expected=max(version,minimum) if version<minimum or flag=='show_control_curves' else version
            r.case(f'aux.probe.v{version}.{flag}-version',response['ok'] and response['minimum_version']==expected)
        request=dict(method='display',changes=dict(both_sides=False,show_normals=False,show_markers=True,show_curvature=True,curvature_scale=2.125))
        response,target=invoke(path,request);after=project.observe(target);raw=after['blocks']['visibility']
        r.case(f'aux.probe.v{version}.values',response['changed'] and struct.unpack_from('<i',raw)[0]==0 and raw[9]==0 and raw[12:14]==b'\1\1' and struct.unpack_from('<f',raw,14)[0]==2.125)
        r.case(f'aux.probe.v{version}.preserve-other-bytes',all(v==after['blocks'][k] for k,v in before['blocks'].items() if k!='visibility'))
        if version>=12:
            response,_=invoke(path,dict(method='auxiliary',operation='rename_control',ids=[1],name='New name'),True,bindings=state(version))
            r.case(f'aux.probe.v{version}.name-version',response['ok'] and response['minimum_version']==max(version,41))
    path=out/'selection.fbm';before=project.fixture(path);data=bytearray(path.read_bytes())
    start,size=before['spans']['control_curves'];data[start+size-1]=1
    # The fixture contains an independently selected flow but hides flowlines.
    data[before['spans']['visibility'][0]+29]=1;path.write_bytes(data);before=project.observe(path)
    response,target=invoke(path,dict(method='display',changes={'show_control_curves':False,'show_flowlines':False}));after=project.observe(target)
    r.case('aux.probe.hide-clears-only-selections',curves(after['blocks']['control_curves'])[0]['selected']==0 and after['flows'][0]['selected']==0 and all(v==after['blocks'][k] for k,v in before['blocks'].items() if k not in ('visibility','control_curves','flowlines')))
    r.case('aux.probe.hide-preserves-flow-source-and-cache',all(after['flows'][0][k]==before['flows'][0][k] for k in before['flows'][0] if k!='selected'))
    for scale,changed in [(1+2**-25,False),(1+2**-23,True),(0,True)]:
        response,_=invoke(path,dict(method='display',changes={'curvature_scale':scale}))
        r.case(f'aux.probe.scale-{scale}',response['ok'] and response['changed']==changed)
    for label,request in [('negative',dict(method='display',changes={'curvature_scale':-.1})),('boolean-scale',dict(method='display',changes={'curvature_scale':True})),('number-flag',dict(method='display',changes={'show_normals':1})),('unknown',dict(method='display',changes={'typo':True})),('nul',dict(method='auxiliary',operation='rename_control',ids=[1],name='A\0B')),('oversize',dict(method='auxiliary',operation='rename_control',ids=[1],name='A'*4097)),('fraction-id',dict(method='auxiliary',operation='delete',kind='curve',ids=[1.5]))]:
        response,_=invoke(path,request);r.case('aux.probe.reject-'+label,not response['ok'])
    # Definition edits cannot modify coordinates, faces, image payloads or
    # stored calculation records. A complete Unicode name survives byte-exactly.
    response,target=invoke(path,dict(method='auxiliary',operation='rename_control',ids=[1],name='船体 Ж A'));after=project.observe(target)
    name='船体 Ж A'.encode('utf8');expected=struct.pack('<iii',1,0,len(name))+name
    r.case('aux.probe.unicode-name',after['blocks']['curve_names']==expected)
    r.case('aux.probe.rename-preserves-other-bytes',all(v==after['blocks'][k] for k,v in before['blocks'].items() if k!='curve_names'))
    response,target=invoke(path,dict(method='auxiliary',operation='delete',kind='curve',ids=[1]));after=project.observe(target)
    r.case('aux.probe.delete-prunes-name',after['blocks']['curve_names']==struct.pack('<i',0) and not curves(after['blocks']['control_curves']))
    r.case('aux.probe.delete-keeps-control-net',all(after['blocks'][k]==before['blocks'][k] for k in ('control_points','control_edges','control_faces')))
    # Count limits use a compact definition fixture, without running 4096 traces.
    for kind,key,record in [('curve','control_curves',before['blocks']['control_curves'][4:]),('flow','flowlines',before['blocks']['flowlines'][4:])]:
        data=path.read_bytes();start,size=before['spans'][key];limit=out/f'limit-{kind}.fbm';limit.write_bytes(data[:start]+struct.pack('<i',4096)+record*4096+data[start+size:])
        bindings=state(44);group='curves' if kind=='curve' else 'flows';bindings[group]=list(range(1,4097));bindings['high_water'][group]=4096
        request=dict(method='auxiliary',operation='create_control',ids=[3]) if kind=='curve' else dict(method='auxiliary',operation='create_flow',view=1,source=[8,.5])
        response,_=invoke(limit,request,bindings=bindings);r.case('aux.probe.count-limit-'+kind,not response['ok'])


def run(r,runtime,source,out):
    out.mkdir(parents=True);timings=[];probe(r,runtime,out/'probe')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None;last=None
        def both(label,method='snapshot',**fields):
            nonlocal step,current,last
            step+=1;values=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                a=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                if method=='save':
                    saved=a['saved_file'];a=q.require_ok(service.command('snapshot',output=out/f'{step:03}-{label}'/(tag+'-snapshot')));a['saved_file']=saved
                values.append((a,json.loads(Path(a['model']).read_text('utf8'))))
            (a,am),(b,bm)=values;prefix=f'aux.{step:03}-{label}';ga,gb=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ga==gb,different_fields=[k for k in ga if ga[k]!=gb.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method in ('display','auxiliary'):
                r.case(prefix+'.native-owner',a.get('edit_engine')=='cpp-auxiliary' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append(dict(method=method,operation=fields.get('operation'),edit_ms=a['auxiliary_edit_ms'],total_ms=a['document_total_ms']))
            current=am;last=a;return am
        def load(path=source):return both('load','load',path=str(path),document_id=str(uuid.uuid4()))
        def aux(operation,**fields):return both(operation,'auxiliary',operation=operation,**fields)
        def display(**changes):return both('display','display',changes=changes)
        def reject(label,method,**fields):
            for tag,service in [('native',native),('oracle',oracle)]:
                before=dict(service.state);reply=service.command(method,output=out/f'reject-{label}-{tag}',**fields)
                r.case('aux.reject-'+label+'.'+tag,not reply['ok'] and service.state==before,error=reply.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('aux.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('aux.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        base=load();changes={k:not base['display'][k] for k in ('both_sides','show_curvature','show_control_curves','show_flowlines','show_normals','show_markers')};changes['curvature_scale']=2.125
        changed=display(**changes);r.case('aux.display-values',all(changed['display'][k]==v for k,v in changes.items()));history('display',base,changed)
        before=dict(native.state);display(**changes);r.case('aux.display-noop',native.state==before)
        for label,patch in [('unknown',{'bogus':True}),('wrong-type',{'show_normals':1}),('negative',{'show_normals':True,'curvature_scale':-1})]:reject(label,'display',changes=patch)
        base=load()
        for kind,key in [('curve','control_curves'),('flow','flowlines')]:
            if current[key]:aux('delete',kind=kind,ids=[c['id'] for c in current[key]])
        base=current;edges=base['control_edges'];edge_map={frozenset((edge['a'],edge['b'])):edge['id'] for edge in edges}
        face=base['control_faces'][0]['points'];cycle=[edge_map[frozenset((a,b))] for a,b in zip(face,face[1:]+face[:1])]
        closed=aux('create_control',ids=cycle);curve=closed['control_curves'][-1];cid=curve['id']
        r.case('aux.closed-cycle',curve['controls'][0]==curve['controls'][-1] and len(curve['controls'])==len(cycle)+1)
        r.case('aux.curve-keeps-hull',q.geometry(closed)==q.geometry(base));history('cycle',base,closed)
        before=dict(native.state);aux('create_control',ids=cycle);r.case('aux.assigned-noop',native.state==before)
        named=aux('rename_control',ids=[cid],name='Hull A');r.case('aux.name-value',named['control_curves'][-1]['name']=='Hull A')
        history('rename',closed,named);before=dict(native.state);aux('rename_control',ids=[cid],name='Hull A');r.case('aux.rename-noop',native.state==before)
        aux('rename_control',ids=[cid],name='hULL a');r.case('aux.name-case',current['control_curves'][-1]['name']=='hULL a')
        aux('rename_control',ids=[cid],name='');r.case('aux.empty-name',current['control_curves'][-1]['name']=='')
        for label,fields in [('duplicate',dict(operation='create_control',ids=[cycle[0]]*2)),('missing',dict(operation='delete',kind='curve',ids=[1999999])),('bad-kind',dict(operation='delete',kind='face',ids=[cid])),('bad-view',dict(operation='create_flow',view=3,source=[1,1])),('outside',dict(operation='create_flow',view=1,source=[1e8,1e8])),('fraction-view',dict(operation='create_flow',view=.5,source=[1,1])),('wrong-source',dict(operation='create_flow',view=1,source=[True,1])),('stale',dict(operation='create_control',ids=[cycle[0]],expected_revision=1)),('long-name',dict(operation='rename_control',ids=[cid],name='A'*4097))]:reject(label,'auxiliary',**fields)
        aux('delete',kind='curve',ids=[cid]);adjacency={}
        for edge in edges:
            for point in (edge['a'],edge['b']):adjacency.setdefault(point,[]).append(edge['id'])
        branch=next(es[:3] for es in adjacency.values() if len(es)>=3);branched=aux('create_control',ids=branch)
        r.case('aux.branch-three-chains',len(branched['control_curves'])==3 and all(len(c['controls'])==2 for c in branched['control_curves']))
        aux('rename_control',ids=[branched['control_curves'][2]['id']],name='Survivor')
        aux('delete',kind='curve',ids=[branched['control_curves'][0]['id']]);r.case('aux.name-reindex',current['control_curves'][-1]['name']=='Survivor')
        aux('delete',kind='curve',ids=[c['id'] for c in current['control_curves']]);first=edges[0];second=next(edge for edge in edges if not {edge['a'],edge['b']}&{first['a'],first['b']})
        separated=aux('create_control',ids=[first['id'],second['id']]);r.case('aux.disconnected',len(separated['control_curves'])==2 and all(c['id']>cid for c in separated['control_curves']))
        point=next(p for p in current['control_points'] if p['id']==first['a']);position=list(point['p']);position[2]+=.025
        moved=both('move','move_point',point_id=point['id'],position=position)
        r.case('aux.move-regenerates-splines',[c['id'] for c in moved['control_curves']]==[c['id'] for c in separated['control_curves']] and moved['control_curves']!=separated['control_curves']);history('move',separated,moved)
        # Free-standing curves normalize their endpoints during the old
        # subdivision rebuild. This is a persistent edit, not just rendering.
        load();wire=[]
        for x in range(30,35):
            prior=set(e.points(current));model=both('wire-point','topology',operation='add_point',position=[x,1,2]);wire.append(next(iter(set(e.points(model))-prior)))
        both('wire-edges','modeling',operation='connect_points',ids=wire)
        chain=[edge['id'] for edge in current['control_edges'] if edge['a'] in wire and edge['b'] in wire]
        before=current;free=aux('create_control',ids=chain)
        types={p['id']:p['vertex_type'] for p in free['control_points']}
        r.case('aux.free-curve-endpoints-and-creases',[types[p] for p in wire]==[3,1,1,1,3] and all(edge['crease'] for edge in free['control_edges'] if edge['id'] in chain))
        r.case('aux.free-curve-coordinates-preserved',e.points(free)==e.points(before))
        history('free-curve',before,free)
        display(show_control_curves=False);display(show_control_curves=True)
        aux('delete',kind='curve',ids=[free['control_curves'][-1]['id']])
        r.case('aux.free-curve-delete-keeps-vertices',e.points(current)==e.points(free) and current['control_edges']==free['control_edges'])
        base=load();lo,hi=base['bounds']['min'],base['bounds']['max'];water=base['settings']['waterline_z']
        # Interior side point on the deterministic box; no view pixels involved.
        flow=aux('create_flow',view=1,source=[lo[0]+.8*(hi[0]-lo[0]),lo[2]+.4*(water-lo[2])]);fid=flow['flowlines'][-1]['id']
        r.case('aux.flow-has-path',len(flow['flowlines'][-1]['spline']['points'])>=2);history('flow',base,flow)
        first=flow['flowlines'][-1]['spline']['points'][0]
        for view,source2 in [(0,[first[1],first[2]]),(2,[first[0],first[1]])]:
            results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                state0=dict(service.state);reply=service.command('auxiliary',operation='create_flow',view=view,source=source2,output=out/f'flow-view-{view}-{tag}');results.append(reply)
                if not reply['ok']:r.case(f'aux.flow-view-{view}-rollback-{tag}',service.state==state0)
            r.case(f'aux.flow-view-{view}-acceptance',results[0]['ok']==results[1]['ok'])
            both(f'flow-view-{view}-snapshot')
        before=current;removed=aux('delete',kind='flow',ids=[fid]);history('flow-delete',before,removed)
        original=current
        for tag,service in [('native',native),('oracle',oracle)]:
            saved=q.require_ok(service.command('save',output=out/('save-'+tag)));response=q.require_ok(service.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),output=out/('reopen-'+tag)))
            actual=json.loads(Path(response['model']).read_text('utf8'));r.case('aux.save-reopen-'+tag,nla.reopened_geometry(actual)==nla.reopened_geometry(original))
        for version in (9,12,19,20,40):
            path=out/f'old-{version}.fbm';project.fixture(path,version=version,rich=False);load(path)
            display(show_control_curves=False,show_flowlines=False);aux('create_control',ids=[current['control_edges'][-1]['id']]);aux('rename_control',ids=[current['control_curves'][-1]['id']],name='Promoted')
            both('old-save','save');r.case(f'aux.old-{version}-promoted',project.observe(Path(last['saved_file']))['version']==41)
        base=load();state0=dict(native.state);blocked=out/'blocked';(blocked/'model.json').mkdir(parents=True)
        response=native.command('auxiliary',operation='create_control',ids=[current['control_edges'][0]['id']],output=blocked)
        r.case('aux.publication-rollback',not response['ok'] and native.state==state0)
        after=both('after-publication-failure');r.case('aux.publication-restores-model',modeling.geometry(after)==modeling.geometry(base))
        for method in ('display','auxiliary'):
            reply=oracle.command(method,external_document=True,output=out/('forbid-'+method));r.case('aux.forbid-old-'+method,not reply['ok'] and 'C++' in reply.get('error',''))
    r.data['native_auxiliary_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('aux.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
