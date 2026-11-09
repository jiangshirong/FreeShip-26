"""Actual STL export against native geometry and independent reflection checks.

Only the file chooser and program entry are replaced in an isolated source copy.
No model, preference or executable in the user's workspace is written by the probe.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

import freeship_ui_test as common
import file_safety as build

ROOT=Path(__file__).resolve().parent
FILES=('stl_symmetry.py','StlSymmetryProbe.pas','file_safety.py','freeship_ui_test.py')

def hashes():
    return {name:common.digest_file(ROOT/name) for name in FILES}

def facet_key(points):
    vertices=tuple(tuple(round(float(x),4) for x in p) for p in points)
    return min(vertices,vertices[1:]+vertices[:1],vertices[2:]+vertices[:2])

def reflected(points):
    return tuple((p[0],-p[1],p[2]) for p in reversed(points))

def facets(path):
    lines=path.read_text(encoding='utf8').splitlines()
    if not lines or lines[0].strip()!='solid' or lines[-1].strip()!='endsolid':
        raise ValueError('STL is incomplete')
    result=[];i=1
    while i<len(lines)-1:
        row=[line.strip().split() for line in lines[i:i+7]]
        if len(row)!=7 or row[0][:2]!=['facet','normal'] or row[1]!=['outer','loop'] or row[5]!=['endloop'] or row[6]!=['endfacet']:
            raise ValueError('Invalid ASCII STL facet structure')
        if any(line[0]!='vertex' or len(line)!=4 for line in row[2:5]):
            raise ValueError('Invalid STL vertex')
        result.append((tuple(map(float,row[0][2:])),tuple(tuple(map(float,line[1:])) for line in row[2:5])))
        i+=7
    return result

def unit_normal(points):
    a,b,c=points; u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
    n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
    length=math.sqrt(sum(x*x for x in n))
    if not length:raise ValueError('Degenerate exported triangle')
    return tuple(x/length for x in n)

def add(cases,name,expected,ok,actual):
    cases.append({'id':name,'expected':expected,'status':'pass' if ok else 'fail','actual':actual})

def inspect(evidence):
    raw=json.loads((evidence/'geometry.json').read_text(encoding='utf-8-sig'))
    if raw.get('error'):raise RuntimeError(raw['error'])
    names={'half-asymmetric','half-symmetric','center-only','mixed-layers','whole-model','negative-half','near-center'}
    if {x['name'] for x in raw['fixtures']}!=names or len(raw['fixtures'])!=len(names):
        raise RuntimeError('Fixture evidence is incomplete')
    cases=[];by_name={}
    for fixture in raw['fixtures']:
        name=fixture['name'];expected=Counter();original=Counter();mirrors=Counter()
        for layer in fixture['layers']:
            if not layer['visible']:continue
            for polygon in layer['polygons']:
                for i in range(1,len(polygon)-1):
                    triangle=(polygon[0],polygon[i],polygon[i+1]);key=facet_key(triangle)
                    expected[key]+=1;original[key]+=1
                    if layer['symmetric'] and any(p[1]!=0 for p in triangle):
                        key=facet_key(reflected(triangle));expected[key]+=1;mirrors[key]+=1
        exported=facets(evidence/(name+'.stl'))
        actual=Counter(facet_key(p) for n,p in exported);by_name[name]=actual
        add(cases,'13.'+name+'.geometry','visible source triangles and only the required reflected triangles, preserving winding',
            actual==expected,{'expected_facets':sum(expected.values()),'actual_facets':len(exported),
                              'missing':sum((expected-actual).values()),'unexpected':sum((actual-expected).values())})
        errors=[max(abs(x-y) for x,y in zip(n,unit_normal(p))) for n,p in exported]
        add(cases,'13.'+name+'.normals','each normal agrees with the exported vertex winding and has unit length within ASCII precision',
            bool(errors) and max(errors)<0.0003 and all(abs(math.sqrt(sum(x*x for x in n))-1)<0.0002 for n,p in exported),
            {'max_component_error':max(errors,default=None)})
        physical=Counter(tuple(sorted(key)) for normal,points in exported for key in (facet_key(points),))
        add(cases,'13.'+name+'.duplicates','no triangle is duplicated in these disjoint fixture surfaces, including opposite winding',
            bool(physical) and max(physical.values())==1,{'maximum_multiplicity':max(physical.values(),default=0)})
    half=by_name['half-asymmetric']
    whole=half+Counter({facet_key(reflected(key)):n for key,n in half.items()})
    add(cases,'13.symmetric-exact-reflection','enabling symmetry retains every original facet and adds exactly its reflection',
        by_name['half-symmetric']==whole,{'asymmetric':sum(half.values()),'symmetric':sum(by_name['half-symmetric'].values())})
    add(cases,'13.complete-model-preserved','an explicitly modelled full hull marked non-symmetric is exported once',
        by_name['whole-model']==whole,{'expected_facets':sum(whole.values()),'actual_facets':sum(by_name['whole-model'].values())})
    add(cases,'13.near-center-not-discarded','representable geometry close to the center plane retains both sides',
        any(p[1]==0.0001 for key in by_name['near-center'] for p in key) and
        any(p[1]==-0.0001 for key in by_name['near-center'] for p in key),{})
    return cases

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output',type=Path,default=ROOT/'reports'/('stl-symmetry-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout',type=int,default=240)
    ap.add_argument('--mutation',choices=('none','unconditional-mirror'),default='none')
    args=ap.parse_args();src=args.source.resolve();out=args.output.resolve()
    if out.exists() or out==src or src in out.parents:raise SystemExit('Output must be new and outside the production source')
    out.mkdir(parents=True)
    report={'status':'error','cases':[],'test_code_sha256':hashes(),'mutation':args.mutation,
            'limitations':['Actual in-process exporter with synthetic surfaces; no native file chooser, desktop interaction or external CAD application acceptance.',
                           'STL precision remains the existing four decimal places. Geometry below that output precision is not added by this repair.']}
    before=None
    try:
        before,count=common.fingerprint(src);report.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(args.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-stl-symmetry-') as run:
            work=run/'source';work.mkdir();evidence=run/'evidence';evidence.mkdir();profile=run/'profile';profile.mkdir()
            project,exe,snapshot=build.prepare(src,work,tools,False)
            if snapshot!=before or common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during snapshot')
            shutil.copy2(ROOT/'StlSymmetryProbe.pas',work/'Units'/'StlSymmetryProbe.pas')
            dpr=work/'FreeShip.dpr'
            build.replace(dpr,"  Main in 'Forms/Main.pas' {MainForm},","  StlSymmetryProbe in 'Units/StlSymmetryProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            build.replace(dpr,'  Application.Run;','  Application.ShowMainForm := False;\n  StlSymmetryProbe.RunStlSymmetryProbe(MainForm);')
            edit=work/'Units'/'FreeEdit.inc';text=edit.read_text(encoding='utf8')
            start=text.index('procedure TFreeEdit.File_ExportSTL;');end=text.index('end;{TFreeEdit.File_ExportSTL}',start)
            part=text[start:end];anchor='  if SaveDialog.Execute then'
            if part.count(anchor)!=1:raise RuntimeError('STL chooser adapter changed')
            part=part.replace(anchor,"  SaveDialog.FileName := SysUtils.GetEnvironmentVariable('FS_STL_TARGET');\n  if SaveDialog.FileName <> '' then")
            if args.mutation=='unconditional-mirror':
                anchor='if Layer.Symmetric and\n                  ((C1.Y <> 0.0) or (C2.Y <> 0.0) or (C3.Y <> 0.0)) then'
                if part.count(anchor)!=1:raise RuntimeError('Mutation site changed')
                part=part.replace(anchor,'if True then')
            edit.write_text(text[:start]+part+text[end:],encoding='utf8')
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_STL_OUTPUT=str(evidence))
            report['instrumented_source_sha256']=common.fingerprint(work)[0]
            report['compiler_sha256']=build.compile_source(project,tools,run,env,out,args.timeout)
            report['exe_sha256']=common.digest_file(exe)
            p=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=args.timeout,creationflags=common.FLAGS)
            (out/'runtime.log').write_bytes(p.stdout+b'\n'+p.stderr);shutil.copytree(evidence,out/'evidence')
            if p.returncode:raise RuntimeError('Exporter probe exited with '+str(p.returncode))
            report['cases']=inspect(evidence)
            report['status']=common.classify_statuses(c['status'] for c in report['cases'])
    except Exception as exc:
        report['status']='error';report['error']=f'{type(exc).__name__}: {exc}'
    if before and common.fingerprint(src)[0]!=before:
        report['status']='error';report['error']='Source changed during validation'
    else:report['source_unchanged_during_run']=True
    if hashes()!=report['test_code_sha256']:
        report['status']='error';report['error']='Test code changed during validation'
    report['summary']=dict(Counter(c['status'] for c in report['cases']))
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:report.get(k) for k in ('status','summary','error')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__':raise SystemExit(main())
