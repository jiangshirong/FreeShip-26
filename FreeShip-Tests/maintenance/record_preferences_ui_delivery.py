"""Audit an existing preferences UI installation; no build, promotion or cleanup.

Maintenance evidence, not a regression suite. Functional checks are registered
as qt-preferences-ui in run_tests.py. All referenced tests and binaries must match.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    names=('core','bootstrap','publication','installed','startup')
    for name in names:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();installed=(q.QT/'dist').resolve();sources=q.current_sources(q.QT)
    manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    assert all(digest(installed/n)==h for n,h in manifest['binaries_sha256'].items())
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==read(ROOT/'maintenance/preferences-native-installed-20261103.json')['source_sha256']
    data={};reports={};counts={k:0 for k in ('pass','fail','review','not_run','error')}
    for name in names:
        path=getattr(a,name).resolve(strict=True);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for file,h in report.get('test_sources_sha256',{}).items():assert digest(ROOT/file)==h,file
        reports[name]={'path':str(path),'sha256':digest(path),'counts':report['counts']}
        for key in counts:counts[key]+=report['counts'][key]
        runtime=Path(report.get('runtime',report.get('installed',''))).resolve(strict=True)
        other=read(runtime/'build-manifest.json')
        assert {k.replace('\\','/'):v for k,v in other['source_sha256'].items()}==sources,name
        for exe in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe'):
            assert digest(runtime/exe)==digest(installed/exe),(name,exe)
    def passed(report,key):assert any(c.get('id')==key and c['status']=='pass' for c in report['cases']),key
    for name in ('core','installed'):
        for key in ('apply.live-graphics-and-toolbar','apply.selection-camera-coordinate-draft-preserved',
                    'restart.restores-preferences','cancel.exact-preference-file',
                    'backend-failure.rolls-back-file-and-appearance','conflict.preserve-other-writer-and-live-state',
                    'native.trim-preserves-current-fbm','colors.saved-bgr-becomes-correct-rgb'):
            passed(data[name],key)
    for i in (1,2,3):
        for key in ('complete-snapshot-exact','worker-input-exact','scene-exact','dxf-exact'):passed(data['publication'],f'model-{i}.{key}')
    assert Path(data['installed']['runtime']).resolve()==installed
    for name in ('installed','startup'):
        for run in data[name]['production_runs']:
            assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
            assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    record={'created_utc':datetime.now(timezone.utc).isoformat(),'status':'partial',
            'scope':'Qt preferences dialog, persistent settings and live document presentation; no language work',
            'installed':str(installed),'previous':str(q.QT/'build/dist-before-preferences-ui-20261104'),
            'package_manifest_sha256':digest(installed/'build-manifest.json'),'source_sha256':legacy,
            'reports':reports,'counts':counts,'audit_script_sha256':digest(Path(__file__)),
            'limitations':['Counts are check occurrences, including installed repetition, not unique scenarios.',
                           'Desktop mouse, focus, multi-display and DPI acceptance were not performed.',
                           'Language support belongs to the separate translation workstream.']}
    a.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(a.output),'counts':counts},ensure_ascii=False))


if __name__=='__main__':main()
