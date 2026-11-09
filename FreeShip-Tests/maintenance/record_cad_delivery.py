"""One-time local delivery evidence after actual-directory verification.

This retains hashes and coverage boundaries; it is not an application test.
"""
import hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q

def read(path):return json.loads(path.read_text('utf-8-sig'))
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    installed=ROOT.parent/'FreeShip-Qt/dist';report_root=Path('C:/FreeShip26Tests/reports')
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path)
    q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==q.current_sources(q.QT)
    for name,value in manifest['binaries_sha256'].items():assert digest(installed/name)==value,name
    reports={}
    expected={
        'qt-cad-c':(461,1,1),'qt-cad-unified/qt-cad':(461,1,1),
        'qt-exchange-cad':(280,1,1),'qt-migration-cad':(2154,27,18),
        'qt-cad-package':(203,1,1),'qt-cad-delivery':(461,1,1),
        'qt-migration-cad-delivery':(85,0,4),
    }
    for name,counts in expected.items():
        path=report_root/name/'report.json';report=read(path);actual=report['counts']
        assert (actual['pass'],actual['review'],actual['not_run'])==counts,name
        assert actual['fail']==actual['error']==0,name
        reports[name]={'path':str(path),'status':report['status'],'counts':actual,'report_sha256':digest(path)}
    unified=read(report_root/'qt-cad-unified/report.json')
    assert len(unified['cases'])==1 and unified['cases'][0]['id']=='qt-cad' and unified['cases'][0]['status']=='partial'
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    result={
        'date':'2026-11-02','installed':str(installed),
        'previous_installation':str(ROOT.parent/'FreeShip-Qt/build/dist-before-cad-20261102'),
        'scope':'IGES surfaces, 3D DXF section/control splines, 3DFACE mesh, offsets and coordinates.',
        'binaries_sha256':{n:digest(installed/n) for n in ('FreeShipQt.exe','bridge/FreeShipGeometryBridge.exe')},
        'package_manifest_sha256':digest(manifest_path),'package_files_verified':len(manifest['binaries_sha256']),
        'legacy_source_sha256':legacy,'bridge_build':'C:/FreeShip26Tests/build/cad-service-02',
        'reports':reports,
        'independent_consumers':{'ezdxf':'1.4.4','cadquery-ocp':'7.8.1.1.post1','vtk':'9.3.1'},
        'notes':[
            'IGES patch construction retains the original Plus algorithm on a disposable document. It is not exact equivalence to a subdivision surface or a sewn solid.',
            'All 166 IGES patches from real 9a are transferred by OpenCascade. Planar fixtures check independent physical bounds and areas; cubic curves check an analytic polynomial.',
            'Roundtrip compares every coordinate/knot and oriented triangle; camera state, hidden layers, negative-Y, centre and near-centre cases are separate checks.',
            'Numeric IGES parameters no longer span physical records. The failed first implementation remains in qt-cad-a as migration evidence, not an upstream defect.',
            'Repeated patch conversion uses a fixed grid rotation independent of pointer order. Original geometry, settings, undo, dirty state, scene and existing DXF are unchanged by export/cancel/failure.',
            'Independent consumer libraries are optional test dependencies outside the package, not shipped application dependencies.',
            'Actual installed-directory checks use hidden Windows Qt widgets. Commercial CAD desktop workflows, real mouse/file chooser, manufacturing and full original UI fidelity remain unverified.',
            'GHS, other specialist formats, remaining imports and the remaining native C++ core migration are still pending.',
            'All test and maintenance sources remain in FreeShip-Tests; new output/build directories are on C. The legacy fs entry and QtPilot were not replaced.',
        ],
        'free_bytes_after':{d:shutil.disk_usage(d+':/').free for d in ('C','D')},
    }
    target=ROOT/'maintenance/cad-installed-20261102.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'verified_files':len(manifest['binaries_sha256']),'reports':len(reports)}))

if __name__=='__main__':main()
