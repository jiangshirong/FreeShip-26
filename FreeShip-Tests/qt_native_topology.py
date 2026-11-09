"""C++ control-net editing against the original implementation and known geometry.

Included by qt-document. --runtime also runs just this direction in a private
output folder. No desktop automation, original files or user settings are used.
"""
import argparse
import json
import uuid
from collections import Counter
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as nla

OPERATIONS={'split_edges','connect_points','flip_faces','remove_unused','delete_negative','collapse_points'}


def run(r,runtime,source,out):
    out.mkdir(parents=True)
    timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0
        current=None

        def both(label,method='snapshot',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                response=q.require_ok(s.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                model=json.loads(Path(response['model']).read_text('utf8'));results.append((response,model))
            (a,am),(b,bm)=results
            ag,bg=modeling.geometry(am),modeling.geometry(bm)
            r.case('net.'+label+'.complete-model',ag==bg,different_fields=[k for k in ag if ag[k]!=bg.get(k)])
            r.case('net.'+label+'.state',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare('net.'+label+'.'+key,a[key],b[key])
            if method=='topology' or method=='modeling' and fields['operation'] in OPERATIONS:
                r.case('net.'+label+'.native-dispatch',a.get('edit_engine')=='cpp-topology' and a.get('fbm_write_engine')=='cpp-control-net-writer')
                timings.append({'operation':fields['operation'],'edit_ms':a['topology_edit_ms'],'total_ms':a['document_total_ms']})
            current=am
            return am

        def reload():return both('load','load',path=str(source),document_id=str(uuid.uuid4()))
        def op(operation,**fields):return both(operation,'modeling' if operation in OPERATIONS else 'topology',operation=operation,**fields)
        def add(positions):
            ids=[]
            for p in positions:
                old=set(e.points(current));after=op('add_point',position=p);ids.append(next(iter(set(e.points(after))-old)))
            return ids
        def reject(label,method='topology',**fields):
            for tag,s in [('native',native),('oracle',oracle)]:
                before=dict(s.state);result=s.command(method,output=out/f'reject-{label}-{tag}',**fields)
                r.case(f'net.reject-{label}.{tag}',not result['ok'] and s.state==before,error=result.get('error'))
        def roundtrip(label,before,after):
            undone=both(label+'-undo','undo');r.case('net.'+label+'.undo',modeling.geometry(before)==modeling.geometry(undone))
            redone=both(label+'-redo','redo');r.case('net.'+label+'.redo',modeling.geometry(after)==modeling.geometry(redone))

        base=reload();layer=base['layers'][0]['id']
        points={p['id']:p for p in base['control_points']}
        edges=[v for v in base['control_edges'] if all(points[i]['batch_movable'] for i in (v['a'],v['b']))]
        split=op('split_edge',edge_id=edges[0]['id'])
        new_id=next(iter(set(e.points(split))-set(points)))
        r.case('net.split.exact-midpoint',e.points(split)[new_id]==[e.f32((a+b)/2) for a,b in zip(points[edges[0]['a']]['p'],points[edges[0]['b']]['p'])])
        roundtrip('split',base,split);both('branch-undo','undo')
        again=op('split_edge',edge_id=edges[0]['id']);other=next(iter(set(e.points(again))-set(points)))
        r.case('net.branch-never-reuses-id',other>new_id and not native.state['can_redo'])
        both('before-batch','undo');split=op('split_edges',ids=[v['id'] for v in edges[:3]])
        r.case('net.batch-independent-count',len(split['control_points'])==len(points)+min(3,len(edges)))
        base=reload()
        incidence=Counter(tuple(sorted((f['points'][i],f['points'][(i+1)%len(f['points'])]))) for f in base['control_faces'] for i in range(len(f['points'])))
        interior=next((v for v in edges if incidence[tuple(sorted((v['a'],v['b'])))]==2),None)
        if interior:
            after=op('set_crease',ids=[interior['id']],crease=not interior['crease']);roundtrip('crease',base,after)
        base=reload();ids=add([[30,1,2],[34,1,2],[34,5,2],[30,5,2]])
        before=current;face_model=op('add_face',ids=ids,layer_id=layer)
        face=next(f for f in face_model['control_faces'] if f['points']==ids)
        r.case('net.face.exact-boundary',len(face_model['control_edges'])==len(before['control_edges'])+4)
        reject('duplicate-face',operation='add_face',ids=ids,layer_id=layer)
        reject('duplicate-selection',operation='delete',kind='point',ids=[ids[0],ids[0]])
        reject('late-invalid',operation='delete',kind='point',ids=[ids[0],1999999])
        locked=op('set_locked',ids=[ids[0]],locked=True)
        reject('locked-neighbor',operation='delete',kind='face',ids=[face['id']])
        op('set_locked',ids=[ids[0]],locked=False)
        state=dict(native.state);op('assign_layer',ids=[face['id']],layer_id=layer)
        r.case('net.noop-preserves-state',native.state==state)
        before=current;after=op('flip_faces',ids=[face['id']]);r.case('net.flip.exact-winding',next(f['points'] for f in after['control_faces'] if f['id']==face['id'])==list(reversed(ids)))
        roundtrip('flip',before,after);both('unflip','undo')
        before=current;after=op('connect_points',ids=[ids[0],ids[2]])
        r.case('net.diagonal.two-triangles',len(after['control_faces'])==len(before['control_faces'])+1 and sum(set(f['points']).issubset(ids) and len(f['points'])==3 for f in after['control_faces'])==2)
        roundtrip('diagonal',before,after);both('undo-diagonal','undo')
        before=current;edge=next(v['id'] for v in current['control_edges'] if v['a'] in ids and v['b'] in ids)
        for kind,selection in [('edge',[edge]),('point',[ids[0]]),('face',[face['id']])]:
            after=op('delete',kind=kind,ids=selection)
            # Current Plus removes the face first, leaving its other wire edges.
            # Preserve this observed source behavior; orphan cleanup is explicit.
            removed={'edge':1,'point':2,'face':0}[kind]
            r.case('net.delete-'+kind+'.cascade',len(after['control_faces'])==len(before['control_faces'])-1 and len(after['control_edges'])==len(before['control_edges'])-removed)
            roundtrip('delete-'+kind,before,after);both('restore-'+kind,'undo')
        # Free chains exercise curve splitting, names, groups and index remapping.
        reload();ids=add([[30,1,2],[31,1,2],[32,1,2],[33,1,2],[34,1,2]])
        connected=op('connect_points',ids=ids);chain=[v['id'] for v in connected['control_edges'] if v['a'] in ids and v['b'] in ids]
        before=both('create-curve','auxiliary',operation='create_control',ids=chain)
        before=both('create-group','modeling',operation='create_group',ids=ids,name='Native net group',locked=False)
        after=op('split_edge',edge_id=chain[1])
        midpoint=next(iter(set(e.points(after))-set(e.points(before))))
        collapsed=op('collapse_points',ids=[midpoint]);roundtrip('collapse-new-midpoint',after,collapsed)
        both('undo-midpoint-collapse','undo');roundtrip('curve-split',before,after);both('undo-curve-split','undo')
        after=op('delete',kind='edge',ids=[chain[1]])
        r.case('net.curve-delete.two-connected-parts',sorted(len(c['controls']) for c in after['control_curves'])==[2,3])
        roundtrip('curve-delete',before,after);both('restore-curve','undo')
        after=op('collapse_points',ids=[ids[2]])
        r.case('net.collapse.exact-curve',after['control_curves'][0]['controls']==[ids[0],ids[1],ids[3],ids[4]])
        roundtrip('collapse',before,after);both('restore-before-clean','undo')
        after=op('remove_unused');r.case('net.cleanup.curves-and-empty-group',not after['control_curves'] and len(after['point_groups'])==1 and not after['point_groups'][0]['points'])
        roundtrip('cleanup',before,after)
        # Invalid concave chord, nonmanifold boundary, and whole-request rollback.
        reload();ids=add([[40,1,0],[44,1,0],[44,2,0],[41,2,0],[41,5,0],[40,5,0]])
        op('add_face',ids=ids,layer_id=layer)
        reject('outside-chord','modeling',operation='connect_points',ids=[ids[2],ids[4]])
        ids=add([[50,-2,0],[51,-2,0],[50,-1,0]])
        before=op('add_face',ids=ids,layer_id=layer);after=op('delete_negative')
        r.case('net.negative.explicit-removal',all(i not in e.points(after) for i in ids))
        roundtrip('negative',before,after)
        saved=q.require_ok(native.command('save',output=out/'saved'))
        reopened=both('native-fbm-reopen','load',path=saved['saved_file'],document_id=str(uuid.uuid4()))
        r.case('net.save-reopen-full-geometry',nla.reopened_geometry(reopened)==nla.reopened_geometry(after))
        for method,fields in [('topology',{'operation':'add_point','position':[0,0,0]}),*[('modeling',{'operation':op,'ids':[1]}) for op in sorted(OPERATIONS)]]:
            result=oracle.command(method,output=out/('forbidden-'+fields['operation']),external_document=True,**fields)
            r.case('net.forbid-old-'+fields['operation'],not result['ok'] and 'C++' in result.get('error',''))
    r.data['native_topology_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args()
    r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('net.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
