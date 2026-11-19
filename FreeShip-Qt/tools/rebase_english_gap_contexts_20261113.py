"""Materialize the reviewed English-gap decisions over the fixed source capture.

Original model receipts remain in the base revision. This is an explicit offline
source/terminology delta, not another model pass or a blanket new semantic audit.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import shutil

import review_english_contexts as tool


def export_generated_captions(out, decisions, frozen):
    """Finite composed runtime labels are separate from literal use-site counts."""
    rows = []
    for item in decisions.get('generated_captions', []):
        source = item['source_text']
        rows.append(dict(
            id='qt.generated.'+tool.digest(source.encode())[:24],
            source_language='en', text=item['canonical_en'],
            context=item['context_en'], role=item['role'],
            source_key=source, evidence_file=item['evidence_file'],
            dependency_files={item['evidence_file']: tool.digest((frozen/item['evidence_file']).read_bytes())},
            provenance='Finite composed runtime caption; supplemental to literal use-site inventory.',
        ))
    (out/'generated_translation_units.jsonl').write_text(
        ''.join(json.dumps(row, ensure_ascii=False)+'\n' for row in rows), encoding='utf8')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    project=tool.QT;record=tool.load(project/'docs/migration/ENGLISH-GAPS-FROZEN-SOURCE-20261113.json')
    frozen=Path(record['source']);previous=project.parent/'翻译工作档案'/'TranslationContext_Qt_20261113_verified';out=a.output.resolve()
    decisions=tool.load(project/'docs/migration/ENGLISH-GAPS-DECISIONS-20261113.json')
    for relative,expected in record['source_sha256'].items():
        if tool.digest((frozen/relative).read_bytes())!=expected:raise ValueError('Fixed source changed')
    (frozen/'docs/migration').mkdir(parents=True,exist_ok=True)
    shutil.copy2(project/'docs/migration/UI-COPY-LEDGER.json',frozen/'docs/migration/UI-COPY-LEDGER.json')
    tool.QT=frozen
    tool.prepare(out,previous)
    manifest=tool.load(out/'manifest.json');manifest.update(writer='inherited_revision',reviewer='repository_source_delta_review',review_mode='offline_english_gap_resolution',new_model_calls=0,frozen_source=str(frozen),final_records_include_refinements=True)
    tool.write(out/'manifest.json',manifest)
    evidence=tool.load(out/'evidence.json');english=tool.load(frozen/'src/app/languages/en.json')['messages']
    old=[json.loads(s) for s in (previous/'contexts.jsonl').read_text('utf8').splitlines()]
    by_id={r['candidate_id']:r for r in old};by_text=defaultdict(list)
    for row in old:by_text[row['source']['file'],row['source_text']].append(row)
    resolved={r['source_text']:r['canonical_en'] for r in decisions['decisions']}
    provenance=dict(previous_revision=str(previous),previous_contexts_sha256=tool.digest((previous/'contexts.jsonl').read_bytes()),
        source_delta_sha256=tool.digest((project/'docs/migration/ENGLISH-GAPS-DECISIONS-20261113.json').read_bytes()),
        acceptance='English wording/context delta over the fixed source; no new model pass or individual full semantic re-review.')
    overrides={}
    for packet_path in sorted((out/'packets').glob('*.json')):
        packet=tool.load(packet_path);result=[]
        for entry in packet['entries']:
            site_results=[]
            for site in entry['sites']:
                source=entry['source_text'];file=entry['file'];prior=by_id.get(site['candidate_id'])
                if prior is None:
                    candidates=by_text.get((file,source),[])
                    prior=candidates[0] if candidates else None
                if prior is None and tool.CJK.search(source):raise ValueError('New text needs a source context: '+source)
                en=prior['canonical_en'] if prior else source
                ctx=prior['mt_context_en'] if prior else 'Internal metadata, resource or animation identifier; not natural-language UI copy.'
                role=prior['role'] if prior else 'technical';status=prior['status'] if prior else 'technical'
                if prior is None and re.search(r'[A-Za-z]{3}\s+[A-Za-z]{3}',source):raise ValueError('New English prose needs review: '+source)
                if source in resolved:en=resolved[source]
                if source in decisions.get('feedback_messages',{}):en=decisions['feedback_messages'][source]
                scope=None
                if source=='计算' and file.endswith('studio_actions.cpp'):scope='menu';ctx='Heading of the calculation-tools menu. Opening the menu does not itself run a calculation.';role='heading'
                if source=='图层' and file.endswith('studio_actions.cpp'):scope='collection';ctx='Heading of the layer-management menu, referring to the collection of model layers.';role='heading'
                if source=='关闭' and (file.endswith('hull_view.cpp') or site['locator'].endswith('/rscoordinateaxisoff')):scope='visibility';ctx='Off option for coordinate-label visibility. It hides labels and does not close a window.';role='state'
                if source=='计算' and site['locator'].endswith('/rsstudiocalculations'):scope='menu'
                if source=='图层' and site['locator'].endswith('/rsstudiolayers'):scope='collection'
                if source=='选择' and file.endswith('modeling_actions.cpp') and (
                        prior['role']=='action' or 'bindUiText' in site['locator'] or 'addButton' in site['locator']):
                    scope='action';ctx='Button that selects the points of the highlighted point group.';role='action'
                if source=='提示' and file.endswith('performance_dialog.cpp'):scope='warnings';ctx='Report section heading for calculation warning messages.';role='heading'
                current_line=(frozen/file).read_text('utf8').splitlines()[(site.get('line') or 1)-1]
                if source=='快捷方式' and file.endswith('shortcuts_dialog.h') and (
                        'column' in ctx.lower() or 'setHorizontalHeaderLabels' in current_line):scope='column';ctx='Shortcut-reference table column containing the key combination.';role='heading'
                if source=='横向受风投影面积 [m2]':
                    if '/wind/fields/2/label' in site['locator']:scope='performance.wind.Edit4.label'
                    elif '/wind/fields/4/label' in site['locator']:scope='performance.wind.Edit6.label'
                if scope:en=english[scope+'\x04'+source]
                if source in resolved and status not in ('reference_only','technical','review'):status='source_reviewed'
                refs=[site['evidence_ref']]+site.get('additional_evidence_refs',[])
                gaps=prior['gaps_en'] if status=='review' else []
                if en is not None:en=re.match(r'^\s*',source)[0]+en.strip()+re.search(r'\s*$',source)[0]
                verdict='technical' if status=='technical' else 'insufficient' if status=='review' else 'revise'
                site_results.append(dict(candidate_id=site['candidate_id'],canonical_en=en,context_en=ctx,
                    role=role,verdict=verdict,gaps_en=gaps,evidence_refs=refs,
                    disposition='technical' if status=='technical' else 'insufficient' if status=='review' else 'ui'))
                # Preserve per-use reference classification and exact previously
                # investigated decimal/prose exceptions without blessing a new pair.
                if status in ('reference_only','review','source_reviewed') or prior and prior.get('format_contract'):
                    support=dict(id='qt.site.'+site['candidate_id'][:24],source_text=source,
                        source=dict(file=frozen.name+'/'+file,sha256=manifest['source_files'][file],line=site.get('line') or 1),
                        evidence=[],translation_action='reference_only' if status=='reference_only' else 'hold_technical_passage' if status=='review' else 'use_recommendation',
                        mt_context_en=ctx,recommended_en=en,missing_evidence=gaps,
                        findings_en='Recorded English terminology/context resolution over an unchanged original source string; original review stays in the base revision.')
                    decision=dict(candidate_id=site['candidate_id'],source_text=source,canonical_en=en,context_en=ctx,
                        role=role,verdict=verdict,gaps_en=gaps,evidence_refs=refs,
                        dependency_files={evidence[r]['file']:evidence[r]['file_sha256'] for r in refs},
                        review_note_en=support['findings_en'],source_review_record=support)
                    if scope:decision['catalog_context']=scope
                    issues=tool.protected_issues(source,en) if en is not None and status not in ('reference_only','technical','review') else []
                    if issues:
                        if not prior or not prior.get('format_contract') or not set(issues)<={'A numeric source literal was omitted or changed.','Line-break count differs from source.'}:raise ValueError('New format exception requires source review')
                        decision['format_contract']=dict(source_sha256=tool.digest(source.encode()),target_sha256=tool.digest(en.encode()),
                            exceptions=issues,reason='Retained exact reviewed numeric-table/prose transformation.',
                            review_record_sha256=tool.digest(json.dumps(support,ensure_ascii=False,sort_keys=True).encode()))
                    overrides[site['candidate_id']]=decision
                    capture=out/'source_review_sources'/support['source']['file'];capture.parent.mkdir(parents=True,exist_ok=True)
                    if not capture.exists():capture.write_bytes((frozen/file).read_bytes())
            result.append(dict(id=entry['id'],sites=site_results))
        receipt=dict(packet_sha256=tool.digest(packet_path.read_bytes()),result=dict(entries=result),usage={},receipt_type='offline_source_delta',source_delta_provenance=provenance)
        for stage in ('draft','review'):tool.write(out/stage/packet_path.name,receipt)
    tool.write(out/'assistant_decisions.json',list(overrides.values()))
    tool.write(out/'source_delta_review.json',dict(**provenance,review_kind='english_gap_resolution',frozen_record=record,
        decisions_file=str(project/'docs/migration/ENGLISH-GAPS-DECISIONS-20261113.json'),translation_api_calls=0))
    tool.export(out)
    export_generated_captions(out, decisions, frozen)


if __name__=='__main__':main()
