"""Audit installed import delivery and its completed regressions; no mutation of the app."""
import argparse
import json
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--installed-report',type=Path,required=True)
    p.add_argument('--regression-report',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    installed=q.QT/'dist';manifestPath=installed/'build-manifest.json';manifest=read(manifestPath)
    sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert sources=={k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}
    assert legacy==read(q.QT/'docs/migration/BASELINE.json')['legacy_source_sha256']
    q.validate_test_manifest(manifest)
    for name,h in manifest['binaries_sha256'].items():
        target=(installed/name).resolve();assert installed.resolve() in target.parents and digest(target)==h,name
    reports={}
    for file in [a.installed_report,*a.regression_report]:
        data=read(file);assert data['counts']['fail']==data['counts']['error']==0
        for name,h in data['test_sources_sha256'].items():assert digest(ROOT/name)==h,name
        tested=Path(data.get('runtime',data.get('runtime_path'))).resolve();testedManifest=read(tested/'build-manifest.json')
        assert {k.replace('\\','/'):v for k,v in testedManifest['source_sha256'].items()}==sources
        if 'qt_sources_sha256' in data:assert data['qt_sources_sha256']==sources
        if 'source_sha256' in data:assert data['source_sha256']==legacy
        code={n:h for n,h in testedManifest['binaries_sha256'].items() if Path(n).suffix.lower() in ('.exe','.dll')}
        for name,h in code.items():
            assert digest(tested/name)==h and digest(installed/name)==h,name
            if 'runtime_sha256' in data:assert data['runtime_sha256'][name]==h,name
        if file==a.installed_report:assert tested==installed.resolve() and data['suite']=='qt-curve-import'
        reports[data['suite']]=dict(path=str(file.resolve()),sha256=digest(file),counts=data['counts'],tested_runtime=str(tested))
    assert set(reports)=={'qt-curve-import','qt-exchange','qt-polycad','qt-publication'}
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Hull, Carene, chine-text and surface-text: native fitting, layers, creases, markers, control curves, stations, Qt actions and saved-file roundtrip.',
        installed=str(installed),previous_installation=str(q.QT/'build/dist-before-curve-import-20261104'),
        package_manifest_sha256=digest(manifestPath),verified_files=len(manifest['binaries_sha256']),
        executable_sha256={n:h for n,h in manifest['binaries_sha256'].items() if Path(n).suffix.lower()=='.exe'},
        legacy_source_sha256=legacy,reports=reports,
        counts={k:sum(item['counts'][k] for item in reports.values()) for k in ('pass','fail','review','not_run','error')},
        limits=['Fitted nets, not arbitrary lossless surface exchange; original input curves are retained as markers.',
                'Synthetic source-compatible files and independent decimal fitting oracle; no real external Hull/Carene samples supplied.',
                'Hidden Windows Qt callbacks and screenshots; full visual parity and physical desktop acceptance remain separate.'],
        retained_sources=['qt_curve_import.py','qt/exchange_tests.cpp','run_tests.py',
                          'maintenance/record_curve_import_delivery.py','maintenance/prune_curve_import_intermediates.ps1'])
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=result['counts']),ensure_ascii=False))


if __name__=='__main__':main()
