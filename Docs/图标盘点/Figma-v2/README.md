# FreeShip_26 第二批统一图标

日期：2026-10-31。延续第一批20×20、1.6px圆头圆角描边与深海青风格。

## 本轮实际结果

- 原Figma文件：https://www.figma.com/design/MTqjNNRAStv8Hnsbb1FCgI
- 已在原页创建72个独立Icon主组件：4个已有本地稿补入、68个新增。文件共87个Icon主组件；此前15个组件ID与六态按钮不改。
- 新组件实际ID见figma-master-ledger.json，每个组件的用途、状态和复用关系见图标设计与接入映射.csv。
- 现有颜色变量被复用，导入矢量前景绑定color/ink，遮罩底绑定color/surface。1.6px为图形几何，不另建重复变量。
- 各SVG独立交付，PNG从矢量直接栅格化；72×7=504份，尺寸16/20/24/25/30/32/40。
- 本轮未修改FreeShip.exe、LRS、运行时主题或功能入口。

## 已完成与未完成的验收

Figma返回了72个新组件ID与87个总数，结构创建完成。随后创建评审板和截取组件截图均被Starter套餐MCP调用限额阻止，没有把Figma视觉验收记为通过。已查看本地总览和实尺寸检查。export-check.json检查每份PNG尺寸、非空透明图形与哈希；不等同于Figma导出或Lazarus实际界面验收。

第二批72枚-设计总览.png是本地按源SVG生成的完整检查图，不是Figma截图。实尺寸清晰度检查.png显示所有七档原尺寸资源，不能用图片查看器缩小后的整体图判断16px细节。

figma-resume-gallery.js仅为未执行的总览补齐脚本，额度恢复后可复用；不得将其存在视为评审板已经创建。

## 功能与状态

图标内无语言文字。中俄英文标签由UI保留。普通动作采用Default/Hover/Pressed/Disabled；四视图、显示方式等开关另用Active；下拉入口用MenuOpen。复用此前六态IconButton及其Icon替换属性，不复制72×6个图形。

各文件格式共用ExportFile，旋转角度共用方向图标，由真实控件文字表示格式和角度；此类映射不是重新实现功能。Cancel仅表示取消未提交操作，不能因此恢复已移除的关闭按钮。缩放、撤销等图形只供当前合法入口，不恢复主顶栏已删除入口。

## 旧清单覆盖边界

107个旧资源键并非107个当前可见功能。逐项映射与排除见旧资源覆盖与剩余项.json；旧背景、旧关闭、旧窗口排列等入口不重建。

仍未包含于本轮72枚的旧键：InsertPlane, KeelRudderWizard, LayerIntersection, ShowControlCurves, ShowSubmergedArea。这些项没有假称已经补进Figma，需在确认当前实际入口和用途后继续设计。系统光标、应用Logo、低频阻力/推进分析不在本批范围。
