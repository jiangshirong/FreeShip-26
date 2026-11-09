"""Read-only installation/report audit; delivery bookkeeping, not a test suite."""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--report',type=Path,action='append',required=True)
    p.add_argument('--installed',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    installed=(q.QT/'dist').resolve(strict=True);manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    sources=q.current_sources(q.QT);assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    files={f.relative_to(installed).as_posix():digest(f) for f in installed.rglob('*') if f.is_file() and f.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256']
    baseline=read(q.QT/'docs/migration/BASELINE.json');legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']==baseline['legacy_source_sha256']
    assert digest(q.common.DEFAULT_SOURCE/'FreeShip.exe')==baseline['legacy_executable_sha256']
    assert digest(q.PILOT/'dist/FreeShipQtPilot.exe')==baseline['accepted_pilot_executable_sha256']
    reports=[]
    def check_executed(value):
        if isinstance(value,dict):
            if 'executed_path' in value and 'executable_sha256' in value:
                exe=Path(value['executed_path']);assert digest(exe)==value['executable_sha256']
                # Qt and native probes must match the installed file, not only
                # the candidate's current manifest after unrelated probe builds.
                if (installed/exe.name).is_file():assert digest(installed/exe.name)==value['executable_sha256']
            for child in value.values():check_executed(child)
        elif isinstance(value,list):
            for child in value:check_executed(child)
    for path in [*a.report,a.installed]:
        path=path.resolve(strict=True);data=read(path);assert data['counts']['fail']==data['counts']['error']==0
        check_executed(data)
        if 'qt_sources_sha256' in data:assert data['qt_sources_sha256']==sources
        for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
        runtime=Path(data['runtime']).resolve(strict=True);tested=read(runtime/'build-manifest.json')
        assert {n.replace('\\','/'):h for n,h in tested['source_sha256'].items()}==sources
        for n,h in tested['binaries_sha256'].items():
            if Path(n).suffix.lower() in ('.exe','.dll'):assert digest(runtime/n)==h==digest(installed/n),n
        if path==a.installed.resolve():
            assert runtime==installed and Path(data['production']['executed_path']).resolve()==installed/'FreeShipQt.exe'
            assert data['production']['executable_sha256']==digest(installed/'FreeShipQt.exe')
        reports.append(dict(path=str(path),sha256=digest(path),counts=data['counts']))
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),installed=str(installed),
        previous_package=str(q.QT/'build/dist-before-underwater-20261105'),verified_files=len(files),reports=reports,
        source_sha256=sources,manifest_sha256=digest(installed/'build-manifest.json'),
        counts={k:sum(r['counts'][k] for r in reports[:-1]) for k in ('pass','fail','review','not_run','error')},installed_route_counts=reports[-1]['counts'],
        limits=['Hidden event/render checks only; physical desktop input, focus and multi-monitor acceptance remain pending.'],
        free_bytes_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},audit_script_sha256=digest(Path(__file__)))
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(counts=result['counts'],verified_files=len(files),receipt=str(a.output)),ensure_ascii=False))
if __name__=='__main__':main()
