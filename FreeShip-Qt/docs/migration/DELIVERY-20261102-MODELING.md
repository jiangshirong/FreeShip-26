# 本机更新记录：模型编辑补全

2026-11-02。本次按整个模型编辑方向交付，不再按一个参数窗口拆分。
新建船型、点边合并与连接、约束和点组、面处理、网格面板与圆柱、船型变换、
模型检查、标记及相关显示均已接入。具体行为与限制见[完成清单](MODEL-EDITING-COMPLETION.md)。

Qt 构建使用 `build/modeling-runtime-01`，最终计算组件为 `build/modeling-service-11`。
完整包先生成到 `build/modeling-package-01`，验证后放入 `dist`；原包备份位置为
`build/dist-before-modeling-20261102`。实际程序为 `dist/FreeShipQt.exe`。
旧 FreeShip、QtPilot、用户模型与设置和 Win+R `fs` 入口不随此次更新改动。

## 回归结果

| 运行 | pass | fail | review | not_run | error |
|---|---:|---:|---:|---:|---:|
| [完整模型编辑](../../../FreeShip-Tests/reports/qt-modeling-20261102-g/report.json) | 2781 | 0 | 2 | 2 | 0 |
| [原有变换、挤出、镜像及导出](../../../FreeShip-Tests/reports/qt-advanced-modeling-20261102-b/report.json) | 1436 | 0 | 15 | 5 | 0 |
| [控制曲线与流线](../../../FreeShip-Tests/reports/qt-auxiliary-modeling-20261102-a/report.json) | 841 | 0 | 15 | 5 | 0 |

前两组使用最终计算组件。曲线回归在旋转轴心修正前完成，曲线实现随后没有改动；
不把它写成最终二进制的重复运行。三种旧船模、独立数学样本、直接读取 FBM 的
原核心探针分别保留。DXF 比较没有降低精度或放宽实体预期。

模型编辑覆盖实际 Qt 动作、参数窗口、取消、撤销、重做、保存重开和四视图组件
事件。点组中心拖动、成员点单独编辑、约束传播与循环拒绝、曲线随点删除、图层
全部材料/计算开关、标记和细分法向都有独立检查。

最后的变换回归发现了旋转轴心接入错误：旧 `AddPoint` 会把负 Y 截成零。现在
使用带符号的加法，原点旋转、标记旋转和非零轴心越过中心线均验证通过。
此前网格、点组保存、曲线删除和自动分组中发现的问题也已修正。失败证据保存在
测试工程 `maintenance/modeling-fixed-regressions-20261102.json`，复现源码继续
保留在 `qt_modeling.py`、`qt_modeling_extra.py`、`qt_advanced.py` 及共享探针中。

Lackenby 使用原有 Single 积分口径，矩形样本的舍入偏差和目标收敛范围在完成
清单中单列。这些回归不代替后续专业计算验证。9a 编辑后的旧绘图采样除零仍记
`not_run`，独立旧 DXF 对照继续执行，不以缺失画面对照冒充通过。

## 实际运行包复核

- [完整包模型编辑流程](../../../FreeShip-Tests/reports/qt-modeling-package-20261102-a/report.json)
- [正式目录模型编辑流程](../../../FreeShip-Tests/reports/qt-modeling-delivery-20261102-a/report.json)
- [正式目录启动与最近文件恢复](../../../FreeShip-Tests/reports/qt-startup-modeling-delivery-20261102-a/report.json)

这些入口直接运行被验目录里的程序，核对完整资源清单、来源、程序指纹和逐阶段
场景/DXF/剖面。输入文件和设置均为私有副本，不修改用户最近打开记录。
安装指纹和实际运行计数另存为测试工程 `maintenance/modeling-installed-20261102.json`。

整体窗口外观、真实鼠标/焦点、多屏和高 DPI 尚未人工验收，分别保留 `review`
或 `not_run`。隐藏窗口和截图没有代替这些验收。后续工作是专业流程、板材展开、
其余格式交接、剩余 Pascal 核心替换，以及完整视觉、本地化与发行工作。
