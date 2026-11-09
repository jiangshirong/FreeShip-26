"""Native Lackenby integration, exact legacy oracle and independent invariants.

run(report, runtime, source, out) is the qt-document regression entry.
--probe selects the retained standalone C++ diagnostic for isolating numerical
differences before service integration; all artifacts remain in --output.
"""
import argparse
import json
import os
import subprocess
import struct
import time
import uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as nla
import qt_native_transforms as affine


def fixture(path,model,imperial=False):
    """Independent FBM 5.0 encoding with a second non-hydro layer and raw auxiliaries.

    The hull is the original new-hull template supplied by the oracle. Surface
    serialization, auxiliary payloads and expected preservation do not call the
    C++ codec or Lackenby. An above-water triangle tests real layer subsets.
    """
    data=bytearray()
    def pack(fmt,*values):data.extend(struct.pack('<'+fmt,*values))
    def floats(values):pack('f'*len(values),*values)
    def text(value):value=value.encode('ascii');pack('i',len(value));data.extend(value)
    text('FREE!ship');pack('Bi',44,1)
    pack('i',1);data.extend(bytes([1,0,1,1,1,1,1,0,0,0]));pack('fBf',1,0,.1);data.extend(bytes(7))
    text('Native Lackenby oracle');text('Independent regression fixture')
    floats([20,5,2]);pack('BffBiiBfB',1,64 if imperial else 1.025,1.0625,0,0x5080c0,int(imperial),0,10,0)
    text('Preserve all unrelated hydro and calculation bytes');text('FreeShip regression')
    pack('iBB',0,0,1);floats([.25,2,.125,.375]);pack('i',2);floats([4,6]);floats([2,8,.5]);pack('Bi',0,2);floats([0,20]);pack('i',2);floats([.1,.2]);pack('BfBBi',0,1.25,0,0,123)
    pack('i',2)
    for index,name in enumerate(('Hull','Unselected above-water triangle')):
        text(name);pack('iiBBBBBffBi',index+7,0x80a0c0,1,int(index==0),0,1,int(index==0),7.85,.004,1,255)
    pack('i',0)
    points=model['control_points'];lookup={p['id']:i for i,p in enumerate(points)}
    extra=[[5,8,12],[10,9,12],[15,8,13]]
    pack('i',len(points)+3)
    for p in points:floats(p['p']);pack('iBB',p['vertex_type'],0,0)
    for p in extra:floats(p);pack('iBB',3,0,0)
    pack('i',len(model['control_edges'])+3)
    for edge in model['control_edges']:pack('iiBB',lookup[edge['a']],lookup[edge['b']],int(edge['crease']),0)
    for a,b in ((0,1),(1,2),(2,0)):pack('iiBB',len(points)+a,len(points)+b,1,0)
    pack('i',0);pack('i',len(model['control_faces'])+1)
    for face in model['control_faces']:
        p=[lookup[i] for i in face['points']];pack('i',len(p));pack('i'*len(p),*p);pack('iB',0,0)
    pack('iiii',3,len(points),len(points)+1,len(points)+2);pack('iB',1,0)
    for kind,plane in enumerate([(1,0,0,-10),(0,1,0,-1),(0,0,1,-1),(0,.70710678,.70710678,-1)],1):
        pack('iiB',1,kind,0);floats(plane);pack('Bi',0,0)
    pack('i',1);pack('BBBfi',1,0,1,.125,2)
    for p in ((1,2,3),(4,5,6)):floats(p);pack('B',1)
    for index,(_,count,flags) in enumerate(affine.RECORDS):
        if index==2:pack('ii',0,0)
        floats([.375+i*.125+index*.03125 for i in range(count)]);data.extend(bytes(flags)+bytes([0xa5])*(4-flags))
    pack('ii',1,0);text('Reference point')
    for _ in range(3):pack('i',0)
    pack('ii',0,1);text('Reference group');pack('i',2);text('0');text('3');pack('i',0)
    path.write_bytes(data)
    return affine.observe(path)


def probe(r, runtime, source, out, executable):
    out.mkdir(parents=True, exist_ok=True)
    environment=dict(os.environ)
    environment['PATH']=str(runtime)+os.pathsep+environment.get('PATH','')
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        for label,path in [('box',source),('synthetic',q.DEFAULT_MODELS[1]),('new',None)]:
            folder=out/label;folder.mkdir()
            if path:
                q.require_ok(oracle.command('load',output=folder/'load',path=str(path),document_id=str(uuid.uuid4())))
            else:
                q.require_ok(oracle.command('new',output=folder/'new',document_id=str(uuid.uuid4()),length=20,beam=5,draft=2,columns=7,rows=5,units=0))
            saved=q.require_ok(oracle.command('save',output=folder/'save'))
            snapshot=q.require_ok(oracle.command('load',output=folder/'reload',path=saved['saved_file'],document_id=str(uuid.uuid4())))
            model=json.loads(Path(snapshot['model']).read_text('utf8'))
            original=q.require_ok(oracle.command('inspect_modeling'))['analysis']
            (folder/'expected-analysis.json').write_text(json.dumps(original,indent=2),'utf8')
            result=folder/'measure.json'
            process=subprocess.run([str(executable),'--model',saved['saved_file'],'--output',str(result)],env=environment,capture_output=True,timeout=180)
            (folder/'probe.log').write_bytes(process.stdout+process.stderr)
            actual=json.loads(result.read_text('utf8'))
            r.case('lackenby.probe.'+label+'.measure',actual.get('analysis')==original,actual=actual,expected=original)
            if label!='new':continue
            for operation,factor,delta in [('noop',1,0),('volume',1.01,0),('lcb',1,.025),('combined',.99,-.02)]:
                q.require_ok(oracle.command('load',output=folder/(operation+'-load'),path=saved['saved_file'],document_id=str(uuid.uuid4())))
                request=dict(method='modeling',operation='lackenby',ids=[l['id'] for l in model['layers']],displacement=original['displacement']*factor,lcb=original['lcb']+delta,iterations=30)
                request_path=folder/(operation+'.request.json');request_path.write_text(json.dumps(request),'utf8')
                output=folder/(operation+'.native.json')
                process=subprocess.run([str(executable),'--model',saved['saved_file'],'--request',str(request_path),'--output',str(output)],env=environment,capture_output=True,timeout=180)
                (folder/(operation+'.log')).write_bytes(process.stdout+process.stderr)
                actual=json.loads(output.read_text('utf8'))
                response=oracle.command('modeling',output=folder/(operation+'-oracle'),**{k:v for k,v in request.items() if k!='method'})
                r.case('lackenby.probe.'+operation+'.status',actual['ok']==response['ok'],actual=actual.get('error'),expected=response.get('error'))
                if actual['ok'] and response['ok']:
                    after=json.loads(Path(response['model']).read_text('utf8'))
                    expected=[p['p'] for p in after['control_points']]
                    r.case('lackenby.probe.'+operation+'.points',actual['points']==expected,actual=actual['points'],expected=expected)
                    analysis=q.require_ok(oracle.command('inspect_modeling'))['analysis']
                    r.case('lackenby.probe.'+operation+'.after-measure',actual['after']==analysis,actual=actual['after'],expected=analysis)


def run(r,runtime,source,out):
    out.mkdir(parents=True,exist_ok=True);timings=[];measurements=[]
    from maintenance.lackenby_math import audit
    constants=audit(out/'rtl-convergence')
    r.case('lackenby.fixed-convergence-binary64',constants['ok'] and constants['values']==100,evidence=str(out/'rtl-convergence/report.json'))
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None;new_template=False
        def both(label,method='snapshot',**fields):
            nonlocal step,current,new_template
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                response=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                results.append((response,json.loads(Path(response['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'lackenby.{step:03}-{label}'
            ag,bg=modeling.geometry(am),modeling.geometry(bm)
            if method=='new':new_template=True
            elif method=='load':new_template=False
            if new_template:
                # FreeProjectSettings.Clear enables preview for new hulls. Only
                # bridge/NewHull.inc disables it to avoid creating old GUI views.
                # Assert both contracts explicitly; compare all remaining fields.
                r.case(prefix+'.new-hull-preview-enabled',am['settings']['save_preview'] is True)
                r.case(prefix+'.headless-oracle-preview-disabled',bm['settings']['save_preview'] is False)
                bg['settings']=dict(bg['settings'],save_preview=True)
            r.case(prefix+'.model',ag==bg,different_fields=[k for k in ag if ag[k]!=bg.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='modeling' and fields.get('operation')=='lackenby':
                r.case(prefix+'.native-dispatch',a.get('edit_engine')=='cpp-lackenby' and a.get('fbm_write_engine')=='cpp-coordinate-writer')
                timings.append(dict(operation=label,edit_ms=a['lackenby_edit_ms'],total_ms=a['document_total_ms']))
            current=am;return am
        def load(label,path):return both(label,'load',path=str(path),document_id=str(uuid.uuid4()))
        def measure(label):
            results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                old=dict(service.state);start=time.perf_counter();response=service.command('inspect_modeling');elapsed=(time.perf_counter()-start)*1000
                r.case('lackenby.measure-'+label+'.'+tag+'.readonly',service.state==old)
                results.append(response);measurements.append(dict(case=label,engine=tag,total_ms=elapsed))
            a,b=results;r.case('lackenby.measure-'+label+'.status',a['ok']==b['ok'],actual=a.get('error'),expected=b.get('error'))
            if a['ok'] and b['ok']:
                r.case('lackenby.measure-'+label+'.exact',a['analysis']==b['analysis'],actual=a['analysis'],expected=b['analysis'])
                r.case('lackenby.measure-'+label+'.native-dispatch',a.get('analysis_engine')=='cpp-lackenby')
                return a['analysis']
            return None
        def transform(label,analysis,factor=1,delta=0,ids=None,iterations=30):
            before=current;points=e.points(before)
            after=both(label,'modeling',operation='lackenby',ids=ids or [l['id'] for l in before['layers']],
                       displacement=analysis['displacement']*factor,lcb=analysis['lcb']+delta,iterations=iterations)
            r.case('lackenby.'+label+'.only-x',all(e.points(after)[i][1:]==p[1:] for i,p in points.items()))
            for key in ('settings','layers','control_edges','control_faces','control_curves','markers','point_groups'):
                r.case('lackenby.'+label+'.preserve-'+key,after.get(key)==before.get(key))
            return after
        def reject(label,request):
            before=current
            for tag,service in [('native',native),('oracle',oracle)]:
                old=dict(service.state);response=service.command('modeling',output=out/('reject-'+label+'-'+tag),**request)
                r.case('lackenby.reject-'+label+'.'+tag,not response['ok'] and service.state==old,error=response.get('error'))
            after=both('after-reject-'+label)
            r.case('lackenby.reject-'+label+'.atomic-model',modeling.geometry(after)==modeling.geometry(before))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('lackenby.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('lackenby.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))

        box=load('box',source);box_measure=measure('box')
        if box_measure and source.resolve()==q.DEFAULT_MODELS[0].resolve():
            # The independent prism volume is exact; the inherited 500-fragment
            # quadrature has a documented finite sampling error. Oracle equality
            # above remains strict, with no epsilon.
            dimensions=box['settings'];volume=dimensions['length']*dimensions['beam']*dimensions['draft']
            r.case('lackenby.prism.analytic-volume',abs(box_measure['volume']/volume-1)<1e-4,analytic=volume,actual=box_measure['volume'])
            r.case('lackenby.prism.center',abs(box_measure['lcb']-dimensions['length']/2)<dimensions['length']*5e-5)
        load('supplied-synthetic',q.DEFAULT_MODELS[1]);measure('supplied-synthetic')
        if q.DEFAULT_MODELS[2].is_file():load('supplied-course',q.DEFAULT_MODELS[2]);measure('supplied-course')
        template=both('new-template','new',document_id=str(uuid.uuid4()),length=20,beam=5,draft=2,columns=7,rows=5,units=0)
        analysis=measure('new-template');old=dict(native.state)
        transform('noop',analysis);r.case('lackenby.noop-history',native.state==old)
        for label,factor,delta in [('volume',1.01,0),('lcb',1,.025),('combined',.99,-.02)]:
            before=current;after=transform(label,analysis,factor,delta);actual=measure(label)
            r.case('lackenby.'+label+'.target-volume',abs(actual['displacement']/(analysis['displacement']*factor)-1)<=1e-4)
            r.case('lackenby.'+label+'.target-lcb',abs(actual['lcb']-analysis['lcb']-delta)<=max(.002,analysis['length']*5e-5))
            history(label,before,after);both(label+'-reset','undo')

        for precision in (0,3):
            both('precision-'+str(precision),'update_project',changes={'precision':precision,'simplify_intersections':False})
            analysis=measure('precision-'+str(precision));before=current;after=transform('precision-'+str(precision),analysis,1.005)
            measure('precision-'+str(precision)+'-after');both('precision-transform-undo','undo');both('precision-settings-undo','undo')
        analysis=measure('restored');ids=list(e.points(current))
        both('translated','advanced',operation='translate',kind='point',ids=ids,vector=[100,0,0],adjust_markers=False)
        both('translated-split','update_project',changes={'use_default_split_section_location':False,'split_section_location':110})
        translated=measure('translated');transform('translated-target',translated,1.01);measure('translated-after')

        # Independent multi-layer fixtures also seed every resistance record,
        # marker attributes, stored hydro ranges, names and a point group.
        for imperial in (False,True):
            label='imperial' if imperial else 'metric';path=out/(label+'.fbm');original=fixture(path,template,imperial)
            before=load(label,path);analysis=measure(label);selected=before['layers'][0]['id']
            excluded={p for f in before['control_faces'] if f['layer']!=selected for p in f['points']}
            hull={p for f in before['control_faces'] if f['layer']==selected for p in f['points']};excluded-=hull
            r.case('lackenby.'+label+'.real-subset-fixture',len(excluded)==3)
            after=transform(label+'-subset',analysis,1.01,ids=[selected]);measure(label+'-after')
            r.case('lackenby.'+label+'.unselected-points-fixed',all(e.points(before)[p]==e.points(after)[p] for p in excluded))
            r.case('lackenby.'+label+'.selected-points-changed',any(e.points(before)[p]!=e.points(after)[p] for p in hull))
            saved=q.require_ok(native.command('save',output=out/(label+'-saved')));q.require_ok(oracle.command('save',output=out/(label+'-oracle-saved')))
            observed=affine.observe(Path(saved['saved_file']))
            for key in ('settings','hydro_bytes','markers','marker_attributes','planes','calculation_bytes'):
                r.case('lackenby.'+label+'.raw-'+key,observed[key]==original[key])
            history(label+'-subset',before,after)
            reopened=load(label+'-reopen',saved['saved_file']);r.case('lackenby.'+label+'.save-reopen',nla.reopened_geometry(reopened)==nla.reopened_geometry(after))
            # Excluding non-hydro geometry must not exclude it from explicit
            # all-layer edits. Its X changes using the same mapping function.
            analysis=measure(label+'-reopened');previous=current
            all_layers=transform(label+'-all-layers',analysis,.995)
            decoration={p for f in previous['control_faces'] if f['layer']==previous['layers'][1]['id'] for p in f['points']}
            r.case('lackenby.'+label+'.explicit-nonhydro-points-move',any(e.points(previous)[p]!=e.points(all_layers)[p] for p in decoration))

        base=load('validation-baseline',out/'metric.fbm');analysis=measure('validation');layer=base['layers'][0]['id']
        common=dict(operation='lackenby',ids=[layer],displacement=analysis['displacement']*1.01,lcb=analysis['lcb'],iterations=30)
        for label,patch in [('empty',{'ids':[]}),('duplicate',{'ids':[layer,layer]}),('unknown',{'ids':[1999999]}),
                ('fractional-id',{'ids':[1.25]}),('negative-displacement',{'displacement':-1}),('zero-displacement',{'displacement':0}),
                ('bad-displacement-type',{'displacement':True}),('nonfinite-displacement',{'displacement':1e100}),
                ('outside-lcb',{'lcb':-1}),('bad-lcb-type',{'lcb':'10'}),('zero-iterations',{'iterations':0}),
                ('too-many-iterations',{'iterations':101}),('fractional-iterations',{'iterations':1.5}),
                ('nonconvergence',{'displacement':analysis['displacement']*1.2,'iterations':1})]:
            reject(label,{**common,**patch})
        point=next(iter(e.points(base)))
        both('lock','topology',operation='set_locked',ids=[point],locked=True);reject('locked',common);both('unlock','undo')
        ids=list(e.points(base))[:2]
        both('anchor','modeling',operation='anchor',ids=ids,hard=False);reject('dependent',common);both('unanchor','undo')
        before=current;old=dict(native.state);blocked=out/'blocked-output';blocked.mkdir();(blocked/'model.json').mkdir()
        response=native.command('modeling',output=blocked,**common)
        r.case('lackenby.publication-failure.atomic',not response['ok'] and native.state==old)
        after=both('after-publication-failure');r.case('lackenby.publication-failure.model',modeling.geometry(after)==modeling.geometry(before))
        from qt_document import stop_owned_adapter
        status=q.require_ok(native.command('document_status'))
        if status['adapter_pid']:stop_owned_adapter(int(status['adapter_pid']),runtime/'bridge/FreeShipGeometryBridge.exe')
        both('after-adapter-recovery');analysis=measure('after-recovery');transform('recovered-transform',analysis,1.01)
        response=oracle.command('modeling',output=out/'forbidden-transform',external_document=True,**common)
        r.case('lackenby.forbidden-legacy-transform',not response['ok'] and 'C++' in response.get('error',''))
        response=oracle.command('inspect_modeling',external_document=True)
        r.case('lackenby.forbidden-legacy-measure',not response['ok'] and 'C++' in response.get('error',''))
    r.data['native_lackenby_timings']=timings;r.data['native_lackenby_measurement_timings']=measurements


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);p.add_argument('--probe',type=Path)
    a=p.parse_args();r=q.Report()
    try:
        if a.probe:probe(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve(),a.probe.resolve())
        else:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('lackenby.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True)
    (a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
