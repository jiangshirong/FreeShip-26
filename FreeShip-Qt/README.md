# FreeShip_26 · Qt

这是 FreeShip_26 的正式迁移工程，沿用已验收的 Qt 型线图试点。
旧版和试点留在旁边，便于逐项对照。这里目前是开发版，还没有覆盖旧版的全部功能。

最近接入了[静水力附加报表](docs/migration/HYDRO-AUXILIARY.md)：设置开关、Bonjean/SAC、首尾水线位置和完整图层重量明细，可查看并选择保存。
功能范围和未测项见[迁移记录](docs/migration/STATUS.md)。

第一阶段打通一条实际流程：打开 FBM 船模，在四视图中选择和移动控制点，
撤销、重做、保存，再重新打开；型线图随同一个模型版本更新，可保存图片和导出 DXF。
各视图有独立镜头，平移和缩放不会触发船体重建。

完整型线图现在由 C++ 生成：控制网细分、求交、投影、填色、标签及 DXF 均已接入
实际程序。输入直接来自模型，不从四视图的画面反推。文档状态、撤销、FBM 结构和
坐标/控制网保存、基本与高级建模已迁入 C++；最新一组是
[面板、圆柱、求交与船型形变](docs/migration/NATIVE-CONSTRUCTION.md)。
点属性、点组、标记线、面片分组/组装和朝向修复也已接入 C++，见
[其余建模编辑](docs/migration/NATIVE-MODELING-COMPLETION.md)。三类建模命令
均由原生文档服务执行。[工程设置、图层和四类剖面编辑](docs/migration/NATIVE-PROJECT-DEFINITIONS.md)
也已迁入 C++，包括单位转换、保存和撤销。随后完成
[背景图片、显示和曲线定义](docs/migration/NATIVE-AUXILIARY-EDITING.md) 的原生编辑。
[四视图网格、法线和控制曲线样条](docs/migration/NATIVE-SURFACE-DISPLAY.md)，以及
[打开、新建、导入和 FBM 版本转换](docs/migration/NATIVE-DOCUMENT-CREATION.md) 已接入 C++。
流线、静水力、主要阻力/推进流程、板材展开和 IGES 曲面准备已迁入 C++。
[模型快照与标注](docs/migration/NATIVE-PUBLICATION.md) 也已直接接入 C++，
[偏好与绘图文字初始化](docs/migration/NATIVE-PREFERENCES.md) 也已迁入 C++，
常用文档路径不再启动旧版程序。独立对照用的旧诊断和未迁移工具兼容入口仍保留。

## 使用

“编辑 → 偏好设置”已接入常规、目录和图形三页，设置保存、重启恢复及模型边界见
[Qt 偏好设置](docs/migration/PREFERENCES-UI.md)。语言工作由专门的翻译模型负责。

“文件 → 导入模型／导出模型”支持 OBJ、STL、FEF。OBJ/STL 请确认文件单位与是否半船；
完整工程继续保存 FBM。支持范围和限制见 [模型交接](docs/migration/MODEL-EXCHANGE.md)。
“导出模型”另有 IGES 曲面、三维 DXF 曲线/面、型值和坐标点，
范围与单位约定见 [CAD 输出](docs/migration/CAD-EXPORT.md)。
另有 [GHS 对称船体输出](docs/migration/GHS-EXPORT.md)，使用已有横剖站；敞开甲板的水平封顶需明确勾选。

板材展开已接入“计算”菜单：在图层属性中勾选“可展开”，再生成展开图。
支持板片移动、旋转、镜像、误差查看和完整边界导出，详见 [板材展开](docs/migration/PLATES.md)。

完整的本地运行包生成后，打开 `dist/FreeShipQt.exe`。支持把一个 FBM 文件路径
作为启动参数。原来的 Win+R `fs` 入口仍指向旧版。
没有启动参数时自动打开上次成功打开或另存为的船模，“文件 → 最近打开”保留十个路径。
旧 Qt 没有保存这份记录，升级后请先成功打开一次；不恢复未保存修改。

- 左键选择控制点；在侧视图、俯视图、正视图中拖动可改坐标。
- Ctrl/Shift 点选可增减选择，正交视图空白处拖动可框选；拖动已选点可整体移动。
- 右键拖动平移，滚轮以鼠标位置为中心缩放。
- 选择点、边、面或控制曲线后，“变换”菜单可移动、缩放和旋转；边菜单可挤出边界边，面菜单可镜像所选面。操作支持撤销、重做和保存，原点与单位在参数窗口中说明。
- 右键单击视图可切换当前视图的坐标标注、网格和静水力显示；“视图”菜单中的相同开关用于四个视图。
- 在横剖、纵剖或俯视图内右键 → “背景影像”，可导入 JPEG/BMP、拖动原点、设置参考比例、调整图片边框，以及设置透明色和混合。Esc 或右键结束调整；修改支持撤销、重做和保存重开。
- 打开模型时显示控制点、辅助线和文字的进入动画；平移或缩放会立即完成动画。图片输出始终完整，不截取动画中间状态。
- 透视图空白处左键拖动旋转，也可按住 Alt 旋转。
- 从“点 → 所选对象详情”打开坐标栏，可修改选中点；未修改的坐标轴保持原值。
- 多选时坐标栏输入各方向的移动距离；选中集合含锁定或约束点时，整组移动会被拒绝。
- “工程设置”从当前模型立即打开；“图层 → 图层属性”修改已有图层，点“应用”提交。
- 主界面沿用 StudioLight 顶栏和原图标。坐标及图层属性按需打开；视图按钮可收起、恢复各视图。
- “点／边／面”菜单支持基本控制网编辑：新增点、拆边、折痕、建面、删除、面归属和锁点。建面前按轮廓顺序点选。
- “剖面线”管理横剖线、纵剖线、水线、斜剖线，支持单条/批量添加、修改距离、删除和清空当前类别。四视图同步显示，可从“视图”菜单分类型开关。
- “视图 → 双舷显示”切换半船与全船；半船横剖面按分割剖面显示前后体。剖面线的“曲率”列可勾选，视图菜单可开关曲率梳和调整比例。曲率梳在线框显示下可见。
- “图层”菜单支持新建、删除所选空图层和清理空图层；含面的图层须先转移面。上述操作都可撤销。
- “文件 → 新建船型”按主尺度和控制点数建立模型；“面”菜单可添加网格面板或圆柱。
- “点”菜单可设置控制点名称、类型，建立锚点、直线约束及点组。点组中心标记用于整组选取和移动，成员点仍可单独编辑。对齐和约束按点选顺序确定首尾点。
- “模型检查”只列出问题并定位；面朝向修复单独执行，可以撤销。“变换”菜单另有绕船中旋转和 Lackenby 船型变换。
- “视图”菜单可显示选中面的法向、管理标记；选中控制曲线后可以查看曲率梳，分叉边创建曲线时会自动分链。
- “计算”菜单提供设计静水力、吃水表和稳性曲线。可取消计算、查看表格和曲线、导出 CSV/TXT/JSON/PDF；计算范围可保存到船模，支持撤销。重心采用模型坐标，X/Y 仅随报告保留。进水、不收敛和暂不支持的结果会明确说明。
- Ctrl+Z 撤销，Ctrl+Y 重做，Ctrl+S 保存。
- “计算 → 阻力计算／螺旋桨与推进”可提取或填写参数，查看曲线及报告、取消计算、保存工况和导出 CSV/TXT/JSON/PDF。外部工具目录可选；本机包引用已有工具。三个 CALCPROP 任务、DOS B 系列及空实现的限制见[阻力与推进](docs/migration/PERFORMANCE.md)。
- “型线图”页直接使用同一模型生成的完整矢量场景。DXF 导出不受当前镜头裁剪影响。

当前保存使用临时文件和原子替换，写入失败不把文档标记为已保存。
带预览的 FBM 在编辑保存后更新四视图缩略图；工程设置可关闭，新建船模默认开启。未修改的旧文件仍原样保存。
DXF 折痕线改为只读、顺序确定的提取；相较旧版，部分线段的方向和分组会变化，
原坐标、颜色、图层及连通关系保持不变，具体对照保留在迁移报告中。
旧 FBM 没有文字编码标记，新文字须能用当前编码无损写回，否则整个操作会被拒绝。
模型编辑方向已补齐，[完成范围与验证边界](docs/migration/MODEL-EDITING-COMPLETION.md)
逐项记录。[静水力与稳性](docs/migration/NATIVE-HYDROSTATICS.md)、
[阻力](docs/migration/NATIVE-RESISTANCE.md)、[推进](docs/migration/NATIVE-PROPULSION.md)
及[板材展开](docs/migration/NATIVE-PLATES.md) 已接入 C++；部分专业方法仍调用原外部工具。
[IGES 曲面准备](docs/migration/NATIVE-CAD.md) 也已迁入 C++，保留现有导出窗口和数值协议。
非对称面积/稳心数据、其余专业计算、旧格式交接、分析对象装载及全窗口
视觉验收仍在迁移清单中；软件目前仍是开发版。

## 构建与测试

当前使用本机已有的 Qt 6.8.3 / MinGW 13.1 和 Lazarus 工具链，Qt 工具位于同级
`FreeShip-QtPilot/.tools`。构建目录通过各自的 ASCII 路径别名访问，支持中文项目路径，
不会重绑已验收试点的别名。复制到别的机器时需要先安装对应工具链。

在本工程目录运行：

```powershell
python bridge/build_bridge.py --output build/bridge-new
./package.ps1 -BridgeBuild build/bridge-new -DeployDir dist
```

`package.ps1` 只接受新目标目录，核对桥接程序、适配代码及旧源码指纹后组包。
它包含计算组件、资源、许可证和构建清单。`build.ps1 -Deploy` 仅用于单独构建 Qt
和测试程序，不能代替完整组包。验证通过后再提升为需要使用的版本。

全部可复用测试接入同级 `FreeShip-Tests`：

```powershell
python ../FreeShip-Tests/run_tests.py --suite qt-migration --output ../FreeShip-Tests/reports/qt-migration-new
python ../FreeShip-Tests/run_tests.py --suite qt-editing --output ../FreeShip-Tests/reports/qt-editing-new
python ../FreeShip-Tests/run_tests.py --suite qt-native-geometry --output ../FreeShip-Tests/reports/qt-native-geometry-new
python ../FreeShip-Tests/run_tests.py --suite qt-native-subdivision --output ../FreeShip-Tests/reports/qt-native-subdivision-new
python ../FreeShip-Tests/run_tests.py --suite qt-native-sections --output ../FreeShip-Tests/reports/qt-native-sections-new
python ../FreeShip-Tests/run_tests.py --suite qt-native-app --output ../FreeShip-Tests/reports/qt-native-app-new
python ../FreeShip-Tests/run_tests.py --suite qt-topology --output ../FreeShip-Tests/reports/qt-topology-new
python ../FreeShip-Tests/run_tests.py --suite qt-sections-editing --output ../FreeShip-Tests/reports/qt-sections-new
python ../FreeShip-Tests/run_tests.py --suite qt-overlays --output ../FreeShip-Tests/reports/qt-overlays-new
python ../FreeShip-Tests/run_tests.py --suite qt-backgrounds --output ../FreeShip-Tests/reports/qt-backgrounds-new
```

该测试在隔离副本中执行三类模型的服务命令、真实 Qt 应用动作、文件往返和导出对照，
并检查旧源码、旧程序、试点及输入模型没有变化。测试报告分别列出通过、失败、待看、
未执行和错误。隐藏窗口事件与离屏图像不等于真实桌面交互验收。

`qt-native-geometry` 单独构建 C++ 平面/样条候选及原 Pascal 对照程序，运行可手算例题、
边界输入、缓存失效和真实模型曲线检查。它不启动或替换正式应用。只构建该候选时可用：

```powershell
./build-native.ps1 -BuildDir (Join-Path $PWD 'build/native-new') -RuntimeDir (Join-Path $PWD 'build/native-runtime-new')
```

运行目录必须是新目录。基础几何库只使用 C++ 标准库，测试入口用 Qt Core 读写 JSON。
实际应用通过独立的 `FreeShipNativeLinesplan.exe` 调用这些库，字形尺寸使用 Qt；
计算失败不发布半成品，也不提交对应编辑。当前边界见下面的原生型线图记录。

`qt-native-subdivision` 验证完整静态控制网、逐级细分、折痕、控制曲线和图层数据。
测试同时检查旧核心回读、独立小网格预期及真实模型的坐标/有向拓扑，不只比较面数。
只构建该目标可在上面的构建命令追加 `-Target native_subdivision_tests`。原有平面/样条
目标仍为默认值。两组测试均不会替换交付程序。

`qt-native-sections` 验证细分曲面的平面剖切与交线拼接，分别观察原始交段、
拓扑连接及距离拼接后的点链，再检查折角和样条采样。只构建该目标可使用
`-Target native_sections_tests`。它单独核对底层计算；完整型线图、DXF、缓存和编辑
后的更新由 `qt-native-app` 覆盖。缓存目前仅在当前文档会话中复用，不承诺跨应用重启。

## 迁移记录

- [当前进度与证据](docs/migration/STATUS.md)
- [C++ 文档状态与 FBM 保存](docs/migration/NATIVE-DOCUMENT.md)
- [Archimedes 与 PAM 输出](docs/migration/SHIP-FORMATS.md)
- [界面保真迁移](docs/migration/UI-FIDELITY.md)
- [辅助线、标注与动画](docs/migration/OVERLAYS.md)
- [控制网基本编辑](docs/migration/TOPOLOGY-EDITING.md)
- [C++ 完整型线图接入](docs/migration/NATIVE-LINESPLAN-APP.md)
- [C++ 四视图曲面与控制曲线](docs/migration/NATIVE-SURFACE-DISPLAY.md)
- [C++ 文档构造与版本转换](docs/migration/NATIVE-DOCUMENT-CREATION.md)
- [完整功能清单和后续阶段](docs/migration/FEATURES.md)
- [模型接口约定](docs/migration/PROTOCOL.md)
- [接口与文件安全审查](docs/migration/ARCHITECTURE-REVIEW.md)
- [本轮编辑功能复核](docs/migration/M2-REVIEW.md)
- [ShipCAD 参考代码评估](docs/migration/SHIPCAD.md)
- [原生平面与样条验证](docs/migration/NATIVE-GEOMETRY.md)
- [原生几何复核](docs/migration/NATIVE-GEOMETRY-REVIEW.md)
- [控制网输入约定](docs/migration/CONTROL-NET.md)
- [原生细分验证](docs/migration/NATIVE-SUBDIVISION.md)
- [原生细分复核](docs/migration/NATIVE-SUBDIVISION-REVIEW.md)
- [原生剖切与交线验证](docs/migration/NATIVE-SECTIONS.md)
- [原生剖切与交线复核](docs/migration/NATIVE-SECTIONS-REVIEW.md)
- [来源与许可](NOTICE.md)

本项目继续保留 FreeShip / FreeShip Plus 的作者和许可声明。当前本地开发包尚不是公开发行包。
