# 构建、测试与迁移记录

> 本文保存旧版 README 中面向开发者的内容（2026-11-17 迁出，正文未改）。功能概览见 [项目首页](../README.md)，操作细节见 [操作说明](USAGE.md)。

> **注意：** 下列命令依赖开发机上的目录布局：同级的 `FreeShip-QtPilot/.tools`（Qt 6.8.3 / MinGW 13.1）、Lazarus 工具链和 `FreeShip-Tests` 测试仓库。这些目录目前没有随本仓库公开。在其他机器上需要先准备相同的工具链。

## 迁移概况

这是 FreeShip 26 的正式迁移工程，沿用已验收的 Qt 型线图试点。
旧版和试点留在旁边，便于逐项对照。这里目前是开发版，还没有覆盖旧版的全部功能。

已确认的上游缺陷及本项目修复见[修复记录](upstream-fixes/README.md)，包括保存失败保护、单位换算、图层对称导出和部分静水力算法。每项保留原代码、影响条件及验证范围；[README 说明稿](upstream-fixes/README-excerpt.md)摘出了影响较大的问题。

在[静水力附加报表](migration/HYDRO-AUXILIARY.md)的各页面可按需计算 Bonjean、横剖面面积和重量，查看并保存结果。首尾水线位置随横剖面面积生成。
功能范围和未测项见[迁移记录](migration/STATUS.md)。

[日常运行包](migration/NATIVE-RUNTIME.md)已与开发测试包分开：`package-runtime.ps1`
从已验证的开发包提取四个 C++ 程序及资源，去掉 Pascal 对照程序和测试探针。
打开 `dist/FreeShipQt.exe` 使用；无需安装 Lazarus。公开发行准备另行处理。

外部计算程序的配置和故障处理见 [配置外部计算程序](CALCULATION-TOOLS.md)；
专业计算的数值差异、验证范围见 [可靠性记录](migration/CALCULATION-RELIABILITY.md)。

[Qt 的 Windows 安装器](migration/WINDOWS-INSTALLER.md)由 `build-installer.ps1` 生成，
入口为 `FreeShip 26 Setup.exe`，使用独立的当前用户目录和开始菜单入口。
外层事务在升级失败时自动恢复已核验的原版本及安装登记；内部 MSI 不作为独立升级入口。
本机回归及发行边界见安装记录；现有 `dist/FreeShipQt.exe` 继续可用。

第一阶段打通一条实际流程：打开 FBM 船模，在四视图中选择和移动控制点，
撤销、重做、保存，再重新打开；型线图随同一个模型版本更新，可保存图片和导出 DXF。
各视图有独立镜头，平移和缩放不会触发船体重建。

完整型线图现在由 C++ 生成：控制网细分、求交、投影、填色、标签及 DXF 均已接入
实际程序。输入直接来自模型，不从四视图的画面反推。文档状态、撤销、FBM 结构和
坐标/控制网保存、基本与高级建模已迁入 C++；最新一组是
[面板、圆柱、求交与船型形变](migration/NATIVE-CONSTRUCTION.md)。
点属性、点组、标记线、面片分组/组装和朝向修复也已接入 C++，见
[其余建模编辑](migration/NATIVE-MODELING-COMPLETION.md)。三类建模命令
均由原生文档服务执行。[工程设置、图层和四类剖面编辑](migration/NATIVE-PROJECT-DEFINITIONS.md)
也已迁入 C++，包括单位转换、保存和撤销。随后完成
[背景图片、显示和曲线定义](migration/NATIVE-AUXILIARY-EDITING.md) 的原生编辑。
[四视图网格、法线和控制曲线样条](migration/NATIVE-SURFACE-DISPLAY.md)，以及
[打开、新建、导入和 FBM 版本转换](migration/NATIVE-DOCUMENT-CREATION.md) 已接入 C++。
流线、静水力、主要阻力/推进流程、板材展开和 IGES 曲面准备已迁入 C++。
[模型快照与标注](migration/NATIVE-PUBLICATION.md) 也已直接接入 C++，
[偏好与绘图文字初始化](migration/NATIVE-PREFERENCES.md) 也已迁入 C++，
常用文档路径不再启动旧版程序。独立对照用的旧诊断和未迁移工具兼容入口仍保留。

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

- [当前进度与证据](migration/STATUS.md)
- [C++ 文档状态与 FBM 保存](migration/NATIVE-DOCUMENT.md)
- [Archimedes 与 PAM 输出](migration/SHIP-FORMATS.md)
- [界面保真迁移](migration/UI-FIDELITY.md)
- [辅助线、标注与动画](migration/OVERLAYS.md)
- [控制网基本编辑](migration/TOPOLOGY-EDITING.md)
- [C++ 完整型线图接入](migration/NATIVE-LINESPLAN-APP.md)
- [C++ 四视图曲面与控制曲线](migration/NATIVE-SURFACE-DISPLAY.md)
- [C++ 文档构造与版本转换](migration/NATIVE-DOCUMENT-CREATION.md)
- [完整功能清单和后续阶段](migration/FEATURES.md)
- [模型接口约定](migration/PROTOCOL.md)
- [接口与文件安全审查](migration/ARCHITECTURE-REVIEW.md)
- [本轮编辑功能复核](migration/M2-REVIEW.md)
- [ShipCAD 参考代码评估](migration/SHIPCAD.md)
- [原生平面与样条验证](migration/NATIVE-GEOMETRY.md)
- [原生几何复核](migration/NATIVE-GEOMETRY-REVIEW.md)
- [控制网输入约定](migration/CONTROL-NET.md)
- [原生细分验证](migration/NATIVE-SUBDIVISION.md)
- [原生细分复核](migration/NATIVE-SUBDIVISION-REVIEW.md)
- [原生剖切与交线验证](migration/NATIVE-SECTIONS.md)
- [原生剖切与交线复核](migration/NATIVE-SECTIONS-REVIEW.md)
- [来源与许可](../NOTICE.md)

本项目继续保留 FreeShip / FreeShip Plus 的作者和许可声明。当前本地开发包尚不是公开发行包。
