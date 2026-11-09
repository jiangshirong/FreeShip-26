from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,csv,collections,textwrap,re
r=Path('K:/');o=r/'Docs/图标盘点';summary=json.loads((o/'统计口径.json').read_text(encoding='utf8'))
def atlas(paths,name):
 cols=8;cw=154;ch=105;im=Image.new('RGB',(cols*cw,((len(paths)+cols-1)//cols)*ch),(247,249,247));d=ImageDraw.Draw(im);f=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',12)
 for i,p in enumerate(paths):
  x=(i%cols)*cw;y=(i//cols)*ch
  a=Image.open(p).convert('RGBA');a.thumbnail((44,44),Image.Resampling.LANCZOS)
  if a.width<40:a=a.resize((a.width*2,a.height*2),Image.Resampling.NEAREST)
  im.paste(a,(x+(cw-a.width)//2,y+9),a)
  label=p.stem
  lines=textwrap.wrap(label,22)
  for j,line in enumerate(lines[:3]):d.text((x+6,y+58+j*14),line,fill=(32,60,55),font=f)
 im.save(o/name)
active=[('Menu','菜单'),('Edge','线'),('Face','面'),('Point','点'),('ControlNet','控制网格'),('Layers','图层'),('Sections','剖面线'),('LinesPlan','型线图'),('Calculate','计算'),('Info','关于'),('More','更多（宽度不足时）')]
atlas([r/f'Themes/StudioLight/toolbar/20/{k}.png' for k,_ in active],'当前顶栏图标.png')
atlas([p for p in sorted((r/'Themes/Default/icons/24').glob('*.png')) if p.stem not in ['icon-sprite-24','00-freeship']],'旧主题图标总览.png')
refs=list(csv.DictReader((o/'01-窗口控件图标引用.csv').open(encoding='utf-8-sig')))
counts=collections.Counter(x['文件'] for x in refs)
main=sorted(set(x['图标键'] for x in refs if x['文件']=='Forms\\Main.lfm' and x['控件类型']=='TAction'))
text='''# RongShip 图标使用盘点

盘点日期：2026-10-29。对象：当前工作区的源码和资源；本次没有修改程序或操作桌面。

## 统计口径

这里的“引用位置”“功能图标键”“图片文件”是三种不同数量，不能相加。LFM 的 Action 和使用它的菜单/按钮各有记录，但可能共用一枚图标；不同尺寸、不同主题也可能重复同一图形。

这是可追溯的静态清单，不是对所有窗口逐个打开后的可见性验收。动态显示、条件编译、旧窗口和已隐藏入口均有可能仍保留资源。不能把以下全部引用直接当作 Figma 必须新画的数量。

'''
text+='| 统计项 | 数量 | 说明 |\n|---|---:|---|\n'
for label,key,note in [('包含图标引用的窗体/框架资源文件','lfm_files_with_icons','含旧资源，不等于55个当前可见窗口'),('窗体静态引用记录','lfm_references','包括动作定义和控件使用，存在复用'),('其中动作定义','actions','其余为控件引用或标准按钮图标'),('代码加载/绘制/光标调用记录','code_calls','动态顶栏数组另行列出'),('传统界面具名图标键','legacy_named_keys','动作名/加载名；不含全部匿名内嵌Glyph'),('StudioLight顶栏PNG图标名','studio_assets','其中11个当前接入，11个仅保留资源'),('主题、光标及根目录相关资源文件','asset_files','多尺寸、多主题、预览图均计入，绝非设计枚数')]:text+=f'| {label} | {summary[key]} | {note} |\n'
text+='''
## 1. 当前主窗口顶栏：11 枚接入图标

通常显示10枚，“更多”只在窗口宽度不足时显示。入口已删除的图标不列入此表。

| Figma 建议名称 | 当前功能 | 来源 |
|---|---|---|
'''
for k,zh in active:text+=f'| toolbar/{k} | {zh} | Themes/StudioLight/toolbar/20/{k}.png |\n'
text+='''
同目录仍有 NewModel、Open、Save、Undo、Redo、Deselect、ZoomIn、ZoomOut、Fit、Background、Hydrostatics 共11枚未接入当前顶栏的资源。它们对应的其他菜单功能可能仍存在，不能据此认为整项功能已删除。

现有顶栏加载20、25、30、40像素四档。source 文件夹保留SVG，但并非每个PNG都有对应SVG；例如 Calculate、Info 需要补齐可编辑源稿。

## 2. 主菜单及“计算”下拉菜单

菜单与顶栏复用功能图标。完整动作及使用位置见CSV，不应为同一个“图层”或“型线图”分别画两种风格。

主窗口资源中带图标索引的动作名如下（含旧工具栏及当前隐藏功能，待按发布范围筛选）：

'''+', '.join(main)+'''

当前旧工具栏 PanelMain 已隐藏。阻力、推进、部分流体分析菜单也有运行时隐藏逻辑；原资源仍被收录，属于后续范围，不是当前可见入口证明。

## 3. 四视图右键菜单及视图内窗口按钮

- 相机：内嵌 ImagesHull 图像列表索引0，只在透视视图显示；当前图形语义与相机不符，建议优先重画。
- 背景影像：内嵌图像列表索引7；其加载/保存子动作也保留索引。此前仅移除了顶栏入口，当前源码和右键菜单仍存在，纳入本清单。
- 打印：同一内嵌列表中的打印图标。
- 模式、亮度、保存图片、显示控制点坐标：当前没有明确独立图标赋值，不应计为已有图标；如要全菜单图标化，这是新增范围。
- 子视图标题栏：最小化、最大化、恢复，来自 Units/MDIPanel.inc 的代码绘制，不会随替换PNG自动更新。
- 子视图关闭、系统菜单/Logo：代码仍可生成，但当前四视图按钮集合没有启用它们，不列为当前必画项。
- 主窗口、普通对话框的系统标题栏关闭/最大化按钮由Windows绘制，通常不在应用图标替换范围内；左上角应用Logo属于应用品牌资源。

## 4. 编辑、分析和设置窗口

下面列出所有含静态图标引用的资源文件。数量是该文件中“动作定义＋控件使用”的记录数，不是独立图形数；逐控件位置、文字、索引及来源见01号CSV。窗口中的通用确定、取消、打印、保存、计算应统一复用。

| 窗口/框架源文件 | 静态引用记录数 |
|---|---:|
'''
for file,n in sorted(counts.items()):text+=f'| {file} | {n} |\n'
text+='''
图层窗口特别注意：代码加载 ExitProgram、NewLayer、DeleteEmptyLayers、MoveUp、MoveDown。这里的ExitProgram图形是旧的关闭动作资源，不等于主菜单“退出”入口仍然存在。

## 5. 关于、品牌与光标

- 关于页底部“版本信息”和“许可证信息”：2个资源名 Info、Legal，嵌入 Forms/RongShipAboutIcons.lrs；Info可与顶栏“关于”共用图形，需统一资源出口。
- 应用品牌图标：FreeShip.ico、FreeShip.png 及可执行程序资源；文件名仍为FreeShip不代表展示品牌必须如此。
- 自定义光标：当前代码明确加载 Rotate、SetOrigin、SetScale、ColorPicker、SetPoint、SetLine、SetSpline 共7类，32/48像素。另有 SmallCross 等文件存在，资源文件存在不等于当前已加载。
- 平移、拖动边框、文本、系统忙碌等还使用Windows/LCL标准光标，避免误作应用按钮图标。
- Logo、GPL标记、示意船体图、文件预览及颜色样本属于品牌/内容/状态图形，另见05号CSV，不能都当作操作图标重画。
- Windows原生打开/另存为窗口的文件夹、磁盘、文件类型图标由系统提供，不需要在Figma重新设计；旧自定义文件选择窗口的图标引用仍保留在清单，须与原生流程区分。

## 建议的Figma交付结构

1. 先做当前顶栏11枚、子视图最小化/最大化/恢复3枚、关于页Legal1枚，合计15个图形；关于页Info复用顶栏Info。品牌Logo单独设计。这是第一批建议，不是全软件图标总数。
2. 第二批覆盖通用确定/取消、增删、上下移动、保存/打印/计算，以及右键相机等当前入口；按照04号CSV复用已有语义。
3. 第三批再覆盖低频分析窗口和当前隐藏功能。不要为已删除的工具栏按钮增加可见入口。
4. 功能图标源稿统一SVG，按实际使用补PNG。当前新顶栏需要20/25/30/40档；旧主题使用16/24/32/48/64/96/128档。可先统一源稿，再按接入计划导出，不必手画每种尺寸。
5. 建议在Figma区分 default、hover、pressed、disabled、active（开关/模式）、menu-open（展开菜单）。这些多数应由控件配色表达，不需要每种状态都复制一张不同图形。
6. 图标键尽量使用清单中的稳定英文名称，中文功能写在说明中。源稿与PNG均保留透明背景；光标另外提供热点坐标，不能直接把按钮PNG当光标。

## 文件索引

- 01-窗口控件图标引用.csv：逐窗体、逐控件和动作的完整静态引用。
- 02-代码加载与绘制引用.csv：运行时代码加载、代码绘制和自定义光标线索。
- 03-资源文件清单.csv：资源路径、格式、大小及SHA256，便于排查重复文件。
- 04-按功能图标去重.csv：具名图标键与全部引用位置，不包括未能解析身份的匿名Glyph。
- 05-图片对象另册.csv：图片内容对象，和操作图标分开审核。
- 当前顶栏图标.png、旧主题图标总览.png：现有资源参考图；参考图不证明每枚图标都在当前界面显示。

后续导入新设计时需同时处理三条路径：StudioLight外部PNG、Default主题/窗体内嵌图像列表、代码绘制及LRS资源。只替换顶栏图片不能完成全软件统一。
'''
(o/'图标使用清单.md').write_text(text,encoding='utf8')
print('report',o/'图标使用清单.md');print('main_action_count',len(main))
