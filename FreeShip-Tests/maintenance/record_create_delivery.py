"""Audit native document creation delivery against complete, current evidence.

Read-only audit plus one receipt. Does not rerun tests or establish desktop QA.
"""
import argparse,json,shutil,statistics
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('core','exchange','startup'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/create-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-create-20261103').resolve(strict=True)
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path)
    sources=q.current_sources(q.QT);q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    files={}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path!=manifest_path:files[path.relative_to(installed).as_posix()]=digest(path)
    assert files=={k.replace('\\','/'):v for k,v in manifest['binaries_sha256'].items()}
    assert manifest['package_script_sha256']==digest(q.QT/'package.ps1')
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge=read(installed/'bridge/report.json')
    assert bridge['source_sha256']==legacy and bridge['adapter_sha256']==q.bridge_builder(q.QT).adapter_hashes()
    assert digest(installed/'bridge/FreeShipGeometryBridge.exe')==bridge['build']['executable_sha256']
    reports={};data={}
    for name in ('core','exchange','startup'):
        path=getattr(a,name);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        if name=='startup':
            # Startup stores per-process executable hashes plus an installed
            # manifest check, rather than duplicating that manifest at top level.
            for case in ('installed-source-and-file-manifest','installed-files-unchanged'):
                assert any(c['id']==case and c['status']=='pass' for c in report['cases']),case
        else:assert report['qt_sources_sha256']==sources,name
        for file,h in report['test_sources_sha256'].items():assert digest(ROOT/file)==h,file
        reports[name]=dict(path=str(path.resolve()),sha256=digest(path),counts=report['counts'])
    core=data['core'];exchange=data['exchange'];startup=data['startup']
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
              'native_document_create_probe.exe','qt_document_codec_tests.exe',
              'qt_exchange_tests.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    assert Path(exchange['runtime_path']).resolve()==installed
    assert exchange['runtime_sha256']==manifest['binaries_sha256']
    assert Path(startup['installed']).resolve()==installed
    flags=set()
    for child in [*[m['production'] for m in exchange['models']],*startup['production_runs']]:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
        flags.add(child['command'][1])
    assert flags=={'--verify-exchange','--verify-startup'}
    codec=read(Path(core['codec_report']));assert codec['status']=='pass' and codec['supplied_files']==80
    reports['core'].update(elapsed_seconds=core['elapsed_seconds'],phase_seconds=core['phase_seconds'])
    times=[row['creation_ms'] for row in core['native_create_timings']]
    receipt=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Native FBM document loading/construction, new hull, exchange import and version transitions; old analysis hydration remains.',
        installed=str(installed),previous_installation=str(backup),manifest_sha256=digest(manifest_path),
        verified_package_files=len(files),legacy_source_sha256=legacy,
        binaries_sha256={name:digest(installed/name) for name in binaries},reports=reports,
        combined_counts={key:sum(row['counts'][key] for row in reports.values()) for key in ('pass','fail','review','not_run','error')},
        native_creation_ms=dict(samples=len(times),minimum=min(times),median=statistics.median(times),maximum=max(times)),
        notes=['All 45 FBM versions have independent format fixtures; early 0..6 transport promotion does not alter unchanged saved files.',
               'Native/Plus comparison retains exact coordinates, topology, surface, lines-plan scene, sections and DXF entities.',
               'Creation timings exclude preference context, old analysis hydration and snapshot publication; not total UI open time.',
               'Old analysis object loading, flow tracing, analysis settings, hydrostatics/stability, resistance/propulsion, plates and IGES preparation remain.',
               'Installed Qt callbacks run privately; physical desktop acceptance is not claimed. Source stays in the project; reports/builds are on C.'],
        maintenance_sources_sha256={name:digest(ROOT/'maintenance'/name) for name in ('record_create_delivery.py','record_topology_delivery.py','promote_runtime.ps1')},
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['combined_counts'],verified_package_files=len(files)),ensure_ascii=False))


if __name__=='__main__':main()
