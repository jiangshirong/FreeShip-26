"""Validate the isolated C++ plane/spline candidate against Pascal and analytic cases.

No application is installed or launched. Numerical disagreement is retained as
review, not accepted by a tolerance selected after observing the candidate.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import time

import freeship_ui_test as common
import native_geometry_cases as fixtures
import native_geometry_probe as oracle

ROOT=Path(__file__).resolve().parent
WORKSPACE=ROOT.parent
TEST_FILES=('native_geometry.py','native_geometry_cases.py','native_geometry_probe.py',
            'NativeGeometryProbe.pas','qt/native_geometry_tests.cpp','freeship_ui_test.py','native_flicker_test.py','run_tests.py')
DEFAULT_MODELS=[WORKSPACE/'FreeShip-MCP/examples/box.fbm',
                WORKSPACE/'FreeShip-MCP/examples/synthetic-hull.fbm',
                Path('D:/桌面文件夹/学习/大学课程/过考试/船舶设计原理/9a/9a-Женя.fbm')]
STATES=('pass','fail','review','not_run','error')


def dump(path,value):
    Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf8')


class Report:
    def __init__(self):
        self.data={'status':'error','cases':[],'scope':'Standalone native plane/spline candidate; no production integration',
            'acceptance_policy':{
                'compatibility':'Every supported finite Single number is reconstructed from JSON and compared exactly; no numeric tolerance. This is not a raw in-memory bit identity assertion.',
                'transport':'JSON does not preserve signed-zero representation; positive and negative zero compare as the same zero.',
                'difference':'Numeric differences remain review; shape, count, order and Boolean differences fail.',
                'safety_changes':'Degenerate inputs have separately tested native behavior and explicit compatibility differences.',
                'production_gate':'Any unresolved review, failure or error prevents declaring this a compatible replacement.'},
            'limitations':[
                'This validates only Plane and Spline, not subdivision surfaces, model edits, FBM/DXF exports or the GUI.',
                'Real models contribute selected section splines; coverage.json records selection and generated fixture sections.',
                'Independent cubic examples are exact rational examples, not a proof over all possible splines.',
                'Pascal has no matching line-plane API; these results use independent analytic examples.',
                'No overall startup or display-frame-rate performance claim is made.']}

    def case(self,name,ok=None,*,status=None,**details):
        state=status or ('pass' if ok else 'fail')
        if state not in STATES:raise ValueError('Unknown result state')
        self.data['cases'].append({'id':name,'status':state,**details})

    def finish(self):
        counts=Counter(c['status'] for c in self.data['cases'])
        self.data['counts']={s:counts[s] for s in STATES}
        self.data['status']=next((s for s in ('error','fail','review','not_run') if counts[s]),'pass')
        self.data['compatible_replacement']=self.data['status']=='pass' and not self.data.get('intentional_differences')
        comparable=[c for c in self.data['cases'] if c['id'].endswith('.pascal')]
        self.data['compatibility_totals']={key:sum(c.get(key,0) for c in comparable) for key in
            ('scalar_count','reconstructed_single_equal_count','numeric_difference_count','structure_difference_count')}
        self.data['compatibility_totals']['case_count']=len(comparable)


def files_hashes(folder):
    return {p.relative_to(folder).as_posix():common.digest_file(p) for p in sorted(folder.rglob('*')) if p.is_file()}


def native_source_hashes(source):
    paths=[*sorted((source/'src/native').rglob('*')),source/'CMakeLists.txt',source/'build-native.ps1']
    return {p.relative_to(source).as_posix():common.digest_file(p) for p in paths if p.is_file()}


def pilot_source_hashes():
    pilot=WORKSPACE/'FreeShip-QtPilot'
    paths=[pilot/'CMakeLists.txt',pilot/'build.ps1']
    paths.extend(p for folder in ('src','bridge') for p in (pilot/folder).rglob('*')
                 if p.is_file() and p.suffix.lower() in ('.h','.cpp','.pas','.inc','.py','.json'))
    return {p.relative_to(pilot).as_posix():common.digest_file(p) for p in sorted(paths) if p.is_file()}


def ensure_private(path,source,qt_source):
    for protected in (source,qt_source,WORKSPACE/'FreeShip-QtPilot',WORKSPACE/'References'):
        protected=protected.resolve()
        if path==protected or protected in path.parents:raise ValueError('Use a private output outside product trees: '+str(path))
    if path in (ROOT,WORKSPACE):raise ValueError('Use a new private child directory')


def validate_runtime(runtime,qt_source):
    manifest=json.loads((runtime/'native-build-manifest.json').read_text('utf-8-sig'))
    if manifest['source_sha256']!=native_source_hashes(qt_source):raise RuntimeError('Native runtime source hash is stale')
    expected={'qt/native_geometry_tests.cpp':common.digest_file(ROOT/'qt/native_geometry_tests.cpp')}
    if manifest['test_source_sha256']!=expected:raise RuntimeError('Native JSON adapter hash is stale')
    actual=files_hashes(runtime);actual.pop('native-build-manifest.json',None)
    if actual!=manifest['binaries_sha256']:raise RuntimeError('Native runtime binaries do not match the manifest')
    if 'Qt6Widgets.dll' in actual:raise RuntimeError('This suite must not depend on a GUI runtime')
    return manifest


def build_native(qt_source,out):
    runtime=out/'runtime'
    command=['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(qt_source/'build-native.ps1'),
             '-BuildDir',str(out/'native-build'),'-RuntimeDir',str(runtime),'-WorkspaceRoot',str(WORKSPACE)]
    with (out/'native-build.log').open('wb') as log:
        result=subprocess.run(command,cwd=WORKSPACE,stdout=log,stderr=subprocess.STDOUT,
                              timeout=600,creationflags=common.FLAGS)
    if result.returncode:raise RuntimeError('Native compile failed; see native-build.log')
    return runtime


def validate_probe(folder,source,qt_source):
    report=json.loads((folder/'report.json').read_text('utf8'))
    if report.get('status')!='pass' or report['source_sha256']!=common.fingerprint(source)[0]:
        raise RuntimeError('Pascal probe source hash is stale')
    if report['test_sha256']!={p:common.digest_file(ROOT/p) for p in ('NativeGeometryProbe.pas','native_geometry_probe.py')}:
        raise RuntimeError('Pascal probe adapter hash is stale')
    if report['adapter_sha256']!=oracle._adapter(qt_source).adapter_hashes():raise RuntimeError('Headless bridge adapter hash is stale')
    if report['geometry_sha256']!={p:common.digest_file(source/p) for p in oracle.GEOMETRY_FILES}:
        raise RuntimeError('Pascal geometry hash is stale')
    exe=folder/'NativeGeometryProbe.exe'
    if report.get('executable_sha256')!=common.digest_file(exe):raise RuntimeError('Pascal probe executable has no matching hash')
    return exe,report


def native_run(runtime,input_path,out):
    out.mkdir(parents=True,exist_ok=False)
    environment=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();environment[key]=str(folder)
    before=common.digest_file(input_path)
    process=subprocess.run([str(runtime/'native_geometry_tests.exe'),str(input_path),str(out/'results.json')],
        cwd=out,env=environment,capture_output=True,timeout=180,creationflags=common.FLAGS)
    (out/'native.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError('Native runner failed: '+str(out/'native.log'))
    if common.digest_file(input_path)!=before:raise RuntimeError('Native runner changed the input')
    result=json.loads((out/'results.json').read_text('utf8'))
    if result.get('format')!='FreeShipNativeGeometryResults' or result.get('version')!=1 or not isinstance(result.get('cases'),list):
        raise RuntimeError('Malformed native output')
    return result


def bits(value):
    if value==0:value=0.0
    return struct.unpack('<I',struct.pack('<f',value))[0]


def ordered_bits(value):
    raw=bits(value)
    return ~raw & 0xffffffff if raw & 0x80000000 else raw | 0x80000000


def compare_values(actual,expected):
    """Retain every mismatch; structure and numeric discrepancies are separate."""
    stats={'scalar_count':0,'reconstructed_single_equal_count':0,'numeric_difference_count':0,
           'structure_difference_count':0,'max_abs_error':0.0,'max_ulp_error':0,'differences':[]}
    def visit(a,b,path):
        if isinstance(b,bool) or b is None or isinstance(b,str) or path.endswith('.count'):
            if type(a) is not type(b) or a!=b:
                stats['structure_difference_count']+=1;stats['differences'].append({'path':path,'actual':a,'expected':b,'type':'structure'})
        elif isinstance(b,(int,float)):
            if isinstance(a,bool) or not isinstance(a,(int,float)) or not math.isfinite(a) or not math.isfinite(b):
                stats['structure_difference_count']+=1;stats['differences'].append({'path':path,'actual':a,'expected':b,'type':'nonfinite-or-type'})
                return
            stats['scalar_count']+=1
            if bits(a)==bits(b):stats['reconstructed_single_equal_count']+=1
            else:
                difference=abs(a-b);ulp=abs(ordered_bits(a)-ordered_bits(b))
                stats['numeric_difference_count']+=1
                stats['max_abs_error']=max(stats['max_abs_error'],difference)
                stats['max_ulp_error']=max(stats['max_ulp_error'],ulp)
                stats['differences'].append({'path':path,'actual':a,'expected':b,'abs_error':difference,'ulp_error':ulp,'type':'numeric'})
        elif isinstance(b,list):
            if not isinstance(a,list) or len(a)!=len(b):
                stats['structure_difference_count']+=1;stats['differences'].append({'path':path,'actual_count':len(a) if isinstance(a,list) else None,'expected_count':len(b),'type':'shape'})
            if isinstance(a,list):
                for index,(x,y) in enumerate(zip(a,b)):visit(x,y,path+f'[{index}]')
        elif isinstance(b,dict):
            if not isinstance(a,dict):
                stats['structure_difference_count']+=1;stats['differences'].append({'path':path,'type':'object'});return
            for key,value in b.items():
                if key not in a:
                    stats['structure_difference_count']+=1;stats['differences'].append({'path':path+'.'+key,'type':'missing'})
                else:visit(a[key],value,path+'.'+key)
        else:raise ValueError('Unexpected comparison input')
    visit(actual,expected,'$')
    return stats


def comparison(report,name,actual,expected,out,*,analytic=False,strict=False):
    stats=compare_values(actual,expected)
    evidence=out/(name.replace('/','_').replace(':','_')+'.json')
    dump(evidence,stats)
    differences=stats.pop('differences')
    state='fail' if stats['structure_difference_count'] or ((analytic or strict) and stats['numeric_difference_count']) else 'review' if stats['numeric_difference_count'] else 'pass'
    report.case(name,status=state,**stats,evidence=str(evidence),first_differences=differences[:4],
        oracle='independent exact analytic example' if analytic else 'original Pascal object versus reconstructed Pascal spline' if strict else 'unmodified current Pascal geometry')


def common_result(result,kind):
    fields=('coefficients','evaluations','projections','mirrors','boxes') if kind=='plane' else ('knots','total_length','values','first','second','cuts')
    expected={k:result[k] for k in fields if k not in result.get('unsupported',[])}
    if 'cuts' in expected:expected['cuts']=[{k:c[k] for k in ('parameters','points','found','count')} for c in expected['cuts']]
    if 'mutations' in result:expected['mutations']=[common_result(c,kind) for c in result['mutations']]
    return expected


def exercise(report,prefix,inputs,actual,expected,folder):
    folder.mkdir(parents=True,exist_ok=False)
    expected_ids=[c['id'] for c in inputs['cases']]
    report.case(prefix+'.native-case-order',expected_ids==[c.get('id') for c in actual['cases']])
    report.case(prefix+'.oracle-case-order',expected_ids==[c.get('id') for c in expected['cases']])
    actual_by_id={c['id']:c for c in actual['cases']};expected_by_id={c['id']:c for c in expected['cases']}
    for item in inputs['cases']:
        name=prefix+'.'+item['id'];a=actual_by_id.get(item['id']);b=expected_by_id.get(item['id'])
        if a is None or b is None:
            report.case(name,status='error',reason='Missing native or oracle case');continue
        if item.get('policy') in ('zero-fragment-cut','invalid-plane-cut'):
            if b.get('status')!='ok':
                report.case(name,status='error',reason='Original-core safety-boundary observation failed',oracle=b)
                continue
            report.case(name+'.native-rejects-incomplete-sampling',a.get('status')=='error' and a.get('error_kind')=='domain_error')
            reason=('With zero fragments the old method samples only t=0; native rejects an incomplete intersection request. Plain value/derivative evaluation remains supported.' if item.get('policy')=='zero-fragment-cut' else 'Zero-normal plane intersection now rejects invalid geometry; the old routine reports every sample as a coincident hit.')
            report.case(name+'.legacy-behavior-difference',status='review',reason=reason,native=a,oracle=b)
            report.data.setdefault('intentional_differences',[]).append(name)
            continue
        if a.get('status')!='ok' or b.get('status')!='ok':
            report.case(name,status='error',native=a,oracle=b);continue
        independent=fixtures.analytic_examples(item)
        if independent:
            observed=dict(a)
            if 'analytic_parameters' in item:
                indices=[item['parameters'].index(t) for t in item['analytic_parameters']]
                for field in ('values','first','second'):
                    if field in independent:observed[field]=[a[field][i] for i in indices]
            if 'line_relations' in independent:
                observed['line_relations']=[line['relation'] for line in a['lines']]
                observed['first_line_point']=a['lines'][0].get('point')
                observed['first_line_parameter']=a['lines'][0].get('parameter')
            comparison(report,name+'.analytic',observed,independent,folder,analytic=True)
            if 'analytic_parameters' in item:
                expanded=dict(item);expanded.pop('analytic_parameters')
                mathematical=fixtures.analytic_examples(expanded)
                observations=compare_values(a,mathematical)
                if observations['numeric_difference_count']:
                    for difference in observations['differences']:
                        match=re.fullmatch(r'\$\.(values|first|second)\[(\d+)\]\[(\d+)\]',difference['path'])
                        if match:
                            field,row,axis=match[1],int(match[2]),int(match[3])
                            difference['parameter']=item['parameters'][row]
                            difference['pascal_actual']=b[field][row][axis]
                    dump(folder/(name+'.analytic-rounding-observation.json'),observations)
                    report.case(name+'.analytic-rounding-observation',status='review',
                        reason='At additional near-knot query points, the inherited Single interpolation can differ from a real-arithmetic closed form. Native/Pascal equality is still required separately. This difference is retained, with no accepted numerical tolerance. In the near-endpoint natural-V example, the multiplication rounds before subtractive cancellation; the 2^-21 difference is half an ulp at 12.',
                        evidence=str(folder/(name+'.analytic-rounding-observation.json')),**observations)
        if item.get('policy')=='invalid-plane':
            safe=a.get('valid') is False and all(v is None for field in ('evaluations','projections','mirrors','boxes') for v in a[field]) and all(c['relation']=='invalid' for c in a['lines'])
            report.case(name+'.native-invalid-plane-policy',safe)
            report.case(name+'.legacy-behavior-difference',status='review',reason='Zero-normal plane now has explicit invalid results; legacy returns origin projections and non-geometric mirrors.',native=a,oracle=b)
            report.data.setdefault('intentional_differences',[]).append(name)
        else:
            expected_fields=common_result(b,item['kind'])
            if item.get('policy')=='short-spline':
                expected_fields.pop('cuts',None)
                report.case(name+'.native-has-no-fake-origin-cuts',all(not c['found'] and c['count']==0 and not c['points'] and not c['parameters'] for c in a['cuts']))
                report.case(name+'.legacy-short-spline-cut-difference',status='review',reason='Empty/single-point legacy spline Value returns ZERO and the sampled cut reports origin hits. Native returns no intersections for fewer than two points.',native=a['cuts'],oracle=b['cuts'])
                report.data.setdefault('intentional_differences',[]).append(name)
            comparison(report,name+'.pascal',a,expected_fields,folder)
        if 'knots' in b.get('unsupported',[]):
            report.case(name+'.legacy-parameters-unavailable',status='not_run',reason='Original spline has no allocated parameter array for fewer than two points; native safe extension is separately tested.')
            report.case(name+'.native-short-knots',a['knots']==([0] if len(item['points'])==1 else []))
        if item['kind']=='spline':
            report.case(name+'.repeat-cut-clears-output',a['cuts']==a['cuts_repeat'])
            for index,m in enumerate(a.get('mutations',[])):
                report.case(name+f'.mutation-{index}.repeat-cut-clears-output',m['cuts']==m['cuts_repeat'])
    for check in actual.get('api_checks',[]):report.case(prefix+'.api.'+check['id'],status=check['status'])
    report.data.setdefault('numeric_metadata',{})[prefix]={'native':actual.get('numeric_types'),'pascal':expected.get('metadata')}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=WORKSPACE/'FreeShip-Qt')
    parser.add_argument('--runtime',type=Path,help='Reuse only a current source-verified private native runtime')
    parser.add_argument('--reuse-probe',type=Path,help='Reuse only a current source-verified oracle build folder')
    parser.add_argument('--model',type=Path,action='append')
    parser.add_argument('--small-only',action='store_true',help='Diagnostic branch examples only; real-model coverage stays not_run')
    args=parser.parse_args()
    source=args.source.resolve(strict=True);qt_source=args.qt_source.resolve(strict=True);out=args.output.resolve()
    ensure_private(out,source,qt_source);out.mkdir(parents=True,exist_ok=False)
    models=[p.resolve(strict=True) for p in (args.model or DEFAULT_MODELS)]
    report=Report();source_before=None;protected={};dist_before={};tests_before={};native_before={};pilot_before={};runtime=None
    try:
        source_before=common.fingerprint(source)[0]
        report.data['legacy_source_sha256']=source_before
        tests_before={p:common.digest_file(ROOT/p) for p in TEST_FILES};report.data['test_sha256']=tests_before
        native_before=native_source_hashes(qt_source);report.data['native_source_sha256']=native_before
        pilot_before=pilot_source_hashes();report.data['pilot_source_sha256']=pilot_before
        for path in [*models,source/'FreeShip.exe',WORKSPACE/'FreeShip-QtPilot/dist/FreeShipQtPilot.exe',WORKSPACE/'FreeShip-QtPilot/dist/bridge/FreeShipGeometryBridge.exe']:
            protected[str(path)]=common.digest_file(path)
        for folder in (qt_source/'dist',WORKSPACE/'FreeShip-QtPilot/dist'):
            dist_before[str(folder)]=files_hashes(folder)
        report.data['protected_sha256']=protected;report.data['protected_distributions_sha256']=dist_before
        runtime=args.runtime.resolve(strict=True) if args.runtime else build_native(qt_source,out)
        if qt_source/'build' not in runtime.parents:ensure_private(runtime,source,qt_source)
        report.data['native_manifest']=validate_runtime(runtime,qt_source)
        probe_folder=args.reuse_probe.resolve(strict=True) if args.reuse_probe else out/'oracle-build'
        if not args.reuse_probe:oracle.build(source,qt_source,probe_folder)
        probe,probe_report=validate_probe(probe_folder,source,qt_source)
        report.data['oracle_build']=probe_report
        inputs=fixtures.cases();initial_input=out/'input-initial.json';dump(initial_input,inputs)
        preliminary=oracle.run(probe,initial_input,out/'pascal-parameter-seed')
        report.data['parameter_sampling']='Original Pascal knots and both adjacent float32 values are added to the fixed branch queries. No candidate output chooses queries or expected values.'
        by_id={c['id']:c for c in preliminary['cases']}
        for item in inputs['cases']:
            result=by_id.get(item['id'],{})
            if item['kind']=='spline' and result.get('status')=='ok':
                if fixtures.analytic_examples(item):item['analytic_parameters']=list(item['parameters'])
                additional=[v for knot in result.get('knots',[]) for v in fixtures.neighbors32(knot)]
                item['parameters']=list(dict.fromkeys([*item['parameters'],*additional]))
        input_path=out/'input.json';dump(input_path,inputs)
        actual=native_run(runtime,input_path,out/'native-small')
        expected=oracle.run(probe,input_path,out/'pascal-small')
        exercise(report,'small',inputs,actual,expected,out/'comparisons-small')
        if args.small_only:
            report.case('real-model-sections',status='not_run',reason='Explicit --small-only diagnostic selection')
        else:
            exported=oracle.export_models(probe,models,out/'model-oracle')
            report.data['models']=exported['models']
            replay=oracle.run(probe,Path(exported['input_path']),out/'model-oracle-roundtrip')
            roundtrip=out/'comparisons-roundtrip';roundtrip.mkdir()
            original_cases=exported['results']['cases'];replayed_cases=replay['cases']
            report.case('model-oracle-roundtrip.case-order',[c['id'] for c in original_cases]==[c['id'] for c in replayed_cases])
            for original,rebuilt in zip(original_cases,replayed_cases):
                name='model-oracle-roundtrip.'+original['id']
                if original.get('status')!='ok' or rebuilt.get('status')!='ok':
                    report.case(name,status='error',original=original,rebuilt=rebuilt)
                else:comparison(report,name,common_result(rebuilt,'spline'),common_result(original,'spline'),roundtrip,strict=True)
            actual=native_run(runtime,Path(exported['input_path']),out/'native-models')
            exercise(report,'model',exported['input'],actual,exported['results'],out/'comparisons-model')
        report.case('kernel-microbenchmark',status='not_run',reason='No paired loop-only benchmark implemented; JSON/process wall time is not reported as kernel or GUI performance.')
    except Exception as error:
        report.case('execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        if source_before:report.case('legacy-source-preserved',status='pass' if source_before==common.fingerprint(source)[0] else 'error')
        for path,digest in protected.items():report.case('protected-file:'+path,Path(path).is_file() and common.digest_file(Path(path))==digest)
        for folder,digests in dist_before.items():report.case('protected-distribution:'+folder,files_hashes(Path(folder))==digests)
        if tests_before:report.case('test-sources-stable',status='pass' if tests_before=={p:common.digest_file(ROOT/p) for p in TEST_FILES} else 'error')
        if native_before:report.case('native-sources-stable',status='pass' if native_before==native_source_hashes(qt_source) else 'error')
        if pilot_before:report.case('accepted-pilot-source-preserved',pilot_before==pilot_source_hashes())
        if runtime and runtime.exists() and 'native_manifest' in report.data:
            try:validate_runtime(runtime,qt_source);report.case('native-runtime-preserved',True)
            except Exception as error:report.case('native-runtime-preserved',status='error',reason=str(error))
        report.finish();dump(out/'report.json',report.data)
        dump(out/'issues.json',[c for c in report.data['cases'] if c['status']!='pass'])
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'fail':1,'review':2,'not_run':2,'error':3}[report.data['status']]


if __name__=='__main__':raise SystemExit(main())
