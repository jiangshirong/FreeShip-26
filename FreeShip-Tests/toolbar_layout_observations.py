"""Observe toolbar group bounds from inventory; no fixed product-design verdict."""
from pathlib import Path
import argparse,json,hashlib,itertools
p=argparse.ArgumentParser();p.add_argument('--inventory',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=json.loads(a.inventory.read_text(encoding='utf-8'));layouts=[]
for s in d.get('snapshots',[]):
 if not s['state'].startswith('toolbar_fixture_'):continue
 bindings={n['component_path']:n for n in s['nodes'] if n.get('kind')=='button'}
 buttons=[];groups={}
 for n in s['nodes']:
  if n.get('kind')!='text_control' or n.get('class')!='TStudioSpeedButton':continue
  b=bindings.get(n['component_path'],{});entry={'caption':n['text'],'parent':b.get('parent'),'name':b.get('name'),'action':b.get('action'),'bounds':n['bounds'],'visible':n['visible_excluding_hidden_form']}
  buttons.append(entry)
  if not entry['visible']:continue
  key=b.get('parent')
  if key=='StudioToolbar':key='standalone:'+str(b.get('name'))
  groups.setdefault(key,[]).append(entry)
 spans=[]
 for key,items in groups.items():
  spans.append({'group':key,'left':min(i['bounds']['x'] for i in items),'right':max(i['bounds']['x']+i['bounds']['width'] for i in items),'members':[i['caption'] for i in items]})
 spans.sort(key=lambda v:v['left']);gaps=[{'from':x['group'],'to':y['group'],'pixels':y['left']-x['right']} for x,y in zip(spans,spans[1:])]
 overlaps=[]
 for x,y in itertools.combinations([b for b in buttons if b['visible']],2):
  r=x['bounds'];t=y['bounds']
  if max(r['x'],t['x'])<min(r['x']+r['width'],t['x']+t['width']) and max(r['y'],t['y'])<min(r['y']+r['height'],t['y']+t['height']):overlaps.append([x['caption'],y['caption']])
 layouts.append({'state':s['state'],'groups':spans,'gaps':gaps,'button_overlaps':overlaps,'buttons':buttons})
r={'status':'review' if layouts else 'not_run','inventory_sha256':hashlib.sha256(a.inventory.read_bytes()).hexdigest(),'scope':'Fixed-width hidden toolbar fixture observations; not native desktop or a universal layout policy','layouts':layouts}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'layouts':len(layouts),'observed_overlap_pairs':sum(len(x['button_overlaps']) for x in layouts),'output':str(a.output)},ensure_ascii=False))
