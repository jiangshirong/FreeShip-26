"""Reproducible Qt pilot, legacy geometry bridge and offscreen behavioral checks."""
import argparse,json,os,shutil,subprocess,sys,time,statistics
from pathlib import Path
import freeship_ui_test as common
from PIL import Image,ImageChops

ROOT=Path(__file__).resolve().parent
PILOT=ROOT.parent/'FreeShip-QtPilot'
sys.path.insert(0,str(PILOT/'bridge'))
import build_bridge
from dxf_geometry import entities as dxf_entities

def run_bridge(exe,model,out,variant,reference=False):
    out.mkdir(parents=True,exist_ok=False)
    shutil.copy2(model,out/'input.fbm')
    with common.temporary_workspace('fs-qtpilot-') as td:
        work=Path(td);shutil.copy2(model,work/'input.fbm');env=os.environ.copy()
        for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
            (work/key).mkdir();env[key]=str(work/key)
        start=time.perf_counter()
        r=subprocess.run([str(exe),str(work/'input.fbm'),str(work),variant],cwd=work,env=env,
            capture_output=True,timeout=180,creationflags=common.FLAGS)
        duration=(time.perf_counter()-start)*1000
        (out/'bridge.log').write_bytes(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError('Geometry bridge failed: '+str(out/'bridge.log'))
        names=['scene.fslp','reference.dxf','bridge-timing.txt']
        if reference:names+=['legacy.bmp','captured.bmp']
        elif list(work.rglob('*.bmp')) or list(work.rglob('*.fsc')):
            raise RuntimeError('Scene-only bridge unexpectedly created a raster/cache file')
        for n in names:shutil.copy2(work/n,out/n)
    return duration

def timing(folder):
    return {k:float(v) for k,v in (line.split('=',1) for line in
        (folder/'bridge-timing.txt').read_text('utf8').splitlines())}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reuse-build',type=Path,help='Explicitly reuse a bridge built from the identical current source')
    p.add_argument('--reuse-reference',type=Path,help='Reuse the identical current legacy oracle build')
    p.add_argument('--baseline',type=Path,help='Earlier report directory containing immutable scenes and DXFs')
    p.add_argument('--startup-repeats',type=int,default=3,help='Paired fresh-process samples per model (at least 1)')
    a=p.parse_args();out=a.output.resolve();source=a.source.resolve()
    if out==source or source in out.parents:p.error('Output must be outside product')
    if a.startup_repeats<1:p.error('--startup-repeats must be positive')
    out.mkdir(parents=True,exist_ok=False);report={'status':'error','cases':[],'models':[],
        'limitations':['Synthetic Qt events and offscreen CPU paint timings are not native desktop acceptance.',
          'Geometry and DXF algorithms remain Pascal; scene generation no longer constructs legacy forms.',
          'Control-point edits trigger full bridge regeneration; partial Qt geometry updates are not implemented.',
          'Label anchors preserve the legacy fitted-view math; Qt uses its own translation-stable collision layout.',
          'Fresh-process timings use a warm operating-system file cache; not a cold-boot benchmark.']}
    cases=report['cases']
    def case(id,ok):cases.append({'id':id,'status':'pass' if ok else 'fail'})
    def compare_file(id,actual,expected):
        byte_equal=common.digest_file(actual)==common.digest_file(expected)
        equal=dxf_entities(actual)==dxf_entities(expected) if actual.suffix=='.dxf' else byte_equal
        cases.append({'id':id,'status':'pass' if equal else 'fail','byte_equal':byte_equal,
            'expected':'Exact complete DXF entities, including attributes and vertex connectivity; only entity order or closed-loop traversal may differ.' if actual.suffix=='.dxf' else 'Byte-identical complete scene.'})
        if equal and not byte_equal:cases.append({'id':id+'.bytes','status':'review',
            'reason':'Geometry and attributes match exactly, bytes differ. Legacy ExtractAllEdgeLoops sorts pointer addresses (FreeSubdivisionSurface.inc:3587, FasterList.pas:671).'} )
    try:
        before=common.fingerprint(source)[0];report['source_sha256']=before
        models=a.model or [ROOT.parent/'FreeShip-MCP/examples/box.fbm',ROOT.parent/'FreeShip-MCP/examples/synthetic-hull.fbm']
        originals={str(m.resolve()):common.digest_file(m) for m in models};report['model_sha256']=originals
        test_files=[Path(__file__),ROOT/'dxf_geometry.py',ROOT/'qt/linesplan_tests.cpp',PILOT/'build.ps1',
            *[ROOT.parent/n for n in build_bridge.adapter_hashes()],*sorted((PILOT/'src').glob('*'))]
        report['test_and_pilot_sha256']={str(f.relative_to(ROOT.parent)):common.digest_file(f) for f in test_files}
        def get_build(reuse,name,reference):
            folder=reuse.resolve() if reuse else out/name
            build=json.loads((folder/'report.json').read_text('utf8')) if reuse else build_bridge.build(source,folder,reference)
            if build['status']!='pass' or build['source_sha256']!=before:raise RuntimeError('Bridge build failed or stale: '+str(folder))
            if build.get('adapter_sha256')!=build_bridge.adapter_hashes() or build.get('reference')!=reference:raise RuntimeError('Reused bridge adapter is stale: '+str(folder))
            report[name]=build
            return folder
        bridge_dir=get_build(a.reuse_build,'bridge-build',False)
        reference_dir=get_build(a.reuse_reference,'reference-build',True)
        runtime=out/'runtime'
        with (out/'qt-build.log').open('wb') as log:
            r=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(PILOT/'build.ps1'),'-Deploy','-DeployDir',str(runtime)],stdout=log,stderr=subprocess.STDOUT,creationflags=common.FLAGS,timeout=180)
        if r.returncode:raise RuntimeError('Qt build failed; see qt-build.log')
        # Baseline installed binary is preserved for provenance, never executed here.
        if (source/'FreeShip.exe').exists():
            shutil.copy2(source/'FreeShip.exe',out/'legacy-installed.exe');report['legacy_installed_sha256']=common.digest_file(source/'FreeShip.exe')
        exe=bridge_dir/'FreeShipGeometryBridge.exe'
        reference_exe=reference_dir/'FreeShipGeometryBridge.exe'
        deployed=runtime/'bridge';deployed.mkdir(parents=True,exist_ok=True)
        shutil.copy2(exe,deployed/exe.name)
        for name in ('locale','Themes','Languages'):shutil.copytree(bridge_dir/name,deployed/name,dirs_exist_ok=True)
        shutil.copy2(bridge_dir/'source-id.txt',deployed/'source-id.txt')
        testexe=runtime/'qt_lines_tests.exe';report['qt_test_exe_sha256']=common.digest_file(testexe)
        baseline=json.loads((a.baseline/'report.json').read_text('utf8')) if a.baseline else None
        for i,model in enumerate(models):
            entry={'model':str(model.resolve()),'variants':[]};report['models'].append(entry)
            for variant in ('original','move-point'):
                folder=out/f'model-{i+1}'/variant;wall=run_bridge(exe,model,folder,variant)
                ref=out/f'model-{i+1}'/(variant+'-legacy');refwall=run_bridge(reference_exe,model,ref,variant,True)
                equal=ImageChops.difference(Image.open(ref/'legacy.bmp').convert('RGB'),Image.open(ref/'captured.bmp').convert('RGB')).getbbox() is None
                case(f'{i+1}.{variant}.capture-does-not-change-legacy-pixels',equal)
                case(f'{i+1}.{variant}.matched-viewport-size',Image.open(ref/'legacy.bmp').size==(1280,800))
                for name in ('scene.fslp','reference.dxf'):
                    compare_file(f'{i+1}.{variant}.{name}-equals-legacy',folder/name,ref/name)
                tm=timing(folder)
                case(f'{i+1}.{variant}.no-legacy-forms-or-viewports',tm.get('forms_created')==0 and tm.get('viewports_created')==0)
                if baseline:
                    indices=[j for j,m in enumerate(baseline['models']) if baseline['model_sha256'][m['model']]==originals[str(model.resolve())]]
                    if len(indices)!=1:raise RuntimeError('Baseline model is missing or ambiguous')
                    frozen=a.baseline/f'model-{indices[0]+1}'/variant
                    for name in ('scene.fslp','reference.dxf'):
                        compare_file(f'{i+1}.{variant}.{name}-equals-frozen',folder/name,frozen/name)
                env=os.environ.copy();env['QT_QPA_PLATFORM']='windows'
                qtout=folder/'qt';start=time.perf_counter()
                r=subprocess.run([str(testexe),str(folder/'scene.fslp'),str(qtout)],env=env,capture_output=True,timeout=180,creationflags=common.FLAGS)
                (folder/'qt.log').write_bytes(r.stdout+r.stderr)
                qr=json.loads((qtout/'report.json').read_text('utf8'))
                if r.returncode not in (0,2) and qr['status'] not in ('fail','error'):raise RuntimeError('Test exit/report mismatch')
                for c in qr['cases']:cases.append({**c,'id':f'{i+1}.{variant}.'+c['id']})
                if qr['status']=='error':raise RuntimeError(str(qr))
                entry['variants'].append({'variant':variant,'bridge_wall_ms':wall,'legacy_wall_ms':refwall,
                    'timings':tm,'legacy_timings':timing(ref),'qt':qr})
            original=out/f'model-{i+1}'/'original';moved=out/f'model-{i+1}'/'move-point'
            case(f'{i+1}.model-edit-changes-scene',common.digest_file(original/'scene.fslp')!=common.digest_file(moved/'scene.fslp'))
            # Separate process regeneration checks deterministic full-model input handling.
            restart=out/f'model-{i+1}'/'restart';run_bridge(exe,model,restart,'original')
            case(f'{i+1}.restart-scene-identical',common.digest_file(original/'scene.fslp')==common.digest_file(restart/'scene.fslp'))
            compare_file(f'{i+1}.restart-dxf-identical',restart/'reference.dxf',original/'reference.dxf')
            # Independent negative controls: a coordinate/color change must fail
            # even when all other output is identical.
            ref_dxf=original/'reference.dxf';expected=dxf_entities(ref_dxf)
            raw=ref_dxf.read_text('utf8').splitlines()
            for code,replacement in (('10','123.000000000001'),('62','217')):
                changed=raw.copy();idx=next(j for j in range(0,len(raw),2) if raw[j]==code)
                changed[idx+1]=replacement;bad=out/f'model-{i+1}'/f'mutated-code-{code}.dxf'
                bad.write_text('\n'.join(changed)+'\n',encoding='utf8')
                case(f'{i+1}.dxf-comparator-rejects-code-{code}',dxf_entities(bad)!=expected)
            samples=[]
            for n in range(a.startup_repeats):
                pair={}
                order=(False,True) if n%2==0 else (True,False)
                for legacy in order:
                    key='legacy' if legacy else 'scene';sample=out/f'model-{i+1}'/f'startup-{n+1}-{key}'
                    elapsed=run_bridge(reference_exe if legacy else exe,model,sample,'original',legacy)
                    pair[key]={'wall_ms':elapsed,'phases':timing(sample)}
                    case(f'{i+1}.startup-{n+1}-{key}.scene-exact',common.digest_file(sample/'scene.fslp')==common.digest_file(original/'scene.fslp'))
                    compare_file(f'{i+1}.startup-{n+1}-{key}.dxf-exact',sample/'reference.dxf',original/'reference.dxf')
                samples.append(pair)
            entry['startup']={'samples':samples,'median_ms':{key:statistics.median(p[key]['wall_ms'] for p in samples) for key in ('scene','legacy')}}
            cases.append({'id':f'{i+1}.startup-performance','status':'review','measurement':entry['startup']['median_ms'],
                'reason':'Machine-specific timing observation; fresh process, warm filesystem cache, no desktop presentation measurement.'})
            # Invoke the shipped executable's actual async loader/export code,
            # with the top-level window hidden. No OS input is synthesized.
            try:
                # Same Qt binary, runtime, model and first-frame endpoint. Swap
                # only the child executable in this private report's runtime.
                for legacy in (False,True):
                    key='production-legacy' if legacy else 'production'
                    shutil.copy2(reference_exe if legacy else exe,deployed/exe.name)
                    smoke=out/f'model-{i+1}'/key
                    env=os.environ.copy();env['QT_QPA_PLATFORM']='windows'
                    r=subprocess.run([str(runtime/'FreeShipQtPilot.exe'),'--verify-model',str(model.resolve()),str(smoke)],
                        env=env,capture_output=True,timeout=90,creationflags=common.FLAGS)
                    (smoke/'process.log').write_bytes(r.stdout+r.stderr)
                    sr=json.loads((smoke/'report.json').read_text('utf8'));entry[key]=sr
                    case(f'{i+1}.{key}-loader',r.returncode==0 and sr['status']=='pass')
                    if sr['status']=='pass':
                        compare_file(f'{i+1}.{key}-dxf-kept-exact',smoke/'production.dxf',original/'reference.dxf')
                        case(f'{i+1}.{key}-dxf-copy-byte-identical',common.digest_file(smoke/'production.dxf')==common.digest_file(smoke/'bridge-reference.dxf'))
                        case(f'{i+1}.{key}-scene-kept-exact',common.digest_file(smoke/'production.fslp')==common.digest_file(original/'scene.fslp'))
                modern=out/f'model-{i+1}'/'production/production.png'
                legacy_image=out/f'model-{i+1}'/'production-legacy/production.png'
                case(f'{i+1}.production-image-equals-legacy-bridge',ImageChops.difference(
                    Image.open(modern).convert('RGB'),Image.open(legacy_image).convert('RGB')).getbbox() is None)
            finally:shutil.copy2(exe,deployed/exe.name)
        case('product-source-unchanged',before==common.fingerprint(source)[0])
        case('original-models-unchanged',all(common.digest_file(Path(f))==h for f,h in originals.items()))
        case('test-and-pilot-source-unchanged',all(common.digest_file(ROOT.parent/f)==h for f,h in report['test_and_pilot_sha256'].items()))
        if 'legacy_installed_sha256' in report:case('installed-exe-unchanged',common.digest_file(source/'FreeShip.exe')==report['legacy_installed_sha256'])
        report['status']='fail' if any(c['status']=='fail' for c in cases) else 'partial'
    except Exception as e:report.update(status='error',error=str(e))
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'report':str(out/'report.json'),'error':report.get('error')},ensure_ascii=False))
    return 2 if report['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
