"""Read-only audit of installed thumbnail refresh, save and recovery evidence."""
import argparse,json,shutil
from datetime import datetime,timezone
from pathlib import Path
from record_topology_delivery import ROOT,q,read,digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('preview','saving','recovery','publication','dialogs','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();installed=(q.QT/'dist').resolve(strict=True);manifest=read(installed/'build-manifest.json')
    q.validate_test_manifest(manifest);sources=q.current_sources(q.QT)
    assert sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}
    files={f.relative_to(installed).as_posix():digest(f) for f in installed.rglob('*') if f.is_file() and f.name!='build-manifest.json'}
    assert files==manifest['binaries_sha256']
    baseline=read(q.QT/'docs/migration/BASELINE.json');legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']==baseline['legacy_source_sha256']
    assert digest(q.common.DEFAULT_SOURCE/'FreeShip.exe')==baseline['legacy_executable_sha256']
    assert digest(q.PILOT/'dist/FreeShipQtPilot.exe')==baseline['accepted_pilot_executable_sha256']
    reports={};images={};times=[]
    for name in ('preview','saving','recovery','publication','dialogs'):
        path=getattr(a,name).resolve(strict=True);data=read(path)
        assert data['counts']['fail']==data['counts']['error']==0
        if name!='publication':assert data['qt_sources_sha256']==sources and data['source_sha256']==legacy
        for n,h in data['test_sources_sha256'].items():assert digest(ROOT/n)==h,n
        runtime=Path(data['runtime']).resolve(strict=True);tested=read(runtime/'build-manifest.json')
        assert {n.replace('\\','/'):h for n,h in tested['source_sha256'].items()}==sources
        for n,h in tested['binaries_sha256'].items():
            if Path(n).suffix.lower() in ('.exe','.dll'):assert digest(runtime/n)==h==digest(installed/n),n
        if name=='preview':
            assert runtime==installed
            for child in data['production_runs']:
                assert Path(child['executed_path']).resolve()==installed/'FreeShipQt.exe'
                assert child['executable_sha256']==digest(installed/'FreeShipQt.exe')
            for child in data['renders']:
                for image in child['artifacts'].values():images[image]=digest(Path(image))
            times=data['production_save_times']
        reports[name]=dict(path=str(path),sha256=digest(path),counts=data['counts'])
    result=dict(created_utc=datetime.now(timezone.utc).isoformat(),installed=str(installed),
        previous_package=str(q.QT/'build/dist-before-preview-image-20261105'),verified_files=len(files),
        source_sha256=sources,manifest_sha256=digest(installed/'build-manifest.json'),reports=reports,
        images_sha256=images,production_save_times=times,
        counts={k:sum(r['counts'][k] for r in reports.values()) for k in ('pass','fail','review','not_run','error')},
        limits=['Disabled/old/opaque previews stay unchanged unless explicitly enabled in Project Settings; new hulls default to enabled.',
                'Hidden Qt Save actions do not certify desktop preview browsers or hardware power loss.'],
        free_bytes_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},audit_script_sha256=digest(Path(__file__)))
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(counts=result['counts'],verified_files=len(files),receipt=str(a.output)),ensure_ascii=False))

if __name__=='__main__':main()
