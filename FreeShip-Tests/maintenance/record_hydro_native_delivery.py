"""Verify the installed native hydrostatics package against completed reports.

This records existing evidence. It does not execute tests or turn review/not_run
into pass. Runtime promotion is a separate reversible maintenance operation.
"""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('core','exchange','startup'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/hydro-native-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    previous=(q.QT/'build/dist-before-hydro-native-20261103').resolve(strict=True)
    manifest=read(installed/'build-manifest.json');sources=q.current_sources(q.QT);q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    files={}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path.name!='build-manifest.json':files[path.relative_to(installed).as_posix()]=digest(path)
    assert files=={k.replace('\\','/'):v for k,v in manifest['binaries_sha256'].items()}
    assert manifest['package_script_sha256']==digest(q.QT/'package.ps1')
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge=read(installed/'bridge/report.json')
    assert bridge['source_sha256']==legacy and bridge['adapter_sha256']==q.bridge_builder(q.QT).adapter_hashes()
    assert bridge['build']['executable_sha256']==digest(installed/'bridge/FreeShipGeometryBridge.exe')
    reports={};data={}
    for name in ('core','exchange','startup'):
        path=getattr(a,name);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        if name=='startup':
            for case in ('installed-source-and-file-manifest','installed-files-unchanged'):
                assert any(c['id']==case and c['status']=='pass' for c in report['cases'])
        else:assert report['qt_sources_sha256']==sources
        for key,h in report['test_sources_sha256'].items():assert digest(ROOT/key)==h,key
        reports[name]=dict(path=str(path.resolve()),sha256=digest(path),counts=report['counts'])
    core,exchange,startup=(data[k] for k in ('core','exchange','startup'))
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
              'qt_document_codec_tests.exe','qt_exchange_tests.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    required=[f'hydro.{i}.{suffix}' for i in range(4) for suffix in
              ('without-pascal','invalidated','undo','forbid-old-hydrostatics','forbid-old-hydro_defaults','balance-True.native-no-sections')]
    for case in required:assert any(c['id']==case and c['status']=='pass' for c in core['cases']),case
    hydro_runs=[m['production'] for m in core['models'] if 'production' in m]
    assert hydro_runs
    for child in hydro_runs:assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    assert Path(exchange['runtime_path']).resolve()==installed and exchange['runtime_sha256']==manifest['binaries_sha256']
    assert Path(startup['installed']).resolve()==installed
    for child in [*[m['production'] for m in exchange['models']],*startup['production_runs']]:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    receipt=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Native hydrostatics single/table, fixed/free-trim equilibrium, KN/GZ and defaults through existing Qt workflows.',
        installed=str(installed),previous_installation=str(previous),manifest_sha256=digest(installed/'build-manifest.json'),
        verified_package_files=len(files),legacy_source_sha256=legacy,binaries_sha256={name:digest(installed/name) for name in binaries},reports=reports,
        combined_counts={key:sum(row['counts'][key] for row in reports.values()) for key in ('pass','fail','review','not_run','error')},
        document_seconds=core['elapsed_seconds'],phase_seconds=core['phase_seconds'],hydro_timings=core['native_hydro_timings'],
        notes=['Original Single storage, sample density and numerical tolerances retained. Independent box formulas supplement old-core comparisons.',
               'Equilibrium skips unused maximum-section searches; per-document mesh reuse is checked across edits and undo.',
               'Asymmetric section/metacentre data remains explicitly unavailable; regulatory/damage-stability criteria are outside this migration.',
               'Existing hidden Qt callbacks cover all hydro modes, cancellation and four report formats. Physical desktop acceptance is separate.',
               'Other analysis and internal annotation routes still use the legacy adapter. This is not a claim that all migration is complete.',
               'Test code remains in FreeShip-Tests; generated builds and reports are on C.'],
        maintenance_sources_sha256={name:digest(ROOT/'maintenance'/name) for name in ('record_hydro_native_delivery.py','record_topology_delivery.py','promote_runtime.ps1')},
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['combined_counts'],verified_package_files=len(files)),ensure_ascii=False))
if __name__=='__main__':main()
