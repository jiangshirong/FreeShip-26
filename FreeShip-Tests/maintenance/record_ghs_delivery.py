"""One-time GHS delivery receipt. Not a regression suite or automatic updater."""
import hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q

def read(p):return json.loads(p.read_text('utf-8-sig'))
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
    installed=ROOT.parent/'FreeShip-Qt/dist';base=Path('C:/FreeShip26Tests/reports')
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path)
    q.validate_test_manifest(manifest)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==q.current_sources(q.QT)
    for name,h in manifest['binaries_sha256'].items():assert digest(installed/name)==h,name
    expected={'qt-ghs-final':(90,1,1),'qt-ghs-unified/qt-ghs':(89,1,1),
              'qt-cad-ghs':(461,1,1),'qt-exchange-ghs':(280,1,1),
              'qt-ghs-package':(90,1,1),'qt-migration-ghs-package':(85,0,4),
              'qt-ghs-delivery':(90,1,1),'qt-cad-ghs-delivery':(461,1,1),
              'qt-migration-ghs-delivery':(85,0,4)}
    reports={}
    for name,wanted in expected.items():
        p=base/name/'report.json';data=read(p);c=data['counts']
        assert c['fail']==c['error']==0 and (c['pass'],c['review'],c['not_run'])==wanted,(name,c)
        reports[name]={'path':str(p),'sha256':digest(p),'counts':c,'status':data['status']}
        if name in ('qt-ghs-delivery','qt-cad-ghs-delivery'):
            assert Path(data['runtime_path']).resolve()==installed.resolve()
            assert data['qt_sources_sha256']==q.current_sources(q.QT)
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    ghs=read(base/'qt-ghs-delivery/model-3/export/result.json')
    assert ghs['status']=='pass' and ghs['stations']==11 and ghs['points']==2171
    receipt={
        'date':'2026-11-02','installed':str(installed),
        'previous_installation':str(ROOT.parent/'FreeShip-Qt/build/dist-before-ghs-20261102'),
        'scope':'Native C++ GHS symmetric displacement-hull sections and Qt export/cancel route.',
        'manifest_sha256':digest(manifest_path),'verified_package_files':len(manifest['binaries_sha256']),
        'binaries_sha256':{n:digest(installed/n) for n in ('FreeShipQt.exe','FreeShipGeometryExport.exe','bridge/FreeShipGeometryBridge.exe')},
        'legacy_source_sha256':legacy,'bridge_build':'C:/FreeShip26Tests/build/cad-service-02',
        'reports':reports,'real_9a_export':ghs,
        'notes':[
            'The original document origin, every original ordinate and knuckle are preserved. Adaptive chord error refers to spline tessellation, not total hull accuracy.',
            'Only hydrostatics layers and existing stations are used, independently of camera or display visibility. Deck closing requires an explicit option.',
            'Separate lobes may connect on centerline only; analytic area and centroid tests ensure zero added area/moment. Lateral gaps, self-intersection and asymmetric hulls are rejected.',
            'The closed-half-surface defect is a new migration issue, with a failing negative control retained in qt-ghs-closed-negative. It is not an upstream issue.',
            'Installed-directory tests use hidden production Qt widgets. Physical desktop and commercial GHS, especially multi-lobe handling, remain unverified.',
            'The unified suite ran before the last closed-centerplane regression. The final package and actual directory include and pass that regression.',
            'Other specialist exports/imports and remaining Pascal model/computation ownership still require migration.',
            'Tests and maintenance sources are retained in FreeShip-Tests. Outputs and disposable builds are on C. Legacy fs and QtPilot are unchanged.'
        ],
        'free_bytes_after':{drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')}
    }
    target=ROOT/'maintenance/ghs-installed-20261102.json';target.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'verified_files':receipt['verified_package_files'],'reports':len(reports)},ensure_ascii=False))

if __name__=='__main__':main()
