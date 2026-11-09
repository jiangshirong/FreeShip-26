# 本机更新记录：控制曲线和流线

2026-11-01。正式 Qt 目录 `FreeShip-Qt/dist` 已更新；旧版程序、QtPilot、用户模型、
设置和 Win+R 的 `fs` 入口未改。上一版完整包保留在
`FreeShip-Qt/build/dist-before-curves-20261101`。

新增选边建控制曲线、视图取流线起点、选择、删除、显示、撤销及保存重开。
本批还修复了改点或调整精度后流线缓存可能没有失效的问题。
具体来源、实现及剩余边界见 [控制曲线和流线](AUXILIARY-CURVES.md)。

## 验证结果

| 运行 | pass | fail | review | not_run | error |
|---|---:|---:|---:|---:|---:|
| [完整曲线/流线回归](../../../FreeShip-Tests/reports/qt-auxiliary-20261101-d/report.json) | 813 | 0 | 15 | 5 | 0 |
| [原有显示/曲率回归](../../../FreeShip-Tests/reports/qt-display-curves-20261101/report.json) | 518 | 0 | 11 | 5 | 0 |
| [正式目录的实际程序](../../../FreeShip-Tests/reports/qt-auxiliary-delivery-20261101/report.json) | 151 | 0 | 3 | 1 | 0 |
| [正式目录启动恢复](../../../FreeShip-Tests/reports/qt-startup-curves-delivery-20261101/report.json) | 54 | 0 | 0 | 17 | 0 |

三种模型的控制曲线/流线数据和旧核心一致，DXF 几何对照通过。曲线添加或移除时
型线图保持原结果；改点后的结果与对应修改后的模型对照。最后两组直接运行
`dist/FreeShipQt.exe`，使用私有模型与配置，没有修改用户的上次文件历史。

Qt 主程序 SHA-256：
`764ef310e3d60bdd52aefc28347e41fc7e3a02dc98ece2c73361289d81be5af1`

桥接程序 SHA-256：
`5a7d3812ee7a75caeff5880e4675eaaaa7cd286a1d8aad7aa2dc7c1ad5815285`

测试源码已并入 `FreeShip-Tests` 的 `qt-auxiliary` 入口。完整图片保留供核对；
`review` 和 `not_run` 没有算作通过，未操作真实桌面进行验收。
控制曲线专用曲率梳、整形/拟合、复杂分叉和高级拓扑编辑仍未完成。
