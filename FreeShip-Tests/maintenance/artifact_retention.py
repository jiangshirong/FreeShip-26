"""Read-only retention planner for generated test output; never deletes files.

Run: python -B maintenance/artifact_retention.py --output <plan.json>
Keep referenced baselines (including transitive report references), newest run
and newest successful/partial run of each named family. Unknown output is kept.
The separate PowerShell executor checks the plan again before removing anything.
"""
from pathlib import Path
import argparse, hashlib, json, os, re
from collections import defaultdict

ROOT=Path(__file__).resolve().parents[1]
WORKSPACE=ROOT.parent
REPORTS=ROOT/'reports'
WORDS=r'first|second|third|fourth|fifth|sixth|seventh|eighth|ninth|tenth|final|verified|initial|latest|release|delivery|dev|regression|current|smoke|preflight'
def family(name):
    # Different feature names stay distinct; only dated/numbered run suffixes
    # and explicit iteration labels are removed.
    value=re.sub(r'-(?:20\d{6})(?:-.*)?$','',name)
    value=re.sub(r'-(?:'+WORDS+r')(?:\d+)?(?:-.*)?$','',value)
    return re.sub(r'-\d+$','',value)
def files_under(root):
    for base,dirs,files in os.walk(root,followlinks=False):
        dirs[:]=[d for d in dirs if not Path(base,d).is_symlink() and not getattr(os.path,'isjunction',lambda _:False)(Path(base,d))]
        for name in files:yield Path(base,name)
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    folders={p.name:p for p in REPORTS.iterdir() if p.is_dir() and not p.is_symlink()}
    # Tokenize paths once rather than scanning every byte against hundreds of
    # directory-name alternatives. Chinese/Latin path leaves are both retained.
    pattern=re.compile(r'[\w.-]+')
    protected=defaultdict(set);metadata={};groups=defaultdict(list)
    def references(text,source):
        for name in set(pattern.findall(text))&folders.keys():protected[name].add(source)
    # Use rg's tracked/source inventory, excluding generated and external trees.
    import subprocess
    r=subprocess.run(['rg','--files','--hidden','-g','!**/.git/**','-g','!**/reports/**','-g','!**/build/**','-g','!**/dist/**','-g','!**/.tools/**','-g','!**/References/**','-g','*.py','-g','*.ps1','-g','*.md','-g','*.json','-g','*.cpp','-g','*.pas'],cwd=WORKSPACE,capture_output=True,check=True,encoding='utf8')
    for relative in r.stdout.splitlines():
        p=WORKSPACE/relative
        if p.parent==Path(__file__).parent and p.suffix=='.json':continue
        if p.stat().st_size>10_000_000:continue
        references(p.read_text('utf8',errors='replace'),'source:'+relative)
    for name,path in folders.items():
        report=path/'report.json'
        try:
            raw=report.read_bytes();data=json.loads(raw.decode('utf-8-sig'))
            status=data.get('status');counts=data.get('counts',{})
            if status not in ('pass','partial','review','fail','error','not_run'):raise ValueError('unknown report schema')
            metadata[name]={'status':status,'counts':counts,'report_sha256':hashlib.sha256(raw).hexdigest(),'family':family(name),'report_mtime_ns':report.stat().st_mtime_ns}
            groups[family(name)].append(name)
        except (OSError,ValueError):protected[name].add('unknown-or-missing-report')
    for group,names in groups.items():
        ordered=sorted(names,key=lambda n:metadata[n]['report_mtime_ns'],reverse=True)
        protected[ordered[0]].add('latest-run:'+group)
        valid=[n for n in ordered if metadata[n]['status'] in ('pass','partial','review') and not metadata[n]['counts'].get('fail',0) and not metadata[n]['counts'].get('error',0)]
        if valid:protected[valid[0]].add('latest-effective:'+group)
    # A retained report may depend on an older frozen baseline through absolute
    # artifact paths even when no source file names that baseline explicitly.
    scanned=set()
    while set(protected)-scanned:
        for name in list(set(protected)-scanned):
            scanned.add(name)
            report=folders[name]/'report.json'
            if report.exists():references(report.read_text('utf8',errors='replace'),'retained-report:'+name)
    candidates=[];kept=[];total=0
    for name,path in folders.items():
        sizes=[p.stat().st_size for p in files_under(path) if p.is_file()];size=sum(sizes);total+=size
        entry={'path':str(path.resolve()),'bytes':size,'files':len(sizes),**metadata.get(name,{})}
        if name in protected:entry['reasons']=sorted(protected[name]);kept.append(entry)
        else:entry['reason']='superseded-unreferenced-report';candidates.append(entry)
    plan={'format':'FreeShipArtifactRetentionPlan','version':1,'root':str(REPORTS.resolve()),'total_bytes':total,
          'reclaim_bytes':sum(c['bytes'] for c in candidates),'candidates':sorted(candidates,key=lambda c:-c['bytes']),
          'kept':kept,'scope':'Only complete superseded report directories; no source, fixtures, builds, installed programs or kept baseline contents.'}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'total_gib':round(total/2**30,2),'reclaim_gib':round(plan['reclaim_bytes']/2**30,2),'delete_directories':len(candidates),'keep_directories':len(kept),'largest':[(Path(c['path']).name,round(c['bytes']/2**30,2)) for c in plan['candidates'][:12]],'plan':str(args.output)},ensure_ascii=False))
if __name__=='__main__':main()
