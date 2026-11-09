"""Record verified save-confirmation delivery; no product/test result mutation."""
import argparse
import json
import sys
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))

def preexisting_audit(results,runtime,rollback,document_report):
    """Keep the failing report; independently require the before/after evidence."""
    path=results/'preexisting-document-audit.json';audit=read(path)
    if not audit['preexisting_confirmed'] or audit['new_failure_ids'] or audit['native_differences']:
        raise RuntimeError('Document differences have not been classified')
    if not audit['worker_unchanged'] or not audit['same_stopping_error'] or audit['native_comparisons']<100:
        raise RuntimeError('Incomplete before/after comparison')
    if q.common.digest_file(document_report)!=audit['full_document_report_sha256']:
        raise RuntimeError('Document report changed after audit')
    if q.common.digest_file(runtime/'FreeShipDocumentService.exe')!=audit['candidate_service_sha256']:
        raise RuntimeError('Audited candidate differs from installed service')
    if q.common.digest_file(rollback/'FreeShipDocumentService.exe')!=audit['installed_service_sha256']:
        raise RuntimeError('Audited baseline differs from previous installation')
    if q.common.digest_file(runtime/'FreeShipNativeLinesplan.exe')!=q.common.digest_file(rollback/'FreeShipNativeLinesplan.exe'):
        raise RuntimeError('Geometry worker changed')
    previous_failures=set()
    for baseline in audit['baselines'].values():
        if q.common.digest_file(Path(baseline['report']))!=baseline['sha256']:
            raise RuntimeError('Baseline report changed')
        previous_failures.update(c['id'] for c in read(baseline['report'])['cases'] if c['status']=='fail')
    failures={c['id'] for c in read(document_report)['cases'] if c['status']=='fail'}
    if failures!=previous_failures:raise RuntimeError('Document failures do not match baseline failures')
    return {'path':str(path),'sha256':q.common.digest_file(path),
            'native_comparisons':audit['native_comparisons'],
            'new_failure_ids':audit['new_failure_ids'],'native_differences':audit['native_differences'],
            'changed_product_sources':audit['changed_product_sources'],
            'baseline_reports':audit['baselines']}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--results',type=Path,required=True);a=parser.parse_args()
    results=a.results.resolve(strict=True);runtime=q.QT/'dist';manifest=read(runtime/'build-manifest.json')
    if q.current_sources(q.QT)!={k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}:raise RuntimeError('Installed source mismatch')
    q.validate_test_manifest(manifest)
    if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Installed binary mismatch')
    reports={};rollback=q.QT/'build/dist-before-save-confirmation-20261109'
    for name,relative in [('candidate','candidate-save/qt-save-protection'),('installed','installed-save/qt-save-protection'),
                          ('recovery','related-regression/qt-recovery'),('preview','related-regression/qt-preview-image'),
                          ('background','background-regression'),('document','related-regression/qt-document')]:
        path=results/relative/'report.json';value=read(path)
        if name!='document' and (value['counts']['fail'] or value['counts']['error']):
            raise RuntimeError('Unresolved regression: '+name)
        for file,digest in value.get('test_sources_sha256',{}).items():
            if q.common.digest_file(ROOT/file)!=digest:raise RuntimeError('Changed test source: '+file)
        reports[name]={'path':str(path),'sha256':q.common.digest_file(path),'counts':value['counts']}
    if Path(read(reports['installed']['path'])['runtime']).resolve()!=runtime.resolve():raise RuntimeError('Installed route was not exercised')
    audit=preexisting_audit(results,runtime,rollback,Path(reports['document']['path']))
    receipt={'created_utc':datetime.now(timezone.utc).isoformat(),'scope':'Receipt-bound file save confirmation and idempotent acknowledgements',
        'runtime':str(runtime),'build_manifest_sha256':q.common.digest_file(runtime/'build-manifest.json'),
        'source_sha256':manifest['source_sha256'],'reports':reports,
        'rollback_package':str(rollback),
        'regression_status':'Targeted suites pass; full-document suite retains pre-existing failures and a stopping error.',
        'preexisting_document_audit':audit,
        'diagnostic_sources_sha256':{p.name:q.common.digest_file(p) for p in
            [Path(__file__),ROOT/'maintenance/audit_save_confirmation_baseline.py']},
        'boundaries':['Owned hidden processes and production callbacks, not desktop acceptance.',
                      'The full document suite is not green; cases after its stopping error remain unexecuted.',
                      'Retries handle lost replies; backend process death still uses the existing reopen/recovery flow.',
                      'No durable cross-process undo journal or hardware power-loss guarantee.',
                      'No change to Lazarus, QtPilot, user models or fs launcher.']}
    target=ROOT/'maintenance/save-confirmation-delivery-20261109.json'
    if target.exists():raise RuntimeError('Receipt already exists')
    target.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'reports':{k:v['counts'] for k,v in reports.items()}},ensure_ascii=False))

if __name__=='__main__':main()
