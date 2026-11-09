"""Water tint opacity: FBM bytes, immutable geometry, production dialog/save and CPU/GPU pixels.

This is a tint strength, not hull transparency. Historical blue-channel typo is
not an oracle; the renderer probe uses independently specified RGB expectations.
"""
import argparse,json,os,shutil,struct,subprocess,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_native_project as fixtures
import qt_preview_image as preview
import fbm_preview_bytes as fbm

FIELD='underwater_color_alpha'
FILES=('qt_underwater.py','qt/underwater_tests.cpp','fbm_preview_bytes.py','qt_native_project.py','qt_preview_image.py','qt_migration.py','qt_editing.py','run_tests.py')
def read(path):return json.loads(Path(path).read_text('utf8'))
def patch(raw,alpha):
    data=bytearray(raw);struct.pack_into('<i',data,fbm.envelope(data)['underwater_alpha_offset'],alpha);return bytes(data)

def formats(r,runtime,out):
    out.mkdir();env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','')
    for version in range(45):
        source=out/f'v{version}.fbm';fixtures.fixture(source,version=version,rich=version>=20)
        before=source.read_bytes();original=fbm.envelope(before,234)[FIELD]
        for name,alpha in [('noop',original),('zero',0),('middle',128),('full',255)]:
            minimum_only=version<44 and alpha!=original
            req=out/f'v{version}-{name}.json';target=req.with_suffix('.fbm')
            req.write_text(json.dumps(dict(bindings=dict(fbm_encoding='cp1251',default_underwater_color=0x800000,default_underwater_alpha=234),request=dict(method='update_project',changes={FIELD:alpha}),minimum_only=minimum_only)),encoding='utf8')
            child=subprocess.run([str(runtime/'native_project_edit_probe.exe'),str(source),str(req),str(target)],capture_output=True,env=env,timeout=15,creationflags=q.common.FLAGS)
            req.with_suffix('.log').write_bytes(child.stdout+child.stderr)
            if child.returncode:raise RuntimeError('Project probe crashed')
            result=json.loads(child.stdout);r.case(f'v{version}.{name}.accepted',result['ok'],observed=result)
            if not result['ok']:continue
            r.case(f'v{version}.{name}.minimum',result['minimum_version']==(44 if minimum_only else version))
            if not minimum_only:r.case(f'v{version}.{name}.bytes',target.read_bytes()==(before if name=='noop' else patch(before,alpha)))

def backend(r,runtime,out):
    out.mkdir();model=out/'input.fbm';fixtures.fixture(model,rich=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        def save(name):return Path(call(name,'save')['saved_file']).read_bytes()
        base=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()));raw=model.read_bytes();state=dict(s.state)
        call('noop','update_project',changes={FIELD:123});r.case('noop.keeps-revision-and-bytes',s.state==state and save('noop-save')==raw)
        for alpha in (0,127,255):
            changed=call(f'alpha-{alpha}','update_project',changes={FIELD:alpha});actual=save(f'save-{alpha}')
            r.case(f'alpha-{alpha}.exact-bytes',actual==patch(raw,alpha));r.case(f'alpha-{alpha}.published',read(changed['model'])['settings'][FIELD]==alpha)
            for kind in ('scene','dxf','surface','sections'):r.compare(f'alpha-{alpha}.unchanged-'+kind,base[kind],changed[kind])
        call('undo','undo');r.case('undo.exact',save('undo-save')==patch(raw,127));call('redo','redo');r.case('redo.exact',save('redo-save')==patch(raw,255))
        for name,value in [('negative',-1),('high',256),('fraction',.1),('bool',True),('text','127'),('null',None)]:
            state=dict(s.state);response=s.command('update_project',output=out/('invalid-'+name),changes={FIELD:value,'name':'must not apply'})
            r.case('invalid.'+name,not response['ok'] and s.state==state)
        r.case('invalid.mixed-atomic',save('invalid-save')==patch(raw,255))
        stale=s.command('update_project',output=out/'stale',expected_revision=1,changes={FIELD:64});r.case('stale.rejected',not stale['ok'])
        saved=out/'save-255/saved.fbm';call('reopen','load',path=str(saved),document_id=str(q.uuid.uuid4()))
        r.case('reopen.exact',save('reopened-save')==saved.read_bytes())
        for version in (0,15,43):
            old=out/f'old-{version}.fbm';fixtures.fixture(old,version=version,rich=False)
            call(f'old-{version}-load','load',path=str(old),document_id=str(q.uuid.uuid4()))
            r.case(f'old-{version}.passive-exact',save(f'old-{version}-original')==old.read_bytes())
            changed=call(f'old-{version}-change','update_project',changes={FIELD:127})
            upgraded=save(f'old-{version}-save');r.case(f'old-{version}.promoted',fbm.envelope(upgraded)['version']==44 and fbm.envelope(upgraded)[FIELD]==127)
            r.case(f'old-{version}.published',read(changed['model'])['settings'][FIELD]==127)
            call(f'old-{version}-undo','undo');r.case(f'old-{version}.undo-exact',save(f'old-{version}-undo-save')==old.read_bytes())

def production(r,runtime,out):
    out.mkdir();private=out/'input.fbm';shutil.copy2(q.DEFAULT_MODELS[0],private)
    # Stage a shaded, preview-enabled model through the native backend, isolated
    # from user files. Use non-round opacity to catch UI percent quantization.
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'prepare',native_linesplan=True) as s:
        q.require_ok(s.command('load',path=str(private),document_id=str(q.uuid.uuid4()),output=out/'prepared-load'))
        initial=q.require_ok(s.command('update_project',changes={FIELD:234,'shade_underwater':True,'save_preview':True,'underwater_color':0xdc6414},output=out/'prepared-settings'))
        old_picture=preview.picture(r,runtime,initial,out/'original-preview','original-preview')
        saved=q.require_ok(s.command('save',output=out/'prepared-save'));private.write_bytes(Path(saved['saved_file']).read_bytes())
    before=private.read_bytes();edits={'underwater_opacity_percent':50}
    commands=[dict(kind='startup',path=str(private)),dict(kind='project_action',changes={}),dict(kind='project_action',changes=edits,cancel=True),
        dict(kind='project_action',changes=edits),dict(kind='undo'),dict(kind='redo'),dict(kind='save_action')]
    request=out/'request.json';request.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,'project-actions',runtime/'FreeShipQt.exe',['--verify-startup',request],out/'qt',platform='windows');r.data['production']=child
    rows=read(child['artifacts']['observations']);r.case('production.complete',len(rows)==len(commands) and all(not row['error'] for row in rows))
    models=[read(row['model']) for row in rows]
    r.case('production.untouched-no-quantization',models[0]==models[1]);r.case('production.cancel-exact',models[0]==models[2])
    for index,alpha in ((3,127),(4,234),(5,127),(6,127)):r.case(f'production.{index}.opacity',models[index]['settings'][FIELD]==alpha)
    r.case('production.only-approved-bytes',fbm.replace(private.read_bytes(),enabled=False)==fbm.replace(patch(before,127),enabled=False))
    r.case('production.backup-exact',private.with_suffix('.Bak').read_bytes()==before)
    for i in range(1,len(rows)):
        for kind in ('scene','dxf'):r.compare(f'production.{i}.unchanged-'+kind,rows[0][kind],rows[i][kind])
    current=preview.picture(r,runtime,rows[-1],out/'current-preview','current-preview')
    r.case('production.preview-matches-current-model',fbm.envelope(private.read_bytes())['jpeg']==current)
    r.case('production.opacity-changes-preview',old_picture!=current)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT)
    originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-underwater',runtime=str(runtime),qt_sources_sha256=sources,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);formats(r,runtime,out/'formats');backend(r,runtime,out/'service')
        for platform in ('offscreen','windows'):
            r.data[platform]=q.run_child(r,platform,runtime/'qt_underwater_tests.exe',[],out/platform,platform=platform)
        production(r,runtime,out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='Hidden production actions and rendered images; no physical desktop acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
