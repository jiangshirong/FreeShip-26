"""Copy an explicit source-file set into the existing GitHub backup checkout.

Maintenance only. Requires a clean checkout on the known backup branch, stages
only the named source files and updated hash manifest, and never commits/pushes,
deletes files or copies build/test outputs. Call with --repo and --paths JSON
containing repository-relative workspace paths.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repo',type=Path,required=True);p.add_argument('--paths',type=Path,required=True);a=p.parse_args()
    repo=a.repo.resolve(strict=True)
    def git(*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True,encoding='utf8').strip()
    assert Path(git('rev-parse','--show-toplevel')).resolve()==repo
    assert git('branch','--show-current')=='backup/migration-20261109'
    assert git('remote','get-url','origin')=='https://github.com/jiangshirong/FreeShip-26.git'
    assert not git('status','--porcelain'),'Backup checkout has existing edits'
    names=json.loads(a.paths.read_text('utf8'));assert isinstance(names,list) and names
    manifest=repo/'BACKUP-MANIFEST.json';data=json.loads(manifest.read_text('utf8'))
    records={item['path']:item for item in data['files']};prepared=[]
    for name in names:
        path=Path(name);assert not path.is_absolute() and '..' not in path.parts
        assert path.parts[0] in ('FreeShip-Qt','FreeShip-Tests')
        assert not any(part in ('.git','.tools','dist','build','reports','__pycache__') for part in path.parts)
        assert path.suffix.lower() in ('.cpp','.h','.py','.pas','.inc','.md','.json','.ps1','.txt')
        src=(ROOT/path).resolve(strict=True);dest=(repo/path).resolve()
        assert ROOT in src.parents and repo in dest.parents and src.is_file() and src.stat().st_size<40*1024*1024
        prepared.append((name,src,dest,hashlib.sha256(src.read_bytes()).hexdigest()))
    for name,src,dest,digest in prepared:
        dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
        assert hashlib.sha256(dest.read_bytes()).hexdigest()==digest
        records[name]=dict(path=name,sha256=digest)
    data['files']=list(records.values());data['updated_utc']=datetime.now(timezone.utc).isoformat()
    manifest.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    git('-c','core.autocrlf=true','add','--',*names,'BACKUP-MANIFEST.json')
    print(json.dumps(dict(repo=str(repo),copied=len(prepared),total_manifest_files=len(records))))


if __name__=='__main__':main()
