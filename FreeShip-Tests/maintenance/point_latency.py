"""Paired current-vs-candidate document latency probe; no timing pass threshold.

Example: python -B FreeShip-Tests/maintenance/point_latency.py --candidate
C:/FreeShip26Tests/build/point-package-01 --baseline FreeShip-Qt/dist --output
C:/FreeShip26Tests/reports/point-latency-01
Timing depends on the host load. Keep this diagnostic separate from deterministic
regression assertions; native kernel time is not GUI end-to-end latency.
"""
import argparse
import json
import shutil
import statistics
import sys
import time
import uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--baseline',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    report={'purpose':'paired full document service latency, not desktop frame rate','models':[],
            'script_sha256':q.common.digest_file(Path(__file__)),'executables':{}}
    for name,directory in [('baseline',a.baseline),('candidate',a.candidate)]:
        directory=directory.resolve(strict=True)
        manifest=json.loads((directory/'build-manifest.json').read_text('utf8'))
        exe=directory/'FreeShipDocumentService.exe'
        assert q.common.digest_file(exe)==manifest['binaries_sha256']['FreeShipDocumentService.exe']
        report['executables'][name]={'path':str(exe),'sha256':q.common.digest_file(exe)}
    for i,source in enumerate(q.DEFAULT_MODELS,1):
        folder=out/f'model-{i}';folder.mkdir();model=folder/'input.fbm';shutil.copy2(source,model)
        original=q.common.digest_file(source);identity=str(uuid.uuid4());entry={'source':str(source),'sha256':original,'samples':[]}
        with q.Service(a.baseline.resolve()/'FreeShipDocumentService.exe',folder/'baseline') as old, \
             q.Service(a.candidate.resolve()/'FreeShipDocumentService.exe',folder/'candidate') as new:
            states=[]
            for name,s in [('baseline',old),('candidate',new)]:
                response=q.require_ok(s.command('load',path=str(model),document_id=identity,output=folder/name/'load'))
                states.append(json.loads(Path(response['model']).read_text('utf8')))
            point=next(v for v in states[0]['control_points'] if v['batch_movable'])
            for n in range(3):
                position=list(point['p']);position[2]+=(n+1)*.125
                for name,s in ([('baseline',old),('candidate',new)] if n%2==0 else [('candidate',new),('baseline',old)]):
                    start=time.perf_counter()
                    response=q.require_ok(s.command('move_point',point_id=point['id'],position=position,output=folder/name/f'move-{n}'))
                    entry['samples'].append({'engine':name,'wall_ms':(time.perf_counter()-start)*1000,
                                             'point_edit_ms':response.get('point_edit_ms'),'document_total_ms':response['document_total_ms']})
        assert q.common.digest_file(source)==original and q.common.digest_file(model)==original
        entry['median_ms']={name:statistics.median(v['wall_ms'] for v in entry['samples'] if v['engine']==name) for name in ('baseline','candidate')}
        entry['candidate_over_baseline']=entry['median_ms']['candidate']/entry['median_ms']['baseline']
        report['models'].append(entry)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'report':str(out/'report.json'),'timings':[{'model':Path(m['source']).name,**m['median_ms'],'ratio':m['candidate_over_baseline']} for m in report['models']]},ensure_ascii=False))


if __name__=='__main__':main()
