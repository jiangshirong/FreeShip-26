"""Model-editing completion: actual Qt actions, geometry, constraints and atomic history.

Private files/processes only. GUI observations are offscreen and do not claim
desktop acceptance. --service-executable is a bounded diagnostic entry retaining
its evidence, not a substitute for the complete runtime suite.
"""
from pathlib import Path
import argparse, json, math, shutil, uuid
import qt_migration as q
import qt_editing as e
import native_linesplan_app as native
import qt_backgrounds as backgrounds
import qt_modeling_extra as extra


def geometry(m):
    result={k:v for k,v in m.items() if k not in ('document_id','revision','diagnostics')}
    # Normal vectors are a set per control face; original-core pointer traversal
    # order changes on undo. Retain every exact vector, including multiplicity.
    if 'surface_normals' in result:result['surface_normals']=sorted(result['surface_normals'],key=lambda n:(n['face'],n['p'],n['n']))
    return result


def services(r,executable,source,out):
    out.mkdir(parents=True,exist_ok=True)
    with q.Service(executable,out/'process') as s:
        index=0
        def take(label,method='modeling',**fields):
            nonlocal index
            index+=1
            folder=out/f'{index:03}-{label}'
            if method=='load':fields.setdefault('document_id',str(uuid.uuid4()))
            return q.validate_snapshot(r,'modeling.'+folder.name,s.command(method,output=folder,**fields),folder)
        base=take('load','load',path=str(source))
        def operation(op,**fields):return take(op,operation=op,**fields)
        def invalid(label,current,**fields):
            previous=dict(s.state);response=s.command('modeling',output=out/(label+'-rejected'),**fields)
            r.case(label+'.rejected',response.get('ok') is False,error=response.get('error'))
            r.case(label+'.history-preserved',s.state==previous)
            after=take(label+'-after','snapshot');e.same(r,label+'.atomic',after,current)
        def roundtrip(name,before,after):
            undone=take('undo-'+name,'undo');r.case(name+'.undo-all-fields',geometry(undone['model'])==geometry(before['model']))
            e.same(r,name+'.undo-exports',undone,before)
            redone=take('redo-'+name,'redo');r.case(name+'.redo-all-fields',geometry(redone['model'])==geometry(after['model']))
            e.same(r,name+'.redo-exports',redone,after)
            return redone
        layer=base['model']['layers'][0]['id']
        # Three orthogonal planes must keep every origin component and span the
        # entered dimensions. Old Plus divides by point count and drops offsets.
        for axis in range(3):
            origin=[30+axis*10,5,4];before=base
            panel=operation('grid_panel',origin=origin,size_a=6,size_b=4,columns=4,rows=3,plane=axis,layer_id=layer)
            old=e.points(before['model']);new={p['id']:p['p'] for p in panel['model']['control_points'] if p['id'] not in old}
            expected=set()
            for i in range(4):
                for j in range(3):
                    p=list(origin);a,b=((0,1),(0,2),(1,2))[axis];p[a]+=2*i;p[b]+=2*j;expected.add(tuple(p))
            r.case(f'grid.{axis}.coordinates',set(map(tuple,new.values()))==expected and len(new)==12)
            r.case(f'grid.{axis}.topology',len(panel['model']['control_faces'])-len(before['model']['control_faces'])==6 and len(panel['model']['control_edges'])-len(before['model']['control_edges'])==17)
            roundtrip('grid-'+str(axis),before,panel);base=take('undo-grid','undo')
        invalid('invalid-grid',base,operation='grid_panel',origin=[0,0,0],size_a=1,size_b=1,columns=1,rows=3,plane=0,layer_id=layer)
        # Free construction points isolate constraint behavior from hull geometry.
        ids=[]
        for position in ([30,0,2],[31,2,2],[34,0,2],[38,0,2]):
            previous=e.points(base['model']);base=take('add-point','topology',operation='add_point',position=position)
            ids.append(next(iter(set(e.points(base['model']))-set(previous))))
        before=base
        aligned=operation('align',ids=ids[:3]);r.case('align.projection',e.points(aligned['model'])[ids[1]]==[31,0,2]);roundtrip('align',before,aligned);base=take('undo-align','undo')
        constrained=operation('linear_constraint',ids=ids[:3]);r.case('constraint.references',next(p for p in constrained['model']['control_points'] if p['id']==ids[1])['line_a']==ids[0])
        moved=take('move-endpoint','move_point',point_id=ids[2],position=[34,4,2]);r.case('constraint.follows-line',e.points(moved['model'])[ids[1]]==[30.5,0.5,2])
        roundtrip('constraint-propagation',constrained,moved)
        invalid('constraint-cycle',moved,operation='linear_constraint',ids=[ids[1],ids[0],ids[2]])
        cleared=operation('clear_constraints',ids=[ids[1]]);r.case('constraint.clear',next(p for p in cleared['model']['control_points'] if p['id']==ids[1])['line_a']==0)
        before=cleared;anchored=operation('anchor',ids=[ids[0],ids[3]],hard=True)
        old=e.points(anchored['model']);moved=take('move-anchor','move_point',point_id=ids[0],position=[31,0,2]);r.case('anchor.preserves-offset',e.points(moved['model'])[ids[3]]==[old[ids[3]][0]+1,*old[ids[3]][1:]])
        invalid('anchor-cycle',moved,operation='anchor',ids=[ids[3],ids[0]],hard=False)
        e.reject(r,'modeling',s,out,'hard-anchor-direct',moved,'move_point',point_id=ids[3],position=[40,0,2])
        cleared=operation('clear_constraints',ids=[ids[3]])
        invalid('unrepresentable-name',cleared,operation='point_properties',ids=[ids[0]],name='中文点',vertex_type=3)
        properties=operation('point_properties',ids=[ids[0]],name='Point A',vertex_type=3)
        r.case('point.name-type',any(p['id']==ids[0] and p['name']=='Point A' and p['vertex_type']==3 for p in properties['model']['control_points']))
        group=operation('create_group',ids=ids[:2],name='Deck group',locked=False);r.case('group.members',set(group['model']['point_groups'][-1]['points'])==set(ids[:2]))
        group_index=group['model']['point_groups'][-1]['index'];locked=operation('update_group',index=group_index,ids=ids[:2],name='Deck group',locked=True)
        r.case('group.lock-restricts',all(not p['batch_movable'] for p in locked['model']['control_points'] if p['id'] in ids[:2]))
        roundtrip('group-lock',group,locked)
        unlocked=operation('update_group',index=group_index,ids=ids[:2],name='Deck group',locked=False)
        deleted=operation('delete_group',index=group_index);r.case('group.dissolve-keeps-points',e.points(deleted['model'])==e.points(unlocked['model']) and not deleted['model']['point_groups'])
        roundtrip('group-dissolve',unlocked,deleted)
        connected=operation('connect_points',ids=ids[:3]);new_edges=[x for x in connected['model']['control_edges'] if x['a'] in ids and x['b'] in ids]
        r.case('connect.chain',len(new_edges)==2)
        split=operation('split_edges',ids=[x['id'] for x in new_edges]);new_points=set(e.points(split['model']))-set(e.points(connected['model']))
        r.case('split.batch-count',len(new_points)==2)
        collapsed=operation('collapse_points',ids=sorted(new_points));r.case('collapse.chain-coordinates',e.points(collapsed['model'])==e.points(connected['model']))
        roundtrip('collapse-points',split,collapsed)
        invalid('collapse-free-endpoint',collapsed,operation='collapse_points',ids=[ids[0]])
        marker=operation('add_markers',paths=[[[0,0,0],[1,1,1],[2,0,2]]]);r.case('markers.exact-source-points',marker['model']['markers'][-1]['spline']['points']==[[0,0,0],[1,1,1],[2,0,2]])
        deleted_markers=operation('delete_markers');roundtrip('markers-delete',marker,deleted_markers)
        # A clean original copy tests face splitting/merging and winding repair.
        base=take('reload','load',path=str(source));layer=base['model']['layers'][0]['id']
        face=next(f for f in base['model']['control_faces'] if len(f['points'])>=4);pair=[face['points'][0],face['points'][2]]
        cut=operation('connect_points',ids=pair);diagonal=next(x for x in cut['model']['control_edges'] if {x['a'],x['b']}==set(pair))
        r.case('connect.face-splits',len(cut['model']['control_faces'])==len(base['model']['control_faces'])+1)
        merged=operation('collapse_edges',ids=[diagonal['id']]);r.case('merge.face-count',len(merged['model']['control_faces'])==len(base['model']['control_faces']))
        roundtrip('merge-edge',cut,merged)
        flipped=operation('flip_faces',ids=[merged['model']['control_faces'][0]['id']]);r.case('flip.keeps-coordinates',e.points(flipped['model'])==e.points(merged['model']))
        repaired=operation('repair_normals');roundtrip('repair-normals',flipped,repaired)
        # Plane insertion changes control topology, without moving existing vertices.
        base=take('reload-plane','load',path=str(source));positions=list(e.points(base['model']).values());middle=(min(p[0] for p in positions)+max(p[0] for p in positions))/2
        plane=operation('insert_plane',axis=0,position=[middle,0,0],create_curve=True)
        original=e.points(base['model']);actual=e.points(plane['model']);new=[p for i,p in actual.items() if i not in original]
        r.case('plane.new-points-on-plane',bool(new) and all(p[0]==e.f32(middle) for p in new))
        r.case('plane.old-positions',all(actual[i]==p for i,p in original.items()));roundtrip('insert-plane',base,plane)
        base=take('reload-cylinder','load',path=str(source));n=8
        cylinder=operation('cylinder',origin=[30,0,0],end=[30,0,4],radius=2,segments=n,close_start=True,close_end=True,layer_id=layer)
        r.case('cylinder.closed-topology',len(cylinder['model']['control_points'])-len(base['model']['control_points'])==2*n and len(cylinder['model']['control_faces'])-len(base['model']['control_faces'])==n+2)
        roundtrip('cylinder',base,cylinder)
        saved=e.saved_copy(s,out/'save');reopened=take('reopen','load',path=str(saved))
        r.case('save-reopen.geometry',native.reopened_geometry(reopened['model'])==native.reopened_geometry(cylinder['model']))
        r.compare('save-reopen.dxf',reopened['paths']['dxf'],cylinder['paths']['dxf'])
        before=dict(s.state)
        # New document must leave old model available if construction is invalid.
        response=s.command('new',output=out/'bad-new',document_id=str(uuid.uuid4()),length=0,beam=4,draft=1,columns=7,rows=5,units=0)
        r.case('new.invalid-atomic',not response['ok'] and s.state==before)
        new=take('new-hull','new',document_id=str(uuid.uuid4()),length=20,beam=5,draft=2,columns=7,rows=5,units=0)
        r.case('new.template-counts',len(new['model']['control_points'])==35 and len(new['model']['control_faces'])==24 and s.state['modified'])
        blocked=out/'new-publish-failure';blocked.mkdir();(blocked/'model.json').mkdir();before=dict(s.state)
        response=s.command('new',output=blocked,document_id=str(uuid.uuid4()),length=24,beam=6,draft=2,columns=7,rows=5,units=0)
        r.case('new.publish-failure-preserves-document',not response['ok'] and s.state==before)
        e.same(r,'new.publish-failure-exact',take('new-after-publish-failure','snapshot'),new)
        before=dict(s.state);analysis=q.require_ok(s.command('inspect_modeling'))['analysis'];r.case('lackenby.measure-readonly',s.state==before and analysis['displacement']>0)
        e.no_op(r,'modeling',s,out,'lackenby-noop',new,'modeling',operation='lackenby',ids=[l['id'] for l in new['model']['layers']],displacement=analysis['displacement'],lcb=analysis['lcb'],iterations=30)
        invalid('lackenby-nonconvergence',new,operation='lackenby',ids=[l['id'] for l in new['model']['layers']],displacement=analysis['displacement']*1.2,lcb=analysis['lcb'],iterations=1)
        transformed=operation('lackenby',ids=[l['id'] for l in new['model']['layers']],displacement=analysis['displacement']*1.01,lcb=analysis['lcb'],iterations=30)
        actual=q.require_ok(s.command('inspect_modeling'))['analysis'];r.case('lackenby.target',abs(actual['displacement']/analysis['displacement']/1.01-1)<=1e-4 and abs(actual['lcb']-analysis['lcb'])<=max(.002,analysis['length']*5e-5))
        roundtrip('lackenby',new,transformed)
        original=take('undo-before-translated-lackenby','undo')
        take('translate-hull','advanced',operation='translate',kind='point',ids=list(e.points(original['model'])),vector=[100,0,0],adjust_markers=False)
        shifted=take('translated-midship','update_project',changes={'use_default_split_section_location':False,'split_section_location':analysis['midship']+100})
        shifted_analysis=q.require_ok(s.command('inspect_modeling'))['analysis']
        r.case('lackenby.translated-measure',abs(shifted_analysis['volume']/analysis['volume']-1)<1e-5 and abs(shifted_analysis['lcb']-analysis['lcb']-100)<2e-4)
        changed=operation('lackenby',ids=[l['id'] for l in shifted['model']['layers']],displacement=shifted_analysis['displacement']*1.01,lcb=shifted_analysis['lcb'],iterations=30)
        measured=q.require_ok(s.command('inspect_modeling'))['analysis']
        r.case('lackenby.translated-target',abs(measured['displacement']/shifted_analysis['displacement']/1.01-1)<=1e-4 and abs(measured['lcb']-shifted_analysis['lcb'])<=.002)
        r.case('lackenby.only-longitudinal-coordinates',all(e.points(changed['model'])[i][1:]==p[1:] for i,p in e.points(shifted['model']).items()))


def production_request(model,out):
    out.mkdir(parents=True,exist_ok=True)
    commands=[]
    def add(phase,operation,fields=None,select=None,**extra):
        command={'phase':phase,'operation':operation,**extra}
        if fields is not None:command['fields']=fields
        if select is not None:command['select']=select
        commands.append(command)
    def points(*positions):return {'kind':'point','positions':positions}
    add('new','new',{'length':20,'beam':5,'draft':2,'columns':7,'rows':5,'units':0})
    add('cancel_lackenby','lackenby',{},cancel=True)
    # The fixed 20 x 5 x 2 template is measured independently in the native
    # Lackenby suite. Exercise the real asynchronous measure -> dialog ->
    # accepted transform route, then restore the shared workflow baseline.
    add('lackenby_applied','lackenby',{'displacement':47.48781967163086*1.01,'lcb':9.551198959350586,'iterations':30})
    add('lackenby_undo','undo');add('lackenby_redo','redo');add('lackenby_restore','undo')
    grid={'origin_x':30,'origin_y':5,'origin_z':4,'size_a':6,'size_b':4,'columns':4,'rows':3,'plane':0,'layer_id':0}
    add('cancel_grid','grid_panel',grid,cancel=True);add('panel','grid_panel',grid)
    add('group','create_group',{'name':'Deck group','locked':False},points([30,5,4],[32,5,4]))
    add('group_edit','point_groups',{'name':'Deck B','locked':False},group_row=0,group_action=2,dialog_operation='update_group')
    add('group_select','point_groups',{},group_row=0,group_action=1)
    add('group_move','translate',{'x':1,'y':0,'z':0})
    add('point_properties','point_properties',{'name':'Deck point','vertex_type':3},points([34,5,4]))
    add('cancel_point_properties','point_properties',{'name':'Cancelled'},points([34,5,4]),cancel=True)
    add('constraint','linear_constraint',{},points([34,5,4],[34,7,4],[34,9,4]))
    add('clear_constraint','clear_constraints',select=points([34,7,4]))
    add('anchor','anchor',{'hard':False},points([34,5,4],[36,5,4]))
    add('clear_anchor','clear_constraints',select=points([36,5,4]))
    add('cancel_coincide','coincide',{},points([34,5,4],[36,5,4]),cancel=True)
    add('normals','show_normals');add('markers_hidden','show_markers')
    add('model_check','model_check')
    marker_file=out/'markers.txt';marker_file.write_text('FreeShip markers\n30 0 0\n31 1 2\n32 0 3\nEOF\n',encoding='utf8')
    add('markers_import','add_markers',file=str(marker_file));add('markers_delete','delete_markers',{});add('markers_undo','undo')
    add('group_dissolve','point_groups',{},group_row=0,group_action=3,dialog_operation='delete_group')
    add('undo_group_dissolve','undo');add('redo_group_dissolve','redo')
    add('cylinder','cylinder',{'origin_x':40,'origin_y':0,'origin_z':0,'end_x':40,'end_y':0,'end_z':4,'radius':2,'segments':8,'close_start':True,'close_end':True,'layer_id':0})
    add('cancel_plane','insert_plane',{'axis':0,'distance':10,'create_curve':True},cancel=True)
    add('plane_applied','insert_plane',{'axis':0,'distance':10,'create_curve':True});add('plane_restore','undo')
    for operation in ('intersect_layers','delete_negative','assemble_faces','auto_group_layers','remove_unused'):
        add('cancel_'+operation,operation,{},cancel=True)
    for operation in ('auto_group_layers','assemble_faces'):
        add(operation+'_applied',operation,{},select={'kind':'face','ids':[]})
        add(operation+'_undo','undo');add(operation+'_redo','redo');add(operation+'_restore','undo')
    add('faces_flip','flip_faces',select={'kind':'face','all':True});add('faces_flip_undo','undo')
    add('one_face_flip','flip_faces',select={'kind':'face','ids':[2]})
    add('normals_repaired','model_check',{},check_action='repair_normals',dialog_operation='repair_normals')
    add('normals_repair_undo','undo');add('normals_repair_redo','redo');add('normals_repair_restore','undo');add('one_face_restore','undo')
    add('select_all','select_all',select=points());add('invert_selection','invert_selection');add('select_leaks','select_leaks');add('clear_selection','clear_selection')
    add('saved','save');add('reopened','reopen')
    request=out/'request.json';request.write_text(json.dumps({'model':str(model),'commands':commands}),encoding='utf8')
    return request,commands


def production(r,runtime,model,out):
    request,commands=production_request(model,out)
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-modeling',request],out/'workflow',timeout=300,platform='offscreen')
    r.data['production']=child
    r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
    if child['status']=='error':raise RuntimeError('Production modeling workflow failed')
    if any(c['status']=='not_run' and c.get('name','').endswith('.entry') for c in child.get('cases',[])):
        r.case('production.dependent-checks',status='not_run',reason='Action discovery did not permit the complete workflow');return
    art=child['artifacts'];states={phase:json.loads(Path(art[phase+'_model']).read_text('utf8')) for phase in ['new']+[c['phase'] for c in commands]}
    r.case('production.new-dimensions',len(states['new']['control_points'])==35 and len(states['new']['control_faces'])==24)
    r.case('production.lackenby-applied',e.points(states['lackenby_applied'])!=e.points(states['new']) and all(e.points(states['lackenby_applied'])[i][1:]==p[1:] for i,p in e.points(states['new']).items()))
    r.case('production.lackenby-history',geometry(states['new'])==geometry(states['lackenby_undo'])==geometry(states['lackenby_restore']) and geometry(states['lackenby_applied'])==geometry(states['lackenby_redo']))
    r.case('production.plane-applied',len(states['plane_applied']['control_points'])>len(states['cylinder']['control_points']) and len(states['plane_applied']['control_curves'])>len(states['cylinder']['control_curves']))
    r.case('production.plane-restored',geometry(states['plane_restore'])==geometry(states['cylinder']))
    for operation in ('auto_group_layers','assemble_faces'):
        applied=states[operation+'_applied'];base=states['plane_restore']
        r.case('production.'+operation+'.coordinates',e.points(applied)==e.points(base))
        r.case('production.'+operation+'.changes-layers',applied['layers']!=base['layers'])
        r.case('production.'+operation+'.history',geometry(states[operation+'_undo'])==geometry(base)==geometry(states[operation+'_restore']) and geometry(applied)==geometry(states[operation+'_redo']))
    repaired=states['normals_repaired'];before=states['one_face_flip']
    boundaries={}
    for face in repaired['control_faces']:
        for a,b in zip(face['points'],face['points'][1:]+face['points'][:1]):boundaries.setdefault(tuple(sorted((a,b))),[]).append((a,b))
    r.case('production.normal-repair-consistent',all(len(v)!=2 or v[0]==tuple(reversed(v[1])) for v in boundaries.values()))
    r.case('production.normal-repair-changes-winding',repaired['control_faces']!=before['control_faces'] and e.points(repaired)==e.points(before))
    r.case('production.normal-repair-history',geometry(states['normals_repair_undo'])==geometry(before)==geometry(states['normals_repair_restore']) and geometry(repaired)==geometry(states['normals_repair_redo']) and geometry(states['one_face_restore'])==geometry(states['faces_flip_undo']))
    r.case('production.panel-count',len(states['panel']['control_points'])==47 and len(states['panel']['control_faces'])==30)
    for a,b in [('new','cancel_grid'),('point_properties','cancel_point_properties'),('clear_anchor','cancel_coincide'),('cylinder','cancel_plane')]:
        r.case('production.'+b+'.unchanged',geometry(states[a])==geometry(states[b]))
    r.case('production.group-name',states['group_edit']['point_groups'][0]['name']=='Deck B')
    group_ids=states['group_edit']['point_groups'][0]['points'];before=e.points(states['group_edit']);after=e.points(states['group_move'])
    r.case('production.group-selection-moves-together',all(after[i]==[p[0]+1,p[1],p[2]] if i in group_ids else after[i]==p for i,p in before.items()))
    r.case('production.properties',any(p['name']=='Deck point' and p['vertex_type']==3 for p in states['point_properties']['control_points']))
    r.case('production.constraint-created',sum(bool(p['line_a']) for p in states['constraint']['control_points'])==1)
    r.case('production.constraint-cleared',not any(p['line_a'] for p in states['clear_constraint']['control_points']))
    r.case('production.anchor-created',sum(bool(p['anchor']) for p in states['anchor']['control_points'])==1)
    r.case('production.anchor-cleared',not any(p['anchor'] for p in states['clear_anchor']['control_points']))
    r.case('production.check-is-readonly',geometry(states['model_check'])==geometry(states['markers_hidden']))
    r.case('production.groups-undo',states['undo_group_dissolve']['point_groups']==states['group_edit']['point_groups'])
    r.case('production.groups-redo',not states['redo_group_dissolve']['point_groups'])
    r.case('production.markers-import',states['markers_import']['markers'][-1]['spline']['points']==[[30,0,0],[31,1,2],[32,0,3]])
    r.case('production.markers-delete-undo',not states['markers_delete']['markers'] and states['markers_undo']['markers']==states['markers_import']['markers'])
    r.case('production.select-all',set(art['select_all_selection'])==set(e.points(states['select_all'])))
    r.case('production.invert-selection',not art['invert_selection_selection'])
    r.case('production.select-leaks',set(art['select_leaks_selection'])=={p['id'] for p in states['select_leaks']['control_points'] if p['leak']})
    r.case('production.clear-selection',not art['clear_selection_selection'])
    r.case('production.saved-geometry',native.reopened_geometry(states['saved'])==native.reopened_geometry(states['reopened']))
    r.case('production.saved-display',states['saved']['display']==states['reopened']['display'])
    r.compare('production.saved-dxf',Path(art['saved_dxf']),Path(art['reopened_dxf']))
    q.run_child(r,'core-renderer',runtime/'qt_migration_tests.exe',[Path(art['reopened_model'])],out/'core-renderer',platform='offscreen')
    r.case('production.visual-parity',status='review',reason='New dialogs and modeling overlays need comparison on the real desktop; screenshots retained')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--service-executable',type=Path);p.add_argument('--model',type=Path);p.add_argument('--reuse-probe',type=Path)
    p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve()
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required')
        reference=json.loads((a.workflow_reference/'production/request.json').read_text('utf8'))
        phases=tuple(c['phase'] for c in reference['commands'])
        return q.verify_deployment(a,[(a.model or q.DEFAULT_MODELS[0]).resolve()],out,'--verify-modeling',phases,('scene','dxf','sections'),
            workflow_prepare=lambda model,folder:production_request(model,folder/'request')[0])
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False)
    model=(a.model or q.DEFAULT_MODELS[0]).resolve();r=q.Report();r.data['suite']='qt-modeling';r.data['models']=[]
    protected={str(p):q.common.digest_file(p) for p in (model,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe') if p.exists()};legacy=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source)
    tests=('qt_modeling.py','qt_modeling_extra.py','qt_migration.py','qt_editing.py','native_linesplan_app.py','QtEditingFixture.pas','qt_editing_fixture.py','qt_backgrounds.py','qt/migration_tests.cpp','run_tests.py')
    r.data['source_sha256']=legacy;r.data['qt_sources_sha256']=sources;r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    try:
        if a.service_executable:executable=a.service_executable.resolve();r.case('runtime-integration',status='not_run',reason='Diagnostic service-only entry')
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);executable=runtime/'bridge/FreeShipGeometryBridge.exe'
        services(r,executable,model,out/'service')
        probe=None if a.service_executable else backgrounds.prepare_oracle(a,out,r)
        extra.run(r,executable,model,out/'extra',probe)
        if not a.service_executable:production(r,runtime,model,out/'production')
    except Exception as ex:r.case('suite',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==legacy);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,digest in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        r.case('desktop-acceptance',status='not_run',reason='Offscreen widgets/events only');e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]

if __name__=='__main__':raise SystemExit(main())
