"""Native extrusion, reflection, face merge and constraint-definition regression.

Retained by qt-document. Complete original-service oracle comparisons plus
independent geometry/atomicity expectations; no desktop automation.
"""
import argparse
import json
import uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import qt_advanced as advanced
import native_linesplan_app as nla

OPS={'anchor','linear_constraint','clear_constraints','align','coincide','unlock_all','collapse_edges','extrude_points'}

def run(r,runtime,source,out):
    out.mkdir(parents=True)
    timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='modeling',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                res=q.require_ok(s.command(method,output=out/f'{step:03}-{label}'/tag,**fields));m=json.loads(Path(res['model']).read_text('utf8'));results.append((res,m))
            (a,am),(b,bm)=results;prefix=f'advanced-native.{step:03}-{label}'
            ag,bg=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ag==bg,different_fields=[k for k in ag if ag[k]!=bg.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='advanced' or method=='modeling' and fields['operation'] in OPS:
                r.case(prefix+'.native-dispatch',a.get('edit_engine')=='cpp-topology' and a.get('fbm_write_engine')=='cpp-control-net-writer')
                timings.append({'operation':fields['operation'],'edit_ms':a['topology_edit_ms'],'total_ms':a['document_total_ms']})
            current=am;return am
        def reload():return both('load','load',path=str(source),document_id=str(uuid.uuid4()))
        def op(operation,**fields):return both(operation,operation=operation,**fields)
        def change(operation,**fields):return both(operation,'advanced',operation=operation,**fields)
        def add(positions):
            ids=[]
            for p in positions:
                old=set(e.points(current));after=both('point','topology',operation='add_point',position=p);ids.append(next(iter(set(e.points(after))-old)))
            return ids
        def reject(label,method='modeling',**fields):
            for tag,s in [('native',native),('oracle',oracle)]:
                old=dict(s.state);res=s.command(method,output=out/f'reject-{label}-{tag}',**fields)
                r.case('advanced-native.reject-'+label+'.'+tag,not res['ok'] and s.state==old,error=res.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('advanced-native.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('advanced-native.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))

        base=reload();layer=base['layers'][0]['id'];counts=advanced.incidence(base)
        boundary=[v for v in base['control_edges'] if counts[advanced.edge_key(v['a'],v['b'])]==1]
        edge=boundary[0];other=next(v for v in boundary[1:] if {v['a'],v['b']}&{edge['a'],edge['b']})
        delta=[.125,.25,.5]
        after=change('extrude',kind='edge',ids=[edge['id'],other['id']],vector=delta,layer_id=layer)
        advanced.check_extrusion(r,'advanced-native.extrude',base,after,[edge['id'],other['id']],delta,layer)
        history('extrude',base,after)
        before=current;state=dict(native.state);change('extrude',kind='edge',ids=[edge['id']],vector=[0,0,0],layer_id=layer)
        r.case('advanced-native.noop-history',native.state==state)
        reject('interior-extrude','advanced',operation='extrude',kind='edge',ids=[edge['id']],vector=delta,layer_id=layer)
        reload();reject('center-crossing','advanced',operation='extrude',kind='edge',ids=[edge['id']],vector=[0,-100,1],layer_id=layer)
        reload();ids=add([[30,0,4],[32,0,4],[30,2,4]])
        triangle=both('triangle','topology',operation='add_face',ids=ids,layer_id=layer);face=next(f for f in triangle['control_faces'] if f['points']==ids)
        state=dict(native.state);change('mirror',kind='face',ids=[face['id']],axis=2,distance=4,weld=False);r.case('advanced-native.coplanar-noop',native.state==state)
        after=change('mirror',kind='face',ids=[face['id']],axis=1,distance=0,weld=True)
        new=set(e.points(after))-set(e.points(triangle));r.case('advanced-native.mirror-exact-seam',len(new)==1 and e.points(after)[next(iter(new))]==[30,-2,4])
        history('mirror',triangle,after)
        reject('duplicate-mirror','advanced',operation='mirror',kind='face',ids=[face['id']],axis=1,distance=0,weld=True)
        both('unmirror','undo');before=current;after=change('mirror',kind='face',ids=[face['id']],axis=0,distance=40.125,weld=False)
        r.case('advanced-native.mirror-offset',set(map(tuple,e.points(after).values()))-set(map(tuple,e.points(before).values()))=={(50.25,0,4),(48.25,0,4),(50.25,2,4)})
        # Two independent quads merge along one common edge. The old face winding
        # and boundary-edge types must survive save and the full undo sequence.
        reload();ids=add([[40,1,0],[42,1,0],[44,1,0],[40,3,0],[42,3,0],[44,3,0]])
        both('quad-a','topology',operation='add_face',ids=[ids[0],ids[1],ids[4],ids[3]],layer_id=layer)
        before=both('quad-b','topology',operation='add_face',ids=[ids[1],ids[2],ids[5],ids[4]],layer_id=layer)
        shared=next(v['id'] for v in before['control_edges'] if set((v['a'],v['b']))=={ids[1],ids[4]})
        after=op('collapse_edges',ids=[shared]);r.case('advanced-native.merge-counts',len(after['control_edges'])==len(before['control_edges'])-1 and len(after['control_faces'])==len(before['control_faces'])-1)
        history('merge',before,after);both('before-merge-reject','undo')
        reject('merge-late-invalid',operation='collapse_edges',ids=[shared,1999999]);both('merge-after-rejection','snapshot')
        base=reload();counts=advanced.incidence(base)
        shared=next(v['id'] for v in base['control_edges'] if counts[advanced.edge_key(v['a'],v['b'])]==2)
        merged=op('collapse_edges',ids=[shared]);history('merge-connected-hull',base,merged)
        # Constraints deliberately use reverse target order and a diamond graph.
        # Moving the root updates both line endpoints; the intermediate ordering
        # can affect the constrained point and must match the original setter.
        reload();ids=add([[30,2,3],[31,1,2],[35,4,6],[32,5,7],[40,2,1]])
        before=current;anchored=op('anchor',ids=[ids[0],ids[2],ids[1]],hard=False)
        op('linear_constraint',ids=[ids[1],ids[3],ids[2]])
        moved=both('diamond-root','move_point',point_id=ids[0],position=[31.125,3.5,4.25])
        r.case('advanced-native.anchor-offsets',all(e.points(moved)[i]==[e.f32(p+d) for p,d in zip(e.points(before)[i],[1.125,1.5,1.25])] for i in ids[1:3]))
        # A remaining adapter operation must checkpoint the live reverse order.
        both('constraint-group','modeling',operation='create_group',ids=[ids[1],ids[2]],name='Dependency order',locked=False)
        from qt_document import stop_owned_adapter
        status=q.require_ok(native.command('document_status'))
        if status['adapter_pid']:stop_owned_adapter(int(status['adapter_pid']),runtime/'bridge/FreeShipGeometryBridge.exe')
        both('constraint-recovery','snapshot')
        both('diamond-after-recovery','move_point',point_id=ids[0],position=[31.625,4.125,4.5])
        reject('cycle',operation='anchor',ids=[ids[3],ids[0]],hard=False)
        reject('anchor-line-conflict',operation='anchor',ids=[ids[0],ids[3]],hard=False)
        before=current;cleared=op('clear_constraints',ids=ids[1:4]);history('clear-constraints',before,cleared)
        before=current;aligned=op('align',ids=[ids[0],ids[3],ids[4]]);history('align',before,aligned)
        before=current;coincident=op('coincide',ids=[ids[0],ids[3]]);r.case('advanced-native.coincide-exact',e.points(coincident)[ids[3]]==e.points(coincident)[ids[0]]);history('coincide',before,coincident)
        locked=both('lock','topology',operation='set_locked',ids=[ids[1]],locked=True)
        reject('locked-clear',operation='clear_constraints',ids=[ids[1]])
        unlocked=op('unlock_all');r.case('advanced-native.unlock-all',not any(p['locked'] for p in unlocked['control_points']));history('unlock',locked,unlocked)
        before=current;extruded=op('extrude_points',ids=[ids[0],ids[4]],position=[1,2,3]);new=set(e.points(extruded))-set(e.points(before))
        r.case('advanced-native.extrude-points',set(tuple(e.points(extruded)[i]) for i in new)==set(tuple(e.f32(p+d) for p,d in zip(e.points(before)[i],[1,2,3])) for i in (ids[0],ids[4])))
        history('extrude-points',before,extruded)
        # Coincide was independently checked above. Separate these free points
        # before reopening so the existing exact-ID remapper is unambiguous.
        extruded=both('separate-coincident','move_point',point_id=ids[3],position=[37.25,6.5,8.75])
        saved=q.require_ok(native.command('save',output=out/'saved'))
        reopened=both('reopen','load',path=saved['saved_file'],document_id=str(uuid.uuid4()))
        r.case('advanced-native.save-roundtrip',nla.reopened_geometry(reopened)==nla.reopened_geometry(extruded))
        for method,opname in [('advanced','mirror'),('advanced','extrude'),*[('modeling',x) for x in sorted(OPS)]]:
            res=oracle.command(method,output=out/('forbid-'+opname),operation=opname,external_document=True)
            r.case('advanced-native.forbid-old-'+opname,not res['ok'] and 'C++' in res.get('error',''))
    r.data['native_modeling_timings']=timings

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('native-modeling.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    e.write_report(r,a.output);print(r.data['counts']);raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
