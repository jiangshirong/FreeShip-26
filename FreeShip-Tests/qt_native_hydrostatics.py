"""Native hydrostatics: analytical hulls, independent Plus data and real Qt callbacks.

No display-rounded comparisons and no relaxed numerical tolerances. Reports and
fault injection use private processes. The original Balance is not an oracle.
"""
import argparse,json,math,time,uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_hydrostatics as hydro
import qt_hydro_cases as cases
import qt_native_project as project

def parity(r,runtime,models,out,balance=True):
    out.mkdir(parents=True);timings=[]
    material=out/'materials.fbm';project.fixture(material,rich=False)
    for i,model in enumerate([*models,material]):
        folder=out/str(i)
        with q.Service(runtime/'FreeShipDocumentService.exe',folder/'native') as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'oracle') as oracle:
            def call(s,label,method,**fields):return q.require_ok(s.command(method,output=folder/(label+('-native' if s is native else '-oracle')),**fields))
            for s in (native,oracle):base=call(s,'load','load',path=str(model),document_id=str(uuid.uuid4()))
            settings=json.loads(Path(base['model']).read_text('utf8'))['settings'];draft=settings['draft'];length=settings['length']
            a=call(native,'defaults','hydro_defaults');b=call(oracle,'defaults','hydro_defaults')
            r.case(f'hydro.{i}.native-defaults',a.get('analysis_engine')=='cpp-hydrostatics')
            r.case(f'hydro.{i}.defaults',a['analysis']==b['analysis'],actual=a['analysis'],expected=b['analysis'])
            conditions=[dict(draft=draft*ratio,heel=heel,trim=trim) for ratio,heel,trim in [(1,0,0),(.8,0,0),(1,5,0),(1,0,.01*length)]]
            upright=None
            for j,condition in enumerate(conditions):
                reports=[]
                for s in (native,oracle):
                    before=dict(s.state);reply=call(s,f'single-{j}','hydrostatics',mode='single',**condition)
                    reports.append(json.loads(Path(reply['hydro']).read_text('utf8')))
                    r.case(f'hydro.{i}.{j}.readonly-'+('native' if s is native else 'oracle'),s.state==before)
                a,b=reports;expected=dict(b['rows'][0]);expected['valid']=expected.pop('status')=='ok'
                cases.compare_original(r,f'hydro.{i}.{j}',a['rows'][0],expected)
                r.case(f'hydro.{i}.{j}.engine',a['engine']=='cpp-hydrostatics')
                timings.append(dict(model=str(model),mode='single',condition=condition,native_ms=a['elapsed_ms'],legacy_ms=b['elapsed_ms'],mesh_reused=a['mesh_reused'],sections=a['section_evaluations']))
                if j==0:upright=a['rows'][0]
            if upright['status']!='ok':continue
            for free in ((False,True) if balance else ()):
                args=dict(mode='cross',angles=[0,5,10],displacements=[upright['displacement']],trim=0,free_trim=free,
                          lcg=upright['buoyancy'][0],tcg=0,vcg=upright['keel_z']+draft/2)
                reports=[]
                for s in (native,oracle):
                    reply=call(s,'balance-'+str(free),'hydrostatics',**args);reports.append(json.loads(Path(reply['hydro']).read_text('utf8')))
                a,b=reports
                r.case(f'hydro.{i}.balance-{free}.native-no-sections',a['engine']=='cpp-hydrostatics' and a['section_evaluations']==0)
                for j,(row,original) in enumerate(zip(a['rows'],b['rows'])):
                    r.case(f'hydro.{i}.balance-{free}-{j}.valid',row['status']=='ok' and original['status']=='ok',actual=row,expected=original)
                    if row['status']=='ok' and original['status']=='ok':
                        for key in ('volume','displacement','kn','gz','trim'):cases.close(r,f'hydro.{i}.balance-{free}-{j}.{key}',row[key],original[key])
                timings.append(dict(model=str(model),mode='cross',free_trim=free,native_ms=a['elapsed_ms'],legacy_ms=b['elapsed_ms'],evaluations=a['volume_evaluations']))
            # Reuse is a numerical cache, invalidated by a genuine document edit.
            a=call(native,'cached','hydrostatics',mode='single',draft=draft,heel=0,trim=0)
            r.case(f'hydro.{i}.cached',a['mesh_reused'])
            call(native,'appendage','update_project',changes={'appendage_coefficient':1.125})
            a=call(native,'changed','hydrostatics',mode='single',draft=draft,heel=0,trim=0)
            data=json.loads(Path(a['hydro']).read_text('utf8'))
            r.case(f'hydro.{i}.invalidated',not a['mesh_reused'])
            cases.close(r,f'hydro.{i}.appendage',data['rows'][0]['volume'],upright['volume']*1.125/settings['appendage_coefficient'])
            call(native,'undo','undo');a=call(native,'restored','hydrostatics',mode='single',draft=draft,heel=0,trim=0)
            data=json.loads(Path(a['hydro']).read_text('utf8'))
            r.case(f'hydro.{i}.undo',data['rows'][0]==upright)
            a=call(native,'without-adapter','hydrostatics',mode='single',draft=draft,heel=0,trim=0)
            data=json.loads(Path(a['hydro']).read_text('utf8'))
            r.case(f'hydro.{i}.without-pascal',data['rows'][0]==upright and a.get('adapter_pid')==0 and a.get('adapter_requests')==0)
            for method in ('hydro_defaults','hydrostatics'):
                rejected=oracle.command(method,external_document=True,output=folder/('forbid-'+method))
                r.case(f'hydro.{i}.forbid-old-'+method,not rejected['ok'] and 'C++' in rejected.get('error',''))
    r.data['native_hydro_timings']=timings

def formats(r,runtime,out):
    out.mkdir(parents=True)
    for version in (0,2,6,11,19,20,44):
        path=out/f'v{version}.fbm';project.fixture(path,version=version,rich=True)
        values=[]
        for label,exe in [('native',runtime/'FreeShipDocumentService.exe'),('oracle',runtime/'bridge/FreeShipGeometryBridge.exe')]:
            # Its pre-1.65 decoder is not an oracle for raw early files. Check
            # those absent fields against documented defaults independently.
            if label=='oracle' and version<7:continue
            with q.Service(exe,out/f'v{version}-{label}') as s:
                q.require_ok(s.command('load',path=str(path),document_id=str(uuid.uuid4()),output=out/f'load-{version}-{label}'))
                values.append(q.require_ok(s.command('hydro_defaults'))['analysis'])
        if version>=7:r.case(f'hydro.format-{version}.defaults',values[0]==values[1],actual=values[0],expected=values[1])
        else:
            d=values[0]
            r.case(f'hydro.format-{version}.absent-settings',d['start']==0 and d['end']==1 and d['step']==e.f32(.1) and d['free_trim'] and d['vcg']==1 and d['angles']==[0,2,5,10,15,20,30,40,50,60,70,80,90,100,110,120,130,140,150],actual=d)

def run(r,runtime,models,out,production=True):
    out.mkdir(parents=True,exist_ok=False);exe=runtime/'FreeShipDocumentService.exe'
    hydro.service_cases(r,exe,models[0],out/'service')
    cases.analytic(r,exe,models[0],out/'analytic',None)
    cases.settings_cases(r,exe,models[0],out/'settings',None)
    parity(r,runtime,models,out/'parity')
    formats(r,runtime,out/'formats')
    if production:hydro.production(r,runtime,models[0],out/'production')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--no-production',action='store_true');p.add_argument('--single-only',action='store_true');a=p.parse_args();r=q.Report();start=time.perf_counter()
    try:
        if a.single_only:
            parity(r,a.runtime.resolve(),q.DEFAULT_MODELS,a.output.resolve(),balance=False)
            r.case('balance-and-production',status='not_run',reason='Focused single-condition diagnostic')
        else:run(r,a.runtime.resolve(),q.DEFAULT_MODELS,a.output.resolve(),not a.no_production)
    except Exception as ex:r.case('hydro.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.data['elapsed_s']=time.perf_counter()-start;e.write_report(r,a.output);print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
