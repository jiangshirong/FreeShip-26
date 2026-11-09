"""Record exact tested/installed calculation UI package. Does not run tests."""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True)
    p.add_argument('--candidate-report',default='final-04/qt-calculation-layout/report.json');a=p.parse_args();root=a.root.resolve(strict=True)
    installed=(q.QT/'dist').resolve(strict=True);manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    sources=q.current_sources(q.QT);assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    files={p.relative_to(installed).as_posix():digest(p) for p in installed.rglob('*') if p.is_file() and p.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256']
    baseline=read(q.QT/'docs/migration/BASELINE.json');legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==baseline['legacy_source_sha256']==manifest['legacy_source_sha256']
    assert digest(q.common.DEFAULT_SOURCE/'FreeShip.exe')==baseline['legacy_executable_sha256']
    assert digest(q.PILOT/'dist/FreeShipQtPilot.exe')==baseline['accepted_pilot_executable_sha256']
    previous=q.QT/'build/dist-before-calculation-layout-20261109';old=read(previous/'build-manifest.json')
    changed=sorted(n for n in set(sources)|set(old['source_sha256']) if sources.get(n)!=old['source_sha256'].get(n))
    allowed={'src/app/calculation_pages.h','src/app/calculation_pages-origin.json','src/app/performance_dialog.h','src/app/performance_dialog.cpp','src/app/performance_workflow.cpp'}
    assert set(changed)<=allowed,changed
    for name in ('FreeShipDocumentService.exe','FreeShipGeometryExport.exe','FreeShipGeometryBridge.exe'):
        # The bridge lives in its own subdirectory; all available numerical
        # executables must remain byte-identical across this UI-only change.
        candidates=[p for p in installed.rglob(name) if p.is_file()]
        assert candidates,name
        for path in candidates:assert digest(path)==digest(previous/path.relative_to(installed)),path
    def executed(obj):
        if isinstance(obj,dict):
            if 'executed_path' in obj:
                assert digest(Path(obj['executed_path']))==obj['executable_sha256']
                assert digest(installed/Path(obj['executed_path']).name)==obj['executable_sha256']
            for value in obj.values():executed(value)
        elif isinstance(obj,list):
            for value in obj:executed(value)
    reports=[]
    for name in (a.candidate_report,'installed/qt-calculation-layout/report.json'):
        path=root/name;data=read(path);assert data['counts']['fail']==data['counts']['error']==0,name
        assert data['qt_sources_sha256']==sources
        for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
        executed(data)
        if name.startswith('installed/'):assert Path(data['runtime']).resolve()==installed
        reports.append(dict(path=str(path),sha256=digest(path),counts=data['counts'],elapsed_s=data['elapsed_s']))
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),installed=str(installed),previous_package=str(previous),
        verified_files=len(files),changed_source_files=changed,reports=reports,counts=reports[0]['counts'],installed_counts=reports[1]['counts'],
        source_sha256=sources,manifest_sha256=digest(installed/'build-manifest.json'),audit_script_sha256=digest(Path(__file__)),
        free_bytes_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},
        limits=['Hidden Qt callbacks/screenshots only; desktop input and original chart-detail parity remain pending.',
                'Three CALCPROP tasks produce no numerical output; protocol fixture results do not substitute engineering validation.',
                'Real DBFview executable is unavailable.'])
    target=ROOT/'maintenance/calculation-layout-installed-20261109.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(counts=result['counts'],installed_counts=result['installed_counts'],receipt=str(target)),ensure_ascii=False))
if __name__=='__main__':main()
