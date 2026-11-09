"""Build/run the unmodified Pascal geometry routines in a private source copy."""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

import freeship_ui_test as common
import native_flicker_test as native

ROOT=Path(__file__).resolve().parent
GEOMETRY_FILES=('Units/FreeSpline.inc','Units/FreeGometry_Functions.inc','Units/FreeTypes.pas')


def _adapter(qt_source):
    spec=importlib.util.spec_from_file_location('native_geometry_headless_adapter',qt_source/'bridge/build_bridge.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def build(source:Path,qt_source:Path,out:Path)->Path:
    source,qt_source,out=source.resolve(),qt_source.resolve(),out.resolve()
    if out==source or source in out.parents or out==qt_source or qt_source in out.parents:
        raise ValueError('Probe output must be outside product sources')
    out.mkdir(parents=True,exist_ok=False)
    adapter=_adapter(qt_source);source_hash=common.fingerprint(source)[0]
    adapters=adapter.adapter_hashes()
    tests={name:common.digest_file(ROOT/name) for name in ('NativeGeometryProbe.pas','native_geometry_probe.py')}
    geometry={name:common.digest_file(source/name) for name in GEOMETRY_FILES}
    compiler=common.DEFAULT_LAZARUS/'fpc/3.2.2/bin/x86_64-win64/ppcx64.exe'
    report={'status':'error','source_sha256':source_hash,'adapter_sha256':adapters,
            'test_sha256':tests,'geometry_sha256':geometry,
            'compiler_sha256':common.digest_file(compiler),'lazbuild_sha256':common.digest_file(common.DEFAULT_LAZARUS/'lazbuild.exe'),
            'scope':'Original Pascal leaf geometry; only headless observer adaptations.'}
    try:
        def prepare(work):
            adapter.adapt_scene(work)
            for name,digest in geometry.items():
                if common.digest_file(work/name)!=digest:
                    raise RuntimeError('Geometry oracle was changed by adaptation: '+name)
        with common.temporary_workspace('fs-native-geometry-') as temporary:
            work=Path(temporary)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',
                probe=ROOT/'NativeGeometryProbe.pas',prepare=prepare)
            executable=out/'NativeGeometryProbe.exe';shutil.copy2(work/'NativeLinesplanProbe.exe',executable)
            report['executable_sha256']=common.digest_file(executable)
            shutil.copy2(work/'native.lpi',out/'compiled-project.lpi')
            objects=list(work.rglob('freegeometry.o'))
            if len(objects)==1:
                shutil.copy2(objects[0],out/'FreeGeometry.o')
                report['geometry_object_sha256']=common.digest_file(out/'FreeGeometry.o')
            report['geometry_preserved']=all(common.digest_file(work/name)==digest for name,digest in geometry.items())
        for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=source_hash:raise RuntimeError('Original source changed during compilation')
        if adapter.adapter_hashes()!=adapters:raise RuntimeError('Headless adapter changed during compilation')
        if any(common.digest_file(ROOT/name)!=digest for name,digest in tests.items()):raise RuntimeError('Probe changed during compilation')
        report['status']='pass'
    except Exception as error:report['error']=f'{type(error).__name__}: {error}'
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if report['status']!='pass':raise RuntimeError(f'Oracle build failed: {out/"report.json"}; {report.get("error","")}')
    return executable


def _execute(exe:Path,input_path:Path,out:Path,mode:str)->dict:
    exe,input_path,out=exe.resolve(strict=True),input_path.resolve(strict=True),out.resolve()
    if out==input_path.parent or out==exe.parent:raise ValueError('Use a private output directory')
    out.mkdir(parents=True,exist_ok=False);digest=common.digest_file(input_path)
    private=out/('model.fbm' if mode=='export-model' else 'cases.json');shutil.copy2(input_path,private)
    env=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        folder=out/key;folder.mkdir();env[key]=str(folder)
    process=subprocess.run([str(exe),str(private),str(out),mode],cwd=out,env=env,
        capture_output=True,timeout=180,creationflags=common.FLAGS)
    (out/'oracle.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError(f'Oracle {mode} failed ({process.returncode}): {out/"oracle.log"}')
    if common.digest_file(input_path)!=digest or common.digest_file(private)!=digest:raise RuntimeError('Oracle changed its input')
    result=json.loads((out/'results.json').read_text('utf8'))
    if result.get('format')!='FreeShipNativeGeometryResults' or result.get('version')!=1 or not isinstance(result.get('cases'),list):
        raise RuntimeError('Malformed oracle result')
    return result


def run(exe:Path,input_path:Path,out:Path)->dict:
    return _execute(exe,input_path,out,'cases')


def export_models(exe:Path,models:list[Path],out:Path)->dict:
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    combined={'format':'FreeShipNativeGeometryCases','version':1,'cases':[]}
    expected={'format':'FreeShipNativeGeometryResults','version':1,'cases':[]}
    coverage=[]
    for index,path in enumerate(models,1):
        model=Path(path).resolve(strict=True);folder=out/f'model-{index}'
        result=_execute(exe,model,folder,'export-model')
        source=json.loads((folder/'input.json').read_text('utf8'))
        for item in source['cases']:item['id']=f'model-{index}.'+item['id']
        for item in result['cases']:item['id']=f'model-{index}.'+item['id']
        if [c['id'] for c in source['cases']]!=[c['id'] for c in result['cases']]:raise RuntimeError('Exported source/oracle case mismatch')
        combined['cases'].extend(source['cases']);expected['cases'].extend(result['cases'])
        expected.setdefault('metadata',result.get('metadata',{}))
        entry=json.loads((folder/'coverage.json').read_text('utf8'))
        entry.update(model=str(model),model_sha256=common.digest_file(model),prefix=f'model-{index}')
        entry['input_sha256']=common.digest_file(folder/'input.json')
        if not source['cases']:raise RuntimeError(f'No spline samples exported for model: {model}')
        coverage.append(entry)
    input_path=out/'input.json';results_path=out/'results.json'
    input_path.write_text(json.dumps(combined,ensure_ascii=False,indent=2),encoding='utf8')
    results_path.write_text(json.dumps(expected,ensure_ascii=False,indent=2),encoding='utf8')
    (out/'models.json').write_text(json.dumps(coverage,ensure_ascii=False,indent=2),encoding='utf8')
    return {'input':combined,'results':expected,'models':coverage,'input_path':str(input_path),'results_path':str(results_path)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=ROOT.parent/'FreeShip-Qt')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--input',type=Path)
    parser.add_argument('--model',type=Path,action='append')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    report={'status':'error'}
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
