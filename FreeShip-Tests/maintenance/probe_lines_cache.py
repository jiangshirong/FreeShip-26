"""Retained timing diagnosis on a captured native input, not an acceptance suite."""
import argparse
import copy
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import qt_lines_cache as c

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('runtime','input','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    value=c.read(a.input);r=c.q.Report();cache=out/'native-lines-cache'
    c.worker(r,a.runtime.resolve(),value,out/'cold',cache)
    c.worker(r,a.runtime.resolve(),value,out/'warm',cache)
    moved=copy.deepcopy(value);moved['geometry']['points'][0]['position'][2]+=.125
    c.worker(r,a.runtime.resolve(),moved,out/'local',cache)
    c.worker(r,a.runtime.resolve(),moved,out/'full',None)
    r.data['input_sha256']=c.q.common.digest_file(a.input)
    r.data['worker_sha256']=c.q.common.digest_file(a.runtime/'FreeShipNativeLinesplan.exe')
    (out/'timings.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),encoding='utf8')
    for row in r.data['timings']:print(json.dumps({k:v for k,v in row.items() if k in ('case','total_ms','wall_ms','section_ms','section_index_ms','section_cache_prepare_ms','section_cache_timings','section_cache_hits','section_cache_misses')}))
if __name__=='__main__':main()
