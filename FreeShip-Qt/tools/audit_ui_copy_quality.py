"""Offline gate for role-aware UI copy review and observed delivery surfaces.

The gate detects missing/stale judgments and runtime evidence. It cannot decide
linguistic naturalness from a character count or replace the recorded reviewer.
"""
import argparse,hashlib,json
from pathlib import Path
REQUIRED=('semantic_precision','naturalness','concision','group_consistency')
POLICY_VERSION=1
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fingerprint(row):
    value={k:row.get(k) for k in ('locale','source_key','canonical_en','context_en','ui_role','source_sha256','group_captions','ui_copy_policy_version')}
    return hashlib.sha256(json.dumps(value,ensure_ascii=False,sort_keys=True).encode('utf8')).hexdigest()
def evaluate(records,source_root,runtime_report=None):
    findings=[]
    for row in records:
        identity=dict(locale=row['locale'],source_key=row['source_key'])
        if row.get('ui_copy_policy_version')!=POLICY_VERSION:findings.append(identity|dict(kind='copy_policy_missing_or_changed'))
        if row.get('review_input_sha256')!=fingerprint(row):findings.append(identity|dict(kind='stale_review_input'))
        criteria=row.get('ui_copy_review',{})
        for name in REQUIRED:
            decision=criteria.get(name,{})
            if decision.get('verdict')!='pass' or not decision.get('reason_en'):findings.append(identity|dict(kind='missing_or_unresolved_copy_judgment',criterion=name))
        if not row.get('context_en') or not row.get('source_sha256'):findings.append(identity|dict(kind='missing_source_context_binding'))
        for name,h in row.get('source_sha256',{}).items():
            path=source_root/name
            if not path.exists() or sha(path)!=h:findings.append(identity|dict(kind='stale_source',file=name))
    if runtime_report is None:findings.append(dict(kind='actual_title_surface_not_observed'))
    else:
        counts=runtime_report.get('counts',{})
        if counts.get('fail',0) or counts.get('error',0):findings.append(dict(kind='runtime_validation_failed',counts=counts))
        observed={(row['ui_language'],v['studio'],v['projection'],v['painted_title']) for row in runtime_report.get('observations',[]) if row.get('operation')=='observe' for v in row.get('viewport_titles',[])}
        modes={'船体横剖面型线视图':[(True,2)],'纵剖线图':[(True,0)],'侧视图':[(False,0)],'俯视图':[(True,1),(False,1)],'正视图':[(False,2)],'透视图':[(True,3),(False,3)]}
        for row in records:
            if row['source_key'] not in modes:findings.append(dict(kind='runtime_surface_not_registered',locale=row['locale'],source_key=row['source_key']))
            for mode,projection in modes.get(row['source_key'],[]):
                if (row['locale'],mode,projection,row['final']) not in observed:findings.append(dict(kind='actual_title_translation_not_observed',locale=row['locale'],source_key=row['source_key'],studio=mode))
    return dict(status='review' if findings else 'pass',review_records=len(records),findings=findings,boundary='Presence and source/runtime binding of reviewer judgments; no automatic linguistic or independent expert certification.')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reviews',type=Path,required=True);p.add_argument('--source-root',type=Path,required=True);p.add_argument('--runtime-report',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    records=[json.loads(s) for s in a.reviews.read_text('utf8').splitlines() if s.strip()];runtime=json.loads(a.runtime_report.read_text('utf8')) if a.runtime_report else None
    result=evaluate(records,a.source_root,runtime);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n','utf8');print(json.dumps(result,ensure_ascii=False));return 2 if result['findings'] else 0
if __name__=='__main__':raise SystemExit(main())
