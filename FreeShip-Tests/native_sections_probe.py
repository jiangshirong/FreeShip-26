"""Private, instrumented observation of the real Plus section/segment routines."""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import freeship_ui_test as common
import native_flicker_test as native
import native_subdivision_probe as subdivision
import native_linesplan_observer
from native_geometry_probe import _adapter

ROOT=Path(__file__).resolve().parent
SURFACE=Path('Units/FreeSubdivisionSurface.inc')
OBSERVERS='''
type
 TNativeSectionsSegmentObserver=procedure(Face:TFreeSubdivisionFace;StartPoint,EndPoint:T3DCoordinate;StartKnuckle,EndKnuckle:Boolean;StartEdge,EndEdge:TFreeSubdivisionEdge);
 TNativeSectionsFacesObserver=procedure(Faces:TFasterListTFreeSubdivisionFace);
 TNativeSectionsStageObserver=procedure(Stage:Integer;List:TFasterListTFreeSpline);
var
 NativeSectionsSegmentObserver:TNativeSectionsSegmentObserver;
 NativeSectionsFacesObserver:TNativeSectionsFacesObserver;
 NativeSectionsStageObserver:TNativeSectionsStageObserver;
'''
SEGMENT_CALL='''
                  if Assigned(NativeSectionsSegmentObserver) then
                    NativeSectionsSegmentObserver(Face,Segments[NSegments-1].StartP.Point,Segments[NSegments-1].EndP.Point,
                      Segments[NSegments-1].StartP.Knuckle,Segments[NSegments-1].EndP.Knuckle,
                      Segments[NSegments-1].StartP.Edge,Segments[NSegments-1].EndP.Edge);
'''


def instrument_surface(original:bytes):
    newline=b'\r\n' if b'\r\n' in original else b'\n'
    def enc(value):return value.encode('utf8').replace(b'\n',newline)
    replacements=[
        ('  // first assemble all edges belonging to this set of faces',
         '  if Assigned(NativeSectionsFacesObserver) then NativeSectionsFacesObserver(Faces);\n'),
        ('                    Segments[NSegments - 1].EndP.Knuckle := P2.VertexType = svCorner;',SEGMENT_CALL),
        ('          Segments[NSegments - 1].EndP := IntArray[K - 1];',SEGMENT_CALL),
        ('  if Destination.Count > 1 then',
         '  if Assigned(NativeSectionsStageObserver) then NativeSectionsStageObserver(0,Destination);\n'),
        ('    JoinSplineSegments(0.01, False, Destination);',
         '\n    if Assigned(NativeSectionsStageObserver) then NativeSectionsStageObserver(1,Destination);'),
    ]
    adapted=original;insertions=[]
    for index,(anchor,addition) in enumerate(replacements):
        a,b=enc(anchor),enc(addition)
        if adapted.count(a)!=1:raise RuntimeError('Section observer anchor changed: '+anchor)
        if index in (0,3):adapted=adapted.replace(a,b+a,1)
        else:adapted=adapted.replace(a,a+b,1)
        insertions.append(b)
    restored=adapted
    for insertion in reversed(insertions):
        # The identical segment observer text is intentionally inserted twice.
        restored=restored.replace(insertion,b'',1)
    if restored!=original:raise RuntimeError('Observer removal did not restore original bytes')
    return adapted,insertions


def build(source:Path,qt_source:Path,out:Path)->Path:
    source,qt_source,out=source.resolve(),qt_source.resolve(),out.resolve()
    if any(out==root or root in out.parents for root in (source,qt_source)):raise ValueError('Use private build output')
    out.mkdir(parents=True,exist_ok=False);adapter=_adapter(qt_source)
    source_hash=common.fingerprint(source)[0];adapters=adapter.adapter_hashes();geometry=subdivision.geometry_hashes(source)
    names=('NativeSectionsProbe.pas','native_sections_probe.py','NativeSubdivisionProbe.pas','native_subdivision_probe.py','native_geometry_probe.py',
           'native_linesplan_observer.py','NativeLinesplanProbe.inc')
    tests={name:common.digest_file(ROOT/name) for name in names}
    compiler=common.DEFAULT_LAZARUS/'fpc/3.2.2/bin/x86_64-win64/ppcx64.exe'
    report={'status':'error','source_sha256':source_hash,'adapter_sha256':adapters,'test_sha256':tests,'geometry_sha256':geometry,
            'compiler_sha256':common.digest_file(compiler),'lazbuild_sha256':common.digest_file(common.DEFAULT_LAZARUS/'lazbuild.exe'),
            'scope':'Original algorithms retained including known legacy defects. Read-only segment/stage callbacks; strip restores original source bytes. Final spline sampling occurs after all callbacks.'}
    try:
        def prepare(work):
            adapter.adapt_scene(work)
            for name in geometry:shutil.copy2(source/name,work/name)
            header=work/'Units/FreeGeometry.pas';text=header.read_text(encoding='utf-8-sig')
            properties=dict(subdivision.OBSERVER_PROPERTIES)
            properties['    property Fragments: integer\n']='    property ProbeRawFragments:Integer read FFragments;\n'
            for anchor,addition in properties.items():
                if text.count(anchor)!=1:raise RuntimeError('Observer declaration anchor changed: '+anchor)
                text=text.replace(anchor,addition+anchor,1)
            text=text.replace('\nimplementation\n',OBSERVERS+'\nimplementation\n',1)
            anchor='  FreeStringUtils;\n'
            if text.count(anchor)!=1:raise RuntimeError('Integrity wrapper anchor changed')
            text=text.replace(anchor,anchor+'\nprocedure ShowMessage(const MessageText:string);\nbegin raise EReadError.Create(MessageText) end;\n',1)
            header.write_text(text,encoding='utf8')
            # Reuse the already-reviewed raw control-net loader, not another model reconstruction.
            support=(ROOT/'NativeSubdivisionProbe.pas').read_text('utf-8-sig')
            support=support[support.index('function ReadJSON('):support.index('function Snapshot(')]
            (work/'Tests/NativeSectionsSupport.inc').write_text(support,encoding='utf8')
            report['linesplan_observer']=native_linesplan_observer.prepare(source,work,out)
            report['loader_support_sha256']=hashlib.sha256(support.encode()).hexdigest()
            original=(source/SURFACE).read_bytes();adapted,insertions=instrument_surface(original);(work/SURFACE).write_bytes(adapted)
            (out/'original-section-source.inc').write_bytes(original);(out/'observed-section-source.inc').write_bytes(adapted)
            diff=''.join(difflib.unified_diff(original.decode('utf-8-sig').splitlines(True),adapted.decode('utf-8-sig').splitlines(True),fromfile='original',tofile='read-only-observers'))
            (out/'observer.diff').write_text(diff.replace('\r\n','\n'),encoding='utf8')
            report['observer']={'original_sha256':hashlib.sha256(original).hexdigest(),'instrumented_sha256':hashlib.sha256(adapted).hexdigest(),
                                'stripped_equals_original':True,'insertion_count':len(insertions),'algorithm_statement_changes':0}
            expected=dict(geometry);expected[str(SURFACE)]=hashlib.sha256(adapted).hexdigest()
            if subdivision.geometry_hashes(work)!=expected:raise RuntimeError('Unexpected geometry change in private adapter')
        with common.temporary_workspace('fs-native-sections-') as temporary:
            work=Path(temporary)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'NativeSectionsProbe.pas',prepare=prepare)
            exe=out/'NativeSectionsProbe.exe';shutil.copy2(work/'NativeLinesplanProbe.exe',exe);report['executable_sha256']=common.digest_file(exe)
            shutil.copy2(work/'native.lpi',out/'compiled-project.lpi');shutil.copy2(work/'Units/FreeGeometry.pas',out/'compiled-geometry-declarations.pas')
            shutil.copy2(work/'Tests/NativeSectionsSupport.inc',out/'compiled-loader-support.inc')
            objects=list(work.rglob('freegeometry.o'))
            if len(objects)==1:shutil.copy2(objects[0],out/'FreeGeometry.o')
        for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=source_hash:raise RuntimeError('Original source changed')
        if adapter.adapter_hashes()!=adapters:raise RuntimeError('Adapter changed while building')
        if any(common.digest_file(ROOT/name)!=digest for name,digest in tests.items()):raise RuntimeError('Probe/helper changed while building')
        report['status']='pass'
    except Exception as error:report['error']=f'{type(error).__name__}: {error}'
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if report['status']!='pass':raise RuntimeError(f'Oracle build failed: {out/"report.json"}: {report.get("error","")}')
    return exe


def _execute(exe:Path,input_path:Path,out:Path,mode:str)->dict:
    exe,input_path,out=exe.resolve(strict=True),input_path.resolve(strict=True),out.resolve()
    if out in (input_path.parent,exe.parent):raise ValueError('Use private run output')
    out.mkdir(parents=True,exist_ok=False);digest=common.digest_file(input_path)
    private=out/('model.fbm' if mode=='export-model' else 'input.json');shutil.copy2(input_path,private);env=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();env[key]=str(folder)
    process=subprocess.run([str(exe),str(private),str(out),mode],cwd=out,env=env,capture_output=True,timeout=300,creationflags=common.FLAGS)
    (out/'oracle.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError(f'Oracle {mode} failed ({process.returncode}): {out/"oracle.log"}')
    if common.digest_file(input_path)!=digest or common.digest_file(private)!=digest:raise RuntimeError('Oracle changed input')
    data=json.loads((out/'results.json').read_text('utf8'))
    if data.get('format')!='FreeShipNativeSectionsResults' or data.get('version')!=1:raise RuntimeError('Malformed oracle results')
    return data


def run(exe:Path,input_path:Path,out:Path)->dict:
    return _execute(exe,input_path,out,'cases')


def export_models(exe:Path,models:list[Path],out:Path)->dict:
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    combined={'format':'FreeShipNativeSectionsCases','version':1,'cases':[]};expected={'format':'FreeShipNativeSectionsResults','version':1,'cases':[]};coverage=[]
    for index,path in enumerate(models,1):
        model=Path(path).resolve(strict=True);folder=out/f'model-{index}';result=_execute(exe,model,folder,'export-model');source=json.loads((folder/'input.json').read_text('utf8'))
        for item in source['cases']:item['id']=f'model-{index}.'+item['id']
        for item in result['cases']:item['id']=f'model-{index}.'+item['id']
        combined['cases'].extend(source['cases']);expected['cases'].extend(result['cases']);expected.setdefault('metadata',result.get('metadata',{}))
        entry=json.loads((folder/'coverage.json').read_text('utf8'));entry.update(model=str(model),model_sha256=common.digest_file(model),input_sha256=common.digest_file(folder/'input.json'),prefix=f'model-{index}')
        coverage.append(entry)
        if not entry.get('direct_vs_reconstructed_exact'):raise RuntimeError(f'Original/reconstructed sections differ: {folder/"coverage.json"}')
    input_path=out/'input.json';results_path=out/'results.json'
    input_path.write_text(json.dumps(combined,ensure_ascii=False,indent=2),encoding='utf8');results_path.write_text(json.dumps(expected,ensure_ascii=False,indent=2),encoding='utf8')
    (out/'models.json').write_text(json.dumps(coverage,ensure_ascii=False,indent=2),encoding='utf8')
    return {'input':combined,'results':expected,'models':coverage,'input_path':str(input_path),'results_path':str(results_path)}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=ROOT.parent/'FreeShip-Qt');parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--input',type=Path);parser.add_argument('--model',type=Path,action='append');args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);report={'status':'error'}
    try:
        exe=build(args.source,args.qt_source,out/'build')
        if args.input:report['cases']=run(exe,args.input,out/'cases')
        if args.model:report['models']=export_models(exe,args.model,out/'models')
        report['status']='pass'
    except Exception as error:report['error']=str(error)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'report':str(out/'report.json'),'error':report.get('error')},ensure_ascii=False));return 0 if report['status']=='pass' else 3


if __name__=='__main__':raise SystemExit(main())
