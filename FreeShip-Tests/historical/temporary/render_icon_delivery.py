from pathlib import Path
import json,io,html,zipfile
import resvg_py
from PIL import Image,ImageDraw,ImageFont
out=Path('K:/Docs/图标盘点/Figma-v1');d=json.loads((out/'icons.json').read_text(encoding='utf8'))
fontpath='C:/Windows/Fonts/msyh.ttc'
def font(n):return ImageFont.truetype(fontpath,n)
def icon(k,n,col='#315B55'):
 svg=d['svgs'][k].replace('#315B55',col);return Image.open(io.BytesIO(resvg_py.svg_to_bytes(svg_string=svg,width=n,height=n))).convert('RGBA')
for k in d['svgs']:
 for n in [20,25,30,40]+([16,22,32] if k in ['Minimize','Maximize','Restore'] else []):
  p=out/'PNG'/str(n);p.mkdir(parents=True,exist_ok=True);icon(k,n).save(p/(k+'.png'))
S=2;W=1360;H=1510;im=Image.new('RGB',(W*S,H*S),'#FAFCFA');g=ImageDraw.Draw(im)
def txt(x,y,s,n=14,col='#315B55'):g.text((x*S,y*S),s,font=font(n*S),fill=col)
def box(x,y,w,h,c='#FFFFFF',r=8,outline=None):g.rounded_rectangle((x*S,y*S,(x+w)*S,(y+h)*S),radius=r*S,fill=c,outline=outline,width=2)
def paste(k,x,y,n,col='#315B55'):a=icon(k,n*S,col);im.paste(a,(x*S,y*S),a)
txt(48,30,'RONGSHIP  /  ICON SYSTEM 01',12,'#087F79');txt(48,56,'造型简洁，操作清晰。',36,'#183B36');txt(48,112,'15枚矢量图标 · 圆头圆角描边 · 20px主稿 · 第一批设计样张',16)
for j,(name,col) in enumerate([('默认','#315B55'),('强调','#087F79'),('悬停','#EAF2EF'),('按下','#D6E8E2'),('禁用','#A5B5B1')]):box(48+j*220,153,24,24,col,6);txt(83+j*220,154,name,12)
txt(48,207,'01  图标总览',20,'#183B36')
for i,(k,zh,fn,loc,states,shape) in enumerate(d['items']):
 x=48+(i%5)*256;y=250+(i//5)*210;box(x,y,240,194);paste(k,x+18,y+16,40);txt(x+18,y+67,zh,16,'#183B36');txt(x+18,y+94,k,11,'#087F79');xx=x+18
 for n in [20,25,30,40]:paste(k,xx,y+124,n);txt(xx,y+167,str(n),10);xx+=50
txt(48,910,'02  六种状态，清楚表达不同操作',20,'#183B36')
for j,(name,zh,fg,bg) in enumerate([('Default','默认','#315B55','#FFFFFF'),('Hover','悬停','#183B36','#EAF2EF'),('Pressed','按下','#056B66','#D6E8E2'),('Disabled','禁用','#A5B5B1','#FFFFFF'),('Active','模式开启','#087F79','#E4F2ED'),('MenuOpen','菜单展开','#087F79','#E4F2ED')]):
 x=48+j*211;box(x,960,194,112);box(x+16,973,60,48,bg,6,'#087F79' if name=='Active' else None);paste('Info',x+36,978,20,fg);txt(x+35,1003,'图标',10,fg);txt(x+88,975,name,11);txt(x+88,1001,zh,12)
txt(48,1098,'03  顶栏实际比例示例',20,'#183B36');box(48,1142,1264,74)
for j,k in enumerate(['Menu','Edge','Face','Point','ControlNet','Layers','Sections','LinesPlan','Calculate','Info','More']):
 x=64+j*92;fg='#A5B5B1' if k in ['Edge','Face'] else '#087F79' if k=='ControlNet' else '#315B55';
 if k=='ControlNet':box(x,1152,70,52,'#E4F2ED',6,'#087F79')
 paste(k,x+25,1158,20,fg);label=next(a[1] for a in d['items'] if a[0]==k).split(' / ')[0];txt(x+max(1,(70-len(label)*11)//2),1183,label,11,fg)
txt(48,1243,'04  视图窗口与关于页',20,'#183B36');box(48,1285,600,54);txt(64,1299,'船体横剖面型线视图',13)
for j,k in enumerate(['Minimize','Maximize','Restore']):paste(k,526+j*34,1301,20)
box(672,1285,640,54);paste('Info',690,1301,20);txt(720,1299,'版本信息',14);paste('Legal',1118,1301,20);txt(1148,1299,'许可证信息',14)
txt(48,1371,'交付：SVG源稿 + 顶栏20/25/30/40px PNG + 窗口16/22/32px PNG',14)
txt(48,1408,'本图为源稿的本地渲染，Figma组件已创建；尚未完成Figma画板排版与截图检查。',13)
txt(48,1442,'设计交付，不代表软件已替换。图标本体不含语言文字，标签继续支持中、俄、英。',12)
im.save(out/'15枚图标-设计预览.png')
# Native-scale contact sheet: no upscaling of small PNGs.
native=Image.new('RGB',(900,650),'#FAFCFA');dr=ImageDraw.Draw(native)
for i,(k,zh,*_) in enumerate(d['items']):
 y=12+i*41;dr.text((12,y),k,font=font(13),fill='#315B55')
 for j,n in enumerate([20,25,30,40]):
  a=icon(k,n);native.paste(a,(180+j*110,y),a)
native.save(out/'实尺寸清晰度检查.png')
# Editable SVG overview for optional manual import, independent of MCP quota.
parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="700" viewBox="0 0 1200 700"><rect width="1200" height="700" fill="#FAFCFA"/><g font-family="Microsoft YaHei, Noto Sans SC, sans-serif" fill="#315B55"><text x="32" y="45" font-size="26">RongShip · 15 Icons</text>']
for i,(k,zh,*_) in enumerate(d['items']):
 x=32+(i%5)*236;y=80+(i//5)*195;inner=d['svgs'][k].split('>',1)[1].rsplit('</svg>',1)[0]
 parts.append(f'<g transform="translate({x},{y})"><rect width="216" height="175" rx="8" fill="white"/><g transform="translate(16,16) scale(3)" fill="none" stroke="#315B55" color="#315B55" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">{inner}</g><text x="16" y="115" font-size="16">{html.escape(zh)}</text><text x="16" y="145" font-size="12">{k}</text></g>')
parts.append('</g></svg>');(out/'图标总览-可导入Figma.svg').write_text(''.join(parts),encoding='utf8')
status='''\n\n## 实际交付状态\n\nFigma文件：https://www.figma.com/design/MTqjNNRAStv8Hnsbb1FCgI\n\n已创建15个Icon主组件、一个包含6种状态的IconButton组件集，以及26个颜色/尺寸变量（含基础色和语义别名）。Icon与Label可替换。\n\nStarter套餐MCP调用额度已耗尽，Figma内的总览与中文说明画板未创建，Figma截图检查未完成。完整中文说明在本文件，完整预览与实尺寸检查图在同目录；SVG源稿与69张透明PNG已导出。本地图像已人工查看，不代替Figma视觉验收或软件接入验收。\n'''
p=out/'第一批15枚图标-设计说明.md';s=p.read_text(encoding='utf8');p.write_text(s+status,encoding='utf8')
# Preserve exact IDs and construction scripts to resume the unfinished Figma board safely.
tmp=Path(r'C:\Users\12968\AppData\Local\Temp\FreeShipLabelIntegration')
(out/'后续排版脚本.js').write_text((tmp/'figma_board.js').read_text(encoding='utf8'),encoding='utf8')
with zipfile.ZipFile(out.parent/'RongShip-第一批15枚图标-v1.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in out.rglob('*'):
  if p.is_file():z.write(p,str(p.relative_to(out)))
print('SVG masters',len(d['svgs']),'PNG exports',len(list((out/'PNG').rglob('*.png'))));print('preview',out/'15枚图标-设计预览.png')
