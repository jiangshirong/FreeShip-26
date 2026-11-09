"""Verify completed native-plate evidence and retain the local install receipt.

Maintenance only: does not run tests, promote binaries or relabel untested work.
"""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import json
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    names=('core','migration','installed_workflow','startup')
    for name in names:parser.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'maintenance/plates-native-installed-20261103.json')
    args=parser.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    previous=(q.QT/'build/dist-before-plates-native-20261103').resolve(strict=True)
    manifest=read(installed/'build-manifest.json');sources=q.current_sources(q.QT)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    q.validate_test_manifest(manifest)
    for name,value in manifest['binaries_sha256'].items():assert digest(installed/name)==value,name
    reports={};data={};counts={k:0 for k in ('pass','fail','review','not_run','error')}
    for name in names:
        path=getattr(args,name);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for key,value in report.get('test_sources_sha256',report.get('test_sha256',{})).items():assert digest(ROOT/key)==value,key
        reports[name]={'path':str(path.resolve()),'sha256':digest(path),'counts':report['counts']}
        for key in counts:counts[key]+=report['counts'][key]
    core,migration,workflow,startup=(data[name] for name in names)
    assert core['qt_sources_sha256']==sources
    assert core['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    def passed(report,name):assert any(c['id']==name and c['status']=='pass' for c in report['cases']),name
    for case in ('native.without-pascal','native.forbid-old','native.edit-recomputed','native.undo-restores',
                 'native.redo-restores','native.cancel-in-flight','native.write-failure-isolated',
                 'native.recover-after-failures','native.coplanar-no-invented-contour',
                 'native.version-0.analytic-panel','precision.plane.3.analytic-full-resolution',
                 'plane.production.native-engine','plane.production.plates.late-result-produced-before-cancel',
                 'plane.production.plates.cancel-preserves-result','hole.dxf-all-boundary-edges','plane.pdf-physical-scale'):
        passed(core,case)
    assert {k.replace('\\','/'):v for k,v in migration['qt_build_manifest']['source_sha256'].items()}==sources
    for binary in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
                   'FreeShipGeometryExport.exe','bridge/FreeShipGeometryBridge.exe'):
        assert digest(args.core.parent/'runtime'/binary)==digest(installed/binary),binary
        assert digest(args.migration.parent/'runtime'/binary)==digest(installed/binary),binary
    assert Path(workflow['deployment']).resolve()==installed
    assert workflow['workflow_reference_sha256']==digest(args.core)
    assert workflow['package_manifest']==manifest
    for case in ('package-current-source-test-and-binary-manifest','actual-deployment-files-unchanged',
                 'model-1.deployment.plate-data-equals-tested-runtime',
                 'model-1.deployment.plates.late-result-produced-before-cancel',
                 'model-1.deployment.plates.cancel-preserves-result',
                 'model-1.deployment.plate-dxf-equals-tested-runtime','model-1.deployment.plate-txt-equals-tested-runtime'):
        passed(workflow,case)
    assert Path(startup['installed']).resolve()==installed
    passed(startup,'installed-source-and-file-manifest');passed(startup,'installed-files-unchanged')
    for child in [*[m['production'] for m in workflow['models']],*startup['production_runs']]:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    record={'created_utc':datetime.now(timezone.utc).isoformat(),'status':'partial',
            'scope':'Native C++ plate calculation and existing Qt/export workflow; not complete application migration',
            'installed':str(installed),'previous':str(previous),'package_manifest_sha256':digest(installed/'build-manifest.json'),
            'source_sha256':core['source_sha256'],'reports':reports,'counts':counts,
            'timings':core['native_plate_precision_timings'],
            'limitations':['Hidden Qt callbacks do not cover desktop input, physical printing or machining.',
                           'Full migration still includes IGES preparation, legacy analysis loading and remaining tools/UI.'],
            'audit_script_sha256':digest(Path(__file__))}
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'receipt':str(args.output),'counts':counts},ensure_ascii=False))


if __name__=='__main__':main()
