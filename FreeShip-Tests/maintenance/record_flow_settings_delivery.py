"""Read-only provenance audit of a locally installed flow/settings migration.

Writes one receipt from completed reports; it does not rerun or manufacture QA.
"""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('core','exchange','startup'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/flow-settings-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-flow-settings-20261103').resolve(strict=True)
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
    data={};reports={}
    for key in ('core','exchange','startup'):
        path=getattr(a,key);report=read(path);data[key]=report
        assert report['counts']['fail']==report['counts']['error']==0,key
        if key=='startup':
            for case in ('installed-source-and-file-manifest','installed-files-unchanged'):
                assert any(c['id']==case and c['status']=='pass' for c in report['cases'])
        else:assert report['qt_sources_sha256']==sources
        for name,h in report['test_sources_sha256'].items():assert digest(ROOT/name)==h,name
        reports[key]=dict(path=str(path.resolve()),sha256=digest(path),counts=report['counts'])
    core,exchange,startup=(data[k] for k in ('core','exchange','startup'))
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
              'native_analysis_edit_probe.exe','native_flowline_probe.exe','qt_document_codec_tests.exe',
              'qt_exchange_tests.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    for case in ('flow.edge-fraction-controls-next-tangent','flow.cache.restart','analysis.field-metadata-provenance',
                 'analysis.forbid-old-hydro_settings','analysis.forbid-old-performance_settings','analysis.publication-rollback'):
        assert any(c['id']==case and c['status']=='pass' for c in core['cases']),case
    assert {c['command'][1] for c in core['analysis_production_runs']}=={'--verify-hydro','--verify-performance'}
    for child in core['analysis_production_runs']:
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    assert Path(exchange['runtime_path']).resolve()==installed and exchange['runtime_sha256']==manifest['binaries_sha256']
    assert Path(startup['installed']).resolve()==installed
    for child in [*[m['production'] for m in exchange['models']],*startup['production_runs']]:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    receipt=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Native surface flow tracing and persistent hydro/performance settings; professional calculation/default extraction remains in the analysis adapter.',
        installed=str(installed),previous_installation=str(backup),manifest_sha256=digest(installed/'build-manifest.json'),
        verified_package_files=len(files),legacy_source_sha256=legacy,binaries_sha256={name:digest(installed/name) for name in binaries},reports=reports,
        combined_counts={key:sum(row['counts'][key] for row in reports.values()) for key in ('pass','fail','review','not_run','error')},
        document_seconds=core['elapsed_seconds'],phase_seconds=core['phase_seconds'],
        notes=['Original flow helper leaves its edge parameter uninitialized. Isolated reference explicitly assigns mub; independent varying-tangent fixture checks the intended fraction.',
               'All 15 persisted calculator input sets compare against the old complete-payload setter. Partial patches independently preserve omitted bytes.',
               'Four flow precision levels, edits/history/roundtrip/cache/error rollback retain exact reference comparisons; lines-plan and DXF checks remain exact.',
               'Hidden production Qt callbacks cover settings saving, import/export and startup. Physical desktop acceptance is separate.',
               'Reusable tests stay in FreeShip-Tests; builds and reports are stored on C.'],
        maintenance_sources_sha256={name:digest(ROOT/'maintenance'/name) for name in ('record_flow_settings_delivery.py','record_topology_delivery.py','promote_runtime.ps1')},
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['combined_counts'],verified_package_files=len(files)),ensure_ascii=False))
if __name__=='__main__':main()
