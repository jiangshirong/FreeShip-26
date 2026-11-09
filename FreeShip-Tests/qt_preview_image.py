"""FBM thumbnail refresh: exact persistence, staged save and actual Qt Save.

Private copies only. Pixel output is independently decoded; geometry and every
byte outside the JPEG envelope must remain unchanged by preview replacement.
"""
import argparse,base64,io,json,shutil,time
from pathlib import Path
from PIL import Image
import qt_migration as q
import qt_editing as e
import qt_document
import fbm_preview_bytes as fbm
import qt_native_project as project_fixtures

FILES=('qt_preview_image.py','fbm_preview_bytes.py','qt_native_project.py','qt/preview_image_tests.cpp','qt/fbm_codec_tests.cpp','qt_document.py','qt_migration.py','qt_editing.py','run_tests.py')

def picture(r,runtime,snapshot,out,name):
    child=q.run_child(r,name,runtime/'qt_preview_image_tests.exe',[snapshot[k] for k in ('model','surface','sections')],out,platform='windows')
    r.data.setdefault('renders',[]).append(child)
    path=Path(child['artifacts']['jpeg']);pixels=Image.open(path);pixels.load()
    r.case(name+'.independent-jpeg-decoder',pixels.size==(400,300) and pixels.format=='JPEG')
    return path.read_bytes()

def service(r,runtime,out,model):
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**args):return q.require_ok(s.command(method,output=out/name,**args))
        def saved(name,**args):return Path(call(name,'save',**args)['saved_file']).read_bytes()
        first=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()))
        r.case('service.untouched-save-exact',saved('untouched')==model.read_bytes())
        pixels=picture(r,runtime,first,out/'renderer','render-original')
        status=call('status-before','document_status');state=dict(s.state)
        prepared=saved('prepared',preview_jpeg=base64.b64encode(pixels).decode())
        r.case('service.preview-only-byte-change',prepared==fbm.replace(model.read_bytes(),pixels))
        r.case('service.prepare-does-not-commit',s.state==state and call('status-prepared','document_status')['fbm_sha256']==status['fbm_sha256'])
        # A save whose real destination failed never confirms. A new plain save
        # must not silently adopt the earlier prepared preview.
        r.case('service.cancelled-preparation-keeps-original',saved('cancelled')==model.read_bytes())
        saved('prepared-again',preview_jpeg=base64.b64encode(pixels).decode());call('confirmed','mark_saved')
        r.case('service.confirm-and-repeat-exact',saved('confirmed-bytes')==prepared and not s.state['modified'])
        before=call('state-confirmed','document_status')['fbm_sha256']
        for name,payload in [('base64','%%bad'),('jpeg',base64.b64encode(b'bad image').decode())]:
            result=s.command('save',output=out/('bad-'+name),preview_jpeg=payload)
            r.case('service.reject-'+name,not result['ok'] and call('status-'+name,'document_status')['fbm_sha256']==before)
        blocked=out/'blocked';blocked.write_text('preserve',encoding='utf8')
        result=s.command('save',output=blocked,preview_jpeg=base64.b64encode(pixels).decode())
        r.case('service.failed-output-does-not-adopt-preview',not result['ok'] and blocked.read_text('utf8')=='preserve' and saved('after-failure')==prepared)
        data=json.loads(Path(first['model']).read_text('utf8'));point=next(p for p in data['control_points'] if not p.get('locked') and p.get('batch_movable',True))
        position=list(point['p']);position[0]+=2
        moved=call('move','move_point',point_id=point['id'],position=position)
        raw=saved('edited-raw');current=picture(r,runtime,moved,out/'changed-renderer','render-edited')
        r.case('service.edited-model-changes-preview',current!=pixels)
        updated=saved('edited-preview',preview_jpeg=base64.b64encode(current).decode())
        r.case('service.edited-preview-only-byte-change',updated==fbm.replace(raw,current))
        snapshot=call('after-preview','snapshot')
        for kind in ('scene','dxf','surface','sections'):r.compare('service.preview-preserves-'+kind,moved[kind],snapshot[kind])
        stale=s.command('save',output=out/'stale',expected_revision=1,preview_jpeg=base64.b64encode(pixels).decode())
        r.case('service.stale-preview-rejected',not stale['ok'])
        call('mark-edited','mark_saved');r.case('service.edited-clean-repeat-exact',saved('repeat-edited')==updated and not s.state['modified'])
    # Respect the file's disabled-preview flag, even for an explicit request.
    disabled=out/'disabled.fbm';disabled.write_bytes(fbm.replace(model.read_bytes(),enabled=False))
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'disabled-process',native_linesplan=True) as s:
        q.require_ok(s.command('load',path=str(disabled),document_id=str(q.uuid.uuid4()),output=out/'disabled-load'))
        made=q.require_ok(s.command('save',preview_jpeg=base64.b64encode(pixels).decode(),output=out/'disabled-save'))
        r.case('service.disabled-preview-preserved',not made['preview_updated'] and Path(made['saved_file']).read_bytes()==disabled.read_bytes())
        for value in (0,'true',None):
            rejected=s.command('update_project',changes={'save_preview':value},output=out/('invalid-toggle-'+str(value)))
            r.case('service.invalid-preview-flag-'+str(value),not rejected['ok'])
    # Explicitly enabling the option on a pre-2.1 file uses normal version
    # promotion. Passive loading/saving of old files is covered byte-exactly.
    old=out/'version-15.fbm';project_fixtures.fixture(old,version=15,rich=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'old-process',native_linesplan=True) as s:
        q.require_ok(s.command('load',path=str(old),document_id=str(q.uuid.uuid4()),output=out/'old-load'))
        q.require_ok(s.command('update_project',changes={'save_preview':True},output=out/'old-enable'))
        saved=q.require_ok(s.command('save',output=out/'old-save'))
        info=fbm.envelope(Path(saved['saved_file']).read_bytes())
        r.case('service.explicit-enable-promotes-old-format',info['enabled'] and info['version']>=16)
        q.require_ok(s.command('undo',output=out/'old-undo'))
        saved=q.require_ok(s.command('save',output=out/'old-save-undo'))
        r.case('service.undo-restores-old-format-exactly',Path(saved['saved_file']).read_bytes()==old.read_bytes())

def lifecycle(r,runtime,out,model):
    out.mkdir();private=out/'input.fbm';private.write_bytes(fbm.replace(model.read_bytes(),enabled=False))
    commands=[dict(kind='startup',path=str(private)),
        dict(kind='update_project',changes={'save_preview':True}),dict(kind='save',path=str(out/'enabled.fbm')),
        dict(kind='update_project',changes={'save_preview':False}),dict(kind='save',path=str(out/'disabled.fbm')),
        dict(kind='undo'),dict(kind='save',path=str(out/'undo.fbm')),
        dict(kind='redo'),dict(kind='save',path=str(out/'redo.fbm')),
        dict(kind='new_hull',fields=dict(length=10,beam=4,draft=1,columns=7,rows=5,units=0)),
        dict(kind='save',path=str(out/'new.fbm')),dict(kind='save_action')]
    req=out/'request.json';req.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,'preview-lifecycle',runtime/'FreeShipQt.exe',['--verify-startup',req],out/'qt',platform='windows')
    r.data.setdefault('production_runs',[]).append(child)
    rows=json.loads(Path(child['artifacts']['observations']).read_text('utf8'))
    r.case('lifecycle.commands-complete',len(rows)==len(commands) and all(not row['error'] for row in rows))
    r.case('lifecycle.disabled-and-unchanged-skip-render',all(rows[i]['save_preview_ms']==0 for i in (4,8,11)))
    for name,enabled,step in [('enabled',True,1),('disabled',False,3),('undo',True,5),('redo',False,7),('new',True,9)]:
        path=out/(name+'.fbm');info=fbm.envelope(path.read_bytes())
        r.case('lifecycle.'+name+'.persisted-flag',info['enabled']==enabled)
        settings=json.loads(Path(rows[step]['model']).read_text('utf8'))['settings']
        r.case('lifecycle.'+name+'.published-flag',settings['save_preview']==enabled)
        if enabled:
            expected=picture(r,runtime,rows[step],out/(name+'-render'),'lifecycle.'+name+'.render')
            r.case('lifecycle.'+name+'.current-image',info['jpeg']==expected)
        if name!='new':r.case('lifecycle.'+name+'.other-bytes-preserved',fbm.replace(path.read_bytes(),enabled=False)==private.read_bytes())
        with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/(name+'-legacy-reader'),native_linesplan=False) as legacy:
            loaded=q.require_ok(legacy.command('load',path=str(path),document_id=str(q.uuid.uuid4()),output=out/(name+'-legacy-load')))
            r.case('lifecycle.'+name+'.legacy-readable',Path(loaded['model']).is_file())
    r.case('lifecycle.input-preserved',private.read_bytes()==fbm.replace(model.read_bytes(),enabled=False))

def production(r,runtime,out,model):
    prefix=out.name;out.mkdir();private=out/'ship.fbm';shutil.copy2(model,private);before=private.read_bytes()
    commands=[dict(kind='startup',path=str(private)),dict(kind='save_action'),dict(kind='move',dx=2),dict(kind='save_action'),dict(kind='save_action')]
    req=out/'request.json';req.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,prefix,runtime/'FreeShipQt.exe',['--verify-startup',req],out/'qt',platform='windows');r.data.setdefault('production_runs',[]).append(child)
    observations=child.get('artifacts',{}).get('observations')
    if not observations:raise RuntimeError('Actual Save workflow did not produce observations')
    rows=json.loads(Path(observations).read_text('utf8'));r.case(prefix+'.all-commands-observed',len(rows)==len(commands))
    r.data.setdefault('production_save_times',[]).append(dict(model=str(model),preview_ms=rows[3]['save_preview_ms']))
    r.case(prefix+'.unchanged-save-skips-render',rows[1]['save_preview_ms']==rows[4]['save_preview_ms']==0)
    r.case(prefix+'.no-save-errors',all(not row['error'] for row in rows))
    after=private.read_bytes();preview=fbm.envelope(after)['jpeg']
    r.case(prefix+'.edited-preview-updated',preview!=fbm.envelope(before)['jpeg'])
    expected=picture(r,runtime,rows[2],out/'expected',prefix+'.render-current-model')
    r.case(prefix+'.preview-matches-current-model',preview==expected)
    r.case(prefix+'.first-noop-preserves-file',not rows[1]['backup_file'])
    r.case(prefix+'.backup-exact',private.with_suffix('.Bak').read_bytes()==before)
    r.case(prefix+'.repeat-does-not-rotate-backup',not rows[4]['backup_file'] and not rows[4]['modified'])
    for a,b in ((2,3),(3,4)):
        for kind in ('scene','dxf'):r.compare(f'{prefix}.{a}-{b}.{kind}',rows[a][kind],rows[b][kind])
    req=out/'reopen.json';req.write_text(json.dumps(dict(history_file=str(out/'history-reopen.json'),commands=[dict(kind='startup',path=str(private)),dict(kind='save_action')])),encoding='utf8')
    reopened=q.run_child(r,prefix+'.reopen',runtime/'FreeShipQt.exe',['--verify-startup',req],out/'reopen',platform='windows')
    r.data.setdefault('reopen_runs',[]).append(reopened);r.case(prefix+'.restart-noop-byte-exact',private.read_bytes()==after)
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'legacy-reader',native_linesplan=False) as legacy:
        loaded=q.require_ok(legacy.command('load',path=str(private),document_id=str(q.uuid.uuid4()),output=out/'legacy-load'))
        r.case(prefix+'.legacy-reader-accepts-saved-preview',Path(loaded['model']).is_file())

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];start=time.monotonic()
    originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-preview-image',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        r.case('runtime-manifest',True);qt_document.run_codec(r,runtime,q.DEFAULT_MODELS,out)
        stale=io.BytesIO();Image.new('RGB',(400,300),(180,80,100)).save(stale,format='JPEG',quality=90)
        model=out/'preview-enabled.fbm';model.write_bytes(fbm.replace(q.DEFAULT_MODELS[0].read_bytes(),stale.getvalue()))
        service(r,runtime,out/'service',model);production(r,runtime,out/'production',model)
        lifecycle(r,runtime,out/'lifecycle',model)
        for i,original in enumerate(q.DEFAULT_MODELS[1:],2):
            private=out/f'model-{i}.fbm';private.write_bytes(fbm.replace(original.read_bytes(),stale.getvalue()))
            production(r,runtime,out/f'production-{i}',private)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('original-models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-preview-browser-and-file-picker',status='not_run',reason='Hidden Qt Save actions and saved JPEG evidence only; no desktop automation.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
