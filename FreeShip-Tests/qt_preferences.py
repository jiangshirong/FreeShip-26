"""Native preference bootstrap, exact legacy parity and a Pascal-free runtime.

All configuration edits affect a private runtime, never the installed package
or the user's old profile. Native startup is tested with the legacy executable
absent; the packaged bridge is used only for independent context comparisons.
"""
import argparse
import json
import os
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing
import qt_publication as publication

WIRE_FIELDS = {'id', 'ok', 'document_engine', 'adapter_pid', 'adapter_requests',
               'preferences_engine', 'preferences_ms'}


def clone_runtime(source, destination):
    """Link immutable binaries, copy resource/config files before mutating them."""
    destination.mkdir(parents=True, exist_ok=False)
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.exe', '.dll', '.json'):
            if item.suffix.lower() in ('.exe', '.dll'):
                os.link(item, destination/item.name)
            else:
                shutil.copy2(item, destination/item.name)
        elif item.is_dir() and item.name in ('platforms', 'styles', 'imageformats', 'iconengines', 'translations', 'resources'):
            shutil.copytree(item, destination/item.name)
    bridge = destination/'bridge'
    bridge.mkdir()
    os.link(source/'bridge/FreeShipGeometryBridge.exe', bridge/'FreeShipGeometryBridge.exe')
    for name in ('Themes', 'locale', 'Languages'):
        shutil.copytree(source/'bridge'/name, bridge/name)
    return destination


def write(root, relative, text):
    path = root/relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf8', newline='\n')


def reset(root, source):
    for relative in ('FreeShip.ini', '.coordinate-settings/FreeShip.ini', 'FreeShipGeometryBridge.dta'):
        (root/relative).unlink(missing_ok=True)
    shutil.copy2(source/'bridge/locale/FreeShip.cn.po', root/'locale/FreeShip.cn.po')


def context(r, runtime, folder, colors, oracle=False):
    executable = runtime/('bridge/FreeShipGeometryBridge.exe' if oracle else 'FreeShipDocumentService.exe')
    with q.Service(executable, folder, native_linesplan=True) as service:
        start = time.perf_counter()
        response = q.require_ok(service.command('document_context', external_document=True, colors=colors))
        elapsed = (time.perf_counter()-start)*1000
        if not oracle:
            r.case(folder.parent.name+'.native-engine', response.get('preferences_engine')=='cpp-preferences' and
                   response.get('adapter_pid')==0 and response.get('adapter_requests')==0)
        return {k: v for k, v in response.items() if k not in WIRE_FIELDS}, elapsed


def configurations(r, runtime, source, out):
    root = runtime/'bridge'
    # Independent values include every Windows/LCL system index, CLR_DEFAULT,
    # CLR_NONE and signed raw colors, not just the shipped green/blue theme.
    colors = [0, 255, 65280, 16711680, 0x1fffffff, 0x20000000, -1, 0x123456]
    colors += [i-0x80000000 for i in range(32)]
    parent = '[Theme]\nName=Base\n[Graphic]\nLayerColor=clWindow\nUnderWaterColor=clForm\nStationColor=clMenuText\nFontSize=11\nPointSize=4\n'
    child = '[Theme]\nName=Test\nParentTheme=Base\n[Graphic]\nLayerColor=$00112233\nGridColor=123456\nButtockColor=clHighlight\nWaterlineColor=clDefault\nDiagonalColor=clNone\nUnderWaterColorAlpha=133\n'
    cases = [
        ('default', {}),
        ('global', {'FreeShip.ini':'[General]\nLanguage=Russian\nFbmEncoding=cp1251\nMaxUndoMemory=48\n[Graphic]\nTheme=Default\nStudioThemeInitialized=True\n'}),
        ('user-override', {'FreeShip.ini':'[General]\nFbmEncoding=cp1251\nMaxUndoMemory=48\n[Graphic]\nTheme=Original\n',
                           '.coordinate-settings/FreeShip.ini':'[GENERAL]\nFbmEncoding=cp936\nMaxUndoMemory=64\n[Graphic]\nTheme=Default\nStudioThemeInitialized=1\n'}),
        ('parent-and-user-theme', {'FreeShip.ini':'[Graphic]\nTheme=Test\n', 'Themes/Base/theme.ini':parent,
                                  'Themes/Test/theme.ini':child.replace('123456','654321'),
                                  '.coordinate-settings/Themes/Test/theme.ini':child}),
        ('duplicate-and-invalid-integer', {'FreeShip.ini':'[General]\nFbmEncoding=cp1251\nFbmEncoding=cp1252\nMaxUndoMemory=broken\n'}),
        ('negative-undo', {'FreeShip.ini':'[General]\nMaxUndoMemory=-2\n'}),
        ('no-undo', {'FreeShip.ini':'[General]\nMaxUndoMemory=0\n'}),
        ('quoted-values', {'FreeShip.ini':'[General]\nFbmEncoding="cp1251"\nMaxUndoMemory=\'24\'\n[Graphic]\nTheme="Default"\nStudioThemeInitialized="1"\n'}),
    ]
    dta = [3, 0x808040, 0x808040, 0x808040, 0, 255, 0xc0c0c0, 0xffffff, 0x6f6f6f,
           0x4080, 0xe1e1e1, 0xb95c00, 0xff00ff, 0xffff, 0x8000, 0xf0f0f0, 0xffffff, 0x9f9f9f]
    dta += ['', '', '', '', 0x766f37, 2, '不存在.fbm', 'nonexistent.fbm', 0x99ff00, 0x8000ff,
            0xff00ff, 0xff0080, 0x80, 0xe6e6e6, '0 0 800 1000 0', 'Russian', 37]
    cases.append(('legacy-dta', {'FreeShipGeometryBridge.dta':'\n'.join(map(str,dta))+'\n'}))
    for name, files in cases:
        reset(root, source)
        for path, text in files.items(): write(root, path, text)
        native, native_ms = context(r, runtime, out/name/'native', colors)
        old, old_ms = context(r, runtime, out/name/'oracle', colors, True)
        diff = publication.differences(native, old)
        r.case(name+'.all-context-fields-exact', not diff, differences=diff)
        r.case(name+'.legacy-context-does-not-write-config', all((root/path).read_text('utf8')==text for path,text in files.items()))
        r.data.setdefault('context_timings', []).append(dict(case=name,native_ms=native_ms,old_ms=old_ms))
        if name=='parent-and-user-theme':
            r.case(name+'.independent-precedence',native['default_layer_color']==0x112233 and
                   native['presentation_context']['colors']['grid']['raw']==123456 and native['presentation_context']['font_size']==11)
        if name=='default':
            r.case('default.independent-cn-captions',native['presentation_context']['texts']=={'base':'基线','dwl':'DWL','center':'中心'} and native['layer_name_prefix']=='图层')
    reset(root, source)


def catalogs(r, runtime, source, out):
    root=runtime/'bridge'
    # Source fallback, resource context, multiline and escapes have independent
    # expectations; no production strings are modified to match a test result.
    po='''msgid "Layer"
msgstr "fallback layer"

msgctxt "freestringsunit.rs_layer"
msgid "Layer"
msgstr "图\\\"层\\\""
"\\n第二行"

msgctxt "anotherunit.rs_base"
msgid "Base"
msgstr "wrong context"

msgctxt "freestringsunit.rs_base"
msgid "Base"
msgstr ""

#, fuzzy
msgctxt "freestringsunit.rs_part"
msgid "Part"
msgstr "unreviewed"

msgid "Center"
msgstr "中心线"
'''
    write(root,'locale/FreeShip.cn.po',po)
    value,_=context(r,runtime,out/'catalog'/'native',[])
    r.case('catalog.context-and-multiline',value['layer_name_prefix']=='图"层"\n第二行')
    r.case('catalog.empty-source-fallback',value['presentation_context']['texts']['base']=='Base')
    r.case('catalog.fuzzy-source-fallback',value['plate_context']['part']=='Part')
    r.case('catalog.generic-fallback',value['presentation_context']['texts']['center']=='中心线')
    (root/'locale/FreeShip.cn.po').unlink()
    value,_=context(r,runtime,out/'missing-catalog'/'native',[])
    r.case('catalog.missing-source-fallback',value['layer_name_prefix']=='Layer' and value['presentation_context']['texts']['center']=='Center')
    for name,data in [('invalid-utf8',b'\xff'),('truncated-utf8',b'\xc3'),('embedded-nul',b'\x00')]:
        (root/'locale/FreeShip.cn.po').write_bytes(data)
        with q.Service(runtime/'FreeShipDocumentService.exe',out/name/'process') as service:
            response=service.command('document_context',colors=[])
            r.case('catalog.reject-'+name,not response['ok'],error=response.get('error'))
    reset(root,source)


def detached(r, runtime, source, model, out):
    # This is our private hardlink only. Deleting it cannot remove or modify
    # the original executable. With no binary to launch, bootstrap dependence
    # cannot be concealed by a zero PID after an adapter was stopped.
    executable=runtime/'bridge/FreeShipGeometryBridge.exe'
    assert out.parent in runtime.parents and executable.resolve().parent==runtime/'bridge'
    executable.unlink()
    r.case('detached.legacy-executable-absent',not executable.exists())
    publication.independence(r,runtime,model,out/'publication')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'transactions'/'process') as service:
        first=q.require_ok(service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'first'))
        before=dict(service.state);before_bytes=q.require_ok(service.command('save',output=out/'before-save'))['saved_file']
        failures=[('encoding','[General]\nFbmEncoding=unknown\n'),('theme-path','[Graphic]\nTheme=../escape\n'),
                  ('bad-font','[Graphic]\nTheme=Invalid\n')]
        write(runtime/'bridge','Themes/Invalid/theme.ini','[Graphic]\nFontSize=0\n')
        for name, config in failures:
            write(runtime/'bridge','FreeShip.ini',config)
            failed=service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/('invalid-'+name))
            r.case('reject.'+name,not failed['ok'] and service.state==before,error=failed.get('error'))
            # Current immutable context survives a failed new-file bootstrap.
            snapshot=q.require_ok(service.command('snapshot',output=out/('retained-'+name)))
            r.compare('reject.'+name+'.retains-model',first['model'],snapshot['model'])
            r.compare('reject.'+name+'.retains-export',first['dxf'],snapshot['dxf'])
        reset(runtime/'bridge',source)
        for name, value in [('fraction',[1.5]),('range',[2147483648]),('type',['clRed']),('count',[0]*100003)]:
            response=service.command('document_context',colors=value)
            r.case('reject.palette-'+name,not response['ok'] and service.state==before)
        write(runtime/'bridge','locale/FreeShip.cn.po','msgid "unterminated\n')
        response=service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'bad-catalog')
        r.case('reject.invalid-catalog',not response['ok'] and service.state==before)
        reset(runtime/'bridge',source)
        saved=q.require_ok(service.command('save',output=out/'after-save'))
        r.compare('reject.all-failures-preserve-fbm',before_bytes,saved['saved_file'])
        q.require_ok(service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'recover'))
        hydro=q.require_ok(service.command('hydro_defaults',output=out/'hydro'))
        r.case('detached.hydro-defaults',hydro.get('analysis_engine')=='cpp-hydrostatics' and hydro.get('adapter_requests')==0)
        import qt_native_propulsion as propulsion
        import qt_native_resistance as resistance
        for methods,engine in [(propulsion.METHODS,'cpp-propulsion'),(resistance.METHODS,'cpp-resistance')]:
            for method in methods:
                result=q.require_ok(service.command('performance_defaults',calculator=method,output=out/('defaults-'+method)))
                r.case('detached.defaults-'+method,result.get('analysis_engine')==engine and result.get('adapter_requests')==0)
        result=q.require_ok(service.command('cad_iges',output=out/'cad'))
        r.case('detached.cad',result.get('analysis_engine')=='cpp-cad' and result.get('adapter_requests')==0)
        current=json.loads(Path(first['model']).read_text('utf8'))
        q.require_ok(service.command('update_layer',layer_id=current['layers'][0]['id'],changes={'developable':True},output=out/'enable-plates'))
        result=q.require_ok(service.command('develop_plates',output=out/'plates'))
        r.case('detached.plates',result.get('analysis_engine')=='cpp-plates' and result.get('adapter_requests')==0)
        fresh=q.require_ok(service.command('new',document_id=str(q.uuid.uuid4()),length=20,beam=5,draft=2,columns=7,rows=5,units=0,output=out/'new'))
        r.case('detached.new-hull',fresh.get('adapter_requests')==0 and fresh.get('preferences_engine')=='cpp-preferences')
        r.data['new_hull_preferences_ms']=fresh.get('preferences_ms')
    # Actual MainWindow/ModelSession startup and reopening, using its production
    # callbacks in a hidden process. This is deliberately not desktop acceptance.
    history=out/'history.json';request=out/'startup.json'
    for name,path in [('open',str(model)),('restart','')]:
        request.write_text(json.dumps(dict(history_file=str(history),commands=[dict(kind='startup',path=path)])),encoding='utf8')
        result=q.run_child(r,'detached-ui-'+name,runtime/'FreeShipQt.exe',['--verify-startup',request],out/('ui-'+name),platform='offscreen')
        r.data.setdefault('production_runs',[]).append(result)
        if result['status']=='error':raise RuntimeError('Native Qt startup callback failed')
        rows=json.loads(Path(result['artifacts']['observations']).read_text('utf8'))
        r.case('detached.ui-'+name,rows[0]['ready'] and Path(rows[0]['source']).resolve()==model.resolve())


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for protected in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==protected or protected in out.parents:p.error('Use a private output directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-preferences',runtime=str(runtime),limitations=[
        'Native scene-caption bootstrap preserves the existing cn behavior; whole-UI language switching is a separate unfinished UI task.',
        'Configuration mutation and legacy removal use a private runtime; installed runtime is fingerprint-verified.',
        'Offscreen production callbacks do not certify desktop visual, mouse, focus or device behavior.'])
    names=('qt_preferences.py','qt_publication.py','qt_migration.py','qt_editing.py','qt_native_propulsion.py','qt_native_resistance.py','run_tests.py','maintenance/extract_dxf_palette.py')
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in names}
    model=q.DEFAULT_MODELS[0].resolve();model_hash=q.common.digest_file(model);sources=q.current_sources(q.QT);start=time.perf_counter()
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        for name,h in manifest['binaries_sha256'].items():
            if q.common.digest_file(runtime/name)!=h:raise RuntimeError('Runtime file mismatch: '+name)
        r.case('verified-runtime-manifest',True)
        # Includes a context-only invocation of the actual selected installation.
        context(r,runtime,out/'installed-context'/'native',[])
        private=clone_runtime(runtime,out/'private-runtime')
        configurations(r,private,runtime,out/'configurations')
        catalogs(r,private,runtime,out/'catalogs')
        detached(r,private,runtime,model,out/'detached')
        r.case('protected-model-unchanged',q.common.digest_file(model)==model_hash)
        r.case('runtime-files-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
        r.case('sources-unchanged-during-run',q.current_sources(q.QT)==sources)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.case('desktop-acceptance',status='not_run',reason='No desktop input/visual acceptance was performed.')
    r.data['elapsed_s']=time.perf_counter()-start;editing.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
