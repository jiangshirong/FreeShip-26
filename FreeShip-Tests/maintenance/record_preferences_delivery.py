"""Verify native preference bootstrap delivery; never build, install or delete."""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    names=('core','document','publication','installed','startup')
    for name in names:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'maintenance/preferences-native-installed-20261103.json')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True);sources=q.current_sources(q.QT)
    manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    for name,h in manifest['binaries_sha256'].items():assert digest(installed/name)==h,name
    reports={};data={};counts={k:0 for k in ('pass','fail','review','not_run','error')}
    for name in names:
        path=getattr(a,name).resolve(strict=True);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for file,h in report.get('test_sources_sha256',{}).items():assert digest(ROOT/file)==h,file
        reports[name]=dict(path=str(path),sha256=digest(path),counts=report['counts'])
        for key in counts:counts[key]+=report['counts'][key]
    def passed(report,identifier):
        assert any(c['id']==identifier and c['status']=='pass' for c in report['cases']),identifier
    for name in ('core','installed'):
        report=data[name]
        for case in ('default','global','user-override','parent-and-user-theme','duplicate-and-invalid-integer',
                     'negative-undo','no-undo','quoted-values','legacy-dta'):
            passed(report,case+'.all-context-fields-exact')
        for key in ('detached.legacy-executable-absent','independent.no-pascal-bootstrap','detached.ui-open',
                    'detached.ui-restart','detached.new-hull','detached.cad','detached.plates',
                    'reject.all-failures-preserve-fbm','catalog.reject-truncated-utf8','runtime-files-unchanged'):
            passed(report,key)
    publication=data['publication']
    for i in (1,2,3):
        for key in ('complete-snapshot-exact','worker-input-exact','scene-exact','dxf-exact'):
            passed(publication,f'model-{i}.{key}')
    for name in ('core','installed','publication'):
        runtime=Path(data[name]['runtime'])
        other=read(runtime/'build-manifest.json')
        assert {k.replace('\\','/'):v for k,v in other['source_sha256'].items()}==sources
        for exe in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','bridge/FreeShipGeometryBridge.exe'):
            assert digest(runtime/exe)==digest(installed/exe)
    assert Path(data['installed']['runtime']).resolve()==installed
    document=data['document'];assert document['qt_sources_sha256']==sources
    assert document['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    for key in ('legacy-source-unchanged','qt-source-unchanged','test-sources-unchanged'):passed(document,key)
    startup=data['startup'];assert Path(startup['installed']).resolve()==installed
    for key in ('installed-source-and-file-manifest','installed-files-unchanged'):passed(startup,key)
    for run in startup['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    previous=(q.QT/'build/dist-before-preferences-native-20261103').resolve(strict=True)
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Native model preference/caption bootstrap and startup without Pascal; full UI migration is separate',
        installed=str(installed),previous=str(previous),package_manifest_sha256=digest(installed/'build-manifest.json'),
        source_sha256=document['source_sha256'],reports=reports,counts=counts,
        limitations=['Qt-wide language switching and the old preferences dialog remain UI work.',
                     'Explicit old diagnostic and unported compatibility paths remain packaged.',
                     'Hidden callbacks do not establish desktop visual, mouse, focus or device acceptance.'],
        audit_script_sha256=digest(Path(__file__)))
    a.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
