"""Installation audit, not a regression. qt-model-ui owns reusable behavior tests.

Verifies report/source/binary identities and writes a receipt; never installs or
deletes files. Counts include the repeated checks on the installed runtime.
"""
import argparse
import json
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest


def main():
    p=argparse.ArgumentParser(description=__doc__);names=('core','publication','preferences','installed')
    for name in names:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();installed=(q.QT/'dist').resolve();sources=q.current_sources(q.QT)
    manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    assert all(digest(installed/n)==h for n,h in manifest['binaries_sha256'].items())
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];assert legacy==read(ROOT/'maintenance/preferences-ui-installed-20261104.json')['source_sha256']
    reports={};data={};counts={k:0 for k in ('pass','fail','review','not_run','error')}
    for name in names:
        path=getattr(a,name).resolve(strict=True);r=read(path);data[name]=r;assert r['counts']['fail']==r['counts']['error']==0,name
        for file,h in r.get('test_sources_sha256',{}).items():assert digest(ROOT/file)==h,file
        runtime=Path(r['runtime']).resolve(strict=True);other=read(runtime/'build-manifest.json')
        assert {k.replace('\\','/'):v for k,v in other['source_sha256'].items()}==sources
        for exe in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','qt_model_ui_tests.exe'):assert digest(runtime/exe)==digest(installed/exe),(name,exe)
        reports[name]=dict(path=str(path),sha256=digest(path),counts=r['counts'])
        for key in counts:counts[key]+=r['counts'][key]
    def passed(r,key):assert any(c['id']==key and c['status']=='pass' for c in r['cases']),key
    for name in ('core','installed'):
        for key in ('native.order-preserves-stable-geometry','native.reopen-preserves-face-layer-ownership','native.failed-publication-rolls-back-alpha','legacy-v20.alpha-upgrade-persists-exact-value','production.context-export-is-exact-current-view','production.selection-menu-matches-point-placement','production.placed-point-selected-in-all-views','production-flow.flow-tool-remains-active-after-document-update'):passed(data[name],key)
    for i in (1,2,3):
        for key in ('complete-snapshot-exact','layer-alpha-from-pascal','worker-input-exact','scene-exact','dxf-exact'):passed(data['publication'],f'model-{i}.{key}')
    assert Path(data['installed']['runtime']).resolve()==installed
    run=data['installed']['production'];assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe';assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',scope='Qt layer properties, opacity/order persistence, point placement, continuous flow placement, label recovery and view context actions; no language work',installed=str(installed),previous=str(q.QT/'build/dist-before-model-ui-20261104'),package_manifest_sha256=digest(installed/'build-manifest.json'),source_sha256=legacy,reports=reports,counts=counts,audit_script_sha256=digest(Path(__file__)),limitations=['Counts include installed repetition, not unique scenarios.','Hidden events/screenshots do not certify desktop mouse, focus, native dialogs, physical printing or multi-monitor DPI.','Specialized render modes, camera lenses and lighting remain pending.','Large translucent meshes use CPU fragment compositing; no GPU performance claim.','Language support belongs to the separate translation workstream.'])
    a.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(receipt=str(a.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
