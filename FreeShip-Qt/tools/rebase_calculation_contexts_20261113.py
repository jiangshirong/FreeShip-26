"""Offline context update for the explicitly reviewed calculation-source delta.

Not a general cache relaxation. Original model receipts remain in the parent
revision. New receipts identify a repository source-delta review, never an API
response. Changed claims are reauthored; retained decisions have explicit origin.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

import review_english_contexts as tool
from sync_calculation_english_20261113 import at_pointer,table_at


def full_leaves(value,pointer=''):
    if isinstance(value,dict):
        for k,v in value.items():
            yield from full_leaves(v,pointer+'/'+k.replace('~','~0').replace('/','~1'))
    elif isinstance(value,list):
        for i,v in enumerate(value):
            yield from full_leaves(v,pointer+'/'+str(i))
    else:
        yield pointer,value


def check_delta(previous):
    manifest=tool.load(previous/'manifest.json')
    live=tool.inventory(tool.QT)
    changed={f for f in manifest['source_files'].keys()|live['files'].keys()
             if manifest['source_files'].get(f)!=live['files'].get(f)}
    expected={'src/document/resistance_tables.h','src/document/propulsion_tables.h',
              'src/document/resistance_holtrop.inc','src/app/performance_workflow.cpp',
              'src/app/performance_dialog.cpp','src/app/calculation_plot.cpp'}
    if live['errors'] or changed!=expected:
        raise ValueError('Unreviewed files in the source delta: '+str(changed^expected))
    delta=tool.load(tool.QT/'docs/migration/CALCULATION-COPY-CHANGES-20261113.json')
    actual=[]
    for f,h in delta['source_sha256'].items():
        if live['files'][f]!=h:
            raise ValueError('Table changed after the reviewed delta')
        before=dict(full_leaves(table_at(previous/'source'/f)))
        after=dict(full_leaves(table_at(tool.QT/f)))
        actual.extend((f,p,before.get(p),after.get(p)) for p in before.keys()|after.keys()
                      if before.get(p)!=after.get(p))
    declared=[(r['file'],r['pointer'],r['old'],r['new']) for r in delta['changes']]
    if Counter(actual)!=Counter(declared):
        raise ValueError('The declared JSON diff does not cover the complete table delta')
    f='src/document/resistance_holtrop.inc'
    before=(previous/'source'/f).read_text('utf-8')
    after=(tool.QT/f).read_text('utf-8')
    replacements=[('keel_chord = fgetkeelchordlength() * foot;','keel_chord = fgetkeelchordlength() * foot * foot;'),
        ('rudder_chord = fgetrudderchordlength() * foot;','rudder_chord = fgetrudderchordlength() * foot * foot;'),
        ('rn_rudder = convertedspeed * fgetlwl() / waterviscosity;','rn_rudder = convertedspeed * lengthwaterline / waterviscosity;')]
    for old,new in replacements:
        if before.count(old)!=1:
            raise ValueError('Unexpected Holtrop calculation anchor')
        before=before.replace(old,new)
    if before!=after:
        raise ValueError('Unreviewed calculation change')
    f='src/app/performance_workflow.cpp'
    before=(previous/'source'/f).read_text('utf-8')
    after=(tool.QT/f).read_text('utf-8')
    before=before.replace('#include "performance_dialog.h"','#include "performance_dialog.h"\n#include "ui_language.h"')
    before=before.replace('item["label_fits"]=label->height()>=label->heightForWidth(label->width());break;',
        'item["label_fits"]=label->height()>=label->heightForWidth(label->width());item["label_text"]=label->text();break;')
    needle='    performanceRequest_=QJsonDocument::fromJson(f.readAll()).object();'
    before=before.replace(needle,needle+'\n    if(performanceRequest_.contains("language")){\n'
        '        const auto error=setUiLanguage(performanceRequest_["language"].toString());\n'
        '        if(!error.isEmpty()){finishWorkflow(error);return;}\n    }')
    before=before.replace('combo->currentText()==choices[index].toString()',
        'combo->currentText()==uiText(choices[index].toString())')
    if before!=after:
        raise ValueError('Unreviewed performance-observation change')
    route_replacements={
        'src/app/performance_dialog.cpp':[
            ('setWindowTitle(schema["title"].toString())','setWindowTitle(uiText(schema["title"].toString()))'),
            ('new QGroupBox(group)','new QGroupBox(uiText(group))'),
            ('n->addItem(c.toString())','n->addItem(uiText(c.toString()))'),
            ('auto title=f["label"].toString()','auto title=uiText(f["label"].toString())'),
            ('title+=" ("+f["unit"].toString()+")"','title+=" ("+uiText(f["unit"].toString())+")"'),
            ('series_->addItem(s.toObject()["name"].toString())','series_->addItem(uiText(s.toObject()["name"].toString()))')],
        'src/app/calculation_plot.cpp':[
            ('c.name=s["name"].toString().trimmed()','c.name=uiText(s["name"].toString()).trimmed()'),
            ('c.xTitle=s["x_title"].toString()','c.xTitle=uiText(s["x_title"].toString())'),
            ('c.yTitle=s["y_title"].toString()','c.yTitle=uiText(s["y_title"].toString())')]}
    for file,replacements in route_replacements.items():
        before=(previous/'source'/file).read_text('utf-8')
        after=(tool.QT/file).read_text('utf-8')
        for old,new in replacements:
            if before.count(old)!=1:
                raise ValueError('Unexpected translation-route anchor: '+file)
            before=before.replace(old,new)
        if before!=after:
            raise ValueError('Unreviewed translation-route change: '+file)
    return delta,dict(before=manifest['source_files'],after=live['files'],changed_files=sorted(changed))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--previous',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();previous=a.previous.resolve();out=a.output.resolve()
    delta,hashes=check_delta(previous)
    old_records={r['candidate_id']:r for r in (json.loads(s) for s in (previous/'contexts.jsonl').read_text('utf8').splitlines())}
    old_technical={(r['source']['file'],r['source_text']) for r in old_records.values() if r['status']=='technical'}
    tool.prepare(out,previous)
    english=tool.load(tool.QT/'src/app/languages/en.json')['messages']
    evidence=tool.load(out/'evidence.json');manifest=tool.load(out/'manifest.json')
    manifest.update(writer='inherited_revision',reviewer='repository_source_delta_review',
        review_mode='offline_source_delta',new_model_calls=0)
    tool.write(out/'manifest.json',manifest)
    changes={(r['file'],r['pointer']):r for r in delta['changes']}
    tables={f:table_at(tool.QT/f) for f in delta['source_sha256']}
    decisions={}
    provenance=dict(previous_revision=str(previous),previous_contexts_sha256=tool.digest((previous/'contexts.jsonl').read_bytes()),
        source_delta_sha256=tool.digest((tool.QT/'docs/migration/CALCULATION-COPY-CHANGES-20261113.json').read_bytes()),
        acceptance='Explicit source-delta review, not a new model pass or runtime acceptance.')
    disposition_counts=Counter()
    for packet_path in sorted((out/'packets').glob('*.json')):
        packet=tool.load(packet_path);result=[]
        for entry in packet['entries']:
            results=[]
            for site in entry['sites']:
                prior=old_records.get(site['candidate_id'])
                source=entry['source_text'];file=entry['file']
                pointer='/'+site['locator'].split('/',1)[1] if file in tool.LIVE_TABLES else None
                changed=(file,pointer) in changes
                refs=[site['evidence_ref']]+site.get('additional_evidence_refs',[])
                role=prior['role'] if prior else 'technical'
                en=prior['canonical_en'] if prior else source
                ctx=prior['mt_context_en'] if prior else 'Internal schema, observation or resource identifier. It is not a natural-language translation unit.'
                status=prior['status'] if prior else 'technical'
                gaps=prior['gaps_en'] if status=='review' else []
                if changed:
                    en=english.get(source,source if not tool.CJK.search(source) else None)
                    if en is None:
                        raise ValueError('Missing frozen English mapping: '+site['locator'])
                definition=json.loads(evidence[site['evidence_ref']]['excerpt']) if (
                    evidence[site['evidence_ref']].get('symbol')=='Parsed calculator table definition') else {}
                parent=definition.get('surrounding_definition',{})
                area=isinstance(parent,dict) and parent.get('group')=='附体湿表面积' and '/schema/fields/' in (pointer or '')
                if area and pointer.endswith('/label'):
                    en=english.get(source,en)
                    method=pointer.split('/')[1]
                    ctx=f'Label for the wetted surface area of {en.lower()} in the {method} resistance parameter form. '
                    ctx+='The value is an area in m², not an appendage count or angle. '
                    if method in ('hollenbach','oortmerssen','fung'):
                        ctx+='These inherited appendage fields are disabled when the native solver is selected.'
                    elif method=='holtrop':
                        ctx+='Holtrop currently accepts metric inputs only; any angle in the caption identifies a fixed shaft category.'
                    else:
                        ctx+='Availability follows the selected calculation method.'
                    role='field';status='source_reviewed';gaps=[]
                elif area and pointer.endswith('/group'):
                    en=english[source];ctx='Heading grouping appendage wetted-surface-area inputs in the selected resistance method. These inputs are areas, not counts; field availability follows the selected solver.'
                    role='heading';status='source_reviewed';gaps=[]
                elif area and pointer.endswith('/unit'):
                    en=source;ctx='Area unit symbol accompanying the appendage wetted-surface-area value. Keep the SI symbol; supported native imperial modes use ft² instead.'
                    role='unit';status='source_reviewed';gaps=[]
                elif pointer and '/controls/' in pointer and pointer.endswith('/Caption'):
                    method,key=pointer.split('/')[1:4:2]
                    fields=tables.get(file,{}).get(method,{}).get('schema',{}).get('fields',[])
                    if any(f.get('source_label','').lower()==key.lower() for f in fields):
                        status='reference_only'
                        ctx='Inherited control caption overridden by the current parameter-schema label during initialization. Retain this particular metadata use as reference; the schema supplies the visible field label.'
                        if tool.CJK.search(source):en=english.get(source,en)
                if changed and '运动黏度' in source:
                    ctx='Label for water kinematic viscosity used in the resistance calculation. The numeric input is scaled by 10⁻⁶; the adjoining unit is 10⁻⁶ m²/s, or 10⁻⁶ ft²/s in supported imperial modes. This is not dynamic viscosity.'
                    status='source_reviewed';gaps=[]
                if changed and pointer and pointer.endswith('/rs_ppd___'):
                    ctx='Report prefix for the dimensionless propeller pitch-to-diameter ratio P/D. A numeric value is appended; preserve the equals sign and spacing.'
                if changed and pointer and pointer.endswith('/rs_aae___'):
                    ctx='Report prefix for the dimensionless expanded blade-area ratio Ae/A0, where A0 is the propeller disc area. A numeric value is appended; preserve the equals sign and spacing.'
                if changed and pointer and pointer.endswith('/name'):
                    role='label'
                if changed and status not in ('reference_only','technical','review'):
                    status='source_reviewed'
                if prior is None and not (area or changed) and not (
                        (file=='src/app/performance_workflow.cpp' and re.fullmatch(r'[A-Za-z_./]+',source))
                        or (file,source) in old_technical):
                    raise ValueError('New use site needs explicit review: '+site['locator'])
                # Fresh per-site decisions retain the original investigation as
                # history, but never silently replace its recorded fingerprints.
                if status in ('source_reviewed','reference_only','review') or changed or area:
                    record=dict(id='qt.site.'+site['candidate_id'][:24],source_text=source,
                        source=dict(file='FreeShip-Qt/'+file,line=site.get('line') or 1,
                            locator=site['locator'],sha256=manifest['source_files'][file]),
                        evidence=[dict(file='FreeShip-Qt/'+evidence[ref]['file'],line=1,
                            sha256=evidence[ref]['file_sha256']) for ref in refs],
                        translation_action='reference_only' if status=='reference_only' else 'hold_technical_passage' if status=='review' else 'use_recommendation',
                        recommended_en=en,mt_context_en=ctx,missing_evidence=gaps,
                        findings_zh='Explicit calculation source delta reviewed against the previous site, current schema, backend and frozen English synchronization receipt.',
                        prior_review_status=prior['status'] if prior else None,
                        original_investigation=prior.get('source_review_record') if prior else None)
                    if en is not None:
                        en=re.match(r'^\s*',source)[0]+en.strip()+re.search(r'\s*$',source)[0]
                        record['recommended_en']=en
                    decision=dict(candidate_id=site['candidate_id'],source_text=source,canonical_en=en,
                        context_en=ctx,role=role,verdict='insufficient' if status=='review' else 'technical' if status=='technical' else 'revise',
                        gaps_en=gaps,dependency_files={evidence[ref]['file']:evidence[ref]['file_sha256'] for ref in refs},
                        evidence_refs=refs,review_note_en=record['findings_zh'],source_review_record=record)
                    if en is not None and status not in ('review','reference_only','technical'):
                        issues=tool.protected_issues(source,en)
                        if issues:
                            if not set(issues)<={'A numeric source literal was omitted or changed.','Line-break count differs from source.'}:
                                raise ValueError('Unresolved format contract: '+site['locator']+': '+str(issues))
                            if not prior or (not prior.get('format_contract') and not changed):
                                raise ValueError('A new format exception needs source review: '+site['locator'])
                            decision['format_contract']=dict(source_sha256=tool.digest(source.encode()),target_sha256=tool.digest(en.encode()),
                                exceptions=issues,reason='Reviewed decimal table, source-deduplicated prose or reflowed technical explanation in the explicit source delta.',
                                review_record_sha256=tool.digest(json.dumps(record,ensure_ascii=False,sort_keys=True).encode()))
                    decisions[site['candidate_id']]=decision
                    for e in [record['source']]+record['evidence']:
                        capture=out/'source_review_sources'/e['file'];capture.parent.mkdir(parents=True,exist_ok=True)
                        if not capture.exists():capture.write_bytes(tool.review_evidence_path(e['file']).read_bytes())
                verdict='technical' if status=='technical' else 'insufficient' if status=='review' else 'revise'
                results.append(dict(candidate_id=site['candidate_id'],canonical_en=en,context_en=ctx,
                    role=role,evidence_refs=refs,gaps_en=gaps,verdict=verdict,
                    disposition='technical' if status=='technical' else 'insufficient' if status=='review' else 'ui'))
                disposition_counts[status]+=1
            result.append(dict(id=entry['id'],sites=results))
        receipt=dict(packet_sha256=tool.digest(packet_path.read_bytes()),result=dict(entries=result),usage={},
            receipt_type='offline_source_delta',source_delta_provenance=provenance)
        for stage in ('draft','review'):
            tool.write(out/stage/packet_path.name,receipt)
    tool.write(out/'assistant_decisions.json',list(decisions.values()))
    tool.write(out/'source_delta_review.json',dict(**provenance,hashes=hashes,
        source_locations=len(delta['changes']),dispositions=disposition_counts,translation_api_calls=0))
    tool.export(out)


if __name__=='__main__':
    main()
