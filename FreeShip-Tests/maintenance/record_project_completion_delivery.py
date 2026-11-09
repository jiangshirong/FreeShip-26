"""Audit the installed package and link exact reports; does not execute tests."""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);a=p.parse_args();root=a.root.resolve(strict=True)
    installed=(q.QT/'dist').resolve(strict=True);manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    sources=q.current_sources(q.QT);assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    files={p.relative_to(installed).as_posix():digest(p) for p in installed.rglob('*') if p.is_file() and p.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256']
    baseline=read(q.QT/'docs/migration/BASELINE.json');legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==baseline['legacy_source_sha256']==manifest['legacy_source_sha256']
    assert digest(q.common.DEFAULT_SOURCE/'FreeShip.exe')==baseline['legacy_executable_sha256']
    assert digest(q.PILOT/'dist/FreeShipQtPilot.exe')==baseline['accepted_pilot_executable_sha256']
    paths=['final-02/qt-project-completion/report.json','final-02/qt-dialog-layout/report.json','regression/qt-project-hydro/report.json','regression/qt-hydrodynamics/report.json','propulsion-parity/report.json','installed/report.json']
    reports=[]
    def executed(obj):
        if isinstance(obj,dict):
            if 'executed_path' in obj:
                assert digest(Path(obj['executed_path']))==obj['executable_sha256']
                assert digest(installed/Path(obj['executed_path']).name)==obj['executable_sha256']
            for value in obj.values():executed(value)
        elif isinstance(obj,list):
            for value in obj:executed(value)
    for name in paths:
        path=root/name;data=read(path);assert data['counts']['fail']==data['counts']['error']==0,name
        for n,h in data.get('test_sources_sha256',{}).items():assert digest(ROOT/n)==h,n
        observed=data.get('qt_sources_sha256',{});changes=[n for n in set(observed)|set(sources) if observed.get(n)!=sources.get(n)] if observed else []
        if name.startswith('regression/'):
            # Only the last UI polish changed since these runs: author input
            # length validation and scrolling to the converted viscosity.
            assert set(changes)<= {'src/app/project_dialog.cpp','src/app/performance_dialog.cpp'},changes
            scope='Backend regression on unchanged computation sources; earlier UI checks are supplemental.'
        else:
            assert not changes,changes
            if not name.startswith('propulsion-parity/'):executed(data)
            scope='Current installed source and UI' if not name.startswith('propulsion-parity/') else 'Original-solver parity; unchanged calculation sources'
        if name=='installed/report.json':assert Path(data['runtime']).resolve()==installed
        reports.append(dict(path=str(path),sha256=digest(path),counts=data['counts'],scope=scope,subsequent_ui_source_changes=changes))
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),installed=str(installed),
        previous_package=str(q.QT/'build/dist-before-project-completion-20261109'),verified_files=len(files),reports=reports,
        source_sha256=sources,manifest_sha256=digest(installed/'build-manifest.json'),
        counts={k:sum(report['counts'][k] for report in reports[:-1]) for k in ('pass','fail','review','not_run','error')},installed_route_counts=reports[-1]['counts'],
        free_bytes_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},audit_script_sha256=digest(Path(__file__)),
        limits=['Hidden actions and screenshots only; desktop focus, native dialogs and multi-monitor acceptance remain pending.','Original water table and external solvers are not independently re-certified.'])
    target=ROOT/'maintenance/project-completion-installed-20261109.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(counts=result['counts'],installed_counts=result['installed_route_counts'],receipt=str(target)),ensure_ascii=False))
if __name__=='__main__':main()
