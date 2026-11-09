"""Audit storage retirement and write a compact receipt; not a product test.

Keeps historical build manifests and annotates directories with retired binaries.
Does not delete files or change application state.
"""
import argparse
import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('plan','storage-test','runtime-test','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    plan=read(a.plan);result=read(Path(str(a.plan)+'.result.json'))
    assert result['status']=='complete' and result['plan_sha256'].lower()==digest(a.plan)
    assert result['deleted_files']==len(plan['files']) and result['logical_bytes']==sum(e['bytes'] for e in plan['files'])
    installed=q.QT/'dist';manifest=read(installed/'build-manifest.json')
    assert all(digest(installed/name)==h for name,h in manifest['binaries_sha256'].items())
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==q.current_sources(q.QT)
    source=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert source==read(q.QT/'docs/migration/BASELINE.json')['legacy_source_sha256']
    assert digest(installed/'build-manifest.json')==read(ROOT/'maintenance/hydrodynamics-installed-20261104.json')['package_manifest_sha256']
    # The accepted pilot is a live regression input, not just a historical link.
    frozen=[]
    for model in q.DEFAULT_MODELS:
        folder=q.frozen_folder(q.DEFAULT_BASELINE,digest(model))
        for variant in ('original','move-point'):
            for name in ('scene.fslp','reference.dxf'):
                path=folder/variant/name
                frozen.append(dict(path=str(path),sha256=digest(path)))
    reports={}
    for label,path in [('storage',a.storage_test),('installed_viewport',a.runtime_test)]:
        report=read(path)
        assert report['counts']['fail']==report['counts']['error']==0
        reports[label]=dict(path=str(path.resolve()),sha256=digest(path),counts=report['counts'])
    parent=a.runtime_test.parent.parent
    retention=read(parent/'artifact-retention.json')
    assert retention['status']=='complete' and retention['removed_files']>0
    selected=read(parent/'storage-location.json')
    assert selected['free_bytes']==max(v['free_bytes'] for v in selected['candidates'])
    runtime=read(a.runtime_test)
    for file,h in runtime.get('test_sources_sha256',{}).items():
        assert digest(ROOT/file)==h
    # Preserve the original report bytes: cleanup is recorded separately.
    previous=read(ROOT/'maintenance/hydrodynamics-installed-20261104.json')
    for report in previous['reports'].values():
        assert digest(Path(report['path']))==report['sha256']
    retired=defaultdict(lambda:dict(files=0,bytes=0))
    for entry in plan['files']:
        if entry['reason']!='superseded-compiled-build':continue
        path=Path(entry['path']);root=next(Path(r) for r in plan['roots'] if Path(r) in path.parents)
        folder=root/path.relative_to(root).parts[0]
        retired[str(folder)]['files']+=1;retired[str(folder)]['bytes']+=entry['bytes']
    for folder,info in retired.items():
        (Path(folder)/'RETIRED-BINARIES.txt').write_text(
            '2026-11-04: Superseded compiled files retired after verification.\n'
            'This historical build is no longer a runnable package. Source copies, manifests, reports and images remain.\n'
            'Current program: '+str(installed)+'\n'
            'Retired files: '+str(info['files'])+'; logical bytes: '+str(info['bytes'])+'\n'
            'Plan: '+str(a.plan.resolve())+'\n',encoding='utf8')
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),scope='Test storage maintenance; no product feature migration in this change',
                status='complete',actual_free_delta=result['actual_free_delta'],logical_bytes=result['logical_bytes'],
                removed_files=result['deleted_files'],plan=dict(path=str(a.plan.resolve()),sha256=digest(a.plan)),
                result=dict(path=str(Path(str(a.plan)+'.result.json').resolve()),sha256=digest(Path(str(a.plan)+'.result.json'))),
                reports=reports,source_sha256=source,installed_manifest_sha256=digest(installed/'build-manifest.json'),
                frozen_baselines=frozen,retired_builds=dict(retired),
                new_run_retention=retention,default_output_selection=selected,
                tool_sources_sha256={n:digest(ROOT/n) for n in ('run_tests.py','test_artifacts.py','test_storage.py','output-location.json','maintenance/storage_inventory.py','maintenance/plan_storage_cleanup.py','maintenance/apply_storage_cleanup.ps1','maintenance/record_storage_cleanup.py')},
                limitations=['Free-space delta is measured and can include unrelated concurrent writes; logical removed bytes are exact.',
                             'Older compiled runtimes require rebuilding. Their historical reports are not changed into current validation.',
                             'Desktop acceptance remains outside hidden Qt tests.'])
    a.output.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output),reports=reports,actual_reclaimed_gib={k:round(v/2**30,2) for k,v in result['actual_free_delta'].items()}),ensure_ascii=False))


if __name__=='__main__':main()
