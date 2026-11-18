# 本机更新记录：变换、挤出和镜像

2026-11-01。本批范围是移动、缩放、旋转、边挤出和面镜像，包含 Qt 参数窗口、
撤销/重做及保存。使用入口、规则与剩余范围见 [实现记录](ADVANCED-EDITING.md)。

候选构建为 `build/advanced-runtime-01` 和 `build/advanced-service-03`。
完整包为 `build/advanced-package-02`，正式位置为 `dist/FreeShipQt.exe`。
上一版完整包保留在 `build/dist-before-advanced-20261101`。
旧 FreeShip、QtPilot、用户模型与设置、Win+R `fs` 入口均保留。

## 验证记录

| 运行 | pass | fail | review | not_run | error |
|---|---:|---:|---:|---:|---:|
| [新增编辑完整回归](../../../FreeShip-Tests/reports/qt-advanced-20261101-d/report.json) | 1436 | 0 | 15 | 5 | 0 |
| [原有控制曲线与流线](../../../FreeShip-Tests/reports/qt-auxiliary-advanced-20261101-b/report.json) | 813 | 0 | 15 | 5 | 0 |

三种模型执行实际 Qt 动作、取消、编辑、撤销、重做、保存重开。原核心独立坐标、
主尺度、标记及拓扑预期通过；完整 DXF 逐实体匹配。9a 编辑样本的旧画面因原样条
采样除零而无法完整生成，原因、符号化栈和独立旧 DXF 均保留，没有把该对照计为通过。

首次目录复核发现挤出角点初始化依赖地址排序，候选已撤下并修复。四进程重复编辑
和独立新点属性检查现已通过，失败证据压缩保留在测试工程的
`maintenance/advanced-fixed-regressions-20261101.json`，没有清掉失败后直接重跑。

最终完整包复核与正式目录复核另见：

- [完整包的五项编辑](../../../FreeShip-Tests/reports/qt-advanced-package-20261101-b/report.json)
- [正式目录的五项编辑](../../../FreeShip-Tests/reports/qt-advanced-delivery-20261101-b/report.json)
- [正式目录的启动恢复](../../../FreeShip-Tests/reports/qt-startup-advanced-delivery-20261101-b/report.json)

上述复核均使用私有文件和配置，不改用户的最近文件历史。测试源码已接入统一
`qt-advanced`，旧曲线/流线回归继续保留。真实桌面鼠标、完整窗口观感和高 DPI
验收仍分别为 `not_run`、`review`，没有用隐藏窗口事件冒充用户验收。
