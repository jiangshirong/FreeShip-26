"""Summarize a user-generated local lines-plan trace without operating the desktop."""
import argparse, csv, json
from collections import Counter
from pathlib import Path

def summarize(path):
    with path.open(encoding='utf-8-sig',newline='') as stream:
        rows=list(csv.DictReader(stream,delimiter='\t'))
    counts=Counter(r['event'] for r in rows)
    frames=[]
    for row in rows:
        if row['event']=='full':
            fields=dict(item.strip().split('=',1) for item in row['detail'].split(';') if '=' in item)
            frames.append({'at_ms':int(row['elapsed_ms']),'duration_ms':int(fields['ms']),'zoom':fields.get('zoom')})
    native=[];mismatches=[];reentrant=[];premature=[];pending_compositions=[];composition=None
    for row in rows:
        if row['event']!='native':continue
        fields=dict(item.strip().split('=',1) for item in row['detail'].split(';') if '=' in item)
        sample={'at_ms':int(row['elapsed_ms']),**fields};native.append(sample)
        for layer in ('host','frame','viewport'):
            value=fields.get(layer,'');parts=value.split('/')
            if len(parts)>=2 and parts[0]!=parts[1]:
                mismatches.append({'layer':layer,'logical':parts[0],'native':parts[1],**sample})
        if fields.get('stage')=='present.begin' and fields.get('rendering')=='1':
            reentrant.append(sample)
        if fields.get('stage')=='compose.begin':
            pending=any(len(parts)>=2 and parts[0]!=parts[1] for parts in
                (fields.get(layer,'').split('/') for layer in ('host','frame','viewport')))
            composition=sample if pending else None
        elif fields.get('stage')=='compose.end' and composition is not None:
            pending_compositions.append({'begin':composition,'end':sample,
                'duration_ms':sample['at_ms']-composition['at_ms']})
            composition=None
        view=fields.get('viewport','').split('/')
        if (fields.get('stage')=='present.end' and fields.get('dc_window','0').strip('0') and
            len(view)>=2 and fields.get('buffer')!=view[1]):
            premature.append(sample)
    return {'trace':str(path.resolve()),'status':'review' if native else 'observed','events':dict(counts),
            'full_frames':frames,'has_close_event':bool(counts['closed']),
            'native_observations':native,'native_geometry_mismatches':mismatches,
            'reentrant_presentations':reentrant,'premature_native_presentations':premature,'pending_native_compositions':pending_compositions,'native_animation_acceptance':'not_run',
            'scope':'Observed messages and geometry only. A mismatch or reentrant call is a diagnostic lead, not proof of visible flicker; no desktop input or FPS inference'}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('trace',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args();text=json.dumps(summarize(a.trace),ensure_ascii=False,indent=2)
    if a.output:a.output.write_text(text,encoding='utf-8')
    print(text)
