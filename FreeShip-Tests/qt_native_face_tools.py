"""Native face grouping, rectangular NURBS patches and normal repair.

run(report, runtime, source, out) is the unified regression entry. Synthetic
meshes and FBM payloads are generated independently of the new implementation.
All coordinates, properties, winding and exported numbers compare exactly.
The old assembler sorts heap pointers: alternate valid patch partitions are
reported explicitly as review, with exact geometry and partition evidence.
"""
import argparse
import copy
import json
import math
import os
import struct
import subprocess
import uuid
from collections import Counter,defaultdict
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import qt_native_lackenby as lackenby
import qt_native_transforms as affine
import native_linesplan_app as nla

OPS={'auto_group_layers','assemble_faces','repair_normals'}


def mesh(points,faces,creases=()):
    edges={};incidence=Counter();at=defaultdict(list)
    for i,face in enumerate(faces):
        for a,b in zip(face,face[1:]+face[:1]):
            key=tuple(sorted((a,b)));incidence[key]+=1
            if key not in edges:edges[key]=len(edges)+1
        for p in face:at[p].append(i)
    sharp={key for key,n in incidence.items() if n!=2}|{tuple(sorted(pair)) for pair in creases}
    types=[]
    for p in range(len(points)):
        n=sum(p in edge for edge in sharp)
        types.append(3 if len(at[p])==1 or n>2 else 1 if n==2 else 2 if n==1 else 0)
    return {'control_points':[{'id':i+1,'p':[e.f32(v) for v in p],'vertex_type':types[i]} for i,p in enumerate(points)],
            'control_edges':[{'id':i,'a':a+1,'b':b+1,'crease':(a,b) in sharp} for (a,b),i in edges.items()],
            'control_faces':[{'id':i+1,'points':[p+1 for p in f]} for i,f in enumerate(faces)]}


def grid(columns=4,rows=3,missing=(),barrier=False):
    points=[[1+2*i,1+2*j,.125*i*i+.25*j] for j in range(rows+1) for i in range(columns+1)]
    faces=[]
    for j in range(rows):
        for i in range(columns):
            if (i,j) in missing:continue
            a=j*(columns+1)+i;faces.append([a,a+1,a+columns+2,a+columns+1])
    creases=[(j*(columns+1)+2,(j+1)*(columns+1)+2) for j in range(rows)] if barrier else []
    return mesh(points,faces,creases)


def layer_records(path):
    """Independent 5.0 parser up to layer records, including exact raw payloads."""
    data=path.read_bytes();at=0
    def take(fmt):
        nonlocal at
        values=struct.unpack_from('<'+fmt,data,at);at+=struct.calcsize('<'+fmt);return values[0] if len(values)==1 else values
    def skip(n):
        nonlocal at
        if n<0 or at+n>len(data):raise ValueError('Invalid face-tool fixture')
        at+=n
    def text():
        n=take('i');start=at;skip(n);return data[start:at]
    if text()!=b'FREE!ship' or take('B')!=44:raise ValueError('Face fixture parser requires 5.0')
    skip(34);text();text();skip(36);text();text();skip(4)
    if take('B'):skip(8);skip(take('i'))
    skip(1+16);skip(take('i')*4);skip(13);skip(take('i')*4);skip(take('i')*4);skip(11)
    records=[]
    for _ in range(take('i')):
        name=text();start=at;skip(26);records.append({'name':name,'fields':data[start:at],'offset':start})
    active=take('i')
    return records,active


def make_fixture(path,kind='rectangle'):
    if kind=='rectangle':model=grid()
    elif kind=='crease':model=grid(barrier=True)
    elif kind=='l-shape':model=grid(2,2,missing={(1,1)})
    elif kind=='mixed':model=mesh([[1,1,0],[3,1,0],[3,3,0],[1,3,0],[5,1,0],[5,3,0],[7,2,0]],[[0,1,2,3],[1,4,5,2],[4,6,5]])
    elif kind=='tube':
        points=[[x,4+math.cos(i*math.pi/2),3+math.sin(i*math.pi/2)] for x in (1,5) for i in range(4)]
        model=mesh(points,[[i,(i+1)%4,(i+1)%4+4,i+4] for i in range(4)])
    elif kind=='nonmanifold':model=mesh([[1,1,0],[3,1,0],[1,3,0],[1,1,2],[1,-1,0]],[[0,1,2],[1,0,3],[0,1,4]])
    elif kind=='mobius':
        points=[]
        for i in range(3):
            t=2*math.pi*i/3
            for v in (-.4,.4):points.append([(2+v*math.cos(t/2))*math.cos(t)+4,(2+v*math.cos(t/2))*math.sin(t)+4,v*math.sin(t/2)+2])
        model=mesh(points,[[0,2,3,1],[2,4,5,3],[4,1,0,5]])
    else:raise ValueError(kind)
    lackenby.fixture(path,model)
    records,_=layer_records(path);data=bytearray(path.read_bytes());struct.pack_into('<i',data,records[0]['offset']+22,73);path.write_bytes(data)
    return affine.observe(path),model


def exact_layers(model):
    """Resolve stable IDs by complete attributes and exact owned face IDs."""
    result=copy.deepcopy(modeling.geometry(model));members=defaultdict(list)
    for face in model['control_faces']:members[face['layer']].append(face['id'])
    # JSON 0 and 0.0 represent the same exact number. Normalize the two real
    # fields before encoding an identity string; preserve all numeric values.
    identities={layer['id']:json.dumps([{k:float(v) if k in ('thickness','material_density') else v for k,v in layer.items() if k!='id'},sorted(members[layer['id']])],sort_keys=True) for layer in model['layers']}
    for layer in result['layers']:layer['id']=identities[layer['id']]
    for key in ('control_faces','triangles'):
        for item in result[key]:item['layer']=identities[item['layer']]
    result['layers'].sort(key=lambda layer:layer['id'])
    return result


def geometry_without_layers(model):
    result=copy.deepcopy(modeling.geometry(model));result.pop('layers')
    for key in ('control_faces','triangles'):
        for item in result[key]:item.pop('layer')
    # A layer's FPatches insertion order can change triangle traversal, but all
    # exact triangles including winding and multiplicity must remain present.
    result['triangles'].sort(key=lambda t:json.dumps(t,sort_keys=True))
    return result


def face_properties(model):
    layers={l['id']:{**{k:v for k,v in l.items() if k not in ('id','name')},'alpha_blend':l.get('alpha_blend',255)} for l in model['layers']}
    return {f['id']:layers[f['layer']] for f in model['control_faces']}


def inherited_face_properties(before,after):
    expected=copy.deepcopy(face_properties(before));old={l['id'] for l in before['layers']}
    # Plus AssignProperties/CopyLayerProperties deliberately do not copy alpha.
    # Keep all other properties, and keep alpha on layers that were not replaced.
    for face in after['control_faces']:
        if face['layer'] not in old:expected[face['id']]['alpha_blend']=255
    return face_properties(after)==expected


def expected_waterline(model):
    """Independent geometric definition: draft above the live hydrostatic net.

    Orphan control points and lower non-hydro geometry do not set this datum.
    With no hydro faces, use the complete referenced surface/edge bounds.
    """
    hydro={l['id'] for l in model['layers'] if l['use_in_hydrostatics']}
    ids={p for f in model['control_faces'] if f['layer'] in hydro for p in f['points']}
    if not ids:
        ids={p for f in model['control_faces'] for p in f['points']}
        ids.update(p for edge in model['control_edges'] for p in (edge['a'],edge['b']))
    points=e.points(model)
    return e.f32(min((points[p][2] for p in ids),default=0)+model['settings']['draft'])


def valid_patches(model):
    """Independent topological rectangular/periodic quad-grid certificate."""
    grouped=defaultdict(list);sharp={tuple(sorted((x['a'],x['b']))) for x in model['control_edges'] if x['crease']}
    for face in model['control_faces']:grouped[face['layer']].append(face)
    for layer in model['layers']:
        if not layer['name'].startswith('Patch '):continue
        faces=grouped[layer['id']]
        if not faces:return False
        if len(faces)==1:continue
        if any(len(f['points'])!=4 for f in faces):return False
        edges=defaultdict(list);vertices=Counter()
        for index,face in enumerate(faces):
            for p in face['points']:vertices[p]+=1
            for a,b in zip(face['points'],face['points'][1:]+face['points'][:1]):edges[tuple(sorted((a,b)))].append(index)
        if any(len(adj)>2 or len(adj)==2 and edge in sharp for edge,adj in edges.items()):return False
        adjacency=defaultdict(set);boundary=Counter()
        for edge,adj in edges.items():
            if len(adj)==1:
                for p in edge:boundary[p]+=1
            else:a,b=adj;adjacency[a].add(b);adjacency[b].add(a)
        seen={0};todo=[0]
        while todo:
            for nxt in adjacency[todo.pop()]-seen:seen.add(nxt);todo.append(nxt)
        if len(seen)!=len(faces) or any(n!=2 for n in boundary.values()):return False
        if any(n!=4 for p,n in vertices.items() if p not in boundary):return False
        corners=sum(n==1 for p,n in vertices.items() if p in boundary)
        euler=len(vertices)-len(edges)+len(faces)
        if (euler,corners) not in ((1,4),(0,0)):return False
    return True


def consistent_normals(model):
    edges=defaultdict(list)
    for face in model['control_faces']:
        for a,b in zip(face['points'],face['points'][1:]+face['points'][:1]):edges[tuple(sorted((a,b)))].append(1 if a<b else -1)
    return all(len(v)<=2 and (len(v)<2 or sum(v)==0) for v in edges.values())


def reopened_geometry(model):
    result=nla.reopened_geometry(model)
    points={p['id']:tuple(p['p']) for p in model['control_points']};members=defaultdict(list)
    for face in model['control_faces']:
        ring=tuple(points[p] for p in face['points']);members[face['layer']].append(min(ring[i:]+ring[:i] for i in range(len(ring))))
    identities={layer['id']:json.dumps([{k:v for k,v in layer.items() if k!='id'},sorted(members[layer['id']])],sort_keys=True) for layer in model['layers']}
    for layer in result['layers']:layer['id']=identities[layer['id']]
    for key in ('control_faces','triangles'):
        for item in result[key]:item['layer']=identities[item['layer']]
    result['layers'].sort(key=lambda layer:layer['id'])
    return result


def structural(r,runtime,fixtures,out):
    out.mkdir();executable=runtime/'native_face_tools_probe.exe';environment=dict(os.environ)
    if not executable.is_file():
        r.case('face-tools.structural-probe',status='not_run',reason='Runtime does not contain native_face_tools_probe.exe');return
    environment['PATH']=str(runtime)+os.pathsep+environment.get('PATH','')
    def run(label,kind,request,bindings=None):
        req=out/(label+'.request.json');req.write_text(json.dumps({'method':'modeling',**request}),'utf8');result=out/(label+'.json')
        command=[str(executable),'--model',str(fixtures[kind][0]),'--request',str(req),'--output',str(result)]
        if bindings is not None:
            path=out/(label+'.bindings.json');path.write_text(json.dumps(bindings),'utf8');command+=['--bindings',str(path)]
        digest=q.common.digest_file(fixtures[kind][0]);process=subprocess.run(command,capture_output=True,env=environment,timeout=30,creationflags=q.common.FLAGS)
        (out/(label+'.log')).write_bytes(process.stdout+process.stderr);response=json.loads(result.read_text('utf8'))
        r.case('face-tools.structural-'+label+'.source-unchanged',digest==q.common.digest_file(fixtures[kind][0]));return response
    for kind,message in [('nonmanifold','nonmanifold'),('mobius','consistently oriented')]:
        response=run(kind,kind,{'operation':'repair_normals'});r.case('face-tools.structural-'+kind+'.rejected',not response['ok'] and message in response.get('error',''),error=response.get('error'))
    baseline=run('noop','rectangle',{'operation':'repair_normals'});r.case('face-tools.structural-noop',baseline['ok'] and not baseline['changed'])
    bindings=baseline['bindings'];face_ids=bindings['faces'];original=copy.deepcopy(bindings)
    rows=[face_ids[:-1],face_ids[-1:]]
    for label,patch in [('duplicate-layer-face',{'layer_faces':[face_ids,face_ids[-1:]]}),
            ('incomplete-layer-faces',{'layer_faces':[face_ids[:-2],face_ids[-1:]]}),
            ('wrong-layer-face',{'layer_faces':[face_ids[-1:],face_ids[:-1]]}),
            ('stale-layer-counter',{'last_used_layer_id':0}),('layer-counter-overflow',{'last_used_layer_id':2**31-1})]:
        response=run(label,'rectangle',{'operation':'auto_group_layers','ids':[face_ids[0]]},{**copy.deepcopy(original),**patch})
        r.case('face-tools.structural-'+label+'.rejected',not response['ok'],error=response.get('error'))
    bindings={**copy.deepcopy(original),'layer_faces':rows,'last_used_layer_id':99}
    response=run('valid-layer-order','rectangle',{'operation':'auto_group_layers','ids':[face_ids[0]]},bindings)
    r.case('face-tools.structural-valid-layer-order',response['ok'] and response['bindings']['last_used_layer_id']==100)


def run(r,runtime,source,out):
    out.mkdir(parents=True,exist_ok=True);timings=[]
    fixtures={}
    for kind in ('rectangle','crease','l-shape','mixed','tube','nonmanifold','mobius'):
        path=out/(kind+'.fbm');fixtures[kind]=(path,*make_fixture(path,kind))
    structural(r,runtime,fixtures,out/'structural')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None;oracle_current=None
        def both(label,method='snapshot',partition_review=False,**fields):
            nonlocal step,current,oracle_current
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                response=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                results.append((response,json.loads(Path(response['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'face-tools.{step:03}-{label}'
            r.case(prefix+'.hydrostatic-waterline',am['settings']['waterline_z']==expected_waterline(am),
                   actual=am['settings']['waterline_z'],expected=expected_waterline(am))
            # The historical bridge writes Surface.Min.Z, whereas original
            # FindLowestHydrostaticsPoint uses hydrostatic faces. Check the raw
            # bridge contract, then derive the expected datum from its own net.
            r.case(prefix+'.oracle-global-waterline',bm['settings']['waterline_z']==e.f32(bm['bounds']['min'][2]+bm['settings']['draft']))
            bm=copy.deepcopy(bm);bm['settings']['waterline_z']=expected_waterline(bm)
            ag,bg=exact_layers(am),exact_layers(bm)
            different=ag!=bg;alternative=partition_review and different and geometry_without_layers(am)==geometry_without_layers(bm) and face_properties(am)==face_properties(bm) and valid_patches(am) and valid_patches(bm)
            r.case(prefix+'.model',status='review' if alternative else 'pass' if not different else 'fail',different_fields=[k for k in ag if ag[k]!=bg.get(k)],
                   reason='Original pointer sorting chooses another valid rectangular patch partition; exact coordinates, winding and complete geometry are equal.' if alternative else '')
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            if alternative:
                r.case(prefix+'.geometry-exact',geometry_without_layers(am)==geometry_without_layers(bm));r.case(prefix+'.native-patches-valid',valid_patches(am));r.case(prefix+'.original-patches-valid',valid_patches(bm))
                r.case(prefix+'.export-partition',status='review',actual={k:a[k] for k in ('scene','dxf','sections')},expected={k:b[k] for k in ('scene','dxf','sections')},reason='Retained raw exports use different patch layer names; this is not a numeric equivalence assertion.')
            else:
                for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='modeling' and fields.get('operation') in OPS:
                r.case(prefix+'.native-dispatch',a.get('edit_engine') in ('cpp-face-tools','cpp-topology') and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append({'operation':fields['operation'],'edit_ms':a.get('face_tools_edit_ms',a.get('topology_edit_ms')),'total_ms':a['document_total_ms']})
            current,oracle_current=am,bm;return am
        def load(kind='rectangle'):return both('load-'+kind,'load',path=str(fixtures[kind][0]),document_id=str(uuid.uuid4()))
        def op(operation,**fields):return both(operation,'modeling',operation=operation,**fields)
        def history(label,before,after,review=False):
            undone=both(label+'-undo','undo',partition_review=review);r.case('face-tools.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo',partition_review=review);r.case('face-tools.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        def reject(label,method='modeling',**fields):
            before=current
            for tag,service in [('native',native),('oracle',oracle)]:
                old=dict(service.state);response=service.command(method,output=out/('reject-'+label+'-'+tag),**fields)
                r.case('face-tools.reject-'+label+'.'+tag,not response['ok'] and old==service.state,error=response.get('error'))
            after=both('after-reject-'+label);r.case('face-tools.reject-'+label+'.atomic',modeling.geometry(before)==modeling.geometry(after))
        def save_raw(label):
            responses=[]
            for tag,service in [('native',native),('oracle',oracle)]:responses.append(q.require_ok(service.command('save',output=out/(label+'-'+tag+'-save'))))
            return Path(responses[0]['saved_file'])

        # Supplied source keeps its actual historical FBM version; the synthetic
        # fixtures below deliberately cover the current 5.0 layer record too.
        before=both('provided-source','load',path=str(source),document_id=str(uuid.uuid4()))
        op('repair_normals');before=current
        selected=[before['control_faces'][0]['id']];after=op('auto_group_layers',ids=selected)
        r.case('face-tools.provided-source.coordinates-fixed',e.points(before)==e.points(after))

        # A smooth connected layer is one auto-group, regardless of whether it
        # can be assembled into a rectangular NURBS patch.
        for kind in ('rectangle','l-shape','mixed'):
            before=load(kind);state=dict(native.state);after=op('auto_group_layers')
            r.case('face-tools.'+kind+'.auto-noop',state==native.state and modeling.geometry(before)==modeling.geometry(after))
        before=load('crease');after=op('auto_group_layers');history('crease-group',before,after)
        r.case('face-tools.crease.exact-layer-count',len(after['layers'])==3)
        r.case('face-tools.crease.coordinates-topology-fixed',geometry_without_layers(before)==geometry_without_layers(after))
        saved_path=save_raw('crease-group');records,active=layer_records(saved_path)
        r.case('face-tools.crease.first-layer-active',active==0)
        r.case('face-tools.crease.inherited-alpha-rule',sorted(struct.unpack_from('<i',row['fields'],22)[0] for row in records)==[73,255,255])
        initial=fixtures['crease'][1];saved=affine.observe(saved_path)
        for key in ('settings','hydro_bytes','markers','marker_attributes','planes','calculation_bytes'):
            r.case('face-tools.crease.preserve-'+key,saved[key]==initial[key])

        # Explicit selections use stable face IDs, including deleted slots.
        before=load();selected=[f['id'] for f in before['control_faces'][:2]];after=op('auto_group_layers',ids=selected)
        r.case('face-tools.selection.only-selected-move',all((f['layer']!=next(x for x in before['control_faces'] if x['id']==f['id'])['layer'])==(f['id'] in selected) for f in after['control_faces']))
        history('selection',before,after)
        before=load();removed=before['control_faces'][0]['id']
        both('delete-face-slot','topology',operation='delete',kind='face',ids=[removed]);selected=[current['control_faces'][1]['id']]
        before=current;after=op('auto_group_layers',ids=selected)
        added=next(l for l in after['layers'] if l['id'] not in {v['id'] for v in before['layers']})
        r.case('face-tools.deleted-slot.name-uses-stable-id',added['name']=='Hull '+str(selected[0]))
        history('deleted-slot-group',before,after)
        before=load();layer=before['layers'][0]['id'];both('hide-hull','update_layer',layer_id=layer,changes={'visible':False,'symmetric':False,'use_in_hydrostatics':False,'use_for_intersections':False,'developable':True,'color':0x214365,'thickness':.0125,'material_density':2.75})
        before=current;old=dict(native.state);op('auto_group_layers');r.case('face-tools.hidden-default-noop',old==native.state)
        selected=[before['control_faces'][0]['id']];after=op('auto_group_layers',ids=selected)
        new=next(l for l in after['layers'] if l['id'] not in {x['id'] for x in before['layers']});original=next(l for l in before['layers'] if l['id']==layer)
        r.case('face-tools.hidden-selected-properties',
               {**{k:v for k,v in new.items() if k not in ('id','name')},'alpha_blend':new.get('alpha_blend',255)}==
               {**{k:v for k,v in original.items() if k not in ('id','name')},'alpha_blend':255})
        r.case('face-tools.hidden-original-alpha-retained',next(l for l in after['layers'] if l['id']==layer).get('alpha_blend',255)==original['alpha_blend']==73)
        history('hidden-selected',before,after)

        for kind in ('rectangle','l-shape','mixed','tube','crease'):
            before=load(kind);after=op('assemble_faces',partition_review=True)
            r.case('face-tools.'+kind+'.assemble-valid',valid_patches(after))
            r.case('face-tools.'+kind+'.assemble-exact-geometry',geometry_without_layers(before)==geometry_without_layers(after))
            r.case('face-tools.'+kind+'.assemble-properties',inherited_face_properties(before,after))
            r.case('face-tools.'+kind+'.assemble-all-faces',len(after['control_faces'])==len(before['control_faces']) and all(l['name'].startswith('Patch ') for l in after['layers']))
            if kind=='l-shape':r.case('face-tools.l-shape.assemble-is-not-grouping',len(after['layers'])>2)
            history('assemble-'+kind,before,after,True)
            saved=save_raw('assemble-'+kind);reopened=both('reopen-'+kind,'load',path=str(saved),document_id=str(uuid.uuid4()))
            r.case('face-tools.'+kind+'.reopen',reopened_geometry(reopened)==reopened_geometry(after))

        before=load();layer=before['layers'][0]['id'];both('hide-before-assemble','update_layer',layer_id=layer,changes={'visible':False})
        before=current;after=op('assemble_faces',partition_review=True)
        r.case('face-tools.assemble-skips-hidden-layer',next(l for l in after['layers'] if l['id']==layer)==next(l for l in before['layers'] if l['id']==layer) and all(f['layer']==layer for f in after['control_faces'][:-1]))
        before=load();old=dict(native.state);op('repair_normals');r.case('face-tools.consistent-normals-noop',native.state==old)
        face=before['control_faces'][1];flipped=both('deliberate-winding','modeling',operation='flip_faces',ids=[face['id']]);r.case('face-tools.bad-winding-fixture',not consistent_normals(flipped))
        after=op('repair_normals');r.case('face-tools.repair-independent-orientation',consistent_normals(after));r.case('face-tools.repair-original-winding',after['control_faces']==before['control_faces']);history('repair',flipped,after)
        both('repair-reset','undo');point=face['points'][0];both('lock-repair-point','topology',operation='set_locked',ids=[point],locked=True);reject('locked-repair',operation='repair_normals')
        # Nonmanifold publication is rejected by the native section subsystem
        # before repair is reachable; structural() tests the repair guard itself.
        for tag,service in [('native',native),('oracle',oracle)]:
            old=dict(service.state);response=service.command('load',path=str(fixtures['nonmanifold'][0]),document_id=str(uuid.uuid4()),output=out/('nonmanifold-publication-'+tag))
            r.case('face-tools.nonmanifold-publication.'+tag,not response['ok'] and service.state==old,error=response.get('error'))
        load('mobius');reject('mobius',operation='repair_normals')
        before=load();ids=[f['id'] for f in before['control_faces']]
        for label,selection in [('empty',[]),('duplicate',[ids[0],ids[0]]),('missing',[1999999]),('fractional',[1.5]),('wrong-type',True)]:reject(label,operation='auto_group_layers',ids=selection)
        old=dict(native.state);blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        result=native.command('modeling',operation='auto_group_layers',ids=ids[:2],output=blocked)
        r.case('face-tools.publication-failure.atomic',not result['ok'] and native.state==old)
        after=both('after-publication-failure');r.case('face-tools.publication-failure.model',modeling.geometry(after)==modeling.geometry(before))
        # A native layer edit followed by another topology edit exercises the
        # exact layer insertion-order binding, and surviving IDs after undo.
        grouped=op('auto_group_layers',ids=ids[:2]);target=grouped['layers'][-1]['id'];both('assign-extra-face','topology',operation='assign_layer',ids=ids[2:3],layer_id=target)
        op('auto_group_layers',ids=ids[2:4]);op('repair_normals')
        for operation in sorted(OPS):
            result=oracle.command('modeling',operation=operation,external_document=True,output=out/('forbid-'+operation))
            r.case('face-tools.forbid-old-'+operation,not result['ok'] and 'C++' in result.get('error',''))
    r.data['native_face_tools_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('face-tools.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
