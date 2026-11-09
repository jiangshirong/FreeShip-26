"""Classify full-document failures against the unmodified installed runtime.

Read-only diagnostic, not a pass override: retains every original failing
report and compares native outputs before/after this save-protocol change.
Run before promotion with --results pointing at this delivery's result root.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q
import qt_modeling as modeling
from qt_publication import differences

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--results',type=Path,required=True);a=p.parse_args();root=a.results.resolve(strict=True)
    full=root/'related-regression/qt-document';report=read(full/'report.json')
    installed=q.QT/'dist';candidate=Path('C:/FreeShip26-MultihullTests/runtime')
    old=read(installed/'build-manifest.json');new=read(candidate/'build-manifest.json')
    result={'scope':'Pre-existing full-document failures; never changes a test expectation',
        'installed_service_sha256':q.common.digest_file(installed/'FreeShipDocumentService.exe'),
        'candidate_service_sha256':q.common.digest_file(candidate/'FreeShipDocumentService.exe'),
        'worker_unchanged':q.common.digest_file(installed/'FreeShipNativeLinesplan.exe')==q.common.digest_file(candidate/'FreeShipNativeLinesplan.exe'),
        'changed_product_sources':[n for n in sorted(old['source_sha256'].keys()|new['source_sha256'].keys()) if old['source_sha256'].get(n)!=new['source_sha256'].get(n)],
        'baselines':{},'native_comparisons':0,'native_differences':[]}
    prior_failures=set();prior_errors=[]
    for name in ('construction','lackenby','face-tools'):
        prior=root/('baseline-'+name);current=full/('native-'+name);before=read(prior/'report.json')
        failures=[x for x in before['cases'] if x['status']=='fail'];errors=[x for x in before['cases'] if x['status']=='error']
        prior_failures.update(x['id'] for x in failures);prior_errors+=errors
        result['baselines'][name]={'report':str(prior/'report.json'),'sha256':q.common.digest_file(prior/'report.json'),'counts':before['counts'],'failures':failures,'errors':errors}
        for path in current.rglob('scene.fslp'):
            if path.parent.name!='native':continue
            folder=path.parent.relative_to(current)
            for artifact in ('scene.fslp','reference.dxf','sections.json','surface.json','model.json'):
                left=prior/folder/artifact;right=current/folder/artifact
                if not left.exists() or not right.exists():continue
                equal=modeling.geometry(read(left))==modeling.geometry(read(right)) if artifact=='model.json' else q.common.digest_file(left)==q.common.digest_file(right)
                result['native_comparisons']+=1
                if not equal:result['native_differences'].append(str(right.relative_to(root)))
    failures=[c for c in report['cases'] if c['status']=='fail'];errors=[c for c in report['cases'] if c['status']=='error']
    result['new_failure_ids']=[c['id'] for c in failures if c['id'] not in prior_failures]
    result['full_document_counts']=report['counts'];result['full_document_report']=str(full/'report.json')
    result['full_document_report_sha256']=q.common.digest_file(full/'report.json')
    result['same_stopping_error']=len(errors)==1 and "'alpha_blend'" in errors[0].get('reason','') and any("'alpha_blend'" in c.get('reason','') for c in prior_errors)
    for name,case in [('lack-settings','native-lackenby/004-new-template'),('face-settings','native-face-tools/019-delete-face-slot')]:
        folder=full/case;left=read(folder/'native/model.json');right=read(folder/'oracle/model.json')
        result[name]=differences(left['settings'],right['settings'])
    result['preexisting_confirmed']=not result['new_failure_ids'] and not result['native_differences'] and result['same_stopping_error'] and result['native_comparisons']>100
    path=root/'preexisting-document-audit.json';path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:result[k] for k in ('preexisting_confirmed','native_comparisons','native_differences','new_failure_ids','same_stopping_error','changed_product_sources','lack-settings','face-settings')},ensure_ascii=False))

if __name__=='__main__':main()
