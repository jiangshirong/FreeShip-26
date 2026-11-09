"""Retain an audited installation receipt for the lateral projection work.

Maintenance only: does not build, promote or change user settings. Reusable
numerical/production checks live in the unified qt-lateral-projection suite.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    names=('projection','asymmetric','hydro','options','publication','installed')
    for name in (*names,'output'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();installed=(q.QT/'dist').resolve()
    manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==q.current_sources(q.QT)
    assert all(digest(installed/n)==h for n,h in manifest['binaries_sha256'].items())
    reports={};data={};counts=dict.fromkeys(('pass','fail','review','not_run','error'),0)
    for name in names:
        path=getattr(args,name).resolve(strict=True);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for filename,h in report.get('test_sources_sha256',{}).items():assert digest(ROOT/filename)==h,filename
        runtime=Path(report['runtime']).resolve(strict=True)
        assert digest(runtime/'build-manifest.json')==digest(installed/'build-manifest.json'),name
        for filename in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe'):
            assert digest(runtime/filename)==digest(installed/filename),(name,filename)
        reports[name]=dict(path=str(path),sha256=digest(path),counts=report['counts'])
        for key in counts:counts[key]+=report['counts'][key]
    assert Path(data['installed']['runtime']).resolve()==installed
    for run in data['installed']['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    for name in ('projection','installed'):
        passed={c.get('id') for c in data[name]['cases'] if c['status']=='pass'}
        for key in ('overlap-y.area','overlap-xz.area','moonpool.area','through-y.area',
                    'triangle-profile.area','units.area','undo.numerical-exact',
                    'tunnel.wind.p-2','shifted.km-caption-above-keel',
                    'submerged-5.no-fake-wind','cancellation.interrupted',
                    'clipper.packaged-source-and-license','production.view-3.wind'):
            assert key in passed,(name,key)
    earlier=args.projection.resolve().parents[2]/'first/qt-lateral-projection/report.json'
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Actual asymmetric XZ silhouette union, report integration and four-view hydrostatic markers',
        installed=str(installed),previous=str(q.QT/'build/dist-before-lateral-projection-20261109'),
        package_manifest_sha256=digest(installed/'build-manifest.json'),reports=reports,counts=counts,
        first_run=dict(path=str(earlier),sha256=digest(earlier)) if earlier.is_file() else None,
        audit_script_sha256=digest(Path(__file__)),
        limitations=['Counts include repeated installed checks, not unique user scenarios.',
            'Real desktop input, DPI and overall visual acceptance remain untested.',
            'Only participating hydrostatic surfaces contribute to wind projection; this is not complete superstructure wind loading.',
            'Legacy one-sided entrance angle remains null for asymmetric hulls; symmetric-only historical integration is retained.',
            'The normalized integer clipping grid is finer than stored Single mesh coordinates, not exact real arithmetic.'])
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(receipt=str(args.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
