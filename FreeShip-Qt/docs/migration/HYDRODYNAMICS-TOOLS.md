# 水动力专用工具

2026-11-04。入口在 Qt「计算 → 水动力专用工具」。沿用当前 StudioLight 计算窗口，
提供空气动力、操纵性预估、加速与制动。螺旋桨初步设计窗口增加原数据库查看器入口。

## 接入范围

`src/document/hydrodynamics.cpp` 负责参数、求解器协议和结果读取；正常调用不启动
Pascal。风力 1–11 使用原 Ishercof，12–15 使用原源码的经验系数；操纵性使用
ManeuvPP；加速、自由滑行制动和倒车制动使用 RVRSSHIP。外部求解器没有改写。

计算是只读操作。参数显式保存才修改船模，可以撤销；原 FBM 三块记录均保留，
没有位置的选项（如制动任务号和操纵性复选项）通过完整 JSON 参数文件保存。
手动输入不会被窗口打开时的自动计算覆盖。报告提供 TXT、CSV、JSON 和 PDF。
操纵性是文字报告，不伪造一条“成功”曲线。

阻力和推进资料沿用当前文档 ID、版本及完整缓存批次。编辑船模后拒用旧结果。
提取加速／制动参数需要同版本的推进结果；手动输入推进参数时只要求阻力结果。
模型提取不猜测上层建筑、桅杆或受风面积，这些资料仍须手动填写。

求解器在私有临时目录运行；支持取消、超时、异常退出、文件大小及数字校验。
Windows 进程组随服务结束而退出。执行前保留协议输入，避免旧程序删除任务文件后
失去证据。原始输出和退出日志与解析报告分开保存。

## 没有照搬的旧问题

| 原位置 | 问题与本次处理 |
|---|---|
| `Forms/FreeHydrodyn_ManeuvDlg.lfm` | 部分控件把艏吃水限制为 2 m，其他主尺度也受默认上限影响。Qt 输入范围按实际参数放开，计算时仍检查原方法有效域。 |
| `Forms/FreeHydrodyn_Task1Dlg.lfm` 的 Edit9 | 上限 11 让源码已有的 12–15 号经验公式无法从该控件选择。Qt 开放到 15，并保留方法说明。 |
| `FreeHydrodyn_Task1Dlg.pas::CalculateInWorkspace` | 艏摇力矩漏乘总长，与同项目 `FreeResourceStrings.inc::rs_Hydrodin_Task1_Note` 的归一化定义矛盾。本次补上 LOA；相对风改为矢量合成和 atan2，避免反三角函数丢失象限。两者有独立数学样本。 |
| `FreeHydrodyn_RVRSDlg.pas::CheckBox2Click` | 体积栏取了排水质量。本次提取 `volume`，不再把密度乘进去。 |
| `FreeCalculationFiles.pas` 的 `fcManeuv` | 原固定要求 193–200 行，但关闭选项后的正常报告可以只有 65、92、130 或 157 行。本次按所选分析段落检查，并拒绝 NaN/Inf。 |
| ManeuvPP 外部程序结束流程 | 报告已生成后仍等待回车。新调用只为该程序提供回车，随后正常等它退出；不靠反复休眠猜完成时间。 |
| `FreeCalculationFiles.pas::MaxResultBytes` | 通用 2 MiB 上限挡住了正常自由滑行制动输出。本次仅将 RVRS 原始数据上限设为 32 MiB，仍保留有限数字和时间顺序校验。初始轴速跳变允许同一时间的前后状态，原始点不删除。 |
| Qt 计算结果表 | 数十万行不能逐行创建界面对象。改为按需读取数据的表格模型，完整点列仍用于曲线、提示及导出，没有抽稀结果。 |

## 原版空入口和依赖

`Forms/Main.pas` 隐藏 HydrodynTask 菜单，Task2/Task3 的调用被注释；Task4 另属
附加质量与几何交接范围。本轮不将它们冒充可用计算功能，也不将它们算入三个
已接入工具。附加质量方向仍留在 Michlet/AddMass 后续任务中。

数据库按钮原来只是 `FreePropeller_Task4Dlg.pas::ToolButton17Click` 调用
`DBFview.EXE`。Qt 保留这个独立查看器入口、私有工作目录和结束回调，不访问任何
用户数据库。文件不存在时明确说明；正常关闭后重新计算初步设计，异常退出保留
旧结果。当前提供的求解器包没有 DBFview，因此真实数据库内容及外部界面仍未验收。
测试替身只验证进程和回调，不能证明数据库功能可用。

## 必须保留的边界

- 船型 13 的空气动力系数，执行代码为 0.91，注释为 0.091。本次保留执行值，
  报告提示该差异；没有足够资料替作者决定哪个经验系数正确。
- 主机惯性矩在旧 LFM 写作 t·m²，帮助与报告写作 kg·m²。本次不擅自换算求解器
  数值，窗口和报告说明歧义，真实工程使用前还需核定原求解器口径。
- 模型参数提取后的默认值不等于经过核实的实船工况；经验公式对照与程序成功退出
  也不等于工程认证。当前结果保持这些明确的验证边界。
- 隐藏 Qt 回调和截图不是实际鼠标、焦点、多屏、原生文件框或实体打印验收。
  专业窗口完整原布局的一致性仍单列复核。

## 测试与追溯

统一入口：`FreeShip-Tests/run_tests.py --suite qt-hydrodynamics --runtime <运行包>`。
源码是 `qt_hydrodynamics.py`、`qt/hydrodynamics_solver_fixture.cpp`，复用实际
`--verify-performance` 回调。按动作属性发现入口，不按菜单位置判定功能。

覆盖独立气动力数值、11 个 Ishercof 选项、MPP 选项组合、三种加速／制动任务、
独立求解器协议、原始点列、FBM 字段/重开/撤销、失效缓存、取消和错误输出。
生产窗口检查报告导出、完整表格首中尾行、参数文件和数据库按钮的错误与替身路径。

源声明、旧文案、帮助与范围元数据由
`maintenance/generate_hydrodynamics_tables.py --check` 核对；原文件指纹记录在
`src/document/hydrodynamics-origin.json`。这不是自动翻译公式。
`maintenance/probe_hydrodynamics.py` 和 `probe_rvrs.py` 保留为协议诊断：前者观察
选项对应报告和回车等待，后者观察长时间历程的文件大小与初始重复时间。
它们保留运行参数和证据，不替代正式回归。

本轮专项 279 pass、0 fail、3 review、2 not_run、0 error；另有模型发布及
型线图／DXF 对照 193 pass、0 fail、0 review、1 not_run、0 error。
当前完整证据为 `C:/FreeShip26-HydrodynamicsTests/reports-final/qt-hydrodynamics/report.json`、
`publication-01/report.json` 与安装复验 `installed-01/qt-hydrodynamics/report.json`。
安装指纹及各报告关联由 `FreeShip-Tests/maintenance/hydrodynamics-installed-20261104.json`
记录。安装复验会重复专项检查，累计数不是独立场景数。

早期报告如实保留失败原因：输入被求解器删除、MPP 等待回车、RVRS 长结果超限，
以及测试驱动误用保存返回键和数据库替身目录漏放 PropPred。修复后重新运行，
没有修改旧报告。完整最终证据和诊断源码保留，过时运行包与可重建中间数据按
本轮清理记录处理，不再持续堆积。
