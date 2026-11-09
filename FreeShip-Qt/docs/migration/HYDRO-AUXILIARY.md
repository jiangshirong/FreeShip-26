# 静水力附加报表

2026-11-05。

工程设置的静水力页新增“生成 Bonjean、横剖面面积与重量附加表”。启用后，单次
静水力计算会生成附加表。计算窗口可查看每张表，点击“保存当前附加表”选择保存位置。
没有选择保存时不会向船模目录写入文件。该开关支持撤销、重做和 FBM 保存重开。

| 文件 | 内容 |
| --- | --- |
| SACs.txt | 各水线下的横剖面面积，保留已迁移的对称修正规则 |
| Bonjean.txt | 水线与肋位的原始截面积矩阵 |
| Sterns.txt | 各水线的首尾纵向位置 |
| Weights.txt | 图层各细分面的完整面积、重量、形心及纵向范围 |

附加表使用剖面线窗口中已定义的水平水线，坐标为模型原点下的 Z。它们与主报告的
横倾、纵倾工况分开。计算只读模型：每个肋位求交一次，各条水线复用同一组曲线；
不重建每条水线的细分网格，也不移动船体或改变设计吃水。

文件位置按 `FreeProjectSettings.SaveBinary`：39 版开始保存该布尔开关。
旧文件打开和无修改保存保持原字节，实际启用时才升级。关闭后不会额外生成这些表。

实现参考 `FreeHydrostaticCalc.BuildAuxiliaryReports`、
`FreeSubdivisionLayer.CalculateSurfaceProperties`，并复用现有 C++ 求交与静水力计算。
这两处旧行为没有照搬：

- 旧首尾水线表把绝对水线 Z 当作吃水，船底不在 Z=0 时重复加上船底高度。
  新表明确区分水线 Z 和相对船底的吃水；无排水的水线在 JSON 中标记为 dry。
- 旧重量明细每个子面只记录最后一个三角形的面积和形心，与图层合计不一致。
  新明细使用完整子面的面积、面积加权形心和纵向范围。

SAC、Bonjean 和首尾表保留原文本排列。重量表改为显式标注的 **Weights v2**，
不冒充旧定宽格式；依赖旧 Weights.txt 列位置的外部程序需要适配。
公制密度为 t/m³、厚度 mm、重量 t；英制分别为 lb/ft³、in、long ton。

需要 1–99 条水线、最多 99 个肋位。缺少肋位时仍提供重量和首尾表；缺少水线、
漏点或截面积计算不完整时说明原因，不生成部分冒充完整的文件。
2026-11-09 已接入[非对称截面积表](ASYMMETRIC-HYDROSTATICS.md)，采用有向实际截面，
不再套用对称面积修正；主报告缺少无关可选字段不会阻断这些已验证的附加表。
后续[侧投影](LATERAL-PROJECTION.md)也已补齐，旧单侧进流角仍不提供。
重量明细超过大小上限时拒绝生成，不截断数据。

统一回归入口：

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-hydro-auxiliary --runtime FreeShip-Qt/dist
```

测试包括 45 个格式版本、矩形船体独立积分、平移后的坐标基准、公英制转换、
旧表对照、实际设置与保存按钮、取消和已有静水力回归。源码保存在
`FreeShip-Tests/qt_hydro_auxiliary.py`。最终安装和报告指纹见
`FreeShip-Tests/maintenance/hydro-auxiliary-installed-20261105.json`。
隐藏窗口检查不代替真实桌面、原生文件对话框和多屏验收。
