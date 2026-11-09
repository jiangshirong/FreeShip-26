"""Native point properties, point groups and markers: exact Plus comparison.

Includes independent member-lock/Unicode/float expectations, failed transactions,
noop/undo/redo, binary save/reopen and rejection of legacy editing dispatch.
"""
import argparse,json,uuid
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_editing as e
import native_linesplan_app as nla

OPS={'point_properties','create_group','update_group','delete_group','add_markers','delete_markers'}

def run(r,runtime,source,out):
    out.mkdir(parents=True);timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='modeling',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                result=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                results.append((result,json.loads(Path(result['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'attributes.{step:03}-{label}'
            ga,gb=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ga==gb,different_fields=[k for k in ga if ga[k]!=gb.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='modeling' and fields['operation'] in OPS:
                r.case(prefix+'.native-owner',a.get('edit_engine')=='cpp-attributes' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append({'operation':fields['operation'],'edit_ms':a['attribute_edit_ms'],'total_ms':a['document_total_ms']})
            current=am;return am
        def reload():return both('load','load',path=str(source),document_id=str(uuid.uuid4()))
        def op(operation,**fields):return both(operation,operation=operation,**fields)
        def reject(label,**fields):
            for tag,service in [('native',native),('oracle',oracle)]:
                before=dict(service.state);result=service.command('modeling',output=out/f'reject-{label}-{tag}',**fields)
                r.case('attributes.reject-'+label+'.'+tag,not result['ok'] and service.state==before,error=result.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('attributes.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('attributes.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        def point(model,index):return next(p for p in model['control_points'] if p['id']==index)

        base=reload();ids=[p['id'] for p in base['control_points'] if p.get('batch_movable',not p['locked'])][:4]
        first=ids[0];kind=point(base,first)['vertex_type']
        renamed=op('point_properties',ids=[first],name='Deck A',vertex_type=kind)
        r.case('attributes.rename-only',e.points(renamed)==e.points(base) and point(renamed,first)['name']=='Deck A')
        history('rename',base,renamed)
        before=dict(native.state);op('point_properties',ids=[first],name='Deck A',vertex_type=kind);r.case('attributes.rename-noop',native.state==before)
        both('lock','topology',operation='set_locked',ids=[first],locked=True)
        renamed=op('point_properties',ids=[first],name='Locked label',vertex_type=kind)
        r.case('attributes.locked-rename',point(renamed,first)['locked'] and point(renamed,first)['name']=='Locked label')
        reject('locked-type',operation='point_properties',ids=[first],name='Must rollback',vertex_type=(kind+1)%4)
        group=op('create_group',ids=ids[:2],name='Deck group',locked=False);index=group['point_groups'][-1]['index']
        r.case('attributes.create-unlocked-preserves-locks',point(group,first)['locked'] and not point(group,ids[1])['locked'])
        locked=op('update_group',index=index,ids=ids[:2],name='Deck group',locked=True)
        r.case('attributes.group-lock-members',all(point(locked,i)['locked'] for i in ids[:2]) and locked['point_groups'][index]['locked'])
        before=dict(native.state);op('update_group',index=index,ids=list(reversed(ids[:2])),name='Deck group',locked=True)
        r.case('attributes.group-membership-noop',native.state==before and current['point_groups'][index]['points']==ids[:2])
        shifted=op('update_group',index=index,ids=ids[1:3],name='Deck B',locked=False)
        r.case('attributes.update-leaves-former-member-lock',point(shifted,first)['locked'] and all(not point(shifted,i)['locked'] for i in ids[1:3]))
        overlap=op('create_group',ids=ids[2:4],name='Overlap',locked=True)
        r.case('attributes.overlapping-groups',ids[2] in overlap['point_groups'][index]['points'] and ids[2] in overlap['point_groups'][-1]['points'])
        deleted=op('delete_group',index=index)
        r.case('attributes.dissolve-preserves-locks',[(p['id'],p['locked']) for p in deleted['control_points']]==[(p['id'],p['locked']) for p in overlap['control_points']])
        history('group-delete',overlap,deleted)
        for label,fields in [
            ('one-member',dict(operation='create_group',ids=[first],name='A',locked=False)),
            ('duplicate-member',dict(operation='create_group',ids=[first,first],name='A',locked=False)),
            ('missing-member',dict(operation='create_group',ids=[first,2000000],name='A',locked=False)),
            ('invalid-group',dict(operation='delete_group',index=99999)),
            ('fraction-group',dict(operation='delete_group',index=.5)),
            ('invalid-lock',dict(operation='create_group',ids=ids[:2],name='A',locked='false')),
            ('empty-name',dict(operation='create_group',ids=ids[:2],name='',locked=False)),
            ('large-name',dict(operation='point_properties',ids=[first],name='A'*4097,vertex_type=kind)),
            ('unsupported-name',dict(operation='point_properties',ids=[first],name='中文点',vertex_type=kind)),
            ('fraction-type',dict(operation='point_properties',ids=[first],name='A',vertex_type=.5))]:reject(label,**fields)
        # FPC 3.2.2's JSON scanner drops U+0000, so the old TextValue guard
        # sees 'AB'. Native requests must reject it before any name is changed.
        # Keep the exact legacy difference as evidence, not as a parity success.
        before=dict(native.state)
        fields=dict(operation='point_properties',ids=[first],name='A\0B',vertex_type=kind)
        rejected=native.command('modeling',output=out/'nul-native',**fields)
        r.case('attributes.nul-native-atomic-rejection',not rejected['ok'] and native.state==before)
        legacy=q.require_ok(oracle.command('modeling',output=out/'nul-legacy',**fields))
        legacy_model=json.loads(Path(legacy['model']).read_text('utf8'))
        r.case('attributes.legacy-nul-loss-evidence',point(legacy_model,first)['name']=='AB',reason='Recorded FPC scanner drops escaped NUL; the native editor rejects the original request.')
        base=reload();first=ids[0];kind=point(base,first)['vertex_type'];newkind=(kind+1)%4
        changed=op('point_properties',ids=[first],name='',vertex_type=newkind)
        r.case('attributes.vertex-type',point(changed,first)['vertex_type']==newkind and e.points(changed)==e.points(base))
        history('vertex-type',base,changed)
        constrained=both('anchor','modeling',operation='anchor',ids=ids[:2],hard=False)
        kind=point(constrained,ids[1])['vertex_type']
        op('point_properties',ids=[ids[1]],name='Anchored label',vertex_type=kind)
        reject('constrained-type',operation='point_properties',ids=[ids[1]],name='Anchored label',vertex_type=(kind+1)%4)
        base=reload();paths=[[[.1,2,3],[4,5,6],[7,8,9]],[[10,.2,0],[11,1,1]]]
        after=op('add_markers',paths=paths)
        r.case('attributes.marker-exact-floats',[v['spline']['points'] for v in after['markers'][-2:]]==[[[e.f32(x) for x in p] for p in path] for path in paths])
        r.case('attributes.existing-markers-preserved',after['markers'][:-2]==base['markers'])
        r.case('attributes.markers-no-knuckles',all(not flag for m in after['markers'][-2:] for flag in m['spline']['knuckles']))
        history('markers',base,after)
        for label,value in [('bad-second-path',[paths[0],[[0,0,0],[1,2,'bad']]]),('one-point',[[[0,0,0]]]),('coordinate-limit',[[[0,0,0],[1e10,0,0]]]),('empty',[])]:reject(label,operation='add_markers',paths=value)
        for tag,service in [('native',native),('oracle',oracle)]:
            q.require_ok(service.command('save',output=out/'confirmed-save'/tag));q.require_ok(service.command('mark_saved'))
        saved=q.require_ok(native.command('save',output=out/'saved'))
        reopened=both('reopen','load',path=saved['saved_file'],document_id=str(uuid.uuid4()))
        r.case('attributes.saved-markers-and-names',nla.reopened_geometry(reopened)==nla.reopened_geometry(after))
        deleted=op('delete_markers');r.case('attributes.delete-all-markers',not deleted['markers'])
        old=dict(native.state);op('delete_markers');r.case('attributes.empty-delete-noop',old==native.state)
        restored=both('undo-marker-delete','undo');r.case('attributes.delete-undo',modeling.geometry(restored)==modeling.geometry(reopened))
        old=dict(native.state);kind=point(restored,first)['vertex_type'];op('point_properties',ids=[first],name=point(restored,first)['name'],vertex_type=kind)
        r.case('attributes.noop-keeps-redo',native.state==old and native.state['can_redo'])
        for operation in sorted(OPS):
            result=oracle.command('modeling',external_document=True,operation=operation,output=out/('forbid-'+operation))
            r.case('attributes.forbid-old-'+operation,not result['ok'] and 'C++' in result.get('error',''))
    r.data['native_attribute_timings']=timings

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('attributes.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
