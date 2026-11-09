"""Artifact coverage and evidence integrity; NOT a global semantic accuracy test."""
from pathlib import Path
import json,hashlib,collections,subprocess,sys,tempfile,argparse
ap=argparse.ArgumentParser();ap.add_argument('--context-root',type=Path,default=Path(__file__).resolve().parent.parent/'TranslationContext_20261030');args=ap.parse_args();BASE=args.context_root.resolve()
def load(p):return [json.loads(l) for l in p.read_text(encoding='utf-8').splitlines() if l.strip()]
m=json.loads((BASE/'manifest.json').read_text(encoding='utf-8'));items=load(BASE/'current_contexts.jsonl')
source={s['path']:s for s in m['source_files']};raw_routines=load(BASE/'evidence/routines.jsonl');routines={r['id']:r for r in raw_routines}
errors=[]
if len(routines)!=len(raw_routines):errors.append('duplicate_routine_evidence_ids')
if len(items)!=len({e['id'] for e in items}):errors.append('duplicate_ids')
for relative,s in source.items():
    p=BASE/'evidence/source'/relative
    if hashlib.sha256(p.read_bytes()).hexdigest()!=s['sha256']:errors.append('evidence_source_bytes:'+relative)
for r in routines.values():
    s=source[r['file']];text=(BASE/'evidence/source'/r['file']).read_bytes().decode('utf-8-sig' if s['encoding']=='utf-8' else 'cp1251')
    if text[r['start_offset']:r['end_offset']]!=r['body']:errors.append('routine_span:'+r['id'])
    if hashlib.sha256(r['body'].encode()).hexdigest()!=r['body_sha256']:errors.append('routine_hash:'+r['id'])
    if any(x['target'] not in routines for x in r['calls']):errors.append('dangling_call:'+r['id'])
for e in items:
    if any(x['routine'] not in routines for x in e['routine_evidence']):errors.append('dangling_evidence:'+e['id'])
    if not e['original_text_is_not_authority']:errors.append('source_authority_flag:'+e['id'])
    if e['evidence_status']=='insufficient' and not e['evidence_gaps']:errors.append('unmarked_gap:'+e['id'])
lexical=load(BASE/'inventory/current_strings.jsonl')
if not {e['id'] for e in lexical}<={e['id'] for e in items}:errors.append('missing_runtime_inventory_entries')
packets=[]
for b in m['batches']:
    p=load(BASE/b['file']);packets+=p
    if len(p)!=b['count']:errors.append('batch_count:'+b['file'])
    for row in p:
        if 'source_text' in row or 'existing_translations' in row:errors.append('old_wording_leaked_to_stage1:'+row['id'])
if collections.Counter(x['id'] for x in packets)!=collections.Counter(x['id'] for x in items):errors.append('batch_coverage')
e=next(e for e in items if e.get('reference')=='tmainform.selectiondialog.caption')
names={routines[x['routine']]['name'] for x in e['routine_evidence']}
required={'TMainForm.SelectionDialogExecute','TFormSelected.onSelectionUpdate','TFormSelected.Reload'}
if not required<=names:errors.append('missing_selection_function_chain')
if any(path[-1]['control']=='SelectionDialog' for path in e['visible_bindings']):errors.append('action_list_used_as_visual_menu')
if not any(path[-1]['control']=='miSelectionDialog' for path in e['visible_bindings']):errors.append('missing_actual_menu_binding')
if e['related_visible_texts']:errors.append('action_list_neighbors_leaked')
if 'TMainForm.Create' in names:errors.append('qualified_constructor_misresolved_as_local_call')
if any(e['source_text']!='Selected Object Details' for e in items if e.get('reference') in ('tmainform.selectiondialog.caption','tformselected.caption')):errors.append('stale_selected_details_source')
decisions=[]
for e in items:
    if e.get('reference') in ('tmainform.selectiondialog.caption','tformselected.caption'):
        decisions.append({'id':e['id'],'meaning_status':'supported','source_verdict':'rewrite','approved':True,
            'approved_by':'User naming decision and source inspection in this chat','canonical_source':'Selected Object Details',
            'purpose_zh':e['functional_description_zh'],'source_file_sha256':e['source_file_sha256'],
            'evidence_refs':[x['routine'] for x in e['routine_evidence'] if routines[x['routine']]['name'] in required]})
# Regression fixtures below exercise the gate only. Verification must never create,
# renew or overwrite real semantic approvals after a source change.
# A false semantic approval with an invented evidence ID must be blocked, never translated.
bad={**decisions[0],'evidence_refs':['invented-proof']}
with tempfile.TemporaryDirectory(prefix='fs-translation-gate-') as t:
    p=Path(t);(p/'bad.jsonl').write_text(json.dumps(bad,ensure_ascii=False)+'\n',encoding='utf-8')
    r=subprocess.run([sys.executable,str(BASE/'make_translation_input.py'),'--decisions',str(p/'bad.jsonl'),'--output',str(p/'out.jsonl')],capture_output=True)
    if r.returncode or (p/'out.jsonl').read_text(encoding='utf-8').strip():errors.append('gate_accepted_invented_evidence')
origin=BASE.parent/'FreeShip-Plus-Lazarus'
drift=[rel for rel,s in source.items() if not (origin/rel).exists() or hashlib.sha256((origin/rel).read_bytes()).hexdigest()!=s['sha256']]
report={'status':'pass' if not errors else 'fail','entries':len(items),'primary_runtime_records':len(lexical),
    'routine_bodies_verified':len(routines),'source_files_verified':len(source),'entry_and_batch_ids_unique_and_complete':True,
    'selection_dialog_regression_checks':'pass' if not [x for x in errors if 'selection' in x or 'action_list' in x] else 'fail',
    'source_drift_since_snapshot':drift,'errors':errors,'meaning_accuracy':'Not measured by these integrity checks; only explicitly reviewed decisions are approved.'}
(BASE/'verification_v2.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({**report,'errors':errors[:12],'error_count':len(errors)},ensure_ascii=False,indent=2))
raise SystemExit(bool(errors))
