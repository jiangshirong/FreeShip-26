"""Native persistent hydro/performance settings. No desktop automation.

Independent FBM byte/field expectations, input failures and unchanged history;
full production service checks reuse the existing calculator parameter schema.
The old save path initializes unedited fields, so it is not an oracle for a
partial patch's unrelated bytes. Calculation formula parity lives in qt-performance.
"""
import argparse,json,math,struct,subprocess,uuid
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_native_project as project
from maintenance import generate_analysis_fields as metadata

METHODS=[('delft','delft','LwlBox',44,16),('holtrop','holtr','LwlBox',44,22),
 ('hollenbach','hollenbach','LwlBox',44,25),('oortmerssen','oortmerssen','LwlBox',44,33),
 ('fung','fung','LwlBox',44,35),('ost','ost','LwlBox',44,22),('rbhs','rbhs','LwlBox',44,40),
 ('mh','mh','LwlBox',44,40),('kaper','kaper','Edit2',4,16),('planing','planing','Edit2',4,23),
 ('propeller1','propeller_task1','Edit2',0,22),('propeller2','propeller_task2','Edit2',0,22),
 ('propeller3','propeller_task3','Edit2',0,22),('propeller4','propeller_task4','Edit2',0,26),
 ('propeller5','propeller_task5','Edit2',0,30)]

def run(r,runtime,source,out):
    out.mkdir(parents=True);generated,hashes=metadata.generate()
    r.case('analysis.field-metadata-provenance',(q.QT/'src/document/analysis_fields.h').read_text('utf8')==generated and json.loads((q.QT/'src/document/analysis-fields-origin.json').read_text('utf8'))==hashes)
    env=q.private_environment(out/'probe-env');counter=0
    def probe(path,request):
        nonlocal counter
        counter+=1;req=out/f'probe-{counter}.json';target=out/f'probe-{counter}.fbm'
        req.write_text(json.dumps(dict(request=request,bindings={'fbm_encoding':'utf8'})),'utf8')
        p=subprocess.run([str(runtime/'native_analysis_edit_probe.exe'),str(path),str(req),str(target)],env=env,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
        return json.loads(p.stdout),target
    path=out/'all-fields.fbm';before=project.fixture(path)
    for method,block,key,offset,minimum in METHODS:
        request=dict(method='performance_settings',calculator=method,values={key:24.125})
        response,target=probe(path,request);after=project.observe(target)
        expected=bytearray(before['blocks'][block]);expected[offset:offset+4]=struct.pack('<f',24.125)
        r.case(f'analysis.{method}.exact-field',response['ok'] and response['changed'] and after['blocks'][block]==expected)
        r.case(f'analysis.{method}.all-other-bytes',all(data==after['blocks'][name] for name,data in before['blocks'].items() if name!=block))
        reply,again=probe(target,request);r.case(f'analysis.{method}.noop',reply['ok'] and not reply['changed'] and target.read_bytes()==again.read_bytes())
        reply,empty=probe(path,dict(method='performance_settings',calculator=method,values={}));r.case(f'analysis.{method}.empty-preserves-all-bytes',reply['ok'] and not reply['changed'] and path.read_bytes()==empty.read_bytes())
        # First format without a calculator record must gain the correct block
        # only after a change; an empty patch must never promote the file.
        old=out/f'old-{method}.fbm';project.fixture(old,version=minimum-1,rich=False)
        reply,unchanged=probe(old,dict(method='performance_settings',calculator=method,values={}))
        r.case(f'analysis.{method}.empty-old-version',reply['ok'] and not reply['changed'] and old.read_bytes()==unchanged.read_bytes())
        reply,upgraded=probe(old,request);observed=project.observe(upgraded)
        r.case(f'analysis.{method}.native-promotion',reply['ok'] and reply['version']==minimum and struct.unpack_from('<f',observed['blocks'][block],offset)[0]==24.125)
    table=dict(method='hydro_settings',mode='table',start=.5,end=2.5,step=.125,trim=-.25)
    cross=dict(method='hydro_settings',mode='cross',vcg=1.5,trim=.125,free_trim=True,angles=[0,10,20,30],displacements=[1,2,4])
    for request in (table,cross):
        response,target=probe(path,request);after=project.observe(target);mode=request['mode']
        r.case(f'analysis.hydro-{mode}.changed',response['ok'] and response['changed'])
        r.case(f'analysis.hydro-{mode}.other-blocks',all(data==after['blocks'][key] for key,data in before['blocks'].items() if key!='project_settings'))
        r.case(f'analysis.hydro-{mode}.other-project-fields',before['settings']==after['settings'] and before['preview']==after['preview'])
        if mode=='table':
            expected=dict(before['hydro'],drafts=[.5,2.5,.125,-.25])
        else:expected=dict(before['hydro'],vcg=1.5,trims=[.125],free_trim=1,heels=[0,10,20,30],displacements=[1,2,4],use_steps=0)
        r.case(f'analysis.hydro-{mode}.exact',after['hydro']==expected)
        response,again=probe(target,request);r.case(f'analysis.hydro-{mode}.noop',not response['changed'] and target.read_bytes()==again.read_bytes())
    old=out/'pre-hydro.fbm';project.fixture(old,version=19,rich=False)
    reply,target=probe(old,dict(table,start=0,end=1,step=.1,trim=0))
    r.case('analysis.hydro-noop-does-not-promote',reply['ok'] and not reply['changed'] and target.read_bytes()==old.read_bytes())
    reply,target=probe(old,table);r.case('analysis.hydro-promotes',reply['ok'] and reply['version']==20)
    for label,request in [('small-step',dict(table,step=0)),('large-table',dict(table,step=.0001)),
       ('trim',dict(table,trim=11)),('rounded-angle',dict(cross,angles=[20,20+1e-9])),
       ('rounded-mass',dict(cross,displacements=[1,1+1e-9])),('bool-number',dict(table,start=True)),
       ('empty-list',dict(cross,angles=[])),('many-cases',dict(cross,angles=list(range(181)),displacements=[1,2,3])),
       ('non-numeric',dict(cross,vcg='1')),('bad-flag',dict(cross,free_trim=1)),
       ('unknown-method',dict(method='performance_settings',calculator='bad')),
       ('unknown-field',dict(method='performance_settings',calculator='delft',values={'typo':1})),
       ('unknown-choice',dict(method='performance_settings',calculator='propeller5',values={'ComboBox1':99})),
       ('mixed-invalid',dict(method='performance_settings',calculator='delft',values={'LwlBox':5,'CpBox':2})),
       ('bad-type',dict(method='performance_settings',calculator='delft',values={'LwlBox':True}))]:
        reply,target=probe(path,request);r.case('analysis.reject-'+label,not reply['ok'] and not target.exists())
    # Explicit density and persisted combo offset; padding/unused flags survive.
    reply,target=probe(path,dict(method='performance_settings',calculator='holtrop',values={'A11box':2,'DensityBox':1.03125}))
    if not reply['ok']:
        # Actual keys are declarations, not inferred capitalization.
        catalog=json.loads(generated.split('R"fields(')[1].split(')fields"')[0]);fields=catalog['holtrop']['fields']
        key=lambda name:next(k for k in fields if k.lower()==name.lower())
        reply,target=probe(path,dict(method='performance_settings',calculator='holtrop',values={key('A11box'):2,key('DensityBox'):1.03125}))
    after=project.observe(target)
    r.case('analysis.choice-plus-one',reply['ok'] and struct.unpack_from('<f',after['blocks']['holtr'],42*4)[0]==3)
    r.case('analysis.water-density',after['settings']['water_density']==1.03125)
    # Delft estimate: independent direct formula using positive dimensions.
    values=dict(LwlBox=20,BwlBox=4,DraftBox=2,CpBox=.625,DisplacementBox=50,WlAreabox=60,EstimateBox=True)
    reply,target=probe(path,dict(method='performance_settings',calculator='delft',values=values));after=project.observe(target)
    cm=50/(20*.625)/(4*2);cwp=60/(20*4);c23=.453+.443*(.625*cm)-.286*cm-.00347*(4/2)+.37*cwp
    factor=.616*c23+.111*cm**3+.245*c23/cm-.0228;expected=factor*20*(2*2+4)*math.sqrt(cm)
    actual=struct.unpack_from('<f',after['blocks']['delft'],60)[0]
    r.case('analysis.explicit-wetted-estimate',reply['ok'] and abs(actual-expected)<2e-5,actual=actual,expected=expected)
    # Full dialog payloads also retain the original numerical field values. The
    # isolated old setter is independent of the generated C++ metadata. Unlike a
    # partial patch, supplying every input makes its initialization irrelevant.
    catalog=json.loads(generated.split('R"fields(')[1].split(')fields"')[0]);observed_catalog={}
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'full-native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'full-oracle',native_linesplan=True) as oracle:
        for method,block,key,_,_ in METHODS:
            for tag,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('load',path=str(source),document_id=str(uuid.uuid4()),output=out/f'full-{method}-load-{tag}'))
            defaults=q.require_ok(oracle.command('performance_defaults',calculator=method,output=out/f'full-{method}-defaults'))
            schema=json.loads(Path(defaults['performance']).read_text('utf8'));observed_catalog[method]=schema;fields=schema['fields'];values={f['key']:f['value'] for f in fields}
            key=next(k for k in values if k.lower()==key.lower());values[key]=24.125
            for f in fields:
                meta=catalog[method]['fields'][f['key']]
                equal=f['kind']==meta['kind']
                if f['kind']=='number':equal=equal and f['minimum']==meta['minimum'] and f['maximum']==meta['maximum']
                if f['kind']=='choice':equal=equal and len(f['choices'])==meta['count']
                r.case(f'analysis.schema.{method}.{f["key"]}',equal)
            records=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                q.require_ok(s.command('performance_settings',calculator=method,values=values,output=out/f'full-{method}-set-{tag}'))
                save=q.require_ok(s.command('save',output=out/f'full-{method}-save-{tag}'));records.append(project.observe(Path(save['saved_file'])))
            n,o=records;count,flags=next((count,flags) for name,count,flags in project.affine.RECORDS if name==block)
            r.case('analysis.full-payload.'+method,n['blocks'][block][:count*4+flags]==o['blocks'][block][:count*4+flags],actual=n['blocks'][block][:count*4+flags].hex(),expected=o['blocks'][block][:count*4+flags].hex())
            r.case('analysis.full-density.'+method,n['settings']['water_density']==o['settings']['water_density'])
        for method in ('hydro_settings','performance_settings'):
            reply=oracle.command(method,external_document=True,output=out/('forbid-'+method))
            r.case('analysis.forbid-old-'+method,not reply['ok'] and 'C++' in reply.get('error',''))
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service',native_linesplan=True) as service:
        def call(label,method,**fields):return q.require_ok(service.command(method,output=out/label,**fields))
        base=call('load','load',path=str(source),document_id=str(uuid.uuid4()));base_model=json.loads(Path(base['model']).read_text('utf8'))
        for method,_,target,_,_ in METHODS:
            schema=json.loads(Path(call(method+'-before','performance_defaults',calculator=method)['performance']).read_text('utf8'))
            key=next(f['key'] for f in schema['fields'] if f['key'].lower()==target.lower())
            result=call(method+'-write','performance_settings',calculator=method,values={key:24.125})
            r.case('analysis.service.'+method+'.native',result.get('edit_engine')=='cpp-analysis-settings' and result.get('adapter_operation')=='none' and result.get('snapshot_engine')=='cpp-publication')
            state=dict(service.state);call(method+'-same','performance_settings',calculator=method,values={key:24.125})
            r.case('analysis.service.'+method+'.noop-history',state==service.state)
            call(method+'-undo','undo');undone=json.loads(Path(call(method+'-undone','performance_defaults',calculator=method)['performance']).read_text('utf8'))
            r.case('analysis.service.'+method+'.undo-values',schema['fields']==undone['fields'])
            call(method+'-redo','redo')
        snapshot=call('after','snapshot');after_model=json.loads(Path(snapshot['model']).read_text('utf8'))
        r.case('analysis.service.geometry-unchanged',modeling.geometry(after_model)==modeling.geometry(base_model))
        for key in ('scene','dxf','sections'):r.compare('analysis.service.'+key,snapshot[key],base[key])
        target=Path(call('save','save')['saved_file']);call('reopen','load',path=str(target),document_id=str(uuid.uuid4()))
        for method,_,key,_,_ in METHODS:
            schema=json.loads(Path(call(method+'-reopened','performance_defaults',calculator=method)['performance']).read_text('utf8'))
            value=next(f['value'] for f in schema['fields'] if f['key'].lower()==key.lower())
            # Float-spin UI display has its declared decimal precision. Stored
            # Single bytes were checked independently above.
            decimals=next(f['decimals'] for f in schema['fields'] if f['key'].lower()==key.lower())
            r.case('analysis.service.'+method+'.saved-value',abs(value-24.125)<=.5*10**(-decimals)+1e-8)
        state=dict(service.state);bad=service.command('hydro_settings',output=out/'stale',expected_revision=0,**{k:v for k,v in table.items() if k!='method'})
        r.case('analysis.stale-rejected',not bad['ok'] and service.state==state)
        blocked=out/'blocked';(blocked/'scene.fslp').mkdir(parents=True)
        bad=service.command('hydro_settings',output=blocked,**{k:v for k,v in table.items() if k!='method'})
        r.case('analysis.publication-rollback',not bad['ok'] and service.state==state)
    import qt_performance_checks as performance
    import qt_hydrostatics as hydro
    requests=[('performance',performance.production_request(source,out/'qt-performance-request',method='delft',catalog=observed_catalog,save_settings=True)),
              ('hydro',hydro.production_request(source,out/'qt-hydro-request',mode='table',save_settings=True))]
    children=[]
    for kind,request in requests:
        child=q.run_child(r,'analysis.production-'+kind,runtime/'FreeShipQt.exe',['--verify-'+kind,request],out/('qt-'+kind),timeout=90,platform='windows');children.append(child)
        if child.get('status')=='error':raise RuntimeError('Production settings workflow failed: '+kind)
        if any(c.get('status')=='not_run' for c in child.get('cases',[])):continue
        art=child['artifacts']
        for artifact in ('scene','dxf'):r.compare('analysis.production-'+kind+'.'+artifact,art['original_'+artifact],art['after_'+artifact])
    r.data['analysis_production_runs']=children

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as e:r.case('analysis.execution',status='error',reason=f'{type(e).__name__}: {e}')
    import qt_editing as e;e.write_report(r,a.output);print(r.data['counts'])

    raise SystemExit(1 if r.data["counts"]["fail"] or r.data["counts"]["error"] else 0)
