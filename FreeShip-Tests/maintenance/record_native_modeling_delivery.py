"""Audit the advanced-modeling installation against its actual tested binaries.

Retained one-delivery maintenance tool; not a regression or a deployment action.
Reads reports/package/protected source and writes only the requested receipt.
"""
import argparse
import json
import shutil
import statistics
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core',type=Path,required=True)
    p.add_argument('--installed-report',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/modeling-native-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-modeling-native-20261103').resolve(strict=True)
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path)
    q.validate_test_manifest(manifest);sources=q.current_sources(q.QT)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    files={}
    for file in installed.rglob('*'):
        assert installed in file.resolve().parents
        if file.is_file() and file!=manifest_path:files[file.relative_to(installed).as_posix()]=digest(file)
    assert files=={k.replace('\\','/'):v for k,v in manifest['binaries_sha256'].items()}
    assert manifest['package_script_sha256']==digest(q.QT/'package.ps1')
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge=read(installed/'bridge/report.json')
    assert bridge['source_sha256']==legacy and bridge['adapter_sha256']==q.bridge_builder(q.QT).adapter_hashes()
    assert digest(installed/'bridge/FreeShipGeometryBridge.exe')==bridge['build']['executable_sha256']
    core=read(a.core);assert core['qt_sources_sha256']==sources
    assert core['counts']['fail']==core['counts']['error']==0
    for file,h in core['test_sources_sha256'].items():assert digest(ROOT/file)==h,file
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','qt_document_codec_tests.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    reports={'qt-document':{'path':str(a.core.resolve()),'sha256':digest(a.core),'counts':core['counts'],
        'phase_seconds':core['phase_seconds'],'elapsed_seconds':core['elapsed_seconds'],
        'scope':'Complete isolated suite; relevant binaries match the installation exactly.'}}
    for file in a.installed_report:
        data=read(file);assert data['counts']['fail']==data['counts']['error']==0
        assert Path(data['deployment']).resolve()==installed and data['package_manifest']==manifest
        assert digest(Path(data['workflow_reference'])/'report.json')==data['workflow_reference_sha256']
        reports[file.parent.name]={'path':str(file.resolve()),'sha256':digest(file),'counts':data['counts'],
            'scope':'Actual installed production Qt callbacks, hidden windows.'}
    codec=read(a.core.parent/'codec/report.json');assert codec['status']=='pass' and codec['supplied_files']==80
    timings={}
    for key in ('native_topology_timings','native_modeling_timings','native_transform_timings'):
        values=[v['edit_ms'] for v in core[key]]
        timings[key]={'minimum':min(values),'median':statistics.median(values),'maximum':max(values),'samples':len(values)}
    now=datetime.now(timezone.utc);start=datetime.fromisoformat('2026-11-02T16:43:24+00:00')
    receipt={'recorded_utc':now.isoformat(),
        'scope':'13 additional native commands: 3 transforms, point/edge extrusion, mirror, face merge and 6 point-constraint commands.',
        'installed':str(installed),'previous_installation':str(backup),'manifest_sha256':digest(manifest_path),
        'verified_package_files':len(files),'binaries_sha256':{n:digest(installed/n) for n in binaries},
        'legacy_source_sha256':legacy,'reports':reports,
        'combined_counts':{k:sum(r['counts'][k] for r in reports.values()) for k in ('pass','fail','review','not_run','error')},
        'codec':{'cases':len(codec['cases']),'supplied_files':codec['supplied_files']},
        'candidate_kernel_ms':timings,
        'time_accounting':{'first_observed_utc':start.isoformat(),'elapsed_wall_seconds':(now-start).total_seconds(),
            'core_final_seconds':core['elapsed_seconds'],
            'note':'Focused diagnosis, coding, review, builds and integration also occurred. Wall time minus this final suite is not a measurement of coding time.'},
        'notes':['No new numerical tolerances in full model, scene, DXF or section comparisons.',
            'Live reverse dependency order is captured and restored privately; FBM format is unchanged.',
            'Whole-model scale covers the ten legacy resistance records; eight other calculation records and padding are preserved.',
            'Kernel timings are candidate construction/encoding only, not complete request latency or GUI frame rate.',
            'Remaining modeling tools, other field encoding, format upgrades, four-view mesh and professional analysis still have Pascal dependencies.',
            'Hidden callbacks do not establish physical desktop/visual acceptance. Test sources retained; reports and builds on C.'],
        'maintenance_sources_sha256':{n:digest(ROOT/'maintenance'/n) for n in ('record_native_modeling_delivery.py','record_topology_delivery.py','promote_runtime.ps1')},
        'free_bytes_after':{drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')}}
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'receipt':str(a.output),'counts':receipt['combined_counts'],'verified_package_files':len(files)},ensure_ascii=False))


if __name__=='__main__':main()
