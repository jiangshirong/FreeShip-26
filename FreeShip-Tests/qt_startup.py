"""Isolated multi-process startup/recent-file/save-as regression; no desktop input."""
from pathlib import Path
import argparse, json, shutil
import qt_migration as q

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--installed',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve()
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use a private report directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-startup'
    old=q.common.fingerprint(a.source)[0];qt_before=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in (a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe',q.DEFAULT_MODELS[0])}
    sources=('qt_startup.py','qt_migration.py','run_tests.py');r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in sources}
    installed_before={}
    try:
        if a.installed:
            runtime=a.installed.resolve(strict=True);manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'))
            q.validate_test_manifest(manifest)
            if {n.replace('\\','/'):v for n,v in manifest['source_sha256'].items()}!=qt_before:raise ValueError('Installed sources are stale')
            installed_before={str(x.relative_to(runtime)).replace('\\','/'):q.common.digest_file(x) for x in runtime.rglob('*') if x.is_file()}
            if {n:v for n,v in installed_before.items() if n!='build-manifest.json'}!=manifest['binaries_sha256']:raise ValueError('Installed binary inventory differs')
            r.data['installed']=str(runtime);r.case('installed-source-and-file-manifest',True)
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r)
        history=out/'settings/recent-files.json';model=out/'模型-Женя.fbm';shutil.copy2(q.DEFAULT_MODELS[0],model)
        second=out/'另一艘船.fbm';shutil.copy2(model,second);broken=out/'broken.fbm';broken.write_bytes(b'not a model')
        missing=out/'missing.fbm';saved=out/'另存为-Новый.fbm'
        def run(name,commands,state=history):
            request=out/(name+'.json');request.write_text(json.dumps({'history_file':str(state),'commands':commands},ensure_ascii=False),encoding='utf8')
            result=q.run_child(r,name,runtime/'FreeShipQt.exe',['--verify-startup',request],out/name,platform='offscreen')
            r.data.setdefault('production_runs',[]).append(result)
            if result['status']=='error':raise ValueError('Production startup probe failed')
            if any(c.get('name')=='recent_action_unavailable' for c in result.get('cases',[])):return []
            data=json.loads(Path(result['artifacts']['observations']).read_text('utf8'));r.case(name+'.observed-every-command',len(data)==len(commands));return data
        def command(kind,path=''):return {'kind':kind,'path':str(path)}
        def same_path(actual,expected):return Path(actual).resolve()==Path(expected).resolve() if actual else False
        first=run('first-launch',[command('startup')])[0];r.case('first-launch-stays-empty',not first['ready'] and not first['last'] and not history.exists())
        opened=run('open-success',[command('startup',model)])[0];r.case('successful-open-remembers-original-user-path',same_path(opened['source'],model) and same_path(opened['last'],model))
        restored=run('restart',[command('startup')])[0];r.case('new-process-restores-last-model',restored['ready'] and same_path(restored['source'],model) and not restored['modified'])
        switched=run('explicit-file-wins',[command('startup',second)])[0];r.case('explicit-launch-overrides-last-file',same_path(switched['source'],second))
        rows=run('failed-open-preserves-history',[command('startup'),command('open',broken),command('open',missing)])
        for i,row in enumerate(rows[1:],1):r.case('failed-open-'+str(i)+'.preserves-live-model-and-history',bool(row['error']) and row['ready'] and same_path(row['source'],second) and same_path(row['last'],second))
        rows=run('save-as',[command('startup'),command('save',saved)]);r.case('save-as-remembers-committed-target',saved.is_file() and same_path(rows[-1]['last'],saved) and not rows[-1]['modified'])
        row=run('restart-save-as',[command('startup')])[0];r.case('save-as-restored-in-new-process',same_path(row['source'],saved))
        saved.unlink();row=run('deleted-last-file',[command('startup')])[0]
        r.case('deleted-file-starts-usable-empty-window',not row['ready'] and not row['last'] and bool(row['status_text']))
        row=run('deleted-file-second-start',[command('startup')])[0];r.case('missing-file-does-not-retry-forever',not row['last'] and row['operation']=='idle')
        run('prepare-corruption',[command('startup',second)]);second.write_bytes(b'changed after successful open')
        row=run('corrupt-last-file',[command('startup')])[0];r.case('corrupt-automatic-restore-is-nonmodal-and-clears-retry',bool(row['error']) and not row['last'] and not row['ready'])
        row=run('recover-via-open',[command('startup'),command('open',model)])[-1];r.case('open-recovers-after-restore-failure',row['ready'] and same_path(row['last'],model))
        rows=run('recent-action',[command('startup'),command('recent',model)])
        if rows:r.case('recent-action-uses-production-open',rows[-1]['ready'] and same_path(rows[-1]['source'],model))
        blocked=out/'settings-is-a-file';blocked.write_text('retain me','utf8')
        row=run('unwritable-history',[command('startup',model)],blocked/'recent.json')[0]
        r.case('settings-write-failure-does-not-fail-model-open',row['ready'] and blocked.read_text('utf8')=='retain me')
        corrupt=out/'corrupt-settings.json';corrupt.write_text('{broken','utf8')
        rows=run('corrupt-settings',[command('startup'),command('open',model)],corrupt)
        r.case('bad-settings-start-empty-and-recover-on-success',not rows[0]['ready'] and same_path(rows[-1]['last'],model))
        many=[]
        for i in range(11):
            path=out/f'recent-{i}.fbm';shutil.copy2(model,path);many.append(path)
        rows=run('recent-limit',[command('open',x) for x in many]+[command('open',many[-1])])
        r.case('recent-list-limited-and-deduplicated',len(rows[-1]['recent'])==10 and len(set(rows[-1]['recent']))==10 and same_path(rows[-1]['recent'][0],many[-1]))
        r.case('input-model-unchanged',q.common.digest_file(model)==q.common.digest_file(q.DEFAULT_MODELS[0]))
        r.case('native-desktop-startup',status='not_run',reason='Multiple real application processes used hidden windows and private settings; normal desktop startup remains for user confirmation.')
    except Exception as e:r.case('execution',status='error',reason=f'{type(e).__name__}: {e}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==old);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==qt_before)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,digest in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        if installed_before:r.case('installed-files-unchanged',installed_before=={str(x.relative_to(runtime)).replace('\\','/'):q.common.digest_file(x) for x in runtime.rglob('*') if x.is_file()})
        r.finish();(out/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
