"""Historical closed-consumer audit of the original 74 resource positions.

This is a source-bound proof for these two private caption dictionaries, not a
general C++ call-graph analyzer. New access forms fail closed for manual review.
The confirmed inactive members were actually removed on 2026-11-14. This tool's
CLI describes its original immutable snapshot; use prune_unused_resources.py
and the final pruning manifest to verify the cleaned current dictionaries.
The finite-domain/parser helpers below remain active maintenance dependencies.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys
QT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import LEX,cpp_literals,digest

# Independently read loop bounds and selector assignments in propulsion.cpp and
# propulsion_reports.inc. These finite supersets exclude none of their inputs.
NUMBER_DOMAINS={
 'labelIndex++':list(range(851,866)),
 '865+i-46':list(range(865,878)),
 '385+j':list(range(385,401)),
 '401+j':list(range(401,421)),
 '915+j':list(range(915,936)),
 '421+j':list(range(421,432)),
 '432+j':list(range(432,451)),
 'kk':[381,395],
 # Holtrop loops: ii=14..57 with jj initially 13, advancing on blanks,
 # and at most one decrement by two; later blocks stay below this bound.
 '1499+ii-jj':list(range(1500,1546)),
}

def call_arguments(text,name):
    tokens=list(LEX.finditer(text));covered=[(t.start(),t.end()) for t in tokens]
    for match in re.finditer(r'\b'+name+r'\s*\(',text):
        if any(a<=match.start()<b for a,b in covered):continue
        start=match.end();i=start;depth=1
        while i<len(text) and depth:
            token=next((t for t in tokens if t.start()==i),None)
            if token:i=token.end();continue
            if text[i]=='(':depth+=1
            elif text[i]==')':depth-=1
            i+=1
        if depth:raise ValueError('Unbalanced call')
        arg=text[start:i-1].strip()
        if re.match(r'(?:const\s+|int\s+)',arg):continue
        yield text.count('\n',0,match.start())+1,arg

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--revision',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    pending=[json.loads(x) for x in (a.revision/'evidence_insufficient.jsonl').read_text('utf8').splitlines() if json.loads(x)['kind']=='reference_reachability_unconfirmed']
    if len(pending)!=74:raise ValueError('This review is bound to the original 74-site queue')
    result=[];proofs={};hashes={}
    for family,main,named in [('resistance','resistance.cpp','tr'),('propulsion','propulsion.cpp','r')]:
        symbol=family.title()+'Tables';header=family+'_tables.h';users=[]
        for path in (QT/'src').rglob('*'):
            if path.suffix not in ('.cpp','.h','.inc','.hpp','.cc'):continue
            if path.name==header:continue
            contents=path.read_text('utf8')
            if re.search(r'\b'+symbol+r'\b',contents) or re.search(r'#include\s+"'+re.escape(header)+'"',contents):users.append(path.relative_to(QT).as_posix())
        if users!=['src/document/'+main]:raise ValueError('New dictionary/header consumer requires review: '+repr(users))
        entry=QT/'src/document'/main;files={entry};todo=[entry]
        while todo:
            source=todo.pop()
            for include in re.findall(r'^#include "([^"\n]+\.inc)"',(source).read_text('utf8'),re.M):
                child=source.parent/include
                if child not in files:files.add(child);todo.append(child)
        literals=set();indices=set();dynamic=[];named_calls=[]
        for source in sorted(files):
            text=source.read_text('utf8');relative=source.relative_to(QT).as_posix();hashes[relative]=digest(source.read_bytes())
            literals.update(v for _,v,*_ in cpp_literals(text))
            for line,arg in call_arguments(text,'userstring'):
                compact=re.sub(r'\s+','',arg)
                if compact.isdigit():indices.add(int(compact))
                elif compact in NUMBER_DOMAINS:
                    indices.update(NUMBER_DOMAINS[compact]);dynamic.append(dict(file=relative,line=line,argument=arg,finite_superset=NUMBER_DOMAINS[compact]))
                else:raise ValueError('Unreviewed numbered lookup: '+relative+':'+arg)
            for line,arg in call_arguments(text,named):
                named_calls.append(dict(file=relative,line=line,argument=arg))
                if not re.fullmatch(r'(?:QStringLiteral\s*\(\s*)?"[^"\\]*"\s*\)?',arg):
                    # The complete finite wrappers/arrays are explicitly reviewed.
                    if family=='resistance' and arg not in ('u?"rs_tons":"rs_tonnes"','label'):raise ValueError('New dynamic caption lookup: '+arg)
                    if family=='propulsion' and arg not in ('d(4)==1?"rs_fixed_pitch_propeller":"rs_controllable_reversible_pitch_propeller"','item.first','kind[family]','label[i]','task==1?"rs_propeller_task1_note":task<=3?"rs_propeller_task2_note":task==4?"rs_propeller_task4_note":"rs_propeller_task5_note"'):raise ValueError('New dynamic caption lookup: '+arg)
        proofs[family]=dict(translation_unit=entry.relative_to(QT).as_posix(),all_source_header_and_symbol_consumers=users,included_implementation_files=[s.relative_to(QT).as_posix() for s in sorted(files)],named_calls=named_calls,numbered_dynamic_calls=dynamic,numbered_possible_indices=sorted(indices),boundary='The dictionary and lookup wrappers have anonymous-namespace linkage. Named variable lookups select from literal-key arrays/ternaries; no constructed keys or external dictionary access. Schema consumers select calculator entries, not the strings/numbered dictionaries.')
        for row in pending:
            if row['source']['file']!='src/document/'+family+'_tables.h':continue
            locator=row['source']['locator'];kind,key=locator.split('=/')[-1].split('/',1)
            possible=int(key) in indices if kind=='numbered' else key in literals
            if possible:raise ValueError('Resource has a possible current consumer; inspect: '+key)
            result.append(dict(**row,resolution='inactive_retained_legacy_resource',currently_used=False,proof_family=family,reason_en='No current consumer in the complete private-dictionary lookup domain. Retained historical resource; not emitted by the Qt application. Do not translate as current UI or delete the historical table.'))
    if len(result)!=74:raise ValueError('Incomplete resolution coverage')
    for f,h in hashes.items():
        if digest((QT/f).read_bytes())!=h:raise ValueError('Source changed during analysis')
    for row in pending:
        f=row['source']['file'];hashes[f]=digest((QT/f).read_bytes())
    data=dict(status='resolved_for_exact_source',counts=dict(Counter(r['resolution'] for r in result)),records=result,proofs=proofs,source_sha256=hashes,external_reference_policy='Equal strings in another dictionary are different resource positions. This report resolves only the 74 exact original locators.')
    a.output.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(records=len(result),status=data['status'])))

if __name__=='__main__':main()
