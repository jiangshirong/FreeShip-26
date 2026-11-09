"""Audit the chart delivery and existing reports; does not run or replace tests."""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);a=p.parse_args();root=a.root.resolve(strict=True)
    installed=q.QT/'dist';manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest);sources=q.current_sources(q.QT)
    assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    files={p.relative_to(installed).as_posix():digest(p) for p in installed.rglob('*') if p.is_file() and p.name!='build-manifest.json'};assert files==manifest['binaries_sha256']
    baseline=read(q.QT/'docs/migration/BASELINE.json');legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==baseline['legacy_source_sha256']==manifest['legacy_source_sha256']
    assert digest(q.common.DEFAULT_SOURCE/'FreeShip.exe')==baseline['legacy_executable_sha256']
    assert digest(q.PILOT/'dist/FreeShipQtPilot.exe')==baseline['accepted_pilot_executable_sha256']
    previous=q.QT/'build/dist-before-calculation-charts-20261109';old=read(previous/'build-manifest.json')
    changed=sorted(n for n in set(sources)|set(old['source_sha256']) if sources.get(n)!=old['source_sha256'].get(n))
    allowed={'CMakeLists.txt','build.ps1','src/app/calculation_charts.h','src/app/calculation_charts-origin.json',
             'src/app/calculation_plot.h','src/app/calculation_plot.cpp','src/app/performance_dialog.cpp',
             'src/app/performance_workflow.cpp','src/document/propulsion.cpp'}
    assert set(changed)<=allowed,changed
    def executed(obj):
        if isinstance(obj,dict):
            if 'executed_path' in obj:
                assert digest(Path(obj['executed_path']))==obj['executable_sha256']
                assert digest(installed/Path(obj['executed_path']).name)==obj['executable_sha256']
            for value in obj.values():executed(value)
        elif isinstance(obj,list):
            for value in obj:executed(value)
    reports=[]
    for name in ('final-02/qt-calculation-charts/report.json','layout-regression/report.json','propulsion-protocol/report.json','installed/qt-calculation-charts/report.json'):
        path=root/name;data=read(path);assert data['counts']['fail']==data['counts']['error']==0,name
        if name!='propulsion-protocol/report.json':
            assert data['qt_sources_sha256']==sources
            for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
            executed(data)
        for ref in data.get('references',{}).values():
            assert digest(Path(ref['result']))==ref['sha256'];assert digest(Path(ref['request']))==ref['request_sha256']
        if name.startswith('installed/'):assert Path(data['runtime']).resolve()==installed.resolve()
        reports.append(dict(path=str(path),sha256=digest(path),counts=data['counts']))
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),installed=str(installed),previous_package=str(previous),
        files=len(files),changed_source_files=changed,manifest_sha256=digest(installed/'build-manifest.json'),reports=reports,
        counts={k:sum(r['counts'][k] for r in reports[:-1]) for k in ('pass','fail','review','not_run','error')},installed_counts=reports[-1]['counts'],
        source_sha256=sources,protocol_test_sha256=digest(ROOT/'qt_native_propulsion.py'),
        output_bytes=sum(p.stat().st_size for p in root.rglob('*') if p.is_file()),free_bytes={d:shutil.disk_usage(d+':/').free for d in ('C','D')},
        audit_script_sha256=digest(Path(__file__)),
        limits=['Hidden Qt events and screenshots are not desktop acceptance.','Original CALCPROP remains blocked; Task1-3 use protocol fixtures.','Million-point first rendering remains slow; repeated painting is cached.'])
    target=ROOT/'maintenance/calculation-charts-installed-20261109.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(counts=result['counts'],installed_counts=result['installed_counts'],receipt=str(target)),ensure_ascii=False))
if __name__=='__main__':main()
