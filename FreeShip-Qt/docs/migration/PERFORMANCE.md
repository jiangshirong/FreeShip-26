# 阻力与推进

2026-11-03 更新：五项推进的默认参数、输入输出协议、后处理和缓存衔接已由 C++
执行；窗口及导出保持现有 Qt 路径。十种阻力计算也已迁入 C++，外部求解器限制仍在。
详见 [原生推进流程](NATIVE-PROPULSION.md) 和 [原生阻力流程](NATIVE-RESISTANCE.md)。以下保留首次接入 Qt 的范围和证据。

2026-11-02。入口在“计算 → 阻力计算”和“计算 → 螺旋桨与推进”。
参数、结果表、曲线、报告、取消、保存工况均由 Qt 管理。计算仍调用当前 Plus 的
Pascal 方法及部分外部程序，没有把这些公式记成已完成的 C++ 核心。

## 本次接入

十个阻力入口：Delft、Holtrop、Hollenbach、van Oortmerssen、Fung–Leibman、
OST、圆舭高速艇、多体船、KAPER、滑行艇。五个推进入口保留原 Task1–5 的区分。

计算在当前文档的副本上运行，不产生编辑历史。取消或失败保留上一次完整报告。
“提取参数”明确触发，阻力结果只供同一文档、同一版本的推进计算读取，避免换了
船模或修改后继续拿旧阻力计算。更改参数后，窗口提示旧报告尚未重算。

“将参数保存到船模”使用旧 FBM 的计算记录，可撤销、重做和保存重开。旧格式没有
保存每一个窗口字段，因此另提供完整参数 JSON 的保存与载入，校验方法、版本和
单位。程序目录不写入参数文件。CSV/JSON 保留原始曲线坐标，显示的小数位不改变
未编辑参数的底层值；TXT/PDF 附本次实际计算参数及原方法说明。

## 实测范围

`FreeShip-Tests/reports/qt-performance-final-b`：627 pass、0 fail、1 review、
4 not_run、0 error。测试不是船舶试验或规范认证。

- 15 个入口都经过实际 Qt 动作和参数窗口；12 个主方法的样本得到完整结果。
- Delft、Holtrop、Hollenbach、Oortmerssen、Fung、KAPER、滑行艇的代表工况，
  与单独构建的原 FreeEdit/计算窗入口逐点一致，不设数值误差豁免。
- 独立进程重跑 HOLLENBH、OORTMERS、fungleib、hship、PropPred、FPPCALC，
  对比原始结果文件。FPPCALC 曲线另与文件逐行解析的独立预期核对。
- 滑行艇 0–8 的九个分支、FPPCALC 的十一类系列各有样本。不能据此认定所有
  次级系列、所有参数组合和适用域都已验证。
- 参数事务、保存重开、完整参数文件、失效阻力提取、无效输入、取消、重复计算
  取消保留结果、失败导出保护原文件和 CSV 全精度坐标都有检查。
- 隐藏窗口截图和 PDF 首页已查看；桌面鼠标、焦点、多屏及完整视觉一致性仍待验收。

## 此次发现并处理的问题

位置以旁边保留的 `FreeShip-Plus-Lazarus` 为根。修改只在 Qt 的隔离构建副本中应用，
旧版源码和程序未改动。这些是当前保存版本的实际问题，不推断是哪位作者在哪年引入。

| 位置 | 情况及处理 |
|---|---|
| `Forms/FreeResistance_RBHSDlg.pas:2221`，`CalculateResistanceOST` | 使用从未赋值的 `FExecDirectory`，找不到 hship。改为读取用户配置的计算程序目录。 |
| `Forms/FreeResistance_MHDlg.lfm:813`，`NserBox`；同名 pas 的 `CalculateResistanceOST` | 控件最大值为 10，但方法编号需要 14–24。编号被截断后，结果读取使用未初始化的列数/偏移。修正范围，并拒绝不属于该流程的结果编号。 |
| `Forms/FreeResistance_HollenDlg.lfm`，`StepSpeedBox` | 这个字段实际表示最大航速，控件却最多接收 10。修正上限与名称；Delft 的步长字段仍按步长处理。 |
| `Forms/FreeResistance_OSTDlg.pas`、`FreeResistance_RBHSDlg.pas`、`FreeResistance_MHDlg.pas`，`CalculateInWorkspace` | 无附体时 `R_r` 未初始化，后续可能把内存值加进阻力。现先清零。不能把旧版随机值当对照真值。 |
| `Forms/FreeResistance_PlaningDlg.pas:2334`，结果发布 | 最大航速过低时 `Imax` 无赋值；不足四个点时还会读数组前面。初始化数组/索引并给出工况错误，阻止崩溃和发布坏结果。 |
| `Units/FreeCalculationFiles.pas:228` | 共享阻力表最多接收十行，但 Holtrop 实际写二十行。保留数值列校验，接受真实行数范围。 |
| `Units/FreeProcess.pas:228`，`Child.CloseInput` | PropPred 完成后等待回车，关闭输入只会让它反复提示。仅对这个程序发送回车；超时、取消、退出码及输出校验仍执行。 |
| `Forms/FreePropeller_Task5Dlg.pas`，读取 `OUT.` | 把空行和七列汇总按八列曲线读取；首点判断还访问第零项。改为按实际格式区分曲线与汇总，并先判断首点。 |
| `Units/FreeResourceStrings.inc:982`，`rs_kn` | Pascal 不区分大小写，`kn` 与 `kN` 标识冲突。航速明确标为 kn；单条曲线另标清阻力、功率、角度或系数，保留原坐标中的倍率。 |

## 仍然没有完成的部分

1. **Task1–3 的数值运行。** 本机 CALCPROP（SHA-256
   `947757d7bf58eb4a087033b1206f15613053057d0ccf79f307bdfe6183b0f3a8`）
   在独立 ASCII 目录运行同样退出 0、没有 RES1/2/3 输出。三个结果明确记 `not_run`；
   Qt 会报告缺少结果，不返回假零，也不替换成另一套公式。
2. **Troost B 系列。** 配套 PROPOL 是旧 DOS 程序，本机 64 位 Windows 不能直接执行。
   当前未提供替代求解器。其他系列使用 FPPCALC。
3. **原列表中的空入口。** 滑行艇 Almeter 没有对应计算分支，选择后说明原因。
   多体船列表末尾的 Schwetz 2002、Sahoo 2004、Grubisic 2005 没有对应编号分派，
   本次不把它们映射到别的方法。需取得实现及参考数据后再补。
4. **经验公式的全面审计。** 代表工况和原结果一致不证明原公式正确。包括
   Hollenbach 最小阻力分支的索引、各系列适用范围和自动参数修正，仍需逐项审计。
5. `Hydrodyn_*` 的附加质量、操纵性、倒车、其他专用工具和数据库查看器仍未迁移。
   天气衡准、规范判定、板材展开、其他格式交接、Pascal 核心替换也没有并入本次完成项。

## 构建与来源

`bridge/performance_adapter.py` 从原 `FreeEdit.inc` 提取初始化/保存赋值，从 LFM
对应标签建立参数表。`PerformanceEngine.pas` 管理只读任务和结果；
`PerformanceHooks.pas` 提供取消、非模态错误与有限大小的诊断留存。
算法仍在原 `FreeResistance_*Dlg`、`FreePropeller_Task*Dlg` 中执行。

本机包的 `calculation-tools.json` 只引用已有的 `FreeShip-Tests/fixtures/solvers35/`
目录，不复制外部程序。FPPCALC 自带说明写有教育及非商业用途限制，不能把这些
二进制一并当作 GPL 源码发布。外部工具的许可和可替代实现另行处理。
没有使用 ShipCAD 的阻力或推进计算代码。
