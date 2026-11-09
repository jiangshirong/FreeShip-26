"""Replay a retained construction protocol on an independently chosen old service.

Diagnostic evidence for Plus heap-order variation, not an expected-output
generator. Never changes fixtures, assertions, installed files or user models.
All attempts are retained, including nonmatching results.
"""
import argparse,json,sys,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q
from qt_native_construction import attributed_model


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--protocol',type=Path,required=True);p.add_argument('--target',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--attempts',type=int,default=4)
    p.add_argument('--preserve-layout',action='store_true',help='Keep original output suffixes and path lengths; old pointer ordering depends on allocation sizes')
    a=p.parse_args()
    if not 1<=a.attempts<=8:raise ValueError('Use 1 to 8 bounded attempts')
    a.output.mkdir(parents=True,exist_ok=False);commands=[]
    for line in a.protocol.read_text('utf8').splitlines():
        entry=json.loads(line)
        if 'request' not in entry:continue
        request=entry['request'];commands.append(request)
        if request.get('operation')=='insert_plane' and request.get('create_curve'):break
    assert commands and commands[-1].get('operation')=='insert_plane'
    executable=a.runtime/'bridge/FreeShipGeometryBridge.exe';digest=q.common.digest_file(executable)
    target=attributed_model(json.loads((a.target/'model.json').read_text('utf8')));report=dict(service=str(executable.resolve()),service_sha256=digest,protocol_sha256=q.common.digest_file(a.protocol),target=str(a.target.resolve()),attempts=[])
    original_root=a.protocol.resolve().parent.parent
    report['preserve_layout']=a.preserve_layout
    for i in range(a.attempts):
        name=f'attempt-{i+1}'
        if a.preserve_layout:
            padding=len(str(original_root))-len(str((a.output/name).resolve()))
            if padding<0:raise ValueError('Use a shorter output root to preserve original path lengths')
            name+='x'*padding
        folder=a.output/name
        with q.Service(executable,folder/('oracle' if a.preserve_layout else 'service'),native_linesplan=True) as service:
            for step,request in enumerate(commands,1):
                fields={k:v for k,v in request.items() if k not in ('id','method','output','native_linesplan','expected_revision','expected_document_id','document_id')}
                if request['method']=='load':fields['document_id']=str(uuid.uuid4())
                relative=Path(request['output']).resolve().relative_to(original_root) if a.preserve_layout else Path(f'{step:03}')
                response=q.require_ok(service.command(request['method'],output=folder/relative,**fields))
            result=attributed_model(json.loads(Path(response['model']).read_text('utf8')))
            report['attempts'].append(dict(model=response['model'],matches_target=result==target,different_fields=[k for k in target if target[k]!=result.get(k)],artifacts={k:response[k] for k in ('model','scene','dxf','sections')},sha256={k:q.common.digest_file(Path(response[k])) for k in ('model','scene','dxf','sections')}))
    assert q.common.digest_file(executable)==digest
    (a.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8');print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':main()
