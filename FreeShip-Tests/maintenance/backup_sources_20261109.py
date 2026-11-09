"""Create a source-only Git backup snapshot; does not alter existing repositories."""
import json, subprocess, shutil, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[2]
dest=Path('C:/FreeShip26-GitSourceBackup-20261109')
dest.mkdir(exist_ok=False)
records=[]; skipped=[]
for name in ('FreeShip-Qt','FreeShip-Plus-Lazarus','FreeShip-QtPilot','FreeShip-Tests'):
    base=root/name
    if (base/'.git').exists():
        raw=subprocess.check_output(['git','-C',str(base),'ls-files','-co','--exclude-standard','-z'])
        files=sorted(set(raw.decode('utf8').strip('\0').split('\0')))
    else:
        files=[]
        import os
        for here,dirs,names in os.walk(base,followlinks=False):
            dirs[:]=[d for d in dirs if d not in ('.tools','.venv','node_modules','reports','__pycache__','.capture-deps','build','dist','.git') and not (Path(here)/d).is_junction() ] if hasattr(Path,'is_junction') else [d for d in dirs if d not in ('.tools','.venv','node_modules','reports','__pycache__','.capture-deps','build','dist','.git') and not (Path(here)/d).is_symlink()]
            for f in names:files.append((Path(here)/f).relative_to(base).as_posix())
    for f in files:
        src=base/f
        if not src.is_file():continue
        if src.suffix.lower() in ('.exe','.dll','.o','.obj','.ppu','.a','.lib','.pyc','.pdb','.zip','.7z') or src.stat().st_size>40*1024*1024:
            skipped.append(name+'/'+f);continue
        target=dest/name/f;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target)
        records.append(dict(path=name+'/'+f,sha256=hashlib.sha256(src.read_bytes()).hexdigest()))
(dest/'BACKUP-MANIFEST.json').write_text(json.dumps(dict(files=records,excluded_artifacts=skipped),ensure_ascii=False,indent=2),encoding='utf8')
(dest/'README.md').write_text('# FreeShip_26 source backup — 2026-11-09\n\nSource snapshot of the Qt migration, reusable tests, Lazarus baseline and Qt pilot. Existing licenses remain in each directory. Generated builds and historical test outputs are excluded. See BACKUP-MANIFEST.json for file hashes.\n',encoding='utf8')
print(json.dumps(dict(path=str(dest),files=len(records),excluded=len(skipped))))

