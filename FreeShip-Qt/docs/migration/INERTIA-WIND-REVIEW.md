# 惯性矩与风力系数复核（2026-11-10）

惯性矩单位已核定并修正说明。船型 13 的风力系数仍缺原表，不能报为已修好。

## RVRSSHIP 惯性矩

旧 LFM 写 t·m²，旧报告资源写 kg·m²。不能靠两处文字投票决定单位。
本次直接运行随附 RVRSSHIP v1.08，通过自由制动阶段的转矩和桨轴转速变化检查角动量平衡。
核定程序 SHA256：

`50b9b1c79d27fb05a12902181b0c23d5b98050c764bcfcdfb3351d16336ec2e6`

每个相邻输出点按 `I_eff = -1000 Q_current Δt / (Δn × 2π/60)` 反算。
Q 为 kN·m，n 为 r/min，t 为 s；排除起始切换和重复时间。取有效区间中位数，
避免原输出有限位数导致个别差分放大误差。改变输入惯性矩、桨径、桨数、效率、密度、盘面比和叶数，共 14 组。

| 桨径 m | 输入惯性矩 | 反算等效惯性矩 kg·m²（约） |
|---|---:|---:|
| 4.685 | 70 | 6055 |
| 4.685 | 700 | 6684 |
| 4.685 | 7000 | 12987 |
| 3 | 70 | 714 |
| 3 | 700 | 1344 |

同一桨径下输入增加 630，反算增加约 630，而不是 630000。
五组差分斜率均按 1±1.5% 检查，支持 **kg·m²**，排除 t·m²。
桨数从 1 改为 2 不使该惯性量翻倍，效率变化不改变它；方程输出的是桨轴转速。
因此输入按**单根桨轴基准的主机等效机械惯性矩**解释，不能把多根轴的惯量直接相加。
另一轴上给定的惯量应按 `J_ref = J × (n_source/n_propeller)²` 折算。

反算总量不等于输入量：求解器另加与桨参数有关的惯性项。
固定其他参数时该项随 D⁵ 变化，密度、盘面比也会影响它。
本次没有把这个附加项完整分解成桨体和附连水的物理公式，不能把经验项准确性称为已认证。
用户不能把包含这部分的总惯量再次全部填入主机项。

`inspect_rvrs_binary.py` 为辅助静态诊断：输入读入地址 0x44f640，
0x403200 的加法和 0x403206 的除以 1000 可复核。完整诊断在私有输出目录，
并非用重写一套反编译算法替换原求解器。单位结论主要来自上述独立力学辨识。

本次修改界面、帮助、结果报告、JSON 约定和参数文件的单位标签；不改变求解器数值。
旧 FBM 与旧参数文件的原始值仍原样读写。曾按 t·m² 标签填值的旧项目要对照设备资料重新输入，
不能给所有历史值自动乘 1000，因为很多用户可能一直按 kg·m² 填写。
新参数文件明确记录单位及轴系基准；带冲突标签的文件拒绝载入，不静默换算。
自选求解器若 SHA256 不同，报告仍提示协议未核定。

## 船型 13：0.91 与 0.091

Plus `Forms/FreeHydrodyn_Task1Dlg.pas` 的 OIIMF 四组表中，船型 13 注释为 0.091，执行为 0.91。
本地是浅克隆；最早可见快照已含冲突，不能据此确定原作者何时、为何改过。
[固定版本源码](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Forms/FreeHydrodyn_Task1Dlg.pas)。

已查到并保存 Ищейкин 2022 年论文相关选页。p.60 式 2.22 分别采用横向面积、侧面积和侧面积×船长；
p.61 起明确使用 Isherwood，pp.62–63 是按角度排列的回归表，后面是七阶正弦拟合。
这不是 FreeShip 的四组九参数表，不能拿其中系数的数量级判定 0.91 或 0.091。
[大学原文](https://www.aumsu.ru/images/pdf/disser/Attestat_dela/Isheikin/Text_diser.pdf)。

另找到 М. Б. Косой 2003 年《Методика расчёта аэродинамических характеристик надводной части корпуса транспортных судов》，
Вісник ОНМУ 11，90–106 的大学书目，但没有取得全文，不能称它是已确认的原表出处。
[大学书目](https://onu.edu.ua/pub/bank/userfiles/files/science/sci_baze/ONU_publications.pdf)。

处理：保留 0.91 的兼容结果，报告同时给出 0.091 候选的纵向力敏感性对照。
二者之差在系数层面是 `0.819 cos(theta)`；正迎风差十倍不意味着任何风向的纵向力均差十倍。
该对照沿用同一 AT 假设，不是已核定值、推荐值或误差上下界。
12–15 组的参考面积未独立核定，也在报告中说明，避免将 Isherwood 帮助误当作 OIIMF 原始定义。

**剩余条件：取得同一原始来源的四组完整系数、船型分类、分量参考面积、风向和符号约定。**
本次没有凭数量级改常数，也没有关闭这个待核定项。

## 保存的依据与测试

- `FreeShip-Tests/qt_inertia_identification.py`：统一套件 `qt-inertia-identification`，独立角动量辨识。
- `FreeShip-Tests/fixtures/rvrs-inertia`：可重复输入，不依赖旧测试输出。
- `FreeShip-Tests/qt_hydrodynamics.py`：实际求解器、参数保存往返、旧格式兼容、冲突单位拒绝、报告与导出、0/90/180°敏感性。
- `FreeShip-Tests/qt_calculation_reliability.py`：交付工具身份与惯性矩约定。
- `FreeShip-Tests/maintenance/wind-source-audit-20261110.json`：源码摘录和检索边界。
- `FreeShip-Tests/maintenance/wind-isheikin-2022-selected-pages.pdf`、同名 `.txt` 与 retrieval JSON：原文选页、来源和校验值，总量不足 1 MB。
- `FreeShip-Tests/maintenance/fetch_wind_literature_20261110.py`、`wind_source_audit_20261110.py`、`inspect_rvrs_binary.py`：保留的一次性资料/静态诊断工具，不冒充数值回归。

正式结果与安装身份以 `FreeShip-Tests/maintenance/inertia-wind-delivery-20261110.json` 为准。
隐藏窗口回调、数值检查不等于真实桌面验收；不涉及模型、设置或 GitHub 推送。
