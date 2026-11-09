"""Read-only installation audit plus a receipt; not a product regression test."""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('installed-report','exchange-report','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();installed=q.QT/'dist';path=installed/'build-manifest.json';manifest=read(path)
    source=q.current_sources(q.QT)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==source
    q.validate_test_manifest(manifest)
    for name,h in manifest['binaries_sha256'].items():
        target=(installed/name).resolve()
        assert installed.resolve() in target.parents and digest(target)==h, name
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==read(q.QT/'docs/migration/BASELINE.json')['legacy_source_sha256']
    reports={}
    for label,file in [('installed_polycad',a.installed_report),('existing_exchange',a.exchange_report)]:
        report=read(file)
        assert report['qt_sources_sha256']==source and report['source_sha256']==legacy
        assert report['counts']['fail']==report['counts']['error']==0
        for name,h in report['test_sources_sha256'].items():assert digest(ROOT/name)==h,name
        # Documentation was packaged after the first existing-format run; match
        # executable code, not older documentation hashes, to the final package.
        for name,h in report['runtime_sha256'].items():
            if Path(name).suffix.lower() in ('.exe','.dll'):
                assert digest(installed/name)==h,name
        if label=='installed_polycad':assert Path(report['runtime']).resolve()==installed.resolve()
        reports[label]=dict(path=str(file.resolve()),sha256=digest(file),counts=report['counts'])
    storage=ROOT/'maintenance/storage-cleanup-verified-20261104.json'
    receipt=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Polycad control-grid import, Qt actions, native editing, failure preservation and FBM roundtrip. Not original NURBS surface reconstruction.',
        installed=str(installed),previous_installation=str(q.QT/'build/dist-before-polycad-20261104'),
        package_manifest_sha256=digest(path),verified_files=len(manifest['binaries_sha256']),
        executable_sha256={n:h for n,h in manifest['binaries_sha256'].items() if Path(n).suffix.lower()=='.exe'},
        legacy_source_sha256=legacy,reports=reports,
        limits=['Independent synthetic grids and source-derived grammar; no external Polycad installation or real Polycad export supplied.',
                'Hidden Windows callbacks and screenshots do not certify full original UI parity or physical desktop interaction.',
                'Other import/export formats and remaining migration work are tracked separately.'],
        storage_receipt=dict(path=str(storage),sha256=digest(storage),scope='Earlier storage maintenance, before this product update.'),
        retained_sources=['qt_polycad.py','qt_exchange.py','run_tests.py','test_artifacts.py',
                          'maintenance/record_polycad_delivery.py','maintenance/prune_polycad_intermediates.ps1'])
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output),reports=reports),ensure_ascii=False))


if __name__=='__main__':main()
