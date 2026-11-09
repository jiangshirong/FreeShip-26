"""Retained diagnostic for RVRS result sizes and its initial event timestamps.
Copies captured protocol inputs into an isolated ASCII directory; no models.
"""
import argparse,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs',type=Path,required=True);p.add_argument('--solver',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
for f in a.inputs.iterdir():
    if f.is_file() and f.name in ('TMP4.tsk','resist.dat'):shutil.copy2(f,a.output/f.name)
r=subprocess.run([str(a.solver)],cwd=a.output,input=b'',capture_output=True,timeout=30,creationflags=0x08000000 if os.name=='nt' else 0)
(a.output/'process.log').write_bytes(r.stdout+r.stderr)
f=a.output/'RVRSRES.dat';ls=f.read_text('ascii').splitlines() if f.exists() else []
report=dict(exit=r.returncode,size=f.stat().st_size if f.exists() else None,lines=len(ls),head=ls[:4],tail=ls[-4:])
(a.output/'probe.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
