"""Independent hydrostatics expectations and direct original-core observations.

The rectangular prismatic cases use volume moments integrated analytically;
the old Balance routine and the new solver are never used as an oracle.
"""
import json, math, threading
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_editing_fixture as fixture


def calculate(r,s,out,name,**inputs):
    state=dict(s.state);folder=out/name
    response=q.require_ok(s.command('hydrostatics',output=folder,**inputs))
    result=json.loads(Path(response['hydro']).read_text('utf8'))
    r.case(name+'.readonly',s.state==state and result['revision']==state['revision'])
    r.case(name+'.private-inputs',not any(k in result['inputs'] for k in ('id','method','output','document_id','revision','native_linesplan')))
    return result


def close(r,name,got,expected,rel=3e-5,absolute=3e-5):
    r.case(name,math.isclose(got,expected,rel_tol=rel,abs_tol=absolute),observed=got,expected=expected)


def analytic(r,exe,model,out,probe):
    out.mkdir(parents=True)
    with q.Service(exe,out/'process') as s:
        base=e.take(r,'extended',s,out,'base','load',path=str(model))
        def change(name,method,**fields):return e.take(r,'extended',s,out,name,method,**fields)
        def job(name,**fields):return calculate(r,s,out,name,**fields)
        def cross(name,**overrides):
            fields=dict(mode='cross',angles=[0,5,10],displacements=[mass],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2);fields.update(overrides)
            return job(name,**fields)
        mass=job('upright',mode='single',draft=2,heel=0,trim=0)['rows'][0]['displacement']
        result=cross('integral');rows=result['rows'];wanted=0
        for i,row in enumerate(rows):
            if i:wanted+=(rows[i-1]['gz']+row['gz'])*.5*math.radians(row['heel']-rows[i-1]['heel'])
            close(r,f'integral-{i}',row['gz_integral'],wanted,absolute=1e-12)
        # Cancel an actual running batch, rather than only a pre-created flag.
        cancelled=out/'in-progress-cancel';cancelled.mkdir();before=dict(s.state)
        signalled=threading.Event()
        def signal_cancel():
            (cancelled/'cancel.flag').write_text('cancel');signalled.set()
        timer=threading.Timer(.025,signal_cancel);timer.start()
        try:
            response=s.command('hydrostatics',output=cancelled,mode='cross',angles=[i*.5 for i in range(181)],displacements=[mass,mass*1.5],trim=0,free_trim=True,lcg=10,tcg=0,vcg=2)
        finally:timer.cancel();timer.join()
        if response['ok'] and not signalled.is_set():
            r.case('in-progress-cancel.timing',status='not_run',reason='The batch completed before the timer fired; no cancellation request was delivered.')
        elif response['ok']:
            aborted=json.loads(Path(response['hydro']).read_text('utf8'))
            r.case('in-progress-cancel.signalled',aborted['cancelled'],row_count=len(aborted['rows']),elapsed_ms=aborted['elapsed_ms'])
            r.case('in-progress-cancel.partial-results',len(aborted['rows'])<362)
        else:r.case('in-progress-cancel.timing',status='not_run',reason='Cancellation arrived before the first completed condition',error=response.get('error'))
        r.case('in-progress-cancel.readonly',s.state==before)
        rows=cross('signed-heel',angles=[-10,0,10])['rows']
        close(r,'signed-heel.odd',rows[0]['gz'],-rows[-1]['gz'])
        r.case('signed-heel.no-false-integral',all(row['gz_integral'] is None for row in rows))
        rows=cross('off-center-gravity',angles=[0,10],tcg=.25)['rows']
        for row in rows:
            phi=math.radians(row['heel']);gz=(1+1.5-2+.75*math.tan(phi)**2)*math.sin(phi)-.25*math.cos(phi)
            close(r,'off-center-gravity.'+str(row['heel']),row['gz'],gz)
        # Rectangular hull, longitudinal plane z = 2 + trim*(x/L - 1/2).
        # x_B=L/2+trim*L/(12*T), z_B=T/2+trim^2/(24*T).
        # Equilibrium in the horizontal frame: (x_B-x_G)+(z_B-z_G)*trim/L=0.
        for label,initial in [('free-trim-offset',0),('free-trim-invalid-initial',20)]:
            row=cross(label,angles=[0],free_trim=True,lcg=10.2,trim=initial)['rows'][0]
            r.case(label+'.valid',row['status']=='ok',row=row)
            if row['status']=='ok':
                trim=row['trim'];residual=-.2+trim*(20/24-1/20)+trim**3/960
                close(r,label+'.analytic-moment',residual,0,absolute=6e-5)
                r.case(label+'.convergence',abs(row['longitudinal_residual'])<=row['length_tolerance'])
                if probe:
                    original=fixture.run(probe,model,out/(label+'-oracle'),'hydro',{'conditions':[dict(heel=0,trim=trim,plane=row['plane'])]})['rows'][0]
                    close(r,label+'.original-volume',row['volume'],original['volume'],absolute=1e-5)
                    for axis in range(3):close(r,label+f'.original-b{axis}',row['buoyancy'][axis],original['buoyancy'][axis],absolute=1e-5)
        impossible=cross('unbalanced-cg',angles=[0],free_trim=True,lcg=1e6)['rows'][0]
        r.case('unbalanced-cg.error',impossible['status']=='error' and 'gz' not in impossible)
        flooded=cross('cross-flooding',angles=[0,10,90,180])['rows']
        r.case('cross-flooding.explicit',flooded[2]['status']=='error' and flooded[3]['status']=='error')
        r.case('cross-flooding.no-fake-zero',all('gz' not in row for row in flooded[2:]))
        high=job('table-flooding',mode='table',start=7,end=9,step=1,trim=0)['rows']
        r.case('table-flooding.independent-rows',high[0]['status']=='ok' and high[-1]['status']=='error' and 'volume' not in high[-1])
        fractional=job('fractional-table',mode='table',start=.1,end=.3,step=.1,trim=0)['rows']
        r.case('fractional-table.count',len(fractional)==3)
        for row in fractional:close(r,'fractional-table.volume',row['volume'],120*row['draft'])
        before=dict(s.state);blocked=out/'blocked-report';blocked.mkdir();(blocked/'hydro.json').mkdir()
        response=s.command('hydrostatics',output=blocked,mode='single',draft=2,heel=0,trim=0)
        r.case('report-write-failure.atomic',not response['ok'] and s.state==before)
        close(r,'after-write-failure.alive',job('after-write-failure',mode='single',draft=2,heel=0,trim=0)['rows'][0]['volume'],240)
        for name,fields in [
            ('missing-cg',dict(mode='cross',angles=[0],displacements=[mass],trim=0,free_trim=False)),
            ('invalid-mode',dict(mode='unrecognized',trim=0)),
            ('duplicate-mass',dict(mode='cross',angles=[0],displacements=[mass,mass],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2)),
            ('negative-draft',dict(mode='single',draft=-1,heel=0,trim=0)),
            ('non-numeric-angle',dict(mode='cross',angles=['5'],displacements=[mass],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2)),
        ]:
            before=dict(s.state);response=s.command('hydrostatics',output=out/name,**fields)
            r.case(name+'.atomic-rejection',not response['ok'] and s.state==before)
        after=change('after-reads','snapshot');e.same(r,'extended.readonly-geometry',after,base)
        # Translation in X/Z must preserve mass and the righting arm when CG is
        # translated as well. This catches mixing absolute Z with a keel datum.
        shifted=change('shift','advanced',operation='translate',kind='point',ids=list(e.points(base['model'])),vector=[7,0,3])
        row=cross('shifted-cross',angles=[10],lcg=17,vcg=5)['rows'][0]
        r.case('shifted-cross.valid',row['status']=='ok',row=row)
        if row['status']=='ok':close(r,'shifted-cross.gz',row['gz'],result['rows'][-1]['gz'])
        static=job('shifted-static',mode='single',draft=2,heel=0,trim=0)['rows'][0]
        close(r,'shifted-static.volume',static['volume'],240);close(r,'shifted-static.bz-keel',static['buoyancy'][2],1)
        change('shift-undo','undo')
        # Physical units convert lengths, density and mass together.
        imperial=change('imperial','update_project',changes={'units':'imperial'})
        foot=.3048;ton=1016.0469088
        im=job('imperial-static',mode='single',draft=2/foot,heel=0,trim=0)['rows'][0]
        close(r,'imperial.volume',im['volume']*foot**3,240)
        close(r,'imperial.mass',im['displacement']*ton/1000,mass)
        row=cross('imperial-cross',angles=[10],displacements=[im['displacement']],lcg=10/foot,vcg=2/foot)['rows'][0]
        r.case('imperial-cross.valid',row['status']=='ok',row=row)
        if row['status']=='ok':close(r,'imperial.gz',row['gz']*foot,result['rows'][-1]['gz'])
        if probe:
            saved=e.saved_copy(s,out/'imperial-save')
            original=fixture.run(probe,saved,out/'imperial-oracle','hydro',{'conditions':[dict(draft=2/foot,heel=0,trim=0)]})['rows'][0]
            compare_original(r,'imperial-original',im,original)
        change('metric-restore','load',path=str(model),document_id=str(q.uuid.uuid4()))
        change('hidden-layer','update_layer',layer_id=1,changes={'visible':False})
        close(r,'hidden-layer.still-hydro',job('hidden-hydro',mode='single',draft=2,heel=0,trim=0)['rows'][0]['volume'],240)
        change('excluded-layer','update_layer',layer_id=1,changes={'use_in_hydrostatics':False})
        r.case('excluded-layer.explicit-error',job('no-hydro-layer',mode='single',draft=2,heel=0,trim=0)['rows'][0]['status']=='error')
        change('asymmetric-open','update_layer',layer_id=1,changes={'use_in_hydrostatics':True,'symmetric':False})
        r.case('asymmetric-centerline-leak',job('asymmetric-leak',mode='single',draft=2,heel=0,trim=0)['rows'][0]['status']=='error')
        change('close-centerline','topology',operation='add_face',ids=[1,4,8,7],layer_id=1)
        half=job('asymmetric-half',mode='single',draft=2,heel=0,trim=0)['rows'][0]
        r.case('asymmetric-half.valid',half['status']=='ok',row=half)
        if half['status']=='ok':
            close(r,'asymmetric-half.volume',half['volume'],120)
            for key,expected in dict(waterplane_area=60,waterplane_ix=45,waterplane_iy=2000,midship_area=6,kmt=1.375,kml=1+2000/120).items():
                close(r,'asymmetric-half.'+key,half[key],expected)
            close(r,'asymmetric-half.lateral-area',half['lateral_area'],40)
            for axis,value in enumerate([10,0,1]):close(r,'asymmetric-half.lateral-cog-'+str(axis),half['lateral_cog'][axis],value)
            r.case('asymmetric-half.one-sided-entrance-unavailable',half.get('partial') and half['entrance_angle'] is None)
            row=cross('asymmetric-balance',angles=[10],displacements=[half['displacement']],tcg=1.5)['rows'][0]
            r.case('asymmetric-balance.valid',row['status']=='ok',row=row)
            if row['status']=='ok':
                phi=math.radians(10);close(r,'asymmetric-balance.analytic',row['gz'],(1+.375-2+.1875*math.tan(phi)**2)*math.sin(phi))


def compare_original(r,name,row,original):
    r.case(name+'.valid',row['status']=='ok' and original['valid'],actual=row.get('message'),original=original.get('error'))
    if row['status']!='ok' or not original['valid']:return
    for key,val in original.items():
        if key in ('error','valid'):continue
        # Exact float values are recorded. A few integration traversals reorder
        # equal-position points after save/reload; bound comparisons to Single
        # accumulation tolerance, never use display-rounded values.
        if isinstance(val,list):
            def flatten(v):return [x for item in v for x in (flatten(item) if isinstance(item,list) else [item])]
            a,b=flatten(row[key]),flatten(val)
            r.case(name+'.'+key,len(a)==len(b) and all(math.isclose(x,y,rel_tol=3e-5,abs_tol=3e-5) for x,y in zip(a,b)),actual=row[key],original=val)
        else:close(r,name+'.'+key,row[key],val)


def original_cases(r,exe,probe,models,out):
    out.mkdir(parents=True)
    for index,model in enumerate(models):
        folder=out/f'model-{index}';folder.mkdir()
        with q.Service(exe,folder/'process') as s:
            base=e.take(r,'original',s,folder,'base','load',path=str(model))
            draft=base['model']['settings']['draft'];length=base['model']['settings']['length']
            conditions=[dict(draft=draft*ratio,heel=heel,trim=trim) for ratio,heel,trim in [(1,0,0),(.8,0,0),(1,5,0),(1,0,.01*length)]]
            observations=fixture.run(probe,model,folder/'oracle','hydro',{'conditions':conditions})['rows']
            for i,(condition,original) in enumerate(zip(conditions,observations)):
                actual=calculate(r,s,folder,f'condition-{i}',mode='single',**condition)['rows'][0]
                compare_original(r,f'original-{index}-{i}',actual,original)
            # Test the design-displacement stability path on each real hull,
            # then re-integrate its final planes using the original core alone.
            upright=observations[0];keel=base['model']['bounds']['min'][2]
            for free in (False,True):
                label=f'balance-{index}-'+('free' if free else 'fixed')
                cg=[upright['buoyancy'][0],0,keel+draft/2]
                result=calculate(r,s,folder,label,mode='cross',angles=[0,5,10] if not free else [5],
                    displacements=[upright['displacement']],trim=0,free_trim=free,lcg=cg[0],tcg=cg[1],vcg=cg[2])
                successful=[row for row in result['rows'] if row['status']=='ok']
                r.case(label+'.converged',len(successful)==len(result['rows']),rows=result['rows'],elapsed_ms=result['elapsed_ms'])
                if not successful:continue
                request={'conditions':[dict(heel=row['heel'],trim=row['trim'],plane=row['plane']) for row in successful]}
                observed=fixture.run(probe,model,folder/(label+'-oracle'),'hydro',request)['rows']
                for i,(row,original) in enumerate(zip(successful,observed)):
                    r.case(label+f'.{i}.mass',abs(original['displacement']-upright['displacement'])<=row['mass_tolerance'],actual=original['displacement'],target=upright['displacement'])
                    phi=math.radians(row['heel']);theta=math.atan(row['trim']*math.cos(phi)/length)
                    gravity_y=cg[1]*math.cos(phi)+(cg[2]-result['keel_z'])*math.sin(phi)
                    gravity_x=cg[0]*math.cos(theta)+(-cg[1]*math.sin(phi)+(cg[2]-result['keel_z'])*math.cos(phi))*math.sin(theta)
                    close(r,label+f'.{i}.independent-gz',row['gz'],original['buoyancy'][1]-gravity_y)
                    if free:r.case(label+f'.{i}.longitudinal-balance',abs(original['buoyancy'][0]-gravity_x)<=row['length_tolerance'],residual=original['buoyancy'][0]-gravity_x)


def settings_cases(r,exe,model,out,probe):
    out.mkdir(parents=True)
    with q.Service(exe,out/'process') as s:
        base=e.take(r,'settings',s,out,'base','load',path=str(model))
        defaults=q.require_ok(s.command('hydro_defaults'))['analysis']
        fields=dict(mode='cross',angles=[0,5,10,30],displacements=[100,200,300],trim=.25,vcg=1.75,free_trim=True)
        changed=e.take(r,'settings',s,out,'cross','hydro_settings',**fields)
        now=q.require_ok(s.command('hydro_defaults'))['analysis']
        r.case('settings.cross-values',all(now[k]==v for k,v in fields.items() if k not in ('mode','trim')) and now['cross_trim']==fields['trim'])
        for key in ('scene','dxf'):r.compare('settings.no-geometry-change.'+key,changed['paths'][key],base['paths'][key])
        state=dict(s.state);q.require_ok(s.command('hydro_settings',output=out/'noop',**fields));r.case('settings.noop-history',state==s.state)
        q.require_ok(s.command('undo',output=out/'undo'));r.case('settings.cross-undo',q.require_ok(s.command('hydro_defaults'))['analysis']==defaults)
        q.require_ok(s.command('redo',output=out/'redo'));r.case('settings.cross-redo',q.require_ok(s.command('hydro_defaults'))['analysis']==now)
        before=dict(s.state);blocked=out/'failed-settings';blocked.mkdir();(blocked/'model.json').mkdir()
        response=s.command('hydro_settings',output=blocked,**{**fields,'vcg':2})
        r.case('settings.failed-publish',not response['ok'] and s.state==before and q.require_ok(s.command('hydro_defaults'))['analysis']==now)
        for label,patch in [('rounded-duplicate',{'displacements':[200,200.0000001]}),('bad-list',{'angles':[10,5]}),('too-many',{'angles':list(range(101)),'displacements':list(range(1,7))})]:
            response=s.command('hydro_settings',output=out/label,**{**fields,**patch});r.case('settings.'+label,not response['ok'] and s.state==before)
        saved=e.saved_copy(s,out/'save');q.require_ok(s.command('load',output=out/'reopen',path=str(saved),document_id=str(q.uuid.uuid4())))
        r.case('settings.cross-fbm-roundtrip',q.require_ok(s.command('hydro_defaults'))['analysis']==now)
        q.require_ok(s.command('update_project',output=out/'imperial',changes={'units':'imperial'}))
        converted=q.require_ok(s.command('hydro_defaults'))['analysis']
        close(r,'settings.unit-vcg',converted['vcg']*.3048,1.75)
        close(r,'settings.unit-trim',converted['cross_trim']*.3048,.25)
        close(r,'settings.unit-mass',converted['displacements'][0]*1.0160469088,100)
    if probe:
        # Legacy FBM version promotion is observable through an independent
        # reader; the original source file must remain byte-identical.
        seed=fixture.run(probe,model,out/'legacy-seed','seed');old=Path(seed['artifacts']['legacy.fbm'])
        with q.Service(exe,out/'legacy-process') as s:
            q.require_ok(s.command('load',output=out/'legacy-load',path=str(old),document_id=str(q.uuid.uuid4())))
            q.require_ok(s.command('hydro_settings',output=out/'legacy-change',mode='table',start=.5,end=3,step=.5,trim=.2))
            saved=e.saved_copy(s,out/'legacy-save')
            q.require_ok(s.command('load',output=out/'legacy-reopen',path=str(saved),document_id=str(q.uuid.uuid4())))
            got=q.require_ok(s.command('hydro_defaults'))['analysis'];r.case('settings.legacy-persists',got['start']==.5 and got['end']==3 and got['step']==.5 and abs(got['trim']-.2)<1e-7)
