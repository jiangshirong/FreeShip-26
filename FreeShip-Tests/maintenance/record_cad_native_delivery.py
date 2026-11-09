"""Audit native IGES delivery evidence. Does not build, run, promote or clean."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import json
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    names=('core','migration','installed_workflow','startup')
    for name in names:parser.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'maintenance/cad-native-installed-20261103.json')
    args=parser.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    previous=(q.QT/'build/dist-before-cad-native-20261103').resolve(strict=True)
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
    assert core['qt_sources_sha256']==workflow['qt_sources_sha256']==sources
    assert core['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    def passed(report,name):assert any(c['id']==name and c['status']=='pass' for c in report['cases']),name
    for report in (core,workflow):
        for case in ('native.without-pascal','native.forbid-pascal-fallback','native.edit-recomputed',
                     'native.undo-restores','native.redo-restores','native.atomic-failure','native.retry',
                     'hidden.all.analytic-area','hidden.visible.analytic-area','layers.retain-layer-ownership',
                     'ui.late-cancel-no-output','cad.native-engine'):
            passed(report,case)
    for i in (1,2,3):passed(core,f'native-model-{i}.control-points-and-knots')
    assert {k.replace('\\','/'):v for k,v in migration['qt_build_manifest']['source_sha256'].items()}==sources
    for binary in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
                   'FreeShipGeometryExport.exe','bridge/FreeShipGeometryBridge.exe'):
        assert digest(args.core.parent/'runtime'/binary)==digest(installed/binary),binary
        assert digest(args.migration.parent/'runtime'/binary)==digest(installed/binary),binary
    assert Path(workflow['runtime_path']).resolve()==installed
    assert workflow['runtime_manifest_sha256']==digest(installed/'build-manifest.json')
    assert Path(startup['installed']).resolve()==installed
    passed(startup,'installed-source-and-file-manifest');passed(startup,'installed-files-unchanged')
    for child in startup['production_runs']:
        assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
    record={'created_utc':datetime.now(timezone.utc).isoformat(),'status':'partial',
            'scope':'Native C++ IGES surface preparation and existing Qt export workflow; not full application migration',
            'installed':str(installed),'previous':str(previous),'package_manifest_sha256':digest(installed/'build-manifest.json'),
            'source_sha256':core['source_sha256'],'reports':reports,'counts':counts,
            'timings':core['native_cad_timings'],
            'limitations':['Hidden Qt callbacks and OpenCascade readers do not cover physical desktop or commercial CAD/manufacturing acceptance.',
                           'Other formats, legacy analysis loading and remaining tools/UI are still in the migration backlog.'],
            'audit_script_sha256':digest(Path(__file__))}
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'receipt':str(args.output),'counts':counts},ensure_ascii=False))


if __name__=='__main__':main()
