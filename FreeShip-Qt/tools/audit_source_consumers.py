"""Conservative source-file dependency inventory, including test/build consumers.

Unreached files are candidates for review, never automatically deleted. Runtime
string/property bindings and packaged assets require their own finite-domain proof.
"""
import argparse,hashlib,json,re
from pathlib import Path

QT=Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES={'.cpp','.h','.hpp','.inc','.cc','.cxx'}

def inventory():
    files={p.resolve():p for p in (QT/'src').rglob('*') if p.suffix in SOURCE_SUFFIXES}
    cmake=(QT/'CMakeLists.txt').read_text('utf8')
    seeds={};edges={};unknown=[]
    for name in re.findall(r'\bsrc/[A-Za-z0-9_./-]+\.(?:cpp|cc|cxx)\b',cmake):
        p=(QT/name).resolve()
        if p in files:seeds[p]=['CMakeLists.txt']
    # A current probe may include production headers/implementations directly.
    probe_sources=list((QT.parent/'FreeShip-Tests/qt').glob('*.cpp'))+list((QT.parent/'FreeShip-Tests/qt').glob('*.h'))
    for path in list(files.values())+probe_sources:
        found=[]
        for include in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"',path.read_text('utf8'),re.M):
            target=next((p.resolve() for p in (path.parent/include,QT/'src'/include,QT/include) if p.is_file()),None)
            if target in files:
                found.append(target)
                if path not in files:seeds.setdefault(target,[]).append(str(path.relative_to(QT.parent)))
        edges[path.resolve()]=found
    # Exact source filenames in direct build/probe scripts are real consumers.
    scripts=list(QT.glob('*.ps1'))+list((QT/'tools').glob('*.py'))+list((QT.parent/'FreeShip-Tests').glob('*.py'))
    contents={str(p.relative_to(QT.parent)):p.read_text('utf8') for p in scripts}
    for path in files:
        if path in seeds:continue
        names=[path.name,path.relative_to(QT).as_posix()]
        consumers=[name for name,source in contents.items() if any(re.search(r'["\']'+re.escape(key)+r'["\']',source) for key in names)]
        if consumers:seeds[path]=consumers
    reached=set(seeds);pending=list(reached)
    while pending:
        for target in edges.get(pending.pop(),[]):
            if target not in reached:reached.add(target);pending.append(target)
    return dict(kind='conservative_dependency_inventory',sources=len(files),build_or_probe_roots=len(seeds),reachable=len(reached),
        candidates=[dict(file=p.relative_to(QT).as_posix(),status='review',missing='No current CMake, include-chain or exact build/probe consumer was found. Review generated/configuration/compatibility routes before deletion.') for p in sorted(set(files)-reached)],
        roots={p.relative_to(QT).as_posix():refs for p,refs in seeds.items()},
        boundary='All test source includes count as consumers. Documentation and source hashing alone do not establish runtime use. Assets, dynamically named actions and public API functions are not declared unused by this scan.')

def catalog_review(revision):
    """List exact catalog keys still needing a closed dynamic-input trace.

    Literal equality establishes only a possible consumer, never live UI proof.
    An absent match establishes only a candidate, never permission to delete.
    """
    inv=json.loads((revision/'inventory.json').read_text('utf8'))
    for relative,value in inv['files'].items():
        if hashlib.sha256((QT/relative).read_bytes()).hexdigest()!=value:
            raise ValueError('Catalog audit source snapshot is stale: '+relative)
    messages=json.loads((QT/'src/app/languages/en.json').read_text('utf8'))['messages']
    generated=[json.loads(x) for x in (revision/'generated_translation_units.jsonl').read_text('utf8').splitlines()]
    composed={r['source_key'] for r in generated}
    literal={r['text'] for r in inv['candidates']}
    missing=[dict(key=key,value=value,status='review',currently_unused='unproven',
                  required_evidence=['Trace this exact source key through uiMetadataText source/context/parts produced by document-service responses.',
                                     'Trace report_language::text owned caption inputs and formatted caption builders; equal display text elsewhere is insufficient.',
                                     'Check installer-context lookup and maintenance/test consumers before removal.'],
                  possible_input_routes=['src/app/ui_language.cpp:uiMetadataText',
                                         'src/app/ui_language.cpp:uiTextParts',
                                         'src/document/report_language.h:text'])
             for key,value in messages.items() if key.split('\x04')[-1] not in literal|composed]
    return dict(kind='catalog_candidate_review_not_dead_code_proof',catalog_entries=len(messages),
                literal_input_candidates=sum(k.split('\x04')[-1] in literal for k in messages),
                composed_input_candidates=sum(k.split('\x04')[-1] in composed for k in messages),
                unproven_candidates=len(missing),records=missing,
                source_files=inv['files'],catalog_sha256=hashlib.sha256((QT/'src/app/languages/en.json').read_bytes()).hexdigest(),
                boundary='No catalog key is deleted from this scan. Each exact unmatched member has its missing input-domain evidence listed; a failed text search is not a liveness proof.')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--context-revision',type=Path);a=p.parse_args()
    report=inventory()
    if a.context_revision:report['catalog_review']=catalog_review(a.context_revision.resolve())
    a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('roots','boundary','catalog_review')},ensure_ascii=False))
if __name__=='__main__':main()
