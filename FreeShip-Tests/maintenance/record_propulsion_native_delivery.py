"""Verify installed propulsion migration against completed regression evidence.

This maintenance command does not execute tests or promote a runtime. It retains
review/not_run separately and refuses failed, stale or mismatched evidence.
"""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('core','installed-workflow','startup'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/propulsion-native-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    previous=(q.QT/'build/dist-before-propulsion-native-20261103').resolve(strict=True)
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
    for name in ('core','installed_workflow','startup'):
        path=getattr(a,name);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for key,h in report.get('test_sources_sha256',{}).items():assert digest(ROOT/key)==h,key
        reports[name]=dict(path=str(path.resolve()),sha256=digest(path),counts=report['counts'])
    core,workflow,startup=(data[k] for k in ('core','installed_workflow','startup'))
    assert core['qt_sources_sha256']==sources
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
              'FreeShipGeometryExport.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(Path(core['runtime_path'])/name)==digest(installed/name),name
    required=['propulsion.table-provenance','propulsion.report-provenance','propeller4.derived-report',
              'family.11.curves','family.11.report','fixture.match-to-passport','passport.actual-area-ratio',
              'passport.engine-independent-line','passport.thrust-independent-line',
              'solver.cancel-running','solver.cancel-child-exited','solver.forced-parent.child-exited',
              'detached.real-solver','extract.old-revision-rejected']
    required += [f'{prefix}.propeller{i}' for i in range(1,6) for prefix in ('detached','forbid-old')]
    required += [f'strength.{mode}.material-{i}.literal-threshold' for mode in ('normal','weak') for i in range(1,9)]
    for name in required:assert any(c['id']==name and c['status']=='pass' for c in core['cases']),name
    for i in range(1,4):assert any(c['id']==f'propeller{i}.numerical-result' and c['status']=='not_run' for c in core['cases'])
    assert any(c['id']=='fixture.passport.legacy-postprocessing' and c['status']=='review' for c in core['cases'])
    assert Path(workflow['deployment']).resolve()==installed
    assert workflow['workflow_reference_sha256']==digest(a.core)
    assert workflow['package_manifest']==manifest
    for name in ('package-current-source-test-and-binary-manifest','actual-deployment-files-unchanged',
                 'model-1.deployment.performance-equals-tested-runtime','model-1.deployment.performance-csv-equals-tested-runtime'):
        assert any(c['id']==name and c['status']=='pass' for c in workflow['cases']),name
    for name in ('installed-source-and-file-manifest','installed-files-unchanged'):
        assert any(c['id']==name and c['status']=='pass' for c in startup['cases']),name
    assert Path(startup['installed']).resolve()==installed
    for child in [*[m['production'] for m in workflow['models']],*startup['production_runs']]:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    actual=workflow['models'][0]['production']['artifacts']
    assert actual['effective_inputs']['calculator']=='propeller5'
    assert read(Path(actual['report_json']))['calculator']=='propeller5'
    receipt=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Five native C++ propulsion workflows, existing Qt dialogs and original external solvers.',
        installed=str(installed),previous_installation=str(previous),manifest_sha256=digest(installed/'build-manifest.json'),
        verified_package_files=len(files),legacy_source_sha256=legacy,binaries_sha256={name:digest(installed/name) for name in binaries},
        reports=reports,combined_counts={key:sum(row['counts'][key] for row in reports.values()) for key in ('pass','fail','review','not_run','error')},
        core_elapsed_seconds=core['elapsed_seconds'],
        notes=['PropPred and all 11 FPPCALC families compared with original real outputs. Task4 caption repairs are checked separately from numeric parity.',
               'CALCPROP Task1-3 still produce no output; recorded not_run. Fixed protocol fixtures are not physical-solver validation.',
               'Old Task3 rejects the fixed complete protocol; review retained, with independent matrix and linear expectations for native postprocessing.',
               'Four-point interval overrun, Task3 area overwrite, eight-material strength verdicts and missing numbered captions repaired.',
               'Running cancellation and forced backend shutdown terminate the owned solver; cache extraction is document/revision scoped.',
               'All 15 hidden Qt performance callbacks checked; installed native propulsion report/CSV and geometry equal the tested runtime.',
               'Desktop input and full visual parity remain separate. Resistance methods and other remaining Pascal modules are outside this delivery.',
               'Test sources and generators remain in FreeShip-Tests; output is on C. Solver binaries are not redistributed.'],
        maintenance_sources_sha256={name:digest(ROOT/'maintenance'/name) for name in ('record_propulsion_native_delivery.py','record_topology_delivery.py','promote_runtime.ps1')},
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=receipt['combined_counts'],verified_package_files=len(files)),ensure_ascii=False))


if __name__=='__main__':main()
