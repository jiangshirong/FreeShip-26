"""Read-only installed AddMass audit; writes a receipt, not a functional test."""
import argparse, json, shutil
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--installed-report',type=Path,required=True)
    p.add_argument('--regression-report',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--delivery',choices=('added-mass','grid-preview'),default='added-mass')
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/('build/dist-before-'+a.delivery+'-20261104')).resolve(strict=True)
    manifest=read(installed/'build-manifest.json');sources=q.current_sources(q.QT)
    assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    q.validate_test_manifest(manifest);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']==read(q.QT/'docs/migration/BASELINE.json')['legacy_source_sha256']
    files={f.relative_to(installed).as_posix():digest(f) for f in installed.rglob('*') if f.is_file() and f.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256'];reports={}
    for path in [a.installed_report,*a.regression_report]:
        data=read(path);assert data['counts']['fail']==data['counts']['error']==0
        assert data['qt_sources_sha256']==sources
        if 'source_sha256' in data:assert data['source_sha256']==legacy
        else:assert any(c['id']=='legacy-protected' and c['status']=='pass' for c in data['cases'])
        for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
        runtime=Path(data.get('runtime',data.get('runtime_path'))).resolve(strict=True)
        tested=read(runtime/'build-manifest.json')
        for n,h in tested['binaries_sha256'].items():
            if Path(n).suffix.lower() in ('.exe','.dll'):assert digest(runtime/n)==h==digest(installed/n),n
        if path==a.installed_report:assert runtime==installed and data['suite']=='qt-'+a.delivery
        reports[data['suite']]=dict(path=str(path.resolve()),sha256=digest(path),counts=data['counts'])
    assert set(reports)==({'qt-grid-preview','qt-added-mass','qt-legacy-exchange'} if a.delivery=='grid-preview' else {'qt-added-mass','qt-hydrodynamics','qt-legacy-exchange'})
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope=('Exact sampled-grid preview in Michlet/AddMass export and AddMass calculation dialogs; camera, invalidation, cancellation and unchanged exports.' if a.delivery=='grid-preview' else 'AddMass input, external solver execution, validated matrix and ellipsoid, bounded empirical postprocessing, Qt actions and reports.'),
        installed=str(installed),previous_installation=str(backup),verified_files=len(files),
        package_manifest_sha256=digest(installed/'build-manifest.json'),legacy_source_sha256=legacy,
        executable_sha256={n:h for n,h in files.items() if Path(n).suffix.lower()=='.exe'},reports=reports,
        counts={k:sum(v['counts'][k] for v in reports.values()) for k in ('pass','fail','review','not_run','error')},
        limits=['ADD_MASS.EXE remains an external original solver, not a native C++ numerical rewrite.',
                'Unsupported high-Cb river correction is omitted because upstream uses uninitialized variables.',
                ('Engineering certification, full old dialog fidelity and desktop acceptance remain separate.' if a.delivery=='grid-preview' else 'Engineering certification, old 3D offsets preview, full old dialog fidelity and desktop acceptance remain separate.')],
        free_bytes_after={drive:shutil.disk_usage(drive+':/').free for drive in ('C','D')})
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output),counts=result['counts'],verified_files=len(files)),ensure_ascii=False))
if __name__=='__main__':main()
