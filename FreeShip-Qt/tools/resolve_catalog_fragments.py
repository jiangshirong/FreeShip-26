"""Source-bound disposition of the retained fragment catalog queue.

This applies an explicit consumer/data-flow review, not a general C++ reachability
proof. Any changed source, input queue, catalog or newly reachable exact source
invalidates the approval. No global word-fragment composition authorizes use.
"""
import argparse,json,re,sys
from pathlib import Path
QT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import inventory,digest,CJK
from prune_translation_catalogs import plan
from prune_unused_resources import table,domains

def load(p):return json.loads(p.read_text('utf8'))
def rows(p):return [json.loads(s) for s in p.read_text('utf8').splitlines() if s.strip()]
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n','utf8')

def parsed_literal_labels(text):
    """Conservative labels for the actual legacyResult selectors, before context gates.

    Includes all pipe cells and possible numeric-table headers, even when the
    following row is not numeric. Assignment/space fields follow the real gates.
    Completed free-form values remain outside owned-source approval.
    """
    field=re.compile(r'^([^=：:。；;]{1,120})\s*[=：:]\s*(\S.*)$')
    space=re.compile(r'^([^0-9=：:。；;]{1,120}?)\s+([+-]?\d[^。；;]*)$')
    out=set()
    for raw in text.splitlines():
        line=raw.strip();cells=line.split('|')
        if len(cells)>1:out.update(c.strip() for c in cells if c.strip())
        for separator in (r'\s{2,}',r' / '):
            cells=re.split(separator,line)
            if len(cells)>1:out.update(c.strip() for c in cells if c.strip())
        parts=re.split('[；;]',line);parsed=[]
        for part in parts:
            match=field.match(part.strip());assignment=bool(match)
            if len(parts)>1 and not assignment:match=space.match(part.strip())
            if (not match or match[2].startswith('//') or
                (len(parts)>1 and ((assignment and '=' not in part) or match[2].endswith('。')))):
                parsed=[];break
            parsed.append(match[1].strip())
        out.update(parsed)
    return out

def validate_review(review,current,queue,english):
    if current['errors'] or review['source_files']!=current['files']:
        raise ValueError('Consumer/source review is stale; recheck the changed source before deleting.')
    if digest(queue.read_bytes())!=review['queue_sha256']:raise ValueError('Different catalog queue')
    wanted={x['key'] for x in rows(queue)}
    decisions={x['key']:x for x in review['dispositions']}
    if len(decisions)!=len(review['dispositions']) or wanted!=decisions.keys():raise ValueError('Incomplete/duplicate key dispositions')
    exact={x['text'] for x in current['candidates']}
    for key,item in decisions.items():
        if item['action'] not in {'delete','retain','review'}:raise ValueError('Unknown disposition')
        if item['action']=='delete' and key in exact:raise ValueError('Current exact source cannot be removed: '+key)
        if not item.get('reason_en') or CJK.search(item['reason_en']):raise ValueError('Missing English decision reason')
        if item['action']=='retain' and not all(item.get(k) for k in ('canonical_en','mt_context_en','consumer')):raise ValueError('Active entry requires English and functional context')
    return decisions

def resolve(previous,queue,review_path,english_override=None):
    review=load(review_path);current=inventory(QT);english=english_override if english_override is not None else load(QT/'src/app/languages/en.json')['messages']
    decisions=validate_review(review,current,queue,english)
    # Reuse the closed named/numbered table-consumer checks. Unknown selectors
    # or newly introduced non-literal uiText inputs fail before any application.
    conservative=plan(previous)
    table_domains={family:domains(family) for family in ('resistance','propulsion')}
    if any(x['key'] not in english for x in rows(queue)):raise ValueError('Queue is not the current retained set')
    generated={r['source_key'] for r in rows(previous/'generated_translation_units.jsonl')}
    if generated & {k for k,x in decisions.items() if x['action']=='delete'}:raise ValueError('A generated translation is still active')
    # The only untranslated CJK caption in these private dictionaries is the
    # explicitly held whole MH help note. All other captions enter the report
    # translator whole, before values or legacyResult parsing are involved.
    missing=[]
    for family in table_domains:
        data=table(QT/f'src/document/{family}_tables.h')[2]
        for kind in ('strings','numbered'):
            for key,value in data[kind].items():
                if isinstance(value,str) and CJK.search(value) and value not in english and f'{family}.{kind}.{key}\x04{value}' not in english:
                    missing.append([family,kind,key])
    if missing!=[['resistance','strings','rs_resistance_mh_note']]:raise ValueError('Report-caption translation coverage changed: '+repr(missing))
    parser_candidates=[]
    wanted=set(decisions)
    for site in current['candidates']:
        if not site['file'].startswith('src/document/'):continue
        hits=parsed_literal_labels(site['text']) & wanted
        if hits:parser_candidates.append(dict(file=site['file'],locator=site['locator'],text_sha256=site['text_sha256'],keys=sorted(hits)))
    if parser_candidates!=review['legacy_parser_candidate_exclusions']:raise ValueError('Legacy report parser candidates changed')
    catalogs={p.name:dict(sha256=digest(p.read_bytes()),entries=len(load(p)['messages'])) for p in sorted((QT/'src/app/languages').glob('*.json'))}
    return dict(kind='reviewed_catalog_fragment_resolution',previous_revision=str(previous),queue=str(queue),queue_sha256=digest(queue.read_bytes()),review_file=str(review_path),review_sha256=digest(review_path.read_bytes()),source_files=current['files'],catalogs=catalogs,dispositions=review['dispositions'],consumer_routes=review['consumer_routes'],dynamic_lookup_domains=conservative['dynamic_lookup_domains'],legacy_parser_candidate_exclusions=parser_candidates,counts={action:sum(x['action']==action for x in decisions.values()) for action in ('delete','retain','review')},boundary='Owned first-party translation inputs only. Arbitrary user names/paths and external solver values are not software wording. This is a source-bound explicit review, not a general call-graph theorem.')

def apply(record,output):
    if inventory(QT)['files']!=record['source_files']:raise ValueError('Source changed before application')
    for name,expected in record['catalogs'].items():
        if digest((QT/'src/app/languages'/name).read_bytes())!=expected['sha256']:raise ValueError('Catalog changed: '+name)
    backup=(output.parent/(output.stem+'-preimage')).resolve();backup.mkdir(exist_ok=False)
    deleted={r['key'] for r in record['dispositions'] if r['action']=='delete'};changes=[]
    for name in record['catalogs']:
        p=QT/'src/app/languages'/name;data=load(p);removed={k:v for k,v in data['messages'].items() if k in deleted}
        if not removed:continue
        (backup/name).write_bytes(p.read_bytes())
        for key in removed:del data['messages'][key]
        temp=p.with_suffix('.resolution.tmp');write(temp,data);temp.replace(p)
        changes.append(dict(file=name,removed=len(removed),removed_members=removed,after_entries=len(data['messages']),after_sha256=digest(p.read_bytes())))
    record['applied']=dict(backup=str(backup),changes=changes);write(output,record)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--previous',type=Path,required=True);p.add_argument('--queue',type=Path,required=True);p.add_argument('--review',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--apply',action='store_true');a=p.parse_args()
    if a.output.exists():raise ValueError('Use a new immutable receipt')
    record=resolve(a.previous.resolve(),a.queue.resolve(),a.review.resolve());write(a.output,record)
    if a.apply:apply(record,a.output.resolve())
    print(json.dumps(dict(counts=record['counts'],catalogs=len(record['catalogs']),applied=a.apply)))
if __name__=='__main__':main()
