"""One-time local export delivery receipt, not a regression or automatic updater."""
import hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q

def read(path):return json.loads(path.read_text('utf-8-sig'))
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    installed=(ROOT.parent/'FreeShip-Qt/dist').resolve();base=Path('C:/FreeShip26Tests/reports')
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path);q.validate_test_manifest(manifest)
    current=q.current_sources(q.QT)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==current
    for name,h in manifest['binaries_sha256'].items():
        path=(installed/name).resolve();assert installed in path.parents and digest(path)==h,name
    expected={'qt-ship-formats-unified/qt-ship-formats':(215,1,2),
              'qt-ship-formats-delivery':(215,1,2),'qt-ghs-ship-delivery':(90,1,1),
              'qt-cad-ship-delivery':(461,1,1),'qt-migration-ship-delivery':(85,0,4)}
    reports={}
    for name,wanted in expected.items():
        path=base/name/'report.json';data=read(path);counts=data['counts']
        assert counts['fail']==counts['error']==0 and (counts['pass'],counts['review'],counts['not_run'])==wanted,(name,counts)
        reports[name]={'path':str(path),'sha256':digest(path),'counts':counts,'status':data['status']}
        if name!='qt-migration-ship-delivery':
            assert data['qt_sources_sha256']==current
            for file,h in data['test_sources_sha256'].items():assert digest(ROOT/file)==h,file
            if name.endswith('-delivery'):assert Path(data['runtime_path']).resolve()==installed
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    old=base/'qt-ghs-delivery/model-3/export/船体 with spaces.gf'
    new=base/'qt-ghs-ship-delivery/model-3/export/船体 with spaces.gf'
    assert old.read_bytes()==new.read_bytes()
    exports={}
    for fmt,expected_points in [('arch_hll',4331),('arch_app',4331),('pam',2171)]:
        data=read(base/'qt-ship-formats-delivery/model-3'/fmt/'result.json')
        assert data['status']=='pass' and data['stations']==11 and data['half_section_points']==2171 and data['points']==expected_points
        exports[fmt]=data
    receipt={
        'date':'2026-11-02','installed':str(installed),
        'previous_installation':str(ROOT.parent/'FreeShip-Qt/build/dist-before-ship-formats-20261102'),
        'scope':'Native C++ Archimedes HLL/APP and legacy Windows PAM geometry template, Qt export/cancel/reference editing.',
        'manifest_sha256':digest(manifest_path),'verified_package_files':len(manifest['binaries_sha256']),
        'binaries_sha256':{name:digest(installed/name) for name in ('FreeShipQt.exe','FreeShipGeometryExport.exe','bridge/FreeShipGeometryBridge.exe')},
        'legacy_source_sha256':legacy,'bridge_build':'C:/FreeShip26Tests/build/cad-service-02',
        'reports':reports,'real_9a_exports':exports,
        'ghs_9a_byte_comparison':{'previous':str(old),'current':str(new),'sha256':digest(new),'equal':True},
        'notes':[
            'HLL has escaped UTF-8 names and longitudinal draught-mark positions. APP/PAM description fields retain ASCII. File paths preserve Unicode and spaces.',
            'Geometry uses all hydrostatics layers and existing stations, preserving origin and precision. The 0.1 mm chord bound is a curve tessellation bound, not total hull accuracy.',
            'PAM uses metres and kg/m3 and an explicit midship / waterline reference. Opaque original calculation parameters remain unchanged. This is not Add_Mass.EXE INA. input or solver integration.',
            'The first diagnostic report had a null half_section_points field due to an invalidated QJsonObject value reference. It was fixed and tested; this new migration defect is not an upstream issue.',
            'Installed-directory tests exercise hidden production Windows Qt callbacks. Physical desktop and external Archimedes/PAM/GHS receiver acceptance remain unverified, including centerline-connected multiple lobes.',
            'The old source, QtPilot, user models and fs launcher remain unchanged. New test outputs and disposable builds are on C; reusable tests and one-time maintenance sources remain in FreeShip-Tests.',
            'Michlet/AddMass, parts and remaining imports, Pascal model ownership and professional computations still require migration.'
        ],
        'free_bytes_after':{drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')}
    }
    target=ROOT/'maintenance/ship-formats-installed-20261102.json'
    target.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'verified_package_files':receipt['verified_package_files'],'reports':len(reports),'installed_passes':sum(v[0] for n,v in expected.items() if n.endswith('-delivery'))},ensure_ascii=False))

if __name__=='__main__':main()
