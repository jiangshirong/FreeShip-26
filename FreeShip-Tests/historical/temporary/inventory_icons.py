from pathlib import Path
import re,csv,json,collections,hashlib
root=Path('K:/');out=root/'Docs'/'图标盘点';out.mkdir(exist_ok=True)
def dump(name,rows):
 if not rows:return
 with (out/name).open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def clean(s):
 return re.sub(r'\{[\s\S]*?\}|\(\*[\s\S]*?\*\)|//[^\n]*',lambda m:'\n'*m[0].count('\n'),s)
refs=[];objects=[]
for path in sorted((root/'Forms').glob('*.lfm')):
 stack=[];nodes=[]
 for line,text in enumerate(path.read_text(encoding='utf8',errors='replace').splitlines(),1):
  m=re.match(r'\s*(?:object|inherited|inline) (\w+): (\w+)',text)
  if m:
   n={'name':m[1],'type':m[2],'props':{},'line':line,'parent':stack[-1] if stack else None};stack.append(n);nodes.append(n);continue
  if text.strip()=='item':stack.append(None);continue
  if re.match(r'^\s*end\s*>?$',text):
   if stack:stack.pop()
   continue
  m=re.match(r'\s*([\w.]+)\s*=\s*(.*)',text)
  if m and stack and stack[-1] is not None:stack[-1]['props'][m[1]]=m[2].strip()
 title=nodes[0]['props'].get('Caption','') if nodes else ''
 lookup={n['name']:n for n in nodes}
 for n in nodes:
  p=n['props'];action=lookup.get(p.get('Action',''));idx=p.get('ImageIndex','');key=''
  if not idx and action:idx=action['props'].get('ImageIndex','')
  mechanisms=[]
  if idx and re.fullmatch(r'\d+',idx):mechanisms.append('ImageIndex/Action');key=p.get('Action',n['name'] if n['type']=='TAction' else '')
  if 'Glyph.Data' in p:mechanisms.append('内嵌Glyph')
  if p.get('Kind','').startswith('bk') and p.get('Kind')!='bkCustom':mechanisms.append('LCL标准按钮:'+p['Kind'])
  if 'Picture.Data' in p:objects.append({'文件':str(path.relative_to(root)),'控件':n['name'],'类型':n['type'],'标题':title,'行号':n['line'],'说明':'图片资源；可能是图示/Logo/预览，不直接计为操作图标'})
  if not mechanisms:continue
  caption=p.get('Caption',action['props'].get('Caption','') if action else '')
  src='待关联图像列表'
  if key and (root/f'Themes/Default/icons/24/{key}.png').exists():src=f'Themes/Default/icons/24/{key}.png'
  ancestors=[];q=n['parent']
  while q:
   ancestors.append(q['name']);q=q['parent']
  scope='静态引用，运行时可见性待逐入口确认'
  if path.stem.lower()=='main' and 'PanelMain' in ancestors:scope='旧工具栏，当前Studio界面隐藏'
  if p.get('Visible')=='False':scope='资源中默认隐藏，可能由代码启用'
  refs.append({'文件':str(path.relative_to(root)),'窗口标题':title,'控件路径':'/'.join(reversed(ancestors))+('/' if ancestors else '')+n['name'],'控件类型':n['type'],'功能文字':caption,'动作':p.get('Action',''),'图标键':key,'索引':idx,'机制':'；'.join(mechanisms),'资源线索':src,'状态':scope,'行号':n['line']})
dump('01-窗口控件图标引用.csv',refs);dump('05-图片对象另册.csv',objects)
calls=[]
for p in sorted([*(root/'Forms').glob('*.pas'),*(root/'Units').glob('*.pas'),*(root/'Units').glob('*.inc')]):
 s=clean(p.read_text(encoding='utf8',errors='replace'))
 for m in re.finditer(r'LoadImageInto(?:Bitmap|List)\s*\(([^;]*?)\)\s*;',s,re.S):
  names=re.findall(r"'([^']+)'",m[1]);calls.append({'文件':str(p.relative_to(root)),'行号':s[:m.start()].count('\n')+1,'调用':' '.join(m[0].split()),'图标键':names[-1] if names else '动态值'})
 for m in re.finditer(r'(?:Glyph\.Assign\(draw\w+Icon\([^;]+|LoadFromLazarusResource\([^;]+|Screen\.Cursors\[[^;]+LoadCursor[^;]+)',s):
  calls.append({'文件':str(p.relative_to(root)),'行号':s[:m.start()].count('\n')+1,'调用':' '.join(m[0].split()),'图标键':'代码绘制/资源/光标'})
dump('02-代码加载与绘制引用.csv',calls)
assets=[]
for base in [root/'Themes',root/'Cursors']:
 for p in sorted(base.rglob('*')):
  if p.suffix.lower() not in ('.svg','.png','.ico','.cur','.bmp','.xpm'):continue
  assets.append({'路径':str(p.relative_to(root)),'图标键':p.stem,'格式':p.suffix,'尺寸目录':p.parent.name,'字节':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
for name in ['FreeShip.ico','FreeShip.png','gplv3-or-later.png','gplv3-or-later-sm.png']:
 p=root/name
 if p.exists():assets.append({'路径':name,'图标键':p.stem,'格式':p.suffix,'尺寸目录':'root','字节':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
dump('03-资源文件清单.csv',assets)
keys=collections.defaultdict(list)
for r in refs:
 if r['图标键']:keys[r['图标键']].append(r['文件']+':'+r['控件路径'])
for c in calls:
 if c['图标键'] not in ('动态值','代码绘制/资源/光标'):keys[c['图标键']].append(c['文件']+':'+str(c['行号']))
rows=[]
for k,uses in sorted(keys.items()):rows.append({'图标键':k,'引用数':len(uses),'窗口数':len(set(x.split(':')[0] for x in uses)),'默认主题24像素资源':str((root/f'Themes/Default/icons/24/{k}.png').exists()),'使用位置':' | '.join(uses)})
dump('04-按功能图标去重.csv',rows)
summary={'lfm_files_with_icons':len(set(r['文件'] for r in refs)),'lfm_references':len(refs),'actions':sum(r['控件类型']=='TAction' for r in refs),'nonaction_refs':sum(r['控件类型']!='TAction' for r in refs),'code_calls':len(calls),'legacy_named_keys':len(keys),'asset_files':len(assets),'default24_names':len(list((root/'Themes/Default/icons/24').glob('*.png'))),'studio_assets':len(list((root/'Themes/StudioLight/toolbar/20').glob('*.png'))),'pictures':len(objects)}
(out/'统计口径.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(summary,ensure_ascii=False))
print('TOP WINDOWS',collections.Counter(r['文件'] for r in refs).most_common(12))
print('NAMED CALLS',sorted(set(c['图标键'] for c in calls)))
