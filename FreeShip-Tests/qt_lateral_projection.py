"""Actual XZ projection union and four-view hydrostatic labels.

Independent rectangle/polygon integrals distinguish occluded moonpools from
through-Y holes, and overlapping hulls from disjoint silhouettes. Production
callbacks exercise the existing action workflow; screenshots are evidence, not
desktop acceptance. All inputs, outputs and settings are isolated.
"""
import argparse
import copy
import json
import math
import shutil
import threading
import time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_hydro_cases as checks
from qt_asymmetric_hydro import prism, ring, exchange
from qt_project_hydro import FIELDS

FILES=('qt_lateral_projection.py','qt_asymmetric_hydro.py','qt_hydro_cases.py','qt_project_hydro.py','qt_migration.py','qt_editing.py','run_tests.py')


def read(path):return json.loads(Path(path).read_text('utf8'))


def rectangles(items):
    """Independent signed rectangle inclusion/exclusion in the XZ plane."""
    parts=[((x1-x0)*(z1-z0)*sign,(x0+x1)/2,(z0+z1)/2) for x0,x1,z0,z1,sign in items]
    area=sum(a for a,_,_ in parts)
    return area,[sum(a*x for a,x,_ in parts)/area,0,sum(a*z for a,_,z in parts)/area]


def box(x0=0,x1=10,y0=-1,y1=3,z0=0,z1=4,layer=0):
    return prism(x0,x1,[(y0,z0),(y1,z0),(y1,z1),(y0,z1)],layer)


def fixtures():
    positive=[(0,10,0,2,1)]
    through_points,through_faces=ring()
    # Swap original Y/Z, reversing winding to compensate for reflection.
    through=([(x,z-2,y+1) for x,y,z in through_points],
             [dict(f,points=list(reversed(f['points']))) for f in through_faces])
    layer=lambda name,symmetric,hydro,visible=True:dict(name=name,symmetric=symmetric,use_in_hydrostatics=hydro,visible=visible)
    mixed=exchange([box(y0=1,y1=2,layer=0),box(x0=2,x1=8,y0=4,y1=5,layer=1),box(x0=-100,x1=100,y0=7,y1=8,layer=2)],
        [layer('Mirrored',True,True),layer('Hidden participating',False,True,False),layer('Excluded',False,False)])
    return {
        'offset':(exchange([box()]),positive),
        'overlap-y':(exchange([box(y0=-4,y1=-2),box(x0=2,x1=8,y0=1,y1=5)]),positive),
        'overlap-xz':(exchange([box(x1=6,y0=-4,y1=-2),box(x0=4,y0=1,y1=3,z0=1)]),[(0,6,0,2,1),(4,10,1,2,1),(4,6,1,2,-1)]),
        'disjoint-x':(exchange([box(x1=4),box(x0=6)]),[(0,4,0,2,1),(6,10,0,2,1)]),
        'narrow-x-gap':(exchange([box(x1=4.999),box(x0=5.001)]),[(0,4.999,0,2,1),(5.001,10,0,2,1)]),
        'touching-x':(exchange([box(x1=5,y0=-4,y1=-2),box(x0=5)]),positive),
        'moonpool':(exchange([ring()]),positive),
        'through-y':(exchange([through]),[(0,10,0,2,1),(2,8,.5,2,-1)]),
        'mixed-layers':(mixed,positive),
    }


def backend(r,runtime,out):
    out.mkdir(parents=True);models={};timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**params):return q.require_ok(s.command(method,output=out/name,**params))
        def load(name,data):
            path=out/(name+'.json');path.write_text(json.dumps(data),encoding='utf8')
            return call(name+'-import','import_model',path=str(path),document_id=str(q.uuid.uuid4()))
        def close(name,got,wanted,absolute=3e-5):checks.close(r,name,got,wanted,rel=3e-5,absolute=absolute)
        def calc(name,**inputs):
            state=dict(s.state);started=time.perf_counter()
            response=call(name,'hydrostatics',**dict(dict(mode='single',draft=2,heel=0,trim=0),**inputs))
            result=read(response['hydro']);row=result['rows'][0]
            r.case(name+'.readonly',s.state==state);r.case(name+'.valid',row['status']=='ok',row=row)
            if row['status']!='ok':raise RuntimeError(name+': '+row.get('message','Invalid hydrostatics'))
            timings.append(dict(name=name,calculation_ms=result['elapsed_ms'],wall_ms=(time.perf_counter()-started)*1000))
            return row
        def projected(name,row,area,cog,absolute=3e-5):
            close(name+'.area',row['lateral_area'],area,absolute)
            for i,value in enumerate(cog):close(name+'.cog-'+str(i),row['lateral_cog'][i],value,absolute)
        for name,(data,rects) in fixtures().items():
            before=load(name,data);row=calc(name)
            area,cog=rectangles(rects);projected(name,row,area,cog)
            r.case(name+'.actual-projection',row['lateral_method']=='clipped-mesh-projection-union')
            r.case(name+'.entrance-angle-not-invented',row['entrance_angle'] is None)
            again=calc(name+'-cached');r.case(name+'.cache-exact',row==again)
            after=call(name+'-snapshot','snapshot')
            for artifact in ('model','scene','dxf'):r.compare(name+'.readonly-'+artifact,before[artifact],after[artifact])
            model=out/(name+'.fbm');shutil.copy2(call(name+'-save','save')['saved_file'],model);models[name]=model
            call(name+'-reopen','load',path=str(model),document_id=str(q.uuid.uuid4()))
            r.case(name+'.reopen-exact',calc(name+'-reopened')==row)

        ps,fs=prism(-1,3,[(0,0),(10,0),(0,4)])
        triangular=exchange([([(y,x,z) for x,y,z in ps],[dict(f,points=list(reversed(f['points']))) for f in fs])])
        load('triangle-profile',triangular)
        # Integrate width w(z)=10-2.5z over 0..2: A=15,
        # first X moment=int(w*w/2)=175/3, first Z moment=40/3.
        projected('triangle-profile',calc('triangle-profile'),15,[35/9,0,8/9])

        call('tilt-load','load',path=str(models['offset']),document_id=str(q.uuid.uuid4()))
        for heel,trim in ((10,0),(-10,0),(0,.4),(8,.4),(-8,-.4)):
            # Projection onto body XZ: the wet silhouette reaches the higher
            # waterline edge in Y. Its upper edge is a linear wedge in X.
            h=2+math.tan(math.radians(heel))*(3 if heel>0 else -1)
            a=h-trim/2;b=trim/10;length=10;area=length*h
            cx=(a*length**2/2+b*length**3/3)/area
            cz=(a*a*length+a*b*length**2+b*b*length**3/3)/(2*area)
            name=f'tilt-{heel}-{trim}';projected(name,calc(name,heel=heel,trim=trim),area,[cx,0,cz])

        # Revisions must replace both cached wet and dry silhouettes. Undo
        # restores the exact former snapshot and bytes, not just a close value.
        initial=calc('edit-before');before=call('edit-snapshot','snapshot');snapshot=read(before['model'])
        saved_before=Path(call('edit-save','save')['saved_file']).read_bytes()
        call('lengthen','move_points',moves=[dict(point_id=p['id'],position=[12,p['p'][1],p['p'][2]]) for p in snapshot['control_points'] if p['p'][0]==10])
        projected('edited',calc('edited'),24,[6,0,1])
        call('undo','undo');r.case('undo.numerical-exact',calc('restored')==initial)
        r.case('undo.fbm-exact',Path(call('undo-save','save')['saved_file']).read_bytes()==saved_before)
        r.case('undo.overlay-exact',read(call('undo-snapshot','snapshot')['model'])['overlay']==snapshot['overlay'])

        # Scale range exercises the normalized integer grid. Translation tests
        # recentering and report keel-relative Z independently of display Z.
        for factor in (.0001,10000):
            data=exchange([box()])
            for p in data['points']:p['p']=[v*factor for v in p['p']]
            for key in ('length','beam','draft'):data['project'][key]*=factor
            name='scale-'+str(factor);load(name,data)
            projected(name,calc(name,draft=2*factor),20*factor**2,[5*factor,0,factor],absolute=1e-12)
        call('units-load','load',path=str(models['offset']),document_id=str(q.uuid.uuid4()))
        call('units','update_project',changes={'units':'imperial'});row=calc('units',draft=2/.3048)
        projected('units',dict(row,lateral_area=row['lateral_area']*.3048**2,lateral_cog=[v*.3048 for v in row['lateral_cog']]),20,[5,0,1])

        shifted=exchange([box(x0=1000,x1=1010,y0=9,y1=13,z0=7,z1=11)])
        shifted['project'].update(dict.fromkeys(FIELDS,True))
        load('shifted',shifted)
        call('stations-clear','sections',kind=1,operation='clear');call('stations','sections',kind=1,operation='add',positions=[1001,1005,1009])
        row=calc('shifted');projected('shifted',row,20,[1005,0,1])
        full=call('shifted-all','snapshot');hydro=read(full['model'])['overlay']['hydro'];markers={m['key']:m for m in hydro['markers']}
        r.case('shifted.overlay-available',hydro['available'])
        expected={'displacement':[1005,11,8],'km':[1005,11,7+5/3],'lcf':[1005,11,9],'lateral':[1005,0,8],'wind':[1005,0,10]}
        for key,point in expected.items():
            r.case('shifted.'+key+'.present',key in markers)
            for axis,value in enumerate(point):close('shifted.'+key+'.p-'+str(axis),markers[key]['p'][axis],value)
        r.case('shifted.km-caption-above-keel',markers['km']['text']=='KM=1.67')
        r.case('shifted.areas-and-curve',len(hydro['areas'])==3 and len(hydro['path'])>0)
        for area in hydro['areas']:close('shifted.station-area',area['value'],8)
        model=out/'shifted.fbm';shutil.copy2(call('shifted-save','save')['saved_file'],model);models['shifted']=model
        keys=[{'displacement','weight'},{'lateral','wind'},set(),{'km'},{'lcf'}]
        for field,wanted in zip(FIELDS,keys):
            response=call(field,'update_project',changes=dict(dict.fromkeys(FIELDS,False),**{field:True}))
            h=read(response['model'])['overlay']['hydro'];seen={m['key'] for m in h['markers']}
            # Weight is absent when the specified layer material has no mass.
            r.case(field+'.requested-only',h['available'] and seen==wanted-({'weight'} if 'weight' not in markers else set()))
            r.case(field+'.curve-selected',bool(h['areas'])==bool(h['path'])==(field=='hydro_sectional_areas'))
            for artifact in ('scene','dxf','surface','sections'):r.compare(field+'.unchanged-'+artifact,full[artifact],response[artifact])
        response=call('off','update_project',changes=dict.fromkeys(FIELDS,False));h=read(response['model'])['overlay']['hydro']
        r.case('overlay.all-off',h['available'] and not any(h[k] for k in ('markers','areas','path')))

        call('tunnel-load','load',path=str(models['through-y']),document_id=str(q.uuid.uuid4()))
        response=call('tunnel-overlay','update_project',changes=dict.fromkeys(FIELDS,True));h=read(response['model'])['overlay']['hydro']
        m={p['key']:p for p in h['markers']}
        for key,area,cog in [('lateral',11,[5,0,8.75/11]),('wind',17,[5,0,53.25/17])]:
            r.case('tunnel.'+key+'.caption',m[key]['text'].endswith('='+f'{area:.2f}'))
            for axis,value in enumerate(cog):close('tunnel.'+key+'.p-'+str(axis),m[key]['p'][axis],value)
        call('submerged-load','load',path=str(models['offset']),document_id=str(q.uuid.uuid4()))
        for draft in (4,5):
            response=call('submerged-'+str(draft),'update_project',changes=dict(draft=draft,**dict.fromkeys(FIELDS,True)))
            h=read(response['model'])['overlay']['hydro']
            r.case('submerged-'+str(draft)+'.no-fake-wind',h['available'] and 'wind' not in {p['key'] for p in h['markers']})
            projected('submerged-'+str(draft),calc('submerged-row-'+str(draft),draft=draft),40,[5,0,2])
        call('precision-draft','update_project',changes={'draft':2})
        for precision in (1,2,3):
            call('precision-'+str(precision),'update_project',changes={'precision':precision})
            projected('precision-'+str(precision),calc('precise-'+str(precision)),20,[5,0,1])
        cancel=out/'cancel';cancel.mkdir();state=dict(s.state);signalled=threading.Event()
        def signal():(cancel/'cancel.flag').write_text('cancel');signalled.set()
        timer=threading.Timer(.05,signal);timer.start()
        try:response=s.command('hydrostatics',output=cancel,mode='table',start=.1,end=3.9,step=.02,trim=.1)
        finally:timer.cancel();timer.join()
        if response['ok']:
            result=read(response['hydro'])
            r.case('cancellation.interrupted',result['cancelled'] and len(result['rows'])<191,signalled=signalled.is_set(),rows=len(result['rows']))
        else:r.case('cancellation.before-first-row',signalled.is_set(),error=response.get('error'))
        r.case('cancellation.readonly',s.state==state)
        projected('after-cancel',calc('after-cancel'),20,[5,0,1])
        # The numeric translation case above deliberately places its XZ
        # projection (Y=0) outside the body/plan viewport fitted around Y=11.
        # A separate visible-marker fixture keeps all anchors inside each view;
        # it tests rendering, without requiring off-screen labels to be shown.
        visible=exchange([box(x0=1000,x1=1010,z0=7,z1=11)])
        visible['project'].update(dict.fromkeys(FIELDS,True));load('visible-markers',visible)
        call('visible-stations-clear','sections',kind=1,operation='clear');call('visible-stations','sections',kind=1,operation='add',positions=[1001,1005,1009])
        model=out/'visible-markers.fbm';shutil.copy2(call('visible-save','save')['saved_file'],model);models['visible-markers']=model
    r.data['timings']=timings
    return models


def production(r,runtime,out,model):
    out.mkdir();private=out/'input.fbm';shutil.copy2(model,private);before=private.read_bytes()
    child=q.run_child(r,'overlay-production',runtime/'FreeShipQt.exe',['--verify-overlays',private],out/'workflow',timeout=240,platform='offscreen')
    r.data.setdefault('production_runs',[]).append(child)
    artifacts=child.get('artifacts',{})
    original=read(artifacts['original_model']);moved=read(artifacts['moved_model']);undo=read(artifacts['undo_model']);redo=read(artifacts['redo_model'])
    r.case('production.overlay-available',original['overlay']['hydro']['available'])
    r.case('production.undo-overlay-exact',undo['overlay']==original['overlay'])
    r.case('production.redo-overlay-exact',redo['overlay']==moved['overlay'])
    r.case('production.edit-updates-overlay',moved['overlay']['hydro']!=original['overlay']['hydro'])
    r.case('production.original-untouched',private.read_bytes()==before)
    evidence=read(out/'workflow/overlay-evidence.json')
    r.data['overlay_evidence']=evidence
    r.case('production.four-views',len(evidence)==4)
    # View evidence describes the renderer's actual projected label set, not
    # fixed menu locations or object names. Check the hydro marker identifiers.
    for i,view in enumerate(evidence):
        encoded=json.dumps(view)
        for key in ('displacement','km','lcf','lateral','wind'):
            r.case(f'production.view-{i}.{key}','hydro:'+key in encoded)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--no-production',action='store_true');a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use private output outside source trees')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.perf_counter();sources=q.current_sources(q.QT)
    r.data.update(suite='qt-lateral-projection',runtime=str(runtime),test_sources_sha256={f:q.common.digest_file(q.ROOT/f) for f in FILES},limitations=['Projection of participating hydrostatic mesh only; no claim of complete superstructure windage.','Hidden callbacks and rendered images do not certify desktop input or full visual acceptance.'])
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        origin=read(q.QT/'third_party/clipper2/origin.json')
        r.case('clipper.pinned-source',all(q.common.digest_file(q.QT/'third_party/clipper2'/n)==h for n,h in origin['files_sha256'].items()))
        r.case('clipper.packaged-source-and-license',all((runtime/'third_party/clipper2'/n).is_file() and q.common.digest_file(runtime/'third_party/clipper2'/n)==h for n,h in origin['files_sha256'].items()))
        r.case('runtime-manifest',True);models=backend(r,runtime,out/'backend')
        if not a.no_production:production(r,runtime,out/'production',models['visible-markers'])
        r.case('sources-unchanged',q.current_sources(q.QT)==sources)
        r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.case('desktop-acceptance',status='not_run',reason='No desktop input or device acceptance.');r.data['elapsed_s']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
