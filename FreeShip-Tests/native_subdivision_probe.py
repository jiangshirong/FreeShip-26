"""Independent Pascal control-net/subdivision observer; all builds and models are private."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

import freeship_ui_test as common
import native_flicker_test as native
from native_geometry_probe import _adapter

ROOT=Path(__file__).resolve().parent
OBSERVER_PROPERTIES={
    '    property AlphaBlend: byte': '    property ProbeRawName:string read FDescription;\n',
    '    property Built: boolean\n      read FBuilt write SetBuilt;\n    property Color: TColor\n      read GetColor;': '    property ProbeRawVisible:Boolean read FVisible;\n',
    '    property Crease: boolean read FCrease write FSetCrease;': '    property ProbeControlDescendant:Boolean read FControlEdge;\n',
    '    constructor Create(Owner: TFreeSubdivisionSurface); override;\n    //property Surface: TFreeSubdivisionSurface read FSurface write FSurface;': '    property ProbeSubdivisionLevel:Integer read SubdivisionLevel;\n',
}


def geometry_hashes(source):
    paths=sorted((source/'Units').glob('FreeSubdivision*.inc'))
    paths += [source/'Units'/name for name in ('FreeSpline.inc','FreeGometry_Functions.inc','FreeTypes.pas')]
    return {str(path.relative_to(source)):common.digest_file(path) for path in paths}


def build(source:Path,qt_source:Path,out:Path)->Path:
    source,qt_source,out=source.resolve(),qt_source.resolve(),out.resolve()
    if any(out==root or root in out.parents for root in (source,qt_source)):raise ValueError('Use a private build directory')
    out.mkdir(parents=True,exist_ok=False);adapter=_adapter(qt_source)
    source_hash=common.fingerprint(source)[0];adapters=adapter.adapter_hashes();geometry=geometry_hashes(source)
    tests={name:common.digest_file(ROOT/name) for name in ('NativeSubdivisionProbe.pas','native_subdivision_probe.py','native_geometry_probe.py')}
    compiler=common.DEFAULT_LAZARUS/'fpc/3.2.2/bin/x86_64-win64/ppcx64.exe'
    report={'status':'error','source_sha256':source_hash,'adapter_sha256':adapters,'test_sha256':tests,'geometry_sha256':geometry,
            'compiler_sha256':common.digest_file(compiler),'lazbuild_sha256':common.digest_file(common.DEFAULT_LAZARUS/'lazbuild.exe'),
            'scope':'Original geometry includes byte-for-byte unchanged. Headless adapter, four read-only properties, and a local integrity-message exception wrapper only.'}
    try:
        def prepare(work):
            adapter.adapt_scene(work)
            # The product bridge also fixes a layer-name setter and substitutes
            # integrity dialogs. Neither belongs in an independent old-core oracle.
            # Restore every geometry include; shadow only the message UI below.
            for name in geometry:shutil.copy2(source/name,work/name)
            header=work/'Units/FreeGeometry.pas';text=header.read_text(encoding='utf-8-sig')
            for anchor,addition in OBSERVER_PROPERTIES.items():
                if text.count(anchor)!=1:raise RuntimeError('Observer declaration anchor changed: '+anchor)
                text=text.replace(anchor,addition+anchor,1)
            anchor='  FreeStringUtils;\n'
            if text.count(anchor)!=1:raise RuntimeError('Integrity wrapper anchor changed')
            text=text.replace(anchor,anchor+'\nprocedure ShowMessage(const MessageText:string);\nbegin raise EReadError.Create(MessageText) end;\n',1)
            header.write_text(text,encoding='utf8')
            if geometry_hashes(work)!=geometry:raise RuntimeError('Original subdivision algorithm was changed by adapter')
        with common.temporary_workspace('fs-native-subdivision-') as temporary:
            work=Path(temporary)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'NativeSubdivisionProbe.pas',prepare=prepare)
            exe=out/'NativeSubdivisionProbe.exe';shutil.copy2(work/'NativeLinesplanProbe.exe',exe)
            report['executable_sha256']=common.digest_file(exe)
            shutil.copy2(work/'native.lpi',out/'compiled-project.lpi');shutil.copy2(work/'Units/FreeGeometry.pas',out/'compiled-geometry-declarations.pas')
            objects=list(work.rglob('freegeometry.o'))
            if len(objects)==1:shutil.copy2(objects[0],out/'FreeGeometry.o')
            report['geometry_preserved']=geometry_hashes(work)==geometry
        for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=source_hash:raise RuntimeError('Original source changed')
        if adapter.adapter_hashes()!=adapters:raise RuntimeError('Adapter changed while building')
        if any(common.digest_file(ROOT/name)!=digest for name,digest in tests.items()):raise RuntimeError('Probe changed while building')
        report['status']='pass'
    except Exception as error:report['error']=f'{type(error).__name__}: {error}'
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if report['status']!='pass':raise RuntimeError(f'Oracle build failed: {out/"report.json"}: {report.get("error","")}')
    return exe


def _execute(exe:Path,input_path:Path,out:Path,mode:str)->dict:
    exe,input_path,out=exe.resolve(strict=True),input_path.resolve(strict=True),out.resolve()
    if out in (input_path.parent,exe.parent):raise ValueError('Use a private run directory')
    out.mkdir(parents=True,exist_ok=False);digest=common.digest_file(input_path)
    private=out/('model.fbm' if mode=='export-model' else 'input.json');shutil.copy2(input_path,private)
    env=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();env[key]=str(folder)
    result=subprocess.run([str(exe),str(private),str(out),mode],cwd=out,env=env,capture_output=True,timeout=300,creationflags=common.FLAGS)
    (out/'oracle.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(f'Oracle {mode} failed ({result.returncode}): {out/"oracle.log"}')
    if common.digest_file(input_path)!=digest or common.digest_file(private)!=digest:raise RuntimeError('Oracle changed its input')
    data=json.loads((out/'results.json').read_text('utf8'))
    if data.get('format')!='FreeShipNativeSubdivisionResults' or data.get('version')!=1:raise RuntimeError('Malformed oracle result')
    return data


def run(exe:Path,input_path:Path,out:Path)->dict:
    return _execute(exe,input_path,out,'cases')


def export_models(exe:Path,models:list[Path],out:Path)->dict:
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    combined={'format':'FreeShipNativeSubdivisionCases','version':1,'cases':[]}
    expected={'format':'FreeShipNativeSubdivisionResults','version':1,'cases':[]};coverage=[]
    for index,path in enumerate(models,1):
        model=Path(path).resolve(strict=True);folder=out/f'model-{index}';result=_execute(exe,model,folder,'export-model')
        source=json.loads((folder/'input.json').read_text('utf8'))
        for item in source['cases']:item['id']=f'model-{index}.'+item['id']
        for item in result['cases']:item['id']=f'model-{index}.'+item['id']
        if [c['id'] for c in source['cases']]!=[c['id'] for c in result['cases']]:raise RuntimeError('Source/result IDs differ')
        combined['cases'].extend(source['cases']);expected['cases'].extend(result['cases']);expected.setdefault('metadata',result.get('metadata',{}))
        entry=json.loads((folder/'coverage.json').read_text('utf8'))
        entry.update(model=str(model),model_sha256=common.digest_file(model),input_sha256=common.digest_file(folder/'input.json'),prefix=f'model-{index}')
        coverage.append(entry)
        if not entry.get('control_net_roundtrip_exact') or not entry.get('direct_vs_reconstructed_geometry_exact'):
            raise RuntimeError(f'Exported control-net replay differs: {folder/"coverage.json"}')
    input_path=out/'input.json';results_path=out/'results.json'
    input_path.write_text(json.dumps(combined,ensure_ascii=False,indent=2),encoding='utf8')
    results_path.write_text(json.dumps(expected,ensure_ascii=False,indent=2),encoding='utf8')
    (out/'models.json').write_text(json.dumps(coverage,ensure_ascii=False,indent=2),encoding='utf8')
    return {'input':combined,'results':expected,'models':coverage,'input_path':str(input_path),'results_path':str(results_path)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);parser.add_argument('--qt-source',type=Path,default=ROOT.parent/'FreeShip-Qt')
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--input',type=Path);parser.add_argument('--model',type=Path,action='append')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);report={'status':'error'}
    try:
        exe=build(args.source,args.qt_source,out/'build')
        if args.input:report['cases']=run(exe,args.input,out/'cases')
        if args.model:report['models']=export_models(exe,args.model,out/'models')
        report['status']='pass'
    except Exception as error:report['error']=str(error)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'report':str(out/'report.json'),'error':report.get('error')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 3


if __name__=='__main__':raise SystemExit(main())
