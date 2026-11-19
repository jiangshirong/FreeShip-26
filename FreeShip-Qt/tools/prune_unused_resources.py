"""Plan/apply removal of unreachable members of private native caption tables.

Uses the reviewed bounded lookup domains, not text searches across translations.
Unknown consumers/expressions fail closed. JSON numeric keys are never renumbered.
The complete before/after decision is retained for audit, not loaded by the app.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

QT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import cpp_literals, digest, LEX
from resolve_legacy_resource_reachability import call_arguments, NUMBER_DOMAINS

FAMILIES = {
    'resistance': ('ResistanceTables','tr',{'u?"rs_tons":"rs_tonnes"','label'}),
    'propulsion': ('PropulsionTables','r',{
        'd(4)==1?"rs_fixed_pitch_propeller":"rs_controllable_reversible_pitch_propeller"',
        'item.first','kind[family]','label[i]',
        'task==1?"rs_propeller_task1_note":task<=3?"rs_propeller_task2_note":task==4?"rs_propeller_task4_note":"rs_propeller_task5_note"'}),
}

# This review is deliberately tied to the current private wrappers. A new raw
# table access/alias, even with an existing key, needs review before pruning.
# Keeping complete statements also catches a changed wrapper parameter domain.
TABLE_ROUTES={
 'resistance':[
  'const QJsonObject &tables(){static auto t=QJsonDocument::fromJson(ResistanceTables).object();return t;}',
  'QString tr(const QString &s){auto v=tables()["strings"].toObject()[s].toString();if(v.isEmpty())bad("Missing resistance caption: "+s);return report_language::text(v,"resistance.strings."+s);}',
  'QString userstring(int n){return report_language::text(tables()["numbered"].toObject()[QString::number(n)].toString(),"resistance.numbered."+QString::number(n));}',
  'clock.start();meta=tables()[method].toObject();schema=meta["schema"].toObject();',
  'bool nativeResistance(const QString &method){return tables().contains(method)&&tables()[method].toObject().contains("schema");}',
 ],
 'propulsion':[
  'const QJsonObject &tables(){static auto t=QJsonDocument::fromJson(PropulsionTables).object();return t;}',
  'QString r(const char *s){return report_language::text(tables()["strings"].toObject()[s].toString(),"propulsion.strings."+QString::fromUtf8(s));}',
  'QString userstring(int i){return report_language::text(tables()["numbered"].toObject()[QString::number(i)].toString(),"propulsion.numbered."+QString::number(i));}',
  'schema=tables()["schema"].toObject()[req["calculator"].toString()].toObject();',
  'QJsonArray curves;const auto descriptions=tables()["series"].toObject()[QString::number(task)].toArray();',
 ],
}

def validate_table_routes(family,relative,source):
    covered=[(m.start(),m.end()) for m in LEX.finditer(source)]
    allowed={re.sub(r'\s+','',line) for line in TABLE_ROUTES[family]}
    for match in re.finditer(r'\btables\b',source):
        if any(start<=match.start()<end for start,end in covered):continue
        line=source[source.rfind('\n',0,match.start())+1:source.find('\n',match.end()) if '\n' in source[match.end():] else len(source)]
        if relative!='src/document/'+family+'.cpp' or re.sub(r'\s+','',line) not in allowed:
            raise ValueError('Unreviewed raw table route or alias: '+relative+': '+line.strip())

def table(path):
    source=path.read_text('utf8')
    match=re.search(r'R"([^ ()\\\t\r\n]{0,16})\((.*)\)\1";', source, re.S)
    if not match:raise ValueError('Unknown embedded table format: '+str(path))
    data=json.loads(match[2])
    if not isinstance(data.get('strings'),dict) or not isinstance(data.get('numbered'),dict):
        raise ValueError('Unexpected dictionary schema')
    return source,match,data

def domains(family):
    symbol,lookup,variables=FAMILIES[family]
    header=family+'_tables.h';main='src/document/'+family+'.cpp'
    users=[]
    for path in sorted((QT/'src').rglob('*')):
        if path.suffix not in ('.cpp','.h','.inc','.hpp','.cc') or path.name==header:continue
        source=path.read_text('utf8')
        if re.search(r'\b'+symbol+r'\b',source) or re.search(r'#include\s+"'+header+'"',source):
            users.append(path.relative_to(QT).as_posix())
    if users!=[main]:raise ValueError('Unknown table consumer: '+repr(users))
    files={QT/main};pending=list(files)
    while pending:
        path=pending.pop()
        for include in re.findall(r'^#include "([^"\n]+\.inc)"',path.read_text('utf8'),re.M):
            child=path.parent/include
            if child not in files:files.add(child);pending.append(child)
    literals=set();indices=set();calls=[];hashes={}
    for path in sorted(files):
        source=path.read_text('utf8');relative=path.relative_to(QT).as_posix()
        hashes[relative]=digest(path.read_bytes())
        validate_table_routes(family,relative,source)
        literals.update(value for _,value,*_ in cpp_literals(source))
        # Reviewed table access is confined to these wrappers and calculator
        # schema fields. No raw dictionary object is returned across the TU.
        for _,arg in call_arguments(source,'userstring'):
            expression=re.sub(r'\s+','',arg)
            if expression.isdigit():indices.add(int(expression))
            elif expression in NUMBER_DOMAINS:indices.update(NUMBER_DOMAINS[expression])
            else:raise ValueError('Unreviewed numeric lookup: '+relative+': '+arg)
        for line,arg in call_arguments(source,lookup):
            if not re.fullmatch(r'(?:QStringLiteral\s*\(\s*)?"[^"\\]*"\s*\)?',arg) and arg not in variables:
                raise ValueError('Unreviewed named lookup: '+relative+': '+arg)
            calls.append(dict(file=relative,line=line,expression=arg))
    return dict(consumers=users,files=hashes,literals=literals,indices=indices,calls=calls)

def make_plan(original,baseline=None):
    legacy=json.loads(original.read_text('utf8'))
    for file,expected in legacy['source_sha256'].items():
        source=(baseline/file) if baseline else (QT/file)
        if digest(source.read_bytes())!=expected:
            raise ValueError('Original proof is stale: '+file)
    originals={(r['source']['file'],r['source']['locator'].split('=/',1)[1]) for r in legacy['records']}
    headers=[];coverage=set()
    for family in FAMILIES:
        domain=domains(family);relative='src/document/'+family+'_tables.h'
        original_header=(baseline/relative) if baseline else QT/relative
        source,match,data=table(original_header);removals=[]
        kept={}
        for kind in ('strings','numbered'):
            kept[kind]=[]
            for key,value in list(data[kind].items()):
                used=int(key) in domain['indices'] if kind=='numbered' else key in domain['literals']
                if used:kept[kind].append(key);continue
                locator=kind+'/'+key
                removals.append(dict(locator=locator,value=value,original_74=(relative,locator) in originals))
                coverage.add((relative,locator));del data[kind][key]
        replacement=json.dumps(data,ensure_ascii=False,separators=(',',':'))
        after=source[:match.start(2)]+replacement+source[match.end(2):]
        headers.append(dict(file=relative,before_sha256=digest(original_header.read_bytes()),after_sha256=digest(after.encode()),
            before_bytes=original_header.stat().st_size,after_bytes=len(after.encode()),removed=removals,kept=kept,
            dependencies=domain['files'],consumers=domain['consumers'],calls=domain['calls']))
    if not originals<=coverage:raise ValueError('An original position was not proven removable')
    return dict(format='FreeShipPrivateResourcePruning',baseline_source=str(baseline) if baseline else str(QT),original_proof=str(original),original_proof_sha256=digest(original.read_bytes()),
        original_positions=len(originals),headers=headers,numbering='Stable object keys; no renumbering',
        counts={h['file']:dict(Counter(r['locator'].split('/')[0] for r in h['removed'])) for h in headers})

def apply(plan):
    # Check every source first, then perform the narrowly scoped dictionary edits.
    for header in plan['headers']:
        for file,expected in header['dependencies'].items():
            if digest((QT/file).read_bytes())!=expected:raise ValueError('Source changed: '+file)
    for header in plan['headers']:
        path=QT/header['file'];source,match,data=table(path)
        before=json.loads(match[2])
        for record in header['removed']:
            kind,key=record['locator'].split('/',1)
            if key in data[kind]:
                if data[kind][key]!=record['value']:raise ValueError('Resource changed')
                del data[kind][key]
        # No changes to calculator schemas, retained values or stable IDs.
        for kind in ('strings','numbered'):
            if list(data[kind])!=header['kept'][kind]:raise ValueError('Retained keys differ')
            if any(data[kind][key]!=before[kind][key] for key in data[kind]):raise ValueError('Retained value changed')
        after=source[:match.start(2)]+json.dumps(data,ensure_ascii=False,separators=(',',':'))+source[match.end(2):]
        if digest(after.encode())!=header['after_sha256']:raise ValueError('Unexpected output')
        path.write_bytes(after.encode())

def verify(plan):
    results=[]
    for header in plan['headers']:
        _,_,data=table(QT/header['file']);domain=domains(Path(header['file']).stem.split('_')[0])
        absent=all(key not in data[kind] for kind,key in (r['locator'].split('/',1) for r in header['removed']))
        retained=all(list(data[kind])==header['kept'][kind] for kind in ('strings','numbered'))
        # The retained values are bound to the exact expected new file hash.
        exact=digest((QT/header['file']).read_bytes())==header['after_sha256']
        dependencies=domain['files']==header['dependencies']
        results.append(dict(file=header['file'],removed_absent=absent,retained_keys_stable=retained,
            exact_source=exact,consumer_dependencies_unchanged=dependencies,pass_=absent and retained and exact and dependencies))
    return results

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['plan','apply','verify'])
    parser.add_argument('--manifest',type=Path,required=True)
    parser.add_argument('--proof',type=Path,default=QT.parent/'翻译工作档案'/'TranslationContext_Qt_20261113_resolved/legacy_resource_reachability.json')
    parser.add_argument('--baseline-source',type=Path,help='Verified immutable original source; allows a cumulative plan after reviewed method removals')
    args=parser.parse_args()
    if args.action=='plan':
        if args.manifest.exists():raise ValueError('Manifest already exists')
        plan=make_plan(args.proof,args.baseline_source);args.manifest.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n','utf8')
        print(json.dumps(dict(counts=plan['counts'],original_positions=plan['original_positions'])))
    else:
        plan=json.loads(args.manifest.read_text('utf8'))
        if args.action=='apply':apply(plan)
        checks=verify(plan);print(json.dumps(checks))
        if not all(r['pass_'] for r in checks):raise SystemExit(1)

if __name__=='__main__':main()
