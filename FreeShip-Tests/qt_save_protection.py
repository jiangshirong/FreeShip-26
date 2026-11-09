"""Previous-version backups, conflicting writers and interrupted save confirmation.

Uses production ModelSession and actual Qt Save action. Native file pickers and
non-cooperating-writer races after the last content check remain separate limits.
"""
import argparse, json, shutil
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_save_confirmation

FILES=('qt_save_protection.py','qt_save_confirmation.py','qt/save_protection_tests.cpp','qt_startup.py','qt_migration.py','qt_editing.py','run_tests.py')
def production(r,runtime,out,original):
    out.mkdir(parents=True);model=out/'船模-Женя.fbm';shutil.copy2(original,model);before=model.read_bytes()
    commands=[dict(kind='startup',path=str(model)),dict(kind='move',dx=.125),dict(kind='save_action'),dict(kind='save_action'),
              dict(kind='move',dx=.125),dict(kind='save_action'),dict(kind='undo'),dict(kind='save_action'),dict(kind='redo'),dict(kind='save_action')]
    request=out/'request.json';request.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands),ensure_ascii=False),encoding='utf8')
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-startup',request],out/'window',timeout=150,platform='windows');r.data['production']=child
    observation=child.get('artifacts',{}).get('observations')
    if not observation:
        if any(c.get('status')=='not_run' for c in child.get('cases',[])):return
        raise RuntimeError('Save action probe did not produce observations')
    rows=json.loads(Path(observation).read_text('utf8'))
    r.case('production.every-command-observed',len(rows)==len(commands))
    r.case('production.no-operation-errors',all(not row['error'] for row in rows))
    if len(rows)!=len(commands):return
    r.case('production.first-backup-exact',Path(rows[2]['backup_snapshot']).read_bytes()==before)
    r.case('production.unchanged-save-does-not-rotate',not rows[3]['backup_file'] and not rows[3]['modified'])
    r.case('production.dirty-revisions-saved',all(not rows[i]['modified'] and rows[i]['operation']=='save' for i in (2,3,5,7,9)))
    r.case('production.backup-announced',all('备份' in rows[i]['status_text'] for i in (2,5,7,9)))
    r.case('production.undo-and-redo-backups-exact',Path(rows[5]['backup_snapshot']).read_bytes()==Path(rows[9]['backup_snapshot']).read_bytes() and Path(rows[7]['backup_snapshot']).read_bytes()==model.read_bytes())
    r.case('production.previous-version-is-distinct',Path(rows[9]['backup_snapshot']).read_bytes()!=before and Path(rows[9]['backup_snapshot']).read_bytes()!=model.read_bytes())
    for a,b in ((1,2),(2,3),(4,5),(6,7),(8,9),(2,7),(5,9)):
        for key in ('scene','dxf'):r.compare(f'production.{a}-{b}.{key}',Path(rows[a][key]),Path(rows[b][key]))
    r.case('production.final-backup-matches-observed',model.with_suffix('.Bak').read_bytes()==Path(rows[-1]['backup_snapshot']).read_bytes())
    r.case('production.saved-document-recovery-cleaned',not list((out/'recovery').glob('*.fsr')))
    r.case('production.one-backup-only',len(list(out.glob('*.Bak')))==1)
    # Reopen the actual backup using the ordinary application route.
    req=out/'reopen.json';req.write_text(json.dumps(dict(history_file=str(out/'history2.json'),commands=[dict(kind='startup',path=str(model.with_suffix('.Bak')))])),encoding='utf8')
    opened=q.run_child(r,'production.reopen-backup',runtime/'FreeShipQt.exe',['--verify-startup',req],out/'reopen',timeout=120,platform='windows')
    if opened.get('artifacts',{}).get('observations'):
        row=json.loads(Path(opened['artifacts']['observations']).read_text('utf8'))[0]
        r.case('production.backup-is-editable-document',row['ready'] and not row['modified'] and not row['error'])
        for key in ('scene','dxf'):r.compare('production.reopened-backup.'+key,Path(row[key]),Path(rows[2][key]))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);old=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    r.data.update(suite='qt-save-protection',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=old,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},limitations=[
        'Hidden production actions do not establish native file-dialog or desktop acceptance.',
        'Content checks detect changed files and Qt writers use a shared lock; an unrelated program ignoring the lock can still race the final check and atomic replacement.',
        'Target/backup are separate filesystem names; hardware power loss is not simulated.'])
    original=q.DEFAULT_MODELS[0];original_hash=q.common.digest_file(original)
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        r.case('runtime-manifest',True)
        qt_save_confirmation.run(r,runtime,out/'confirmation',original)
        r.data['core']=q.run_child(r,'core',runtime/'qt_save_protection_tests.exe',[original],out/'core',timeout=180,platform='windows')
        production(r,runtime,out/'production',original)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==old)
        r.case('original-model-protected',q.common.digest_file(original)==original_hash)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('native-desktop-and-file-picker',status='not_run',reason='Only hidden Qt objects and private files; no desktop automation.')
        r.case('power-loss',status='not_run',reason='Process death and filesystem errors are exercised; no hardware power-loss test.')
        e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
