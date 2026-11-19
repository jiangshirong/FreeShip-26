# 修复清单与代码依据

2026-11-12 整理。上游统一指向提交 `1de17e99fb2b1cc559cdf27c5cafb595a627a641`。当前代码链接指向本仓库；旧分支和测试工程位于同级目录。若发布时只上传 Qt 仓库，需要一起提供引用的测试源码，不能把本机路径当成公开证据。

“重点”表示适合在 README 说明数据或工程结果风险；“一般”表示主要影响功能可用性。它们不是 CVE 严重性评级。下列测试名是可复跑入口，历史报告摘要与哈希另见 [evidence.json](evidence.json)。本次只整理和复核证据，没有运行新的全量回归。

<a id="u01"></a>
## 01 · 保存失败后，正式文件可能缺失或不完整〔重点〕

原版保存已有项目时，先把正式文件改名为 `.Bak`，再生成、写入新文件。后续失败没有恢复正式文件的步骤。因此保存失败后，原文件名下可能已经没有完整模型；旧内容可能仍在 `.Bak`，不能说所有数据必然永久丢失。

本项目改为先完成临时文件，再替换正式文件；失败保留原文件及未保存状态。Lazarus 修复在 `Units/FreeSafeFile.pas::SaveProjectFile`、`FreeEdit.inc::File_Save`；Qt 对应 [model_file_save.cpp](../../src/core/model_file_save.cpp) 和 [fbm_codec.cpp](../../src/document/fbm_codec.cpp)。

依据：[原保存顺序](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L6090-L6135)；`save_safety.py`、`qt_save_protection.py`。已覆盖部分写入、刷新、备份、替换失败及文件占用；不承诺断电或所有网络文件系统下绝不丢失。

<a id="u02"></a>
## 02 · 打开失败会提前清空当前模型〔重点〕

原版 `LoadProject` 在解析新文件前先清空现有模型。新文件不存在、损坏或被截断时，旧的内存模型已被清掉；错误文件标识的分支还可能继续设置加载成功状态。这会危及当前未保存的修改，但不表示磁盘上的原项目也被删除。

本项目先解析、重建候选模型，通过后才替换当前文档。失败保留模型、选择和撤销历史。Lazarus 修复在 `FreeShip.inc::LoadProject` 及打开入口；Qt 对应 [document_service.cpp](../../src/document/document_service.cpp) 和 [model_session.cpp](../../src/app/model_session.cpp)。

依据：[提前清空](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeShip.inc#L2042-L2066)、[错误标识后的状态](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeShip.inc#L2228-L2243)；`file_safety.py`、`qt_document.py`。失败前后模型字节、对象状态及撤销/重做有检查；真实桌面操作另验。

<a id="u03"></a>
## 03 · 文件读写无法前进时，循环可能不退出〔一般〕

读取中的文件被截断，或写操作返回零字节时，原循环没有零进度退出条件，可能一直等待。现在短读写继续处理，零进度则报错退出并关闭句柄；失败不会公开半个输入缓冲区。

依据：[读取循环](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeFileBuffer.pas#L2177-L2191)、[写入循环](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeFileBuffer.pas#L2216-L2227)。Lazarus 修改 `FreeFileBuffer.pas`、`FreeSafeFile.pas`；Qt 使用 [fbm_codec.cpp](../../src/document/fbm_codec.cpp) 的受检查读取和原子写入。回归：`buffer_safety.py`、`save_safety.py`。这是故障处理修复，不代表磁盘本身可靠性提高。

<a id="u04"></a>
## 04 · 损坏文件中的长度和索引未完整检查〔重点〕

原版按文件中的 JPEG 长度分配并复制数据，部分点面引用未经完整边界检查；异常索引还可能被转成正常点号。损坏模型可能引发异常分配、越界访问或错误连接，不能仅凭“成功打开”认定模型完好。

本项目检查剩余字节、集合数量和引用索引；失败停止加载，并保留当前模型。Lazarus 对应 `FreeFileBuffer.pas`、`FreeSubdivisionSurface.inc` 及集合读取；Qt 对应 [fbm_codec.cpp](../../src/document/fbm_codec.cpp)。

依据：[JPEG 长度](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeFileBuffer.pas#L334-L349)、[面的点索引](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeSubdivisionControlFace.inc#L1232-L1254)；`buffer_safety.py`、`file_safety.py --case geometry`、Qt 文档回归。没有完成恶意文件的全面安全证明，不标作远程代码执行漏洞。

<a id="u05"></a>
## 05 · 密度与部分阻力输入的单位换算错误〔重点〕

原版使用 `0.44642857` 推导磅质量和密度换算。`1.025 t/m³` 被换成约 `65.01548 lb/ft³`，标准换算约为 `63.98866 lb/ft³`，密度数值偏高约 **1.60%**。此外，Kaper、Planing 的手工排水质量被当作体积，乘上长度换算系数的三次方，会改变后续计算输入的物理含义。

本项目分别处理长度、容积、密度和公吨/长吨换算，使用 `1 lb = 0.45359237 kg`。Qt 实现在 [project_edit.cpp](../../src/document/project_edit.cpp)，Lazarus 在 `FreeGeometry.pas`、`FreeProjectSettings.inc` 和 `FreeEdit.inc::Model_Scale`。

依据：[旧常数](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeGeometry.pas#L100-L103)、[换算入口](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeProjectSettings.inc#L262-L298)；`units_conversion.py`、`qt_native_project.py`。1.60% 是该密度换算的偏差，不是所有阻力或排水量结果的统一误差。已有英制文件不自动改数，需核对来源。

<a id="u06"></a>
## 06 · 切换单位后，剖面位置没有跟着转换〔重点〕

原版单位切换缩放了船体，却注释掉了相应交线位置的变换。例如原来 `x=10 m` 的横剖线，改用英尺后若仍为 `10 ft`，实际位置就变成 `3.048 m`，而不是原来的 10 米。随后重新求交得到的是另一位置的截面。

本项目让横剖线、纵剖线、水线、斜剖线及相关带单位的计算参数随单位换算；普通几何缩放与单位转换分开。Lazarus 在 `FSetProjectUnits`；Qt 在 [project_edit.cpp](../../src/document/project_edit.cpp)。

依据：[原调用](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeProjectSettings.inc#L274-L298)、[被注释的交线变换](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L8222-L8230)；`units_conversion.py`、`qt_native_project.py`。覆盖平面位置、曲线范围和重复公英制往返，不只比较换算常数。

<a id="u07"></a>
## 07 · 最大横剖面定位受坐标原点和显示站线影响〔重点〕

默认位置使用 `0.5 * ModelMax.X`，没有计入 `ModelMin.X`。船体若位于 X=100～120，默认值会落在船体之外的 X=60；缺少显示站线时，后续搜索不会修正它。截面积及 Cm、Cp 等系数因此可能受影响。

本项目在实际船体范围内搜索最大水下横剖面，独立于显示站线。Lazarus 在 `CalculateMidshipLocation`；Qt 在 [hydrostatics.cpp](../../src/document/hydrostatics.cpp) 的 `midship`。这是正确性修复，早期实现反而增加了求交次数，不能记成加速成果。

依据：[原搜索函数](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeHydrostaticCalc.inc#L1318-L1393)；`hydro_corrections.py` 和 `hydro_search.py`。已比较平移、站线变化、多峰及密集参考扫描；有限采样不能证明任意曲面的全局最大值。

<a id="u08"></a>
## 08 · 几何重建异常后，忙碌标志不恢复〔一般〕

原版中途重建失败后，`FIsBuilding` 可能一直为真，后续重建或绘制会直接退出。Lazarus 修复在 `FreeSubdivisionSurface.inc::Rebuild`：异常退出也恢复状态，失败不标记为构建成功。

依据：[进入时设标志](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeSubdivisionSurface.inc#L4582-L4594)、[只在正常结束复位](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeSubdivisionSurface.inc#L4681-L4687)；`file_safety.py` 的重建故障注入。Qt 使用候选文档发布，不使用这个 Pascal 状态字段；不重复计作另一项 C++ 修复。

<a id="u09"></a>
## 09 · 加载标志被同名局部变量遮蔽〔一般〕

`LoadBinary` 改的是局部 `FIsLoading`，监听器检查的却是成员字段。原意是加载期间不通知中间状态，实际并未生效。Lazarus 已去掉同名局部变量并保证状态恢复。

依据：[局部变量](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeSubdivisionSurface.inc#L4208-L4219)、[成员状态检查](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeSubdivisionSurface.inc#L113-L123)；`file_safety.py`。本条证明状态屏蔽失效，不凭此单独宣称所有加载都会崩溃。Qt 不沿用该对象事件模型。

<a id="u10"></a>
## 10 · 取消变换也会丢失重做历史〔重点〕

原版可能在新操作尚未确认时截断重做分支。具体路径是“撤销一步 → 打开 Lackenby 变换 → 取消”，原来的重做记录仍可能被删除。

本项目只在真实修改提交后截断分支，取消和无变化操作保留历史。Lazarus 修改 `CreateUndoObject`、`FreeUndoObject.inc`；Qt 由 [document_service.cpp](../../src/document/document_service.cpp) 提交历史。

依据：[原历史截断](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L195-L222)、[对话框前创建快照](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L8142-L8161)；`undo_background_safety.py`、Qt 编辑及 Lackenby 回归。

<a id="u11"></a>
## 11 · 外部计算失败、卡住或留下旧结果时，处理不完整〔重点〕

原等待循环没有超时，子程序退出码也未可靠地传回调用方作为失败。部分计算共用文件名；正常退出但没生成 OUT 的操纵性分支还会继续无限等待。后果包括界面长时间无响应，以及失败后仍进入结果读取流程、混用旧文件的风险。

本项目增加超时、退出码、取消及结果结构检查，每个任务使用独立目录，跨任务数据显式传递。Lazarus 在 `freeprocess.pas`、`FreeCalculationFiles.pas`；Qt 见 [外部工具交付与可靠性](../migration/CALCULATION-RELIABILITY.md)及 [hydrodynamics.cpp](../../src/document/hydrodynamics.cpp)。

依据：[原等待循环](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/freeprocess.pas#L35-L79)、[返回值处理](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/freeprocess.pas#L148-L157)；`process_safety.py`、`process_callers.py`、`calculation_contracts.py`。早期覆盖 22 种输出格式和 6 个真实外部程序；格式检查不等于经验公式正确，合法行边界上的截断仍可能不可识别。

<a id="u12"></a>
## 12 · 静水力吃水表重复计算终点〔一般〕

起点 1、终点 2、步长 0.5，在不提前退出的情况下，原循环会计算 `1、1.5、2、2`；满足写表条件时产生重复行。现在先生成有限、去重的吃水序列，再逐项计算，错误步长明确报错。

依据：[原循环和终点回填](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Forms/FreeHydrostaticsFrm.pas#L277-L288)。Lazarus 修改 `FreeHydrostaticsFrm.pas`；Qt 在 [hydrostatics.cpp](../../src/document/hydrostatics.cpp)。回归：`hydro_corrections.py`、`qt_hydrostatics.py`。

<a id="u13"></a>
## 13 · STL 无条件镜像非对称图层〔重点〕

原版每输出一个三角面，就把 Y 坐标取反再写一个，没有检查图层是否对称。非对称部件或已经建好的整船因此可能多出镜像面，后续网格计算或加工接收到的几何与设计模型不一致。

本项目按图层对称设置输出，中心面不重复，镜像面保持正确顶点顺序。Lazarus 修改 `File_ExportSTL`；Qt 对应 [model_exchange.cpp](../../src/core/model_exchange.cpp)。

依据：[原无条件镜像](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L4258-L4273)；`stl_symmetry.py`、`qt_exchange.py`。几何范围、三角数量、法向及旧错误负控有验证；未据此宣称所有切片器或加工链都已验收。此条不是上游已修过的 IGES 对称问题。

<a id="u14"></a>
## 14 · 导入 JPEG 背景图后访问已释放对象〔一般〕

原版先 `FreeAndNil(JPEGImage)`，随后读取 `JPEGImage.CompressionQuality`，该路径存在明确的空引用访问，可在导入图片时触发异常。

本项目在释放前保存属性，解码成功后才替换旧图；失败保留原背景图。Lazarus 修改 `FreeBackgroundImage.inc::Open` 及相关入口；Qt 使用 [背景图片流程](../migration/BACKGROUND-IMAGES.md)。

依据：[原释放后读取](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeBackgroundImage.inc#L335-L342)；`undo_background_safety.py --case background`。正常、损坏、取消和重复更换有检查；旧 JPEG 重存的像素损失是另一问题，不包含在本条空引用修复中。

<a id="u15"></a>
## 15 · 普通静水力计算隐式写删当前目录的附表〔重点〕

满足附表条件时，原版普通计算或提取船体数据会直接写 `SACs.txt`、`Bonjean.txt` 等相对路径文件。工作目录不可写时，计算也可能失败；多个任务或已有同名文件存在互相覆盖的风险。

Lazarus 已改为先在内存生成，明确保存报告时写入新的附表目录；Qt 的 [hydrostatics.cpp](../../src/document/hydrostatics.cpp) 返回独立任务结果，由窗口显式导出。没有把旧固定文件名留在用户工作目录当作通信协议。

依据：[原隐式写盘](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeHydrostaticCalc.inc#L1955-L2013)及同文件 `Clear`；`hydro_corrections.py`、`qt_hydro_auxiliary.py`。覆盖只读目录、同名文件、并发计算、取消和写入失败；没有声称社区每个 Access denied 都由这一原因造成。

<a id="u16"></a>
## 16 · 板材展开面积公式与方案评分不正确〔重点〕

原二维三角形面积式不是叉积面积公式，普通平板也能出现虚假面积误差。方案评分又把长度差和面积差直接相加，局部正负面积差还能抵消；换单位或缩尺可能改变评分权重。它用于选择展开候选，因此不只是标签缺单位。

本项目使用正确三角面积式，并以无量纲长度误差和绝对面积误差评分，保留带单位的原始指标。Lazarus 修改 `FreeDevelopedPatch.inc::Unroll`；Qt 对应 [plates.cpp](../../src/document/plates.cpp)。

依据：[原误差和评分](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeDevelopedPatch.inc#L1534-L1577)及同函数 `TriangleArea`；`plate_errors.py`、`qt_native_plates.py`。平面、圆柱、鞍面、尺寸变换和恢复旧公式的负控已有检查；几何误差接近零不等于符合材料成形或加工允差。

<a id="u17"></a>
## 17 · 部分随附旧 FBM 的计算参数布局无法读取〔一般〕

同一文件版本号下存在不同长度的历史计算参数记录。原读取器按当前布局读取，随附样本中有 37 个因此失败。不是“所有旧 FBM 都打不开”，也不是所有版本枚举都按四字节读。

本项目识别已确认的四种布局，保留有效字段并提示迁移；四个样本中无效的 Task5 段要求重新填写，不编造恢复值。Lazarus 在 `FreeFileBuffer.pas::PrepareLegacyCalculations`；Qt 对应 [fbm_codec.cpp](../../src/document/fbm_codec.cpp)。

依据：[原读取器](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeFileBuffer.pas)与[布局排查记录](history/17-旧文件兼容性仍需定位.md)；`legacy_compatibility.py`、Qt 文档回归。历史 82 个样本均可读取，有 2 项 JPEG 往返差异保留为待复核；原 FBM 无校验和，不能识别所有恰好落在合法字段边界的截断。

<a id="u18"></a>
## 18 · 工程设置的隐式改模、黏度端点及无效输入〔重点〕

**生成报告时可能改变船形。** 原版启用自动移动且船底不在零位时，报告生成会调用 `Face_MoveZAuto`；该函数跳过锁定点。如果船体同时有普通点和锁定点，就可能只移动一部分点，形成变形而不是整船平移。

Qt 已改为工程设置中的明确整体对齐操作；存在锁定或约束冲突时整笔拒绝，计算报告只读。依据：[报告触发](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeHydrostaticCalc.inc#L851-L852)、[只移动未锁定点](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeHydrostaticCalc.inc#L897-L949)；当前 [project_edit.cpp::alignToBaseline](../../src/document/project_edit.cpp)，测试 `qt_project_completion.py`。

同条还处理两个设置问题：水温未持久化，查表在 **30°C 返回零黏度**；风面积参考位置输入框被重置且没有写回。Qt 将黏度换算放到实际使用它的计算窗口，补齐 0–30°C 端点和保存约定，不照搬无效控件。原位置为 `FreeProjectSettings.inc::LoadBinary/SaveBinary/Edit`、[FindWaterViscosity](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeShipUnit_Functions.inc#L26-L58)和 `FreeProjectSettingsDlg.Execute`；当前 [water_viscosity.h](../../src/core/water_viscosity.h)。

这组修复落在 Qt，不表示旧 Lazarus 报告副作用已全部改掉。旧文件开关字节仍兼容保留，重新用原软件打开时可能继续执行。详见[工程设置收尾](../migration/PROJECT-SETTINGS-COMPLETION.md)。

<a id="u19"></a>
## 19 · 多轮廓、孔洞和非对称船体的面积与惯性矩算法〔重点〕

原 `CalculateArea` 为截线补镜像，把每个轮廓调整为同向；面积累加后，水线面惯性矩却只用最后一个轮廓留下的 `MomI`。孔洞不能正确作为负面积扣除，分离浮体的惯性矩也不能完整合计。静水力流程又在 Cm、Cwp 超过阈值时折半，不能可靠地区分重复模型、正常几何和参考尺度差异。

这可能使月池、多体船或非对称船体的水线面积、惯性矩、横剖面积及稳心指标失真。Qt 改用实际船面的有向截线积分，孔洞相减、分离船体相加，镜像只由图层决定；去掉系数阈值折半。扭曲四边形先固定三角剖分再裁切，使面积和体积对应同一船面。

依据：[最后轮廓的惯性矩](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeIntersection.inc#L263-L294)、[镜像与轮廓方向](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeIntersection.inc#L339-L389)、[按 Cm 折半](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeHydrostaticCalc.inc#L1819-L1827)。当前 [hydrostatics.cpp](../../src/document/hydrostatics.cpp) 的 `prepare/fullSection/fullArea/volume`。

独立算例：10×6 米水线面，扣除 6×2 米月池，水线面积应为 **48 m²**。保留旧算法的修正前 Qt 运行包得到约 **35.9994 m²**，修正后为 48 m²；横向水线面惯性矩应为 **176 m⁴**，旧算法结果约 **359.9836 m⁴**。这是迁移代码中的旧算法算例，不冒充未执行过的完整上游发行版桌面测试。原代码缺陷另由上述固定源码确认。

回归：`qt_symmetric_hydro.py`、`qt_asymmetric_hydro.py`、`qt_hydro_surface_integrals.py`。解析箱体、月池、多浮体和独立面元积分提供依据，不只要求新旧结果相同。几何重合检查的后续补齐见[半船约定](../migration/HYDRO-SYMMETRY-CONTRACT.md)，其中的 Qt 漏检不另算上游缺陷。未完成任意自交、部分重叠的布尔并集或船级社认证。

<a id="u20"></a>
## 20 · Archimedes HLL 输出的 XML 与字段含义错误〔一般〕

原 HLL 在 XML 声明前写空格，项目名未经转义直接进入标签；名称含 `&` 或 `<` 时不是合法 XML。三个吃水标志的纵向位置又全部写成 `ProjectDraft`，英制数据也未按采用的米制约定转换。接收端可能拒绝文件或读到不正确的参考位置和尺度。

Qt 使用 XML 写入器，转换米制，并显式填写各参考标志的纵向位置。依据：[原 HLL 输出](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L1451-L1477)；当前 [ship_formats.cpp](../../src/core/ship_formats.cpp)；`qt_ship_formats.py`。[格式说明](../migration/SHIP-FORMATS.md)保留原作者手册来源。已用独立读取器核对，尚未取得接收软件验收，不写成“完全兼容 Archimedes”。

<a id="u21"></a>
## 21 · PAM 米制模板中的英尺换算方向相反〔重点〕

原 `File_ExportPAM` 在英制下使用 `1 / Foot`，而 `Foot=0.3048`。例如 1 ft 应转为 0.3048 m，原式却输出约 3.28084，作为米解释时长度相差约 **10.76 倍**。这个比例是输出坐标的换算差，不是经过接收软件实测的计算误差。

Qt 按英尺到米转换，保留完整断面及明确的基线、水线参考。依据：[原 PAM 函数](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeEdit.inc#L2346)中的 `Scale := 1 / Foot`；当前 [ship_formats.cpp](../../src/core/ship_formats.cpp)；`qt_ship_formats.py`。PAM 其他历史计算参数仍需接收端核对，没有把几何模板测试说成专业求解器验收。

<a id="u22"></a>
## 22 · RVRSSHIP 主机惯性矩的单位标签冲突〔重点〕

原窗口写 **t·m²**，旧报告资源写 **kg·m²**。对已核定的 RVRSSHIP v1.08，如果用户依照 t·m² 标签填数，而程序按 kg·m² 接收，传入的主机惯量会比用户意图小 **1000 倍**。这会影响加速、制动相关计算；不能说制动时间或距离也必然相差 1000 倍，因为求解器还含其他惯性项。

Qt 已统一界面、帮助、结果与参数文件为 **kg·m²、单根桨轴基准的主机等效机械惯性矩**。旧值不一律乘 1000，避免破坏原本就按 kg·m² 输入的数据；用户须对照设备资料核对。

依据：[原窗口标签](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Forms/FreeHydrodyn_RVRSDlg.lfm#L275)、[原输入取值](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Forms/FreeHydrodyn_RVRSDlg.pas#L413-L421)；当前 [hydrodynamics.cpp](../../src/document/hydrodynamics.cpp) 和 `hydrodynamics_tables.h`。

`qt_inertia_identification.py` 对实际程序做 14 组角动量辨识：固定桨参数时，输入惯量增加 630，反算等效惯量约增加 630 kg·m²，而非 630000。核定求解器 SHA-256 为 `50b9b1c79d27fb05a12902181b0c23d5b98050c764bcfcdfb3351d16336ec2e6`。其他版本不能无条件套用；惯性附加项本身的经验精度未认证。完整依据见[惯性矩复核](../migration/INERTIA-WIND-REVIEW.md)。
