# C++ 模型快照与标注

2026-11-03。打开、新建、导入、编辑、撤销和重做后的界面数据，现在直接从
C++ 持有的 FBM 和实体 ID 表生成，不再把整个船模装回 Pascal 对象。
四视图和型线图继续使用现有 Qt 渲染器，窗口布局没有改动。

`src/document/publication.cpp` 读取工程属性、图层、控制网、名称、约束、点组、
剖面、标记线和背景图片，准备同一份原生几何输入。已有场景工作进程继续生成
网格、法线、控制曲线、流线、型线图与 DXF。JPEG 保留原字节。

同一不可变文档的重复快照复用已准备的数据；场景继续使用原有磁盘缓存。
输出先在私有目录生成，校验身份、校验和及完整模型，再用于提交历史。
提交阶段复用刚完成的校验结果，避免再次解析同一份大型网格。
保存仍写 C++ 文档中的完整 FBM；未编辑保存不会重新编码原文件。

静水力标注复用 C++ 计算内核。旧版无窗口入口用于折痕位置的计算吃水是零，
标注则使用工程吃水，这两个条件分别保留。回归还发现惯性矩公式里的 `1/12`
在旧编译器中按 Single 参与运算；按 Double 计算会使某些 KM 标注相差一个末位。
现已恢复原运算精度和舍入顺序，没有增加比较容差。

有一处有意修正：半船图层关闭对称后，中心面敞开，旧标注仍可能显示排水量、
KM 等数值。现在沿用原生静水力内核的漏水检查，不发布这组无效标注。
回归用独立的边归属检查证明样例在水下不封闭；保留旧结果差异为 `review`，
其余模型属性和导出仍要求相同。非空船体的零船长在提交前明确拒绝。

测试入口为 `FreeShip-Tests/run_tests.py --suite qt-publication`，源码在
`qt_publication.py`；文档编辑组合继续使用 `qt-document`。
覆盖三个现有船型、11 个历史版本、背景 JPEG、名称和约束、新建船型、
重复请求、关闭私有 Pascal 进程后的编辑、失败回滚和保存字节。
`maintenance/inspect_publication_pairs.py` 只解释留存样例的差异，不修改测试预期。

本轮交付时旧 Pascal 服务还提供空模型的偏好、系统颜色和翻译上下文；后续已由
[C++ 偏好初始化](NATIVE-PREFERENCES.md) 接替。显式旧诊断入口及未迁移的专业工具
兼容分支仍保留，不能据此宣称整个程序已去掉 Lazarus。
真实桌面鼠标、窗口视觉和设备验收继续单列；隐藏 Qt 回调不能代替这些验收。

来源是 FREE!ship / Plus 的 LoadBinary、`ModelService.WriteModel`、
`NativeLinesplanInput`、`OverlaySnapshot` 和 `FreeIntersection.CalculateArea`。
保留 Martijn van Engeland、Timoshenko V.F.、Mark Malakanov 的来源与
GPL-2.0-or-later 许可。本模块没有引用 ShipCAD。

最终候选为 `D:/FreeShip26Tests/build/publication-native-package-04`，已更新到
`FreeShip-Qt/dist`；上一版保存在 `build/dist-before-publication-native-20261103`。
最终报告分别为：

| 报告目录（位于 `D:/FreeShip26Tests/reports`） | pass | fail | review | not_run | error |
|---|---:|---:|---:|---:|---:|
| `publication-core-03` | 179 | 0 | 0 | 1 | 0 |
| `publication-document-02` | 13219 | 0 | 29 | 3 | 0 |
| `publication-migration-02` | 751 | 0 | 9 | 7 | 0 |
| `publication-installed-02/qt-publication` | 153 | 0 | 0 | 1 | 0 |
| `publication-startup-01` | 54 | 0 | 0 | 17 | 0 |

安装审计保存在 `FreeShip-Tests/maintenance/publication-native-installed-20261103.json`。
合计 14356 pass、0 fail、38 review、29 not_run、0 error，属于各报告检查次数，
不是独立用例数量。待审包括已有的指针次序/面片划分差异、新的开放半船标注修正
和界面视觉核对；没有把这些项目算作通过。旧源码、旧程序、试点和原船模指纹未变。
