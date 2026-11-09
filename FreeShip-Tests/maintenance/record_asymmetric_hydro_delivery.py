"""Audit native asymmetric hydrostatics installation and retained evidence.

Maintenance receipt only. Numerical and production callbacks are registered in
qt-asymmetric-hydro; this script never builds, changes settings or promotes.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sections','regression','publication','installed','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();installed=(q.QT/'dist').resolve()
    sources=q.current_sources(q.QT);manifest=read(installed/'build-manifest.json');q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    assert all(digest(installed/n)==h for n,h in manifest['binaries_sha256'].items())
    reports={};data={};counts=dict.fromkeys(('pass','fail','review','not_run','error'),0)
    for name in ('sections','regression','publication','installed'):
        path=getattr(args,name).resolve(strict=True);report=read(path);data[name]=report
        assert report['counts']['fail']==report['counts']['error']==0,name
        for filename,h in report.get('test_sources_sha256',{}).items():assert digest(ROOT/filename)==h,filename
        runtime=Path(report['runtime']).resolve(strict=True)
        assert digest(runtime/'build-manifest.json')==digest(installed/'build-manifest.json'),name
        for filename in ('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe'):
            assert digest(runtime/filename)==digest(installed/filename),(name,filename)
        reports[name]={'path':str(path),'sha256':digest(path),'counts':report['counts']}
        for key in counts:counts[key]+=report['counts'][key]
    assert Path(data['installed']['runtime']).resolve()==installed
    for run in data['installed']['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    for name in ('sections','installed'):
        passed={c.get('id') for c in data[name]['cases'] if c['status']=='pass'}
        for key in ('offset-box.waterplane_ix','two-hulls.waterplane_ix','moonpool.waterplane_ix',
                    'mixed-layers.waterplane_ix','narrow-gap.waterplane_area','warped.area-volume-identity',
                    'imperial.kmt','undo.exact','one-reversed-face.explicit-error','open-bottom.explicit-error',
                    'production.actual-native-results','production.csv-transverse-flotation'):
            assert key in passed,(name,key)
    earlier=Path(args.sections).resolve().parents[2]/'warped-check/qt-asymmetric-hydro/report.json'
    evidence={'path':str(earlier),'sha256':digest(earlier)} if earlier.is_file() else None
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Asymmetric actual-mesh section moments, consistent volume clipping, auxiliary tables and report UI',
        installed=str(installed),previous=str(q.QT/'build/dist-before-asymmetric-hydro-20261109'),
        package_manifest_sha256=digest(installed/'build-manifest.json'),reports=reports,counts=counts,
        original_warped_face_failure=evidence,audit_script_sha256=digest(Path(__file__)),
        limitations=['Counts include repeated installation checks, not distinct user scenarios.',
            'General lateral projection and the legacy one-sided entrance angle remain null for asymmetric hulls.',
            'Symmetric-only historical integration is retained; this does not certify all symmetric multi-contour cases.',
            'Asymmetric hydrostatic viewport overlays and real desktop input/DPI acceptance remain pending.',
            'Overlapping solids are not automatically boolean-unioned; accuracy refers to the selected subdivision mesh.'])
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(receipt=str(args.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
