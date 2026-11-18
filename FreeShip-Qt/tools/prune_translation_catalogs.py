"""Plan/apply removal of retired owned-source translations after source pruning.

Retain exact literals, registered generated keys and any possible substring,
template or literal-composition input. Missing ownership/derivation evidence is
an explicit review item. No deletion based solely on a failed text search.
"""
import argparse
from collections import defaultdict,Counter
import json
from pathlib import Path
import re
import sys
QT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import inventory,cpp_literals,digest,CJK
from resolve_legacy_resource_reachability import call_arguments

# Reviewed non-literal uiText source domains, over the exact captured tree.
# Table/header inputs are owned metadata; legacy report parsing may slice them.
# Live bindings replay previously registered sources, not arbitrary widget data.
DYNAMIC={
 'message':'Complete owned ModelSession failure message, matched exactly before file/model values are appended. No substring or reverse widget matching.',
 'sourceMessage':'Complete owned MainWindow error message; exact catalog lookup only, without rewriting embedded user values.',
 'work':'About contribution description supplied by the adjacent literal rows.',
 'post?"后 AI 时代":"前 AI 时代"':'Finite About-era heading selection.',
 'data["message"].toString()':'Hydrostatics-owned status/error messages.',
 'session_.model() ? "正在更新模型…" : "正在打开船模…"':'Finite loading feedback.',
 'title':'Pipe/space report headers parsed from owned result text.',
 'match.captured(1).trimmed()':'Owned legacy report field label parsed before its value.',
 'v.toVariant().toString()':'Owned performance table column heading, never data cells.',
 'schema["title"].toString()':'Calculator schema title.',
 'group':'Calculator field group heading.',
 'c.toString()':'Calculator parameter choice label.',
 'f["label"].toString(),"performance."+method_+"."+key+".label"':'Owned scoped calculator field label.',
 'f["unit"].toString()':'Owned calculator unit symbol.',
 'cols[j].toString()':'Owned result table column heading.',
 'choices[index].toString()':'Owned parameter choice label.',
 'f["label"].toString(),"performance."+method_+"."+f["key"].toString()+".label"':'Owned scoped report parameter label.',
 'col.toString()':'Owned result column heading.',
 'labels.at(i).toString()':'Owned result row-label metadata.',
 'warning.toString()':'Owned calculation warning; formatted templates retained conservatively.',
 'text':'Plate workflow error text, originating in owned messages.',
 'source':'Shortcut description/scope from registered source properties.',
 'it->source':'Replay of a source previously registered by the language binder.',
 '*it':'Finite standard Qt caption mapping.',
 'source.toUtf8().constData()':'QString overload forwarding the same lookup input.',
 '(QString::fromUtf8(context)+QChar(4)+QString::fromUtf8(source)).toUtf8().constData()':'Scoped forwarding overload.',
 'context+QChar(4)+source':'Scoped forwarding overload.',
 'part.toString()':'Owned metadata fragment; explicit value fragments bypass translation.',
 'object["source"].toString(),object["context"].toString()':'Owned scoped metadata fragment.',
 'metadata[key].toString()':'Owned result metadata fallback; explicit parts take priority.',
}

def load(p):return json.loads(p.read_text('utf8'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')

def plan(previous, english_override=None):
    current=inventory(QT)
    if current['errors']:raise ValueError(current['errors'])
    values={r['text'] for r in current['candidates']};pieces=set(values);dynamic=[]
    for path in (QT/'src').rglob('*'):
        if path.suffix not in ('.cpp','.h','.inc','.hpp','.cc'):continue
        text=path.read_text('utf8');pieces.update(v for _,v,*_ in cpp_literals(text) if v)
        for line,arg in call_arguments(text,'uiText'):
            if re.match(r'^(?:QStringLiteral\()?"(?:\\.|[^"\\])*"\s*(?:\)|,|$)',arg):continue
            if arg not in DYNAMIC:raise ValueError('New dynamic translation input requires source review: '+str(path)+': '+arg)
            dynamic.append(dict(file=path.relative_to(QT).as_posix(),line=line,expression=arg,domain_en=DYNAMIC[arg]))
    pruning=load(QT/'docs/migration/UNUSED-RESOURCES-FINAL-20261114.json')
    retired=defaultdict(list)
    for header in pruning['headers']:
        for row in header['removed']:retired[row['value']].append(dict(file=header['file'],locator=row['locator']))
    ledger=load(QT/'docs/migration/UI-COPY-LEDGER.json')
    for row in ledger['items']:
        if row['file'].startswith('src/') and row['text'] not in values:
            retired[row['text']].append(dict(file=row['file'],provenance='Earlier source-copy ledger; exact old literal no longer present.'))
    generated={r['source_key'] for r in map(json.loads,(previous/'generated_translation_units.jsonl').read_text('utf8').splitlines())}
    required_installer=set()
    for path in (QT/'tools/installer').glob('*.cs'):
        required_installer.update(json.loads(m[1]) for m in re.finditer(r'\b(?:T|Text)\(\s*("(?:\\.|[^"\\])*")\s*\)',path.read_text('utf8')))
    index=defaultdict(list)
    for text in pieces:
        if text:index[text[0]].append(text)
    templates=[]
    for text in values:
        if CJK.search(text) and re.search(r'%L?\d|%n',text):
            templates.append(re.compile('^'+re.sub(r'%L?\d+|%n','.*',re.escape(text))+'$',re.S))
    def composition(key):
        reachable={0}
        for i in range(len(key)):
            if i not in reachable:continue
            if key[i].isascii() and (key[i].isdigit() or key[i].isspace()):reachable.add(i+1)
            for piece in index.get(key[i],[]):
                if key.startswith(piece,i):reachable.add(i+len(piece))
        return len(key) in reachable
    catalogs={p.name:dict(sha256=digest(p.read_bytes()),entries=len(load(p)['messages'])) for p in sorted((QT/'src/app/languages').glob('*.json'))}
    english=english_override if english_override is not None else load(QT/'src/app/languages/en.json')['messages'];decisions=[]
    for key,en in english.items():
        context,source=key.split('\x04',1) if '\x04' in key else ('',key)
        if context=='installer':
            if source not in required_installer:decisions.append(dict(key=key,value=en,action='delete',reason_en='Exact installer-scoped source is absent from the complete embedded-catalog T/Text input set.'))
            continue
        if source in values or source in generated:continue
        safeguards=[]
        if any(source in value for value in values):safeguards.append('May be sliced from current owned metadata/report text; trace the exact parser consumer.')
        if composition(source):safeguards.append('Can be composed from current literal fragments; confirm the concrete lookup expression.')
        if any(pattern.fullmatch(source) for pattern in templates):safeguards.append('Matches a current formatted message template; confirm whether lookup occurs before or after formatting.')
        if not retired.get(source):safeguards.append('No authoritative retired producer has been located for this old catalog member.')
        decisions.append(dict(key=key,value=en,action='review' if safeguards else 'delete',reason_en='Retired owned producer; no exact literal, registered generated key, current substring, literal composition or formatted-template input remains.' if not safeguards else 'Input-domain evidence remains incomplete; preserve pending review.',retired_producers=retired.get(source,[]),gaps_en=safeguards))
    return dict(kind='post_cleanup_catalog_pruning',previous_revision=str(previous),source_files=current['files'],catalogs=catalogs,dynamic_lookup_domains=dynamic,generated_keys=sorted(generated),decisions=decisions,counts=dict(Counter(r['action'] for r in decisions)),boundary='Only confirmed retired owned producers without any conservative dynamic-input candidate are removed. Review records are not claimed to be active UI or confirmed unused. User values and framework-generated data are not owned translation sources.')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--previous',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--apply',action='store_true');a=p.parse_args()
    if a.output.exists():raise ValueError('Use a new output receipt')
    record=plan(a.previous.resolve());write(a.output,record)
    if a.apply:
        backup=(a.output.parent/(a.output.stem+'-preimage')).resolve();backup.mkdir(exist_ok=False)
        keys={r['key'] for r in record['decisions'] if r['action']=='delete'};changes=[]
        if inventory(QT)['files']!=record['source_files']:raise ValueError('Source changed before application')
        for name,expected in record['catalogs'].items():
            path=QT/'src/app/languages'/name
            if digest(path.read_bytes())!=expected['sha256']:raise ValueError('Catalog changed before application: '+name)
        for name in record['catalogs']:
            path=QT/'src/app/languages'/name;data=load(path);removed={k:data['messages'][k] for k in keys if k in data['messages']}
            if not removed:continue
            (backup/name).write_bytes(path.read_bytes())
            for key in removed:del data['messages'][key]
            temporary=path.with_suffix('.pruning.tmp');write(temporary,data);temporary.replace(path)
            changes.append(dict(file=name,removed=len(removed),removed_members=removed,after_sha256=digest(path.read_bytes()),after_entries=len(data['messages'])))
        record['applied']=dict(backup=str(backup),changes=changes);write(a.output,record)
    print(json.dumps(dict(counts=record['counts'],catalogs=len(record['catalogs']),dynamic_calls=len(record['dynamic_lookup_domains']),applied=a.apply)))

if __name__=='__main__':main()
