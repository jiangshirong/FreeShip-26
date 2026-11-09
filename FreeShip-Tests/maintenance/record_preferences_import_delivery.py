"""Audit legacy preference import reports against the installed Qt package.

Read-only maintenance evidence, not another regression. Functional checks live
in the unified qt-preferences-ui / qt-preferences-import suites.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('core', 'bootstrap', 'installed', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    installed = (q.QT/'dist').resolve()
    sources = q.current_sources(q.QT)
    manifest = read(installed/'build-manifest.json')
    q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()} == sources
    assert all(digest(installed/n)==h for n,h in manifest['binaries_sha256'].items())
    counts = dict.fromkeys(('pass','fail','review','not_run','error'), 0)
    reports = {}
    data = {}
    for name in ('core','bootstrap','installed'):
        path = getattr(args,name).resolve(strict=True)
        report = read(path)
        data[name] = report
        assert report['counts']['fail']==report['counts']['error']==0, name
        for filename, expected in report['test_sources_sha256'].items():
            assert digest(ROOT/filename)==expected, filename
        runtime = Path(report['runtime']).resolve(strict=True)
        assert digest(runtime/'build-manifest.json')==digest(installed/'build-manifest.json'), name
        for filename in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe'):
            assert digest(runtime/filename)==digest(installed/filename), (name,filename)
        for key in counts:
            counts[key] += report['counts'][key]
        reports[name] = {'path':str(path),'sha256':digest(path),'counts':report['counts']}
    assert Path(data['installed']['runtime']).resolve()==installed
    for run in data['installed']['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    for name in ('core','installed'):
        passed = {case.get('id') for case in data[name]['cases'] if case['status']=='pass'}
        for key in ('cancel.keeps-file-and-live-state','draft.no-write-before-confirm',
                    'apply.geometry-exact','apply.dxf-exact-except-selected-colors','apply.fbm-exact',
                    'restart.independent-of-old-theme','invalid-files.no-persistence',
                    'dta.fixed-and-optional-order','encoding.current-document-bytes-unchanged',
                    'legacy-source-bytes-unchanged'):
            assert 'import.'+key in passed, (name,key)
    record = {
        'created_utc':datetime.now(timezone.utc).isoformat(), 'status':'partial',
        'scope':'Explicit read-only INI/DTA import into the Qt preferences draft',
        'installed':str(installed),
        'previous':str(q.QT/'build/dist-before-preferences-import-20261109'),
        'package_manifest_sha256':digest(installed/'build-manifest.json'),
        'reports':reports, 'counts':counts, 'audit_script_sha256':digest(Path(__file__)),
        'limitations':[
            'Counts include installed repetition, not distinct user scenarios.',
            'Hidden production callbacks and reviewed screenshots are not desktop input/focus/DPI acceptance.',
            'No language migration, recent-file or window-layout import.',
            'Unsupported theme options and invalid directories are reported and skipped.',
            'Text files must be valid UTF-8; legacy ANSI code pages are not guessed.',
            'Selected display alpha and DXF colors may change; model geometry and FBM bytes remain exact.'
        ]}
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(args.output),'counts':counts},ensure_ascii=False))


if __name__=='__main__':
    main()
