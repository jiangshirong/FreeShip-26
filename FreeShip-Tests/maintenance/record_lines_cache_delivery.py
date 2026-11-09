"""Read-only installed cache delivery audit, not an installer or cleaner."""
import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','publication','startup'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-lines-cache-20261105').resolve(strict=True)
    sources=q.current_sources(q.QT);manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert sources=={n.replace('\\','/'):v for n,v in manifest['source_sha256'].items()}
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']==read(q.QT/'docs/migration/BASELINE.json')['legacy_source_sha256']
    files={f.relative_to(installed).as_posix():digest(f) for f in installed.rglob('*') if f.is_file() and f.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256'];reports={};timings={}
    def passed(data,name):assert any(c['id']==name and c['status']=='pass' for c in data['cases']),name
    for name in ('cache','publication','startup'):
        path=getattr(a,name);data=read(path);assert data['counts']['fail']==data['counts']['error']==0
        for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
        runtime=Path(data['installed'] if name=='startup' else data['runtime']).resolve(strict=True)
        if name in ('cache','startup'):assert runtime==installed
        tested=read(runtime/'build-manifest.json')
        assert sources=={n.replace('\\','/'):v for n,v in tested['source_sha256'].items()}
        for n,h in tested['binaries_sha256'].items():
            if Path(n).suffix.lower() in ('.exe','.dll'):assert digest(runtime/n)==h==digest(installed/n),n
        if name=='cache':
            assert data['qt_sources_sha256']==sources and data['source_sha256']==legacy
            for case in ('application.cross-process-cache-hit','session.edit-reuses-unaffected-cuts',
                         'session.undo-redo-full-cache','corrupt-full-regenerates','corrupt-cuts-regenerate',
                         'font-metrics-revalidated','original-models-protected','legacy-protected'):passed(data,case)
            timings=dict(application=data['application_timings'],real_edit=data['real_edit'],worker=data['timings'])
        elif name=='publication':
            passed(data,'sources-unchanged-during-test');passed(data,'protected-models-unchanged')
            for i in (1,2,3):
                for kind in ('complete-snapshot-exact','worker-input-exact','scene-exact','dxf-exact'):passed(data,f'model-{i}.{kind}')
        else:
            passed(data,'installed-source-and-file-manifest');passed(data,'installed-files-unchanged')
            for child in data['production_runs']:
                assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
                assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
        reports[name]=dict(path=str(path.resolve()),sha256=digest(path),counts=data['counts'])
    receipt=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Persistent Qt scene cache, exact local section reuse, removal of redundant background render-path compilation.',
        installed=str(installed),previous_installation=str(backup),verified_files=len(files),
        package_manifest_sha256=digest(installed/'build-manifest.json'),legacy_source_sha256=legacy,
        executable_sha256={n:h for n,h in files.items() if Path(n).suffix.lower()=='.exe'},reports=reports,timings=timings,
        counts={k:sum(v['counts'][k] for v in reports.values()) for k in ('pass','fail','review','not_run','error')},
        limits=['Incremental scope is section intersection; subdivision, fill, layout and surface serialization still rebuild.',
                'Worker timings are not whole-window latency; hidden callbacks are not desktop acceptance.'],
        free_bytes_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},audit_script_sha256=digest(Path(__file__)))
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['counts'],verified_files=len(files)),ensure_ascii=False))
if __name__=='__main__':main()
