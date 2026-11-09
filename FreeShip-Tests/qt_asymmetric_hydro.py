"""Oriented asymmetric sections: independent solids, holes, multiple hulls and UI.

Prisms/rings are specified directly as control meshes. Reference values come
from elementary area/volume integrals, never the old mirror-only area routine.
All files and callbacks are isolated; no real desktop acceptance is implied.
"""
import argparse
import copy
import csv
import io
import json
import math
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_hydro_cases as checks
import qt_hydrostatics as ui

FILES=('qt_asymmetric_hydro.py','qt_hydro_cases.py','qt_hydrostatics.py','qt_migration.py','qt_editing.py','run_tests.py')


def prism(x0, x1, yz, layer=0):
    n=len(yz)
    points=[(x,y,z) for x in (x0,x1) for y,z in yz]
    faces=[list(reversed(range(n))),list(range(n,2*n))]
    faces += [[j,(j+1)%n,(j+1)%n+n,j+n] for j in range(n)]
    return points,[dict(points=f,layer=layer) for f in faces]


def ring():
    outer=[(0,-1),(10,-1),(10,3),(0,3)]
    inner=[(2,-.5),(8,-.5),(8,1.5),(2,1.5)]
    points=[(x,y,z) for z in (0,4) for loop in (outer,inner) for x,y in loop]
    faces=[]
    for j in range(4):
        k=(j+1)%4
        faces.extend([[j,k,k+8,j+8],[k+4,j+4,j+12,k+12],
                      [j,j+4,k+4,k],[j+8,k+8,k+12,j+12]])
    return points,[dict(points=f,layer=0) for f in faces]


def exchange(parts, layers=None):
    points=[];faces=[]
    for ps,fs in parts:
        offset=len(points);points.extend(ps)
        faces.extend(dict(f,points=[i+offset for i in f['points']]) for f in fs)
    edges=sorted({tuple(sorted((a,b))) for f in faces for a,b in zip(f['points'],f['points'][1:]+f['points'][:1])})
    return dict(format='FreeShipExchange',version=1,creator='Independent hydrostatic solid',particulars_set=True,
        project=dict(length=10,beam=4,draft=2,precision=0,units='metric',appendage_coefficient=1,
                     coefficient_basis=1,generate_auxiliary_reports=False,save_preview=False),
        layers=layers or [dict(name='Full hull',symmetric=False,use_in_hydrostatics=True)],
        points=[dict(p=p,vertex_type=3) for p in points],edges=[[a,b,True] for a,b in edges],faces=faces)


def rectangle_moments(rectangles):
    """Signed rectangles (x0,x1,y0,y1,weight), e.g. a moonpool has weight -1."""
    entries=[((x1-x0)*(y1-y0)*w,(x0+x1)/2,(y0+y1)/2,x1-x0,y1-y0) for x0,x1,y0,y1,w in rectangles]
    area=sum(a for a,_,_,_,_ in entries)
    x=sum(a*x for a,x,_,_,_ in entries)/area;y=sum(a*y for a,_,y,_,_ in entries)/area
    ix=sum(a*(b*b/12+(cy-y)**2) for a,_,cy,_,b in entries)
    iy=sum(a*(l*l/12+(cx-x)**2) for a,cx,_,l,_ in entries)
    return dict(waterplane_area=area,waterplane_ix=ix,waterplane_iy=iy),[x,y]


def backend(r,runtime,out):
    out.mkdir(parents=True)
    fixtures={
        'offset-box':exchange([prism(0,10,[(-1,0),(3,0),(3,4),(-1,4)])]),
        'two-hulls':exchange([prism(0,10,[(-4,0),(-2,0),(-2,4),(-4,4)]),prism(2,8,[(1,0),(5,0),(5,4),(1,4)])]),
        'moonpool':exchange([ring()]),
        'sloped':exchange([prism(0,10,[(-1,0),(3,0),(5,4),(-1,4)])]),
        'narrow-gap':exchange([prism(0,10,[(-2,0),(-1.01,0),(-1.01,4),(-2,4)]),prism(0,10,[(-.99,0),(0,0),(0,4),(-.99,4)])]),
    }
    mixed=exchange([prism(0,10,[(1,0),(2,0),(2,4),(1,4)],0),prism(2,8,[(4,0),(5,0),(5,4),(4,4)],1)],
        [dict(name='Mirrored pair',symmetric=True,use_in_hydrostatics=True),dict(name='One actual hull',symmetric=False,use_in_hydrostatics=True)])
    fixtures['mixed-layers']=mixed
    warped=copy.deepcopy(fixtures['offset-box']);warped['points'][6]['p']=(10,5,4)
    fixtures['warped-face']=warped
    reference={
        'offset-box':([(0,10,-1,3,1)],8,[8,8,8]),
        'two-hulls':([(0,10,-4,-2,1),(2,8,1,5,1)],12,[4,12,4]),
        'moonpool':([(0,10,-1,3,1),(2,8,-.5,1.5,-1)],8,[8,4,8]),
        'mixed-layers':([(0,10,1,2,1),(0,10,-2,-1,1),(2,8,4,5,1)],6,[4,6,4]),
        'narrow-gap':([(0,10,-2,-1.01,1),(0,10,-.99,0,1)],3.96,[3.96]*3),
    }
    models={};base_rows={};timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**params):return q.require_ok(s.command(method,output=out/name,**params))
        def calc(name,**params):
            state=dict(s.state);response=call(name,'hydrostatics',mode='single',draft=params.pop('draft',2),heel=params.pop('heel',0),trim=params.pop('trim',0),**params)
            result=json.loads(Path(response['hydro']).read_text('utf8'));row=result['rows'][0]
            r.case(name+'.readonly',s.state==state)
            r.case(name+'.valid',row['status']=='ok',row=row)
            if row['status']!='ok':raise RuntimeError(name+': '+row.get('message','missing result'))
            timings.append(dict(name=name,elapsed_ms=result['elapsed_ms'],sections=result['section_evaluations']))
            return row,result
        def close(name,a,b):checks.close(r,name,a,b,rel=3e-5,absolute=3e-5)
        for name,data in fixtures.items():
            path=out/(name+'.json');path.write_text(json.dumps(data),encoding='utf8')
            original=call(name+'-import','import_model',path=str(path),document_id=str(q.uuid.uuid4()))
            call(name+'-clear','sections',kind=1,operation='clear')
            call(name+'-stations','sections',kind=1,operation='add',positions=[1,5,9])
            model=out/(name+'.fbm');shutil.copy2(call(name+'-save','save')['saved_file'],model);models[name]=model
            before=call(name+'-before','snapshot');row,result=calc(name)
            base_rows[name]=row
            r.case(name+'.new-method',row['area_method']=='oriented-mesh-sections')
            r.case(name+'.projection-method',row['lateral_method']=='clipped-mesh-projection-union' and row['lateral_area']>0)
            r.case(name+'.one-sided-entrance-unavailable',row['entrance_angle'] is None and row['partial'])
            if name in reference:
                rectangles,mid,stations=reference[name];moments,center=rectangle_moments(rectangles)
                a=moments['waterplane_area']
                for key,value in dict(moments,volume=2*a,midship_area=mid,kmt=1+moments['waterplane_ix']/(2*a),kml=1+moments['waterplane_iy']/(2*a)).items():close(name+'.'+key,row[key],value)
                for axis,v in enumerate(center+[2]):close(name+'.cog-'+str(axis),row['waterplane_cog'][axis],v)
                for i,value in enumerate(stations):close(name+'.section-'+str(i),row['sectional_areas'][i][1],value)
                length=max(x1 for _,x1,_,_,_ in rectangles)-min(x0 for x0,_,_,_,_ in rectangles)
                beam=max(y1 for _,_,_,y1,_ in rectangles)-min(y0 for _,_,y0,_,_ in rectangles)
                for key,value in dict(cb=a/(length*beam),cm=mid/(beam*2),cp=2*a/(mid*length),cwp=a/(length*beam),cvp=1).items():close(name+'.'+key,row[key],value)
            elif name=='sloped':
                moments,center=rectangle_moments([(0,10,-1,4,1)])
                for key,value in dict(moments,volume=90,midship_area=9,kmt=28/27+moments['waterplane_ix']/90).items():close(name+'.'+key,row[key],value)
            cached,_=calc(name+'-cached');r.case(name+'.repeat-exact',cached==row)
            after=call(name+'-after','snapshot')
            for artifact in ('model','scene','dxf'):
                r.compare(name+'.unchanged-'+artifact,after[artifact],before[artifact])
            r.compare(name+'.unchanged-fbm',call(name+'-resave','save')['saved_file'],model)

        # Independent differential identity: d(displaced volume)/d(horizontal
        # water level) equals waterplane area. Richardson removes the cubic
        # volume term while staying away from mesh vertices at integral heights.
        call('warped-reload','load',path=str(models['warped-face']),document_id=str(q.uuid.uuid4()))
        center,_=calc('warped-center',draft=1.75);derivatives=[]
        for step in (.125,.0625):
            lo,_=calc('warped-low-'+str(step),draft=1.75-step)
            hi,_=calc('warped-high-'+str(step),draft=1.75+step)
            derivatives.append((hi['volume']-lo['volume'])/(2*step))
        close('warped.area-volume-identity',center['waterplane_area'],(4*derivatives[1]-derivatives[0])/3)

        # The rectangle on a heeled/trimmed waterplane is a parallelogram in
        # the horizontal frame. Its two basis vectors give centroidal moments.
        call('box-reload','load',path=str(models['offset-box']),document_id=str(q.uuid.uuid4()))
        for heel,trim in ((10,0),(-10,0),(0,.4),(8,.4)):
            name=f'tilt-{heel}-{trim}';row,_=calc(name,heel=heel,trim=trim)
            phi=math.radians(heel);theta=math.atan(trim*math.cos(phi)/10)
            cp,sp,ct,st=math.cos(phi),math.sin(phi),math.cos(theta),math.sin(theta)
            def rotate(x,y,z):return [x*ct+(-y*sp+z*cp)*st,y*cp+z*sp,-x*st+(-y*sp+z*cp)*ct]
            # Positive heel gives z=T+y*tan(phi), y'=y*cos(phi)+z*sin(phi).
            u=rotate(10,0,trim);v=rotate(0,4,4*math.tan(phi));center=rotate(5,1,2+math.tan(phi))
            area=abs(u[0]*v[1]-v[0]*u[1]);ix=area*(u[1]**2+v[1]**2)/12;iy=area*(u[0]**2+v[0]**2)/12
            for key,value in dict(waterplane_area=area,waterplane_ix=ix,waterplane_iy=iy,volume=40*(2+math.tan(phi))).items():close(name+'.'+key,row[key],value)
            for axis,value in enumerate(center):close(name+'.center-'+str(axis),row['waterplane_cog'][axis],value)

        # Translation and physical unit conversion must not alter centroidal I.
        call('translate-reload','load',path=str(models['offset-box']),document_id=str(q.uuid.uuid4()))
        snap=json.loads(Path(call('translate-snapshot','snapshot')['model']).read_text('utf8'))
        call('translate','move_points',moves=[dict(point_id=p['id'],position=[p['p'][0]+1000,p['p'][1]-20,p['p'][2]+7]) for p in snap['control_points']])
        row,_=calc('translated')
        for key in ('volume','waterplane_area','waterplane_ix','waterplane_iy','kmt','kml','midship_area'):close('translated.'+key,row[key],base_rows['offset-box'][key])
        for axis,value in enumerate([1005,-19,2]):close('translated.center-'+str(axis),row['waterplane_cog'][axis],value)
        call('units-reload','load',path=str(models['offset-box']),document_id=str(q.uuid.uuid4()))
        call('units','update_project',changes={'units':'imperial'});row,_=calc('imperial',draft=2/.3048)
        for key,power in [('volume',3),('waterplane_area',2),('waterplane_ix',4),('waterplane_iy',4),('kmt',1),('kml',1),('midship_area',2)]:close('imperial.'+key,row[key]*.3048**power,base_rows['offset-box'][key])

        # All waterlines use the same signed station cuts, including moonpool
        # subtraction. No symmetry correction or invented closing bridge.
        call('aux-reload','load',path=str(models['moonpool']),document_id=str(q.uuid.uuid4()))
        call('aux-waterlines-clear','sections',kind=3,operation='clear');call('aux-waterlines','sections',kind=3,operation='add',positions=[1,2,3])
        call('aux-enable','update_project',changes={'generate_auxiliary_reports':True})
        row,result=calc('auxiliary');aux=result['auxiliary']
        r.case('auxiliary.complete',aux['status']=='ok',result=aux)
        if aux['status']=='ok':
            for water in aux['waterlines']:
                z=water['waterline_z']
                for i,value in enumerate([4*z,2*z,4*z]):
                    close(f'auxiliary.{z}.{i}.sac',water['sac_areas'][i],value)
                    close(f'auxiliary.{z}.{i}.bonjean',water['bonjean_areas'][i],value)
                r.case('auxiliary.no-mirror-correction',not water['sac_correction'])
        model=out/'moonpool-aux.fbm';shutil.copy2(call('aux-save','save')['saved_file'],model);models['moonpool-aux']=model
        # Geometry edits and undo must invalidate then recover the section cache.
        call('undo-base','load',path=str(models['offset-box']),document_id=str(q.uuid.uuid4()))
        row,_=calc('undo-before');snap=json.loads(Path(call('undo-snapshot','snapshot')['model']).read_text('utf8'))
        call('widen','move_points',moves=[dict(point_id=p['id'],position=[p['p'][0],p['p'][1]+1,p['p'][2]]) for p in snap['control_points'] if p['p'][1]==3])
        changed,_=calc('widened');close('widened.area',changed['waterplane_area'],50)
        call('undo','undo');restored,_=calc('undo-restored');r.case('undo.exact',restored==row)
        call('ends-clear','sections',kind=1,operation='clear');call('ends-add','sections',kind=1,operation='add',positions=[0,5,10])
        ends,_=calc('coincident-end-planes')
        for i,area in enumerate(ends['sectional_areas']):close('coincident-end-'+str(i),area[1],8)
        submerged,_=calc('fully-submerged',draft=5)
        r.case('fully-submerged.no-invented-waterplane',submerged['waterplane_area']==0 and submerged['waterplane_ix']==0 and submerged['waterplane_iy']==0)
        for precision in (1,2,3):
            call('precision-'+str(precision),'update_project',changes={'precision':precision})
            precise,_=calc('precision-check-'+str(precision))
            for key in ('waterplane_area','waterplane_ix','waterplane_iy','midship_area'):close('precision-'+str(precision)+'.'+key,precise[key],base_rows['offset-box'][key])
        # Bad orientation and open immersed boundaries must not turn into a
        # positive-looking signed-area result. Reverse one face, then all faces.
        for label in ('one-reversed-face','all-reversed','open-bottom'):
            data=copy.deepcopy(fixtures['offset-box'])
            if label=='one-reversed-face':data['faces'][2]['points'].reverse()
            elif label=='all-reversed':
                for face in data['faces']:face['points'].reverse()
            else:data['faces'].pop(2)
            path=out/(label+'.json');path.write_text(json.dumps(data),encoding='utf8')
            call(label+'-import','import_model',path=str(path),document_id=str(q.uuid.uuid4()))
            state=dict(s.state);reply=call(label+'-calc','hydrostatics',mode='single',draft=2,heel=0,trim=0)
            result=json.loads(Path(reply['hydro']).read_text('utf8'))['rows'][0]
            r.case(label+'.explicit-error',result['status']=='error' and bool(result['message']) and 'waterplane_area' not in result,result=result)
            r.case(label+'.readonly',s.state==state)
        ordered=copy.deepcopy(fixtures['offset-box']);ordered['faces'].reverse()
        path=out/'reordered.json';path.write_text(json.dumps(ordered),encoding='utf8');call('reordered-import','import_model',path=str(path),document_id=str(q.uuid.uuid4()))
        reordered,_=calc('reordered')
        for key in ('waterplane_area','waterplane_ix','waterplane_iy','midship_area','kmt','kml'):close('reordered.'+key,reordered[key],base_rows['offset-box'][key])
        # An empty asymmetric layer is not a participating hull. It must not
        # switch the already verified symmetric integration to a new method.
        ps,fs=prism(0,10,[(0,0),(2,0),(2,4),(0,4)]);fs.pop()
        half=exchange([(ps,fs)],[dict(name='Half hull',symmetric=True,use_in_hydrostatics=True)])
        rows=[]
        for extra in (False,True):
            if extra:half['layers'].append(dict(name='Empty',symmetric=False,use_in_hydrostatics=True))
            path=out/('empty-layer-'+str(extra)+'.json');path.write_text(json.dumps(half),encoding='utf8')
            call('empty-layer-import-'+str(extra),'import_model',path=str(path),document_id=str(q.uuid.uuid4()))
            row,_=calc('empty-layer-'+str(extra));rows.append(row)
        r.case('empty-asymmetric-layer.preserves-symmetric-path',rows[0]==rows[1] and 'area_method' not in rows[1])
    r.data['timings']=timings
    return models


def production(r,runtime,out,model):
    request=ui.production_request(model,out,'single',save_auxiliary=True)
    data=json.loads(request.read_text('utf8'));data['inputs'].update(draft=2,heel=0);request.write_text(json.dumps(data),encoding='utf8')
    child=q.run_child(r,'asymmetric.production',runtime/'FreeShipQt.exe',['--verify-hydro',request],out/'workflow',timeout=240)
    r.data.setdefault('production_runs',[]).append(child)
    if child['status'] in ('fail','error'):raise RuntimeError('Production callback failed')
    art=child['artifacts'];report=json.loads(Path(art['report_json']).read_text('utf8'));row=report['rows'][0]
    r.case('production.actual-native-results',row['status']=='ok' and row['waterplane_area']==28 and row['kmt'] is not None)
    r.case('production.actual-lateral-projection',row['lateral_area']==20 and row['lateral_cog']==[5,0,1])
    r.case('production.auxiliary',report['auxiliary']['status']=='ok')
    table=list(csv.reader(io.StringIO(Path(art['report_csv']).read_text('utf-8-sig'))))
    for caption,key in [('水线面面积','waterplane_area'),('水线面 Ix','waterplane_ix'),('横稳心高度 KMt','kmt'),('侧投影面积','lateral_area')]:
        cols=[i for i,label in enumerate(table[0]) if label.startswith(caption+' (')]
        r.case('production.csv-'+key,len(cols)==1 and float(table[1][cols[0]])==row[key])
    cols=[i for i,label in enumerate(table[0]) if label.startswith('漂心 Y (')]
    r.case('production.csv-transverse-flotation',len(cols)==1 and float(table[1][cols[0]])==row['waterplane_cog'][1])
    for kind in ('scene','dxf'):r.compare('production.readonly-'+kind,art['original_'+kind],art['after_'+kind])
    ui.pdf_check(r,'production',Path(art['report_pdf']))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--no-production',action='store_true');a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use private output outside source trees')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.perf_counter();sources=q.current_sources(q.QT)
    r.data.update(suite='qt-asymmetric-hydro',runtime=str(runtime),test_sources_sha256={f:q.common.digest_file(q.ROOT/f) for f in FILES},limitations=['Actual mesh sections and lateral projection; the one-sided entrance angle remains unavailable.','Hidden Qt callbacks do not certify desktop input, focus or visual acceptance.'])
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);models=backend(r,runtime,out/'backend')
        if not a.no_production:production(r,runtime,out/'production',models['moonpool-aux'])
        r.case('sources-unchanged',q.current_sources(q.QT)==sources)
        r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.case('desktop-acceptance',status='not_run',reason='No desktop input or device acceptance.');r.data['elapsed_s']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
