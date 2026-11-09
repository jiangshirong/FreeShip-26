"""Crash recovery through real document service, ModelSession and Qt file actions.

All files/settings are private. Container parsing and payload hashes below are
independent of the C++ producer. Crash helper exits without running destructors.
"""
import argparse, ctypes, hashlib, json, os, shutil, struct, time, uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e

FILES=('qt_recovery.py','qt/recovery_tests.cpp','qt_startup.py','qt_migration.py','qt_editing.py','run_tests.py')
MAGIC=b'FreeShipRecovery\n'
def dump(path,data):
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
def same_path(actual,expected):return bool(actual) and Path(actual).resolve()==Path(expected).resolve()
def capsule(path):
    raw=path.read_bytes();assert raw.startswith(MAGIC)
    start=len(MAGIC)+4;n=struct.unpack('<I',raw[len(MAGIC):start])[0]
    h=json.loads(raw[start:start+n]);payload=raw[start+n:]
    assert len(payload)==h['bytes'] and hashlib.sha256(payload).hexdigest()==h['sha256']
    return h,payload
def mutate_capsule(path,payload):
    h,_=capsule(path);h.update(bytes=len(payload),sha256=hashlib.sha256(payload).hexdigest());b=json.dumps(h).encode()
    path.write_bytes(MAGIC+struct.pack('<I',len(b))+b+payload)
def wait_service(pid):
    if os.name=='nt':
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.OpenProcess.restype=ctypes.c_void_p;kernel.OpenProcess.argtypes=(ctypes.c_uint,ctypes.c_int,ctypes.c_uint)
        kernel.WaitForSingleObject.argtypes=(ctypes.c_void_p,ctypes.c_uint);kernel.CloseHandle.argtypes=(ctypes.c_void_p,)
        handle=kernel.OpenProcess(0x100000,False,pid)
        if handle:
            try:assert kernel.WaitForSingleObject(handle,10000)==0,'Document service survived parent crash'
            finally:kernel.CloseHandle(handle)
def production(r,runtime,out,model):
    root=out/'恢复副本';history=out/'history.json';original=model.read_bytes()
    def crash(name,new=False,which=model):
        request=out/(name+'.json');dump(request,dict(mode='crash',root=str(root),model=str(which),new=new))
        result=q.run_child(r,name,runtime/'qt_recovery_tests.exe',[request],out/name,timeout=150,platform='windows')
        if result.get('status')!='pass':raise ValueError('Crash producer failed: '+str(result))
        wait_service(int(result['service_pid']));h,payload=capsule(Path(result['capsule']))
        r.case(name+'.complete-committed-revision',h['document_id']==result['document_id'] and payload!=original)
        r.case(name+'.source-not-modified',model.read_bytes()==original)
        return result
    def run(name,commands,folder=root):
        request=out/(name+'.json');dump(request,dict(history_file=str(history),recovery_root=str(folder),commands=commands))
        child=q.run_child(r,name,runtime/'FreeShipQt.exe',['--verify-startup',request],out/name,timeout=150,platform='windows')
        r.data.setdefault('production',[]).append(child)
        if child.get('status') in ('fail','error'):raise ValueError('Production recovery workflow failed: '+str(child))
        rows=json.loads(Path(child['artifacts']['observations']).read_text('utf8'))
        r.case(name+'.every-command-observed',len(rows)==len(commands));return rows
    def cmd(kind,**kw):return dict(kind=kind,**kw)
    first=crash('crash-existing');path=Path(first['capsule']);_,payload=capsule(path)
    second=crash('crash-second-window');second_path=Path(second['capsule'])
    run('later-keeps-all',[cmd('startup',recovery_decision='later')])
    r.case('later-retains-both-sessions',path.exists() and second_path.exists())
    row=run('explicit-file-precedence',[cmd('startup',path=str(model))])[0]
    r.case('explicit-open-does-not-overwrite-recovery',row['ready'] and not row['modified'] and path.exists())
    target=out/'另存为-Восстановлено.fbm';blocked=out/'blocked-destination';blocked.mkdir()
    rows=run('restore-save-failure-and-save',[cmd('startup',recovery_decision='restore',recovery_id=first['document_id']),cmd('save',path=str(blocked)),cmd('save',path=str(target))])
    recovered,failed,saved=rows
    r.case('restored-is-unsaved-and-forces-save-as',recovered['ready'] and recovered['modified'] and not recovered['source'] and same_path(recovered['recovery_origin'],model))
    r.case('failed-save-keeps-recovery-and-dirty-state',failed['modified'] and failed['error'] and failed['recovery_file']==recovered['recovery_file'])
    r.case('save-commits-exact-fbm',target.read_bytes()==payload and not saved['modified'] and same_path(saved['source'],target))
    r.case('save-cleans-only-its-session',not path.exists() and not Path(recovered['recovery_file']).exists() and second_path.exists())
    for key in ('scene','dxf'):r.compare('restored-'+key+'-exact',Path(recovered[key]),Path(first[key]))
    original_snapshot=json.loads(Path(first['model']).read_text('utf8'));restored=json.loads(Path(recovered['model']).read_text('utf8'))
    for key in ('control_points','control_edges','control_faces','layers','settings','control_curves','backgrounds'):
        r.case('restored-'+key+'-exact',key in restored and restored[key]==original_snapshot[key])
    rows=run('undo-redo-cleanup',[cmd('startup',path=str(model)),cmd('move'),cmd('undo'),cmd('redo'),cmd('discard_recovery')])
    r.case('undo-to-saved-clears-recovery',not rows[2]['modified'] and not rows[2]['recovery_file'])
    r.case('redo-recreates-recovery',rows[3]['modified'] and bool(rows[3]['recovery_file']))
    r.case('explicit-discard-clears-recovery',not Path(rows[3]['recovery_file']).exists() and not rows[4]['recovery_file'])
    rows=run('normal-close-cancel',[cmd('startup',path=str(model)),cmd('move'),cmd('close',discard=False)])
    cancelled_copy=Path(rows[1]['recovery_file'])
    r.case('normal-close-cancel-keeps-dirty-session-and-copy',rows[-1]['ready'] and rows[-1]['modified'] and cancelled_copy.exists())
    rows=run('normal-close-discard',[cmd('startup',path=str(model)),cmd('move'),cmd('close',discard=True)])
    r.case('normal-close-discard-cleans-its-copy',rows[-1]['operation']=='close' and not Path(rows[1]['recovery_file']).exists() and cancelled_copy.exists())
    # Deletion is confirmed through the actual dialog button/confirmation route.
    run('delete-cancel',[cmd('recover',recovery_decision='delete',recovery_id=second['document_id'],confirm_delete=False)])
    r.case('delete-cancel-preserves-copy',second_path.exists())
    run('delete-confirm',[cmd('recover',recovery_decision='delete',recovery_id=second['document_id'],confirm_delete=True)])
    r.case('delete-confirm-removes-selected-copy',not second_path.exists())
    fresh=crash('crash-never-saved',new=True);fresh_target=out/'new-recovered.fbm'
    rows=run('restore-never-saved',[cmd('startup',recovery_decision='restore',recovery_id=fresh['document_id']),cmd('save',path=str(fresh_target))])
    r.case('never-saved-document-recovered',rows[0]['ready'] and rows[0]['modified'] and not rows[0]['source'] and not rows[0]['recovery_origin'] and fresh_target.exists())
    corrupt=crash('crash-before-corruption');bad=Path(corrupt['capsule']);raw=bad.read_bytes();bad.write_bytes(raw[:-1]+bytes([raw[-1]^1]))
    row=run('checksum-failure',[cmd('startup',recovery_decision='restore',recovery_id=corrupt['document_id'])])[0]
    r.case('checksum-failure-is-visible-and-keeps-copy',bool(row['error']) and not row['ready'] and bad.exists())
    bad.write_bytes(raw);mutate_capsule(bad,b'invalid FBM with valid container checksum')
    rows=run('malformed-fbm-keeps-current-document',[cmd('startup',path=str(model)),cmd('recover',recovery_decision='restore',recovery_id=corrupt['document_id'])])
    r.case('parse-failure-preserves-live-model-history-and-copy',bool(rows[1]['error']) and rows[1]['ready'] and same_path(rows[1]['source'],model) and same_path(rows[1]['last'],model) and bad.exists())
    # Recovery storage failure is independent of successful document edits.
    unavailable=out/'storage-is-file';unavailable.write_bytes(b'keep')
    rows=run('unavailable-storage',[cmd('startup',path=str(model)),cmd('move'),cmd('save',path=str(out/'manual-save.fbm'))],unavailable/'recovery')
    r.case('storage-failure-does-not-rollback-edit',rows[1]['ready'] and rows[1]['modified'] and not rows[1]['error'] and bool(rows[1]['recovery_error']))
    r.case('manual-save-still-works-with-storage-failure',not rows[2]['modified'] and (out/'manual-save.fbm').exists() and unavailable.read_bytes()==b'keep')
    r.case('all-original-bytes-protected',model.read_bytes()==original)

def service(r,runtime,out,model):
    out.mkdir();root=out/'recovery'
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        loaded=q.require_ok(s.command('load',path=str(model),document_id=str(uuid.uuid4()),recovery_root=str(root),recovery_source=str(model),output=out/'load'))
        m=json.loads(Path(loaded['model']).read_text('utf8'));p=next(p for p in m['control_points'] if not p['locked'])
        position=p['p'].copy();position[0]+=.125
        changed=q.require_ok(s.command('move_point',point_id=p['id'],position=position,output=out/'edit'));recovery=Path(changed['recovery_file']);before=recovery.read_bytes()
        saved=q.require_ok(s.command('save',output=out/'save'))
        r.case('service.exact-save-bytes',capsule(recovery)[1]==Path(saved['saved_file']).read_bytes())
        # Save staging is not the user's destination commit.
        r.case('service.staged-save-does-not-clear-recovery',recovery.read_bytes()==before and saved['modified'])
        wrong=s.command('move_point',point_id=p['id'],position=position,expected_revision=changed['revision']-1,output=out/'stale')
        r.case('service.stale-edit-keeps-capsule',not wrong['ok'] and recovery.read_bytes()==before)
        broken=out/'bad.fbm';broken.write_bytes(b'broken')
        bad=s.command('load',path=str(broken),document_id=str(uuid.uuid4()),recovery_root=str(root),output=out/'bad-load')
        r.case('service.failed-load-keeps-capsule',not bad['ok'] and recovery.read_bytes()==before)
        q.require_ok(s.command('snapshot',output=out/'snapshot'))
        for key in ('scene','dxf'):
            r.compare('service.read-only-'+key,Path(changed[key]),out/'snapshot'/Path(changed[key]).name)
        r.case('service.read-only-does-not-rewrite',recovery.read_bytes()==before)
        r.data['recovery_ms']=changed['recovery_ms']
        (out/'explicit-saved.fbm').write_bytes(Path(saved['saved_file']).read_bytes())
        q.require_ok(s.command('mark_saved',recovery_source=str(out/'explicit-saved.fbm')))
        r.case('service.confirmed-save-clears-capsule',not recovery.exists())
        real=q.DEFAULT_MODELS[2];original_hash=q.common.digest_file(real)
        loaded=q.require_ok(s.command('load',path=str(real),document_id=str(uuid.uuid4()),recovery_root=str(root),recovery_source=str(real),output=out/'real-load'))
        m=json.loads(Path(loaded['model']).read_text('utf8'));p=next(p for p in m['control_points'] if not p['locked'] and p['batch_movable'])
        timings=[]
        for i in range(3):
            position=p['p'].copy();position[0]+=(i+1)*.125
            changed=q.require_ok(s.command('move_point',point_id=p['id'],position=position,output=out/f'real-edit-{i}'))
            timings.append(changed['recovery_ms'])
        saved=q.require_ok(s.command('save',output=out/'real-save'));h,payload=capsule(Path(changed['recovery_file']))
        r.case('real-ship.latest-full-fbm-exact',payload==Path(saved['saved_file']).read_bytes() and h['revision']==changed['revision'])
        r.case('real-ship.one-copy-for-repeated-edits',len(list(root.glob('*.fsr')))==1)
        r.case('real-ship-original-protected',q.common.digest_file(real)==original_hash)
        r.data['real_ship_recovery']={'bytes':len(payload),'checkpoint_ms':timings}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--runtime',type=Path);parser.add_argument('--output',type=Path,required=True);a=parser.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:parser.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);before=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    r.data.update(suite='qt-recovery',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=before,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},limitations=['Hidden production callbacks are separate from desktop acceptance.','Only committed document revisions are recovered; undo history and edits still in progress are not persisted.'])
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        r.case('runtime-manifest',True);model=out/'原船模-Женя.fbm';shutil.copy2(q.DEFAULT_MODELS[0],model)
        req=out/'store.json';dump(req,dict(model=str(model)))
        r.data['store']=q.run_child(r,'store',runtime/'qt_recovery_tests.exe',[req],out/'store',timeout=60,platform='windows')
        production(r,runtime,out/'production',model);service(r,runtime,out/'service',model)
        q.run_child(r,'session-regression',runtime/'qt_session_tests.exe',[model],out/'session-regression',timeout=150,platform='windows')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==before)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='No desktop automation; production dialogs, process crash and filesystem routes exercised in isolation.')
        e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
