"""Audit an installed background/display/auxiliary migration without changing the app.

Requires a clean full document report and actual installed Qt callback reports.
This is a delivery receipt, not a substitute for regression or desktop testing.
"""
import argparse,json,shutil,statistics
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core',type=Path,required=True)
    p.add_argument('--installed-report',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/auxiliary-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-auxiliary-20261103').resolve(strict=True)
    manifest_file=installed/'build-manifest.json';manifest=read(manifest_file)
    sources=q.current_sources(q.QT);q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    files={}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path!=manifest_file:files[path.relative_to(installed).as_posix()]=digest(path)
    assert files=={k.replace('\\','/'):v for k,v in manifest['binaries_sha256'].items()}
    assert manifest['package_script_sha256']==digest(q.QT/'package.ps1')
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge=read(installed/'bridge/report.json')
    assert bridge['source_sha256']==legacy and bridge['adapter_sha256']==q.bridge_builder(q.QT).adapter_hashes()
    assert digest(installed/'bridge/FreeShipGeometryBridge.exe')==bridge['build']['executable_sha256']
    core=read(a.core);assert core['qt_sources_sha256']==sources and core['counts']['fail']==core['counts']['error']==0
    for file,h in core['test_sources_sha256'].items():assert digest(ROOT/file)==h,file
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','qt_document_codec_tests.exe','native_auxiliary_edit_probe.exe','native_background_edit_probe.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    reports={'qt-document':dict(path=str(a.core.resolve()),sha256=digest(a.core),counts=core['counts'],phase_seconds=core['phase_seconds'],elapsed_seconds=core['elapsed_seconds'],scope=core.get('validation_mode','Complete isolated document suite; tested binaries match installation.'))}
    workflow_flags=set()
    for file in a.installed_report:
        data=read(file);assert data['counts']['fail']==data['counts']['error']==0
        assert Path(data['deployment']).resolve()==installed and data['package_manifest']==manifest
        assert digest(Path(data['workflow_reference'])/'report.json')==data['workflow_reference_sha256']
        for model in data['models']:
            child=model['production'];workflow_flags.add(child['command'][1])
            assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
            assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
        reports[file.parent.name]=dict(path=str(file.resolve()),sha256=digest(file),counts=data['counts'],scope='Actual installed Qt callbacks in private hidden windows.')
    assert workflow_flags=={'--verify-backgrounds','--verify-display','--verify-auxiliary'}
    codec=read(Path(core.get('codec_report',a.core.parent/'codec/report.json')));assert codec['status']=='pass' and codec['supplied_files']==80
    timings={}
    for key in ('native_auxiliary_timings','native_background_timings'):
        values=[row['edit_ms'] for row in core[key]]
        timings[key]=dict(samples=len(values),minimum=min(values),median=statistics.median(values),maximum=max(values))
    receipt=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Native background image edits, display settings and control-curve/flowline definitions, including history/save and existing Qt entry points.',
        installed=str(installed),previous_installation=str(backup),manifest_sha256=digest(manifest_file),verified_package_files=len(files),
        binaries_sha256={name:digest(installed/name) for name in binaries},legacy_source_sha256=legacy,reports=reports,
        combined_counts={k:sum(row['counts'][k] for row in reports.values()) for k in ('pass','fail','review','not_run','error')},
        codec=dict(cases=len(codec['cases']),supplied_files=codec['supplied_files']),candidate_kernel_ms=timings,
        notes=['No relaxed coordinate or export tolerances. Existing JPEG bytes and quality fields are preserved.',
               'The original loader still limits production opening of some pre-1.65 formats; direct native probes cover their layout.',
               'Control curves retain graph ordering and stable IDs; invalid flow traces and failed publication do not commit.',
               'Auxiliary spline/flowline computation, old format conversion/new/import, four-view mesh, analysis parameter editing and professional analysis still have Pascal dependencies.',
               'Hidden callbacks are not physical desktop acceptance. Reusable test source retained; outputs and builds are on C.'],
        validation_mode=core.get('validation_mode','Complete isolated document run'),
        validation_runs=core.get('validation_runs',[]),
        maintenance_sources_sha256={name:digest(ROOT/'maintenance'/name) for name in ('record_auxiliary_delivery.py','record_topology_delivery.py','plane_oracle_replay.py','recheck_document_construction.py','promote_runtime.ps1')},
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['combined_counts'],verified_package_files=len(files)),ensure_ascii=False))


if __name__=='__main__':main()
