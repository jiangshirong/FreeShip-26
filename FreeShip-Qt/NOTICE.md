# 来源与许可记录

本迁移工程延续已验收的 FreeShip-QtPilot，基于本地 FreeShip-Plus-Lazarus 工作树，不是与 FreeShip 无关的独立重写。几何服务编译了原 Pascal 源码，原文件中的作者、版权和许可声明保存在运行包 `bridge/legacy-source.zip` 中；项目原有 `COPYING` 同时保留在此目录。

迁移起点的旧源码指纹为 `277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b`。这个指纹包含本地已经完成的修复，不能把整个工作树写成未经修改的上游发布。独立型线图场景的来源另见 `bridge/scene-origin.json`；适配代码和每次实际构建使用的源码指纹随构建报告保存。

Qt 6.8.3、MinGW 13.1 及其运行库是第三方组件。当前 dist 是本地验证包，不能把“本机可运行”当作面向公众的完整发布准备。Qt 的模块和第三方依赖清单可在工具链的 `sbom` 目录核对；正式分发时应随发行包整理对应的许可文本和源代码获取说明。

这次迁移不改变原 FreeShip 代码的许可，也不删除任何原作者声明。

`src/document/preferences.cpp` 和 `dxf_palette.h` 保留当前 Plus 的偏好默认值、
主题读取顺序和 DXF 调色板。原生 INI/PO 读取与事务边界由本项目实现；系统颜色
通过 Win32 解析。来源为 `FreePreferences.inc`、`FreeShip.inc`、`FreeGeometry.pas`
及桥接上下文，继续保留原作者及 GPL-2.0-or-later 声明，不依赖 ShipCAD。

## ShipCAD 的平面和样条计算候选

`src/native/geometry.h`、`plane.cpp`、`spline.cpp` 借鉴了
gpgreen/ShipCAD 的 `ShipCADlib/plane.{h,cpp}`、`spline.{h,cpp}`，
固定提交为 `4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd`。
保留 Greg Green（2015）及 Martijn van Engeland（2005）的署名，文件声明为
GPL 第 2 版或后续版本。原许可全文位于 `third_party/shipcad/LICENSE`，
原文件指纹和派生文件映射位于 `third_party/shipcad/provenance.json`。

这批代码剥离了界面和文件读写，并按当前 Plus 修正参数化、折角和交点输出等差异。
各函数的来源及调整见 `src/native/PROVENANCE.md`。现已用于正式工程的 C++ 型线图
生成与 DXF 导出；FBM 结构读写、原子保存与建模编辑已接入 C++，旧版本
装载/转换及其余字段写入仍有 Pascal 依赖。

## 控制网与细分计算候选

`src/native/subdivision.h`、`subdivision.cpp` 按当前 Plus 的实际细分规则整理为
独立 C++ 计算模块，保留原 FREE!ship、Plus 的作者及 GPL 第 2 版或后续版本声明。
数据容器、对象编号、输入检查、缓存和结果所有权在新模块中单独实现。

ShipCAD 的相关文件用于交叉阅读，没有引入其细分类、界面、控制器或内存池。
它与当前 Plus 不同的三角平均系数没有采用。具体来源文件、指纹和选择理由见
`src/native/SUBDIVISION-PROVENANCE.md`。现已接入型线图生成和导出；四视图网格和
编辑状态尚未交由该模块管理。

## 曲面剖切与交线拼接候选

`src/native/sections.h`、`sections.cpp` 根据当前 Plus 的曲面平面相交和样条拼接
流程编写，保留原 FREE!ship、Plus 的署名与 GPL 第 2 版或后续版本声明。
接口使用独立控制网及细分结果，不依赖原窗口或对象地址。来源、兼容行为与边界见
`src/native/SECTIONS-PROVENANCE.md`；现已接入完整型线图和导出。

## 完整型线图生成

`src/native/scene_builder.cpp` 按当前本地 `FreeLinesplanScene.pas`、
`FreeGometry_Functions.inc` 和 `StableEdgeLoops.pas` 的实际行为移植布局、填色、
标注和 DXF 输出，保留原 FREE!ship、Plus 的署名与 GPL 声明。不是从 ShipCAD
复制完整窗口或导出流程。`document.*`、`worker.cpp` 和桥接原始输入协议为本工程
新增的边界实现。旧编译器的 Single 十进制输出另做兼容，避免导出时改变末位数值。
实现边界和与旧显示的已知差异见 `docs/migration/NATIVE-LINESPLAN-APP.md`。

## 四视图辅助线、标注与动画

`src/render/view_overlay.*`、`label_layout.*`、`overlay_animation.*` 和
`bridge/OverlaySnapshot.inc` 依据当前本地 Plus 的 `FreeShipUnit.pas`、
`FreeHydrostaticCalc.inc`、`FreeLabelLayout.pas`、`FreeOverlayReveal.pas` 及
`FreeViewport.inc` 中的显示规则移植和适配，继续作为本项目 GPL 派生代码保留。
其中动画和避让规则包含本地 FreeShip_26 的既有修改，不能全部归为原上游实现。
Qt 定时器、空间索引、不可变快照协议和独立静水力缓存是新适配；没有使用 ShipCAD
的显示窗口替换现有设计。源码依据和行为边界见 `docs/migration/OVERLAYS.md`。

## 背景图片与标定

`bridge/BackgroundEdits.inc`、`src/core/background_image.*` 和
`src/render/background_geometry.*` 按当前 Plus 的 `FreeBackgroundImageData.inc`、
`FreeBackgroundImage.inc`、`FreeEdit.inc` 及 `freehullformwindow_panel.pas` 的数据和
标定规则适配。整数像素原点、Single 比例及 W/E/N/S 边框调整沿用原语义，继续
作为本工程的 GPL 派生代码；没有使用 ShipCAD 的图片编辑窗口。Qt 不可变图片缓存、
局部拖动预览、命令事务和图片输入边界是本次适配。来源和差异记录在
`docs/migration/BACKGROUND-IMAGES.md`。

## 半船显示与剖面曲率梳

`src/render/body_projection.h`、`curvature_comb.*` 和 `hull_view.cpp` 按当前 Plus 的
控制点、边、面分侧规则及 `FreeSpline.Curvature/Draw`、`Normalize` 适配，继续采用
GPL-2.0-or-later。来源包括原 FREE!ship、Plus 维护者及已有本地修订，不能全部归为
本次原创。Qt 的快照缓存、交互坐标转换和显示协议为本次适配；未使用 ShipCAD 的窗口。
具体来源和精度对照见 `docs/migration/STARTUP-AND-DISPLAY.md`。

## 控制曲线与流线

`bridge/AuxiliaryCurves.inc` 和 `src/render/auxiliary_geometry.*` 根据当前 Plus 的
`FreeSubdivisionControlCurve.inc`、`FreeSubdivisionSurface.inc`、`FreeFlowline.inc`
以及 `FreeEdit.inc` 的数据关系和样条行为适配，继续采用 GPL-2.0-or-later。
原 FREE!ship、Plus 和 Lazarus 移植作者的既有署名与许可继续适用。
稳定编号、事务命令、Qt 曲线投影和按需命中区域是迁移适配；流线追踪仍调用原核心。
没有使用 ShipCAD 的曲线窗口或流线算法。详见 `docs/migration/AUXILIARY-CURVES.md`。

## 模型编辑补全

`bridge/ModelingEdits.inc`、`ModelTools.inc` 和 `NewHull.inc` 依据当前 Plus 的
`FreeEdit.inc`、`FreeSubdivisionControlPoint/Edge/Curve/Surface/Layer.inc` 适配。
`LackenbyEngine.pas` 从 `Forms/FreeLackenbyDlg.pas` 提取数值计算，保留原模板、
积分及变换的来源与 GPL-2.0-or-later 许可。原 FREE!ship、Plus、Lazarus 移植
维护者以及已有本地修改的署名继续适用。

事务保护、依赖循环检查、凹面内对角线检查、临时对象清理、Qt 参数窗口和只读
诊断属于本次适配。没有采用 ShipCAD 的整套编辑窗口或 Lackenby 实现。
差异及验证边界见 `docs/migration/MODEL-EDITING-COMPLETION.md`。

## 静水力与稳性

`HydrostaticsEngine.pas`、`HydrostaticsEdits.inc`、Qt 参数窗口和报告依据当前 Plus 的
`FreeHydrostaticCalc.inc`、`FreeProjectSettings.inc`、`FreeHydrostaticsFrm` 及
`FreeCrosscurvesDlg` 的数据约定适配，沿用 GPL-2.0-or-later。积分调用原 FREE!ship/
Plus 核心；原作者、移植维护者及已有本地修订的署名继续适用。
有界排水量/纵倾求解、只读任务、事务和 Qt 展示是本次适配，没有使用 ShipCAD
的稳性计算。原 `Balance` 不作为新平衡结果的正确性标准。详见 `HYDROSTATICS.md`。

## 阻力与推进

`bridge/performance_adapter.py`、`PerformanceEngine.pas`、`PerformanceHooks.pas`
及 Qt 参数/报告窗口依据当前 FREE!ship/Plus 的 `FreeEdit.inc`、
`FreeResistance_*Dlg`、`FreePropeller_Task*Dlg` 和既有计算文件协议适配，沿用
GPL-2.0-or-later，保留原 FREE!ship、Plus、Lazarus 移植及计算方法作者署名。
本次没有使用 ShipCAD 的阻力或推进实现。

外部求解器不是这份 GPL 授权的一部分。本机配置只引用已有程序目录，运行包不
附送这些二进制。FPPCALC 的运行说明明确限定教育与非商业用途；公开发行前须
分别处理工具许可或替代实现。详细使用与验证范围见 `docs/migration/PERFORMANCE.md`。

## 板材展开

`PlateEngine.pas`、`PlateSnapshot.inc`、`plate_adapter.py` 依据当前 Plus 的
`FreeDevelopedPatch.inc`、`FreeSubdivisionLayer.inc` 和 `FreeEdit.inc::Layer_Develop`
适配，沿用 GPL-2.0-or-later 及原作者署名。2026-11-03 的 `src/document/plates.cpp`
依据相同来源迁入 C++，原生细分、样条和剖线模块的来源另有记录。旧 Pascal
展开入口保留用于独立对照；Qt 板片布局、绘图和边界输出为本次实现。没有采用
ShipCAD 的展开算法。详见 `docs/migration/NATIVE-PLATES.md`。

## 模型交接

`model_exchange.*` 和 `ExchangeImport.inc` 按当前 Plus 的 OBJ/STL/FEF 字段及控制网
含义实现，沿用 GPL-2.0-or-later。FEF 字段依据 `FreeEdit.inc::File_ExportFEF`、
`FreeSubdivisionSurface.inc::ExportFeFFile` 以及点、边、面的流记录；原项目作者署名保留。
Qt 解析、完整快照输出、校验和事务接入为本次实现，未复制 ShipCAD 的文件读写代码。
# CAD interchange (2026-11-02)

The IGES patch construction is extracted from the current FreeShip Plus
`Units/FreeEdit.inc::File_ExportIGES` in an isolated build, with provenance pinned
in `bridge/cad_adapter.py`. Its original authorship and GPL-2.0-or-later license
are retained. The new serializer and UI are in this project. The CAD work does
not import additional ShipCAD code. OpenCascade and ezdxf are independent test
consumers installed separately; their packages are not bundled with this app.


## GHS、Archimedes 与 PAM

`hull_sections.*`、`ghs_export.*`、`ship_formats.*` 的格式结构对照当前 Plus
`FreeEdit.inc::File_ExportGHS/File_ExportArchimedes/File_ExportPAM`，沿用
GPL-2.0-or-later 和原项目作者署名。数值剖切使用当前原生核心，Qt 事务接入为本次
实现；没有采用 ShipCAD 对应代码。Archimedes 含义参考 John A. MacSween 的原始
手册，文档保留出处；不打包 Archimedes 或 PAM 接收软件。

## Native document core

The FBM structural codec follows FreeShip Plus `FreeShip.inc`, `FreeFileBuffer.pas`,
`FreeVersionUnit.pas` and the versioned LoadBinary/SaveBinary layouts. Original
FREE!ship/Plus authorship and GPL-2.0-or-later are retained. Document transactions,
history ownership and the Qt service boundary are implemented here; the remaining
Pascal operation algorithms are explicitly retained, not claimed as newly native.

Native point movement follows the current Plus `FreeSubdivisionControlPoint`
SetCoordinate/MoveBy and `FreeGometry_Functions.ProjectPointOnLine` semantics.
Their authorship and GPL-2.0-or-later are retained. Iterative dependency traversal,
validated coordinate patching and derived-cache invalidation are implemented in
the C++ document layer; this work does not copy ShipCAD editing code.

Native topology and advanced editing follow the current Plus control-point,
edge, face and curve implementations, `FreeEdit.Model_Scale`, `RotateVector`
and the versioned calculation records in `FreeTypes.pas`. Original FREE!ship
and Plus authorship and GPL-2.0-or-later are retained. The C++ candidate model,
stable-ID bindings and transaction validation are new adaptation code. The
scope and unchanged legacy behaviors are recorded in
`docs/migration/NATIVE-TOPOLOGY.md` and `NATIVE-ADVANCED-EDITING.md`.

Rotation uses an altered, bounded C++ translation of the Free Pascal 3.2.2
runtime's `rtl/inc/genmath.inc` trigonometric routines in `third_party/fpc/trig.h`.
This retains the existing Windows calculation's argument reduction and rounding,
including quadrant residues that differ from MinGW libm. Original contributor,
Fred Fish, Odent Jean Philippe, Carl Eric Codere, Wolfgang Ehrhardt and Sun
notices remain in that file; the RTL license and linking exception are retained
in `third_party/fpc/COPYING` and `COPYING.FPC`. This is a modified translation,
not an original trigonometric algorithm or an unmodified FPC distribution.

Native panels, cylinders and surface intersections follow current Plus
`ModelTools.inc`, `FreeMatrices.pas`, `RotatePointAroundVector`, `InsertPlane`
and `CalculateIntersectionPoints`. Native Lackenby follows the isolated
`LackenbyEngine.pas`, the original intersection-area/spline routines and
submerged-hull bounds. Original FREE!ship/Plus authorship and GPL-2.0-or-later
are retained. No additional ShipCAD editing code is used. Deterministic cut
ordering and the old heap-order evidence are recorded in
`docs/migration/NATIVE-CONSTRUCTION.md`. The bounded FPC trigonometric
translation also supplies the cylinder's radian Sin/Cos operations.

Native point properties, groups, markers and face tools follow the current
Plus serialization and the isolated `ModelingEdits.inc`/`ModelTools.inc`
contracts. Rectangular patch assembly adapts `AssembleFacesToPatches(amNurbs)`
and its regularity rules, with stable face IDs replacing heap-address order.
Original FREE!ship and Plus authorship and GPL-2.0-or-later are retained;
no additional ShipCAD implementation was copied for this group of tools.

`third_party/lazarus/fbm_codepages.h` is generated from the installed LazUtils
`commoncodepages.inc`, retaining all 31 single-byte conversion tables. Its
modified LGPL license is provided alongside it. The header records the exact
input SHA256; the retained generator/audit and provenance are described in
`third_party/lazarus/README.md`. The generated mapping is not represented as
newly authored data. Runtime text conversion rejects unrepresentable names.

Native project, unit, layer and section editing follows current Plus
`FreeProjectSettings.inc`, `FreeEdit.Model_Scale`, `FreeSubdivisionLayer.inc`,
`FreeIntersection.inc` and the isolated `ModelEdits.pas`/`SectionEdits.inc`
contracts. Original FREE!ship/Plus authorship and GPL-2.0-or-later are retained.
Independent format and arithmetic probes are in FreeShip-Tests. These modules
do not incorporate additional ShipCAD code.

Native background, visibility and auxiliary-definition editing follows current
Plus `FreeBackgroundImageData.inc`, `FreeVisibility.inc`,
`FreeSubdivisionControlCurve.inc`, `FreeFlowline.inc` and the isolated
`BackgroundEdits.inc`/`DisplayEdits.inc`/`AuxiliaryCurves.inc` contracts. Original
FREE!ship/Plus authorship and GPL-2.0-or-later are retained. This group does not
copy additional ShipCAD code. Flowline tracing and auxiliary display splines
remain in the read-only Pascal geometry adapter.

The native propulsion workflow and generated postprocessing adapt
`FreePropeller_Task1Dlg.pas` through `FreePropeller_Task5Dlg.pas` from Plus,
including Timoshenko V.F.'s 2007-2013 work, under GPL-2.0-or-later. Captions and
field metadata retain their original provenance in `propulsion-origin.json`.
The retained generators live in FreeShip-Tests/maintenance. No ShipCAD code is
used for this workflow. External PropPred, CALCPROP, PROPOL and FPPCALC programs
are separate dependencies; this local package only references their existing
directory and does not redistribute them or change their licensing terms.

The native resistance calculations and generated reports adapt the ten Plus
FreeResistance_*Dlg units (Martijn van Engeland; Timoshenko V.F.; Mark Malakanov),
plus SFINEX1 and the original translations, under GPL-2.0-or-later. See
src/document/resistance-origin.json and FreeShip-Tests/maintenance/port_resistance.py.
No additional ShipCAD code is used. Original external resistance executables and
their databases are separate dependencies and are not redistributed by this package.

The native Hydrodyn workflows adapt Plus FreeHydrodyn_Task1Dlg,
FreeHydrodyn_ManeuvDlg and FreeHydrodyn_RVRSDlg, including Timoshenko V.F.'s
calculations, coefficient tables and help, under GPL-2.0-or-later. Existing
FREE!ship/Plus authorship remains applicable. Original strings and file hashes
are recorded in src/document/hydrodynamics-origin.json. The metadata generator
and independent protocol tests are retained in FreeShip-Tests. Numerical and
process corrections, and unresolved source conflicts, are documented in
docs/migration/HYDRODYNAMICS-TOOLS.md. No ShipCAD code is used for these tools.
Ishercof, ManeuvPP, RVRSSHIP and DBFview remain separate external programs;
this package references the configured tool directory and does not redistribute
them or claim a new license over their binaries or databases.

The Polycad control-grid importer adapts the file interpretation and grid ordering
in Plus Units/FreeEdit.inc (File_ImportPolycad) and
Units/FreeSubdivisionSurface.inc (ImportControlPointGrid), GPL-2.0-or-later.
Existing FREE!ship/Plus authorship remains applicable. The C++ parser adds
bounded validation and transactional integration; no ShipCAD code is used.
This is control-net conversion, not exact B-spline/NURBS surface reconstruction.

The Hull, Carene, chine-text and surface-text importers adapt Plus
Units/FreeEdit.inc (File_ImportHull/Carene/Chines/Surface) and
Units/FreeShip.inc (ImportChines), retaining original FREE!ship/Plus authorship
under GPL-2.0-or-later. Curve sampling reuses the existing native Spline port,
whose ShipCAD/Plus provenance remains in src/native/geometry.h, spline.cpp and
PROVENANCE.md. The bounded text readers, direct tridiagonal fit and transactional
annotation import are implemented in this Qt project. Corrected station units
and explicit fitting/format limits are documented in docs/migration/CURVE-IMPORT.md.

The part-file and Michlet wave readers and Michlet/AddMass input writers adapt
current Plus FreeShip.SavePart, FreeEdit.File_ImportPart / File_Import_MichletWaves,
FreeMichletOutputDlg and FreeAddMassOutputDlg, under GPL-2.0-or-later with the
original FREE!ship/Plus authorship retained. VRML static scene parsing and
bounded cubic waterline root isolation are new implementations. VRML transform
semantics follow the Web3D VRML97 specification; no Web3D implementation code is
copied. Scope and intentional corrections: docs/migration/LEGACY-EXCHANGE.md.

The Michlet three-mode hull arrangement follows the existing Plus UI. Relative
hull positions, bow-row interpretation and constant trim/sinkage records are
checked against Leo Lazauskas's Michlet 9.33 manual (sections 3.1, 7.13, 7.16, 8.1).
No Michlet binary or manual is redistributed. Native mesh symmetry validation
and explicit wave-origin mapping are new code; see docs/migration/MICHLET-MULTIHULL.md.

The AddMass workflow and empirical postprocessing adapt Plus
FreeEdit.ExportAddedMassInWorkspace / FreeAddMassOutputDlg (Timoshenko V.F.)
under GPL-2.0-or-later. The bounded result parser, process isolation and Qt
matrix tables are implemented here. Undefined upstream river corrections are
not copied. ADD_MASS.EXE remains an external, separately licensed solver and
is not redistributed in this package. See docs/migration/ADDED-MASS.md.

The read-only offset-grid preview restores the role of the Plus Michlet/AddMass
viewport. The Qt renderer and asynchronous controller are new implementations;
they consume the exact offsets from the existing GPL-2.0-or-later input writer.
See docs/migration/GRID-PREVIEW.md for coordinate and validation boundaries.

The atomic crash-recovery container, process-lease handling and Qt recovery
dialog are new GPL-2.0-or-later implementations. Their payload is the existing
native FBM save representation; original format authorship is retained.
See docs/migration/RECOVERY.md for persistence and validation boundaries.

The Qt model-file save transaction, content-version checks, cooperating writer
locks and backup rollback are new GPL-2.0-or-later implementations. The previous
version .Bak behavior follows current Plus FreeSafeFile requirements; native FBM
encoding and its original authorship are unchanged. Qt file APIs are used under
their existing package license. See docs/migration/SAVE-PROTECTION.md.

The content-addressed section cache, persistent application cache routing and
dependency fingerprints are new GPL-2.0-or-later implementations. They retain
the existing Plus-derived section selection, joining and spline algorithms;
original geometry attribution remains unchanged. See docs/migration/LINESPLAN-CACHE.md.

The Qt FBM thumbnail renderer and staged save integration are new GPL-2.0-or-later
implementations. The 400x300 four-view JPEG convention, preview flag and binary
envelope follow Plus FreeShip.FGetPreview, FreeProjectSettings.SaveBinary and
FreeFileBuffer.Add(TJPEGImage). Existing surface algorithms and attribution remain
unchanged. See docs/migration/FBM-PREVIEW.md.

The asymmetric hull's lateral silhouette uses Angus Johnson's Clipper2 engine,
release Clipper2_2.0.1, commit 21ebba05db8894f0c7217ad35ea518080f324946,
under the Boost Software License 1.0. The unmodified engine, headers, LICENSE
and file hash manifest are retained in third_party/clipper2. Source repository:
https://github.com/AngusJohnson/Clipper2 . The clipped surface projection,
batched union, caching and moment integration are new GPL-2.0-or-later code;
see docs/migration/LATERAL-PROJECTION.md. No network fetch is needed to build.
