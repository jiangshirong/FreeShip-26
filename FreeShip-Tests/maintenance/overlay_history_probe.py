"""Retained diagnostic for stale hydrostatics versus native scene ordering.
Usage: python -B maintenance/overlay_history_probe.py --runtime <runtime> --model <fbm> --output <new-dir>
Calls the production service; keeps all four native-input snapshots for diagnosis.
This is folded into qt-overlays' production history checks once resolved.
"""
from pathlib import Path
import argparse,json,sys,uuid
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import qt_migration as q
import native_linesplan_app as native
def main():
    p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--model',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);report=q.Report();stages={}
    with q.Service(a.runtime.resolve()/'bridge/FreeShipGeometryBridge.exe',out/'service') as s:
        for phase in ('original','moved','undo','redo'):
            target=out/phase;target.mkdir();fields={};method=phase
            if phase=='original':method='load';fields={'path':str(a.model.resolve()),'document_id':str(uuid.uuid4())}
            elif phase=='moved':
                point=next(p for p in stages['original']['model']['control_points'] if not p['locked']);position=list(point['p']);position[2]+=.01
                method='move_point';fields={'point_id':point['id'],'position':position}
            response=s.command(method,output=target,**fields);stages[phase]=q.validate_snapshot(report,phase,response,target)
    x,y=[json.loads((out/phase/'native-input.json').read_text('utf8')) for phase in ('moved','redo')]
    def diffs(a,b,path=''):
        if a==b:return []
        if isinstance(a,dict) and isinstance(b,dict):return [d for k in a.keys()|b.keys() for d in diffs(a.get(k),b.get(k),path+'.'+k)]
        if isinstance(a,list) and isinstance(b,list) and len(a)==len(b):return [d for i,(v,w) in enumerate(zip(a,b)) for d in diffs(v,w,path+f'[{i}]')]
        return [{'path':path,'moved':a,'redo':b}]
    report.data['native_input_differences']=diffs(x,y)
    report.case('redo-model-geometry',q.geometry(stages['moved']['model'])==q.geometry(stages['redo']['model']))
    report.case('redo-hydro',stages['moved']['model']['overlay']==stages['redo']['model']['overlay'])
    report.compare('redo-scene',stages['moved']['paths']['scene'],stages['redo']['paths']['scene'])
    report.finish();(out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'counts':report.data['counts'],'input_diff':report.data['native_input_differences'][:8]},ensure_ascii=False))
if __name__=='__main__':main()
