# 功能迁移清单

核对日期：2026-11-04。源码位置以 `FreeShip-Plus-Lazarus` 为根；测试位于同级 `FreeShip-Tests`。函数名比行号更适合后续追踪。

2026-11-09 补充：[保存确认](SAVE-PROTECTION.md)现在核对准备凭据和已安装文件，
拒绝过期/错误确认，支持丢失回复后的幂等重试；不是仅凭请求清除未保存标记。

最新补齐 [Michlet 旧三种船体方式、波形区域及波形导入原点](MICHLET-MULTIHULL.md)。
AddMass 已接入[实际调用、矩阵和经验公式后处理](ADDED-MASS.md)；输入仍限对称单体，
数值求解器仍为独立的原程序。[三维型值预览](GRID-PREVIEW.md)现已接入 Michlet 与 AddMass，
完整视觉保真、真实桌面和发行验收继续分别跟踪。

用户已确认 **Qt 型线图试点和 M1 新主窗口的使用体验**。M2 模型编辑命令及对应 Qt 入口现已补齐，范围和验证边界见[完成清单](MODEL-EDITING-COMPLETION.md)。这不等于全部窗口视觉和专业流程已迁移；各项构建和回归状态以 [STATUS.md](STATUS.md) 及对应报告为准。

## 怎么推进

**UI 是贯穿所有阶段的正式工作。** 按用户要求保留既有 StudioLight 外观、图标、
布局和交互；逐项状态见 [界面保真迁移](UI-FIDELITY.md)。Qt 试点的临时界面不是
最终设计。每个功能都要同时记录计算、命令、界面与使用验收，不能只迁移后端。

| 阶段 | 做到什么才进入下一步 |
|---|---|
| M1，已交付 | 文档只有一个所有者；Qt 各视图看到同一版本；点编辑、撤销、保存重开能对照原模型；保存失败不丢文件；保留已验收型线图体验。计算仍由 Pascal 服务完成。 |
| M2，编辑功能 | 模型编辑范围已接入选择、点边面操作、约束与点组、曲线、图层、交线、背景图、项目设置和船型变换；保存、撤销及失败恢复分别验证。桌面体验和完整视觉保真仍待验收。 |
| M3，专业流程 | 静水力、稳性、阻力、螺旋桨、板材展开和格式交接。先保持计算输入输出可比，再替换界面或计算实现。 |
| M4，原生核心 | 按读写、几何、求交、积分等模块逐个替换 Pascal。每个替换都有独立几何或数值预期，并与当前可靠版本交叉检查。 |
| M5，发行 | 安装包、资源、许可、升级与配置迁移；高 DPI、不同显卡、中文/俄文路径等实际使用验收。满足常用工作流后再决定正式入口切换。 |

不能只按“界面已经做出来”计进度。每项至少记录：代码接入、自动回归、实际使用确认；缺一项就注明缺什么。

用户补充了 ShipCAD 参考实现，已完成[源码评估](SHIPCAD.md)。M2 当前批次完成后，
建立了[独立的平面/样条 C++ 对照目标](NATIVE-GEOMETRY.md)，随后完成
[静态控制网导出与逐级细分验证](NATIVE-SUBDIVISION.md)，现已新增
[平面剖切与交线拼接候选](NATIVE-SECTIONS.md)、[显式交线简化候选](NATIVE-SIMPLIFY.md)
和[型线图曲线投影与镜像候选](NATIVE-LINESPLAN-CURVES.md)。
这些模块现已接入[完整型线图生成和 DXF](NATIVE-LINESPLAN-APP.md)，并覆盖斜剖线、
编辑、工程设置及图层变化后的更新。文档状态、结构读取、控制网保存、点编辑、
变换、约束，以及[面板、圆柱、求交和 Lackenby](NATIVE-CONSTRUCTION.md)已接入 C++。
其余建模命令及[工程设置、图层和剖面定义](NATIVE-PROJECT-DEFINITIONS.md)也已迁入 C++。
[背景图片、显示和辅助曲线定义](NATIVE-AUXILIARY-EDITING.md) 也已迁入 C++。
[四视图网格、法线和控制曲线显示样条](NATIVE-SURFACE-DISPLAY.md) 复用 C++ 细分。
[打开、新建、导入和 FBM 版本转换](NATIVE-DOCUMENT-CREATION.md) 已迁入 C++。
[流线追踪和计算参数保存](NATIVE-FLOW-ANALYSIS-SETTINGS.md) 已迁入 C++。
[静水力与稳性内核](NATIVE-HYDROSTATICS.md)、[五项推进流程及默认参数](NATIVE-PROPULSION.md)
以及 [十种阻力流程](NATIVE-RESISTANCE.md)、[板材展开计算](NATIVE-PLATES.md)、
[IGES 曲面准备](NATIVE-CAD.md)、[模型快照与标注](NATIVE-PUBLICATION.md) 也已迁入 C++。
[偏好与绘图文字初始化](NATIVE-PREFERENCES.md) 已迁入 C++，正常打开、新建、导入
不再启动 Pascal。旧诊断和未迁移工具的兼容入口仍保留；[Qt 偏好窗口](PREFERENCES-UI.md)
已接入常规、目录、图形设置。语言支持另由翻译模型处理，整体视觉验收见 UI 清单。
ShipCAD 是可选参考，各模块可以复用、改写或重新实现，不要求沿用其旧架构。
ShipCAD 的样条参数化与当前 Plus 不同，文件基线也较早；其原生 FBM 读写另做兼容和
保存验收。已经顺畅的 Qt 绘图和当前事务层继续使用。

## 1. 文件安全和兼容性

| 要保留的行为 | 旧代码入口 | Qt 迁移范围与验收 |
|---|---|---|
| 新建、打开、最近文件、未保存提醒、另存为 | `Forms/Main.pas` 的 `NewModelExecute`、`LoadFileExecute`、`LoadNamedFile`、`FLoadRecentFile`；`Units/FreeEdit.inc` 的 `File_Load`、`File_Save`、`File_SaveAs` | 已接入新建船型、FBM 打开、保存、重开、最近十个文件和启动恢复。新建先生成候选，失败保留旧文档；坏文件不能清空当前模型，取消保存不能被当作保存成功。 |
| 原生 FBM 版本及旧计算参数 | `Units/FreeShip.inc` 的 `LoadProject`、`SaveProject`、`DetectMinFileVersion`；`Units/FreeProjectSettings.inc` 的 `LoadBinary`、`SaveBinary` | 已接入 C++ 文档构造、版本转换、建模与设置字段写入；未修改文件仍原样保存，派生缓存随输入失效。Qt 快照不替代完整 FBM，见 [文档核心](NATIVE-DOCUMENT.md) 和[控制点编辑](NATIVE-POINT-EDITING.md)。 |
| 失败不破坏正式文件和备份 | `Units/FreeSafeFile.pas` 的 `SaveProjectFile`、`ReplaceFileAtomically`；`Units/FreeFileBuffer.pas` | 服务先写私有文件，Qt 安装到目标后才确认保存点。现已补齐 `.Bak`、无变化不轮换、原文件冲突检测和失败回退；两个会话、真实文件占用及确认中断已验证，详见[保存保护](SAVE-PROTECTION.md)。硬件断电与不遵守锁的外部写入竞争仍有边界。 |
| 损坏文件防护、读写中断 | `Units/FreeFileBuffer.pas`；`Units/FreeSubdivisionSurface.inc` 的 `LoadBinary`；各集合读取函数 | 延续长度、数量、索引、零进度检查。用户选中损坏文件后，旧文档、撤销和选择仍可用。 |
| 恢复快照、启动容错 | `Forms/Main.pas` 的 `InitiallyLoadModel`；`Units/FreeShip.inc` 的 `FGetPreview` | 已接入完整 FBM 恢复副本、重启提示、未保存状态恢复、另存为和关闭清理；并发窗口、损坏副本和写入失败分别处理。见 [未保存船模的恢复](RECOVERY.md)。撤销历史和未提交输入不持久化。 |

回归入口：`save_safety.py`、`file_safety.py`、`buffer_safety.py`、`legacy_compatibility.py`、`qt_recovery.py`、`qt_save_protection.py`。上游记录 **01–04、08、09、17** 在当前 Pascal 代码已按记录范围修复，迁移时要保留；**O01** 的恢复副本已在 Qt 接入。恢复容器有校验，原 FBM 格式本身没有校验和，不能承诺识别所有恰好落在字段边界的截断。

## 2. 模型、拓扑、编辑与撤销

| 要保留的行为 | 旧代码入口 | Qt 迁移范围与验收 |
|---|---|---|
| 控点、边、面、折痕、细分曲面 | `Units/FreeGeometry.pas`；`FreeSubdivisionSurface.inc` 的 `Rebuild`、`SubDivide`；`FreeSubdivisionControlPoint.inc` 等 | 编辑状态和建模命令已迁入 C++；四视图网格、法线和控制曲线显示与型线图复用 C++ 细分结果。模型快照也直接从原生文档生成，常用路径不再重建旧曲面。三角网是显示结果，不是可编辑控制网。 |
| 选取、框选、锁点、锚点、直线约束 | `FreeEdit.inc` 的 `Selection_*`、`Point_Lock`、`Point_AnchorToPoint`、`Point_ProjectStraightLinePermanentConstraint` | 已接入点、边、面选择及点框选、全选/反选/漏点、锁定/解锁、锚点及硬锚点、直线约束、对齐/重合和解除约束。点组支持建立、编辑、解组、中心标记选择及整组移动；成员点仍可单独编辑。约束循环和不可编辑关联被拒绝。 |
| 增删、拆分、合并、挤出、镜像、变换 | `FreeEdit.inc` 的 `Point_Collapse`、`Edge_Split`、`Edge_Collapse`、`Edge_Extrude`、`Face_New`、`Face_MirrorPlane`、`Face_Move/Scale/Rotate`、`Model_LackenbyTransformation` | 基本增删、折叠合并、连接点、批量拆边、点/边挤出、面镜像、船中旋转和 Lackenby 变换均已接入。另有平面插点、图层交点、网格面板、圆柱、翻面、删除负舷、组装及图层自动分组。ID 删除留空位、撤销恢复原号，新对象不复用旧号；详见[完整范围](MODEL-EDITING-COMPLETION.md)。 |
| 控制曲线、站线、水线、纵剖线、斜剖线、流线 | `FreeEdit.inc` 的 `Curve_Add`、`Intersection_Add/Change`、`Flowline_Add`；`FreeIntersection.inc` 的 `Rebuild` | 四类剖面已接入增删、批量添加、改距和曲率勾选；C++ 求交结果同时用于四视图与型线图。控制曲线和流线已接入创建、选择、删除、显示、撤销及文件往返；分叉边自动分链，选中控制曲线显示曲率梳。见[曲线记录](AUXILIARY-CURVES.md)。 |
| 撤销、重做、取消、保存后的修改标记 | `FreeEdit.inc` 的 `CreateUndoObject`、`Undo`、`Redo`；`FreeUndoObject.inc` 的 `Accept`、`Restore` | C++ 文档服务持有完整历史与保存点，并直接发布各版本的界面快照。取消、无变化操作不截断重做；撤销到保存位置恢复未修改状态；失败及运算进程退出不改变已提交状态。 |
| 几何检查 | `FreeEdit.inc` 的 `Model_Check`；`FreeSubdivisionSurface.inc` 的 `CheckIntegrity` | 已接入只读检查和问题定位，检查漏点、无面点、非流形、退化面、朝向与约束问题。面朝向修复须单独执行，可以撤销；检查本身不修改模型。 |

回归入口：`undo_background_safety.py`、`file_safety.py`、`section_incremental.py` 及新迁移回归。**10** 已在旧版修复；**O02、O08、O10** 仍有工作。现有原生撤销序列化会排序控点，稳定 ID 与对象关联必须单独测试。

## 3. 四视图

旧入口是 `Forms/freehullformwindow_panel.pas` 的 `ViewportRedraw`、`ViewportMouseDown/Move/Up`，投影、交互及输出在 `Units/FreeViewport.inc`，模型绘制在 `FreeSubdivisionSurface.inc::Draw`。

| 功能 | 当前迁移边界 |
|---|---|
| 透视、侧视、俯视、横剖视，缩放和平移 | M1 已接入并通过自动验证，用户确认主窗口顺畅。与型线图共享文档版本，各窗口相机独立；不同屏幕与显卡仍需分别验收。 |
| 控制网、内边、曲率、法向、交线、标记、双舷显示、精度选项 | 已接入控制网、四类剖面、剖面/控制曲线曲率梳、半船/双舷、控制曲线、流线、选中面的细分顶点法向及标记显示。标记可导入、删除、撤销和保存；不把这些功能覆盖当作桌面四视图观感等价。 |
| 图层对称、可见性、水下颜色 | M1 快照包含部分字段。对称由图层决定；原始半船网格只镜像一次。水面口径需与旧版及静水力流程分别核对。 |
| 网格、控制点坐标、静水力标注、进入动画 | 已接入 Qt；显示直接使用同版本模型快照，不改模型或型线图结果。包含独立的布局/动画预期和完整图片输出，见 [本轮记录](OVERLAYS.md)。完整视觉一致性仍待核对。 |
| 多窗口、右键菜单、焦点和快捷键、打印/图片输出 | 四视图右键显示菜单已接入，其他入口继续逐项迁移。隐藏窗口内的合成事件只证明事件处理，不能证明桌面焦点和拖曳体验。 |

原有 `window`、`ui`、`overlay-animation` 等测试组保留作为旧版回归。其中与 Lazarus 控件树、句柄或消息相关的探针不会原封不动适用于 Qt；输入行为和预期结果可复用。真实帧率不能由离屏图像读回耗时直接推算。

## 4. 型线图

**已确认：** 用户认可 Qt 试点的平移、缩放和整体使用体验。试点使用保留的完整场景，独立场景入口已移除旧窗口重复绘制。它仍使用 Pascal 计算，不是 C++ 原生几何迁移完成。

**正式工程的新进度：** 完整型线图及 DXF 已切换为 C++ 生成，原有 Qt 渲染器保持。
自动回归核对全部填色及顺序、完整 DXF、编辑更新、失败回滚和缓存；显示投影、
折角保留及文字锚点差异分别保留证据。这一轮的新计算路径尚未由用户实际体验确认。

旧入口：`Forms/Main.pas::ShowLinesplanExecute`、`Forms/FreeLinesplanFrme.pas::ViewportRedraw`、`ExportDXFExecute`、`SaveBitmapExecute`、`PrintExecute`。拆分入口在 `FreeShip-QtPilot/bridge/FreeLinesplanScene.pas`；新工程保留 `src/scene.*`、`src/view.*`、`src/gpu_fill.*`。

| 后续工作 | 要保留的结果 |
|---|---|
| 接入正式文档、点编辑、撤销和重开 | M1 已接入并验证四视图与型线图属于同一版本；持续保留迟到结果不得覆盖较新文档的约束。 |
| 首次生成、局部更新和跨重启缓存 | 正常 Qt 已接入跨重启完整缓存和按实际曲面依赖复用的局部剖面求交；坏缓存重建，背景进程不重复构造绘图路径。细分网格、填色和全局布局仍全量生成，见 [范围及验证](LINESPLAN-CACHE.md)。 |
| 标签、颜色、DPI 与 GPU/CPU 回退 | 保留完整填色；放大再缩回、移出再移回不能丢失未见区域。显示用浮点近似不进入模型或导出。 |
| DXF、图片、打印 | 导出完整模型，不能导出当前屏幕裁切或交互预览。打印、标签可读性及不同硬件仍分别验收。 |

回归入口：`qt_linesplan_pilot.py`（统一组 `qt-linesplan`）、`qt/linesplan_tests.cpp`、`dxf_geometry.py`；旧版性能和缓存检查为 `linesplan_performance.py`、`section_incremental.py`。参考 [试点边界](../../../FreeShip-QtPilot/Docs/试点边界与验收.md)。旧屏幕与 DXF 水线投影存在已记录差异，不能仅以“和旧屏幕相同”证明几何正确。

正式常驻服务另暴露了旧 DXF 折痕线提取会翻转共享边、按内存地址随机分链的问题。本轮在新桥接中处理，旧产品源码保留；这是导出副作用与确定性修复，不是曲面精度升级。新流程的完整场景/实体比较仍须通过；冻结旧 DXF 若只有该分链差异，则保留 `review` 和独立精确几何证据，不能改记成相同输出。范围详见 [接口审查中的折痕线说明](ARCHITECTURE-REVIEW.md#本轮另查出的折痕线导出副作用)。

## 5. 图层、背景图和设置

| 功能 | 旧代码入口 | 后续迁移要求 |
|---|---|---|
| 图层创建、属性、显示、对称、静水力参与、展开和型线图参与 | `FreeSubdivisionLayer.inc`；`Forms/FreeLayerDlg.pas`、`FreeLayerVisibilityDlg.pas`；`FreeEdit.inc::Layer_*` | 已接入图层新建、空层删除/清理、属性及面归属编辑、撤销/保存回读；不同用途标志独立保存。展开窗口和 C++ 展开计算均已接入。 |
| 背景图导入、原点、参考比例、边框、透明色、混合 | `FreeEdit.inc::BackgroundImage_Open`；`FreeBackgroundImage.inc`；`freehullformwindow_panel.pas::Background*Execute` | 已接入三种正交视图，含显示、容差、导出、清除及撤销/保存回读；失败保留旧图，JPEG 原数据不反复重压缩。桌面与旧栅格观感仍待验收，见 [背景图片](BACKGROUND-IMAGES.md)。 |
| 主尺度、单位、密度、设计吃水、剖面分割位置 | `FreeProjectSettings.inc`；`Forms/FreeProjectSettingsDlg.pas` | M2 已接入工程设置草稿、一次提交及撤销/保存回读；单位切换换算完整模型和关联量。桌面新表单验收仍待完成。 |
| 外观、语言、路径、最近项目 | `FreePreferences.inc`、`FreePathResolution.pas`、`FreeSafeLanguage.pas` | Qt 使用独立配置；已接入用户明确选择 INI/DTA 的只读导入，主题、目录及通用参数先进入草稿，见 [偏好设置](PREFERENCES-UI.md)。语言由独立翻译工作流负责；旧窗口布局和最近文件不导入，不覆盖旧软件设置。 |

回归入口：`units_conversion.py`、`undo_background_safety.py`、`legacy_compatibility.py`、`dialog_opening.py`、`freeship_ui_test.py`。**05、06、14** 已在旧版按记录修复；**O03、O09** 尚未完成。旧 JPEG 重存像素差异仍是保真问题，不与释放后访问缺陷混为一项。

## 6. 静水力与稳性

旧入口：`FreeEdit.inc::Hydrostatics_Calculate/Hydrostatics_Dialog/Hydrostatics_Crosscurves`；`Units/FreeHydrostaticCalc.inc::Calculate/CalculateVolume/CalculateGravity/CalculateMidshipLocation/Balance`；`Forms/FreeHydrostaticsDlg.pas`、`FreeHydrostaticsResultsDlg.pas`、`FreeCrosscurvesDlg.pas`。

已接入 Qt 的设计静水力、不同吃水表、排水量和浮心、面积与系数、固定/自由纵倾平衡、KN/GZ、积分和四种报告导出。2026-11-03 这些入口的计算内核和默认参数已迁入 C++，复用不可变文档的网格，参数支持撤销与 FBM 保存重开。平衡求解有明确残差和失败状态。2026-11-09 已替换[非对称截面积、水线面与稳心路径](ASYMMETRIC-HYDROSTATICS.md)，补齐 Bonjean/SAC；后续接入[实际侧投影及四视图静水力标注](LATERAL-PROJECTION.md)。旧单侧进流角和全对称历史积分规则的统一替换仍有边界。经验天气衡准和规范判定仍未迁移，不能把一般稳性曲线等同于完整破舱稳性或审批软件。详见 [Qt 流程](HYDROSTATICS.md) 与 [C++ 内核](NATIVE-HYDROSTATICS.md)。

验收固定单位、密度、坐标基准、参与计算图层、倾角和纵倾，再对照结果。`qt-hydrostatics` 包含解析箱体、真实船型、旧积分直接观察、Qt 回调、取消、参数往返和报告导出；既有 `units_conversion.py`、`hydro_corrections.py`、`hydro_search.py` 仍保留。**05、07、12、15** 的原修复保持；数值搜索的有限样本不等于全局保证。**O03、O11** 涉及的普通工况、重心和基准已在窗口及报告说明，其余专业规范范围仍待处理。

## 7. 阻力、螺旋桨和外部工具

旧入口：`FreeEdit.inc::Resistance_*`、`Propeller_Task1..5`、`Hydrodyn_*`；对应 `Forms/FreeResistance_*Dlg.pas`、`FreePropeller_*Dlg.pas`、`FreeHydrodyn_*Dlg.pas`。进程和文件契约在 `Units/FreeProcess.pas::RunFreeProcess/ExecuteFreePlugin`、`FreeCalculationFiles.pas::ExecuteFreeCalculation/ValidateFreeCalculationOutput`。

阻力十个入口和推进五个入口已接入 Qt 参数、取消、结果、工况保存及报告。两组的默认值、公式、协议和后处理现已迁入 C++，部分方法仍调用原外部求解器。12 个主方法样本完成计算；CALCPROP Task1–3 的原二进制无输出，B 系列的 DOS 求解器不能直接运行，空实现没有冒充已完成。方法及原验证见 [PERFORMANCE.md](PERFORMANCE.md)，最新实现与已修错误见 [NATIVE-PROPULSION.md](NATIVE-PROPULSION.md) 和 [NATIVE-RESISTANCE.md](NATIVE-RESISTANCE.md)。超时、取消、退出码、结果格式和工作目录隔离保留。

2026-11-04：Hydrodyn 的空气动力、操纵性预估、加速／制动三个有效工具已迁移到 C++ 和 Qt，仍调用原 Ishercof、ManeuvPP、RVRSSHIP。参数保存、撤销、报告、完整长时间历程及计算间缓存有专项回归，见 [水动力专用工具](HYDRODYNAMICS-TOOLS.md)。原 Task2/Task3 调用已注释，不虚构可用功能；Task4 属于仍待迁移的 AddMass 方向。螺旋桨初步设计的数据库按钮已接入私有进程和结束回调，DBFview.EXE 缺失时明确报错；真实数据库查看仍未验证。经验系数和惯性矩单位的源码矛盾保留待复核。

**11** 已修复的范围包括 29 个计算调用的独立工作目录、输出校验和共享结果整组发布；不等于所有外部程序都完成真实数值验收。既有测试是 `process_safety.py`、`process_callers.py`、`calculation_contracts.py`。新 `qt-performance` 补充 HOLLENBH、OORTMERS、fungleib、hship、PropPred、FPPCALC 的独立文件对照及 Qt 实际回调。所有方法仍需工程参考数据。**O12** 继续保留。

## 8. 板材展开、导入导出与打印

| 功能 | 旧代码入口 | 迁移验收 |
|---|---|---|
| 板材展开、镜像、边误差、TXT/DXF/打印 | `FreeEdit.inc::Layer_Develop`；`FreeDevelopedPatch.inc::Unroll/SaveToDXF/SaveToTextFile`；`Forms/FreeExpanedPlatesDlg.pas` | 已接入 Qt 只读展开、布局和显示、四类剖面、完整边界输出、按比例 PDF 及打印预览。展开计算已迁入 C++，四档精度、平面、圆柱、双曲率、孔洞、对称及英制分开验证。实体打印和加工链另验；见 [C++ 板材展开](NATIVE-PLATES.md)。 |
| STL、OBJ、IGES、FEF、DXF、型值表等输出 | `FreeEdit.inc::File_Export*`；`FreeIGESUnit.pas`；`FreeNURBSurface.inc` | 型线图 DXF/图像、板材输出及 C++ OBJ/STL/FEF 已接入。另接入 IGES 曲面、三维 DXF 样条/面、原侧型值点列和坐标点；[IGES 曲面准备](NATIVE-CAD.md) 也已迁入 C++，分别保留隐藏图层和相邻图层属性。另接入 [C++ GHS 对称船体输出](GHS-EXPORT.md)，商业 GHS 读取仍未验收；已接入 [Archimedes HLL/APP 和 PAM 旧版几何模板](SHIP-FORMATS.md)，外部接收仍待验证；部件、Michlet/AddMass 输入、矩形型值表及折痕边线 DXF 已接入，见 [剩余交接](LEGACY-EXCHANGE.md)。Michlet 旧单体／双体三种方式及波形参数已补齐，见 [双体衔接](MICHLET-MULTIHULL.md)；外部接收仍待验收。见 [CAD 输出](CAD-EXPORT.md)。 |
| OBJ/STL/FEF/Hull/Polycad/VRML/型值等导入 | `FreeEdit.inc::File_Import*`；`FreeSubdivisionSurface.inc::ImportObjFile/ImportFEFFile` | OBJ/STL/FEF 已接入候选文档、校验、失败恢复和 Qt 入口。网格导入默认保形，OBJ 可选择控制网平滑；FEF 恢复原折痕和顶点类型。已接入 [Polycad 坐标网格](POLYCAD-IMPORT.md)及 [Hull、Carene、折角线与曲面文本拟合](CURVE-IMPORT.md)，保留标记、控制曲线和 Hull 站位；拟合不等于外部曲面无损交换。VRML 静态网格、部件和 Michlet 波形导入已接入，后两者追加到当前工程并支持撤销，见 [范围与验证](LEGACY-EXCHANGE.md)。 |
| 保存图片、打印比例、纸张和预览 | `FreeViewport.inc::SaveAsBitmap/Print`；各窗口的打印动作 | 已有导出文件检测不代替实际打印和路径交互验收。保存目的地必须使用用户选择值。 |

回归入口：`stl_symmetry.py`、`plate_errors.py`、`dxf_geometry.py`，以及历史脚本中仍适用的输出检查。**13、16** 已修复；**O04–O08** 仍包含加工交接、单位/缩尺、曲面质量、封闭厚度检查及保真导入需求。现有 IGES 曲面输出、板材 TXT 和打印本来就存在，不记成迁移新增功能。

## 9. 本地化、测试和发行

语言入口是 `Units/FreeLanguageSupport.pas`、`FreeSafeLanguage.pas::ApplyLanguageSafely` 及 `locale/`、`Languages/`；界面中还包含本地已完成的中俄翻译和术语调整。Qt 迁移须把文字与行为重新对应，不能只机械转成 `tr()`。新布局、快捷键冲突、字体回退、长文本及缩放仍需验证。

测试统一入口为 [`FreeShip-Tests/run_tests.py`](../../../FreeShip-Tests/run_tests.py)。旧 Pascal 探针继续守住过渡核心；Qt 专属探针链接实际产品组件；跨语言的模型样本、独立预期、故障样本和输出比较可以直接复用。控制器调用成功不等于桌面使用成功，未覆盖部分保留 `review` 或 `not_run`。

历史脚本已有逐项去向，见 [历史脚本处理清单](../../../FreeShip-Tests/历史脚本处理清单.md)。没有必要因迁移把已退役的固定布局偏好重新接入。新检查继续进入统一测试程序，不散落为临时验收脚本。

许可证、原作者说明和第三方资源来源随新工程保留。正式安装、配置导入、工具链复现和旧 `fs` 入口切换属于发行阶段，不能在功能尚未覆盖时顺带完成。

## 历史问题的记账方式

[上游问题记录](../../../FreeShip-Plus-Lazarus/Docs/上游问题记录/README.md) 固定对照上游提交 `1de17e99fb2b1cc559cdf27c5cafb595a627a641`。其中 01–17 是本地已按各自范围修复的问题；O01–O14 是更广的改进方向。前两批记录中的“未完成”是当时状态，最终状态应结合第三批和各条补充读取。

本清单是代码盘点，没有重新运行那 17 项回归，也不把旧报告自动算作新 Qt 程序通过。迁移引入的回归记到新工程，不算上游原有缺陷。后续发现旧算法有误时，保留旧版对照，同时增加独立正确性依据，不能只为了逐字节相同而复制错误。
