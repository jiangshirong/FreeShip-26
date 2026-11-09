"""Audit the current-editor backend recovery delivery and its retained reports.

Maintenance receipt only; does not build, promote or touch user settings.
Reusable tests are registered as qt-backend-recovery and existing save/recovery
suites. This script verifies the exact tested and installed runtime identities.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    names=('focused','saving','recovery','publication','installed')
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
        for filename in ('FreeShipQt.exe','FreeShipDocumentService.exe','qt_recovery_tests.exe'):
            assert digest(runtime/filename)==digest(installed/filename),(name,filename)
        reports[name]=dict(path=str(path),sha256=digest(path),counts=report['counts'])
        for key in counts:counts[key]+=report['counts'][key]
    assert Path(data['installed']['runtime']).resolve()==installed
    assert len(data['installed']['production_runs'])==4
    for run in data['installed']['production_runs']:
        assert Path(run['executed_path']).resolve()==installed/'FreeShipQt.exe'
        assert run['executable_sha256']==digest(installed/'FreeShipQt.exe')
    for name in ('focused','installed'):
        passed={c.get('id') for c in data[name]['cases'] if c['status']=='pass'}
        for key in ('backend.all-fault-scenarios','backend.first-edit-reply-lost.checkpoint-ahead-of-ui',
                    'backend.save-ack-lost.confirmed-file-survived','backend.stale.unsafe-resume-rejected',
                    'backend.corrupt.rejection-preserves-evidence','backend.changed-source.unsafe-resume-rejected',
                    'backend.stale-save-receipt.unsafe-resume-rejected','current-action.restored-through-action',
                    'recovery-menu.restored-through-action','new-document.restored-through-action',
                    'clean-restart.source-retained'):
            assert key in passed,(name,key)
    earlier=args.focused.resolve().parents[2]/'first/qt-backend-recovery/report.json'
    record=dict(created_utc=datetime.now(timezone.utc).isoformat(),status='partial',
        scope='Resume current editor after actual service death, recover committed edits and confirmed saves, continue editing and saving',
        installed=str(installed),previous=str(q.QT/'build/dist-before-backend-recovery-20261109'),
        package_manifest_sha256=digest(installed/'build-manifest.json'),reports=reports,counts=counts,
        first_run=dict(path=str(earlier),sha256=digest(earlier)) if earlier.is_file() else None,
        audit_script_sha256=digest(Path(__file__)),limitations=[
            'Counts include repeated installed checks, not unique user scenarios.',
            'Only successfully persisted revisions survive; unsubmitted input, old undo history and camera state are not recovered.',
            'Actual desktop input, DPI and hardware power-loss behavior remain untested.'])
    args.output.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(receipt=str(args.output),counts=counts),ensure_ascii=False))


if __name__=='__main__':main()
