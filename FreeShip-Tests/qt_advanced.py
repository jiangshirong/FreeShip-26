"""Advanced editing through production Qt actions, independent core arithmetic and atomic service history.

All files and settings are private. Hidden dialogs do not establish desktop acceptance.
"""
from pathlib import Path
from collections import Counter
import argparse, json, math, shutil
import qt_migration as q
import qt_editing as editing
import qt_editing_fixture as fixture
import qt_backgrounds as backgrounds
import qt_auxiliary as auxiliary
import native_linesplan_app as native

PHASES=('original','translated','undo_translate','redo_translate','scaled','rotated','extruded','mirrored','undo_mirror','redo_mirror','reopened')
f32=editing.f32


def edge_key(a,b):return tuple(sorted((a,b)))


def incidence(model):
    return Counter(edge_key(a,b) for f in model['control_faces'] for a,b in zip(f['points'],f['points'][1:]+f['points'][:1]))


def direction(face,a,b):
    edges=list(zip(face['points'],face['points'][1:]+face['points'][:1]))
    return 1 if (a,b) in edges else -1 if (b,a) in edges else 0


def observed_core_geometry(model):
    """FBM renumbers records on save; preserve coordinates, relations and winding instead of file ordinals."""
    coords={p['id']:tuple(p['p']) for p in model['control_points']}
    if len(set(coords.values()))!=len(coords):raise ValueError('Core comparison needs distinct point coordinates')
    points=sorted((coords[p['id']],p['locked'],p['batch_movable'],*(coords.get(p[k],()) for k in ('anchor','linear_a','linear_b'))) for p in model['control_points'])
    edges=sorted((tuple(sorted((coords[e['a']],coords[e['b']]))),e['crease']) for e in model['control_edges'])
    faces=[]
    for f in model['control_faces']:
        cycle=[coords[i] for i in f['points']]
        faces.append((min(tuple(cycle[i:]+cycle[:i]) for i in range(len(cycle))),f['layer_index']))
    return points,edges,sorted(faces)


def check_extrusion(r,prefix,before,after,edge_ids,vector,layer_id):
    selected=[e for e in before['control_edges'] if e['id'] in edge_ids]
    original=editing.points(before);endpoints={i for e in selected for i in (e['a'],e['b'])}
    actual=editing.points(after);new={i:p for i,p in actual.items() if i not in original}
    expected={tuple(f32(p+f32(v)) for p,v in zip(original[i],vector)) for i in endpoints}
    r.case(prefix+'.coordinates',len(new)==len(endpoints) and set(map(tuple,new.values()))==expected)
    r.case(prefix+'.old-positions',all(actual[i]==p for i,p in original.items()))
    old_records={p['id']:p for p in before['control_points']}
    r.case(prefix+'.unrelated-old-point-properties',all(p==old_records[p['id']] for p in after['control_points'] if p['id'] in old_records and p['id'] not in endpoints))
    old_faces={f['id'] for f in before['control_faces']};faces=[f for f in after['control_faces'] if f['id'] not in old_faces]
    r.case(prefix+'.face-count-layer-quads',len(faces)==len(selected) and all(f['layer']==layer_id and len(f['points'])==4 for f in faces))
    r.case(prefix+'.edge-count',len(after['control_edges'])==len(before['control_edges'])+len(selected)+len(endpoints))
    face_counts=Counter(i for f in after['control_faces'] for i in f['points'])
    r.case(prefix+'.new-single-face-points-are-corners',all(p['vertex_type']==3 for p in after['control_points'] if p['id'] in new and face_counts[p['id']]<2))
    for e in selected:
        adjacent=[f for f in before['control_faces'] if direction(f,e['a'],e['b'])]
        created=[f for f in faces if direction(f,e['a'],e['b'])]
        r.case(prefix+f'.winding-{e["id"]}',len(created)==1 and (not adjacent or direction(created[0],e['a'],e['b'])==-direction(adjacent[0],e['a'],e['b'])))


def check_math(r,prefix,probe,model,out,before,after,selected,operation,vector):
    points=editing.points(before);ids=sorted(selected)
    expected=fixture.run(probe,model,out,'advanced_math',{'operation':operation,'vector':vector,'points':[points[i] for i in ids]})['points']
    wanted={**points,**dict(zip(ids,expected))}
    r.case(prefix+'.exact-original-arithmetic',editing.points(after)==wanted)
    if operation!='rotate':
        analytic=[[f32(p+f32(v)) if operation=='translate' else f32(p*f32(v)) for p,v in zip(points[i],vector)] for i in ids]
        r.case(prefix+'.independent-coordinate-formula',expected==analytic)
    r.case(prefix+'.topology-identities',before['control_edges']==after['control_edges'] and before['control_faces']==after['control_faces'])


def check_production(r,index,probe,model,folder,child):
    prefix=str(index);art=child['artifacts'];states={s:json.loads(Path(art[s+'_model']).read_text('utf8')) for s in PHASES}
    face=next(f for f in states['original']['control_faces'] if f['id']==art['transformed_face'])
    for operation,previous,current in [('translate','original','translated'),('scale','redo_translate','scaled'),('rotate','scaled','rotated')]:
        vector=[art[operation+'_input'][a] for a in ('x','y','z')]
        check_math(r,prefix+'.'+operation,probe,model,folder/(operation+'-oracle'),states[previous],states[current],face['points'],operation,vector)
        # waterline_z is derived from hull minimum + draft, not a stored project setting.
        stable=lambda m:{k:v for k,v in m['settings'].items() if k!='waterline_z'}
        r.case(prefix+'.'+operation+'.partial-selection-keeps-settings',stable(states[previous])==stable(states[current]))
        r.case(prefix+'.'+operation+'.waterline-follows-hull-min',states[current]['settings']['waterline_z']==f32(states[current]['bounds']['min'][2]+states[current]['settings']['draft']))
    for later,earlier in [('undo_translate','original'),('redo_translate','translated'),('undo_mirror','extruded'),('redo_mirror','mirrored')]:
        r.case(prefix+'.'+later+'.exact-state',q.geometry(states[later])==q.geometry(states[earlier]) and states[later]['sections']==states[earlier]['sections'] and auxiliary.auxiliary(states[later],True)==auxiliary.auxiliary(states[earlier],True))
        for kind in ('scene','dxf','sections'):r.compare(prefix+'.'+later+'.'+kind,art[later+'_'+kind],art[earlier+'_'+kind])
    vector=[art['extrude_input'].get(a,0) for a in ('x','y','z')];before=states['rotated'];after=states['extruded']
    check_extrusion(r,prefix+'.extrude',before,after,[art['extruded_edge']],vector,before['layers'][0]['id'])
    before=states['extruded'];after=states['mirrored'];face=next(f for f in before['control_faces'] if f['id']==art['mirrored_face']);p=editing.points(before)
    expected=fixture.run(probe,model,folder/'mirror-oracle','advanced_math',{'operation':'mirror','vector':[0,0,0],**art['mirror_input'],'points':[p[i] for i in reversed(face['points'])]})['points']
    created=[f for f in after['control_faces'] if f['id'] not in {f['id'] for f in before['control_faces']}];ap=editing.points(after)
    r.case(prefix+'.mirror.exact-reversed-original-reflection',len(created)==1 and [ap[i] for i in created[0]['points']]==expected and created[0]['layer']==face['layer'])
    r.case(prefix+'.mirror.keeps-source',all(ap[i]==v for i,v in p.items()))
    if len(created)==1:
        point_map=dict(zip(reversed(face['points']),created[0]['points']))
        expected_edges={edge_key(point_map[e['a']],point_map[e['b']]):e['crease'] for e in before['control_edges'] if e['a'] in point_map and e['b'] in point_map}
        actual_edges={edge_key(e['a'],e['b']):e['crease'] for e in after['control_edges']}
        r.case(prefix+'.mirror.preserves-crease-properties',all(actual_edges[k]==v for k,v in expected_edges.items()))
    r.case(prefix+'.reopen.attributed-topology',native.reopened_geometry(after)==native.reopened_geometry(states['reopened']))
    for kind in ('scene','dxf','sections'):r.compare(prefix+'.reopen.'+kind,art['mirrored_'+kind],art['reopened_'+kind])
    auxiliary.inspect_saved(r,prefix+'.derived-curves',probe,Path(art['saved_file']),states['reopened'],folder/'saved-observer')
    return states


def service_cases(r,runtime,probe,model,out):
    out.mkdir();exe=runtime/'bridge/FreeShipGeometryBridge.exe'
    with q.Service(exe,out/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):return editing.take(r,'service',service,out,name,method,**fields)
        base=take('base','load',path=str(model));m=base['model'];pts=editing.points(m);degree=incidence(m)
        edge=next(e for e in m['control_edges'] if degree[edge_key(e['a'],e['b'])]==1)
        interior=next(e for e in m['control_edges'] if degree[edge_key(e['a'],e['b'])]==2)
        face=next(f for f in m['control_faces'] if direction(f,edge['a'],edge['b']))
        layer=m['layers'][0]['id'];delta=[.125,.25,.5]
        changed=take('edge-move','advanced',operation='translate',kind='edge',ids=[edge['id']],vector=delta)
        check_math(r,'edge-move',probe,model,out/'edge-math',m,changed['model'],[edge['a'],edge['b']],'translate',delta)
        base=take('undo-edge-move','undo')
        editing.no_op(r,'service',service,out,'zero-move',base,'advanced',operation='translate',kind='face',ids=[face['id']],vector=[0,0,0])
        editing.no_op(r,'service',service,out,'unit-scale',base,'advanced',operation='scale',kind='face',ids=[face['id']],vector=[1,1,1])
        editing.no_op(r,'service',service,out,'zero-extrude',base,'advanced',operation='extrude',kind='edge',ids=[edge['id']],vector=[0,0,0],layer_id=layer)
        common=dict(operation='translate',kind='point',ids=[edge['a']],vector=delta)
        invalid=[('duplicate',{'ids':[edge['a'],edge['a']]}),('missing',{'ids':[999999]}),('empty',{'ids':[]}),('wrong-kind',{'kind':'flow'}),('stale',{'expected_revision':999999}),('overflow',{'vector':[1e12,0,0]}),('zero-scale',{'operation':'scale','vector':[1,0,1]}),('negative-scale',{'operation':'scale','vector':[-1,1,1]}),('markers-with-subset',{'adjust_markers':True}),('angle-limit',{'operation':'rotate','vector':[360001,0,0]})]
        for name,fields in invalid:editing.reject(r,'service',service,out,name,base,'advanced',**{**common,**fields})
        for axis in (-1,3,4294967296):editing.reject(r,'service',service,out,'mirror-axis-'+str(axis),base,'advanced',operation='mirror',kind='face',ids=[face['id']],axis=axis,distance=0,weld=True)
        for name,ids,v in [('interior',[edge['id'],interior['id']],delta),('parallel',[edge['id']],[b-a for a,b in zip(pts[edge['a']],pts[edge['b']])]),('cross-center',[edge['id']],[0,-1000,1])]:
            editing.reject(r,'service',service,out,'extrude-'+name,base,'advanced',operation='extrude',kind='edge',ids=ids,vector=v,layer_id=layer)
        locked=take('lock','topology',operation='set_locked',ids=[edge['a']],locked=True)
        for op,kind,ids,extra in [('translate','point',[edge['a']],{'vector':delta}),('extrude','edge',[edge['id']],{'vector':delta,'layer_id':layer}),('mirror','face',[face['id']],{'axis':0,'distance':100,'weld':False})]:
            editing.reject(r,'service',service,out,'locked-'+op,locked,'advanced',operation=op,kind=kind,ids=ids,**extra)
        take('unlock','undo')
        # A boundary edge can retain either stored direction after face deletion.
        # Flip the adjacent face deliberately; the new side must still oppose its edge direction.
        fixture_state=fixture.run(probe,model,out/'flipped-fixture','advanced_core',{'operation':'flip','vector':[0,0,0],'face_index':m['control_faces'].index(face)})
        flipped=take('flipped','load',path=fixture_state['artifacts']['advanced.fbm'])
        extruded=take('flipped-extrusion','advanced',operation='extrude',kind='edge',ids=[edge['id']],vector=delta,layer_id=layer)
        check_extrusion(r,'flipped-extrusion',flipped['model'],extruded['model'],[edge['id']],delta,layer)
        base=take('reload-original','load',path=str(model))
        boundary=[e for e in m['control_edges'] if degree[edge_key(e['a'],e['b'])]==1]
        other=next(e for e in boundary if e['id']!=edge['id'] and {e['a'],e['b']}&{edge['a'],edge['b']})
        chain=take('chain-extrude','advanced',operation='extrude',kind='edge',ids=[edge['id'],other['id']],vector=delta,layer_id=layer)
        check_extrusion(r,'chain-extrusion',m,chain['model'],[edge['id'],other['id']],delta,layer)
        old=fixture.run(probe,model,out/'chain-original-core','advanced_core',{'operation':'extrude','vector':delta,'edge_indices':[m['control_edges'].index(e) for e in (edge,other)],'layer_index':0})
        r.case('chain.direct-original-coordinates',Counter(tuple(p['p']) for p in old['control_points'])==Counter(tuple(p['p']) for p in chain['model']['control_points']))
        base=take('undo-chain','undo')
        blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        for operation,kind,ids,extra in [('extrude','edge',[edge['id']],{'vector':delta,'layer_id':layer}),('mirror','face',[face['id']],{'axis':0,'distance':100,'weld':False})]:
            before=dict(service.state);response=service.command('advanced',output=blocked,operation=operation,kind=kind,ids=ids,**extra)
            r.case('publication.'+operation+'.rejected-and-history',not response['ok'] and service.state==before,error=response.get('error'))
            after=take('after-failed-'+operation);editing.same(r,'publication.'+operation,after,base)
        # Build a separate center-plane triangle with exact, independent geometry.
        x=m['bounds']['max'][0]+8;z=m['bounds']['max'][2]+4;known=set(pts);ids=[]
        for i,p in enumerate(([x,0,z],[x+2,0,z],[x,2,z])):
            current=take('triangle-point-'+str(i),'topology',operation='add_point',position=p);new=next(p['id'] for p in current['model']['control_points'] if p['id'] not in known);known.add(new);ids.append(new)
        triangle=take('triangle','topology',operation='add_face',ids=ids,layer_id=layer);f=next(f for f in triangle['model']['control_faces'] if f['points']==ids)
        coplanar=editing.no_op(r,'service',service,out,'coplanar-mirror',triangle,'advanced',operation='mirror',kind='face',ids=[f['id']],axis=2,distance=z,weld=False)
        welded=take('welded','advanced',operation='mirror',kind='face',ids=[f['id']],axis=1,distance=0,weld=True)
        p=editing.points(welded['model']);new=set(p)-known;new_faces=[v for v in welded['model']['control_faces'] if v['id'] not in {v['id'] for v in triangle['model']['control_faces']}]
        r.case('mirror.weld-shares-center-seam',len(new)==1 and p[next(iter(new))]==[x,-2,z] and len(new_faces)==1 and new_faces[0]['points']==[next(iter(new)),ids[1],ids[0]])
        seam=edge_key(ids[0],ids[1]);r.case('mirror.weld-manifold-seam',incidence(welded['model'])[seam]==2)
        editing.reject(r,'service',service,out,'mirror-duplicate',welded,'advanced',operation='mirror',kind='face',ids=[f['id']],axis=1,distance=0,weld=True)
        # One selected curve may contain a repeated endpoint. Its coordinates change once.
        curve_edges=[e['id'] for e in triangle['model']['control_edges'] if e['a'] in ids and e['b'] in ids]
        curve=take('curve','auxiliary',operation='create_control',ids=curve_edges);curve_id=curve['model']['control_curves'][-1]['id']
        moved=take('curve-moved','advanced',operation='translate',kind='curve',ids=[curve_id],vector=delta)
        check_math(r,'curve-transform',probe,model,out/'curve-math',curve['model'],moved['model'],ids,'translate',delta)
        saved=editing.saved_copy(service,out/'curve-save');auxiliary.inspect_saved(r,'curve-transform',probe,saved,moved['model'],out/'curve-inspect')


def whole_model_cases(r,runtime,probe,model,out):
    out.mkdir();seed=fixture.run(probe,model,out/'seed','advanced_seed');saved=Path(seed['artifacts']['advanced.fbm'])
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):return editing.take(r,'whole',service,out,name,method,**fields)
        original=take('original','load',path=str(saved));ids=list(editing.points(original['model']));delta=[.125,.25,.5]
        for kind,distance in [(1,2),(2,1),(3,1)]:take('section-'+str(kind),'sections',operation='add',kind=kind,positions=[distance])
        base=take('with-sections');moved=take('moved','advanced',operation='translate',kind='point',ids=ids,vector=delta,adjust_markers=True)
        check_math(r,'whole.translate',probe,saved,out/'move-math',base['model'],moved['model'],ids,'translate',delta)
        r.case('whole.sections-all-four-types',set(s['kind'] for s in base['model']['sections'])=={1,2,3,4})
        for i,(a,b) in enumerate(zip(base['model']['sections'],moved['model']['sections'])):
            expected=a['plane'][3]-sum(n*v for n,v in zip(a['plane'][:3],delta))
            r.case('whole.section-plane-'+str(i),b['plane'][:3]==a['plane'][:3] and math.isclose(b['plane'][3],expected,rel_tol=2e-7,abs_tol=1e-7))
        result=editing.saved_copy(service,out/'moved-save');observed=fixture.run(probe,result,out/'moved-inspect')
        expected=[[[f32(a+b) for a,b in zip(p,delta)] for p in marker['points']] for marker in seed['markers']]
        r.case('whole.translated-markers',expected==[m['points'] for m in observed['markers']])
        r.case('whole.translated-explicit-split',observed['settings']['stored_split_location']==f32(seed['settings']['stored_split_location']+delta[0]))
        undone=take('undo-move','undo');editing.same(r,'whole.undo-move',undone,base)
        factor=[1.25,1.5,2];scaled=take('scaled','advanced',operation='scale',kind='point',ids=ids,vector=factor,adjust_markers=True)
        result=editing.saved_copy(service,out/'scaled-save');actual=fixture.run(probe,result,out/'scaled-inspect')
        expected=fixture.run(probe,saved,out/'scale-old-core','advanced_core',{'operation':'scale','vector':factor,'adjust_markers':True})
        r.case('whole.scale-exact-old-attributed-geometry',observed_core_geometry(actual)==observed_core_geometry(expected))
        for key in ('settings','markers'):r.case('whole.scale-exact-old-'+key,actual[key]==expected[key])
        r.case('whole.scale-main-dimensions',[scaled['model']['settings'][k] for k in ('length','beam','draft')]==[f32(base['model']['settings'][k]*v) for k,v in zip(('length','beam','draft'),factor)])
        take('undo-scale','undo');rotated=take('rotate90','advanced',operation='rotate',kind='point',ids=ids,vector=[0,0,90],adjust_markers=False)
        # Independent analytic right-angle expectation under FreeShip's documented rotation convention.
        r.case('whole.rotate90-analytic',all(math.isclose(a,b,abs_tol=1e-6) for i,p in editing.points(base['model']).items() for a,b in zip(editing.points(rotated['model'])[i],[p[1],-p[0],p[2]])))
        result=editing.saved_copy(service,out/'rotated-save');observed=fixture.run(probe,result,out/'rotate-inspect')
        r.case('whole.unchecked-markers-stay-fixed',observed['markers']==seed['markers'])
        take('undo-rotate','undo')
        take('rotate-with-markers','advanced',operation='rotate',kind='point',ids=ids,vector=[0,0,90],adjust_markers=True)
        result=editing.saved_copy(service,out/'rotated-markers-save');observed=fixture.run(probe,result,out/'rotated-markers-inspect')
        expected=fixture.run(probe,saved,out/'marker-math','advanced_math',{'operation':'rotate','vector':[0,0,90],'points':[p for m in seed['markers'] for p in m['points']]})['points']
        r.case('whole.checked-markers-rotate-exactly',expected==[p for m in observed['markers'] for p in m['points']])
    constraints=fixture.run(probe,model,out/'constraints','seed')['artifacts']['constraints.fbm']
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'constraint-service',native_linesplan=True) as service:
        base=editing.take(r,'constraint',service,out,'constraint-base','load',path=constraints);m=base['model']
        targets=[p['id'] for p in m['control_points'] if not p['batch_movable'] and not p['locked']]
        r.case('constraints.fixture-has-dependent-points',bool(targets))
        for i in targets:editing.reject(r,'constraint',service,out,'point-'+str(i),base,'advanced',operation='translate',kind='point',ids=[i],vector=[1,0,0])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--reuse-probe',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve();models=[m.resolve(strict=True) for m in (a.model or q.DEFAULT_MODELS)]
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required')
        return q.verify_deployment(a,models,out,'--verify-advanced',PHASES,('scene','dxf','sections'))
    for root in (a.source,a.qt_source,q.PILOT):
        if root==out or root in out.parents:p.error('Use a private report directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-advanced'
    legacy=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe']}
    tests=('qt_advanced.py','QtEditingFixture.pas','qt_editing_fixture.py','qt_migration.py','qt_backgrounds.py','qt_auxiliary.py','run_tests.py')
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    try:
        bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);probe=backgrounds.prepare_oracle(a,out,r)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            child=q.run_child(r,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-advanced',private],folder/'production',platform='offscreen')
            r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
            if any(c.get('name')=='advanced_workflow_unavailable' for c in child.get('cases',[])) or child['status']=='error':
                r.case(f'{i}.dependent-artifacts',status='not_run',reason='Production workflow unavailable; see discovery/error');continue
            states=check_production(r,i,probe,private,folder,child);art=child['artifacts']
            with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'legacy-export-service',native_linesplan=False) as service:
                response=service.command('load',output=folder/'old-export',path=art['saved_file'],document_id=str(q.uuid.uuid4()))
                if response['ok']:
                    old=q.validate_snapshot(r,f'{i}.old',response,folder/'old-export')
                    r.case(f'{i}.dxf-exact-original-core',native.numeric_entities(old['paths']['dxf'])==native.numeric_entities(Path(art['reopened_dxf'])))
                    native.compare_scene(r,f'{i}.scene-original-core',old['paths']['scene'],Path(art['reopened_scene']))
                else:
                    diagnostic=folder/'legacy-diagnostic';observed=fixture.run(probe,Path(art['saved_file']),diagnostic,'legacy_exports')
                    trace=(diagnostic/'symbolized-stack.txt').read_text('utf8',errors='replace') if (diagnostic/'symbolized-stack.txt').exists() else ''
                    known=observed.get('scene_error')=='EInvalidOp: Invalid floating point operation' and any(s['fragments']==1 for s in observed['short_sampling']) and 'GETVALUES' in trace and 'FreeSpline.inc:722' in trace
                    r.case(f'{i}.old-drawing-failure-attributed',known,observation=observed,stack=str(diagnostic/'symbolized-stack.txt'),original_error=response.get('error'))
                    if not known:raise RuntimeError('Unattributed original scene failure; see retained diagnostic')
                    r.case(f'{i}.scene-original-core',status='not_run',reason='Original GetValues divides by fragments-1 with fragments=1. Full scene comparison unavailable; no substitute baseline accepted.',evidence=str(diagnostic/'state.json'))
                    r.case(f'{i}.legacy-short-sampling',status='review',reason='Original drawing defect surfaced after edits; Qt uses native scene generation. Original source is unchanged.')
                    r.case(f'{i}.dxf-exact-original-core',observed.get('dxf')=='pass' and native.numeric_entities(diagnostic/'reference.dxf')==native.numeric_entities(Path(art['reopened_dxf'])))
            q.run_child(r,f'{i}.lines-regression',runtime/'qt_lines_tests.exe',[art['reopened_scene']],folder/'lines-regression',platform='offscreen')
            if i==2:
                # Separate processes vary heap ordering. Save/undo in the real workflow
                # sorts native point arrays; extrusion must still initialize the new points.
                for repeat in range(3):
                    repeated=q.run_child(r,f'{i}.repeat-{repeat}',runtime/'FreeShipQt.exe',['--verify-advanced',private],folder/f'repeat-{repeat}',platform='offscreen')
                    if repeated['status']=='error' or any(c.get('name')=='advanced_workflow_unavailable' for c in repeated.get('cases',[])):
                        r.case(f'{i}.repeat-{repeat}.artifacts',status='not_run',reason='Production action sequence unavailable');continue
                    observed=repeated['artifacts']
                    for phase in ('extruded','mirrored','reopened'):
                        current=json.loads(Path(observed[phase+'_model']).read_text('utf8'))
                        r.case(f'{i}.repeat-{repeat}.{phase}.attributed-geometry',native.reopened_geometry(current)==native.reopened_geometry(states[phase]))
                        for kind in ('scene','dxf','sections'):r.compare(f'{i}.repeat-{repeat}.{phase}.{kind}',observed[phase+'_'+kind],art[phase+'_'+kind])
                    current=json.loads(Path(observed['extruded_model']).read_text('utf8'))
                    check_extrusion(r,f'{i}.repeat-{repeat}.extrusion',states['rotated'],current,[observed['extruded_edge']],[observed['extrude_input'].get(k,0) for k in ('x','y','z')],current['layers'][0]['id'])
            if i==1:
                service_cases(r,runtime,probe,private,folder/'service-cases')
                whole_model_cases(r,runtime,probe,private,folder/'whole-model')
    except Exception as e:r.case('suite.execution',status='error',reason=f'{type(e).__name__}: {e}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==legacy);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,h in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==h)
        r.case('desktop-acceptance',status='not_run',reason='Offscreen widgets/events only');editing.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')},ensure_ascii=False));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]

if __name__=='__main__':raise SystemExit(main())
