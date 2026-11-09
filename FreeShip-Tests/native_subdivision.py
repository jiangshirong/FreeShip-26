"""Isolated control-net/subdivision compatibility and independent geometry checks."""
from __future__ import annotations

import argparse
from collections import Counter,defaultdict
import copy
import json
import math
import os
from pathlib import Path
import re
import subprocess

import freeship_ui_test as common
import native_geometry as leaf
import native_subdivision_cases as fixtures
import native_subdivision_probe as oracle

ROOT=Path(__file__).resolve().parent
WORKSPACE=ROOT.parent
TEST_FILES=('native_subdivision.py','native_subdivision_cases.py','native_subdivision_probe.py',
    'NativeSubdivisionProbe.pas','qt/native_subdivision_tests.cpp','native_geometry.py',
    'native_geometry_cases.py','native_geometry_probe.py','freeship_ui_test.py','native_flicker_test.py','run_tests.py')
LEVEL_FIELDS=('level','points','faces','edges','curves','layers','loose_control_points','loose_control_edges','bounds','empty')


class Report:
    def __init__(self):
        self.data={'status':'error','cases':[],'scope':'Isolated native control-net and subdivision candidate; no production integration',
            'acceptance_policy':{
                'identity':'Point IDs follow control-face order, child order and oriented face-point traversal; curves follow afterward. Coordinates never select identity.',
                'topology':'Complete oriented face cycles, edge incidence, crease/curve identity, layers, loose objects and counts compare exactly.',
                'numbers':'Finite position/bounds/material values reconstruct Single from JSON and compare exactly. Signed-zero memory bits are not asserted. No numeric tolerance.',
                'independent_geometry':'Hand-derived boundary, Loop and Catmull-Clark examples are separate from Pascal compatibility. A shared mathematical discrepancy remains review, never automatic correctness.',
                'gate':'Any failure, error, untested requirement or review prevents claiming a production-ready replacement.'},
            'limitations':[
                'Only explicitly requested levels and fixtures are checked; this is not a proof over all control nets.',
                'The native candidate is not linked into the application. Editing, FBM/DXF export and GUI behavior are outside this suite.',
                'Original unused allocated subdivision objects are diagnostic metadata, not canonical mesh vertices.',
                'Mixed surface/free-standing curves and curves bridging surface points without a face edge are explicitly rejected; they are not migrated by this candidate.',
                'No UI timing or whole-application performance claim is made.']}

    def case(self,name,ok=None,*,status=None,**details):
        state=status or ('pass' if ok else 'fail')
        if state not in leaf.STATES:raise ValueError('Unknown result state')
        self.data['cases'].append({'id':name,'status':state,**details})

    def finish(self):
        counts=Counter(c['status'] for c in self.data['cases'])
        self.data['counts']={s:counts[s] for s in leaf.STATES}
        self.data['status']=next((s for s in ('error','fail','review','not_run') if counts[s]),'pass')
        comparisons=[c for c in self.data['cases'] if c['id'].endswith('.positions-pascal')]
        self.data['compatibility_totals']={k:sum(c.get(k,0) for c in comparisons) for k in
            ('scalar_count','reconstructed_single_equal_count','numeric_difference_count','structure_difference_count')}
        self.data['compatibility_totals']['level_count']=len(comparisons)
        self.data['compatible_replacement']=False


def validate_runtime(runtime,qt_source):
    manifest=json.loads((runtime/'native-build-manifest.json').read_text('utf-8-sig'))
    if manifest.get('target')!='native_subdivision_tests':raise RuntimeError('Wrong native target')
    if manifest['source_sha256']!=leaf.native_source_hashes(qt_source):raise RuntimeError('Native source manifest is stale')
    if manifest['test_source_sha256']!={'qt/native_subdivision_tests.cpp':common.digest_file(ROOT/'qt/native_subdivision_tests.cpp')}:
        raise RuntimeError('Native subdivision adapter is stale')
    actual=leaf.files_hashes(runtime);actual.pop('native-build-manifest.json',None)
    if actual!=manifest['binaries_sha256']:raise RuntimeError('Private runtime binary hash mismatch')
    if 'Qt6Widgets.dll' in actual:raise RuntimeError('This is a calculation-only suite')
    return manifest


def build_native(qt_source,out):
    runtime=out/'runtime'
    command=['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(qt_source/'build-native.ps1'),
        '-Target','native_subdivision_tests','-BuildDir',str(out/'native-build'),'-RuntimeDir',str(runtime),'-WorkspaceRoot',str(WORKSPACE)]
    with (out/'native-build.log').open('wb') as log:
        result=subprocess.run(command,cwd=WORKSPACE,stdout=log,stderr=subprocess.STDOUT,timeout=600,creationflags=common.FLAGS)
    if result.returncode:raise RuntimeError('Native compile failed; see native-build.log')
    return runtime


def validate_probe(folder,source,qt_source):
    report=json.loads((folder/'report.json').read_text('utf8'));exe=folder/'NativeSubdivisionProbe.exe'
    if report.get('status')!='pass' or report['source_sha256']!=common.fingerprint(source)[0]:raise RuntimeError('Pascal probe source is stale')
    if report['test_sha256']!={p:common.digest_file(ROOT/p) for p in ('NativeSubdivisionProbe.pas','native_subdivision_probe.py','native_geometry_probe.py')}:
        raise RuntimeError('Pascal probe observer is stale')
    if report['adapter_sha256']!=oracle._adapter(qt_source).adapter_hashes():raise RuntimeError('Private headless adapter is stale')
    if report['geometry_sha256']!=oracle.geometry_hashes(source):raise RuntimeError('Original subdivision includes differ')
    if report.get('executable_sha256')!=common.digest_file(exe):raise RuntimeError('Pascal probe executable hash mismatch')
    return exe,report


def native_run(runtime,input_path,out):
    out.mkdir(parents=True,exist_ok=False);environment=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();environment[key]=str(folder)
    before=common.digest_file(input_path)
    result=subprocess.run([str(runtime/'native_subdivision_tests.exe'),str(input_path),str(out/'results.json')],
        cwd=out,env=environment,capture_output=True,timeout=300,creationflags=common.FLAGS)
    (out/'native.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError('Native subdivision driver failed: '+str(out/'native.log'))
    if common.digest_file(input_path)!=before:raise RuntimeError('Native driver changed its input')
    report=json.loads((out/'results.json').read_text('utf8'))
    if report.get('format')!='FreeShipNativeSubdivisionResults' or report.get('version')!=1 or not isinstance(report.get('cases'),list):
        raise RuntimeError('Malformed native result')
    return report


def split_level(level):
    """Integer identity/topology is never compared as approximate coordinates."""
    topology={k:copy.deepcopy(level[k]) for k in LEVEL_FIELDS}
    numeric={'positions':[p.pop('position') for p in topology['points']],
        'loose_positions':[p.pop('position') for p in topology['loose_control_points']],
        'bounds':topology.pop('bounds'),
        'layer_materials':[{k:l.pop(k) for k in ('material_density','thickness')} for l in topology['layers']]}
    return topology,numeric


def split_geometry(geometry):
    topology=copy.deepcopy(geometry)
    numeric={'positions':[p.pop('position') for p in topology['points']],
             'layer_materials':[{k:l.pop(k) for k in ('material_density','thickness')} for l in topology['layers']]}
    return topology,numeric


def structural_differences(a,b,path='$',result=None):
    result=[] if result is None else result
    if type(a) is not type(b):result.append({'path':path,'type':'type','actual':type(a).__name__,'expected':type(b).__name__})
    elif isinstance(b,dict):
        if a.keys()!=b.keys():result.append({'path':path,'type':'keys','actual':list(a),'expected':list(b)})
        for key in a.keys()&b.keys():structural_differences(a[key],b[key],path+'.'+key,result)
    elif isinstance(b,list):
        if len(a)!=len(b):result.append({'path':path,'type':'length','actual':len(a),'expected':len(b)})
        for i,(x,y) in enumerate(zip(a,b)):structural_differences(x,y,path+f'[{i}]',result)
    elif a!=b:result.append({'path':path,'type':'value','actual':a,'expected':b})
    return result


def numeric_comparison(report,name,a,b,folder,*,strict=False):
    statistics=leaf.compare_values(a,b);evidence=folder/(name+'.json');leaf.dump(evidence,statistics)
    differences=statistics.pop('differences')
    state='fail' if statistics['structure_difference_count'] or (strict and statistics['numeric_difference_count']) else 'review' if statistics['numeric_difference_count'] else 'pass'
    report.case(name,status=state,**statistics,evidence=str(evidence),first_differences=differences[:6])


def compare_level(report,name,a,b,folder,*,strict=False):
    ta,pa=split_level(a);tb,pb=split_level(b)
    differences=structural_differences(ta,tb);path=folder/(name+'.topology.json');leaf.dump(path,differences)
    report.case(name+'.topology',not differences,difference_count=len(differences),first_differences=differences[:6],evidence=str(path))
    numeric_comparison(report,name+('.positions-roundtrip' if strict else '.positions-pascal'),pa,pb,folder,strict=strict)


def topology_problems(level):
    problems=[];points=level['points'];edges=level['edges'];faces=level['faces'];curves=level['curves']
    expected=defaultdict(list);referenced=set()
    def valid(index):return type(index) is int and 0<=index<len(points)
    for fi,face in enumerate(faces):
        cycle=face['points']
        if len(cycle)<3 or len(set(cycle))!=len(cycle) or not all(valid(p) for p in cycle):problems.append(f'face {fi}: invalid oriented cycle');continue
        referenced.update(cycle)
        for a,b in zip(cycle,cycle[1:]+cycle[:1]):expected[tuple(sorted((a,b)))].append(fi)
    for ci,curve in enumerate(curves):
        chain=curve['points']
        if not all(valid(p) for p in chain):problems.append(f'curve {ci}: invalid point reference');continue
        if len(curve['knuckles'])!=len(chain):problems.append(f'curve {ci}: knuckle count differs')
        referenced.update(chain)
        for a,b in zip(chain,chain[1:]):expected.setdefault(tuple(sorted((a,b))),[])
    actual={}
    for ei,edge in enumerate(edges):
        key=tuple(edge['points'])
        if len(key)!=2 or not all(valid(p) for p in key) or key[0]>=key[1]:problems.append(f'edge {ei}: invalid canonical endpoints');continue
        if key in actual:problems.append(f'edge {ei}: duplicate endpoint identity')
        actual[key]=edge['faces']
        if Counter(edge['faces'])!=Counter(expected.get(key,[])):problems.append(f'edge {ei}: wrong face incidence')
    if set(actual)!=set(expected):problems.append('edge set does not equal face/curve topology')
    if level['level']>0 and referenced!=set(range(len(points))):problems.append('canonical refined point is not referenced')
    if level['empty']!=(not referenced):problems.append('empty flag disagrees with references')
    if referenced:
        minimum=[min(points[i]['position'][axis] for i in referenced) for axis in range(3)]
        maximum=[max(points[i]['position'][axis] for i in referenced) for axis in range(3)]
        if level['bounds']!={'min':minimum,'max':maximum}:problems.append('bounds do not cover exactly referenced geometry')
    return problems


def point_at_role(level,role):
    match=re.fullmatch(r'root(\d+)\.child(\d+)\.vertex(\d+)',role)
    if not match:raise ValueError('Invalid independent role')
    root,child,vertex=map(int,match.groups())
    face=next(f for f in level['faces'] if f['root_control_face']==root and f['child_index']==child)
    return level['points'][face['points'][vertex]]['position']


def independent(report,name,item,actual,expected,folder):
    mathematical=fixtures.independent_expectations(item,actual['level'])
    if not mathematical:return
    expected_vertices=mathematical.get('vertices',{})
    observed={key:point_at_role(actual,key) for key in expected_vertices}
    original={key:point_at_role(expected,key) for key in expected_vertices}
    statistics=leaf.compare_values(observed,expected_vertices)
    shared=leaf.compare_values(observed,original)
    same_single=not shared['structure_difference_count'] and not shared['numeric_difference_count']
    state='fail' if statistics['structure_difference_count'] else 'review' if statistics['numeric_difference_count'] and same_single else 'fail' if statistics['numeric_difference_count'] else 'pass'
    if expected_vertices:
        path=folder/(name+'.independent-positions.json')
        leaf.dump(path,{'mathematical':expected_vertices,'native':observed,'pascal':original,**statistics})
        differences=statistics.pop('differences')
        report.case(name+'.independent-positions',status=state,**statistics,evidence=str(path),first_differences=differences[:6],
            reason='Closed-form boundary/Loop/Catmull-Clark expectation. Shared discrepancies are retained for review; equality with Pascal alone does not prove mathematical accuracy.')
    conditions=[]
    for key,actual_value in [('point_count',len(actual['points'])),('face_count',len(actual['faces'])),('edge_count',len(actual['edges'])),
                             ('boundary_edge_count',sum(len(e['faces'])==1 for e in actual['edges']))]:
        if key in mathematical:conditions.append((key,actual_value==mathematical[key],actual_value,mathematical[key]))
    if 'face_arity' in mathematical:conditions.append(('face_arity',all(len(f['points'])==mathematical['face_arity'] for f in actual['faces']),None,mathematical['face_arity']))
    if 'orientation_sign' in mathematical:
        signed=[]
        for f in actual['faces']:
            p,q,r=[actual['points'][i]['position'] for i in f['points'][:3]]
            signed.append((q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]))
        conditions.append(('orientation',all(v*mathematical['orientation_sign']>0 for v in signed),signed,mathematical['orientation_sign']))
    if mathematical.get('disjoint_roots'):
        refs=defaultdict(set)
        for f in actual['faces']:refs[f['root_control_face']].update(f['points'])
        conditions.append(('coincident-geometry-distinct-identity',not(refs[0]&refs[1]),None,True))
    if 'curve_point_counts' in mathematical:
        sizes=[len(c['points']) for c in actual['curves']]
        conditions.append(('curve-point-counts',sizes==mathematical['curve_point_counts'],sizes,mathematical['curve_point_counts']))
    for label,ok,value,wanted in conditions:report.case(name+'.independent-'+label,ok,actual=value,expected=wanted)


def exercise(report,prefix,inputs,native,pascal,folder):
    folder.mkdir(parents=True,exist_ok=False)
    ids=[item['id'] for item in inputs['cases']]
    report.case(prefix+'.native-case-order',ids==[c['id'] for c in native['cases']])
    report.case(prefix+'.pascal-case-order',ids==[c['id'] for c in pascal['cases']])
    na={c['id']:c for c in native['cases']};pa={c['id']:c for c in pascal['cases']}
    for item in inputs['cases']:
        name=prefix+'.'+item['id'];a=na.get(item['id']);b=pa.get(item['id'])
        if not a or not b or a.get('status')!='ok' or b.get('status')!='ok':report.case(name,status='error',native=a,pascal=b);continue
        report.case(name+'.requested-levels',[x['level'] for x in a['levels']]==item['levels']==[x['level'] for x in b['levels']])
        report.case(name+'.different-request-order',a['levels']==a['levels_repeat'])
        report.case(name+'.incremental-request-order',a['levels']==a['levels_forward'])
        control_reference=b.get('loaded_geometry',item['geometry'])
        ta,native_numbers=split_geometry(a['loaded_geometry']);tb,reference_numbers=split_geometry(control_reference)
        input_differences=structural_differences(ta,tb)
        report.case(name+'.loaded-control-net-topology',not input_differences,differences=input_differences,
            reference='Original loader before subdivision' if 'loaded_geometry' in b else 'Complete original-FBM control net export')
        numeric_comparison(report,name+'.loaded-control-net-numbers',native_numbers,reference_numbers,folder,strict=True)
        if 'loaded_geometry' in b:
            wanted,values=split_geometry(item['geometry'])
            differences=structural_differences(tb,wanted)
            report.case(name+'.pascal-input-fields-preserved',not differences,differences=differences)
            numeric_comparison(report,name+'.pascal-input-numbers-preserved',reference_numbers,values,folder,strict=True)
        for native_level,pascal_level in zip(a['levels'],b['levels']):
            levelname=name+f'.level-{native_level["level"]}'
            issues=topology_problems(native_level);report.case(levelname+'.topology-invariants',not issues,problems=issues)
            compare_level(report,levelname,native_level,pascal_level,folder)
            independent(report,levelname,item,native_level,pascal_level,folder)
    for check in native.get('api_checks',[]):report.case(prefix+'.api.'+check['id'],status=check['status'])
    report.data.setdefault('oracle_metadata',{})[prefix]=pascal.get('metadata')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=WORKSPACE/'FreeShip-Qt');parser.add_argument('--runtime',type=Path)
    parser.add_argument('--reuse-probe',type=Path);parser.add_argument('--model',type=Path,action='append');parser.add_argument('--small-only',action='store_true')
    args=parser.parse_args();source=args.source.resolve(strict=True);qt_source=args.qt_source.resolve(strict=True);out=args.output.resolve()
    leaf.ensure_private(out,source,qt_source);out.mkdir(parents=True,exist_ok=False)
    models=[p.resolve(strict=True) for p in (args.model or leaf.DEFAULT_MODELS)]
    report=Report();source_before=None;tests_before={};native_before={};pilot_before={};protected={};distributions={};runtime=None
    try:
        source_before=common.fingerprint(source)[0];report.data['legacy_source_sha256']=source_before
        tests_before={p:common.digest_file(ROOT/p) for p in TEST_FILES};report.data['test_sha256']=tests_before
        native_before=leaf.native_source_hashes(qt_source);report.data['native_source_sha256']=native_before
        pilot_before=leaf.pilot_source_hashes();report.data['pilot_source_sha256']=pilot_before
        for path in [*models,source/'FreeShip.exe',WORKSPACE/'FreeShip-QtPilot/dist/FreeShipQtPilot.exe',WORKSPACE/'FreeShip-QtPilot/dist/bridge/FreeShipGeometryBridge.exe']:
            protected[str(path)]=common.digest_file(path)
        for path in (qt_source/'dist',WORKSPACE/'FreeShip-QtPilot/dist'):distributions[str(path)]=leaf.files_hashes(path)
        report.data['protected_sha256']=protected;report.data['protected_distributions_sha256']=distributions
        runtime=args.runtime.resolve(strict=True) if args.runtime else build_native(qt_source,out)
        if qt_source/'build' not in runtime.parents:leaf.ensure_private(runtime,source,qt_source)
        report.data['native_manifest']=validate_runtime(runtime,qt_source)
        probe_folder=args.reuse_probe.resolve(strict=True) if args.reuse_probe else out/'oracle-build'
        if not args.reuse_probe:oracle.build(source,qt_source,probe_folder)
        probe,probe_report=validate_probe(probe_folder,source,qt_source);report.data['oracle_build']=probe_report
        inputs=fixtures.cases();input_path=out/'input.json';leaf.dump(input_path,inputs)
        expected=oracle.run(probe,input_path,out/'pascal-small');actual=native_run(runtime,input_path,out/'native-small')
        exercise(report,'small',inputs,actual,expected,out/'comparisons-small')
        if args.small_only:report.case('real-model-subdivision',status='not_run',reason='Explicit small-only diagnostic selection')
        else:
            exported=oracle.export_models(probe,models,out/'model-oracle');report.data['models']=exported['models']
            for model in exported['models']:
                report.case(model['prefix']+'.original-control-net-roundtrip',model['control_net_roundtrip_exact'])
                report.case(model['prefix']+'.original-subdivision-roundtrip',model['direct_vs_reconstructed_geometry_exact'])
                differences=model.get('replay_diagnostic_differences',[])
                if differences:report.case(model['prefix']+'.original-debug-level-residue',status='review',differences=differences,
                    reason='Original objects retain a debug subdivision-level field at level zero; reconstructed objects start at zero. Full geometry, topology and input fields are separately required to match.')
            replay=oracle.run(probe,Path(exported['input_path']),out/'model-roundtrip');roundtrip=out/'comparisons-roundtrip';roundtrip.mkdir()
            report.case('model-roundtrip.ids',[c['id'] for c in exported['results']['cases']]==[c['id'] for c in replay['cases']])
            for original,rebuilt in zip(exported['results']['cases'],replay['cases']):
                if original.get('status')!='ok' or rebuilt.get('status')!='ok':report.case('model-roundtrip.'+original['id'],status='error',original=original,rebuilt=rebuilt);continue
                report.case('model-roundtrip.'+original['id']+'.level-order',[x['level'] for x in original['levels']]==[x['level'] for x in rebuilt['levels']])
                for a,b in zip(rebuilt['levels'],original['levels']):compare_level(report,'model-roundtrip.'+original['id']+f'.level-{a["level"]}',a,b,roundtrip,strict=True)
            actual=native_run(runtime,Path(exported['input_path']),out/'native-models')
            exercise(report,'model',exported['input'],actual,exported['results'],out/'comparisons-model')
    except Exception as error:report.case('execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        if source_before:report.case('legacy-source-preserved',status='pass' if source_before==common.fingerprint(source)[0] else 'error')
        for path,digest in protected.items():report.case('protected-file:'+path,Path(path).is_file() and common.digest_file(Path(path))==digest)
        for folder,digests in distributions.items():report.case('protected-distribution:'+folder,leaf.files_hashes(Path(folder))==digests)
        if tests_before:report.case('test-sources-stable',status='pass' if tests_before=={p:common.digest_file(ROOT/p) for p in TEST_FILES} else 'error')
        if native_before:report.case('native-sources-stable',status='pass' if native_before==leaf.native_source_hashes(qt_source) else 'error')
        if pilot_before:report.case('accepted-pilot-source-preserved',pilot_before==leaf.pilot_source_hashes())
        if runtime and 'native_manifest' in report.data:
            try:validate_runtime(runtime,qt_source);report.case('native-runtime-preserved',True)
            except Exception as error:report.case('native-runtime-preserved',status='error',reason=str(error))
        report.finish();leaf.dump(out/'report.json',report.data);leaf.dump(out/'issues.json',[c for c in report.data['cases'] if c['status']!='pass'])
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'fail':1,'review':2,'not_run':2,'error':3}[report.data['status']]


if __name__=='__main__':raise SystemExit(main())
