# 启动恢复、半船显示与曲率梳

2026-11-01。这批补的是正常使用中缺少的文档恢复，以及四视图的两项旧功能。
StudioLight 顶栏、四视图排列和型线图入口没有重新设计。

## 再次启动时恢复船模

之前 Qt 只处理启动参数和“打开模型”，没有把上次成功打开的路径写入配置。
因此关闭窗口以后，下一次启动不知道该打开哪个文件。这是迁移遗漏。

现在成功打开或另存为后，会把原文件的绝对路径记入当前用户配置目录下的
`recent-files.json`，位置由 Qt 的 `AppConfigLocation` 决定。不是后台私有副本，
也不读取或改写旧 Lazarus 的配置。无启动参数时恢复该文件；显式传入文件时优先打开
传入的文件。“文件 → 最近打开”保留最近十个不同路径。

写入采用原子替换和文件锁。失败的打开不覆盖成功记录；另存为只有目标文件成功写入
后才记住新路径。上次文件被移动、删除或损坏时，窗口仍能正常使用，状态栏说明情况，
并取消下一次自动重试。配置不能写入时不影响模型打开或保存。

旧 Qt 没有留下可恢复的记录，所以升级后需要先成功打开一次船模。
本功能恢复已保存文件，不恢复未保存修改，也不包含镜头及窗口布局恢复。

实现：`src/app/file_history.*`、`startup.cpp`、`model_session.cpp`。

## 半船与双舷

“视图 → 双舷显示”对应原 FBM 的 `Visibility.ModelView`。关闭时，俯视和透视只画
源侧船体；横剖面按工程设置的分割剖面，把后体放在另一侧。跨过分割剖面的边和面
先裁切，再分别投影，不跨中心线连接。中间站线按旧规则在两侧显示。

填色、控制网、点选、边/面选择、框选、坐标标注和拖点使用同一套位置规则。
从后体一侧拖动时，横向屏幕位移换算回正确的 Y 方向；选中点跨越前后半船时，仍然
整体移动同一个世界坐标增量。坐标文字显示实际模型坐标。

来源：当前 Plus 的 `FreeSubdivisionControlPoint.inc`、`FreeSubdivisionControlEdge.inc`、
`FreeSubdivisionControlFace.inc` 和 `FreeIntersection.inc`。适配集中在
`src/render/body_projection.h`、`hull_view.cpp`，没有套用 ShipCAD 的窗口代码。

## 剖面线曲率梳

“剖面线”的曲率列可以勾选，视图菜单提供总开关及增大/减小比例。保持原线框显示
语义：船体填色开启时不叠加曲率梳。选项、比例和逐条勾选都写入原 FBM 字段，支持
撤销、重做和保存重开。控制曲线和流线的编辑仍待迁移。

C++ 求交结果附带原样条控制点及折角标志；首次显示所选曲率梳时才求导和采样。
采样密度保持旧版四档 `384/768/1536/3072`，折角插点、梳齿间隔、Single 舍入、
有限差分一阶导数及比例规则均有独立旧核心对照。每个不可变模型快照缓存结果，
镜头移动和缩放不重复求导；关闭曲率显示时不生成这份缓存。

曲率计算按 `FreeSpline.Curvature/Draw` 和 `FreeGometry_Functions.Normalize` 移植，
见 `src/render/curvature_comb.*`。显示失败不会改写船体或导出曲线。
型线图与 DXF 保留原生成路径；附加的样条数据写在四视图的 `sections.json` 中。
磁盘缓存键包含原生计算程序哈希，升级后不会误读缺少样条定义的旧缓存。

## 回归与边界

统一入口为 `qt-startup` 和 `qt-display`，脚本及 C++ 探针保存在 `FreeShip-Tests`。
启动测试使用私有配置、多个独立进程、中文/俄文路径，覆盖成功打开、显式启动路径、
另存为、失败打开、删除文件、损坏文件、损坏配置、写入失败和最近文件去重。

显示测试通过实际动作和曲率勾选回调执行；没有剖面线的样本只在测试副本里补一条。
三种模型执行切换、撤销、重做、保存重开，并比较完整场景、DXF 和剖面文件。
另用直接调用原核心的 `QtEditingFixture.pas` 检查保存结果和曲率采样，
不拿桥接程序的序列化输出反过来当数值预期。

当前完整显示回归记录为
[qt-display-20261101-c](../../../FreeShip-Tests/reports/qt-display-20261101-c/report.json)，
518 pass、0 fail、11 review、5 not_run、0 error。
曲率对照共有 48,402 个坐标值，Single 数值精确一致；半船填色、面选取、跨分割边、
框选、整组拖动和导航期间缓存复用另有独立预期。

背景图片及辅助线回归分别见
[qt-backgrounds-display-20261101](../../../FreeShip-Tests/reports/qt-backgrounds-display-20261101/report.json)、
[qt-overlays-display-20261101](../../../FreeShip-Tests/reports/qt-overlays-display-20261101/report.json)。
分别为 877 pass / 13 review / 8 not_run 和 755 pass / 48 review / 8 not_run，
均无 fail 或 error。启动候选回归
[qt-startup-20261101-b](../../../FreeShip-Tests/reports/qt-startup-20261101-b/report.json)
为 52 pass、17 not_run，无失败。

本批桥接构建为 `build/display-service-01`，C++ 复用 `build/topology-qt-01`。
运行包核对后更新 `dist/FreeShipQt.exe`；替换前保留旧包，不关闭用户进程。
实际路径复核分别记录在
[显示交付报告](../../../FreeShip-Tests/reports/qt-display-delivery-20261101/report.json) 和
[启动交付报告](../../../FreeShip-Tests/reports/qt-startup-restore-delivery-20261101/report.json)，
安装和执行状态以这两份报告为准。
隐藏窗口及离屏事件不是桌面验收；完整视觉一致性为 `review`，桌面操作为 `not_run`。

本批仍由主代理独立完成。旧源码、旧程序、QtPilot、用户模型和 Win+R `fs` 入口保持原状。
后续继续控制曲线、流线及其余编辑命令，再推进专业计算窗口和剩余核心替换。
