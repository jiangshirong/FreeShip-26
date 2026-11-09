"""Verify and record native publication delivery. No build/deploy/cleanup actions."""
import argparse,json
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    names=('core','document','migration','installed','startup')
    for name in names:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/publication-native-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True);sources=q.current_sources(q.QT)
    manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    for name,h in manifest['binaries_sha256'].items():assert digest(installed/name)==h,name
    reports={};data={};counts={k:0 for k in ('pass','fail','review','not_run','error')}
    for name in names:
        path=getattr(a,name).resolve(strict=True);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for file,h in report.get('test_sources_sha256',report.get('test_sha256',{})).items():assert digest(ROOT/file)==h,file
        reports[name]=dict(path=str(path),sha256=digest(path),counts=report['counts'])
        for k in counts:counts[k]+=report['counts'][k]
    def passed(report,identifier):assert any(c['id']==identifier and c['status']=='pass' for c in report['cases']),identifier
    core,document,migration,actual,startup=(data[name] for name in names)
    for report in (core,actual):
        for i in (1,2,3):
            for key in ('complete-snapshot-exact','worker-input-exact','scene-exact','dxf-exact'):
                passed(report,f'model-{i}.{key}')
        for key in ('independent.only-context-bootstrap','transaction.failed-output-keeps-state','transaction.recovers-without-adapter',
                    'new.rounding.snapshot-exact','new.rounding.km-exact'):
            passed(report,key)
        runtime=Path(report['runtime'])
        for exe in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','bridge/FreeShipGeometryBridge.exe'):
            assert digest(runtime/exe)==digest(installed/exe)
    assert Path(actual['runtime']).resolve()==installed
    assert document['qt_sources_sha256']==sources
    assert document['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    passed(document,'legacy-source-unchanged');passed(document,'qt-source-unchanged');passed(document,'test-sources-unchanged')
    assert {k.replace('\\','/'):v for k,v in migration['qt_build_manifest']['source_sha256'].items()}==sources
    passed(startup,'installed-source-and-file-manifest');passed(startup,'installed-files-unchanged')
    assert Path(startup['installed']).resolve()==installed
    for run in startup['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    previous=(q.QT/'build/dist-before-publication-native-20261103').resolve(strict=True)
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='C++ document snapshot, scene input and hydrostatic annotations; not complete application migration',
        installed=str(installed),previous=str(previous),package_manifest_sha256=digest(installed/'build-manifest.json'),
        source_sha256=document['source_sha256'],reports=reports,counts=counts,
        limitations=['Preference/translation bootstrap and explicit old diagnostic/compatibility paths remain.',
                     'Open asymmetric half-hull hydro labels are intentionally suppressed; original differences remain review.',
                     'Hidden Qt callbacks do not establish desktop visual, mouse, focus or device acceptance.'],
        audit_script_sha256=digest(Path(__file__)))
    a.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
