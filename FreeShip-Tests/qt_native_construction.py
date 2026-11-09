"""Native panels, cylinders and intersections; retained by qt-document.

Exact Plus oracle comparisons, independent construction expectations, undo and
save/reopen. Test output lives outside the source tree; no desktop automation.
"""
import argparse
import json
import uuid
import copy
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as nla
import fbm_preview_bytes as preview_bytes

OPS={'grid_panel','cylinder','insert_plane','intersect_layers'}

def attributed_model(model):
    """IDs assigned to new Plus objects depend on heap addresses, not geometry.

    Map references by exact coordinates; retain winding, every scalar, name,
    multiplicity, constraint and derived sample. No rounding or epsilon.
    """
    result=copy.deepcopy(modeling.geometry(model))
    points={p['id']:tuple(p['p']) for p in model['control_points']}
    if len(set(points.values()))!=len(points):raise ValueError('Ambiguous exact coordinate identity')
    def cycle(values):
        values=tuple(values);return min(values[i:]+values[:i] for i in range(len(values)))
    faces={f['id']:(f['layer'],cycle(points[i] for i in f['points'])) for f in model['control_faces']}
    for p in result['control_points']:
        p['id']=points[p['id']]
        for key in ('anchor','line_a','line_b'):p[key]=points[p[key]] if p[key] else None
    for edge in result['control_edges']:
        edge.pop('id');edge['a'],edge['b']=sorted((points[edge['a']],points[edge['b']]))
    for face in result['control_faces']:face.pop('id');face['points']=cycle(points[i] for i in face['points'])
    for curve in result['control_curves']:curve.pop('id');curve['controls']=[points[i] for i in curve['controls']]
    for normal in result['surface_normals']:normal['face']=faces[normal['face']]
    for group in result['point_groups']:
        if 'points' in group:group['points']=[points[i] for i in group['points']]
    for key in ('control_points','control_edges','control_faces','control_curves','surface_normals'):
        result[key]=sorted(result[key],key=lambda item:json.dumps(item,sort_keys=True))
    return result

def run(r,runtime,source,out):
    out.mkdir(parents=True)
    timings=[]
    fixture=Path(__file__).parent/'fixtures/native-construction/plane-box'
    provenance=json.loads((fixture/'provenance.json').read_text('utf8'))
    r.case('construction-native.plane-oracle-source',q.common.digest_file(source)==provenance['model_sha256'])
    for i,variant in enumerate(provenance['variants']):
        for name,digest in variant['sha256'].items():r.case(f'construction-native.plane-oracle-{i}.{name}',q.common.digest_file(fixture/str(i)/name)==digest)
    # The frozen geometry predates nine published settings. Recover only those
    # fields from the independently decoded, hash-verified original FBM. Never
    # take expected values from the current publisher or rewrite frozen files.
    persisted=preview_bytes.envelope(source.read_bytes())
    added_settings=dict(save_preview=persisted['enabled'],coefficient_basis=persisted['coefficient_basis'],
                        generate_auxiliary_reports=persisted['generate_auxiliary_reports'],
                        underwater_color_alpha=persisted['underwater_color_alpha'],**persisted['hydro_fields'])
    variants=[]
    for i in range(len(provenance['variants'])):
        frozen=json.loads((fixture/str(i)/'model.json').read_text('utf8'))
        r.case(f'construction-native.plane-oracle-{i}.historical-schema',
               all(frozen['settings'][k]==v for k,v in added_settings.items() if k in frozen['settings']))
        for key,value in added_settings.items():frozen['settings'].setdefault(key,value)
        variants.append(attributed_model(frozen))
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='modeling',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                res=q.require_ok(s.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                results.append((res,json.loads(Path(res['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'construction-native.{step:03}-{label}'
            ag,bg=attributed_model(am),attributed_model(bm)
            variant=next((i for i in range(1,len(variants)) if bg==variants[i]),None)
            pointer_variant=ag==variants[0] and variant is not None
            r.case(prefix+'.model',status='review' if pointer_variant else 'pass' if ag==bg else 'fail',different_fields=[k for k in ag if ag[k]!=bg.get(k)],reason='Exact captured Plus heap-order variants; no numeric tolerance applied.' if pointer_variant else '')
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            if ag==variants[0]:
                for key,name in [('scene','scene.fslp'),('dxf','reference.dxf'),('sections','sections.json')]:r.compare(prefix+'.frozen-original-'+key,a[key],fixture/'0'/name)
            for key in ('scene','dxf','sections'):
                if pointer_variant:
                    name={'scene':'scene.fslp','dxf':'reference.dxf','sections':'sections.json'}[key]
                    r.compare(prefix+'.live-original-known-variant-'+key,b[key],fixture/str(variant)/name)
                    if Path(a[key]).read_bytes()==Path(b[key]).read_bytes():r.compare(prefix+'.'+key,a[key],b[key])
                    else:r.case(prefix+'.'+key,status='review',reason='Live Plus heap order selects a recorded original variant; native matches frozen original variant 0 byte for byte.',variant=variant)
                else:r.compare(prefix+'.'+key,a[key],b[key])
            if method=='modeling' and fields['operation'] in OPS:
                r.case(prefix+'.native-dispatch',a.get('edit_engine')=='cpp-topology' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append({'operation':fields['operation'],'edit_ms':a['topology_edit_ms'],'total_ms':a['document_total_ms']})
            current=am;return am
        def reload():return both('load','load',path=str(source),document_id=str(uuid.uuid4()))
        def op(operation,**fields):return both(operation,operation=operation,**fields)
        def reject(label,**fields):
            for tag,s in [('native',native),('oracle',oracle)]:
                before=dict(s.state);result=s.command('modeling',output=out/f'reject-{label}-{tag}',**fields)
                r.case('construction-native.reject-'+label+'.'+tag,not result['ok'] and s.state==before,error=result.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('construction-native.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('construction-native.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))

        base=reload();layer=base['layers'][0]['id']
        for plane in range(3):
            before=reload();origin=[30.125,5.375,-.875]
            after=op('grid_panel',origin=origin,size_a=4.3,size_b=2.7,columns=4,rows=3,plane=plane,layer_id=layer)
            new=set(e.points(after))-set(e.points(before));axes=[(0,1),(0,2),(1,2)][plane]
            expected=[]
            for i in range(4):
                for j in range(3):
                    p=list(origin);p[axes[0]]=e.f32(p[axes[0]]+e.f32(e.f32(i*e.f32(4.3))/3));p[axes[1]]=e.f32(p[axes[1]]+e.f32(e.f32(j*e.f32(2.7))/2));expected.append(tuple(p))
            r.case(f'construction-native.panel-{plane}.coordinates',sorted(tuple(e.points(after)[i]) for i in new)==sorted(expected))
            r.case(f'construction-native.panel-{plane}.counts',len(new)==12 and len(after['control_faces'])==len(before['control_faces'])+6)
        history('panels',before,after)
        for index,(axis,segments,caps) in enumerate([([0,0,4],8,(True,True)),([3.125,-1.25,4.375],7,(False,True)),([4,0,0],12,(True,False))]):
            base=reload();origin=[30.25,5.125,-.375];end=[x+y for x,y in zip(origin,axis)]
            after=op('cylinder',origin=origin,end=end,radius=1.375,segments=segments,close_start=caps[0],close_end=caps[1],layer_id=layer)
            points=e.points(after);new=sorted(set(points)-set(e.points(base)))
            r.case(f'construction-native.cylinder-{index}.counts',len(new)==2*segments and len(after['control_faces'])==len(base['control_faces'])+segments+sum(caps))
            r.case(f'construction-native.cylinder-{index}.axis',all(points[new[j+1]]==[e.f32(x+y) for x,y in zip(points[new[j]],axis)] for j in range(0,len(new),2)))
            history('cylinder-'+str(index),base,after)
        base=reload()
        after=op('insert_plane',axis=0,position=[5,0,0],create_curve=True)
        r.case('construction-native.plane-fixed-native-baseline',attributed_model(after)==variants[0])
        new=set(e.points(after))-set(e.points(base))
        r.case('construction-native.plane-exact',bool(new) and all(e.points(after)[i][0]==5 for i in new) and all(e.points(after)[i]==p for i,p in e.points(base).items()))
        history('plane',base,after)
        before=dict(native.state);op('insert_plane',axis=0,position=[-50,0,0],create_curve=False)
        r.case('construction-native.plane-noop',native.state==before)
        base=reload();locked=base['control_points'][0]['id']
        both('lock','topology',operation='set_locked',ids=[locked],locked=True)
        reject('plane-locked',operation='insert_plane',axis=0,position=[5,0,0],create_curve=False)
        base=reload();a=op('grid_panel',origin=[30,0,4],size_a=4,size_b=4,columns=2,rows=2,plane=0,layer_id=layer)
        layers=both('add-layer','manage_layers',operation='create',changes={'name':'Cutting surface','symmetric':False,'use_in_hydrostatics':False})
        other=next(v['id'] for v in layers['layers'] if v['id']!=layer)
        b=op('grid_panel',origin=[32,-1,3],size_a=6,size_b=2,columns=2,rows=2,plane=2,layer_id=other)
        after=op('intersect_layers',layer_id=layer,other_layer_id=other)
        new=set(e.points(after))-set(e.points(b))
        r.case('construction-native.intersection-exact',sorted(e.points(after)[i] for i in new)==[[32,0,4],[32,4,4]])
        r.case('construction-native.intersection-fixed-old',all(e.points(after)[i]==p for i,p in e.points(b).items()))
        history('intersection',b,after)
        reject('same-layer',operation='intersect_layers',layer_id=layer,other_layer_id=layer)
        saved=q.require_ok(native.command('save',output=out/'saved'))
        reopened=both('reopen','load',path=saved['saved_file'],document_id=str(uuid.uuid4()))
        r.case('construction-native.save-roundtrip',nla.reopened_geometry(reopened)==nla.reopened_geometry(after))
        # Single-sided edits must not leave a partial candidate on a late error.
        reject('bad-grid-count',operation='grid_panel',origin=[30,1,2],size_a=1,size_b=1,columns=2**32+3,rows=3,plane=0,layer_id=layer)
        reject('zero-cylinder-axis',operation='cylinder',origin=[30,1,2],end=[30,1,2],radius=1,segments=8,close_start=True,close_end=True,layer_id=layer)
        reject('bad-cylinder-count',operation='cylinder',origin=[30,1,2],end=[30,1,4],radius=1,segments=129,close_start=True,close_end=True,layer_id=layer)
        # Old InsertPlane puts a nil InsertControlPoint result in its sorted
        # point list when the crossing is within 1e-4 of an endpoint. The native
        # implementation must skip it without publishing a half-built net.
        base=reload();before=dict(native.state)
        res=q.require_ok(native.command('modeling',operation='insert_plane',axis=0,position=[.00005,0,0],create_curve=True,output=out/'near-endpoint'))
        m=json.loads(Path(res['model']).read_text('utf8'))
        r.case('construction-native.near-endpoint-atomic-noop',native.state==before and modeling.geometry(m)==modeling.geometry(base))
        for operation in sorted(OPS):
            result=oracle.command('modeling',operation=operation,output=out/('forbid-'+operation),external_document=True)
            r.case('construction-native.forbid-old-'+operation,not result['ok'] and 'C++' in result.get('error',''))
        # Two separated cutter sheets cross four adjacent source faces. This
        # independent analytic case exercises restart, repeated face splitting,
        # shared edges and names without relying on Plus pointer allocation.
        def only(label,method='modeling',**fields):
            response=q.require_ok(native.command(method,output=out/('analytic-'+label),**fields))
            return json.loads(Path(response['model']).read_text('utf8'))
        base=only('load','load',path=str(source),document_id=str(uuid.uuid4()))
        only('panel',operation='grid_panel',origin=[30,0,4],size_a=4,size_b=4,columns=3,rows=3,plane=0,layer_id=layer)
        made=only('layer','manage_layers',operation='create',changes={'name':'Two cutters','symmetric':False,'use_in_hydrostatics':False})
        cutter=next(l['id'] for l in made['layers'] if l['id']!=layer)
        for x in (31,33):before=only('cutter-'+str(x),operation='grid_panel',origin=[x,-1,3],size_a=6,size_b=2,columns=2,rows=2,plane=2,layer_id=cutter)
        after=only('intersections',operation='intersect_layers',layer_id=layer,other_layer_id=cutter)
        new=set(e.points(after))-set(e.points(before))
        r.case('construction-native.multicut-exact',sorted(e.points(after)[i] for i in new)==[[x,y,4] for x in (31,33) for y in (0,2,4)])
        r.case('construction-native.multicut-existing-fixed',all(e.points(after)[i]==p for i,p in e.points(before).items()))
        r.case('construction-native.multicut-faces',len(after['control_faces'])==len(before['control_faces'])+4)
        r.case('construction-native.multicut-names',sorted(p['name'] for p in after['control_points'] if p['id'] in new)==['np'+str(i) for i in range(6)])
        undone=only('undo','undo');redone=only('redo','redo')
        r.case('construction-native.multicut-history',modeling.geometry(undone)==modeling.geometry(before) and modeling.geometry(redone)==modeling.geometry(after))
        saved=q.require_ok(native.command('save',output=out/'analytic-saved'))
        reopened=only('reopened','load',path=saved['saved_file'],document_id=str(uuid.uuid4()))
        r.case('construction-native.multicut-save',nla.reopened_geometry(reopened)==nla.reopened_geometry(after))
    r.data['native_construction_timings']=timings

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('native-construction.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    e.write_report(r,a.output);print(r.data['counts']);raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
