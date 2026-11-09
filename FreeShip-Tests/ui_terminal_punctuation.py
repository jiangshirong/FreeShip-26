"""Read-only UI punctuation inventory; candidates are evidence, not semantic verdicts."""
import argparse,ast,json,collections,re
from pathlib import Path

def entries(path):
    row={};field=None
    for line in path.read_text(encoding='utf-8-sig').splitlines()+['']:
        if not line.strip():
            if row.get('msgid'):yield row
            row={};field=None
        elif line.startswith('#:'):
            row.setdefault('references',[]).extend(line[2:].split())
        elif line.startswith(('msgctxt ', 'msgid ', 'msgstr ')):
            field,raw=line.split(' ',1);row[field]=ast.literal_eval(raw)
        elif line.startswith('"') and field:
            row[field]+=ast.literal_eval(line)

def kind(text):
    t=text.rstrip()
    if t.endswith(('...', '…')):return 'ellipsis'
    if t.endswith('。'):return 'chinese_full_stop'
    if t.endswith('.'):return 'period'
    return None

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent/'FreeShip-Plus-Lazarus');ap.add_argument('--inventory',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    rows=[];summary={}
    for code in ('cn','ru','en','fr','de','es','ja'):
        items=[]
        for e in entries(a.source/'locale'/('FreeShip.'+code+'.po')):
            text=e.get('msgstr') or e['msgid'];k=kind(text)
            if not k:continue
            ref=e.get('msgctxt','')+' '+' '.join(e.get('references',[]))
            surface=bool(re.search(r'\.(caption|hint|text)\b',ref,re.I))
            items.append(dict(language=code,context=e.get('msgctxt',''),references=e.get('references',[]),source=e['msgid'],text=text,ending=k,ui_property=surface))
        rows.extend(items)
        summary[code]={'terminal_sentence_mark_entries':sum(x['ending']!='ellipsis' for x in items),'ui_property_sentence_mark_entries':sum(x['ending']!='ellipsis' and x['ui_property'] for x in items),'ellipsis_entries':sum(x['ending']=='ellipsis' for x in items)}
    runtime=[]
    if a.inventory:
        data=json.loads(a.inventory.read_text(encoding='utf-8'));seen=set()
        for shot in data['snapshots']:
            for x in shot['nodes']:
                text=x.get('text') or x.get('caption','');k=kind(text)
                if not k or k=='ellipsis':continue
                path=x.get('component_path') or (x.get('container_id','')+'/'+x.get('path',''))
                key=(shot['language'],path,x.get('text_field','caption'),text)
                if key in seen:continue
                seen.add(key);runtime.append(dict(language=shot['language'],path=path,text=text,ending=k,visible=x.get('visible_effective',x.get('visible_excluding_hidden_form',False))))
    a.output.mkdir(parents=True,exist_ok=True)
    result=dict(status='review',catalog_summary=summary,catalog_candidates=rows,runtime_candidates=runtime,limitations=['Catalog counts include unused strings and complete sentences; UI property classification uses references','Runtime inventory covers only constructed controls and snapshots, not all dialogs or real desktop visibility','Ellipses are counted separately, not as sentence-ending marks'])
    (a.output/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'catalog_summary':summary,'runtime_unique':len(runtime),'runtime_visible':dict(collections.Counter(x['language'] for x in runtime if x['visible']))},ensure_ascii=False))
if __name__=='__main__':main()
