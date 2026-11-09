"""Independent modeling edge cases, retained by the qt-modeling suite.

Uses private service documents and explicit geometric expectations; no desktop
automation. Serialization is also inspected by the original-core fixture.
"""
from pathlib import Path
import uuid
import math
import qt_migration as q
import qt_editing as e
import qt_editing_fixture as fixture
import native_linesplan_app as native


def groups(model):
    points=e.points(model)
    return sorted((g['name'],g['locked'],sorted(tuple(points[i]) for i in g['points'])) for g in model['point_groups'])


def attributed_faces(model):
    layers={l['id']:{k:v for k,v in l.items() if k not in ('id','name')} for l in model['layers']}
    return {f['id']:(f['points'],layers[f['layer']]) for f in model['control_faces']}


def run(r,executable,source,out,probe=None):
    out.mkdir(parents=True,exist_ok=True)
    with q.Service(executable,out/'process') as s:
        index=0
        current=None
        def take(label,method='modeling',**fields):
            nonlocal index,current
            index+=1;folder=out/f'{index:03}-{label}'
            if method=='load':fields.setdefault('document_id',str(uuid.uuid4()))
            current=q.validate_snapshot(r,'extra.'+folder.name,s.command(method,output=folder,**fields),folder)
            return current
        def op(operation,**fields):return take(operation,operation=operation,**fields)
        def reload():return take('reload','load',path=str(source))
        def reject(label,**fields):e.reject(r,'extra',s,out,label,current,'modeling',**fields)
        def undo(label,before,after):
            undone=take('undo-'+label,'undo');e.same(r,label+'.undo',undone,before)
            r.case(label+'.undo-groups',groups(undone['model'])==groups(before['model']))
            redone=take('redo-'+label,'redo');e.same(r,label+'.redo',redone,after)
            r.case(label+'.redo-groups',groups(redone['model'])==groups(after['model']))
        def add_points(positions):
            ids=[]
            for p in positions:
                before=set(e.points(current['model']));after=take('point','topology',operation='add_point',position=p)
                ids.append(next(iter(set(e.points(after['model']))-before)))
            return ids
        def panel(origin,layer,size=(4,4),plane=0,counts=(3,3)):
            return op('grid_panel',origin=origin,size_a=size[0],size_b=size[1],columns=counts[0],rows=counts[1],plane=plane,layer_id=layer)

        base=reload();layer=base['model']['layers'][0]['id']
        reject('wrapped-columns',operation='grid_panel',origin=[0,0,0],size_a=1,size_b=1,columns=2**32+3,rows=3,plane=0,layer_id=layer)
        ids=add_points([[30,0,0],[31,1,0],[32,0,0],[34,0,0]])
        before=current;extruded=op('extrude_points',ids=ids[:2],position=[1,2,3])
        added=set(e.points(extruded['model']))-set(e.points(before['model']))
        r.case('extra.point-extrusion-offsets',{tuple(e.points(extruded['model'])[i]) for i in added}=={(31,2,3),(32,3,3)})
        r.case('extra.point-extrusion-no-implicit-edges',extruded['model']['control_edges']==before['model']['control_edges'])
        undo('point-extrusion',before,extruded);take('undo-point-extrusion','undo')
        before=current;coincident=op('coincide',ids=ids[:2]);r.case('extra.coincide-retains-entities',len(coincident['model']['control_points'])==len(before['model']['control_points']) and e.points(coincident['model'])[ids[1]]==[30,0,0])
        undo('coincide',before,coincident);take('undo-coincide','undo')
        # Reference IDs may change on FBM load. Compare their actual target points.
        op('linear_constraint',ids=ids[:3]);op('anchor',ids=[ids[2],ids[3]],hard=True)
        take('lock','topology',operation='set_locked',ids=ids[:2],locked=True)
        group=op('create_group',ids=ids[:2],name='Persistent group',locked=False)
        r.case('extra.group-create-preserves-existing-locks',group['model']['point_groups'][0]['locked'])
        reject('wrapped-group-index',operation='delete_group',index=2**32)
        saved=e.saved_copy(s,out/'constraints-save');reopened=take('constraints-reopen','load',path=str(saved))
        r.case('extra.constraint-save-references',native.reopened_geometry(reopened['model'])==native.reopened_geometry(group['model']))
        r.case('extra.group-save-membership',groups(reopened['model'])==groups(group['model']))
        if probe:
            observed=fixture.run(probe,saved,out/'original-core-constraints')
            actual_points=e.points(observed);expected_points=e.points(reopened['model'])
            def signature(m,point_map,legacy=False):
                return sorted((tuple(p['p']),p['name'],p['vertex_type'],p['locked'],p['hard_anchor'],
                    tuple(point_map[p['anchor']]) if p['anchor'] else None,
                    tuple(point_map[p['linear_a' if legacy else 'line_a']]) if p['linear_a' if legacy else 'line_a'] else None,
                    tuple(point_map[p['linear_b' if legacy else 'line_b']]) if p['linear_b' if legacy else 'line_b'] else None) for p in m['control_points'])
            r.case('extra.original-core-constraints',signature(observed,actual_points,True)==signature(reopened['model'],expected_points))
            r.case('extra.original-core-groups',groups(observed)==groups(reopened['model']))
        unlocked=op('unlock_all');r.case('extra.unlock-all-preserves-constraints',not any(p['locked'] for p in unlocked['model']['control_points']) and sum(bool(p['anchor']) for p in unlocked['model']['control_points'])==1)
        undo('unlock-all',reopened,unlocked)
        # New IDs must not resurrect IDs freed inside previous transactions.
        reload();ids=add_points([[30,0,0],[31,1,0],[32,0,0]])
        connected=op('connect_points',ids=ids);edge_ids=[x['id'] for x in connected['model']['control_edges'] if x['a'] in ids and x['b'] in ids]
        curve=take('curve','auxiliary',operation='create_control',ids=edge_ids)
        collapsed=op('collapse_points',ids=[ids[1]])
        controls=[c['controls'] for c in collapsed['model']['control_curves']]
        r.case('extra.collapse-curve-survives',len(controls)==1 and set(controls[0])=={ids[0],ids[2]} and len(controls[0])==2)
        undo('collapse-curve',curve,collapsed)
        new_id=add_points([[31,4,0]])[0];r.case('extra.deleted-point-id-not-reused',new_id>max(ids) and new_id!=ids[1])
        before=current;cleaned=op('remove_unused');r.case('extra.remove-unused-clears-free-curves',not cleaned['model']['control_curves'] and len(cleaned['model']['control_points'])==len(base['model']['control_points']))
        undo('remove-unused',before,cleaned)
        # Concave L: the chord across its missing upper-right square is outside.
        reload();ids=add_points([[30,0,0],[33,0,0],[33,1,0],[31,1,0],[31,3,0],[30,3,0]])
        take('concave-face','topology',operation='add_face',ids=ids,layer_id=layer)
        reject('outside-diagonal',operation='connect_points',ids=[ids[2],ids[4]])
        before=current;inside=op('connect_points',ids=[ids[0],ids[3]])
        r.case('extra.concave-inside-diagonal',len(inside['model']['control_faces'])==len(before['model']['control_faces'])+1)
        undo('concave-diagonal',before,inside)
        # Group/assemble disjoint panels while preserving every face's material.
        reload();take('nondefault-material','update_layer',layer_id=layer,changes={'thickness':0.125,'material_density':7.8,'use_in_hydrostatics':False,'show_in_linesplan':False,'use_for_intersections':False,'symmetric':False,'developable':True})
        panel([30,5,4],layer);panel([40,5,4],layer)
        before=current;grouped=op('auto_group_layers')
        r.case('extra.auto-group-components',len(grouped['model']['layers'])>=3)
        r.case('extra.auto-group-face-material',attributed_faces(grouped['model'])==attributed_faces(before['model']))
        undo('auto-group',before,grouped)
        before=current;assembled=op('assemble_faces')
        r.case('extra.assemble-face-material',attributed_faces(assembled['model'])==attributed_faces(before['model']))
        r.case('extra.assemble-points',e.points(assembled['model'])==e.points(before['model']))
        undo('assemble',before,assembled)
        # Two perpendicular flat sheets have the analytic intersection x=32,z=4.
        reload();a=panel([30,0,4],layer,counts=(2,2));old_layers={l['id'] for l in a['model']['layers']}
        made=take('second-layer','manage_layers',operation='create',changes={'name':'Cutter','symmetric':False,'use_in_hydrostatics':False})
        cutter=next(l['id'] for l in made['model']['layers'] if l['id'] not in old_layers)
        panel([32,-1,3],cutter,size=(6,2),plane=2,counts=(2,2));before=current
        intersected=op('intersect_layers',layer_id=layer,other_layer_id=cutter)
        old=e.points(before['model']);new=e.points(intersected['model']);added=[p for i,p in new.items() if i not in old]
        r.case('extra.layers-intersection-analytic',sorted(added)==[[32,0,4],[32,4,4]])
        r.case('extra.layers-intersection-old-points',all(new[i]==p for i,p in old.items()))
        undo('layer-intersection',before,intersected)
        # Negative-side deletion is not clipping: retain faces crossing Y=0.
        reload();panel([30,-4,0],layer,size=(2,2),counts=(2,2));panel([35,-1,0],layer,size=(2,2),counts=(2,2));before=current
        coords=e.points(before['model']);expected={f['id'] for f in before['model']['control_faces'] if any(coords[i][1]>1e-7 for i in f['points'])}
        deleted=op('delete_negative');r.case('extra.delete-negative-exact-faces',{f['id'] for f in deleted['model']['control_faces']}==expected)
        undo('delete-negative',before,deleted)
        # Late publication failure must restore geometry AND the available redo.
        take('undo-before-publish-failure','undo');before=current;history=dict(s.state)
        blocked=out/'publish-failure';blocked.mkdir();(blocked/'model.json').mkdir()
        result=s.command('modeling',output=blocked,operation='flip_faces',ids=[before['model']['control_faces'][0]['id']])
        r.case('extra.late-publish-rejected',not result['ok']);r.case('extra.late-publish-history',s.state==history)
        unchanged=take('publish-rollback','snapshot');e.same(r,'extra.late-publish-geometry',unchanged,before)
        redone=take('publish-rollback-redo','redo');e.same(r,'extra.late-publish-redo',redone,deleted)
        # Pivot math: rotate a point around itself; every coordinate is invariant.
        reload();ids=add_points([[30,2,3]])
        before=current;rotated=take('pivot','advanced',operation='rotate',kind='point',ids=ids,vector=[23,37,51],pivot=[30,2,3],adjust_markers=False)
        r.case('extra.rotation-pivot-invariant',e.points(rotated['model'])==e.points(before['model']))
        # About (20, 2, 3), a clockwise 90-degree Z rotation sends (30, 2, 3)
        # to (20, -8, 3). Negative Y must survive the pivot translation.
        rotated=take('pivot-cross-center','advanced',operation='rotate',kind='point',ids=ids,vector=[0,0,90],pivot=[20,2,3],adjust_markers=False)
        r.case('extra.rotation-pivot-signed-coordinates',e.points(rotated['model'])[ids[0]]==[20,-8,3])
        # Independent rectangular hull integral (box is 20 x 6, draft 4).
        box=reload();analysis=q.require_ok(s.command('inspect_modeling'))['analysis']
        # 82 original Single section samples plus Single weighted accumulation.
        # Keep that arithmetic; bound relative rounding, do not demand Double ULPs.
        r.case('extra.box-volume-integral',abs(analysis['volume']-20*6*4)<=128*2**-23*(20*6*4),observed=analysis['volume'],analytic=480,relative_error=abs(analysis['volume']/480-1),single_rounding_bound=128*2**-23)
        r.case('extra.box-lcb-integral',abs(analysis['lcb']-10)<1e-5)
        r.case('extra.box-prismatic-coefficient',abs(analysis['cp']-1)<1e-5)
        normals=take('normals','display',changes={'show_normals':True})
        r.case('extra.normals-source-present',len(normals['model']['surface_normals'])>0)
        r.case('extra.normals-unit-length',all(abs(sum(x*x for x in n['n'])-1)<1e-5 for n in normals['model']['surface_normals']))
        r.compare('extra.normals-dxf-unchanged',normals['paths']['dxf'],box['paths']['dxf'])
        if not probe:r.case('extra.original-core-inspection',status='not_run',reason='Diagnostic run has no independent probe')
