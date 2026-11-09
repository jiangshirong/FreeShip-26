"""Isolated section/chain candidate: original Plus observations and analytic cuts."""
from __future__ import annotations
import argparse
from collections import Counter
import copy
import json
import os
from pathlib import Path
import subprocess

import freeship_ui_test as common
import native_geometry as leaf
import native_subdivision as subdivision
import native_sections_cases as fixtures
import native_sections_probe as oracle
import native_simplify as simplifier
import native_linesplan as linesplan

ROOT=Path(__file__).resolve().parent
WORKSPACE=ROOT.parent
TEST_FILES=('native_sections.py','native_sections_cases.py','qt/native_sections_tests.cpp','native_sections_probe.py','NativeSectionsProbe.pas',
    'native_subdivision.py','native_subdivision_cases.py','native_subdivision_probe.py','NativeSubdivisionProbe.pas',
    'native_geometry.py','native_geometry_probe.py','freeship_ui_test.py','native_flicker_test.py','run_tests.py',
    'native_simplify.py','fixtures/native_simplify_models.json','native_linesplan.py','native_linesplan_observer.py','NativeLinesplanProbe.inc')
PROBE_FILES=('NativeSectionsProbe.pas','native_sections_probe.py','NativeSubdivisionProbe.pas','native_subdivision_probe.py','native_geometry_probe.py',
             'native_linesplan_observer.py','NativeLinesplanProbe.inc')
FIELDS=('segments','connected_curves','joined_curves','curves','found','selected_control_faces')


class Report:
    def __init__(self):
        self.data={'status':'error','cases':[],'scope':'Independent calculation-only section candidate; not linked into the application',
            'acceptance_policy':{
                'identity':'Source face/root/edge IDs use control-face, child, oriented point topology traversal. Coordinate sorting never establishes identity.',
                'compatibility':'Raw segments and every curve stage retain order, orientation, multiplicity, flags and fragment counts. Integer identity compares exactly.',
                'numbers':'Finite floating values reconstructed as Single compare exactly; no numeric tolerance. JSON does not establish raw-memory signed-zero bit identity.',
                'independent':'Analytic flat-polygon cuts and exact endpoint examples are separate from compatibility with Pascal. Shared heuristic geometry loss is review.',
                'prepend_fix':'Only two named minimal D1/D2 examples may have the documented false-to-true shared endpoint flag. Coordinates and all other fields remain exact; changed samples must exactly match the original spline fed the independently corrected chain.',
                'gate':'Review is not pass. No production replacement, export preservation or UI performance claim is made.'},
            'limitations':[
                'This calls Surface.IntersectPlane/CalculateIntersections and JoinSplineSegments only, without upper-layer Simplify, display mirroring, hydrostatics or export.',
                'GeometryInput records control-face order but not historical per-layer reordering performed through an editor.',
                'Legacy endpoint records have no vertex/source-segment identity fields. These native diagnostics are checked internally; they are not invented for the oracle.',
                'Level zero has no child faces in the original routine and therefore yields no sections; it is not a general polygon-plane intersection claim.',
                'Only the listed finite fixtures, requested levels and saved/model fixture planes are covered. This is not proof for all control nets.']}
    def case(self,name,ok=None,*,status=None,**details):
        state=status or ('pass' if ok else 'fail')
        if state not in leaf.STATES:raise ValueError('Unknown state')
        self.data['cases'].append(dict(id=name,status=state,**details))
    def finish(self):
        counts=Counter(c['status'] for c in self.data['cases']);self.data['counts']={s:counts[s] for s in leaf.STATES}
        self.data['status']=next((s for s in ('error','fail','review','not_run') if counts[s]),'pass')
        items=[c for c in self.data['cases'] if c.get('comparison_role')=='native-pascal']
        self.data['compatibility_totals']={k:sum(c.get(k,0) for c in items) for k in ('scalar_count','reconstructed_single_equal_count','numeric_difference_count','structure_difference_count','topology_difference_count')}
        self.data['compatibility_totals']['comparison_count']=len(items);self.data['compatible_replacement']=False
        ordinary=[c for c in items if not c.get('intentional_change') and c['id'] not in ('small.prepend-start.curves-pascal','small.prepend-end.curves-pascal')]
        self.data['unchanged_behavior_totals']={k:sum(c.get(k,0) for c in ordinary) for k in ('scalar_count','reconstructed_single_equal_count','numeric_difference_count','structure_difference_count','topology_difference_count')}
        self.data['unchanged_behavior_totals']['comparison_count']=len(ordinary)
        self.data['intentional_behavior_change_cases']=[c['id'] for c in items if c not in ordinary]


def validate_runtime(runtime,qt_source):
    manifest=json.loads((runtime/'native-build-manifest.json').read_text('utf-8-sig'))
    if manifest.get('target')!='native_sections_tests':raise RuntimeError('Wrong native target')
    if manifest['source_sha256']!=leaf.native_source_hashes(qt_source):raise RuntimeError('Native source manifest is stale')
    if manifest['test_source_sha256']!={'qt/native_sections_tests.cpp':common.digest_file(ROOT/'qt/native_sections_tests.cpp')}:raise RuntimeError('Native probe is stale')
    actual=leaf.files_hashes(runtime);actual.pop('native-build-manifest.json',None)
    if actual!=manifest['binaries_sha256']:raise RuntimeError('Runtime binary hashes differ')
    if 'Qt6Widgets.dll' in actual:raise RuntimeError('Calculation-only probe must not link Widgets')
    return manifest


def build_native(qt_source,out):
    command=['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(qt_source/'build-native.ps1'),'-Target','native_sections_tests',
        '-BuildDir',str(out/'native-build'),'-RuntimeDir',str(out/'runtime'),'-WorkspaceRoot',str(WORKSPACE)]
    with (out/'native-build.log').open('wb') as log:r=subprocess.run(command,cwd=WORKSPACE,stdout=log,stderr=subprocess.STDOUT,timeout=600,creationflags=common.FLAGS)
    if r.returncode:raise RuntimeError('Native build failed: '+str(out/'native-build.log'))
    return out/'runtime'


def validate_probe(folder,source,qt_source):
    report=json.loads((folder/'report.json').read_text('utf8'));exe=folder/'NativeSectionsProbe.exe'
    if report.get('status')!='pass' or report['source_sha256']!=common.fingerprint(source)[0]:raise RuntimeError('Original source probe is stale')
    if report['test_sha256']!={p:common.digest_file(ROOT/p) for p in PROBE_FILES}:raise RuntimeError('Original observer helper is stale')
    if report['adapter_sha256']!=oracle._adapter(qt_source).adapter_hashes():raise RuntimeError('Headless adapter is stale')
    if report['geometry_sha256']!=oracle.subdivision.geometry_hashes(source):raise RuntimeError('Original algorithms changed')
    if report['executable_sha256']!=common.digest_file(exe):raise RuntimeError('Original executable differs')
    if not report['observer']['stripped_equals_original'] or report['observer']['algorithm_statement_changes']!=0:raise RuntimeError('Read-only observer source integrity failed')
    capture=report['linesplan_observer']
    if not capture['stripped_equals_original'] or capture['draw_statement_changes'] or capture['dxf_statement_changes']:raise RuntimeError('Lines-plan observer changed algorithm')
    for filename,key in [('LegacyLinesplanDraw.inc','active_draw_sha256'),('OriginalLinesplanDXF.inc','original_dxf_sha256'),('LegacyLinesplanDXF.inc','instrumented_dxf_sha256')]:
        if common.digest_file(folder/filename)!=capture[key]:raise RuntimeError('Lines-plan capture source differs')
    return exe,report


def native_run(runtime,input_path,out):
    out.mkdir(parents=True,exist_ok=False);environment=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();environment[key]=str(folder)
    digest=common.digest_file(input_path)
    process=subprocess.run([str(runtime/'native_sections_tests.exe'),str(input_path),str(out/'results.json')],cwd=out,env=environment,capture_output=True,timeout=300,creationflags=common.FLAGS)
    (out/'native.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError('Native driver failed: '+str(out/'native.log'))
    if common.digest_file(input_path)!=digest:raise RuntimeError('Native changed input')
    data=json.loads((out/'results.json').read_text('utf8'))
    if data.get('format')!='FreeShipNativeSectionsResults' or data.get('version')!=1 or not isinstance(data.get('cases'),list):raise RuntimeError('Malformed native output')
    return data


def split(value):
    """Remove floating geometry from the exact integer/topology structure."""
    topology=copy.deepcopy(value);numbers={}
    if 'segments' in topology:
        numbers['segments']=[{k:s[k].pop('position') for k in ('start','end')} for s in topology['segments']]
    for key in ('connected_curves','joined_curves','curves'):
        if key not in topology:continue
        values=[]
        for c in topology[key]:
            v={'points':c.pop('points')}
            if 'samples' in c:
                sample=c.pop('samples');v['samples']={k:sample[k] for k in ('parameters','values','first','second','knots','total_length')}
                c['sample_fragments']=sample['fragments']
                if 'unsupported' in sample:c['unsupported']=sample['unsupported']
            values.append(v)
        numbers[key]=values
    return topology,numbers


def exact(a,b):
    stats=leaf.compare_values(a,b)
    return not stats['numeric_difference_count'] and not stats['structure_difference_count']


def compare(report,name,a,b,folder,*,role=None,intended_prepend=False,corrected_reference=None):
    ta,na=split(a);tb,nb=split(b);topology=subdivision.structural_differences(ta,tb);stats=leaf.compare_values(na,nb)
    state='fail' if topology or stats['structure_difference_count'] or stats['numeric_difference_count'] else 'pass'
    reason=None
    if intended_prepend:
        expected=copy.deepcopy(tb)
        if len(expected.get('curves',[]))==1 and expected['curves'][0].get('knuckles')==[False,False,False]:expected['curves'][0]['knuckles'][1]=True
        else:expected=None
        # Coordinates must remain unchanged. Only the independently established
        # single shared flag may differ; the corrected original spline is an
        # independent causal reference for every changed value and derivative.
        just_numbers=copy.deepcopy(na);old_numbers=copy.deepcopy(nb)
        for c in just_numbers['curves']:c.pop('samples',None)
        for c in old_numbers['curves']:c.pop('samples',None)
        justified=expected==ta and exact(just_numbers,old_numbers) and corrected_reference is not None and exact(a,corrected_reference)
        if justified:
            state='review';reason='Confirmed legacy prepend flag loss: one shared flag changes false to true. All geometry/order/fragments remain exact, and all new samples match the original Pascal spline given the independently corrected chain.'
    evidence=folder/(name+'.json');leaf.dump(evidence,{'topology_differences':topology,'numeric':stats,'native':a if intended_prepend else None,'pascal':b if intended_prepend else None})
    differences=stats.pop('differences')
    report.case(name,status=state,comparison_role=role,topology_difference_count=len(topology),topology_differences=topology[:5],
        **stats,first_differences=differences[:5],evidence=str(evidence),reason=reason)
    return state


def vector_key(point):return tuple(leaf.bits(x) for x in point)
def segment_bag(segments):return Counter(tuple(sorted((vector_key(a),vector_key(b)))) for a,b in segments)


def topology_issues(mesh,result):
    problems=[];points=mesh['points'];faces=mesh['faces'];edges=mesh['edges'];segments=result['segments'];diag=result.get('diagnostics',{})
    for i,segment in enumerate(segments):
        face=segment['face']
        if type(face) is not int or not 0<=face<len(faces):problems.append(f'segment {i}: unknown face');continue
        if segment['root_control_face']!=faces[face]['root_control_face']:problems.append(f'segment {i}: root disagrees with face')
        for side in ('start','end'):
            edge=segment[side]['edge']
            if type(edge) is not int or not 0<=edge<len(edges):problems.append(f'segment {i} {side}: unknown edge');continue
            if face not in edges[edge]['faces']:problems.append(f'segment {i} {side}: edge is not incident to source face')
            vertex=diag.get('endpoint_vertices',[{}]*len(segments))[i].get(side+'_vertex')
            if vertex is not None:
                if vertex not in edges[edge]['points']:problems.append(f'segment {i} {side}: vertex not on source edge')
                elif not exact(segment[side]['position'],points[vertex]['position']):problems.append(f'segment {i} {side}: vertex position differs')
    for stage in ('connected','joined','final'):
        lineage=diag.get(stage+'_source_segments',[]);curves=result['curves' if stage=='final' else stage+'_curves']
        if len(lineage)!=len(curves):problems.append(stage+': missing source lineage');continue
        ids=[abs(s) for chain in lineage for s in chain]
        if len(set(ids))!=len(ids) or any(type(i) is not int or not 1<=i<=len(segments) for i in ids):problems.append(stage+': duplicate/invalid source segment')
        if stage in ('connected','joined') and set(ids)!=set(range(1,len(segments)+1)):problems.append(stage+': source segment multiplicity changed')
        if any(len(c['points'])!=len(c['knuckles']) for c in curves):problems.append(stage+': point/knuckle counts differ')
        if any(len(ids)!=max(0,len(c['points'])-1) for ids,c in zip(lineage,curves)):problems.append(stage+': source chain length differs')
        for chain_ids,curve in zip(lineage,curves):
            if not chain_ids or any(not 1<=abs(i)<=len(segments) for i in chain_ids):continue
            directed=[(segments[abs(i)-1]['start'],segments[abs(i)-1]['end']) if i>0 else (segments[abs(i)-1]['end'],segments[abs(i)-1]['start']) for i in chain_ids]
            flags=[directed[0][0]['knuckle']]+[left[1]['knuckle'] or right[0]['knuckle'] for left,right in zip(directed,directed[1:])]+[directed[-1][1]['knuckle']]
            if flags!=curve['knuckles']:problems.append(stage+': knuckles do not equal source endpoint OR')
            if not exact(curve['points'][0],directed[0][0]['position']) or not exact(curve['points'][-1],directed[-1][1]['position']):problems.append(stage+': signed source orientation disagrees with outer endpoints')
    return problems


def independent(report,name,item,query,a,b):
    expected=fixtures.expected_segments(item,query)
    if expected is not None:
        actual=[[s['start']['position'],s['end']['position']] for s in a['segments']]
        agrees=segment_bag(actual)==segment_bag(expected);state='pass' if agrees else 'fail'
        old=[[s['start']['position'],s['end']['position']] for s in b['segments']]
        reason='Analytic flat-polygon intersection; multiset preserves duplicate geometry. Ordered source topology is checked separately.'
        if not agrees and item['id']=='layers' and query['id'] in ('hydro','explicit-reverse'):
            restored=[[([.25,4,0] if point==[.25,4.000000476837158,0] else point) for point in segment] for segment in actual]
            if segment_bag(restored)==segment_bag(expected) and exact(actual,old):
                state='review';reason='Confirmed shared subdivision/interpolation rounding at the second planar quad centre: analytic Y=4, native and Pascal Y=4.000000476837158; absolute error 4.76837158203125e-7. Other analytic discrepancies remain failures.'
        report.case(name+'.analytic-raw-segments',status=state,actual=actual,pascal=old,expected=expected,reason=reason)
    if item['id']=='cube' and query['id'] in ('station','buttock','waterline','coplanar-face'):
        report.case(name+'.analytic-closed-rectangle',len(a['curves'])==1 and a['curves'][0]['closed'] and len(a['curves'][0]['points'])==9)
    if item['id']=='layers':
        roots={'standard':[0],'hydro':[1],'explicit-reverse':[1,0],'explicit-empty':[]}[query['id']]
        report.case(name+'.layer-root-selection',a['selected_control_faces']==roots,expected=roots)
    observations={
        'coincident-disconnected':(2,1,'Topology assembly retains two separate coincident sheets. The later geometric join merges them, disregarding source connectivity.'),
        'tiny-and-large':(2,1,'Two mathematical intersections exist; the inherited count>1 filter deletes a short component with squared bbox diagonal below 0.001.'),
        'gap-below':(2,1,'Two mathematical sections separated by 0.0625 are joined because the threshold 0.01 applies to squared distance, not distance.')}
    if item['id'] in observations:
        before,after,reason=observations[item['id']]
        observed=len(a['connected_curves'])==before and len(a['curves'])==after
        report.case(name+'.inherited-geometry-policy',status='review' if observed else 'fail',reason=reason,connected_count=len(a['connected_curves']),final_count=len(a['curves']))
    if item['id'] in ('tiny-alone','small-and-large','gap-above'):
        wanted={'tiny-alone':1,'small-and-large':2,'gap-above':2}[item['id']]
        report.case(name+'.component-retention',len(a['curves'])==wanted,expected=wanted)
    if item['id'].startswith('tiny-threshold-'):
        width=item['geometry']['points'][2]['position'][1];squared=fixtures.f32(width*width);expected_count=1 if squared<.001 else 2
        report.case(name+'.squared-bbox-threshold',len(a['curves'])==expected_count,width=width,single_squared_bbox=squared,expected_components=expected_count)
    if item['id']=='coincident-disconnected':
        edge_sets=[{s[k]['edge'] for s in a['segments'] if s['root_control_face']==r for k in ('start','end')} for r in (0,1)]
        report.case(name+'.distinct-coincident-topology',bool(edge_sets[0]) and bool(edge_sets[1]) and not(edge_sets[0]&edge_sets[1]))


def exercise(report,prefix,inputs,native,pascal,folder):
    folder.mkdir(parents=True,exist_ok=False);ids=[i['id'] for i in inputs['cases']]
    report.case(prefix+'.native-order',ids==[c['id'] for c in native['cases']]);report.case(prefix+'.pascal-order',ids==[c['id'] for c in pascal['cases']])
    na={c['id']:c for c in native['cases']};pa={c['id']:c for c in pascal['cases']}
    for item in inputs['cases']:
        name=prefix+'.'+item['id'];a=na[item['id']];b=pa[item['id']]
        if a.get('status')!='ok' or b.get('status')!='ok':report.case(name,status='error',native=a,pascal=b);continue
        if item['kind']=='join':
            is_prepend=item['id'] in ('prepend-start','prepend-end');reference={'curves':pa['corrected-prepend-reference']['curves']} if is_prepend else None
            compare(report,name+'.curves-pascal',{'curves':a['curves']},{'curves':b['curves']},folder,role='native-pascal',intended_prepend=is_prepend,corrected_reference=reference)
            report.case(name+'.same-instance-repeat',a['repeat_equal'])
            if 'expected_join' in item:
                selected=[{k:c[k] for k in ('points','knuckles','fragments')} for c in a['curves']]
                report.case(name+'.independent-shared-endpoint',exact(selected,item['expected_join']),actual=selected,expected=item['expected_join'])
            if is_prepend:
                samples=a['curves'][0]['samples'];old=b['curves'][0]['samples'];checks=[]
                for i,t in enumerate(samples['parameters']):
                    if t>=.75:checks.append(samples['values'][i][1:]==[0,0] and 0<=samples['values'][i][0]<=1)
                report.case(name+'.independent-straight-after-knuckle',all(checks),native_samples=samples['values'],legacy_samples=old['values'],parameters=samples['parameters'])
                report.case(name+'.corrected-original-spline-causal-reference',exact({'curves':a['curves']},reference))
            desired={'join-gap-below':1,'join-gap-above':2,'join-equal':2,'join-threshold-minus':1,'join-threshold-plus':2,'force-gap':1,'closed-skip':2}
            if item['id'] in desired:report.case(name+'.independent-join-threshold',len(a['curves'])==desired[item['id']],expected=desired[item['id']])
            if item['id'].startswith('closed-threshold-'):
                gap=item['chains'][0]['points'][-1][0];wanted=2 if gap<1e-5 else 1
                report.case(name+'.independent-closed-threshold',len(a['curves'])==wanted,endpoint_distance=gap,expected_curve_count=wanted)
            if item['id']=='early-candidate':report.case(name+'.first-near-candidate',a['curves'][0]['points']==[[0,0,0],[1,0,0],[2,0,0]],reason='Observed scan-order policy; not a globally closest endpoint search.')
            continue
        mesh_issues=subdivision.topology_problems(a['mesh']);report.case(name+'.mesh-topology',not mesh_issues,problems=mesh_issues)
        report.case(name+'.plane-order',[p['id'] for p in a['planes']]==[q['id'] for q in item['planes']]==[p['id'] for p in b['planes']])
        for query,x,y in zip(item['planes'],a['planes'],b['planes']):
            plane_name=name+'.'+query['id']
            if x.get('status')!='ok' or y.get('status')!='ok':report.case(plane_name,status='error',native=x,pascal=y);continue
            compare(report,plane_name+'.sections-pascal',{k:x[k] for k in FIELDS},{k:y[k] for k in FIELDS},folder,role='native-pascal')
            report.case(plane_name+'.same-instance-repeat',x['repeat_equal'])
            issues=topology_issues(a['mesh'],x);report.case(plane_name+'.source-topology',not issues,problems=issues)
            independent(report,plane_name,item,query,x,y)
    for check in native.get('api_checks',[]):report.case(prefix+'.api.'+check['id'],status=check['status'])
    report.data.setdefault('oracle_metadata',{})[prefix]=pascal.get('metadata')
    report.data.setdefault('coverage',{})[prefix]={
        'input_case_count':len(inputs['cases']),
        'direct_join_case_count':sum(c['kind']=='join' for c in inputs['cases']),
        'surface_query_count':sum(len(c.get('planes',[])) for c in inputs['cases']),
        'raw_segment_count':sum(len(q.get('segments',[])) for c in native['cases'] for q in c.get('planes',[])),
        'near_duplicate_endpoints_removed':sum(q.get('diagnostics',{}).get('near_duplicate_endpoints_removed',0) for c in native['cases'] for q in c.get('planes',[]))}


def observer_check(report,prefix,inputs,enabled,probe,out):
    data=copy.deepcopy(inputs)
    for case in data['cases']:
        for query in case.get('planes',[]):query['observe']=False
    path=out/'input.json';out.mkdir();leaf.dump(path,data);disabled=oracle.run(probe,path,out/'disabled')
    report.case(prefix+'.observer-case-order',[c['id'] for c in disabled['cases']]==[c['id'] for c in enabled['cases']])
    for a,b in zip(disabled['cases'],enabled['cases']):
        if a['status']!='ok' or b['status']!='ok':report.case(prefix+'.'+a['id']+'.observer',status='error',disabled=a,enabled=b);continue
        if 'planes' not in a:continue
        report.case(prefix+'.'+a['id']+'.observer-plane-order',[q['id'] for q in a['planes']]==[q['id'] for q in b['planes']])
        for x,y in zip(a['planes'],b['planes']):
            compare(report,prefix+'.'+a['id']+'.'+x['id']+'.observer',{'curves':x.get('curves',[]),'found':x.get('found'),'status':x['status']},
                {'curves':y.get('curves',[]),'found':y.get('found'),'status':y['status']},out)
            report.case(prefix+'.'+a['id']+'.'+x['id']+'.observer-disabled-flag',x.get('observer_enabled') is False)


def threshold_inputs(report,inputs):
    for epsilon in (1e-5,1e-4):
        values=[]
        for step in (-1,0,1):
            item=next(c for c in inputs['cases'] if c['id']==f'epsilon-{epsilon:g}-{step:+d}')
            values.append(fixtures.f32(item['planes'][0]['coefficients'][3]))
        bits=[leaf.bits(v) for v in values]
        report.case(f'epsilon-input-{epsilon:g}',len(set(bits))==3 and bits[1]-bits[0]==1 and bits[2]-bits[1]==1,single_values=values,single_bits=bits,
            reason='Plane boundary is at zero, so nearby Single values survive JSON and native/old input conversion.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--qt-source',type=Path,default=WORKSPACE/'FreeShip-Qt');p.add_argument('--runtime',type=Path);p.add_argument('--reuse-probe',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--small-only',action='store_true')
    targeted=p.add_mutually_exclusive_group()
    targeted.add_argument('--simplify-only',action='store_true',help='Targeted spline simplification and three existing section regression fixtures')
    targeted.add_argument('--linesplan-only',action='store_true',help='Targeted curve sampling, projection and mirror checks');args=p.parse_args()
    source=args.source.resolve(strict=True);qt_source=args.qt_source.resolve(strict=True);out=args.output.resolve();leaf.ensure_private(out,source,qt_source);out.mkdir(parents=True,exist_ok=False)
    models=[m.resolve(strict=True) for m in (args.model or leaf.DEFAULT_MODELS)];report=Report();source_before=None;tests={};native_before={};pilot={};protected={};distributions={};runtime=None
    if args.simplify_only:simplifier.configure_report(report)
    if args.linesplan_only:linesplan.configure_report(report)
    try:
        source_before=common.fingerprint(source)[0];report.data['legacy_source_sha256']=source_before
        tests={f:common.digest_file(ROOT/f) for f in TEST_FILES};report.data['test_sha256']=tests
        native_before=leaf.native_source_hashes(qt_source);report.data['native_source_sha256']=native_before
        pilot=leaf.pilot_source_hashes();report.data['pilot_source_sha256']=pilot
        for path in [*models,source/'FreeShip.exe',WORKSPACE/'FreeShip-QtPilot/dist/FreeShipQtPilot.exe',WORKSPACE/'FreeShip-QtPilot/dist/bridge/FreeShipGeometryBridge.exe']:protected[str(path)]=common.digest_file(path)
        for path in (qt_source/'dist',WORKSPACE/'FreeShip-QtPilot/dist'):distributions[str(path)]=leaf.files_hashes(path)
        report.data['protected_sha256']=protected;report.data['protected_distributions_sha256']=distributions
        runtime=args.runtime.resolve(strict=True) if args.runtime else build_native(qt_source,out)
        if qt_source/'build' not in runtime.parents:leaf.ensure_private(runtime,source,qt_source)
        report.data['native_manifest']=validate_runtime(runtime,qt_source)
        probe_folder=args.reuse_probe.resolve(strict=True) if args.reuse_probe else out/'oracle-build'
        if not args.reuse_probe:oracle.build(source,qt_source,probe_folder)
        probe,build=validate_probe(probe_folder,source,qt_source);report.data['oracle_build']=build
        inputs=linesplan.cases() if args.linesplan_only else simplifier.cases() if args.simplify_only else fixtures.cases()
        if not (args.simplify_only or args.linesplan_only):threshold_inputs(report,inputs)
        path=out/'input.json';leaf.dump(path,inputs)
        expected=oracle.run(probe,path,out/'pascal-small');actual=native_run(runtime,path,out/'native-small')
        if args.linesplan_only:
            linesplan.exercise(report,inputs,actual,expected,out/'comparisons-small')
            repeated=native_run(runtime,path,out/'native-small-process-repeat');report.case('linesplan.independent-process-repeat',actual==repeated)
        elif args.simplify_only:
            simplifier.exercise(report,inputs,actual,expected,out/'comparisons-small')
            repeated=native_run(runtime,path,out/'native-small-process-repeat');report.case('simplify.independent-process-repeat',actual==repeated)
        else:
            exercise(report,'small',inputs,actual,expected,out/'comparisons-small')
            repeated=native_run(runtime,path,out/'native-small-process-repeat');report.case('small.independent-process-repeat',actual==repeated)
            observer_check(report,'small',inputs,expected,probe,out/'observer-small')
            report.case('legacy-partial-record-move-branch',status='not_run',reason='No accepted simple-face fixture triggers repeated hits on the same edge. Native diagnostic removal count is zero; malformed repeated-vertex faces are rejected. The unsafe original Move is not claimed runtime-covered.')
            report.case('same-edge-connect-distance-exact-boundary',status='not_run',reason='Same-edge connections are exercised, but no fixture produces two interpolated same-edge endpoints at adjacent Single distances around 1e-4. Plane-side and closed-chain epsilon cases do not establish this branch boundary.')
            if args.small_only:report.case('real-model-sections',status='not_run',reason='Explicit small-only diagnostic')
            else:
                exported=oracle.export_models(probe,models,out/'model-oracle');report.data['models']=exported['models']
                for model in exported['models']:report.case(model['prefix']+'.original-fbm-versus-reconstruction',model['direct_vs_reconstructed_exact'])
                actual=native_run(runtime,Path(exported['input_path']),out/'native-models');exercise(report,'model',exported['input'],actual,exported['results'],out/'comparisons-model')
                repeated=native_run(runtime,Path(exported['input_path']),out/'native-models-process-repeat');report.case('model.independent-process-repeat',actual==repeated)
                observer_check(report,'model',exported['input'],exported['results'],probe,out/'observer-model')
    except Exception as error:report.case('execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        if source_before:report.case('legacy-source-preserved',status='pass' if source_before==common.fingerprint(source)[0] else 'error')
        for path,digest in protected.items():report.case('protected-file:'+path,Path(path).is_file() and common.digest_file(Path(path))==digest)
        for path,digests in distributions.items():report.case('protected-distribution:'+path,leaf.files_hashes(Path(path))==digests)
        if tests:report.case('test-sources-stable',status='pass' if tests=={f:common.digest_file(ROOT/f) for f in TEST_FILES} else 'error')
        if native_before:report.case('native-sources-stable',status='pass' if native_before==leaf.native_source_hashes(qt_source) else 'error')
        if pilot:report.case('accepted-pilot-source-preserved',pilot==leaf.pilot_source_hashes())
        if runtime and 'native_manifest' in report.data:
            try:validate_runtime(runtime,qt_source);report.case('native-runtime-preserved',True)
            except Exception as error:report.case('native-runtime-preserved',status='error',reason=str(error))
        report.finish();leaf.dump(out/'report.json',report.data);leaf.dump(out/'issues.json',[c for c in report.data['cases'] if c['status']!='pass'])
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'fail':1,'review':2,'not_run':2,'error':3}[report.data['status']]


if __name__=='__main__':raise SystemExit(main())
