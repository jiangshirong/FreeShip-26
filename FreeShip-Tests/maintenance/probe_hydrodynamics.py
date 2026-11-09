"""Retained diagnostic: inspect MPP option-dependent reports and Enter pause.
Uses an existing captured INM as input. Does not edit any source or user model.
"""
import argparse,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from solver_launch_probe import probe
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--solver',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
for flags,angle,close in [('000',10,0),('100',10,0),('110',10,0),('111',10,0),('010',10,0),('111',35,0),('111',35,1),('111',20,1)]:
    folder=a.output/(flags+'-'+str(angle)+'-'+str(close));folder.mkdir();s=a.input.read_text('ascii').splitlines();s[2]=s[2][:-6]+''.join(' '+c for c in flags)
    s[0]=s[0][:-2]+f'{close:2d}';s[2]=f'{angle:10.6f}'+s[2][10:]
    (folder/'INM').write_bytes(('\r\n'.join(s)+'\r\n').encode('ascii'))
    r=probe(a.solver,folder,folder/'result',enter=True)
    out=folder/'result/OUT';lines=out.read_text('latin1').splitlines() if out.exists() else []
    print(json.dumps(dict(flags=flags,angle=angle,close=close,exit=r.get('exit_code'),lines=len(lines),nonfinite=[x for x in lines if 'NaN' in x],tail=lines[-4:]),ensure_ascii=False))
