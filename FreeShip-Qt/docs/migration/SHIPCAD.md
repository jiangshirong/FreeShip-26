# ShipCAD 借鉴记录

记录日期：2026-11-01。审查对象为 [gpgreen/ShipCAD][repo]，固定提交
`4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd`，提交作者时间为 2018-11-20（-08:00），说明为 `Working on iges surfaces`。
本地只读副本：[References/ShipCAD](D:/桌面文件夹/项目/FreeShip_26/References/ShipCAD)。

这份记录基于源码静态核对，没有运行 ShipCAD 的构建脚本、安装依赖或执行它的测试，也没有将其代码并入正式程序。下面的“已有实现”指看到了计算过程和调用关系，不代表已验证其结果正确或已在当前 Qt 6 环境编译通过。

后续进展（2026-11-01）：平面和样条的计算部分已提取到 `src/native`，按当前 Plus 的
实际数值行为调整，并建立独立构建与对照测试。它仍未接入正式应用。具体范围、结果与
来源记录见 [NATIVE-GEOMETRY.md](NATIVE-GEOMETRY.md)；下文保留初次审阅时的判断。

## 结论与采用顺序

用户已明确：ShipCAD 只是可选参考，是否采用由本项目按模块判断，不要求持续借用。
年代和语言都不能代替正确性证据。下面的模块顺序是验证路线，不是必须移植其代码的
清单；可以复用、改写或重新实现。当前 Plus 用于核对兼容行为，独立数学例题另查正确性。

ShipCAD 值得利用，最有价值的是已经写成 C++ 的细分曲面、样条、平面求交和部分静水力计算。它能减少下一阶段逐句翻译 Pascal 的工作。但它迁移的基线较早，核心算法、文件版本和编辑流程都不能直接替代我们现在的版本。

验证顺序为：**平面与样条的独立对照 → 控网细分 → 平面切割与交线 → 静水力**。保留已验收的 Qt 绘图、模型事务、稳定 ID 和导出结果。文件读写另立兼容任务，暂不使用 ShipCAD 的读写器写用户文件。

本轮的[控制网与细分候选](NATIVE-SUBDIVISION.md)已按当前 Plus 的计算规则单独
实现索引容器、所有权和缓存，没有搬入 ShipCAD 的细分类或内存池。交叉阅读发现
ShipCAD 的全三角邻域权重用了平方根，而当前 Plus 用平方；该段没有采用，
另以独立数学例题检查。具体选择和来源见
[细分来源记录](../../src/native/SUBDIVISION-PROVENANCE.md)。

当前 M2 的工程设置、图层属性、批量移动，继续按本工程的独立表单数据和一次提交方式完成；ShipCAD 的对话框用于核对字段和旧功能意图。下面已经发现的错误，也应成为我们后续测试的输入。

## 它完成到了哪里

ShipCAD 的 [README][readme] 仍列有不少未完成功能，但清单与代码并不完全同步。例如撤销/重做已有实现，STL 导出、折角线导入也有实际代码。反过来，存在类和方法不等于功能完成：IGES 的 `saveToFile` 没有真正写出文件，`processParameterData` 和 `clear` 仍为空；`ShipCADModel::exportIGES` 只构造对象，未向传入的流输出。见 [IGES 实现][iges]、[模型导出入口][iges-entry]。

| 范围 | 看到的实际实现 | 本工程如何用 |
|---|---|---|
| `ShipCADlib/plane.{h,cpp}` | 平面构造、点投影、镜像、盒相交、直线求交。依赖主要是 `QVector3D` 和常量 | 最小试点，先修退化与共面判断，再做数学用例和 Pascal 对照。[源码][plane] |
| `spline.{h,cpp}` | 参数化、重建、求值、一/二阶导、曲率、折角、简化、平面求交、DXF 样条输出 | 优先提取计算部分；必须先处理与当前 Plus 的参数化差异。不要把 `draw`、`Viewport`、文件写出一起搬入。[重建][spline-build]、[求值][spline-value] |
| `subdivsurface`、`subdivpoint`、`subdivedge`、`subdivface`、`subdivcontrolcurve` | 控点/边/面邻接，三角与四边混合细分、角点和硬边、网格平均、控制曲线、镜像、拓扑操作；不是空壳 | 这是最能节省迁移量的部分。先接纯计算验证目标，逐级比较细分网格，再迁移编辑命令。绘图与内存池耦合需要拆开。[细分主循环][subdivide]、[顶点平均规则][averaging] |
| `subdivsurface::intersectPlane`、`intersection.cpp` | 按图层筛选，包围盒剔除，面与平面切割，交线拼接、简化，截面积/重心计算 | 第二批复用，比较 stations/buttocks/waterlines/diagonals，保留原交线精度和简化开关。[切割入口][cut]、[交线重建][intersection] |
| `hydrostaticcalc.cpp` | 浸水体积、浮心、面积及多个静水力结果有实现；`balance` 内仍有 TODO | 几何一致后再接，需独立解析体和实际船型验算。不能把现有两个盒体测试当完整精度验收。[计算入口][hydro] |
| `filebuffer.cpp`、`shipcadmodel.cpp`、各类 `loadBinary/saveBinary` | FREE!ship 旧文件字段顺序和版本分支有参考价值，但只声明到 `fv260`，异常保护和编码处理不足 | 用作早期格式资料；保留当前 Pascal 读写与原子文件提交，后面重做有边界检查的 C++ reader/writer。[版本][version]、[加载入口][load] |
| `controller.cpp`、`undoobject.cpp` | 已有很多点边面命令、快照式撤销/重做，信号连接对话框；仍有大量空命令 | 可查旧操作顺序，不能直接继承事务/历史实现。当前 `ModelSession` 的失败回滚和文档版本校验继续保留。[控制器][controller]、[撤销实现][undo] |
| `ShipCAD/projectsettingsdialog.*`、`layerdialog.*`、`ShipCADlib/dialogdata.*` | Qt 表单、属性数据副本、单位标签与模型提交的分层思路已存在 | 参考字段覆盖；误接控件、临时数据仍改真实模型等问题见下节。当前 M2 表单继续独立实现。[工程设置][project-dialog]、[图层表单][layer-dialog] |
| 型线图、阻力、完整导出 | README 明列 Linesplan 未完成；阻力只有结构定义，控制器相关计算仍空。底层 `Spline::saveToDXF` 不能代表完整型线图 DXF | 继续使用本工程已验证的场景、渲染和导出。不能因它用了 C++/Qt 就换掉已工作的部分。[待办清单][readme]、[阻力数据][resistance] |

## 与现有 Plus 5.3.3 工作树的实质差别

1. **它的文件基线是 FREE!ship 2.6。** `version.h` 的最后版本和 `k_current_version` 都是 `fv260`，随仓 Pascal 源码也声明 `CurrentVersion=fv260`。本地 Plus 的 [FreeVersionUnit.pas](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Plus-Lazarus/Units/FreeVersionUnit.pas:52) 已有命名实体、线性约束、控制点分组、锚点等后续版本字段，默认文件版本为 `fv462`；产品号 5.3.3 与 FBM 格式版本不是同一编号。不能让旧 reader 丢掉这些字段后再保存。
2. **样条参数化已不同。** ShipCAD `spline.cpp:119,142` 使用 `sqrt(distanceToPoint(...))`；当前 [FreeSpline.inc:148](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Plus-Lazarus/Units/FreeSpline.inc:148)、171 使用 `DistPP3D(...)`，外层 `Sqrt` 已注释，而 [DistPP3D](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Plus-Lazarus/Units/FreeGometry_Functions.inc:425) 本身返回欧氏距离。长度分别为 1、4 的相邻段，中间参数会分别是 1/3、1/5。对于不等距且不共线的点，这会影响曲线和交点；不能用 ShipCAD 的输出替换现有基准。
3. **它没有提供所需的局部增量更新。** `SubdivisionControlPoint::setCoordinate` 调用整个 surface 的 `setBuild(false)`；后者 `clearFaces()` 并将细分级别归零。模型级失效又会清理所有交线和静水力结果。它有“不变时不重建”的标记，但不是控制点依赖范围的局部重算，也不是跨重启磁盘缓存。[控点修改][point-invalidate]、[曲面失效][invalidate]、[模型失效][model-invalidate]。
4. **数字类型并未升级。** 样条、平面、很多几何结果仍用 `float`/`QVector3D`，本地 Plus 的 [TFloatType](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Plus-Lazarus/Units/FreeTypes.pas:14) 是 `single`。换语言不会自动提高精度；迁移时先保持数值行为，再单独讨论双精度方案。
5. **当前本地基线包含已经完成的修复。** 折角边链顺序、保存事务、输入校验和失败回滚等以本工程现有代码及测试为准。不能把 ShipCAD 的早期流程重新引入。来源指纹和边界见 [NOTICE.md](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Qt/NOTICE.md)。

## 需要先挡住的具体问题

这些是本次静态阅读得到的可定位问题；没有声称在 ShipCAD 程序内逐个运行复现。

| 优先级 | 位置与情况 | 借用前的处理和验证 |
|---|---|---|
| P0：写文件正确性 | [filebuffer.cpp:334-353][resistance-write] 的两个阻力结构写出函数把 `&buf` 转成字节地址，读的是指针局部变量所在位置，长度却用整个结构的 `sizeof`。读取函数使用的是 `buf`，两边不对称 | 显式逐字段写固定宽度/字节序。用非默认字段、原 Pascal 文件和重新加载逐项对照；不要只做同一实现的自读自写 |
| P0：输入边界与旧文件保护 | [filebuffer.cpp:117-264][buffer-read] 有直接 `_data[_pos++]`；4 字节值短读后仍用未完全填充的 union；字符串长度未验证就分配。`loadBinary` 先 `clear()`，遇到新版本或错误头只留 TODO，最后仍设为未修改并重建 | 长度、数量、索引、有限值检查；候选模型完整验证后才替换当前文档。过新版本明确拒绝，旧文档和历史保持不变 |
| P1：编辑历史 | [shipcadmodel.cpp:283-349][undo] 的 `createUndo(false)` 已先删除 redo 分支，成功时没有恢复临时改动的版本和预览开关；[controller.cpp:1971-1991][project-commit] 在弹工程设置前调用它 | 确认真正变化并验证成功后再提交历史；取消、无变化、失败均保持 redo、dirty、版本、预览和稳定 ID |
| P1：导出/显示改变模型 | [subdivsurface.cpp:2727-2761][loops] 先按地址排列硬边，再由 `getPreviousEdge` 和 `sortEdges` 交换共享边端点。保存也 [原地排序控制点][save-surface] | 继续使用本工程只读且顺序确定的 [StableEdgeLoops.pas](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Qt/bridge/StableEdgeLoops.pas)。重复生成、换进程、保存前后必须坐标/重数/连通性一致，实体 ID 不因排序变化 |
| P1：平面退化与共面 | [plane.cpp:59-69][plane] 三点共线时法向量归一化后仍除以零长度；[plane.cpp:144][plane-line] 用 `distance(pt)<=1E-5` 判共面，负侧任意远的平行线也满足 | 先处理零法向量；共面判断使用有定义的距离与绝对值容差。加入正负两侧、共面、平行、不平行、共线输入和非单位法向量 |
| P1：重复求交结果 | [spline.cpp:655-695][spline-cut] 仅将 `number_of_intersections` 归零，未清空 `points/parameters`，之后继续 push_back | 连续复用同一输出对象测试“两次相交”和“相交后不相交”；输出计数和数组长度保持一致 |
| P1：坐标与单位变换 | [shipcadmodel.cpp:768-845][transform] 将 waterline 的平面 d 乘/减 y 分量，buttock 用 z，和它自身的投影定义相反；等比单位转换会掩盖缩放问题。单位转换未覆盖斜剖面、流线、背景标定等完整数据 | 批量移动与整船变换分别定义语义；验证每个轴和非等比变换。单位转换从当前 Plus 的完整字段继续做，而非覆盖为 ShipCAD 的较短版本 |
| P1：字符编码 | [filebuffer.cpp:251-275][buffer-string] 固定按 UTF-8 读写；当前 [FreePreferences.inc](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Plus-Lazarus/Units/FreePreferences.inc:173) 默认 `cp1252`，文件没有对应的新编码标记 | 保留当前编码契约和无法无损表示时的明确拒绝。新格式/新标记另立设计，测试拉丁、中文、俄文和旧文件 |

M2 表单还有几个很容易带回来的问题：

- 工程设置的 `savePreviewImageCheckBox` 实际读写 `isSimplifyIntersections/setSimplifyIntersections`（[66、109 行][project-dialog]），预览与交线简化接错了。
- 临时 `ProjectSettingsDialogData` 仍持有真实模型。切换单位时 `changeUnitsInSettingsOnly → setLength/setBeam/setDraft → owner->setFileChanged(true)`，因此“副本”也可能污染当前文档状态；它不是本工程的独立不可变快照。[数据副本][dialog-data]、[设置转换][units]。
- `QSignalBlocker(this)` 不会阻断子控件自身发出的信号；数值 `.toFloat()` 没检查转换是否成功。接受表单仍需逐字段校验，不能只靠输入框 validator。
- 图层厚度标签用 `LengthStr` 的 m/ft，但转换因子是 25.4，实际应为 mm/in。[图层单位][layer-dialog]、[单位转换][units]。
- 图层 `setName` 使用不区分大小写的比较，大小写更名会被忽略（[159-170 行][layer-name]）。
- `moveFaces` 会跳过锁点再继续移动其他点（[814-820 行][transform]）。本工程当前批量命令约定为先验证全部，再一起提交；不要未经说明改成部分成功。

## 自带测试哪些能接着用

[UnitTests.pro][tests] 注册了 13 个 QtTest 子项目。值得改造成可复用输入和断言的有：

- `spline/tst_splinetest.cpp`：点增删、方向翻转、拼接、简化以及 U 形折线与平面相交。缺少当前 Plus 与它的参数化对照，亦没有复用同一 `IntersectionData` 的检查。[交点测试][test-spline]。
- `subdivface/tst_subdivfacetest.cpp`、`subdivcontrolface/...`：三/四/五边面细分，数量、法向等断言，可作为拓扑小样本；不能只比较面数。[面测试][test-face]。
- `intersection/tst_intersectiontest.cpp`：1 m 盒体的水线/横剖面点数、面积和重心。`testDXF` 只检查列表非空，不能证明实体、图层和坐标正确。[交线测试][test-intersection]。
- `hydrostaticcalc/tst_hydrostaticcalctest.cpp`：半浸盒体的体积、排水量、浮心和系数；部分重心/惯性矩断言被注释，多项容差为 `1E-2`。可以保留解析体思路，不能照抄这个宽度作为我们的精度标准。[静水力测试][test-hydro]。
- `projsettings`、`subdivlayer`、`visibility`、控点/边/面读写：可扩充字段覆盖。自读自写测试会漏掉双方共同写错的格式，必须加入真实旧文件和 Pascal 交叉读取。

`subdivsurface` 的两项仅对空对象操作后 `QVERIFY2(true,...)`；`developedpatch` 展开后也只是 `true`。这些可以留下构造冒烟作用，不能计入曲面细分/展开结果正确性的覆盖。[曲面测试][test-surface]、[展开测试][test-patch]。

迁入的可复用测试仍统一放在 [FreeShip-Tests](D:/桌面文件夹/项目/FreeShip_26/FreeShip-Tests)，接统一入口；运行结果继续区分 `pass/fail/review/not_run/error`。本次只记录测试价值，**尚未执行或迁入这些测试**。

## 下一步的具体接入方案

1. **建立独立计算验证目标。** 在现有 CMake 中增加只供验证的 `native_geometry`，先放 plane/spline 计算，不链接 ShipCAD 主窗口或 controller。将显示、文件写出与计算拆开，输出普通结果数组。第一批对照覆盖不等距点、重复点、折角、端点、退化平面和求交容差；比较参数、求值、导数、交点和顺序。
2. **准备完整的几何输入。** 现有 `ModelSnapshot` 为显示服务，控点仅有 id/position/locked，缺少顶点类型、控制曲线及细分模式，不能直接还原完整计算模型。由 Pascal 主模型导出测试专用 `GeometryInput`：固定 ID 的点和顶点类型、有向面顶点序、硬边、曲线、图层筛选/对称属性、细分模式和精度。三角显示网格不作为逆推原模型的输入。
3. **把细分接在同一份输入后。** 先跑三角、四边、混合、硬边/角点、开口、对称与非对称层，再跑当前盒体、合成曲面和 9a。每一级比较顶点、面朝向、边连接、包围盒；再比较所有剖切方向。保留既有冻结样本，允许的浮点差异必须注明来源，不能放宽阈值掩盖形状变化。
4. **先只读替换，后处理编辑。** 候选 C++ 核心先生成结果供差分，不写回模型。达到现有场景与 DXF 标准后才切换计算入口；失败时仍保留 Pascal 路径。拓扑命令、约束/锚点和 FBM 写出分别过关，不能一次引入第二个不完整的权威模型。
5. **局部更新另做依赖记录。** ShipCAD 的父面/子面和点边面邻接可辅助建依赖表，但失效逻辑要重新设计：控点变化标记受影响细分邻域，更新局部包围盒，再重算与旧/新包围盒相关的交线。重算结果始终和完整生成对照。磁盘缓存继续按源文件内容、算法版本、精度与相关设置失效，不复用地址或 UI 状态作为键。

这条路线能复用其 C++ 算法表达和测试小样本，同时保留我们已经验证过的操作、保存和导出。具体能省多少时间、能快多少，需要独立目标编译和差分计时后再给数字。

## 构建和来源记录

ShipCAD 原工程是 qmake，README 要求 C++11、Qt 5.7、Boost ≥1.55、指定 Eigen 快照。`ShipCADlib.pro` 把计算、绘制和 controller 编到同一静态库，依赖 Qt GUI/Widgets/OpenGL；`Entity` 继承 QObject，并含纯虚 `draw(Viewport&,LineShader*)`，所以整库并非现成的无界面核心。[工程文件][build]、[实体接口][entity]。

当前 FreeShip-Qt 使用 C++17、Qt 6.8 和 CMake。优先拆叶子算法；常量所用的 Boost、折角线拟合所用的 Eigen，以及自定义池都不应因为整库链接而自动成为第一步依赖。若确实复用，单独记录版本、用途及构建结果；Qt 5 → 6 的头文件与模块适配也须实际编译确认。

源码的署名与许可并非只有仓库徽章：

| 来源 | 实际看到的声明 | 采用时记录 |
|---|---|---|
| 根目录 `LICENSE` | GNU GPL 第 2 版全文 | 保留原文本和来源 commit。[LICENSE][license] |
| `plane`、`spline`、`subdiv*` 等几何文件 | Greg Green 的移植署名，保留 Martijn van Engeland 2005 的 FREE!ship 原始署名；文件头写 GPL 第 2 版或后续版本 | 按实际使用文件保留两层来源和文件头，不把移植稿说成全新独立算法。[文件头示例][source-header] |
| `dialogdata.h`、`layerdialog.cpp` | Greg Green 自身署名，GPL 第 2 版或后续版本；部分其他 UI 文件无独立文件头 | 引入前按文件记录；无单独头不能当作无许可限制。[对话框数据头][dialog-license] |
| `mempool.h` | 开头标注 MIT、Mohammad Dashti 2014；该文件只有简短标注，未附完整 MIT 正文 | 若采用池实现，先补齐其原始出处和许可正文；或使用本工程明确来源的容器方案。[内存池头][pool] |
| `openglwindow.cpp/.h` | 除项目声明外，还含 Qt 文档示例的 Digia 2013 及 BSD 条款 | 若采用该文件，保留这段第三方声明；当前渲染无需替换。[Qt 示例声明][gl-license] |

这里记录的是仓库内可核对的声明，不作额外法律结论。以后真正引入代码时，在本工程 NOTICE 中逐文件写明原提交、改动与保留的署名。这次只读审查不改变产品的来源记录。

[repo]: https://github.com/gpgreen/ShipCAD/tree/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd
[readme]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/README.md#L23-L75
[version]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/version.h#L44-L75
[plane]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/plane.cpp#L38-L79
[plane-line]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/plane.cpp#L136-L150
[spline-build]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/spline.cpp#L114-L228
[spline-value]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/spline.cpp#L806-L846
[spline-cut]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/spline.cpp#L655-L695
[subdivide]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L3579-L3679
[averaging]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivpoint.cpp#L333-L413
[cut]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L2998-L3026
[intersection]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/intersection.cpp#L218-L228
[hydro]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/hydrostaticcalc.cpp#L920-L1058
[load]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/shipcadmodel.cpp#L930-L1020
[controller]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/controller.cpp
[undo]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/shipcadmodel.cpp#L283-L349
[project-dialog]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCAD/projectsettingsdialog.cpp#L37-L126
[layer-dialog]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCAD/layerdialog.cpp#L123-L188
[resistance]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/resistance.h#L40-L93
[point-invalidate]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivpoint.cpp#L635-L639
[invalidate]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L1379-L1391
[model-invalidate]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/shipcadmodel.cpp#L175-L221
[resistance-write]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/filebuffer.cpp#L326-L354
[buffer-read]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/filebuffer.cpp#L117-L264
[buffer-string]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/filebuffer.cpp#L251-L275
[project-commit]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/controller.cpp#L1971-L1991
[loops]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L2727-L2761
[save-surface]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L3499-L3529
[transform]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/shipcadmodel.cpp#L768-L845
[dialog-data]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/dialogdata.cpp#L112-L117
[units]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/projsettings.cpp#L301-L353
[layer-name]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivlayer.cpp#L159-L170
[iges]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/iges.cpp#L515-L553
[iges-entry]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/shipcadmodel.cpp#L1533-L1537
[tests]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/UnitTests.pro
[test-spline]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/spline/tst_splinetest.cpp#L293-L366
[test-face]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/subdivface/tst_subdivfacetest.cpp#L161-L231
[test-intersection]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/intersection/tst_intersectiontest.cpp#L152-L198
[test-hydro]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/hydrostaticcalc/tst_hydrostaticcalctest.cpp#L98-L171
[test-surface]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/subdivsurface/tst_subdivsurfacetest.cpp#L56-L69
[test-patch]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/UnitTests/developedpatch/tst_developedpatchtest.cpp#L103-L111
[build]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/ShipCADlib.pro#L7-L15
[entity]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/entity.h#L63-L77
[license]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/LICENSE
[source-header]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/subdivsurface.cpp#L1-L28
[dialog-license]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/dialogdata.h#L1-L18
[pool]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/mempool.h#L1-L5
[gl-license]: https://github.com/gpgreen/ShipCAD/blob/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd/ShipCADlib/openglwindow.cpp#L29-L64
