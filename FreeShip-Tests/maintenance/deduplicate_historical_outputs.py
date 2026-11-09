"""Deduplicate immutable historical outputs by verified same-volume hardlinks.
Keeps every path and byte; never traverses reparse points or touches builds.
Future writes to retained historical evidence must replace files atomically.
"""
import os,json,hashlib,collections,shutil,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
roots=[Path.home()/'FreeShip26-TestResults',Path('D:/FreeShip26Tests/reports'),ROOT/'reports']
out=ROOT/'maintenance/storage-dedup-20261109.jsonl'
groups=collections.defaultdict(list)
protected={'baseline','baselines','fixtures'}
for root in roots:
 stack=[root]
 while stack:
  for e in os.scandir(stack.pop()):
   st=e.stat(follow_symlinks=False)
   if st.st_file_attributes&1024:continue
   p=Path(e.path)
   if e.is_dir(follow_symlinks=False):
    if p.name.lower() not in protected:stack.append(p)
   elif st.st_size>=1024*1024 and p.suffix.lower() in {'.json','.fslp','.bmp','.png','.exe','.dll','.dxf','.zip','.mkv','.o','.ppu','.a'}:
    groups[(p.drive.lower(),st.st_size)].append((p,st.st_mtime_ns))
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
def contained(p):
 return any(p.is_relative_to(r) for r in roots) and not any(x.lstat().st_file_attributes&1024 for x in [p,*p.parents] if x!=Path(x.anchor))
count=total=0;before={d:shutil.disk_usage(d+':/').free for d in ('C','D')}
with out.open('x',encoding='utf8') as log:
 for (_,size),entries in sorted(groups.items(),key=lambda x:x[0][1],reverse=True):
  if len(entries)<2:continue
  hashes={}
  for p,mtime in entries:
   st=p.stat()
   if st.st_size!=size or st.st_mtime_ns!=mtime:continue
   h=digest(p)
   if h not in hashes:hashes[h]=p;continue
   canonical=hashes[h]
   if os.path.samefile(p,canonical):continue
   if not contained(p) or not contained(canonical):raise RuntimeError('Unsafe path')
   backup=p.with_name(p.name+'.dedup-backup')
   if backup.exists():raise RuntimeError('Backup already exists')
   if p.stat().st_mtime_ns!=mtime:continue
   os.replace(p,backup)
   try:
    os.link(canonical,p)
    if not os.path.samefile(p,canonical) or digest(p)!=h:raise RuntimeError('Verification failed')
   except BaseException:
    if p.exists():p.unlink()
    os.replace(backup,p);raise
   # Delete only the verified duplicate, whose full path is within report roots.
   backup.unlink();count+=1;total+=size
   log.write(json.dumps(dict(path=str(p),canonical=str(canonical),bytes=size,sha256=h),ensure_ascii=False)+'\n');log.flush()
   if count%100==0:print(json.dumps(dict(files=count,logical_gib=round(total/2**30,2))),flush=True)
summary=dict(files=count,logical_bytes=total,free_before=before,free_after={d:shutil.disk_usage(d+':/').free for d in ('C','D')},policy='Same-volume SHA256-verified hardlinks; historical paths preserved; no reparse traversal')
out.with_suffix('.summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8');print(json.dumps(summary),flush=True)
