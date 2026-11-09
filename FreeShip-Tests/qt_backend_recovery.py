"""Resume the current editor after a real document-service exit.

Owned QProcess failures cover lost edit/save replies and damaged, older, missing
or leased checkpoints. Hidden production actions cover in-window resumption,
existing recovery-menu selection, continued editing, undo and saving. Source
files and user settings are isolated; no desktop automation is used.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as e
from qt_recovery import capsule

FILES=('qt_backend_recovery.py','qt_recovery.py','qt/recovery_tests.cpp','qt_migration.py','qt_editing.py','run_tests.py')


def read(path):return json.loads(Path(path).read_text('utf8'))


def production(r,runtime,out,model):
    out.mkdir();original=model.read_bytes()
    for name,menu in [('current-action',False),('recovery-menu',True),('clean-restart',False),('new-document',False)]:
        folder=out/name;folder.mkdir();target=folder/'恢复后保存-Женя.fbm';second=folder/'继续修改.fbm'
        new=name=='new-document';clean=name=='clean-restart'
        first=dict(kind='new_hull',fields=dict(length=8,beam=3,draft=1,rows=4,columns=5,units=0)) if new else dict(kind='startup',path=str(model))
        commands=[first]
        if not clean:commands.append(dict(kind='move'))
        interrupted=len(commands)-1
        commands.extend([dict(kind='backend_exit'),dict(kind='recover',recovery_decision='restore') if menu else dict(kind='resume_current'),dict(kind='save',path=str(target)),dict(kind='move'),dict(kind='undo'),dict(kind='save',path=str(second))])
        request=folder/'request.json';request.write_text(json.dumps(dict(history_file=str(folder/'history.json'),recovery_root=str(folder/'recovery'),commands=commands)),encoding='utf8')
        child=q.run_child(r,name,runtime/'FreeShipQt.exe',['--verify-startup',request],folder/'workflow',timeout=150,platform='windows')
        r.data.setdefault('production_runs',[]).append(child)
        if child['status'] in ('fail','error'):raise RuntimeError(name+': production workflow failed')
        rows=read(child['artifacts']['observations'])
        r.case(name+'.every-command-observed',len(rows)==len(commands))
        before=rows[interrupted];dead,resumed,saved,edited,undone,again=rows[interrupted+1:]
        r.case(name+'.actual-backend-error',dead['error'] and not dead['ready'] and dead['can_resume_current'])
        r.case(name+'.restored-through-action',resumed['ready'] and not resumed['error'] and not resumed['can_resume_current'] and resumed['modified']==(not clean))
        r.case(name+'.fresh-undo-history',not resumed['can_undo'] and not resumed['can_redo'])
        r.case(name+'.save-completed',saved['ready'] and not saved['modified'] and not saved['error'] and target.is_file())
        r.case(name+'.continued-edit-and-undo',edited['modified'] and edited['can_undo'] and not undone['modified'] and undone['can_redo'])
        r.case(name+'.second-save',not again['error'] and not again['modified'] and second.is_file())
        if clean:r.case(name+'.source-retained',Path(resumed['source']).resolve()==model.resolve())
        else:r.case(name+'.requires-save-as',not resumed['source'] and (bool(resumed['recovery_origin']) if not new else not resumed['recovery_origin']))
        for artifact in ('scene','dxf','surface','sections'):
            r.compare(name+'.resume-'+artifact,before[artifact],resumed[artifact])
            r.compare(name+'.new-undo-'+artifact,resumed[artifact],undone[artifact])
        r.case(name+'.source-untouched',model.read_bytes()==original)
        # New/edited previews can be refreshed by the production save callback;
        # compare semantic geometry across saves independently of JPEG bytes.
        for phase in (resumed,undone,again):
            r.case(name+'.geometry-'+phase['operation'],q.geometry(read(phase['model']))==q.geometry(read(before['model'])))
        r.case(name+'.recovery-cleaned-after-save',not list((folder/'recovery').glob('*.fsr')))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use private output outside source trees')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.perf_counter();sources=q.current_sources(q.QT)
    r.data.update(suite='qt-backend-recovery',runtime=str(runtime),test_sources_sha256={f:q.common.digest_file(q.ROOT/f) for f in FILES},limitations=['Only durable document commits are recovered; undo history, unsubmitted drag/form input and full application restart are separate.','Hidden callbacks do not certify desktop input, DPI or hardware power-loss behavior.'])
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);model=out/'private-model.fbm';shutil.copy2(q.DEFAULT_MODELS[0],model)
        request=out/'backend.json';request.write_text(json.dumps(dict(mode='backend',model=str(model))),encoding='utf8')
        child=q.run_child(r,'backend',runtime/'qt_recovery_tests.exe',[request],out/'backend',timeout=240,platform='windows')
        if child['status']=='error':raise RuntimeError(child.get('error','Backend recovery probe failed'))
        r.case('backend.all-fault-scenarios',len(child['artifacts'])==13)
        for item in child['artifacts']:
            name=item['name'];expected=Path(item['expected']).read_bytes();restored=Path(item['restored']).read_bytes()
            r.case(name+'.independent-restored-bytes',expected==restored)
            if item['capsule']:
                header,payload=capsule(Path(item['capsule']))
                r.case(name+'.independent-capsule-parser',payload==expected and header['revision']>=1)
        production(r,runtime,out/'production',model)
        r.case('sources-unchanged',q.current_sources(q.QT)==sources)
        r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.case('desktop-and-power-loss',status='not_run',reason='No desktop input or hardware power interruption.');r.data['elapsed_s']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__=='__main__':raise SystemExit(main())
