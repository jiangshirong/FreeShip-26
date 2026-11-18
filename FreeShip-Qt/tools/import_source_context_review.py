"""Apply a per-use-site source investigation to a new immutable revision.

Offline only. Preserve the previous snapshot, packets and raw model receipts.
The imported report is a reviewed decision record, not an automated semantic oracle.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import shutil

import review_english_contexts as tool


def text_hash(text):
    return tool.digest(text.encode('utf-8'))


def boundary_text(source,english):
    if not isinstance(english,str):
        return english
    return re.match(r'^\s*',source)[0]+english.strip()+re.search(r'\s*$',source)[0]


def integrate(previous,out,results):
    if out.exists():
        raise ValueError('Use a new revision directory')
    rows=[json.loads(s) for s in results.read_text(encoding='utf-8').splitlines() if s.strip()]
    queue=[json.loads(s) for s in (previous/'review_queue.jsonl').read_text(encoding='utf-8').splitlines() if s.strip()]
    sites={r['id']:r for r in queue}
    if len(rows)!=len(sites) or Counter(r['id'] for r in rows)!=Counter(sites.keys()):
        raise ValueError('Investigation must cover each pending id exactly once')
    manifest=tool.load(previous/'manifest.json')
    current=tool.inventory(tool.QT)
    if current['errors'] or current['files']!=manifest['source_files']:
        raise ValueError('Source drift requires a fresh snapshot before importing decisions')
    audit_evidence={}
    owning_refs={s['candidate_id']:s['evidence_ref']
        for packet in (previous/'packets').glob('*.json')
        for entry in tool.load(packet)['entries'] for s in entry['sites']}
    for row in rows:
        site=sites[row['id']]
        if row['source_changed'] or row['source_text']!=site['source_text']:
            raise ValueError('Investigation no longer matches the original use site: '+row['id'])
        if tool.source_review_issues(row):
            raise ValueError('Stale investigation evidence: '+row['id'])
        if row['translation_action'] not in ('use_recommendation','reference_only','hold_technical_passage'):
            raise ValueError('Unknown investigation action')
        if row['translation_action']=='hold_technical_passage' and (
                row['recommended_en'] is not None or not row['missing_evidence']):
            raise ValueError('A held passage must retain an explicit evidence gap')
        for item in [row['source']]+row['evidence']:
            audit_evidence[item['file']]=item['sha256']
    # No original files are edited; even the former English preimage stays in
    # the original revision. The new one starts from the current live catalog.
    shutil.copytree(previous,out)
    applied=out/'english.applied.json'
    if applied.exists():
        applied.unlink()
    tool.write(out/'english.before.json',tool.load(tool.QT/'src/app/languages/en.json'))
    imported=out/'source_review_results.jsonl'
    imported.write_bytes(results.read_bytes())
    original_decisions={r['candidate_id']:r for r in tool.load(out/'assistant_decisions.json')}
    for row in rows:
        site=sites[row['id']]
        english=boundary_text(row['source_text'],row['recommended_en'])
        dependencies=dict(site['dependency_files'])
        for item in [row['source']]+row['evidence']:
            relative=item['file'].removeprefix('FreeShip-Qt/')
            if relative in manifest['source_files']:
                dependencies[relative]=item['sha256']
            captured=out/'source_review_sources'/item['file']
            captured.parent.mkdir(parents=True,exist_ok=True)
            captured.write_bytes(tool.review_evidence_path(item['file']).read_bytes())
        held=row['translation_action']=='hold_technical_passage'
        decision=dict(candidate_id=site['candidate_id'],source_text=row['source_text'],
            canonical_en=english,context_en=row['mt_context_en'],role=site['role'],
            verdict='insufficient' if held else 'revise',
            gaps_en=row['missing_evidence'] if held else [],
            dependency_files=dependencies,evidence_refs=sorted(set(site['evidence_refs'])|
                {owning_refs[site['candidate_id']]}),
            review_note_en='Per-use-site source investigation by the main-logic chat; evidence and remaining limits are retained in source_review_record.',
            source_review_record=row)
        if row['translation_action']=='use_recommendation':
            issues=tool.protected_issues(row['source_text'],english)
            # The investigator inspected these precise decimal tables, duplicate
            # prose and line wraps. Never relax placeholders, units or filters.
            if issues:
                allowed={'A numeric source literal was omitted or changed.',
                         'Line-break count differs from source.'}
                if not set(issues)<=allowed:
                    raise ValueError('Unresolved token/unit contract: '+row['id']+': '+str(issues))
                decision['format_contract']=dict(source_sha256=text_hash(row['source_text']),
                    target_sha256=text_hash(english),exceptions=issues,
                    reason=row['findings_zh'],
                    review_record_sha256=text_hash(json.dumps(row,ensure_ascii=False,sort_keys=True)))
        original_decisions[site['candidate_id']]=decision
    tool.write(out/'assistant_decisions.json',list(original_decisions.values()))
    tool.write(out/'source_review_integration.json',dict(previous_revision=str(previous),
        results_path=str(results),results_sha256=tool.digest(results.read_bytes()),
        actions=Counter(r['translation_action'] for r in rows),records=len(rows),
        evidence_files=audit_evidence,translation_api_calls=0,
        boundary='Source investigation decisions; runtime UI acceptance and scientific method certification remain separate.'))
    tool.export(out)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--previous',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--results',type=Path,required=True)
    a=p.parse_args()
    integrate(a.previous.resolve(),a.output.resolve(),a.results.resolve())


if __name__=='__main__':
    main()
