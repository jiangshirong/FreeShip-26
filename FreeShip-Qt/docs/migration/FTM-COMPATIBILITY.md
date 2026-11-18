# FTM 兼容读取

2026-11-13 用户调整：只保留 FTM 读取，取消保存为 FTM。打开后使用同一个
文档状态和编辑历史，保存统一输出 FBM。下方首轮交付结果是历史记录。

## 文件约定

依据 `FreeShip-Plus-Lazarus/Units/FreeFileBuffer.pas` 的 `TFreeTextBuffer`，
以及 `FreeShip.inc` 和各数据结构的版本门控实现。不是给 FBM 改扩展名，
也不是 OBJ/FEF 那样的几何交换文件。

- 文件头和版本名称沿用原版，支持解析器的 0–44 版本分支。
- 坐标/平面、实体名称、约束、分组按原版行结构读取。
- JPEG 从十六进制还原，不重新压缩。
- 文本使用 UTF-8；内部 FBM 字段按文档代码页转换，不能表示的字符拒绝转换。
- 数值解析为原格式的 IEEE 754 Single；不调整几何采样精度。
- 二进制计算记录的内存填充字节不属于 FTM 数据，转换会规范化填充。

`TFreeFileBuffer` 和 `TFreeTextBuffer` 分别拥有自己的位置字段。基类计算参数
读取中的字节偏移赋值不等于修改文本子类的行号，不能仅看这条赋值就认定旧版
计算参数错位。回归直接编译未修改的原版读写单元来交叉验证。

## 使用与保护

打开对话框接受 FBM/FTM；另存为仅提供 FBM。打开 FTM 后点击保存，默认建议
同名 `.fbm`，要求用户确认目标。取消后保留未保存修改，原 FTM 不变。
会话拒绝 `.ftm` 保存目标，服务拒绝 `format=ftm`，避免仅隐藏入口仍能写出。
原子替换、前一版本备份、冲突检测和保存回执核验沿用 FBM 实现。

截断、非法数值/引用、不认识的尾部和超限文件拒绝加载，保留当前文档。
文本转换失败不会覆盖源文件。这些限制不是所有历史 FTM 都能读取的承诺。

内部文本编码工具保留用于生成版本测试夹具，应用保存路径不调用它。

## 测试

统一入口：

```powershell
python FreeShip-Tests/run_tests.py --suite qt-ftm --suite qt-save-protection --runtime <候选包> --output <报告目录>
```

`qt/fbm_codec_tests.cpp` 检查 45 个版本及包含图片、标记、分组、约束和锚点的
合成数据；文本数值往返必须保持 Single 坐标精确一致。
`qt_ftm.py` 检查真实 board.ftm、FBM/FTM 互转、异常文件保护、保存确认和
实际 Qt 会话的保存对话框、取消、转存 FBM 和重开。`qt/ftm_oracle.pas` 在
隔离副本中调用原版 SaveProject 生成 FTM，验证本程序读取，不修改原版源码。

窗口测试在非输入桌面执行，不能代替用户操作系统文件选择器的人工验收。

## 首轮交付结果（取消 FTM 保存前，保留审计）

`C:/FreeShip26Tests/reports/ftm-02`：FTM 专项 492 pass / 0 fail /
0 error / 3 not_run；保存保护 133 pass / 0 fail / 0 error / 4 not_run。
原版生成 FTM、原版读取 C++ 生成的 FTM，以及两方向坐标与实体数量检查通过。
45 个版本的文本往返及原有 FBM 解析回归通过。测试代码保存在统一测试目录。

已替换 `FreeShip-Qt/dist`。FreeShipQt.exe SHA-256：
`d238ea6bfb83a1b3cc729f1fbc6b70c77182deab1587d126a4892d3efb51ab31`。
安装后 `qt-ftm-installed` 直接验证正式服务和 Qt 程序：19 pass / 0 fail /
0 error / 3 not_run，报告在
`C:/FreeShip26Tests/reports/ftm-installed-20261113/qt-ftm-installed/report.json`。
not_run 为真实桌面和原生文件选择器验收，不把它们记为通过。

首次测试的窗口探针命令名写错，已改为现有 `move` / `save_action`；该失败报告
保留在 `ftm-01`，未修改行为预期来消除失败。构建期间的源码变化由指纹校验
拦截，最终包与测试时源码一致。

统一入口上传归档索引：`archive-20261113-152147-d05077`、
`archive-20261113-152257-e6af57`。仅确认上传回执，没有下载回验。

## 仅保留读取的验证（2026-11-13）

`C:/FreeShip26Tests/reports/ftm-readonly-09`：FTM 专项 495 pass / 0 fail /
0 error / 4 not_run；保存保护 133 pass / 0 fail / 0 error / 4 not_run。
实际保存动作打开仅含 FBM 的对话框；取消后修改标记和原 FTM 保留；确认后
保存为 FBM，重开场景和 DXF 保持一致。服务拒绝 FTM 输出格式，会话拒绝
直接覆盖 FTM 目标。not_run 为系统原生文件选择器/真实桌面等验收边界。

早期失败来自后台探针对 Windows 原生文件对话框的错误控制方式。正式运行
仍使用系统原生对话框，验证模式显式使用 Qt 对话框并操作实际保存/取消按钮；
没有放宽取消后不得保存的预期。此前失败报告保留。

候选正式包：`C:/FreeShip26Tests/build/ftm-readonly-package-20261113`，
FreeShipQt.exe SHA-256：
`6ccbf5b732423f0a357146a5ac9954ecb8b58105f0a2f81750f5dcd86e6995e0`。
用户确认关闭后，已替换正式 `FreeShip-Qt/dist`。安装前确认候选包源码指纹
与工作区一致，资源差异仅为包来源记录；保留此前安装包用于回退。
正式目录的 `qt-ftm-installed` 验证为 27 pass / 0 fail / 0 error / 4 not_run。
报告：`C:/FreeShip26Tests/reports/ftm-readonly-installed-20261113/qt-ftm-installed/report.json`。
归档索引：`archive-20261113-154753-218058`，仅上传回执确认，未下载回验。
