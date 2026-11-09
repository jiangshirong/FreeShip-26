"""Isolated persistent-model, Qt workflow and four-view migration regression.

All edits and saves target private copies. This invokes production protocols and
Qt actions, not the user's desktop. Visual/native acceptance remains explicit.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import importlib.util
import json
import math
import os
from pathlib import Path
import queue
import shutil
import struct
import subprocess
import sys
import threading
import time
import uuid

import freeship_ui_test as common
import native_flicker_test as native
from dxf_geometry import entities as dxf_entities

ROOT = Path(__file__).resolve().parent
QT = ROOT.parent / 'FreeShip-Qt'
PILOT = ROOT.parent / 'FreeShip-QtPilot'
DEFAULT_BASELINE = ROOT / 'reports/qt-startup-delivery-20261101'
DEFAULT_MODELS = [ROOT.parent / 'FreeShip-MCP/examples/box.fbm',
                  ROOT.parent / 'FreeShip-MCP/examples/synthetic-hull.fbm',
                  Path('D:/桌面文件夹/学习/大学课程/过考试/船舶设计原理/9a/9a-Женя.fbm')]
STATE_KEYS = ('document_id', 'revision', 'modified', 'can_undo', 'can_redo')
GEOMETRY_KEYS = ('units', 'bounds', 'settings', 'control_points', 'control_edges',
                 'control_faces', 'layers', 'triangles')


class Report:
    def __init__(self):
        self.data = {'status': 'error', 'cases': [], 'models': [], 'limitations': [
            'FBM and edit state still use Pascal; native-enabled runtimes generate complete lines-plan scenes and DXF in C++.',
            'Qt events and production actions run in hidden private instances; native desktop input is not tested.',
            'Offscreen image timings include readback and do not measure display frame rate.',
            'Frozen legacy scenes require byte equality. Native display corrections are separately evidenced; current DXF comparisons use exact entities without numerical tolerance.',
            'Control point IDs are scoped to a document and are not promised across a new file load.']}

    def case(self, name, ok=None, *, status=None, **details):
        self.data['cases'].append({'id': name, 'status': status or ('pass' if ok else 'fail'), **details})
        return bool(ok)

    def compare(self, name, actual, expected):
        actual, expected = Path(actual), Path(expected)
        same_bytes = common.digest_file(actual) == common.digest_file(expected)
        same = dxf_entities(actual) == dxf_entities(expected) if actual.suffix.lower() == '.dxf' else same_bytes
        self.case(name, same, actual=str(actual), expected=str(expected), byte_equal=same_bytes)
        if same and not same_bytes:
            self.case(name + '.bytes', status='review', reason='DXF text differs; complete entities match exactly; no numerical tolerance.')

    def compare_frozen_dxf(self, name, actual, expected):
        """Only frozen legacy crease grouping/direction has this review path.

        Current service/session/window exports still use compare(), requiring
        complete entity equality. No general DXF expectation is relaxed.
        """
        actual,expected=Path(actual),Path(expected)
        actual_entities,expected_entities=dxf_entities(actual),dxf_entities(expected)
        if actual_entities==expected_entities:
            self.compare(name,actual,expected)
            return
        def layer(entity):
            return dict(entity[1]).get('8') if entity[0] in ('POLYLINE','RAW') else None
        def affected(entity):
            return layer(entity)=='Knuckle_lines' and (entity[0]=='POLYLINE' or
                (entity[0]=='RAW' and dict(entity[1]).get('0')=='LINE'))
        other_actual=Counter({e:n for e,n in actual_entities.items() if not affected(e)})
        other_expected=Counter({e:n for e,n in expected_entities.items() if not affected(e)})
        a=knuckle_network(actual);b=knuckle_network(expected)
        same_other=other_actual==other_expected
        same_lines=knuckle_line_signatures(actual_entities)==knuckle_line_signatures(expected_entities)
        same_segments=a['segments']==b['segments']
        same_graph=a['adjacency']==b['adjacency'] and a['summary']==b['summary']
        allowed=same_other and same_lines and same_segments and same_graph
        differences={'actual_extra':[[e,n] for e,n in (actual_entities-expected_entities).items()],
                     'expected_extra':[[e,n] for e,n in (expected_entities-actual_entities).items()],
                     'actual_network':a['summary'],'expected_network':b['summary']}
        evidence=actual.parent/'frozen-dxf-entity-differences.json'
        evidence.write_text(json.dumps(differences,ensure_ascii=False,indent=2),encoding='utf8')
        self.case(name,status='review' if allowed else 'fail',exact_entity_equal=False,
            byte_equal=common.digest_file(actual)==common.digest_file(expected),differences=str(evidence),
            reason='Frozen legacy crease-loop segmentation and line direction depended on pointer order/shared-edge swaps. Exact entity mismatch is retained for review only when all other entities, exact line endpoint pairs/attributes, styled segments, multiplicities and graph connections match.')
        self.case(name+'.all-other-entities-exact',same_other)
        self.case(name+'.knuckle-line-direction-only',same_lines)
        self.case(name+'.knuckle-segments-coordinates-style-and-multiplicity-exact',same_segments)
        self.case(name+'.knuckle-closed-loops-and-branches-exact',same_graph,actual=a['summary'],expected=b['summary'])

    def finish(self):
        statuses = [case['status'] for case in self.data['cases']]
        self.data['status'] = ('error' if 'error' in statuses else 'fail' if 'fail' in statuses
                               else 'partial' if any(s != 'pass' for s in statuses) else 'pass')
        self.data['counts'] = {key: statuses.count(key) for key in ('pass', 'fail', 'review', 'not_run', 'error')}


def private_environment(folder):
    env = os.environ.copy()
    for name in ('APPDATA', 'LOCALAPPDATA', 'TEMP', 'TMP'):
        path = folder / name
        path.mkdir(parents=True, exist_ok=True)
        env[name] = str(path)
    env['QT_QPA_PLATFORM'] = 'windows' if os.name == 'nt' else 'offscreen'
    return env


def knuckle_line_signatures(entities):
    """Permit only endpoint reversal for legacy crease LINE records."""
    result=Counter()
    for entity,count in entities.items():
        if entity[0]!='RAW':continue
        fields=dict(entity[1])
        if fields.get('0')!='LINE' or fields.get('8')!='Knuckle_lines':continue
        metadata=tuple((code,value) for code,value in entity[1] if code not in ('10','20','30','11','21','31'))
        start=tuple(fields.get(code,'0') for code in ('10','20','30'))
        end=tuple(fields.get(code,'0') for code in ('11','21','31'))
        result[(metadata,tuple(sorted((start,end))))]+=count
    return result


def knuckle_network(path):
    """Independent raw DXF parser for exact crease segments and connectivity.

    This does not use dxf_geometry.entities or its normalized vertex sequence.
    Split polyline boundaries are ignored, never coordinates, style or edges.
    """
    lines=Path(path).read_text('utf8').splitlines()
    if len(lines)%2:raise ValueError('Odd DXF line count')
    records=[];current=[]
    for code,value in zip(lines[::2],lines[1::2]):
        if code=='0' and current:records.append(current);current=[]
        current.append((code,value))
    if current:records.append(current)
    segments=Counter();adjacency=defaultdict(Counter);index=0
    while index<len(records):
        header=records[index];index+=1;fields=dict(header)
        if fields.get('8')!='Knuckle_lines':continue
        kind=fields.get('0')
        if kind not in ('POLYLINE','LINE'):raise ValueError('Unexpected frozen crease entity type')
        flags=int(fields.get('70','0'));style=[]
        for code,value in header:
            if code=='0' or (kind=='LINE' and code in ('10','20','30','11','21','31')):continue
            if code=='70':
                if flags & ~1:style.append((code,str(flags & ~1)))
            else:style.append((code,value))
        style=tuple(style);points=[]
        if kind=='LINE':
            points=[(fields['10'],fields['20'],fields.get('30','0')),
                    (fields['11'],fields['21'],fields.get('31','0'))]
        else:
            while index<len(records) and dict(records[index]).get('0')=='VERTEX':
                vertex=records[index];index+=1;values=dict(vertex)
                if any(code not in ('0','8','10','20','30') for code,_ in vertex):
                    raise ValueError('Unsupported frozen crease vertex attributes')
                if values.get('8')!='Knuckle_lines' or '10' not in values or '20' not in values:
                    raise ValueError('Invalid crease vertex')
                points.append((values['10'],values['20'],values.get('30','0')))
            if index>=len(records) or dict(records[index]).get('0')!='SEQEND':raise ValueError('Unterminated crease polyline')
            index+=1
        if len(points)<2:raise ValueError('Empty crease polyline')
        if flags&1 and points[-1]!=points[0]:points.append(points[0])
        # 66 is a structural vertex-follow flag. Preserve it per segment, while
        # graph nodes still join visible LINE/POLYLINE vertices of the same style.
        graph_style=tuple((code,value) for code,value in style if code!='66')
        for start,end in zip(points,points[1:]):
            segments[(style,tuple(sorted((start,end))))]+=1
            adjacency[(graph_style,start)][(graph_style,end)]+=1
            adjacency[(graph_style,end)][(graph_style,start)]+=1
    if not segments:raise ValueError('No Knuckle_lines available for legacy review')
    seen=set();components=0
    for vertex in adjacency:
        if vertex in seen:continue
        components+=1;stack=[vertex]
        while stack:
            current=stack.pop()
            if current in seen:continue
            seen.add(current);stack.extend(neighbor for neighbor in adjacency[current] if neighbor not in seen)
    degrees=Counter(sum(neighbors.values()) for neighbors in adjacency.values())
    summary={'vertices':len(adjacency),'segments_with_multiplicity':sum(segments.values()),
             'components':components,'degree_histogram':dict(sorted(degrees.items())),
             'cycle_rank':sum(segments.values())-len(adjacency)+components}
    return {'segments':segments,'adjacency':dict(adjacency),'summary':summary}


def exercise_legacy_dxf_oracle(out,report):
    folder=out/'legacy-dxf-oracle';folder.mkdir()
    def fixture(name,polylines,color='7'):
        pairs=[('0','SECTION'),('2','ENTITIES')]
        for points,closed in polylines:
            pairs.extend([('0','POLYLINE'),('8','Knuckle_lines'),('62',color),('66','1')])
            if closed:pairs.append(('70','1'))
            for x,y in points:pairs.extend([('0','VERTEX'),('8','Knuckle_lines'),('10',str(x)),('20',str(y))])
            pairs.append(('0','SEQEND'))
        pairs.extend([('0','ENDSEC'),('0','EOF')]);path=folder/(name+'.dxf')
        path.write_text('\n'.join(value for pair in pairs for value in pair)+'\n',encoding='utf8')
        return path
    corners=[(0,0),(1,0),(1,1),(0,1)]
    split=fixture('split',[(corners[:3],False),([corners[2],corners[3],corners[0]],False)])
    joined=fixture('closed',[(corners,True)])
    network=knuckle_network(split);other=knuckle_network(joined)
    report.case('legacy-dxf-oracle-retains-entity-mismatch',dxf_entities(split)!=dxf_entities(joined))
    report.case('legacy-dxf-oracle-equivalent-split-closed-network',network==other and network['summary']['cycle_rank']==1)
    bad=[('coordinate',fixture('coordinate',[([(0,0),(2,0),(1,1),(0,1)],True)])),
         ('color',fixture('color',[(corners,True)],'3')),
         ('missing-closure',fixture('missing-closure',[(corners,False)])),
         ('multiplicity',fixture('multiplicity',[(corners+[corners[0],corners[1],corners[0]],False)])),
         ('connectivity',fixture('connectivity',[([corners[0],corners[2],corners[1],corners[3]],True)]))]
    for name,path in bad:
        changed=knuckle_network(path)
        report.case('legacy-dxf-oracle-rejects-'+name,network['segments']!=changed['segments'] and network['adjacency']!=changed['adjacency'])
    for name,before,after in (('vertex-follow-flag','66\n1\n','66\n0\n'),
                              ('polyline-extra-flag','66\n1\n','66\n1\n70\n8\n')):
        path=folder/(name+'.dxf');path.write_text(split.read_text('utf8').replace(before,after),encoding='utf8')
        report.case('legacy-dxf-oracle-rejects-'+name,knuckle_network(path)['segments']!=network['segments'])
    def line_fixture(name,start,end,color='7',flag=None):
        pairs=[('0','SECTION'),('2','ENTITIES'),('0','LINE'),('8','Knuckle_lines'),('62',color)]
        if flag is not None:pairs.append(('70',str(flag)))
        pairs.extend([('10',str(start[0])),('20',str(start[1])),('11',str(end[0])),('21',str(end[1])),('0','ENDSEC'),('0','EOF')])
        path=folder/(name+'.dxf');path.write_text('\n'.join(value for pair in pairs for value in pair)+'\n',encoding='utf8')
        return path
    line=line_fixture('line',(0,0),(2,1));reverse=line_fixture('line-reverse',(2,1),(0,0))
    line_signature=knuckle_line_signatures(dxf_entities(line))
    report.case('legacy-dxf-oracle-line-reversal-keeps-exact-mismatch',dxf_entities(line)!=dxf_entities(reverse))
    report.case('legacy-dxf-oracle-exact-line-reversal',line_signature==knuckle_line_signatures(dxf_entities(reverse)) and knuckle_network(line)==knuckle_network(reverse))
    for name,path in (('coordinate',line_fixture('line-coordinate',(0,0),(2,2))),
                      ('color',line_fixture('line-color',(0,0),(2,1),'3')),
                      ('flag',line_fixture('line-flag',(0,0),(2,1),flag=8))):
        report.case('legacy-dxf-oracle-rejects-line-'+name,line_signature!=knuckle_line_signatures(dxf_entities(path)))


class Service:
    """One owned process, one outstanding request, bounded IO and full evidence."""
    def __init__(self, executable, folder, timeout=120, native_linesplan=None):
        self.folder, self.timeout = folder, timeout
        self.native_linesplan = ((executable.parent.parent/'FreeShipNativeLinesplan.exe').is_file() or (executable.parent/'FreeShipNativeLinesplan.exe').is_file()
                                 if native_linesplan is None else native_linesplan)
        folder.mkdir(parents=True, exist_ok=True)
        self.events, self.counter, self.state = queue.Queue(), 0, {}
        self.prepared_save = None
        self.transcript = (folder / 'protocol.jsonl').open('w', encoding='utf8')
        self.stdout = (folder / 'stdout.log').open('wb')
        self.stderr = (folder / 'stderr.log').open('wb')
        self.process = subprocess.Popen([str(executable), '--serve'], cwd=folder,
            env=private_environment(folder), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, creationflags=common.FLAGS)
        self.readers = [threading.Thread(target=self._read_stdout, daemon=True),
                        threading.Thread(target=self._read_stderr, daemon=True)]
        for reader in self.readers:
            reader.start()

    def _read_stdout(self):
        try:
            for line in iter(self.process.stdout.readline, b''):
                self.stdout.write(line); self.stdout.flush()
                if line.lstrip().startswith(b'{'):
                    try: self.events.put(json.loads(line.decode('utf8')))
                    except Exception as exc: self.events.put(exc)
        except Exception as exc:
            self.events.put(exc)
        finally:
            self.events.put(EOFError('Service stdout closed'))

    def _read_stderr(self):
        for line in iter(self.process.stderr.readline, b''):
            self.stderr.write(line); self.stderr.flush()

    def command(self, method, *, output=None, raw_nonfinite=False, escaped_unicode=False, **parameters):
        self.counter += 1
        request = {'id': self.counter, 'method': method}
        request['native_linesplan'] = self.native_linesplan
        if method not in ('load', 'quit') and self.state:
            request.update(expected_document_id=self.state['document_id'],
                           expected_revision=self.state['revision'])
        request.update(parameters)
        # New native services require the exact prepared receipt and an existing
        # destination. Legacy oracle services retain their original protocol.
        if method == 'mark_saved' and self.prepared_save:
            request.setdefault('save_receipt',self.prepared_save['save_receipt'])
            request.setdefault('saved_path',request.get('recovery_source',self.prepared_save['saved_file']))
        if output is not None:
            request['output'] = str(output.resolve())
        payload = json.dumps(request, ensure_ascii=escaped_unicode, allow_nan=False)
        if raw_nonfinite:
            # JSON exponent overflow tests the actual numeric parser, not Python's NaN extension.
            payload = payload.replace('"EXPONENT_OVERFLOW"', '1e309')
        start = time.perf_counter()
        self.transcript.write(json.dumps({'request': request, 'wire_request': payload}, ensure_ascii=False) + '\n')
        self.transcript.flush()
        self.process.stdin.write((payload + '\n').encode('utf8')); self.process.stdin.flush()
        try: response = self.events.get(timeout=self.timeout)
        except queue.Empty: raise TimeoutError(f'Service timed out: {method}; {self.folder}')
        if isinstance(response, Exception): raise response
        # The C++ JSON parser rejects exponent overflow before it can recover an
        # ID. Only the explicitly malformed-wire diagnostic may accept this
        # parse-level error; normal requests still require the exact ID.
        parse_rejection = (raw_nonfinite and response.get('id') == 0 and response.get('ok') is False
                           and response.get('error','').startswith('Invalid document JSON:'))
        if response.get('id') != request['id'] and not parse_rejection:
            raise RuntimeError(f'Service response id mismatch: {response.get("id")} != {request["id"]}')
        if type(response.get('ok')) is not bool:
            raise ValueError('Service response lacks Boolean ok')
        self.transcript.write(json.dumps({'response': response, 'wall_ms': (time.perf_counter()-start)*1000}, ensure_ascii=False) + '\n')
        self.transcript.flush()
        if response['ok'] and all(key in response for key in STATE_KEYS):
            self.state = {key: response[key] for key in STATE_KEYS}
        if response['ok'] and method in ('load','new','import_model'):self.prepared_save=None
        if response['ok'] and method=='save' and 'save_receipt' in response:self.prepared_save=response
        return response

    def __enter__(self): return self

    def __exit__(self, exc_type, exc, traceback):
        try:
            if self.process.poll() is None:
                if exc_type is None:
                    response = self.command('quit')
                    if not response['ok']: raise RuntimeError('Service rejected quit')
                    self.process.wait(timeout=5)
                else:
                    self.process.terminate()
        finally:
            if self.process.poll() is None:
                self.process.kill()
            self.process.wait(timeout=5)
            for reader in self.readers: reader.join(timeout=2)
            self.transcript.close(); self.stdout.close(); self.stderr.close()


def require_ok(response):
    if not response['ok']:
        raise RuntimeError('Service rejected a required operation: ' + str(response.get('error')))
    return response


def artifact(response, key, output):
    path = Path(response[key]).resolve(strict=True)
    if not path.is_file() or output.resolve() not in path.parents:
        raise ValueError('Artifact escaped the requested private output: ' + str(path))
    return path


def geometry(snapshot): return {key: snapshot[key] for key in GEOMETRY_KEYS}


def validate_snapshot(report, prefix, response, output, oracle=None):
    require_ok(response)
    paths = {key: artifact(response, key, output) for key in ('model', 'scene', 'dxf')}
    model = json.loads(paths['model'].read_text('utf8'))
    report.case(prefix + '.snapshot-format', model.get('format') == 'FreeShipModelSnapshot' and model.get('version') == 1)
    report.case(prefix + '.snapshot-state', model['document_id'] == response['document_id'] and model['revision'] == response['revision'] and type(model['revision']) is int and model['revision'] > 0)
    groups = ('control_points', 'control_edges', 'control_faces', 'layers')
    for group in groups:
        ids = [item['id'] for item in model[group]]
        report.case(prefix + '.' + group + '-unique-ids', bool(ids) and all(type(i) is int and i > 0 for i in ids) and len(ids) == len(set(ids)))
    points = {item['id'] for item in model['control_points']}
    layers = {item['id'] for item in model['layers']}
    report.case(prefix + '.edge-references', all(e['a'] in points and e['b'] in points for e in model['control_edges']))
    report.case(prefix + '.face-references', all(f['layer'] in layers and len(f['points']) >= 3 and all(i in points for i in f['points']) for f in model['control_faces']))
    report.case(prefix + '.triangle-references', bool(model['triangles']) and all(t['layer'] in layers and len(t['p']) == 3 for t in model['triangles']))
    coordinates = [p['p'] for p in model['control_points']] + [p for triangle in model['triangles'] for p in triangle['p']]
    report.case(prefix + '.finite-three-dimensional-coordinates', all(len(p) == 3 and all(type(v) in (int,float) and math.isfinite(v) for v in p) for p in coordinates))
    bounds = model['bounds']; settings = model['settings']
    report.case(prefix + '.ordered-bounds', all(math.isfinite(a) and math.isfinite(b) and a <= b for a,b in zip(bounds['min'], bounds['max'])) and len(bounds['min']) == len(bounds['max']) == 3)
    # The baseline core uses Single precision for both the extent and waterline sum.
    waterline = struct.unpack('<f', struct.pack('<f', bounds['min'][2] + settings['draft']))[0]
    report.case(prefix + '.absolute-waterline', settings['waterline_z'] in (waterline, bounds['min'][2]+settings['draft']))
    diagnostics = model.get('diagnostics', {})
    if 'source_triangle_count' in diagnostics:
        report.case(prefix + '.all-source-triangles-serialized', len(model['triangles']) == diagnostics['source_triangle_count'])
    else:
        report.case(prefix + '.all-source-triangles-serialized', status='not_run', reason='Core-side mesh count unavailable')
    report.case(prefix + '.no-legacy-form-or-viewport', diagnostics.get('forms_created') == 0 and diagnostics.get('viewports_created') == 0)
    if oracle:
        for group in groups:
            report.case(prefix + '.independent-core-count-' + group, len(model[group]) == oracle[group])
        report.case(prefix + '.independent-core-triangle-count', len(model['triangles']) == oracle['source_triangle_count'])
    return {'response': response, 'paths': paths, 'model': model}


def current_sources(qt):
    files = [qt/'CMakeLists.txt', qt/'build.ps1']
    files += [p for folder in ('src', 'bridge', 'third_party') for p in (qt/folder).rglob('*')
              if p.is_file() and p.suffix.lower() in ('.h','.cpp','.pas','.inc','.py','.json')]
    return {str(p.relative_to(qt)).replace('\\','/'): common.digest_file(p) for p in sorted(files) if p.exists()}


def validate_test_manifest(manifest,editing=False):
    required={'qt/linesplan_tests.cpp','qt/migration_tests.cpp','qt/session_tests.cpp'}
    if editing:required.add('qt/editing_tests.cpp')
    allowed=required|{'qt/editing_tests.cpp','qt/overlay_tests.cpp','qt/background_tests.cpp','qt/display_tests.cpp','qt/auxiliary_tests.cpp','qt/exchange_tests.cpp','qt/fbm_codec_tests.cpp','qt/face_tools_probe.cpp','qt/project_edit_probe.cpp','qt/document_create_probe.cpp','qt/definition_edit_probe.cpp','qt/auxiliary_edit_probe.cpp','qt/background_edit_probe.cpp','qt/analysis_edit_probe.cpp','qt/flowline_probe.cpp','qt/model_ui_tests.cpp','qt/camera_lighting_tests.cpp','qt/shading_tests.cpp','qt/added_mass_probe.cpp','qt/grid_preview_tests.cpp','qt/recovery_tests.cpp','qt/save_protection_tests.cpp','qt/dialog_layout_tests.cpp','qt/preview_image_tests.cpp','qt/underwater_tests.cpp','qt/calculation_chart_tests.cpp'}
    allowed.add('qt/native_subdivision_tests.cpp')
    recorded=manifest.get('test_source_sha256',{})
    if not required.issubset(recorded) or not set(recorded).issubset(allowed):
        raise RuntimeError('Qt runtime test-source manifest has missing or unknown entries')
    for name,digest in recorded.items():
        if common.digest_file(ROOT/name)!=digest:raise RuntimeError('Qt runtime test source is stale: '+name)


def bridge_builder(qt):
    spec = importlib.util.spec_from_file_location('freeship_qt_migration_bridge', qt/'bridge/build_bridge.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def exercise_scene_recipe(args,out,builder,report):
    folder=out/'scene-recipe';folder.mkdir()
    shutil.copy2(args.qt_source/'bridge/SceneHeader.inc',folder/'SceneHeader.inc')
    spec=importlib.util.spec_from_file_location('freeship_qt_scene_recipe',args.qt_source/'bridge/extract_scene.py')
    recipe=importlib.util.module_from_spec(spec)
    previous=sys.modules.get('build_bridge')
    try:
        sys.modules['build_bridge']=builder
        spec.loader.exec_module(recipe)
        recipe.ROOT=folder;recipe.SOURCE=args.source;recipe.build_bridge=builder
        recipe.extract()
    finally:
        if previous is None:sys.modules.pop('build_bridge',None)
        else:sys.modules['build_bridge']=previous
    for name in ('FreeLinesplanScene.pas','scene-origin.json'):
        generated=folder/name;checked_in=args.qt_source/'bridge'/name
        # Source reproducibility tolerates only platform line endings. FSLP
        # numerical scenes always retain the separate strict byte comparator.
        generated_text=generated.read_text('utf8').replace('\r\n','\n').replace('\r','\n')
        checked_text=checked_in.read_text('utf8').replace('\r\n','\n').replace('\r','\n')
        generated_hash=common.digest_file(generated);checked_hash=common.digest_file(checked_in)
        report.case('scene-recipe-reproduces-'+name,generated_text==checked_text,
            comparison='Complete text with CRLF/CR normalized to LF only',
            byte_equal=generated_hash==checked_hash,actual_sha256=generated_hash,
            expected_sha256=checked_hash,actual=str(generated),expected=str(checked_in))


def prepare_bridge(args, out, builder, report):
    folder = args.reuse_build.resolve() if args.reuse_build else out/'bridge-build'
    result = json.loads((folder/'report.json').read_text('utf8')) if args.reuse_build else builder.build(args.source,folder,False)
    if result.get('status') != 'pass' or result.get('source_sha256') != common.fingerprint(args.source)[0]:
        raise RuntimeError('Bridge build failed or source fingerprint is stale: ' + str(folder))
    if result.get('adapter_sha256') != builder.adapter_hashes() or result.get('reference') is not False:
        raise RuntimeError('Bridge adapter or entry type is stale: ' + str(folder))
    report.data['bridge_build'] = result
    return folder


def build_fixture(args, out, builder, report):
    folder = out/'fixture-build'; folder.mkdir()
    with common.temporary_workspace('fs-qt-fixture-') as temp:
        work = Path(temp)
        result = native.build(args.source,work,common.DEFAULT_LAZARUS,folder,'current','standalone',
                              probe=ROOT/'QtMigrationFixture.pas',prepare=builder.adapt_scene)
        shutil.copy2(work/'NativeLinesplanProbe.exe',folder/'QtMigrationFixture.exe')
    for name in ('locale','Themes','Languages'):
        shutil.copytree(args.source/name,folder/name)
    report.data['fixture_build'] = result
    return folder/'QtMigrationFixture.exe'


def prepare_runtime(args, out, bridge, report):
    runtime = out/'runtime'
    if args.runtime:
        source_runtime = args.runtime.resolve()
        shutil.copytree(source_runtime,runtime)
    else:
        command = ['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(args.qt_source/'build.ps1'),
                   '-Deploy','-BuildDir',str(out/'qt-build'),'-DeployDir',str(runtime)]
        with (out/'qt-build.log').open('wb') as log:
            result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,creationflags=common.FLAGS,timeout=300)
        if result.returncode: raise RuntimeError('Qt build failed; see qt-build.log')
    manifest_path = runtime/'build-manifest.json'
    manifest = json.loads(manifest_path.read_text('utf8'))
    expected_sources = current_sources(args.qt_source)
    recorded = {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}
    if recorded != expected_sources: raise RuntimeError('Qt runtime source manifest is stale')
    validate_test_manifest(manifest)
    for name, digest in manifest['binaries_sha256'].items():
        file = (runtime/name).resolve()
        if runtime.resolve() not in file.parents or common.digest_file(file) != digest:
            raise RuntimeError('Qt runtime binary manifest mismatch: ' + name)
    for name in ('FreeShipQt.exe','qt_migration_tests.exe','qt_lines_tests.exe','qt_session_tests.exe'):
        if name not in manifest['binaries_sha256']: raise RuntimeError('Missing executable provenance: '+name)
    deployed = runtime/'bridge';deployed.mkdir(exist_ok=True)
    for name in ('FreeShipGeometryBridge.exe','source-id.txt'):
        shutil.copy2(bridge/name,deployed/name)
    for name in ('locale','Themes','Languages'):
        shutil.copytree(bridge/name,deployed/name,dirs_exist_ok=True)
    report.data['qt_build_manifest'] = manifest
    report.data['runtime_sha256'] = {str(p.relative_to(runtime)):common.digest_file(p) for p in runtime.rglob('*') if p.is_file()}
    return runtime


def run_child(report, prefix, executable, arguments, folder, timeout=180, platform=None):
    folder.mkdir(parents=True,exist_ok=True)
    start=time.perf_counter()
    environment=private_environment(folder)
    if platform:environment['QT_QPA_PLATFORM']=platform
    if environment['QT_QPA_PLATFORM']=='offscreen' and os.name=='nt':
        # The offscreen plugin does not discover Windows fonts automatically.
        # Use the installed fonts for legible evidence; this changes no app setting.
        fonts=Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts'
        if fonts.is_dir():environment['QT_QPA_FONTDIR']=str(fonts)
    result=subprocess.run([str(executable),*[str(arg) for arg in arguments],str(folder)],cwd=folder,
        env=environment,capture_output=True,timeout=timeout,creationflags=common.FLAGS)
    (folder/'process.log').write_bytes(result.stdout+result.stderr)
    (folder/'process-result.json').write_text(json.dumps({
        'returncode':result.returncode,'returncode_hex':hex(result.returncode & 0xffffffff),
        'command':result.args,'wall_ms':(time.perf_counter()-start)*1000,
        'qt_platform':environment['QT_QPA_PLATFORM']},indent=2),encoding='utf8')
    if not (folder/'report.json').is_file():
        raise RuntimeError(f'{prefix}: child exited {result.returncode} ({result.returncode & 0xffffffff:#x}) without report.json; inspect {folder}/process.log and process-result.json')
    child=json.loads((folder/'report.json').read_text('utf8'))
    status=child.get('status')
    allowed={'pass':(0,), 'partial':(0,2), 'fail':(1,), 'error':(1,3)}
    if status not in allowed or result.returncode not in allowed[status]:
        raise ValueError(f'Child exit/status mismatch: {prefix}: {result.returncode}/{status}')
    for item in child.get('cases',[]):
        value=dict(item);name=value.pop('id',value.pop('name','unnamed'))
        if value.get('status') not in ('pass','fail','review','not_run','error'):
            raise ValueError('Unrecognized child case status')
        report.data['cases'].append({'id':prefix+'.'+name,**value})
    if status=='error': report.case(prefix+'.execution',status='error',reason=child.get('error',child))
    elif not child.get('cases'): report.case(prefix+'.empty-report',status='error')
    child['wall_ms']=(time.perf_counter()-start)*1000
    child['executed_path']=str(executable.resolve())
    child['executable_sha256']=common.digest_file(executable)
    child['qt_platform']=environment['QT_QPA_PLATFORM']
    if 'QT_QPA_FONTDIR' in environment:child['qt_font_directory']=environment['QT_QPA_FONTDIR']
    child['command']=[str(executable),*[str(arg) for arg in arguments],str(folder)]
    return child


def frozen_folder(baseline, model_hash):
    data=json.loads((baseline/'report.json').read_text('utf8'))
    matches=[index+1 for index, entry in enumerate(data['models'])
             if data['model_sha256'][entry['model']]==model_hash]
    if len(matches)!=1: raise ValueError('Frozen baseline model missing or ambiguous')
    return baseline/f'model-{matches[0]}'


def exercise_history_budget(out, runtime, model, report):
    folder=out/'history-budget';folder.mkdir()
    private_bridge=folder/'bridge';shutil.copytree(runtime/'bridge',private_bridge)
    settings=private_bridge/'.coordinate-settings';settings.mkdir(exist_ok=True)
    (settings/'FreeShip.ini').write_text('[General]\nMaxUndoMemory=0\n',encoding='utf8')
    private_input=folder/'input.fbm';shutil.copy2(model,private_input)
    with Service(private_bridge/'FreeShipGeometryBridge.exe',folder/'service') as server:
        loaded=validate_snapshot(report,'history-budget.load',server.command('load',path=str(private_input),
            document_id=str(uuid.uuid4()),output=folder/'load'),folder/'load')
        report.case('history-budget-private-setting-applied',loaded['response'].get('history_limit_bytes')==0)
        selected=next(p for p in loaded['model']['control_points'] if not p['locked'])
        revisions=[]
        for i in range(1,5):
            position=list(selected['p']);position[2]=struct.unpack('<f',struct.pack('<f',selected['p'][2]+i*0.01))[0]
            target=folder/f'move-{i}'
            response=require_ok(server.command('move_point',point_id=selected['id'],position=position,output=target))
            revisions.append(response)
            report.case(f'history-budget.move-{i}-keeps-two-states',response.get('history_states')==2 and response.get('history_bytes',0)>0)
        undo=require_ok(server.command('undo',output=folder/'undo'))
        report.case('history-budget-evicted-clean-state-remains-dirty',undo['modified'] is True and undo['can_undo'] is False and undo['can_redo'] is True)
        report.compare('history-budget-undo-restores-last-retained-scene',Path(undo['scene']),Path(revisions[-2]['scene']))
        before=dict(server.state);rejected=server.command('undo',output=folder/'too-far')
        report.case('history-budget-rejects-evicted-undo',rejected['ok'] is False and server.state==before)
        redo=require_ok(server.command('redo',output=folder/'redo'))
        report.compare('history-budget-redo-restores-current-scene',Path(redo['scene']),Path(revisions[-1]['scene']))
        require_ok(server.command('save',output=folder/'save'));require_ok(server.command('mark_saved'))
        report.case('history-budget-can-establish-new-clean-state',server.state['modified'] is False)
    report.case('history-budget-private-input-unchanged',common.digest_file(private_input)==common.digest_file(model))


def exercise_model(args, out, runtime, fixture_exe, source_model, index, report):
    prefix=f'model-{index}';folder=out/prefix;folder.mkdir()
    model=folder/'input.fbm';shutil.copy2(source_model,model)
    original_hash=common.digest_file(source_model)
    entry={'model':str(source_model),'sha256':original_hash};report.data['models'].append(entry)
    fixture=folder/'fixture';fixture.mkdir()
    result=subprocess.run([str(fixture_exe),str(model),str(fixture)],cwd=fixture,
        env=private_environment(fixture),capture_output=True,timeout=120,creationflags=common.FLAGS)
    (fixture/'fixture.log').write_bytes(result.stdout+result.stderr)
    if result.returncode: raise RuntimeError('Independent fixture/oracle failed: '+str(fixture))
    oracle=json.loads((fixture/'reference-counts.json').read_text('utf8'))
    entry['independent_core_counts']=oracle
    executable=runtime/'bridge/FreeShipGeometryBridge.exe'
    snapshots={}
    def take(server,name,method='snapshot',**parameters):
        target=folder/name
        response=server.command(method,output=target,**parameters)
        captured=validate_snapshot(report,prefix+'.'+name,response,target,oracle if name=='original' else None)
        snapshots[name]=captured
        return captured
    with Service(executable,folder/'service') as server:
        original=take(server,'original','load',path=str(model),document_id=str(uuid.uuid4()),escaped_unicode=True)
        report.case(prefix+'.load-clean',server.state['modified'] is False and server.state['can_undo'] is False and server.state['can_redo'] is False)
        first=next(p for p in original['model']['control_points'] if not p['locked'])
        initial_state=dict(server.state)
        noop=take(server,'no-op','move_point',point_id=first['id'],position=first['p'])
        report.case(prefix+'.no-op-no-history-or-revision',server.state==initial_state and geometry(noop['model'])==geometry(original['model']))
        report.compare(prefix+'.no-op-preserves-scene',noop['paths']['scene'],original['paths']['scene'])
        moved_position=list(first['p'])
        moved_position[2]=struct.unpack('<f',struct.pack('<f',first['p'][2]+0.01))[0]
        moved=take(server,'moved','move_point',point_id=first['id'],position=moved_position)
        report.case(prefix+'.move-one-revision',server.state['revision']>initial_state['revision'])
        report.case(prefix+'.move-dirty-undoable',server.state['modified'] is True and server.state['can_undo'] is True and server.state['can_redo'] is False)
        points={p['id']:p for p in moved['model']['control_points']}
        report.case(prefix+'.move-only-requested-control-coordinate',all(points[p['id']]['p']==(moved_position if p['id']==first['id'] else p['p']) for p in original['model']['control_points']))
        report.case(prefix+'.move-preserves-topology-ids',all(moved['model'][key]==original['model'][key] for key in ('control_edges','control_faces','layers')))
        report.case(prefix+'.move-changes-linesplan',common.digest_file(moved['paths']['scene'])!=common.digest_file(original['paths']['scene']))
        dirty_state=dict(server.state)
        bad_output=folder/'not-a-directory';bad_output.write_text('Private failed-save fixture.',encoding='utf8')
        bad_model=folder/'corrupt.fbm';bad_model.write_bytes(b'Not a FreeShip model\x00\xff')
        publish_failure=folder/'blocked-model-publication';(publish_failure/'model.json').mkdir(parents=True)
        failed_position=list(moved_position);failed_position[2]+=0.03
        invalid_requests=[
            ('stale-revision','move_point',{'point_id':first['id'],'position':moved_position,'expected_revision':initial_state['revision']}),
            ('stale-document','move_point',{'point_id':first['id'],'position':moved_position,'expected_document_id':'another-document'}),
            ('unknown-point','move_point',{'point_id':max(points)+1000,'position':moved_position}),
            ('unbounded-coordinate','move_point',{'point_id':first['id'],'position':[1e100,0,0]}),
            ('null-coordinate','move_point',{'point_id':first['id'],'position':[None,0,0]}),
            ('nonfinite-coordinate','move_point',{'point_id':first['id'],'position':['EXPONENT_OVERFLOW',0,0],'raw_nonfinite':True}),
            ('failed-load','load',{'path':str(bad_model),'document_id':str(uuid.uuid4())}),
            ('negative-dimension-load','load',{'path':str(fixture/'negative-length.fbm'),'document_id':str(uuid.uuid4())}),
            ('unbounded-metadata-load','load',{'path':str(fixture/'unbounded-draft.fbm'),'document_id':str(uuid.uuid4())}),
            ('nul-name-load','load',{'path':str(fixture/'nul-name.fbm'),'document_id':str(uuid.uuid4())}),
            ('nul-path-load','load',{'path':str(model)+'\x00suffix','document_id':str(uuid.uuid4())}),
            ('failed-model-publication','move_point',{'point_id':first['id'],'position':failed_position,'output':publish_failure}),
            ('failed-save','save',{'output':bad_output}),
            ('stale-mark-saved','mark_saved',{'expected_revision':initial_state['revision']}),
        ]
        for name,method,parameters in invalid_requests:
            parameters=dict(parameters);target=parameters.pop('output',folder/('reject-'+name))
            response=server.command(method,output=target,**parameters)
            report.case(prefix+'.reject-'+name,response['ok'] is False and bool(response.get('error')))
            after=take(server,'after-'+name)
            report.case(prefix+'.'+name+'-preserves-document-and-history',server.state==dirty_state and geometry(after['model'])==geometry(moved['model']))
            report.compare(prefix+'.'+name+'-preserves-scene',after['paths']['scene'],moved['paths']['scene'])
            report.compare(prefix+'.'+name+'-preserves-dxf',after['paths']['dxf'],moved['paths']['dxf'])
        undo=take(server,'undo','undo')
        report.case(prefix+'.undo-restores-original-state',geometry(undo['model'])==geometry(original['model']) and server.state['modified'] is False and server.state['can_redo'] is True)
        report.compare(prefix+'.undo-restores-original-scene',undo['paths']['scene'],original['paths']['scene'])
        report.compare(prefix+'.undo-restores-original-dxf',undo['paths']['dxf'],original['paths']['dxf'])
        redo=take(server,'redo','redo')
        report.case(prefix+'.redo-restores-moved-state',geometry(redo['model'])==geometry(moved['model']) and server.state['modified'] is True)
        report.compare(prefix+'.redo-restores-moved-scene',redo['paths']['scene'],moved['paths']['scene'])
        report.compare(prefix+'.redo-restores-moved-dxf',redo['paths']['dxf'],moved['paths']['dxf'])
        saved=server.command('save',output=folder/'save');require_ok(saved)
        saved_file=artifact(saved,'saved_file',folder/'save')
        report.case(prefix+'.staged-save-does-not-clear-dirty',server.state['modified'] is True and saved_file.stat().st_size>0)
        committed=folder/'用户另存的模型.fbm';shutil.copy2(saved_file,committed)
        reread=folder/'saved-core-reread';reread.mkdir()
        reread_result=subprocess.run([str(fixture_exe),str(committed),str(reread),'inspect'],cwd=reread,
            env=private_environment(reread),capture_output=True,timeout=120,creationflags=common.FLAGS)
        (reread/'fixture.log').write_bytes(reread_result.stdout+reread_result.stderr)
        report.case(prefix+'.saved-file-readable-by-core-loader',reread_result.returncode==0)
        if reread_result.returncode:
            raise RuntimeError('Saved model failed independent core reread')
        reread_counts=json.loads((reread/'reference-counts.json').read_text('utf8'))
        report.case(prefix+'.save-preserves-preview-setting',reread_counts['save_preview']==oracle['save_preview'])
        if oracle['save_preview']:
            report.compare(prefix+'.save-preserves-embedded-jpeg',reread/'embedded-preview.jpg',fixture/'embedded-preview.jpg')
        else:
            report.case(prefix+'.embedded-jpeg-preservation',status='not_run',reason='This input has SavePreview disabled; preview-enabled samples are checked separately.')
        require_ok(server.command('mark_saved'))
        report.case(prefix+'.mark-saved-clears-dirty',server.state['modified'] is False and server.state['can_undo'] is True)
        take(server,'undo-saved','undo');report.case(prefix+'.undo-from-clean-is-dirty',server.state['modified'] is True)
        take(server,'redo-saved','redo');report.case(prefix+'.redo-to-clean-is-clean',server.state['modified'] is False)
        take(server,'branch-base','undo')
        alternate=list(first['p']);alternate[2]=struct.unpack('<f',struct.pack('<f',first['p'][2]+0.02))[0]
        take(server,'branch','move_point',point_id=first['id'],position=alternate)
        report.case(prefix+'.new-edit-discards-redo',server.state['can_redo'] is False and server.state['modified'] is True)
    with Service(executable,folder/'restart-service') as server:
        reopened=take(server,'reopened','load',path=str(committed),document_id=str(uuid.uuid4()))
        report.case(prefix+'.reopen-clean',server.state['modified'] is False and server.state['can_undo'] is False)
        report.compare(prefix+'.save-reopen-scene-exact',reopened['paths']['scene'],moved['paths']['scene'])
        report.compare(prefix+'.save-reopen-dxf-entities-exact',reopened['paths']['dxf'],moved['paths']['dxf'])
        locked=take(server,'locked','load',path=str(fixture/'locked.fbm'),document_id=str(uuid.uuid4()))
        locked_point=next(p for p in locked['model']['control_points'] if p['locked'])
        previous=dict(server.state);position=list(locked_point['p']);position[2]+=0.1
        response=server.command('move_point',output=folder/'locked-rejection',point_id=locked_point['id'],position=position)
        report.case(prefix+'.backend-rejects-locked-point',response['ok'] is False and bool(response.get('error')))
        after=take(server,'after-locked-rejection')
        report.case(prefix+'.locked-rejection-preserves-document',server.state==previous and geometry(after['model'])==geometry(locked['model']))
    if args.baseline and args.baseline.exists():
        frozen=frozen_folder(args.baseline,original_hash)
        accepted_snapshots=snapshots
        if (runtime/'FreeShipNativeLinesplan.exe').exists():
            # Preserve the frozen legacy comparison unchanged. Compare native
            # geometry against this independent old generation separately,
            # retaining the specifically documented display corrections.
            from native_linesplan_app import compare_scene
            accepted_snapshots={}
            with Service(executable,folder/'legacy-reference-service',native_linesplan=False) as legacy:
                for name,method,params in (
                    ('original','load',{'path':str(model),'document_id':str(uuid.uuid4())}),
                    ('moved','move_point',{'point_id':first['id'],'position':moved_position})):
                    target=folder/('legacy-reference-'+name)
                    accepted_snapshots[name]=validate_snapshot(report,prefix+'.legacy-reference-'+name,
                        legacy.command(method,output=target,**params),target)
                    compare_scene(report,prefix+'.native-vs-legacy-'+name,accepted_snapshots[name]['paths']['scene'],snapshots[name]['paths']['scene'])
                    report.compare(prefix+'.native-vs-legacy-'+name+'-dxf',snapshots[name]['paths']['dxf'],accepted_snapshots[name]['paths']['dxf'])
        for name,variant in (('original','original'),('moved','move-point')):
            for key,filename in (('scene','scene.fslp'),('dxf','reference.dxf')):
                comparator=report.compare_frozen_dxf if key=='dxf' else report.compare
                comparator(prefix+'.legacy-reference-'+name+'.'+key+'-equals-accepted-pilot',accepted_snapshots[name]['paths'][key],frozen/variant/filename)
    else:
        report.case(prefix+'.accepted-pilot-comparison',status='not_run',reason='Frozen baseline directory unavailable')
    entry['snapshots']={name:{key:str(path) for key,path in value['paths'].items()} for name,value in snapshots.items()}
    entry['four_views']=run_child(report,prefix+'.four-views',runtime/'qt_migration_tests.exe',[original['paths']['model']],folder/'four-views')
    entry['linesplan']=run_child(report,prefix+'.linesplan',runtime/'qt_lines_tests.exe',[original['paths']['scene']],folder/'linesplan')
    session=run_child(report,prefix+'.session',runtime/'qt_session_tests.exe',[model],folder/'session',timeout=300)
    entry['session']=session
    for name in ('original','moved','reopened'):
        for key in ('scene','dxf'):
            value=session.get('artifacts',{}).get(name+'_'+key)
            if value:
                path=Path(value).resolve(strict=True)
                if (folder/'session').resolve() not in path.parents:raise ValueError('Session artifact escaped private directory')
                report.compare(prefix+'.session.'+name+'.'+key+'-equals-service',path,snapshots[name]['paths'][key])
            else:
                report.case(prefix+'.session.'+name+'.'+key,status='not_run',reason='Session test did not expose artifact')
    production=run_child(report,prefix+'.production',runtime/'FreeShipQt.exe',['--verify-workflow',model],folder/'production',timeout=300)
    entry['production']=production
    artifacts=production.get('artifacts',{})
    for name in ('original','moved','undo','redo','reopened'):
        for key in ('scene','dxf'):
            value=artifacts.get(name+'_'+key)
            if value:
                path=Path(value).resolve(strict=True)
                if (folder/'production').resolve() not in path.parents:raise ValueError('Production test artifact escaped private directory')
                report.compare(prefix+'.production.'+name+'.'+key+'-equals-service',path,snapshots[name]['paths'][key])
            else:
                report.case(prefix+'.production.'+name+'.'+key,status='not_run',reason='Production workflow did not expose artifact')
    report.case(prefix+'.private-input-unchanged',common.digest_file(model)==original_hash)
    report.case(prefix+'.user-model-unchanged',common.digest_file(source_model)==original_hash)


def verify_deployment(args,models,out,workflow_argument='--verify-workflow',
                      workflow_phases=('original','moved','undo','redo','reopened'),
                      artifact_kinds=('scene','dxf'),workflow_prepare=None):
    """Run the installed path itself; never copy/patch a deployment for testing."""
    deployment=args.verify_deployment.resolve(strict=True)
    if out==deployment or deployment in out.parents:
        raise ValueError('Deployment verification output must be outside the installed directory')
    out.mkdir(parents=True,exist_ok=False)
    report=Report();report.data['mode']='actual-deployment-workflows'
    report.data['deployment']=str(deployment)
    protected={};legacy_before=None;pilot_before={};installed_before={}
    try:
        legacy_before=common.fingerprint(args.source)[0]
        pilot_before=current_sources(PILOT)
        for path in [*models,args.source/'FreeShip.exe',PILOT/'dist/FreeShipQtPilot.exe',
                     PILOT/'dist/bridge/FreeShipGeometryBridge.exe']:
            if path.exists():protected[str(path)]=common.digest_file(path)
        installed_before={str(path.relative_to(deployment)).replace('\\','/'):common.digest_file(path)
                          for path in deployment.rglob('*') if path.is_file()}
        manifest=json.loads((deployment/'build-manifest.json').read_text('utf8'))
        report.data['package_manifest']=manifest
        expected_sources=current_sources(args.qt_source)
        recorded={name.replace('\\','/'):digest for name,digest in manifest['source_sha256'].items()}
        if recorded!=expected_sources:raise RuntimeError('Deployment sources do not match current Qt source')
        validate_test_manifest(manifest,editing=workflow_argument=='--verify-editing')
        expected_files={name.replace('\\','/'):digest for name,digest in manifest['binaries_sha256'].items()}
        actual_files={name:digest for name,digest in installed_before.items() if name!='build-manifest.json'}
        if expected_files!=actual_files:raise RuntimeError('Deployment file inventory or binary hash mismatch')
        for name in expected_files:
            path=(deployment/name).resolve(strict=True)
            if deployment not in path.parents:raise ValueError('Deployment contains an external artifact link')
        if manifest.get('legacy_source_sha256')!=legacy_before:raise RuntimeError('Packaged Pascal source fingerprint is stale')
        if manifest.get('package_script_sha256')!=common.digest_file(args.qt_source/'package.ps1'):
            raise RuntimeError('Package recipe changed after deployment')
        bridge_report=json.loads((deployment/'bridge/report.json').read_text('utf8'))
        if bridge_report.get('status')!='pass' or bridge_report.get('reference') is not False:
            raise RuntimeError('Packaged bridge is not a successful persistent-service build')
        if bridge_report['adapter_sha256']!=bridge_builder(args.qt_source).adapter_hashes():
            raise RuntimeError('Packaged bridge adapters are stale')
        if bridge_report['source_sha256']!=legacy_before or (deployment/'bridge/source-id.txt').read_text('ascii').strip()!=legacy_before:
            raise RuntimeError('Packaged bridge source identity mismatch')
        if common.digest_file(deployment/'bridge/FreeShipGeometryBridge.exe')!=bridge_report['build']['executable_sha256']:
            raise RuntimeError('Packaged bridge binary differs from its actual build')
        for name in ('FreeShipQt.exe','bridge/FreeShipGeometryBridge.exe'):
            if name not in expected_files:raise RuntimeError('Package lacks required executable provenance')
        report.case('package-current-source-test-and-binary-manifest',True)
        reference=args.workflow_reference.resolve(strict=True)
        reference_report=json.loads((reference/'report.json').read_text('utf8'))
        if reference_report.get('status') not in ('pass','partial') or any(c['status'] in ('fail','error') for c in reference_report['cases']):
            raise RuntimeError('Workflow reference has failures or errors')
        report.data['workflow_reference']=str(reference)
        report.data['workflow_reference_sha256']=common.digest_file(reference/'report.json')
        for index,model in enumerate(models,1):
            prefix=f'model-{index}';folder=out/prefix;folder.mkdir()
            try:
                digest=common.digest_file(model)
                matching=[entry for entry in reference_report['models'] if entry.get('sha256',entry.get('model_sha256'))==digest]
                if len(matching)!=1:raise RuntimeError('Reference model is missing or ambiguous')
                private_model=folder/'input.fbm';shutil.copy2(model,private_model)
                workflow_input=workflow_prepare(private_model,folder) if workflow_prepare else private_model
                child=run_child(report,prefix+'.deployment',deployment/'FreeShipQt.exe',
                    [workflow_argument,workflow_input],folder/'production',timeout=300,
                    platform='offscreen' if workflow_argument in ('--verify-backgrounds','--verify-display','--verify-auxiliary','--verify-advanced','--verify-modeling') else None)
                entry={'model':str(model),'sha256':digest,'production':child};report.data['models'].append(entry)
                actual=child.get('artifacts',{});expected=matching[0]['production']['artifacts']
                if workflow_argument=='--verify-backgrounds' and any(c.get('name')=='background_workflow_unavailable' for c in child.get('cases',[])):
                    report.case(prefix+'.dependent-artifacts',status='not_run',reason='Production action discovery did not permit the workflow.');continue
                if workflow_argument=='--verify-modeling' and any(c.get('status')=='not_run' and c.get('name','').endswith('.entry') for c in child.get('cases',[])):
                    report.case(prefix+'.dependent-artifacts',status='not_run',reason='Production action discovery did not permit the workflow.');continue
                if workflow_argument in ('--verify-hydro','--verify-performance') and any(c.get('status')=='not_run' and c.get('name') in ('hydro_entry','performance_entry') for c in child.get('cases',[])):
                    report.case(prefix+'.dependent-artifacts',status='not_run',reason='Production action discovery did not permit the workflow.');continue
                for state in workflow_phases:
                    for kind in artifact_kinds:
                        name=state+'_'+kind
                        if name not in actual:raise RuntimeError('Production workflow did not produce '+name)
                        actual_path=Path(actual[name]).resolve(strict=True)
                        expected_path=Path(expected[name]).resolve(strict=True)
                        if (folder/'production').resolve() not in actual_path.parents:
                            raise ValueError('Deployment workflow artifact escaped the private report')
                        if reference not in expected_path.parents:
                            raise ValueError('Workflow reference artifact escaped its report')
                        report.compare(prefix+'.deployment.'+name+'-equals-tested-runtime',actual_path,expected_path)
                if workflow_argument=='--verify-hydro':
                    actual_hydro=json.loads(Path(actual['report_json']).read_text('utf8'))
                    expected_hydro=json.loads(Path(expected['report_json']).read_text('utf8'))
                    # Document UUID, filename and elapsed time belong to this
                    # invocation; every numerical row must remain identical.
                    quantities=('mode','units','water_density','appendage_coefficient','keel_z','rows','cancelled')
                    report.case(prefix+'.deployment.hydro-results-equal-tested-runtime',
                        {k:actual_hydro[k] for k in quantities}=={k:expected_hydro[k] for k in quantities})
                    report.compare(prefix+'.deployment.hydro-csv-equals-tested-runtime',actual['report_csv'],expected['report_csv'])
                if workflow_argument=='--verify-performance':
                    actual_result=json.loads(Path(actual['report_json']).read_text('utf8'))
                    expected_result=json.loads(Path(expected['report_json']).read_text('utf8'))
                    quantities=('calculator','units','series','parameters','warnings')
                    report.case(prefix+'.deployment.performance-equals-tested-runtime',{k:actual_result[k] for k in quantities}=={k:expected_result[k] for k in quantities})
                    report.compare(prefix+'.deployment.performance-csv-equals-tested-runtime',actual['report_csv'],expected['report_csv'])
                if workflow_argument=='--verify-plates':
                    quantities=('schema','units','waterline','underwater_color','shade_submerged','plates','layout')
                    report.case(prefix+'.deployment.plate-data-equals-tested-runtime',
                        {k:actual['plate_data'][k] for k in quantities}=={k:expected['plate_data'][k] for k in quantities})
                    expected_folder=Path(expected['original_scene']).parent.parent
                    for extension in ('dxf','txt'):
                        report.compare(prefix+'.deployment.plate-'+extension+'-equals-tested-runtime',folder/'production'/('plates.'+extension),expected_folder/('plates.'+extension))
                report.case(prefix+'.deployment-private-input-unchanged',common.digest_file(private_model)==digest)
            except Exception as exception:
                report.case(prefix+'.deployment.execution',status='error',reason=f'{type(exception).__name__}: {exception}')
        report.case('deployment-native-desktop-acceptance',status='not_run',reason='Actual installed executable ran hidden production workflows; no native desktop input or composition was automated.')
    except Exception as exception:
        report.case('deployment-manifest-or-setup',status='error',reason=f'{type(exception).__name__}: {exception}')
    finally:
        if legacy_before:report.case('legacy-source-unchanged',common.fingerprint(args.source)[0]==legacy_before)
        if pilot_before:report.case('accepted-pilot-source-unchanged',current_sources(PILOT)==pilot_before)
        for name,digest in protected.items():
            report.case('protected-file-unchanged:'+name,Path(name).exists() and common.digest_file(Path(name))==digest)
        if installed_before:
            installed_after={str(path.relative_to(deployment)).replace('\\','/'):common.digest_file(path)
                             for path in deployment.rglob('*') if path.is_file()}
            report.case('actual-deployment-files-unchanged',installed_after==installed_before)
        report.finish()
        (out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=QT)
    parser.add_argument('--model',type=Path,action='append')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--baseline',type=Path,default=DEFAULT_BASELINE)
    parser.add_argument('--reuse-build',type=Path,help='Reuse only an exactly matching persistent bridge build')
    parser.add_argument('--runtime',type=Path,help='Copy and verify an existing Qt runtime manifest into the private report')
    parser.add_argument('--verify-deployment',type=Path,help='Verify the package manifest, then run that actual installed path without copying it')
    parser.add_argument('--workflow-reference',type=Path,help='Successful full migration report used as the deployment workflow oracle')
    args=parser.parse_args()
    args.source=args.source.resolve();args.qt_source=args.qt_source.resolve();out=args.output.resolve()
    args.baseline=args.baseline.resolve() if args.baseline else None
    for protected in (args.source,args.qt_source,PILOT.resolve()):
        if out==protected or protected in out.parents:parser.error('Output must be outside product and Qt source trees')
    models=[p.resolve() for p in (args.model or DEFAULT_MODELS)]
    if args.verify_deployment:
        if not args.workflow_reference:parser.error('--verify-deployment requires --workflow-reference')
        if args.runtime or args.reuse_build:parser.error('Deployment verification does not build or copy a runtime')
        return verify_deployment(args,models,out)
    if args.workflow_reference:parser.error('--workflow-reference is only used with --verify-deployment')
    out.mkdir(parents=True,exist_ok=False)
    report=Report();protected_hashes={};source_before=None;pilot_before={}
    try:
        source_before=common.fingerprint(args.source)[0];report.data['legacy_source_sha256']=source_before
        for model in models:protected_hashes[str(model)]=common.digest_file(model)
        for file in (args.source/'FreeShip.exe',PILOT/'dist/FreeShipQtPilot.exe',PILOT/'dist/bridge/FreeShipGeometryBridge.exe'):
            if file.exists():protected_hashes[str(file)]=common.digest_file(file)
        pilot_before=current_sources(PILOT)
        report.data['protected_sha256']=protected_hashes;report.data['pilot_source_sha256']=pilot_before
        report.data['test_sha256']={str(p.relative_to(ROOT)):common.digest_file(p) for p in
            (Path(__file__),ROOT/'qt/migration_tests.cpp',ROOT/'qt/session_tests.cpp',ROOT/'QtMigrationFixture.pas',ROOT/'dxf_geometry.py')}
        exercise_legacy_dxf_oracle(out,report)
        builder=bridge_builder(args.qt_source)
        exercise_scene_recipe(args,out,builder,report)
        bridge=prepare_bridge(args,out,builder,report)
        fixture=build_fixture(args,out,builder,report)
        runtime=prepare_runtime(args,out,bridge,report)
        for index,model in enumerate(models,1):
            try:exercise_model(args,out,runtime,fixture,model,index,report)
            except Exception as exc:report.case(f'model-{index}.execution',status='error',reason=f'{type(exc).__name__}: {exc}')
        try:exercise_history_budget(out,runtime,models[0],report)
        except Exception as exc:report.case('history-budget.execution',status='error',reason=f'{type(exc).__name__}: {exc}')
        report.case('native-desktop-acceptance',status='not_run',reason='No native mouse, focus, display composition or user session was automated.')
    except Exception as exc:
        report.case('shared-build-or-instrumentation',status='error',reason=f'{type(exc).__name__}: {exc}')
        report.data['error']=str(exc)
    finally:
        if source_before:
            report.case('legacy-source-unchanged',common.fingerprint(args.source)[0]==source_before)
        if pilot_before:
            report.case('accepted-pilot-source-unchanged',current_sources(PILOT)==pilot_before)
        for file,digest in protected_hashes.items():
            report.case('protected-file-unchanged:'+file,Path(file).exists() and common.digest_file(Path(file))==digest)
        for file,digest in report.data.get('test_sha256',{}).items():
            report.case('test-source-unchanged:'+file,common.digest_file(ROOT/file)==digest)
        if 'qt_build_manifest' in report.data:
            recorded={key.replace('\\','/'):value for key,value in report.data['qt_build_manifest']['source_sha256'].items()}
            report.case('qt-source-unchanged-during-run',current_sources(args.qt_source)==recorded)
        if 'bridge_build' in report.data:
            report.case('bridge-adapter-unchanged-during-run',bridge_builder(args.qt_source).adapter_hashes()==report.data['bridge_build']['adapter_sha256'])
        report.finish()
        (out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]


if __name__=='__main__':
    raise SystemExit(main())
