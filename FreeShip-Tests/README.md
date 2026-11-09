# FreeShip UI 测试试点

## 计算组件异常退出后的恢复

`python -B run_tests.py --suite qt-backend-recovery --runtime <候选包> --output <隔离目录>`，
支持 `--verify-deployment <正式目录>`。测试代码 `qt_backend_recovery.py`、
`qt/recovery_tests.cpp` 保留真实子进程退出、回复丢失、过期/损坏/占用副本、外部文件
改写及失败后重试用例；隐藏窗口通过正式动作恢复、保存、继续编辑和撤销。
不模拟成功回复，不操作用户进程或文件，不将隐藏窗口当作实际桌面验收。

## Qt 工程设置收尾

`python -B run_tests.py --suite qt-project-completion --runtime <Qt运行目录>`
验证创建者、显式船底对齐、锁定整笔拒绝、撤销保存、45 个格式版本、静水力不变性、
水温换算、原求解器黏度输入与完整工况保存。沿用现有运行包，输出默认按可用空间选盘。
`qt-dialog-layout` 另有温度表端点、插值和无效值测试；实际桌面验收仍单列。
编译器短路径丢失时，`maintenance/restore_qt_toolchain_alias.ps1` 可从保留的工具链恢复，
只处理已知联接或不存在的目录；这是环境维护脚本，不是产品回归。

## Qt 静水力附加报表

`python -B run_tests.py --suite qt-hydro-auxiliary --runtime ../FreeShip-Qt/dist`

`qt_hydro_auxiliary.py` 检查 45 个格式版本、精确保存与撤销、矩形船体截面积和重量积分、
船底坐标平移、公英制转换、旧表对照，以及实际工程设置、附加表保存按钮和取消流程。
默认包含已有原生静水力回归；脚本的 `--focused` 仅用于排错，不能当作完整交付证据。
测试输入、输出和设置隔离保存。桌面操作及原生文件选择对话框仍未覆盖。

## Qt 水下不透明度

`python -B run_tests.py --suite qt-underwater --runtime ../FreeShip-Qt/dist`

`qt_underwater.py` 和 `qt/underwater_tests.cpp` 检查 45 个格式版本、精确字节保存、旧格式升级与撤销、
非法和过期请求、实际 Qt 工程设置动作、保存后预览，以及 CPU/GPU 四视图的独立 RGB 预期。
水下颜色混合比例与图层透明度分开核对，型线图、DXF 和几何输出要求不变。
实际桌面鼠标、多屏和焦点未覆盖；生成的 PNG 用于检查绘制结果。

## Qt 工程设置静水力选项

`python -B run_tests.py --suite qt-project-hydro --runtime ../FreeShip-Qt/dist`

`qt_project_hydro.py` 复用独立 FBM 夹具与原生编辑探针，检查 45 个版本的精确字节改动、
升级与撤销、矩形船体的系数公式、逐项标注、旧版读取，以及实际工程设置菜单的取消、确认和保存。
`qt-dialog-layout` 同时检查新增控件的可达性和组合提交。物理桌面验收单列。
安装后的路径复核由 `maintenance/verify_project_hydro_installed.py` 调用同一套正式操作流程，
复用已通过的格式和计算回归；`maintenance/record_project_hydro_delivery.py` 核对文件与报告指纹。

## Qt 文件内嵌预览

`python -B run_tests.py --suite qt-preview-image --runtime ../FreeShip-Qt/dist`

`qt_preview_image.py`、`fbm_preview_bytes.py`、`qt/preview_image_tests.cpp` 与
`qt/fbm_codec_tests.cpp` 覆盖 45 个格式版本、独立字节边界、JPEG 解码、保存确认与失败重试，
以及真实 Qt 保存动作、备份、重启、预览开关、新建和撤销重做。生成后交给旧版读取器打开。
保留 JPEG/PNG、各阶段耗时及场景/DXF 对照；不代表原生文件浏览器或硬件断电验收。
设置控件的布局和操作另由 `qt-dialog-layout` 覆盖。安装复核脚本：
`maintenance/record_preview_delivery.py`，只读取证据，不修改被测文件。

## Qt 曲面结果编码

`python -B run_tests.py --suite qt-surface-serialization --runtime ../FreeShip-Qt/dist`

源码 `qt_surface_serialization.py`：用更新前运行包逐字节对照四份完整产物，覆盖原模型、
控制曲线、非空流线、四档精度、法线和空模型。实际 9a 船模交替运行新旧程序三次，
报告后台计时中位数。固定参照默认在 `FreeShip-Qt/build/dist-before-surface-serialization-20261105`；
也可直接运行脚本并传 `--oracle-runtime <旧运行包>`。相同程序不算独立参照。
输入、原程序指纹和比较结果保留；不按当前输出自动改期望，不代表桌面验收。

## Qt 型线图缓存与局部求交

`python -B run_tests.py --suite qt-lines-cache --runtime ../FreeShip-Qt/dist`

源码 `qt_lines_cache.py`，覆盖真实 Qt 跨进程重开、文档编辑／撤销／重做、控制点与
图层／剖面／精度等输入失效，以及缓存损坏和不可写。缓存与禁用缓存的完整生成逐字节
比较场景、DXF、剖面和曲面，不降低精度。输出中保留分阶段计时和实际局部命中数。
其局部范围是剖面求交，不能等同于细分、填色也已增量化；桌面输入单列未测。

`maintenance/probe_lines_cache.py --runtime <运行包> --input <捕获的native-input.json>
--output <新目录>` 是保留的性能诊断：比较冷生成、重启命中、局部修改和禁用缓存，
记录输入及工作程序指纹。它依赖指定捕获输入，不作为通用通过标准。
`maintenance/record_lines_cache_delivery.py` 核对安装包和报告，用于交付审计，不自动安装或清理。

## Qt 保存保护

`python -B run_tests.py --suite qt-save-protection --runtime ../FreeShip-Qt/dist`

源码为 `qt_save_protection.py`、`qt/save_protection_tests.cpp`；统一入口按实际 C/D 余量
选择输出位置。覆盖上一版 `.Bak`、无变化保存、两会话冲突、另存为、实际 Windows 文件
占用失败及备份回退。还会在文件提交后、确认前终止测试自己创建且已核对路径的文档服务，
检查已写入文件与恢复副本。生产保存动作、撤销／重做和 `.Bak` 重开复用真实 Qt 回调，
保存前后场景和 DXF 逐字节核对。所有输入均使用私有副本。
原生桌面、文件选择窗口和硬件断电单列为未测；外部不遵守锁的写入仍可能争用最后一次替换。

`maintenance/record_save_protection_delivery.py --save <专项报告> --recovery <恢复报告>
--startup <安装启动报告> --output <回执>` 只读核对源码、安装文件和报告指纹，记录交付证据，
不编译、不安装，也不清理历史结果。

## Qt 折线与型值建模导入

`python -B run_tests.py --suite qt-curve-import --verify-deployment ../FreeShip-Qt/dist`

源码在 `qt_curve_import.py`，复用 `qt/exchange_tests.cpp` 和实际 Qt 导入回调。
独立合成 Hull、Carene、折角线/曲面文本；用 60 位十进制稠密消元校验 C++ 的三对角
拟合结果，检查点序、面朝向、内部平滑与纵向折痕、输入单位、Hull 站位、原始标记和
控制曲线。附属字段注入失败须保持活动文档、场景及 DXF；成功后检查编辑/撤销、
保存重开及完整派生曲面。四个实际窗口分别执行参数提交与保存回调。
外部 Hull/Carene 文件、真实桌面和完整旧窗口视觉一致性单列，不拿合成样本代替。

## Qt Polycad 导入

`python -B run_tests.py --suite qt-polycad --verify-deployment ../FreeShip-Qt/dist`

源码是 `qt_polycad.py`，复用模型交接探针和真实 Qt 回调。独立网格样本检查四种段落、
坐标顺序、曲面连接、主尺度、单位、编码与坏文件；后续验证编辑、撤销、保存重开，
以及导入失败不改变原模型、型线图和 DXF。旧格式回归使用 `qt-exchange`。
测试对象是控制网转换，不能把它等同于原始 B 样条曲面；外部 Polycad 和桌面未测项单列。
显式 `--runtime` 可复用已有包；没有输出路径时自动选择余量较多的盘，并清理本轮临时缓存。

## Qt 水动力专用工具

```powershell
python -B run_tests.py --suite qt-hydrodynamics --runtime <运行包> --output <独立报告目录>
```

源码是 `qt_hydrodynamics.py` 和 `qt/hydrodynamics_solver_fixture.cpp`；实际菜单与
结果窗口复用产品 `--verify-performance` 回调，按动作属性发现入口。
独立气动力数学样本、全部 11 个 Ishercof 类型、MPP 选项组合、三种加速／制动任务，
分别检查原始协议、完整点列、无 Pascal 调用、保存／撤销／重开、失效缓存及取消。
同一套测试还执行旧 Delft/Holtrop 窗口，核对共享结果表的首、中、尾行及导出。
原求解器路径取已有 `fixtures/solvers35/unpacked/Freeship/Exec`，不写用户数据库。
DBFview 缺失路径与进程替身回调单独记录，真实查看器仍为 `not_run`；系数、单位
歧义和完整视觉一致性为 `review`。隐藏窗口不代替桌面验收。
独立脚本的 `--service-only` 只测后端，不能据此声称 Qt 窗口通过。

`maintenance/generate_hydrodynamics_tables.py --check` 核对原字段、帮助和来源指纹。
一次性诊断也保留源码：

- `maintenance/probe_hydrodynamics.py --input <已捕获INM> --solver <ManeuvPP.EXE> --output <新目录>`：观察选项组合的报告段落、行数及回车等待。
- `maintenance/probe_rvrs.py --inputs <已捕获协议目录> --solver <RVRSSHIP.EXE> --output <新ASCII目录>`：记录结果大小、初始时间及尾部数据。

两者依赖指定诊断输入，不是通用通过标准；相应可复用行为已进入专项套件。
`maintenance/record_hydrodynamics_delivery.py` 核对安装与报告指纹，
`maintenance/prune_hydrodynamics_intermediates.ps1` 仅在安装验证后清理本轮过时输出；
它们属于显式交付维护，不会在日常测试中自动替换或清理程序。

## 非对称静水力

`python -B run_tests.py --suite qt-asymmetric-hydro --runtime <运行包> --output <独立报告目录>`。
可用 `--verify-deployment <安装目录>` 从正式程序复验。实现测试在 `qt_asymmetric_hydro.py`。
独立箱体、棱柱、多轮廓及孔洞积分，横纵倾、公英制、非平面面片的面积—体积一致性、
朝向/开口拒绝、缓存与撤销、只读保存、Qt 报告和漂心 Y；旧对称路径另由
`qt-hydro-auxiliary` 的完整现有回归核对。测试不把未提供的侧投影面积或桌面体验算作通过。

## Qt 偏好设置

`python -B run_tests.py --suite qt-preferences-ui --runtime <已构建运行包> --output <独立报告目录>`
或用 `--verify-deployment <实际安装目录>` 直接验证安装。源码在 `qt_preferences_ui.py`
和产品内的 `preferences_workflow.cpp`；记录真实偏好动作、字段、三页截图、状态及文件变化。
覆盖取消、保存、重启、并发冲突、失败回退、主题与颜色、目录、撤销历史及几何/FBM/DXF 保持。
不依赖桌面操作工具，不据隐藏回调声称桌面鼠标或视觉验收通过。语言支持不在该任务范围。

2026-11-09：`qt_preferences_import.py` 已并入该套件，独立入口为
`python -B run_tests.py --suite qt-preferences-import --runtime <运行包> --output <独立报告目录>`。
通过实际偏好动作和文件选择回调，覆盖 INI 全局/用户继承、首个重复键、主题一层继承、BGR 颜色、
DTA 字段次序及可选尾部、目录失效、编码/数值异常、UTF-8/NUL/超限/截断文件、草稿取消、重启独立性。
源配置哈希和当前 FBM 字节不变；所有几何、非显示设置及 DXF 非颜色字段精确比较。
水下填色透明度在快照 `settings` 内属于显示字段，其指定变更独立校验，不混入几何不变断言。
探针的 `legacy_imports` 接受路径数组，null 表示取消选择器；记录每次导入后的草稿、报告和落盘状态。
候选最终报告在 `C:/Users/12968/FreeShip26-TestResults/preferences-import-20261109/`；
交付与安装复验记录见 `maintenance/preferences-import-delivery-20261109.json`。

## Qt CAD 输出

统一入口：`python -B run_tests.py --suite qt-cad`。
测试保存在 `qt_cad.py`、`qt_native_cad.py`、`cad_readers.py`、`cad_occt_oracle.py` 及已有 C++ 格式探针。
固定平面、自然三次曲线、混合图层、中心面/近中心面、负舷、单位、错误曲面数据、
取消、版本、模型不变和真实菜单均有检查。独立 IGES 解析器要求数字不跨记录，
不是任意拼接文本后只看能否读回；DXF 由 ezdxf 读取并审计。

```powershell
python -B run_tests.py --suite qt-cad --reuse-build D:/FreeShip26Tests/build/cad-native-bridge-01 `
  --runtime D:/FreeShip26Tests/build/cad-native-package-03 `
  --output D:/FreeShip26Tests/reports/<新目录>
```

`--verify-deployment <运行目录>` 可直接核验并运行实际安装包。
可选 `--occt-python <python.exe>` 指定装有 cadquery-ocp 的独立 Python 环境；
本机为 `C:/FreeShip26Tests/tools/cad-oracle/Scripts/python.exe`，
cadquery-ocp 7.8.1.1.post1、vtk 9.3.1；DXF 读回使用 ezdxf 1.4.4。
缺少可选 OCP 环境会记 `not_run`，不能算外部内核验收通过。
它核对每个曲面实体是否读入、曲面有效性、已知平面的面积和边界；
不是商业 CAD 桌面流程或船舶工程精度认证。
2026-11-03：曲面准备也已迁入 C++。`qt_native_cad.py` 保留独立 FEF 样例生成器，
逐值比较旧 NURBS 控制点和节点，检查解析平面面积、孔洞、三角/五边形、隐藏图层与
跨图层归属、编辑/撤销/重做、无 Pascal 适配器计算、原子写失败及恢复。
Qt 工作流还复现“结果已生成但界面尚未处理时取消”，确保不继续写出文件。
完整报告 `D:/FreeShip26Tests/reports/cad-native-core-01/qt-cad/report.json`：
562 pass、0 fail、1 review、1 not_run、0 error。旧报告 `qt-cad-c` 保留原范围。
隐藏窗口不代替真实鼠标/焦点/文件框和完整 UI 保真验收。
本轮存储与交付工具 `maintenance/promote_cad.ps1`、
`maintenance/record_cad_delivery.py` 也保留源码：前者用于一次本机运行包替换，
后者核对安装清单及报告并记录指纹；它们不是应用回归，不在日常测试中自动执行。
本次原生迁移复用 `maintenance/promote_runtime.ps1`，由
`maintenance/record_cad_native_delivery.py` 核对安装包、对照报告和启动证据。
两者是显式运行的交付维护工具，不在回归中自动替换用户程序。

## 统一入口

本机测试输出候选位置为 `D:/FreeShip26Tests/reports` 和 `C:/FreeShip26Tests/reports`，由 `output-location.json` 配置。
2026-11-04 起，不带 `--output` 时选择当前余量更多的盘，并创建新报告目录；显式路径仍优先。测试源码留在本项目。
历史报告通过目录联接保留，旧报告中的路径仍可访问。
盘符不是固定偏好。`storage-location.json` 记录本次选择及当时余量；默认候选均不足 1 GiB 时停止创建大批输出，显式 `--output` 不受该默认选择限制。
历史示例中的盘符表示当时的输出位置，不是永久要求。

统一入口在子测试全部退出后清理本轮 `native-lines-cache` 的已知格式文件，保留正式发布的
模型、型线图、DXF、截图、日志和报告；`artifact-retention.json` 单独记录清理，不改测试结论。
只处理统一入口自己创建并标记的新目录；遇到目录联接或未知缓存内容就保留。
需要继续研究缓存时使用 `--keep-transient-cache`。独立脚本不自动清理历史结果。
实现和回归为 `test_artifacts.py`、`test_storage.py`，统一入口 `--suite test-storage`。

2026-11-04 清理了 128554 个过时编译文件和临时缓存，逻辑大小 34.72 GiB，实际盘上
余量增加约 20.7 GiB。没有删除原始源码、样本、报告、截图或正式模型/场景/导出证据。
旧构建目录的 `RETIRED-BINARIES.txt` 标明二进制已退役，不能再把那些目录作为完整运行包；
当前安装、最近回退包、试点基准、当前编译目录和桥接构建保留。
清单和复核见 `maintenance/storage-cleanup-20261104.json`、对应 `.result.json` 和
`maintenance/storage-cleanup-verified-20261104.json`。`storage_inventory.py`、
`plan_storage_cleanup.py`、`apply_storage_cleanup.ps1`、`record_storage_cleanup.py` 是显式维护工具，
不在日常测试中清理旧目录或替换软件。

窗口打开准备计时：`python -B dialog_opening.py --model <模型.fbm> --output reports/opening-new`。已接入统一入口 `--suite dialog-opening`（默认测试箱体）。隔离编译后，以创建隐藏原生控件并取消代替模态显示；记录型线图请求、完整准备、工程设置创建/重复打开及剖面线准备耗时。检查取消草稿不泄漏、重新绑定当前项目、排队时重复窗口识别、关闭与模型销毁取消回调。原源码、测试依赖及模型哈希在运行后复核。计时为 review，真实点击到可见为 not_run；不能把隐藏准备时间称作实际屏幕显示延迟。

```powershell
python -B run_tests.py --list
python -B run_tests.py
python -B run_tests.py --suite fixes --suite historical
```

统一入口串行调度现有修复、界面、型线图性能、窗口、历史回归及历史图标资源检查，避免同时运行测时程序互相干扰。每组保留自己的报告、日志与覆盖限制；汇总不把 partial/review/not_run 变成 pass。各独立入口仍保留，用于指定模型、外部求解器或诊断参数。历史脚本的维护副本、替代入口和退役原因见[逐项处理清单](历史脚本处理清单.md)。工具链、用户设置隔离和桌面操作限制仍遵循各入口规定。

## 上游问题修复回归（第一至三批）

```powershell
python improvement_regression.py --output reports/improvements-recheck
python improvement_regression.py --group units --group stl --output reports/units-stl-recheck
python improvement_regression.py --model "D:\模型\船型.fbm" --output reports/improvements-model-recheck
python improvement_regression.py --solvers "D:\历史FreeShip\Exec" --output reports/improvements-with-solvers
```

总入口默认并行运行 3 组（`--jobs 1` 可串行），把各组原始报告保留在子目录。历史样本失败、未做的桌面验收和基础设施错误均保留原状态，不因其他检查通过而改成通过。`--model` 只读复制给加载及历史兼容性检查。输出必须是产品源码目录外的新目录。所有入口支持 `--source`、`--lazarus`、`--output`；执行期间产品源码或测试代码变化，整批作废并标为 `error`。

| 独立入口 | 检查范围 |
| --- | --- |
| `buffer_safety.py`、`save_safety.py`、`file_safety.py` | 读写进度、保存失败、加载事务、损坏索引、重建恢复；具体见下一节 |
| `units_conversion.py` | 公英制密度/质量、带符号交线及计算输入的物理量一致性、往返保存；Kaper/Planing 手工排水量的单位转换及真实窗体参数 |
| `hydro_corrections.py` | 剖面定位、吃水序列、真实计算/报告回调和工作目录写入隔离 |
| `hydro_search.py` | 真实细分多峰/窄峰与密集参考扫描、求值次数和耗时；总入口有多个 `--model` 时取第一个作代表模型 |
| `undo_background_safety.py` | 撤销后取消操作、Lackenby 最终提交、JPEG/BMP 及损坏图保留原状态；含实际背景菜单方法 |
| `process_safety.py` | 真实子进程的启动、退出码、输出洪泛、超时、取消接口及查看器生命周期 |
| `process_callers.py` | 从实际计算方法注入进程故障，验证停止读取结果和工作目录/光标恢复 |
| `calculation_contracts.py` | 22 种外部结果格式，独立目录、失败失效、跨任务传递和成对输出；`--solvers` 可增加六个真实程序的逐字节对照 |
| `stl_symmetry.py` | 对称与非对称、混合图层、中心面、隐藏层的导出几何及方向 |
| `plate_errors.py` | 展开面积、候选评分的量纲与缩尺一致性、误差显示单位 |
| `legacy_compatibility.py --all-ships` | 随附 FBM/FTM 原生加载、字段偏移、计算字段原字节比对和保存往返；无效旧 Task5 段明确报告清空 |

故意恢复旧错误只在临时副本进行，预期结果是 `fail`：`units_conversion.py --mutation old-density` / `--mutation fixed-planes` / `--mutation old-resistance-mass`；`stl_symmetry.py --mutation unconditional-mirror`；`plate_errors.py --mutation legacy-area` / `--mutation legacy-score`；`undo_background_safety.py --mutation eager-redo` / `--mutation freed-jpeg` / `--mutation early-clear-menu`。这些负控用于确认检查确实能发现错误，不能作为发布通过的报告。

单位转换的阻力参数检查会执行真实命令到窗体初始化的绑定，在计算和显示窗口之前采集手工输入；没有验证 Kaper/Planing 阻力算法。另检查普通非等比缩放仍使质量按几何体积比例改变，避免把几何缩放误改为单位转换。

`legacy_compatibility.py --diagnostic` 允许开发过程中快照不同，强制标为 `review`，不能作最终验收。兼容性报告区分加载失败和加载后往返差异；JPEG 压缩字节差异需要结合参数及像素证据解读。历史布局不能仅凭文件短了几字节就补零接受。当前四种迁移规则同时核对版本、整段长度、字段数值和布尔位置；原有有效字段与原文件独立比对，新增字段为零，损坏 Task5 的清空有明确预期。旧格式无校验和，某些恰好在字段边界的截断仍可能与短布局无法区分，测试通过不表示所有旧文件都可恢复。

`calculation_contracts.py` 默认运行 118 项格式与隔离检查；未提供 `--solvers` 时，真实程序对照为 `not_run`。提供目录后，用 `solver_protocol_samples.json` 中的输入分别直接运行六个程序、再经原生包装器运行，比较原始结果字节，合计 124 项。输入是为协议回归准备的合成样本，不是物理精度的标准答案。程序与来源哈希见 [fixtures/README.md](fixtures/README.md)，运行时会继续记录所用二进制哈希。可变长旧协议若没有总记录数或校验和，无法识别恰好停在合法行边界的所有截断。

第三批最终证据在 `reports/remaining-final-20261030-2/report.json`：13 组共 1403 pass、3 review、3 not_run，无 fail/error，总状态 partial。兼容性组 82 个模型全部可读取，两个背景 JPEG 重存差异保留 review；其余 review 是静水力搜索的全局极值保证，not_run 是桌面交互/显示验收。前一次总回归因测试入口传递相对路径导致真实程序组 error，保留在 `remaining-final-20261030-1`；修复入口后重跑全套，没有把失败记录覆盖成通过。

当前旧布局诊断另有两个入口：`legacy_layout_diagnostic.py --report <兼容性report.json> --output <新json>` 只读已有偏移和原样本，汇总候选布局；`legacy_settings_diagnostic.py --model <fbm> --output <新目录>` 隔离加载并拆分各段设置、比较 JPEG 参数和像素（需 Pillow，无此依赖时像素检查为 `not_run`）。两者用于排查，固定 `review`，不是通用格式判定器。

以上运行隔离构建的真实 Pascal 方法，未操作桌面，也没有验证所有船型、所有外部计算程序、打印机或加工流程。报告中的每个 `pass` 只适用于该项记录的样本与预期。


## 文件安全回归（2026-10-30）

这组入口使用当前源码的隔离副本，测试用户文件只读复制。不会操作桌面，也不会覆盖正式程序。

```powershell
python -B buffer_safety.py --output reports/buffer-safety-新目录
python -B save_safety.py --output reports/save-safety-新目录
python -B file_safety.py --output reports/file-safety-新目录
```

- `buffer_safety.py`：真实文件缓冲单元与故障流；短读写、零进度、异常返回、句柄释放，FBM/FTM 字段及 JPEG 长度边界。
- `save_safety.py`：实际 `File_Save` / `File_SaveAs`；在隔离副本注入序列化、部分写入、刷新、备份、替换失败，另测真实 Windows 文件占用和 FBM/FTM 往返。文件选择、提示框由测试替代，业务保存流程保持原样。
- `file_safety.py`：实际 `LoadProject` / `File_Load`；不存在、错误标识、未知版本、多处截断和重建失败，检查原对象、选择、文件状态、序列化及可执行的撤销/重做。正常加载另外检查拓扑和项目数据、事件订阅及点编辑器旧引用。Surface 子探针检查加载标志的恢复和回调抑制。
- 同一入口默认还运行 `GeometrySafetyProbe.pas`：直接调用边、面、曲线、点组和名称/锚点/约束的实际读取方法，检查非法索引和数量；验证图层 `-1` 的既有默认层语义、匿名与命名点组后接锚点的 FBM/FTM 往返，以及真实细分回调抛错后同一个曲面可以重新构建。`FasterList` 的合法读取和越界异常也在此组。`--case geometry` 可只运行这一组；`--case load`、`--case surface` 可分别运行加载或加载状态检查，参数可以重复。

每个入口输出 `report.json` 和编译/执行证据；`--output` 必须是尚不存在、且位于产品源码之外的目录。报告会核对源码、测试和原样本是否变化；发生变化不能当有效通过。

生成普通 GUI 候选程序（不安装、不启动）：

```powershell
python -B file_safety.py --build-release-only --output reports/file-safety-release-新目录
```

这组检查不覆盖断电、网络盘、非 Windows 文件替换及真实菜单/鼠标验收。损坏模型检查目前还没有穷尽全部索引、旧版本和数值字段；不能据此宣称问题 04 已整体解决。加载失败保持原项目时不重建旧对象，因此前后序列化应逐字节相等；重新加载成功的模型会按内存地址重新排序控点，保存文件字节可不同，需比较几何、连接和项目数据。


## 翻译语义上下文与详情名称回归

`python translation_context_check.py --context-root ../TranslationContext_20261030` 检查证据优先语料的 ID/覆盖、源文件与函数正文哈希、菜单到目标实现的已知回归链，以及伪造证据不能通过翻译闸门。它验证资料完整性，不把源码关联或文件解析通过说成全量语义正确。

`python freeship_ui_test.py --case language` 还通过事件绑定发现已选对象详情入口，使用隐藏真实窗体检查已选点数据，独立核对用户明确指定的中文名“已选对象详情”和经功能审查修订的英文名“Selected Object Details”，并采集七种语言的菜单入口/窗口标题。其他语种只记录实际译文，不冒充母语专业验收。该已知功能适配器不依赖菜单位置或列表列号，不调用 Show/ShowModal，不操作真实桌面。

这是给 AI 使用的 Windows 命令行测试工具。它针对同级 `FreeShip-Plus-Lazarus` 当前工作区里的源码，在临时目录构建带测试探针的副本，检查控点、菜单、工具栏、图层、快捷键、绘图标签和保存后的模型数据。它不会修改 FreeShip 源码、原有程序、模型或配置，也不使用桌面操作工具。

## 运行

在本目录运行：

```powershell
python freeship_ui_test.py
```

可以选择一组，或缩小到一个视图和坐标轴：

```powershell
python freeship_ui_test.py --case panel
python freeship_ui_test.py --case inventory
python freeship_ui_test.py --case coordinates --view profile --axis x
python freeship_ui_test.py --case toolbar
python freeship_ui_test.py --case layers
python freeship_ui_test.py --case chinese
python freeship_ui_test.py --case display
python freeship_ui_test.py --case language
python freeship_ui_test.py --case shortcuts
python freeship_ui_test.py --case labels
python freeship_ui_test.py --case models
python freeship_ui_test.py --case persistence
python freeship_ui_test.py --case history
python freeship_ui_test.py --case performance --model 'D:\模型\船型.fbm' --repetitions 3
```

`--case` 可选 `all`、`inventory`、`panel`、`coordinates`、`toolbar`、`layers`、`chinese`、`display`、`language`、`shortcuts`、`labels`、`persistence`、`history`、`performance`、`models`。默认 `all` 包含常规功能组，不包含单独运行的性能和多模型测试。`--view` 可选 `all`、`perspective`（透视）、`profile`（侧视）、`plan`（半宽）、`bodyplan`（横剖）；`--axis` 可选 `all`、`off`、`x`、`y`、`z`。选择单个坐标轴时，也会运行关闭标签的基线，用来对比图像。报告的 `selection` 标明本次范围，未选中的视图或功能没有执行，不代表它们通过。每次仍会隔离编译当前源码，因此选择单项不一定显著减少构建时间。

`models` 使用 FreeShip 源码自带的 board、Ferries+deck、Containership 三个固定样本，样本名称与 SHA-256 固定在测试代码中，原文件变化时拒绝运行。它们被复制到临时目录，各自启动隔离进程。测试四种视图里可见控制点的 X 标签请求和实际绘制；被布局器省略的标签列为待复核。为了限制运行时间，超过 300 个控制点的模型只检查四视图离屏绘制和非空图像，坐标标签明确标为未运行。单个模型无法加载时，其他样本的结果仍会保留。

性能测试不提供 `--model` 时会分别测试普通路径和中文、俄文路径下的箱体副本；提供模型时，还会测试该模型的隔离副本。每种模型默认启动三个独立测试进程，`--repetitions` 允许 1 到 10 次。原始文件在复制前后及测试后会校验哈希。测量记录程序内部读取、解析、重建、视图创建和总耗时，以及独立进程的墙钟时间；系统文件缓存不会被清理，因此首轮不能称为严格冷启动，也未覆盖打开文件对话框和真实窗口响应。所有结果只是观察值，未设置性能合格线。

默认使用同级 `FreeShip-MCP/.toolchain/lazarus` 的 Lazarus 3.2 / FPC 3.2.2 和 `FreeShip-MCP/examples/box.fbm` 样本。需要 Python 3.11；Pillow 用于检查绘图像素变化和面板图像是否为空，缺失时相关检查显示 `not_run`。可通过 `--source`、`--lazarus`、`--output`、`--timeout` 调整路径和超时。输出目录必须不存在，并且位于 FreeShip 源码目录之外。报告默认写入 `reports/<时间>/`。

```powershell
python -m unittest discover -s tests -v
```

上面的自检包含独立的结果分析检查，并会在临时源码副本中故意改错 X/Y/Z 坐标标签、增加轴名前缀、改变控制网格开关、图层开关、中文标签、视图切换、批量图层开关、语言输入保护和快捷键注册，确认测试能抓到这些回归，不会修改你的 FreeShip。完整自检需要可用的 Lazarus 工具链。

## 如何读报告

`report.json` 是机器可读的主要输出，包含源码快照、测试代码、编译器和测试可执行文件的哈希，以及每项测试的预期、实际结果、绘图文字位置和面板控件数据。`build.log`、`runtime.log` 和图像为证据。`<视图>-axis-*.bmp` 是实际船体绘图方法的离屏输出。`panel.bmp` 是隐藏原生窗口控件逐个绘入位图的部分结果；`panel-labels.bmp` 单独记录 LCL 绘制型控件，不构成完整窗口截图。

`issues.json` 是供 AI 快速定位的精简索引，列出失败、待复核、未运行和错误项，包含源码哈希、预期、实际摘要和图片名；详细追踪数据应回到 `report.json`。若总状态为 `error`，索引中的个别结果不能作为有效验收。

`inventory.json` 列出无模型、加载模型、选中控点和中俄语言状态下的已构造菜单、动作、按钮和组合框条目，并给出各状态之间的客观差异。包含动作绑定、快捷键、启用状态、局部和父级可见状态；窗口隐藏，因此实际屏幕可见性不可由此确定。静态快照指出带动态回调的菜单或组合框；另一个隔离进程生成 `dynamic_inventory.json`，对经过源码哈希核准的回调直接调用并记录前后内容，未核准的回调记录为 `not_run`。未列出不代表不存在。入口归属和措辞由 AI 结合当前需求判断；旧版固定位置与措辞断言已移除，尚存的历史设计观察标为 `review`。完整历史审计和后续限制见 [测试原则与历史检查审计.md](测试原则与历史检查审计.md)。

坐标点报告将三个问题分开：样本点请求的数值或身份错误、标为可见的文字没有绘制调用，会作为行为失败；布局器主动省略的点记录原因和位置并标为 `review`，是否应当显示需结合用户要求判断。逐点标签使用模型点身份与位置匹配绘制记录，其他地方出现相同数字不能代替该点的绘制证据。动画未稳定或追踪缺失时，对无法确定的结果标为 `not_run`。

对部分边缘省略，`placement_trace` 的 `layout_debug` 会记录布局器选中的目标坐标、边缘调整后的最终坐标、调整量及是否存在历史位置；这些数据用于排查，不能替代屏幕显示和可读性验证。此诊断只注入隔离源码副本，旧版布局器不支持时可能没有该字段。

`clip_rect` 是布局器传给文字绘制的裁切区域，不是实际字形的像素掩码。重叠与线段相交优先依据这一范围；缺少裁切数据的历史报告仍使用完整文字框，只能作为保守提示。隐藏面板控件的 `clip_rect` 是按父级客户区域估计的可见区域；`panel.ancestor-clipping` 列出可能被父容器截去的控件，真实窗口仍需进一步验证。标签布局在离屏进程中做有限次重绘，动画仍未稳定时相关结论为 `not_run`。

标签测试的 `placement_trace` 包含实际提交布局的类别、文字、锚点、矩形和是否绘制，`text_trace` 是实际文字绘制调用，`leader_trace` 是引线坐标，`segment_trace` 是部分实际线段调用。文字框与这些线段相交会列为候选问题并要求复核；其他图形绘制方式并未全部记录，不能据此证明没有遮挡。隔离副本会记录布局器实际退出分支，例如锚点超出范围、无可用候选位置或背景代价过高；“无可用候选”仍不能单独区分边界与碰撞的具体贡献。图片和记录只对应报告中的源码哈希；复制期间或测试期间源码、测试代码或样本发生变化时，总状态为 `error`，不把该次结果当成当前源码的验收结论。

`panel-layout-map.png` 和 `panel-content-map.png` 根据实际控件几何、文字和数值重建，仅用于定位控件和阅读数据，不是 FreeShip 的真实截图。真实面板的可读性、视觉外观和最终窗口布局仍为 `not_run`。

结果状态：

- `pass`：该项的明确断言通过。
- `fail`：该项明确断言失败。
- `review`：发现需要结合需求和图片判断的情况，例如标签边界超出画布。
- `not_run`：该项没有得到足够证据。
- `error`：编译、执行或测试基础设施出错。

总状态为 `partial` 表示有 `review` 或 `not_run`，并非整体通过。进程退出码：0=全部通过；1=断言失败；2=部分完成或需复核；3=基础设施错误。实际在 PowerShell 中可用 `$LASTEXITCODE` 查看。

## 当前范围和限制

- 样本是已知 8 个控制点的合成箱体，测试四种视图中四个菜单项的实际回调与勾选状态，逐点核对 X/Y/Z 的三位小数纯数值、关闭模式下的 P5 名称，以及四个视图互不影响；还检查隐藏控制网格、隐藏图层网格和选中点的行为。指定点的面板绑定、切换和清除绑定也有检查。
- 快捷键测试从当前菜单和动作绑定读取组合键，在 LCL 应用回调或视图事件中模拟，观察语言切换和选中状态；F24 消息另行验证内部消息路径。不会验证物理按键、真实焦点或模态框。
- 标签测试在箱体上添加交线，离屏绘制中俄两种语言、两种尺寸的船体侧视/半宽/横剖和型线图，记录控点、网格文字、基线、设计水线、静水力和剖面积文字的实际绘制与省略情况。它检查文字矩形、重叠、部分线段与文字的相交以及重要线名存在性；对一般省略或线段相交标为 `review`，关联关系、可读性和真实显示仍未验收。部分剖面积锚点可能本来就在画布外，不应自动归因于布局故障。
- 保存测试在隔离箱体中改动一个点的坐标、名称和第一图层颜色，运行保存动作并重新读取检查；这不代表已经测试真实编辑框输入、另存为对话框或所有模型版本。为避免干扰桌面，测试副本中加载、保存和快捷语言切换的失败提示被转为错误，静水力和型线图离屏绘制的忙碌光标被禁用。
- 撤销/重做测试把一个控点的坐标和图层颜色作为同一次修改，调用动作并检查撤销及重做后两项数据；它不模拟 Ctrl+R 的真实键盘或窗口焦点。
- 工具栏行为按动作处理器及控件绑定发现，动态库存记录缩窄状态和溢出菜单。原生屏幕布局和鼠标操作仍未覆盖。
- 图层表格按回调发现，逐格扫描对象引用，并观察每个复选列改变的属性；随后检查独立图层的切换、恢复、批量开关及排序后的目标。报告 `layer_discovery` 保存列效果和发现依据。无法唯一识别时标为 `not_run`。
- 中文和俄文测试记录相应语言下的库存；切换时检查通过事件发现的编辑器草稿与模型绑定是否保留，不对界面文字位置设固定预期。
- 视图显示检查四个视图切换、五种显示模式的动作与勾选状态，检查另一个视图不受影响；在无交线和添加横剖线、水线、纵剖线、斜剖线的状态下检查显示开关及其勾选与启用状态。还检查控制网格与加点的启用关系，以及控制点选择后取消选择的状态。
- 语言切换在隐藏控件中运行，不验证真实焦点或键盘输入，也不保存用户语言设置。
- 测试使用从当前源码单独构建、加入文字记录和测试入口的程序。它不是普通发布程序本身；调用表单与绘制方法也不等于走完正常启动和系统事件循环。
- 加载后的渐进预览由测试直接调用程序内部的阶段切换处理器，再检查最终画面；真实计时器和用户实际看到的过渡效果仍为 `not_run`。
- `<视图>-axis-*.bmp` 是 800×500 的离屏视图，不是完整窗口截图。文字调用与像素变化不能单独证明文字可读，裁切和重叠提示可能受测试视角和边界影响，应查看图像。标签未绘出时，依赖它的像素比较标记为未运行。
- 原生鼠标、窗口焦点、弹窗实际打开和关闭、多显示器与 DPI、真实显示效果没有覆盖，报告明确标为 `not_run`。不会通过 Computer Use 或其他桌面操作工具绕过这些限制。
- 测试要求来自固定箱体和明确的菜单坐标语义；不得为了通过测试而把预期自动改成当前实现。新的功能需求需要增加独立断言。
- 编译时会在系统临时目录建立指向 Lazarus 的目录连接，并只清理本次运行生成的临时目录；报告保留在指定输出目录。

## 动态入口采集

运行 `python freeship_ui_test.py --case dynamic --output reports/dynamic`，或使用 `--case inventory` / `--case all` 同时生成 `dynamic_inventory.json`。文件按无模型、载入模型、选中控点及中俄语言状态记录已构造弹出菜单和组合框回调的组件路径、执行状态、调用前后条目；工具栏溢出还记录代理的原动作。下拉框目前仅枚举 `TComboBox.OnDropDown`，未构造的对话框、其他控件类型及系统交互不在覆盖范围内。

为避免未知回调打开窗口、访问用户文件或产生桌面输入，`dynamic_callbacks.json` 限定经审查的回调及对应源码 SHA-256；不匹配时只记录、不执行。源文件变更后应重新审查回调和其调用链、更新哈希，而不是自动认可新代码。`--callback-policy` 可指定独立策略；下拉规则使用 `owner_class`、`handler`、`source`、`sha256`，仅适用于能通过运行时方法名识别的回调。源码哈希仅覆盖所列文件，不能证明其调用链安全。报告中的合成组合框用来验证允许和拒绝路径，不能冒充真实应用覆盖。

通过处理器身份发现动作、菜单项、图层控件和编辑器；匹配不到或不唯一时标为 `not_run`。它们仍是针对已知处理器和模型 API 的适配器，不保证理解任意新功能；坐标编辑器使用无序值集合，不能确认轴标签映射。详见审计文档。

边缘吸引与动画的独立布局回归可运行 `python -m unittest tests.test_edge_layout -v`。它在临时目录编译布局器副本，检查四边候选、重合点避让、部分裁切、逐帧移动和最终收敛；这不替代真实桌面视觉验收。

对单个报告的待复核和未运行项按证据类型分类：`python triage_report.py <report.json> <triage.json>`。分类包含逐项依据，不能将“界面候选问题”直接解释为已证实缺陷；不应跨不同源码哈希把数量变化直接归因于某项修复。

## 子视图移动与重绘测试

独立运行 `python freeship_window_test.py --output reports/window`。程序复制当前源码和箱体样本，在临时目录构建专用 `window-probe.exe`，仅以隐藏控件和进程内方法／消息运行；不修改工作区源码、现有程序、个人模型或配置。新建视图路径在单独隔离进程中运行，防止异常影响其他证据。源码、探针和样本运行期间变化会拒绝结果。退出码与主测试一致。

报告包括四个视口的实际模型缓冲图、直接模拟的移动和缩放、冻结／解冻及通过测试探针使缓存失效时的重绘次数、每次旧新区域合并失效请求及返回值、子视图几何重叠与暴露次数、冻结时修改私有样本名称前后的图像和实际文字绘制记录、定时器暂停时间补偿、子窗口状态及隐藏父窗口缩放。`move_samples` 为原始逐次数据，`timings_ms` 给出 P50、P95、最大值与总和，`summary.themes` 按移动、缓存、重绘、窗口状态、计时器等分别汇总；这些是隐藏窗口的本机测量，没有性能门槛，不能代表实际拖动延迟或 DWM 合成效果。可见区域过小、线程调度和其他设备配置都会影响耗时。

`window.capture-cancel-focus`、`window.caption-prepaint`、`window.compositor-trails-flicker`、`window.sibling-visible-repaint`、`window.real-stacking`、`window.print-export` 和 `window.timer-recovery` 明确为 `not_run`：内部状态入口、消息和失效请求不证明物理鼠标、捕获丢失、真实屏幕重绘、打印或计时器事件循环已经正常。用户历史录屏可在 `reports/window-video-evidence` 查看，只是旧问题样本。

`python window_mutation_tests.py` 在临时源码副本中分别取消缓存路径、背景擦除处理、失效请求、最小化排布、位置复制标志、吸附和计时补偿，并验证对应行为检查失败；另检验无关标签探针格式改变不会阻塞独立窗口测试。继承 Paint 的瞬时闪烁及物理拖动原子性变异无法由隐藏最终缓冲证明，汇总中保留为 `not_run`；即使已执行的变异全部检出，总状态仍为 `partial`。变异文件仅是测试样本，不代表 FreeShip 真实版本。

常规 UI 测试的注入点优先精确匹配；当空白变化时按 Pascal 词法标记作唯一匹配，字符串和注释内容仍要求完全相同。零处或多处匹配仍为基础设施错误，`instrumentation_matches` 记录实际定位方式。该改进不保证所有结构改动都能自动适配。源码副本的指纹会与初始源码比对，并记录加入探针后的指纹；源码或探针在运行中变化仍会拒绝结果。

## 上游问题回归探针

运行 `python upstream_regression.py`（可指定 `--source`、`--lazarus`、`--output`、`--timeout`）。它复制当前源码和箱体样本，在临时目录编译，执行模型层的单位切换，并在 `report.json` 保存源码、编译器、样本和测试代码哈希、逐项实际值与预期值。输出目录必须不存在且位于源码树外。进程只在隔离副本内运行，不操作用户桌面或模型；写出证据后终止隔离进程以避开未保存提示，因而不覆盖应用正常退出流程。

17 条问题均有探针或观测入口；这不等于每条都已经全面覆盖。`coverage_matrix` 按记录列出各案例状态；observed 仅表示有运行结果，并不代表覆盖完整。报告将每条尚缺的典型场景单独列为 `not_run`，板材误差观测列为 `review`。主要场景：

- 01、02：注入序列化异常，核对正式文件哈希和修改状态；以非法标识加载，检查旧模型和修改状态。
- 03、04：注入零进度读写并限时；构造长度超过剩余缓冲区的内嵌 JPEG，检查读取位置。
- 05、06：检查密度及横剖线、水线、纵剖线的单向与往返单位换算。单向预期使用 1 英尺 = 0.3048 米、1 磅 = 0.45359237 千克；往返通过不能证明单向正确。
- 07：在无站线的隔离实例中注入模型边界，观察剖面位置是否位于边界内；这不是对实际船型平移后的全部静水力验证。
- 08、09、10：注入重建异常后检查忙碌标志及重试；在加载中注入变更事件检查监听器；建立编辑与撤销后取消临时快照，再按控点名称检查重做。
- 11、12：模拟外部程序返回 0 和 7；记录静水力表实际计算的吃水序列，不显示结果对话框。
- 13、14：选择隔离路径导出 STL 并比较图层对称开关前后的面数和顶点 Y 范围；样本不满足半船前提时标为 `not_run`，满足时仍需复核逐面和法向；使用有效 JPG 检查背景图导入。测试只替换文件选择结果，不替换实际导出或解码代码。
- 15：在独立工作目录计算并检查是否隐式创建报告文件。
- 16：记录箱体展开及其放大两倍后的边误差和面积误差供复核；尚未建立板块身份对应，也没有曲面样本的自动判定。
- 17：复制并记录 Motorboat3 样本哈希，通过桌面应用加载代码检查是否加载出模型，不代表所有旧文件兼容性。

零进度注入确认到达故障点后等待 8 秒，超时终止对应测试子进程；其他探针同样在隔离副本执行。单个子进程超时、无报告或格式错误时该项标为 `error` 并继续执行其他子项；共享插桩和编译失败仍会中断整批。源码及测试文件在运行期间变化时，总状态为 `error`，已有局部结果不可视作完整验收。

后续应按各记录增加真实行为探针，例如隔离文件的写入失败注入、损坏数据和超时进程、可追踪的历史模型样本与几何输出对照。不得把源码文本匹配或由被测实现推导的结果当成独立正确性断言，也不能将缺少样本的情形算作通过。

## 多语言文字裁切证据

运行 `python freeship_ui_test.py --case inventory`；`all`、`toolbar`、`chinese`、`language` 也采集同一证据。隔离副本会枚举应用语言目录中的 `FreeShip.*.po`，尝试切换可识别的语言，`text_visibility.json` 的 `language_attempts` 记录每个文件的采集或未运行状态。每个已构造的文字控件记录完整文字、状态、语言、字体、画布测量、控件边界、祖先客户区交集、局部可见状态、换行属性（有公开属性时）、提示文字及可能的裁切原因。组合框还记录静态选项文字及测量宽度；`dynamic_inventory.json` 对获准执行的动态下拉回调另外记录选项文字宽度。

报告中的宽度是使用控件声明字体在 LCL 位图画布上的估算，控件边界不是文字的实际客户区；菜单栏、弹出菜单和下拉列表实际宽度、原生内边距、图标、省略号、字体回退、DPI、像素绘制和真实屏幕可见性未测量。换行控件的单行宽度超出也只是候选。未创建对话框不在采集范围；动态选项只对获准的回调采集，菜单的完整文本可在库存读取，但菜单项没有宽度判定。候选标为 `review`，未见候选也不表示通过。`--case panel` 的位图是已有的单面板离屏证据，不能当作所有语言的真实窗口截图。


## 型线图性能与结果回归

```powershell
python freeship_ui_test.py --case linesplan --model "D:\模型\船型.fbm"
python freeship_ui_test.py --case linesplan --model "D:\模型\船型.fbm" --compare "D:\参考报告目录"
```

可重复传入 `--model`；默认使用隔离箱体样本。支持 `--source`、`--lazarus`、`--output`、`--timeout`。输出必须为源码目录之外的新目录。`report.json` 记录源码与测试哈希、样本哈希、运行结果、首帧/重复绘制耗时、像素对照和 DXF 记录对照。只观察时间，不设任意速度合格线。

探针在临时源码副本中分段计时，并仅替换 DXF 保存对话框的路径选择，导出计算保持原样。像素对照在隔离副本中固定文字帧时钟，避免把不同动画进度误判为图像差异。通过临时可写的 Printing 标志在位图上执行打印绘制分支，不操作打印机。缓存结果与强制重画逐像素比较，覆盖缩放、平移、光照、颜色、镜像、吃水、显隐、几何修改、尺寸、字体和显示模式；`--compare` 另与参考报告比较。DXF 坐标与线段连接必须精确一致，不使用数值容差；允许直线方向、闭环起点、折线分段边界改变，仍检查实体头和线段数量。实体输出字节是否相同另记为 `byte_equal`。

`--reference-frame` 可指定已保留的旧版 FreeLinesplanFrme.pas，只用于受控前后对照，不代表替换了整个历史版本；会记录该副本，其余依赖取当前源码。测试针对已核对的型线图绘制 API，不用菜单文案或位置作断言。源码结构不匹配时停止适配，不猜入口。

这不是原生窗口打开时间或桌面交互验收；毫秒计时有粒度限制，缓存命中速度不能当作首帧速度。桌面验收记录为 `not_run`，所以正常完成仍是 `partial`、退出码 2；断言失败为 1，基础设施错误为 3。


型线图探针还会调用已核对的右键拖动回调，以及视口真实的滚轮处理方法，各连续执行 12 步，记录 `interaction_ms`。用 QueryPerformanceCounter 和 GdiFlush 计量每步输入处理与离屏栅格完成时间，不当作实际屏幕帧率。松键和滚轮停止后的图像与强制重画比较；交互中直接保存位图也必须绕过预览并恢复导出状态。

DXF 等价比较见 `dxf_geometry.py`，保持全部精确坐标、图层、颜色和线段连接；不把内存遍历顺序造成的折线分组变化当成坐标变化。`python -m unittest discover -s tests -p test_linesplan_dxf.py -v` 包含六个正反例，修改坐标、缺少连接、改变连接都必须失败。


## 真实操作的型线图诊断

使用正式程序参数 `-trace-linesplan`（也支持 `--trace-linesplan`）后，型线图会在系统临时目录保存有上限的 `FreeShip-linesplan-<PID>-<tick>.tsv`。关闭型线图会刷新日志；日志不记录模型内容。正常启动默认不启用记录。

```powershell
python linesplan_trace.py "C:\Users\用户名\AppData\Local\Temp\FreeShip-linesplan-进程号-时间.tsv" --output trace-summary.json
python freeship_ui_test.py --case linesplan --model "D:\模型.fbm" --event-pause-ms 220 --trace-linesplan
```

`linesplan_trace.py` 只汇总真实操作记录，不把事件数或后台绘图时间当成屏幕 FPS。慢节奏回归在鼠标事件之间运行消息泵，记录 `pump_overhead_ms`；它能发现紧密循环漏测的定时器阻塞。鼠标路径改为视口 MouseDown/MouseMove/MouseUp 方法，隐藏窗口的 SetFocus 单独跳过并标明限制。原生输入验收仍以用户操作和反馈为准。

型线图首帧准备检查：`python linesplan_performance.py --output reports/linesplan-startup`。采集创建后与准备后的隐藏状态、首帧缓冲尺寸和窗口背景；不覆盖真实桌面首次显示时序或闪烁的视觉验收。

`python freeship_ui_test.py --case shortcuts` 同时检查四个船体视图的 Ctrl+I/Ctrl+O 缩放、按键消费和缩放上限。使用视图按键回调，不代表真实键盘焦点验收。


文字清晰度回归：`LineplanTextMetrics.pas` 只注入隔离副本，记录每个交互帧实际的文字绘制次数、字号与忙碌光标状态。`GLYPHS` 和 `ANIMATING` 检查防止把“整张位图快速拉伸”误当成文字实时绘制。支持 `--real-label-clock` 另跑真实时钟；固定时钟仅供确定性像素对照。截图导出前的测试文字状态统一清除动画历史，比较完整静态文字，不比较半显字的动画中间态。

右键平移连续性仍用 `--case linesplan`：拖动激活后再连续输入八次 1 像素位移，`pan.pixel-follow` 要求每次实际 Pan 增量为 1。`PANLABEL` 按稳定的绘制序号配对同一标注，`pan.label-translation` 核对文字位置与裁切框是否同量平移；排除窗口边缘吸附半径及其位移范围（按字体高度计算），边缘效果不属于这个断言。`PANDETAIL` 保留偏移异常的具体标注。微步观测完成后统一镜头位置，再做前后图像与导出对照，防止把修正前少移动的三像素误判为绘图变化。适配器只记录当前型线图绘制 API；不代表原生鼠标、屏幕呈现或边缘效果已验收。

填色覆盖回归：可用 `--width 1920 --height 1140` 按较大画布测量交互。探针在放大后连续缩小，以及右键平移露出原画面外区域时，记录网格和文字绘制前的填色层；`fill-reveal.zoom/pan` 与同一镜头位置的强制精绘比较内部填色覆盖。排除四像素边界以避开临时栅格缩放的边缘差异，内部缺色像素必须为零。前后版本比较不要求错误预览图保持一致；停止后的图片、精绘填色图、位图导出和 DXF 仍按原有规则精确比较。此探针能检出原来“只剩原屏幕范围颜色”的缺陷，不等于帧呈现或全部边界质量验收。

型线图分段计时现在在隔离副本使用 QueryPerformanceCounter，区分填色预览、特征线、网格、折角边线、曲线和文字收尾；不把毫秒计时粒度造成的零值当成没有开销。`LABELREUSE` 记录内部文字复用数量，要求右键平移实际使用复用，而滚轮仍使用完整布局。独立规则检查覆盖内部同量平移、新障碍、文字变化、尺寸变化、边缘和默认完整布局；拖动中另检查字号、缩放和样式变化不会沿用旧布局。

`--pan-steps 96 --initial-zoom 1.25 --real-label-clock --event-pause-ms 8` 可检查较长拖动以及缩放后的平移。报告记录步数、初始比例、每步耗时和文字复用证据。这些参数只改变隔离探针，不操作原生鼠标；实际窗口延迟与显示器帧率仍为 not_run。

界面尾部标点盘点：`python ui_terminal_punctuation.py --output reports/ui-terminal-punctuation`；可加 `--inventory <inventory.json>` 合并运行时控件快照。输出 review 候选清单，语言包条目包含未使用旧文案，不能当作实际可见问题总数；省略号单独计数。此命令只读源码和语言包。

`python -m unittest discover -s tests -p test_edge_layout.py` 增加名称标注完全离开边界再返回的连续性检查：返回首帧不得全宽弹出，连续可见帧限制位移。合成布局测试不代表录屏中的所有交互已通过。

`--case labels` 新增 61 帧真实船体 DrawToViewport 往返平移采集（每帧 8 像素），检查参考线标注锚点不得换到另一端；同时保留文字/裁切/定位轨迹及每帧图片，尚不代表真实桌面交互验收。

边界布局回归同时验证参考线名称与附近同值刻度去重（DWL/基线）、远端及异值刻度保留，以及名称不显示时普通刻度不被隐藏。入口仍为 `python -m unittest discover -s tests -p test_edge_layout.py`。

边界布局单元测试新增控制点坐标去重：相同投影像素与格式化数值只允许首个代表，不同值/不同像素保留，每帧重新选择；实际船体按选中点优先的现有顺序调用。不会合并模型点。

型线图最大化重绘回归：`python linesplan_performance.py --output reports/linesplan-resize-recheck`，增加窗体子窗口合成样式、放大/恢复后缓冲尺寸检查。模拟尺寸变化，不操作桌面，不代表原生最大化动画已视觉验收；输出目录必须不存在。

型线图背景覆盖回归通过实际 LM_ERASEBKGND 消息向内存 DC 绘制，检查完整背景覆盖四角；尺寸变化的首帧另走实际缓存呈现方法，要求先生成当前尺寸缓冲。真实桌面开窗/最大化时序仍未覆盖。入口 `python linesplan_performance.py --output reports/linesplan-background-recheck`（目录须为新目录）。

型线图调整尺寸的验收规则：每次呈现都必须以当前客户区尺寸和投影绘制文字，禁止拉伸含文字的整张缓存。原先“调整尺寸过程中不得触发 OnRedraw”的工作量断言已移除，因为它会保护模糊的整图拉伸路径。性能仍记录毫秒耗时，不将少重绘等同于顺滑。

型线图过渡绘制顺序检查：用记录画布统计实际呈现方法，当前尺寸的完整缓存必须一次贴图且零次清白；同时记录合成画面的缩放次数，必须为零。入口 `python linesplan_performance.py --output reports/linesplan-presentation-recheck`；仍不代表桌面原生动画已验收。

窗口探针增加四视图绘图区内侧 6 像素的左右/下边界悬停命中与离开恢复光标检查，模拟事件回调不操作鼠标、不捕获桌面。

首次显示清晰度回归：`python linesplan_performance.py --output reports/linesplan-first-resolution-recheck`。在隐藏窗体完成准备后再次改变尺寸，调用实际缓存呈现方法，要求呈现前缓冲已按新客户区尺寸重绘，避免首帧文字被拉伸。此检查覆盖延迟布局引起的分辨率不匹配，不覆盖 Windows 原生开窗时序或实际屏幕字体观感。

型线图放大/恢复首帧回归：`python linesplan_performance.py --output reports/linesplan-resize-native-recheck`。逐次检查放大、恢复、再次放大及连续 12 个拖动尺寸，无需 PrepareFrame 或等待空闲就调用真实呈现方法；要求每帧实际绘制原字号文字、缓冲与客户区等大、合成文字位图未被缩放，首帧投影与当前尺寸相符。保存 `resize-maximize-first.bmp`、`resize-restore-first.bmp` 供检查；另比较放大期间填色与精绘结果的船体内部是否缺色。测试仅操作隐藏窗体及内存画布，不操作桌面、不代表原生最大化动画已视觉验收。可传 `--model` 使用模型副本；原文件仅供读取并核对哈希。

型线图独立切换窗口：同一性能回归入口检查实际 HWND 的 APPWINDOW / TOOLWINDOW 样式和 stAlways；不模拟 Alt+Tab，不代表系统切换列表已人工验收。

型线图尺寸过渡的独占呈现检查：性能回归在放大/恢复和连续尺寸变化时，分别记录请求目标画布与此前 DrawingCanvas 的输出次数，要求生成缓存时不向此前窗口画布输出，目标只贴图一次且不清白。入口 `python linesplan_performance.py --output reports/linesplan-single-present-recheck`。此检查补充此前仅记录目标画布而漏掉生成缓存时附带输出的问题；不等价于 Windows 窗口动画的视觉验收。

独立窗口激活路由：`python freeship_window_test.py --output reports/activation-routing` 检查正式入口运行时 Application.OnActivate 是否错误绑定主窗口激活回调；型线图性能回归另记录原生 GW_OWNER。两项均不发送激活/Alt+Tab，不代替实际前台切换验收。

激活路由故障注入：`python freeship_window_test.py --mutation app_activation_redirect --output reports/activation-routing-negative`，仅在隔离副本中恢复错误事件绑定，预期 window.application-activation-routing 失败；不调用会抢前台的回调。

放大新增区域回归：性能回归保存 growth-form-background.bmp / growth-frame-background.bmp，直接调用外层窗体和承载面板的实际 EraseBackground 到内存 DC，要求首个背景输出包含型线图而非纯背景色。--host-erase-negative-control 仅在隔离副本恢复旧背景擦除，预期这两项图片内容检查失败。不发送窗口控制或桌面输入，不覆盖 DWM 的实际动画时序。

原生尺寸/绘图时序诊断：正式程序 `-trace-linesplan` 在临时目录写出有上限的 FreeShip-linesplan-<PID>-<tick>.tsv。也可在 exe 同目录放置 `.linesplan-trace-once`，下一次创建型线图消耗该标记，该进程随后打开的型线图也继续记录；记录各层 before/after 消息、LCL/原生客户区及屏幕矩形、缓冲尺寸、绘图目标 DC 与裁切矩形，不记录模型内容。窗口关闭或空闲时刷新。`python linesplan_trace.py <日志> --output <报告.json>` 汇总几何时序差异与重入呈现线索，状态 review，不把差异等同可见闪屏。解析回归：`python -m unittest discover -s tests -p test_linesplan_trace.py -v`。

已有录屏证据提取：`python window_video_evidence.py <录屏.mp4> --output reports/video-evidence --start 1.15 --end 1.75 --step 0.03`，依赖 PyAV 和 Pillow，保存原尺寸逐帧图片、带时间戳的总览和 JSON，核对原文件哈希，默认结果 review。只读取给定文件，不录屏、不控制桌面、不自动判定闪动是否修复。

原生尺寸提交回归（来自实际操作日志）：`python linesplan_performance.py --output reports/native-layout-recheck`。在隐藏控件的 AutoSizing 锁中制造“LCL 尺寸已改、HWND 仍是旧尺寸”的真实中间状态，向关联该 HWND 的记录 DC 请求呈现，要求零次原生贴图/清空；可先在内存准备未来画面，不能把它画入旧 HWND。重复请求也不得输出。另检查已提交尺寸的原生窗体和多层控件树可正常通过屏障；原有内存画布尺寸/字体/导出检查继续运行。`--native-commit-negative-control` 在隔离副本取消屏障，预期两项原生输出检查失败。该检查不控制桌面，不代表 DWM 动画已视觉验收。

本次对比证据：真实日志在 reports/native-user-activation 下，1312 ms 时视图区 LCL/缓冲准备尺寸变为 1916×1136，但原生 HWND 仍为 1031×741；新图贴入旧 DC 后，约 1421 ms 才提交原生尺寸。linesplan_trace.py 新增 premature_native_presentations 汇总此类实际原生输出（排除内存画布与已延迟的请求），仍保留 review 状态。


填色边缘稳定性：`python linesplan_performance.py --output reports/stable-fill --model "D:\模型.fbm"`。在缩放、平移和窗口尺寸变化时，将操作当帧的填色层与同一镜头强制重建的填色层逐像素比较，包含船体边缘，变化像素必须为零。这用于检查延迟替换填色质量，不代替原生窗口闪动验收。`vector_geometry_aligned` 使用独立模型坐标矩形，检查不同目标尺寸、偏移和 DPI 下的填色位置；`vector_cache_resources_released` 检查多颜色重复构建后的 GDI 资源释放。

`--fill-reference <正确旧版报告目录>` 可观察原填色器与新填色器的几何范围差异（参考报告须使用相同模型和尺寸）。记录双方边界及超出两像素距离的填色像素数；原栅格取整、单像素尖端及新边缘滤波会改变这些像素，因此此跨渲染器比较固定为 review，须检查保留的图片，不能当作精确几何断言。比例与位置由独立矩形模型的 vector_geometry_aligned 硬断言验证；质量是否延迟变化则仍由同一渲染器的逐像素比较硬断言验证。`--physical-dpi-negative-control` 只在隔离副本恢复错误的逻辑 DPI 换算；在此 125% 缩放环境中，矩形位置检查应失败。首帧原生窗口动画仍标为 not_run。


原生尺寸提交期间的工作量回归：同一 `linesplan_performance.py` 入口的 `native_pending_size_no_render_work` 要求 HWND 尚未提交时，不绘制文字也不重建未来尺寸缓冲。原生日志发现该同步绘图会在最大化路径阻塞布局 110～125 ms；应等待几何提交后再绘制。仍不等同屏幕闪动验收。


原生窗口合成配置观察：`linesplan_performance.py` 记录实际 HWND 的 ExStyle、类样式、WS_EX_COMPOSITED 及不兼容的 CS_OWNDC / CS_CLASSDC / CS_PARENTDC 位。此项固定 review，不能由样式位存在推断白闪/黑闪已修复；须结合用户实际试用。现有填色边缘稳定性、原生尺寸屏障与导出断言继续执行。

### 隔离实例的真实窗口闪动采集

用户于 2026-10-30 明确允许专用测试程序控制它自己启动的隔离实例。仅此入口使用该授权，普通隐藏探针的覆盖范围不变。

```powershell
python native_flicker_test.py --model "D:\模型.fbm" --output reports/native --cycles 10
python -m unittest discover -s tests -p test_native_flicker.py
```

默认 `--host full-app --backend screen --language cn`：复制当前源码和模型到独立临时目录，构建独立程序，经过主窗口实际型线图动作打开窗口；使用 DXGI 桌面复制采集系统合成后的画面，只保留私有进程全屏灰色遮挡窗覆盖的区域。专用窗口会临时遮挡桌面，但不激活、移动、缩放或结束已有用户窗口。每次最大化、恢复和关闭前均核对 Popen 进程、PID、HWND 和随机标题；不接受外部 HWND。发现其他进程窗口覆盖采集区时停止并报 `error`，不保存其画面。排除任务栏边缘区域。个人配置和原模型不作为写入目标。

依赖：`numpy`、`Pillow`、`av`、`pywin32`，以及本目录 `.capture-deps` 中的 `windows-capture==2.0.1`、`opencv-python==5.0.0.93`。后两项通过 `python -m pip install --no-deps --target .capture-deps windows-capture==2.0.1 opencv-python==5.0.0.93` 安装；不要求更改全局包。

产物包括无损 `capture-lossless.mkv`、全部已收到的变化帧 PNG、总览、事件时间、原生绘制日志及 `report.json`。采集中只编码，关闭私有窗口后才提取 PNG 和分析，避免压缩截图阻塞采集。队列溢出、窗口遮挡、解码缺帧、模型变化或源代码在运行期间变化均使结果为 `error`。报告记录源码、测试代码和编译后程序哈希。另一对话同时修改源码时，不能将该次结果标为当前代码验收通过。

检测规则观察彩色船体是否暂时丢失及是否出现大面积中性黑色；部分丢失为 `review`。稳定画面本身的深色内容参与基准计算。它能检出本次大面积白闪、黑闪，不承诺检测每种细小像素瑕疵，也不保证获得每一次物理显示器刷新；保存的是 DXGI 实际交付的每一变化帧。窗口自身的 WGC 采集可用 `--backend window`，但它遗漏桌面合成动画，不能用于验收本次问题。

故障注入是独立负对照，在自身窗口短暂覆盖白色或黑色区域，预期报告 **fail**：

```powershell
python native_flicker_test.py --host standalone --inject white --output reports/native-white-control --cycles 2
python native_flicker_test.py --host standalone --inject black --output reports/native-black-control --cycles 2
```

`--variant no-background-fix` 仅在隔离副本撤掉尺寸变化期间的背景修复；`--variant black-background-control` 只把该临时背景从白色换成黑色。二者用于观察因果关系，不修改项目源文件；间歇故障在短负对照中没有复现，不代表旧路线没有问题。早期 `legacy-resize`、`standard-background`、`lcl-only` 等探索适配器已退役，其历史报告保留，不属于通用产品规则。`--language en|cn|ru` 切换私有测试语言。每次输出目录必须新建，避免旧证据混入。

重新审查旧录像：`python native_flicker_test.py --reanalyze reports/native/report.json --output reports/native-new-analysis`。保留原始报告、录像和帧文件，只生成带原证据哈希的新分析。2026-10-30 的 `native-final-cn` 初版曾因只检查超过 10% 的黑色区域而判为 pass；人工查看第 590 帧发现约 7.9% 黑色边带。该通过结论已撤回，现行规则按稳定深色基准增加 1.5 个百分点且最低 2% 检查，`native-final-cn-edge-audit` 检出 19 帧，结论 fail。不能引用旧 pass 宣称闪屏修复。`native-final-candidate` 仅表示未插桩候选编译成功，不表示界面验收或已安装；该候选没有替换固定入口。

若只需编译候选：`python native_flicker_test.py --build-release-only --output reports/candidate`。构建发生在隔离目录，产物为报告目录内的 FreeShip.exe，不启动窗口、不修改实际安装入口或根项目 LPI。

2026-10-30 边缘黑带修复证据：`native-edge-fixed-cn` 中文 20 轮、779 帧 pass；`native-edge-fixed-ru` 俄文 5 轮、214 帧 pass，另核对每次最大化和恢复后共享窗口类的背景画刷回到原值。两者使用同一个不可变源码快照，原模型未修改，采集无错误。`native-edge-negative-control` 撤掉修复的 6 轮、291 帧没有复现；`native-edge-black-background-control` 明确改成黑色背景后，6 轮、245 帧检出 3 个黑带异常帧，为预期 fail。不能把短负对照未复现改写成“旧版必定每次闪”。此前白屏、黑屏覆盖注入仍为预期 fail。

实现只在型线图原生最大化/恢复系统命令的同步处理期间提供白色背景画刷，并在 finally 中恢复原值，避免永久修改 LCL 共享窗口类。保留 Windows 原生动画。`native-edge-fixed-regression` 为 240 pass、2 review、1 not_run；后两类仍不算通过。最新合并构建和固定入口的哈希及原程序备份见 `native-edge-release-latest/installation.json`；该构建包含另一个对话的文件读取及计算相关修改，和最初捕获快照的差异逐文件记录，不能冒称是同一个完整源码快照。

安装前后最终复测：`native-edge-installed-source` 使用与已安装程序相同的完整源码哈希，实际主窗口动作打开型线图，3 轮、298 帧 pass，无采集错误，原模型不变，每次操作后共享背景均恢复。`installation.json` 已记录此证据并再次核对固定入口程序哈希。以上三组修复版采集合计 28 轮、1291 帧；只代表本机专用隔离实例已交付的变化帧，不是所有设备和所有刷新帧的保证。

实现参考：[LCL 双缓冲](https://lazarus-ccr.sourceforge.io/docs/lcl/controls/twincontrol.doublebuffered.html)、[Win32 客户区尺寸与有效像素](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize)、[Qt 后台画布完整绘制与提交](https://doc.qt.io/qt-6/qtgui-rasterwindow-example.html)。[DirectComposition 无闪动示例](https://github.com/bigfatbrowncat/noflicker_directx_window)说明在原生尺寸变化前准备新画面的另一条路线；它使用 Direct3D，且注明部分设备仍有问题，本项目未引入其代码或宣称该示例保证无闪动。

### 历史测试代码积累审计

`python historical_regression.py --output reports/history-new --run-reviewed` 现在读取受哈希检查的 `historical_manifest.json`，运行 `probes/history` 中的 31 个维护副本及三个新探针（隐藏关于页/关闭行为、原生图标加载与尺寸回退、隐藏进度回调）。其中 5 个此前已接入，本次新增 26 个。`--probe` 可重复选择；`--source`、`--lazarus`、`--model` 可指定依赖，模型只读复制。没有 `--run-reviewed` 时仅核对清单，运行项列为 not_run。不再从个人临时目录自动覆盖归档。

清单共 71 个来源：31 integrated、13 replaced、17 retired、10 non_test_tool，逐项附原因。后面三种不是“测试通过”。`assertion` 模式的明确行为失败保留 fail；`observation` 模式的旧布局偏好、历史时间阈值及性能采集固定标 review，另存原预期是否满足和原退出码。编译或异常退出仍是 error/fail，不能因为是历史脚本就忽略。详细范围见[逐项处理清单](历史脚本处理清单.md)。

`python historical_assets.py --output reports/assets-new` 按资源目录枚举 PNG，验证解码和声明尺寸，不锁定图标数；已接入统一入口的 `historical-assets` 组。它与原生图标加载探针分开：前者检查文件，后者检查内嵌资源及非标准尺寸回退。图标美观和真实桌面显示未验收。

`python -B -m unittest discover -s tests -p test_historical_catalog.py -v` 检查清单闸门：漏项、原件哈希变化、未审阅状态及把归档文件冒充维护副本都会被拒绝。

### 静水力报告单位与对齐

`python report_presentation_test.py --output reports/report-new` 在隔离源码副本构建 `ReportPresentationProbe.pas`。检查面积、体积、惯性矩和密度的 Unicode 上标单位；在隐藏的原生 TMemo 中通过 EM_POSFROMCHAR 测量不同长度中文、俄文标签的冒号及数值起点是否一致；检查数值保留、纯文本保存不用界面自定义制表位。运行日志保存实际横坐标，JSON 保留 pass/fail/error 及源码哈希。此入口不操作桌面窗口，不能代替最终字体观感或物理打印机验收。

### 单位符号策略校验

`python check_unit_symbols.py --loader ../TranslationReview_20261029/loader_build/check_po_loader.exe --output <结果目录>`

独立依据：用户要求中文数值单位使用符号，七种可选语言的角度统一为 °。使用实际 Lazarus TPOFile 校验新旧英文角度原文的稳定标识查找及中文单位条目；JSON 报告记录 pass/fail。这不覆盖真实报表渲染，loader 需要由 check_po_loader.pas 在 Lazarus 工具链下构建。

### 静水力内嵌表格视图

`python report_presentation_test.py --tables --output reports/hydro-tables-new` 复用隔离构建器运行 `ReportTablesProbe.pas`。检查属性分组、ASCII 数据表转换后的单元格和值、上标单位、分组控件不重叠、窄窗口重建、报告可调整大小和旧文本视图隐藏；字号和窗口尺寸作为观察值记录，不单凭数值断言视觉合格。

`UiNativeRender.pas` 将私有控件的实际 WM_PRINTCLIENT/LM_PAINT 分别渲染到内存表面，再按真实控件边界组合截图，避免共享 DC 原点被子控件改写；只供内存布局预览，不是 DWM 或显示器帧验收。探针会将其自己创建的窗体短暂放在屏幕外以完成原生布局，不控制已有窗口；空白截图标记 error。样本取自用户提供的报告片段，未读取或修改原模型。实际复制按键手感、打印机和最终观感仍由正常程序使用验证。

表头解释与基准说明：`report_presentation_test.py --tables` 现也运行中俄语言包下实际绑定的 OnGetCellHint 回调，检查 Cw 含义、LCB/VCB 测量基准、系数单位行的计算基准、数据行无表头解释；调用实际 AddFooter 生成全部 16 项旧缩写说明，再检查它们已从可见报告移除且含义可查。注 1/注 2 从表格正文收起到分组标题提示；原始导出内容保留。探针输出 `tables.hints.json` 保存 UTF-8 提示文本。此项是回调与状态验证，不声称已执行真实鼠标悬停或验收系统提示窗外观。

顶栏分组观察：`--case toolbar` 现在在隐藏探针中临时取消顶栏面板的 Align，并分别设置 1920/1280/900 像素宽度，采集中俄文布局和真实“更多”菜单 OnPopup 生成后的条目。快照状态以 `toolbar_fixture_` 开头，属于面板尺寸夹具，不能称为主窗口实际桌面验收。原普通快照仍保留。`python toolbar_layout_observations.py --inventory reports/<run>/inventory.json --output reports/<run>/toolbar-layout.json` 按真实父容器整理成员、间距和矩形相交；不把分组名称、顺序或具体间距写死为通用合格条件，默认 review。2026-10-31 本轮用户明确要求为三个左侧组（其他按钮、工程设置/剖面线/型线图、控制网格）及右侧独立关于入口，组间留空；关于保留最右侧已由用户确认。

### 视图 Ctrl+A / Ctrl+I / Ctrl+O 消息路径回归

运行 `python freeship_ui_test.py --case shortcuts`。`shortcuts.posted.*` 在隔离实例的四个视图 HWND 上投递 WM_KEYDOWN/WM_KEYUP，并临时设置测试线程的修饰键状态，覆盖应用级快捷键分发、目标视图缩放、全选切换、部分选择扩为全选以及文本框 Ctrl+A 保留。不会向正在编辑的实例发送输入，也不使用全局 SendInput。这是消息路径验证，实体按键及真实焦点交互仍为 `not_run`。

顶栏绘制证据：`--case toolbar` 额外输出 `toolbar-{Chinese,Russian}-{1920,1280,900}.bmp`，以各控件 WM_PRINTCLIENT / LM_PAINT 绘制合成顶栏，供检查容器文字与按钮文字叠绘等矩形检查无法发现的问题。此为隔离控件绘制，不是桌面截图，不验证 DWM、悬停或真实焦点；须实际打开图片检查，不把文件生成当作视觉通过。

型线图缓存复用检查（dialog-opening）：记录首帧静水力、三角形数量、排序后累计时间、填色与缓存准备阶段；验证关闭窗口后的缓存转移、Draw 失效释放，以及重新打开与冷生成的逐像素一致性。报告中的阶段时间是累计值，计时精度受系统时钟限制，不用于证明个位数毫秒开销。仅修改隔离副本加入探针，普通程序没有这些输出。


### 型线图磁盘缓存和剖面增量检查（2026-10-31）

`python run_tests.py --suite dialog-opening --suite lines-incremental` 可运行内置样本。指定复杂模型时分别运行 `dialog_opening.py --model <fbm> --output <新目录>`、`section_incremental.py --model <fbm> --output <新目录>`。

`dialog_opening.py` 会在隔离环境中启动三个独立进程：首次生成、重启读取、故意损坏缓存后重建。检查缓存转移、模型销毁和冷/热画面一致性。可加 `--compare <旧报告目录>` 逐像素比较旧的 cold-1/2/3.bmp。缓存及损坏注入仅位于私有临时目录。计时是隐藏准备耗时，真实桌面显示仍为 not_run。

`section_incremental.py` 实际修改模型控制点并恢复，再切换剖面简化设置；每条剖面的结果都与独立、禁用复用的完整求交比较坐标、点数和折角。报告记录复用和重算数量。小模型一次改动可能影响全部剖面，不以固定复用比例作为通过条件。同时检查改点后的磁盘缓存身份变化及恢复后的身份稳定。

窗口外观与模态重建回归：`--case toolbar` 增加 `dialog.modal-handle-stable.*`，在测试实例内于屏幕外打开工程设置和剖面线的模态窗口，定时自动取消，确认进入模态前后的原生句柄稳定；不执行编辑和保存。`dialog.cursor-visual` 明确为 not_run，该测试不采集实体光标的闪动。`chrome.native-caption-color` 尝试读取当前窗口 DWM 标题色，不支持或读取失败时 not_run。顶栏 bitmap 只能验收客户区，不能当成系统标题栏截图。

### BFS 线段扩散绘制回归

`python overlay_performance.py --output reports/<新目录> [--compare reports/<基准>]`，或 `python run_tests.py --suite overlay-animation`。在隔离源码副本构建，用 QPC 测量四次连续 960×540 合成线网绘制，记录冷帧、P50、P95、最大值，检查稳定图不重复构建、源点/交点、远端延迟、断开分支完成且不生成假连线，输出 front.bmp / complete.bmp。该工作量不含完整模型、文字布局、Windows 定时器和显示合成，不能换算为真实桌面 FPS。首次构图单独记录，不因稳态绘制更快就认定构图也改善。

### 真实四视图开场动画测量

`python native_animation_test.py --model <fbm> --output reports/<新目录>`（内置盒体可省略 model），也已接入 `python run_tests.py --suite native-animation`。仅启动当前源码的私有完整应用，模型复制、APPDATA/LOCALAPPDATA/TEMP 独立，禁用全局热键；PID、随机标题验证后才捕获该 HWND，实例由测试独立终止。使用 WGC 采集真实可见窗口的合成表面；不接受用户窗口句柄，不发送输入到已有实例。模型哈希和源码指纹必须保持不变。

报告的 views[].visible_updates_per_second 是 BFS 的 1.3 秒区间内各视图采样区域的实际变化次数/秒，draw_intervals_ms 和 draw_cost_ms 记录真实 OnRedraw 的 QPC 时间。采样按各视图 HWND 精确客户区域、内缩 20 像素，每 4 像素取样；细微变化可能漏计。WGC 是变化驱动，不能把整个窗口的回调数冒充四个视图各自帧率，也不表示显示器扫描输出频率。profile.csv 为各重绘阶段 QPC 标记。pass 表示采集成功且四个视图均有数据，不表示流畅度合格或达到 60 FPS。

`--without-base-cache` 仅在隔离构建中禁用动画底图缓存，作为同源码负对照；不修改工作树。`python overlay_performance.py --cache-check --output reports/<新目录>`（统一入口 animation-base）检查底图复用、动态线未烘入、缩放、尺寸和显式模型失效以及动画结束后绕过缓存。真实窗口测量仍应与此功能检查区分。

视图状态入口适配（2026-10-31）：`--case toolbar` 的 `views.*` 依据本轮明确要求检查四个视图的状态按钮、显隐、无标题按钮、标题双击展开/恢复、点击其他视图及顶栏空白时隐藏。按钮入口是本版本已知适配器，未发现时 not_run；测试调用控件 Click、OnDblClick 及 DismissViewsExcept，不把它们等同实体鼠标和命中测试验收。隐藏视图冻结动画，显示时恢复。顶栏中俄文三种宽度图片继续输出。

`python overlay_performance.py --label-reveal --output reports/<新目录>` 测试点、命名线及横纵刻度的数字均从部分宽度逐步出现，静止时揭示单调完成，关闭动画后保留完整文字。视频 113024 中的数字首次入边界问题对应此前非命名标注强制完整宽度的分支；该测试不自动认定全部边界交互已通过真实拖动验收。

### 统一图标接入回归

`python icon_integration_test.py --output reports/<新目录>` 或 `python run_tests.py --suite icons`。隔离构建枚举所有嵌入PNG并按资源声明尺寸加载，检查35px请求的最近尺寸回退；使用实际Preferences加载器在16/20/24/32/40/48/64/96/128档故意让11个不同动作共用索引，验证分配独立槽、重复加载不增长，以及原本无图形的现有动作获得有效图形。实际构造型线图框架、剖面线和图层窗口的工具栏，输出bitmap检查图。pass仅表示这些检查通过，不覆盖所有对话框、实体悬停/按下或全DPI桌面验收。

`native_animation_test.py --snapshot` 在仅由它启动并验证PID/随机标题的隔离窗口捕获WGC表面，另存native-window.png；不控制用户已有窗口。截图模式额外复制表面，会增加采集开销，不用于性能对照。顶栏 `--case toolbar` 继续输出中俄文三种宽度的控件绘制图。

2026-10-31 后续分组调整：用户明确要求控制网格与横剖面、纵剖面、俯视图、透视图属于同一组，原独立StudioModeGroup已撤去，五个控件同属StudioViewsGroup。继续复用toolbar六组中俄文宽度夹具与通用布局观察，不把此产品分组规则升级成通用测试的固定预期。

2026-10-31 用户再次调整交互：四视图不再因外部点击隐藏，StudioDismissViews/DismissViewsExcept 实现保留但不注册输入监听；顶栏显隐和边框双击不变。toolbar 中旧 outside-hide 预期已由新规则替代：通过 Application.NotifyUserInputHandler 分发各视图和顶栏点击消息后，四视图仍可见；这是应用输入分发检查，不等同实体鼠标验收。

Toolbar regression also checks hydro report export text stays in a hidden container after FormShow while retaining export contents. CN/RU 1920/1280/900 fixtures inspect sequential toolbar group layout; hidden rendering is not physical desktop acceptance.

Toolbar fixtures now assert project tools remain directly visible at 1920px in Chinese and Russian, and all four view switches share grid button height, top, glyph layout, spacing and margin. Scope remains isolated native-control rendering, not physical desktop acceptance.

Seven selectable toolbar languages now run at 1920/1280/900 widths under --case toolbar, including repeated language switching. Application icon resource/shell extraction check: python app_icon_test.py --exe PATH_TO_EXE --output reports/app-icon . This inspects only the binary and shell icon extraction; taskbar cache/desktop appearance remain review. Resource rebuild: python ../FreeShip-Plus-Lazarus/Utils/build-app-icon.py .

Runtime app/taskbar icon evidence: `python run_tests.py --suite native-icon --output reports/native-icon` builds current source in isolation, starts only its own PID, captures WM_GETICON and class-icon pixels on main/application HWNDs before and after RecreateWnd, and compares with the source ICO loaded through the same LCL icon conversion. It does not claim a screenshot of Explorer's taskbar cache. Negative control: `python native_icon_test.py --blank-icon-control --output reports/native-icon-control` removes app icon resources only in the private copy; expected status is fail. Raw Win32 LoadIcon and LCL conversions have different pixel alpha/mask handling, so use the same LCL conversion for the reference, not a relaxed image-difference threshold. Target-only shell refresh: app_icon_test.py --exe PATH --output REPORT --refresh-shell sends SHCNE_UPDATEITEM for that executable; it does not clear global caches or restart Explorer.

### C++ Qt 型线图迁移试点

`python run_tests.py --suite qt-linesplan` 或 `python qt_linesplan_pilot.py --model <FBM> --output reports/<新目录>`。`--model` 可重复指定；默认盒体和合成曲面。工具链首次安装见同级 `FreeShip-QtPilot/setup.ps1`。分别在隔离原源码中构建 `QtLegacyBridge.pas` 旧窗口对照和试点的独立场景入口，保存完整矢量场景、旧图像及 DXF。FSLP 以 Float64 传输，保留原 Pascal 算法的浮点类型。C++ 测试源码在 `qt/linesplan_tests.cpp`，链接实际使用的 Scene、LinesView 和 GPU 填色组件。候选运行时在报告目录中构建，不替换试点 `dist`。

检查坐标/文本往返、填色完整性、鼠标锚点、合成右键及滚轮事件、标注重叠、150% 渲染、坏数据拒绝、原模型保护、改点后的场景变化和新进程确定性。另以 `--verify-model` 启动交付程序的隐藏实例，执行实际异步模型加载与 PNG/DXF 导出路径；不显示或操纵桌面窗口。DXF 保留原 Pascal 导出器，并非已完成 C++ 导出迁移。

每次报告保留源码、测试、模型和程序指纹。`--reuse-build` 和 `--reuse-reference` 只允许原源码、适配器指纹及入口类型均匹配的构建。`--baseline reports/<旧报告>` 追加历史场景及 DXF 对照；模型按内容哈希匹配。`--startup-repeats 3` 默认交替运行三对新、旧桥接，记录独立进程耗时及各阶段；不清除系统文件缓存，不能称为冷启动测试。

场景要求逐字节一致。DXF 比较复用 `dxf_geometry.py`，新增完整实体比较，保留折线分组和精确顶点连接；坐标、颜色、属性没有容差。旧 `ExtractAllEdgeLoops` 使用指针排序，实体排列与折线遍历方向可以随分配顺序变化；字节差异单列 review，不冒称文件完全相同。坐标和颜色篡改是独立负例。生产导出的 DXF 另与该次桥接的原文件逐字节对照。

耗时拆分为桥接进程、读取、整理、GPU 初始化、首张离屏图像及测试导出，另外保留 120 次平移/缩放与 CPU 回退对照。GPU 帧耗时包括读回，不等同显示器帧率；新字段 `model_to_first_offscreen_frame_ms` 不包含 PNG/DXF/场景测试写出。`partial` 仍含视觉复核和原生桌面输入 `not_run`，不能当作完整验收。

每个模型还会在同一 Qt 二进制下运行新旧桥接各一次，记录 `production`、`production-legacy` 首张完整离屏图像耗时，并比较 PNG 像素。仅替换报告私有 `runtime` 内的桥接文件，结束或异常时恢复新桥接；用户的 `dist` 不受测试影响。这是一组成对观测，不宣称多次统计或真实桌面延迟。

### C++ Qt 正式迁移回归

`python run_tests.py --suite qt-migration`，或 `python qt_migration.py --output reports/<新目录>`。默认运行盒体、合成曲面和当前工作区的 9a 模型；`--model <FBM>` 可重复指定。`--qt-source` 指定正式 Qt 源码，`--source` 指定保留的 Pascal 核心。所有模型先复制，编辑、保存、坏文件和锁点样本都在报告或私有临时目录中；不会替换 `fs`、正式程序或试点程序。

该套件使用实际常驻桥接的 JSON 协议检查加载、改点、撤销、重做、分支历史、保存暂存、确认保存和新进程重开；错误文档 ID、旧版本、未知点、锁定点、无效坐标、坏模型及保存失败须保持原模型和历史。`QtMigrationFixture.pas` 仅在隔离源码中构建，用原核心直接计数并生成锁点样本，避免给生产服务增加测试命令。快照检查实体引用、稳定 ID、三角形数量和坐标；型线图对照已验收试点的完整 FSLP 字节及精确 DXF 实体，没有降低精度的容差。

`qt/migration_tests.cpp` 链接正式 `ModelSnapshot`、`HullView`，检查错误数据拒绝、四种投影、右键平移、滚轮锚点、正交拖点的轴映射、锁点、焦点中断和快照不可变，保存四视图离屏图片。正式程序的 `--verify-workflow MODEL OUT` 另行执行真实会话与动作路径，导出各阶段场景和 DXF，与协议测试逐一比较。既有型线图渲染测试继续运行。隐藏实例、合成事件、离屏耗时不等于实体鼠标、桌面画面或显示器帧率验收，结果仍会保留 `review` / `not_run`。

`qt/session_tests.cpp` 使用正式 `ModelSession` 和实际桥接，检查保存到无效目标后保持未保存状态、保护原文件、随后成功保存、撤销/重做/重开，以及忙碌期间拒绝无效编辑时不取消在途请求、坏模型加载回滚和临时产物回收。文件写入走实际 `QSaveFile`；“目标已提交而确认保存的响应丢失”目前缺少可靠故障注入，单列 `not_run`。

旧版折痕导出存在按指针顺序拆链、修改共享边的流程问题。新服务不同入口、重复生成及撤销重做之间，DXF 仍要求完整实体严格一致。只有冻结旧版 `Knuckle_lines` 的多段线分组、LINE 端点反向设单独复核：保留原始实体不一致结果和完整差异文件，用另一套原始 DXF 解析检查精确线段坐标、颜色、图层、标志、重数、闭环和分支连接；LINE 还须精确端点对和全部其余属性相同，其余图层实体必须完全相同。任何坐标或属性差异仍为 `fail`，不会用误差容限消掉。这个旧版对照为 `review`，不冒称文件完全一致；通用 `dxf_geometry.py` 的比较器保持不变。独立反例检查坐标、颜色、标志、闭合边、重复边和连接次序篡改都能被发现。场景提取配方也在私有目录重新生成，要求完整源码和来源清单只规范化 CRLF/CR 为 LF 后一致，另记双方字节 SHA；FSLP 数据始终逐字节比较。

`--reuse-build <桥接报告>` 仅复用原源码和适配器指纹完全相同的构建；`--runtime <Qt运行时>` 校验源码和二进制清单后，复制到新报告目录再测试。省略时自动独立构建。`--baseline <旧试点报告>` 按模型内容哈希定位冻结基准；默认使用 `reports/qt-startup-delivery-20261101`。缺少基准会列出 `not_run`，不会悄悄当成一致。报告分别列 `pass`、`fail`、`review`、`not_run`、`error`；共享构建失败单列，不冒称每个模型已经检查。

组包后的实际入口检查：`python qt_migration.py --verify-deployment ../FreeShip-Qt/dist --workflow-reference reports/<已通过的完整迁移报告> --output reports/<新目录>`。核对包内当前 Qt 源码、测试源码、旧核心、适配器、桥接及所有文件 SHA，然后直接启动指定目录里的 `FreeShipQt.exe`，对三个模型执行隐藏生产动作。各阶段 FSLP/DXF 与完整回归的生产结果严格比较，最后再检查安装目录、原模型、旧程序和试点未变。此模式不重建、不复制运行时、不做桌面操作，真实桌面验收仍为 `not_run`。

### Qt 工程设置、图层和批量编辑回归

`python run_tests.py --suite qt-editing`，或 `python qt_editing.py --output reports/<新目录>`。可重复给 `--model <FBM>`；`--reuse-build`、`--runtime` 与迁移套件使用相同的源码及二进制校验。三模型分别执行真实常驻服务和正式程序的 `--verify-editing`：工程设置确认/取消、切单位和撤销、图层 Apply、两点一笔编辑、撤销重做、保存及新进程重开。每个阶段保留完整快照、FSLP、DXF，协议与生产入口之间精确比较，不降低几何精度。

`qt/editing_tests.cpp` 链接实际控件和视图，检查多选、框选、隐藏点过滤、锁点/关联点整组拒绝、单点原约束路径、只读状态、单位变化后的镜头、未修改字段不舍入、延迟键盘输入和确认/取消。用户已经编辑的数值草稿保留 Double 精度，提交后才由核心存为 Single；仅未编辑字段的单位预览直接显示 Single 换算结果。这两条预期分开验证。控件位置或名称不是通用产品设计验收标准。

`QtEditingFixture.pas` 和 `qt_editing_fixture.py` 仅在私有副本里构建。它们直接观察原核心对象，并保留桥接的隔离适配，不能当作完全未修改的旧版对照。因此米/英尺、密度和板厚采用独立物理换算因子，另查交线平面、背景标定、流线种子及隐藏的手动剖面位置。样本还包括旧 FBM 版本、空交线、锁点和约束、系统颜色、空原始层名。测试检查保存版本提升、编码无法表示时整笔拒绝、UTF-8 中俄文本新进程回读、背景 JPEG 原字节保留，以及失败后模型和历史不变。

所有检查并入同一个 `report.json`，逐项保留五种结果。`--protocol-only` / `--skip-fixtures` 只供明确的局部诊断；`--fixtures-only` 可单独重跑原对象、保存格式及属性边界。被跳过的部分必列 `not_run`，不能用局部结果代替完整回归。约束资格由原对象正反关联独立核对，拒绝样本也按该观察选取；单点锚定移动须只传播一次。图层仅改大小写另验快照、撤销、重做和原核心保存读回。部署后可运行 `python qt_editing.py --verify-deployment ../FreeShip-Qt/dist --workflow-reference reports/<完整编辑回归报告> --output reports/<新目录>`，直接验指定部署目录里的正式入口及全部文件指纹。隐藏窗口、合成输入和导出对照不覆盖真实桌面体验。

### C++ 平面与样条内核验证

```powershell
python -B run_tests.py --suite qt-native-geometry --output reports/<新目录>
python -B native_geometry.py --output reports/<新目录>
```

这组只构建独立的 `native_geometry_tests.exe`，链接 `FreeShip-Qt/src/native` 候选计算库。候选库没有接入正式 `FreeShipQt.exe`，也不会替换旧程序、试点或正式 Qt 的 `dist`。测试分别调用 `build-native.ps1` 和隔离 Pascal 源码副本，旧平面、样条文件在编译副本中仍须保持原哈希。报告前后检查原源码、模型、旧程序、试点源码及两个已交付目录的文件哈希。

输入在 `native_geometry_cases.py`，C++ JSON 探针在 `qt/native_geometry_tests.cpp`，旧核心探针为 `NativeGeometryProbe.pas`。检查自然三次样条、非等距弦长、重复点、零长度、折角、端点、越界参数、小间隔分支、正负缩放平面、非轴向平面，以及固定随机种子的多尺度小样本。旧核心的参数节点和两侧相邻 Single 值也纳入查询。独立解析例题在固定的可精确表示参数上严格检查；其他位置的实数公式与 Single 算术差异单独记录，不能靠改容差变成通过。一阶导数沿用旧核心的有限差分行为，不能冒称解析导数。

默认三个模型提供选定的真实剖面样条；没有预设剖面的模型，使用旧核心按明确的固定切面生成测试交线。`model-oracle/models.json` 记录原有、补充、可用及选取数量，不宣称覆盖模型的全部曲线。导出的点、折角和采样数再交回 Pascal 构建样条，必须复现原对象结果；随后才与 C++ 结果比较。

有限输出值通过 JSON 重构为 Single 后精确比较，没有数值容差。JSON 不保证保留负零符号，报告不称原始内存位完全相同。交点数量、次序、重复交点和重复调用清空都须保留；缓存失效、失败修改、失败重建恢复、非有限输入与溢出拒绝另有直接 API 检查。零法向、空/单点样条和零采样求交的新安全行为与旧核心有明确差别，分别列为 `review`，不会混成兼容通过。旧核心没有对应的直线求交接口，使用独立几何例题验证；单点旧参数数组没有可读取内容则标为 `not_run`。

每项使用 `pass` / `fail` / `review` / `not_run` / `error`，详细差异和最大绝对误差、Single ULP 距离写入 `comparisons-*`。任何未处理差异都不构成可直接替换生产内核的结论。当前未做成对的纯内核微基准；进程、JSON 和编译耗时不作为内核或界面性能数字。

`--small-only` 只跑合成边界，真实模型明确标为未运行。`--runtime <私有运行时>` 和 `--reuse-probe <旧核心探针构建目录>` 可复用已构建产物，必须通过当前源码、测试、适配器及程序哈希校验；默认从当前源码重新构建。`--model <FBM>` 可重复指定，所有输入只读，产物写入新报告目录。

### C++ 控制网与细分内核验证

```powershell
python -B run_tests.py --suite qt-native-subdivision --output reports/<新目录>
python -B native_subdivision.py --output reports/<新目录>
```

`native_subdivision.py` 调用 `build-native.ps1 -Target native_subdivision_tests`，独立编译当前候选内核，并在旧 Pascal 源码的私有副本中构建 `NativeSubdivisionProbe.pas`。旧细分、样条和几何算法 include 文件保持原字节；只增加只读观察属性、无界面运行适配和错误消息转异常。候选库仍没有接入正式程序。原源码、模型、旧程序、试点源码及两个交付目录都在测试前后核对哈希。

输入保留完整计算控制网：点坐标与类型、原序边与折痕/曲线归属、有向控制面、图层字段、有向控制曲线，以及松散点边。它不代表完整 FBM 编辑状态；选择、锁定、锚定、背景图、撤销历史等仍属于后续迁移范围。

第 0 级保留原控制点序号；更高层的编号来自“控制面原序 → 子面原序 → 面的有向顶点序”的首次引用，再补有向曲线链。比较完整的面环、共享点身份、边邻面、朝向、折痕、曲线、图层、松散点边和包围盒，不按坐标排序，也不合并位置重合的点。旧内核未被面或曲线引用的分配对象数量只作诊断，不能混成输出网格点数。旧 `Surface.Min/Max` 与引用网格包围盒含义不同，分别记录。

小例同时运行 Quad/Triangle 和 Catmull–Clark：三、四、五边面，开口/闭体、折痕、角点、混合面，三/六/九价三角扇、四价四边形峰值、反向面、位置重合但不相连的网格，以及附面/独立/闭合/共享节点的曲线。每级新建实例，同时检查同实例正序、逆序请求和改点失效；升级失败须保留低级缓存并可恢复。小例通常检查 0–2 级，曲线补到 3 级，闭立方体和开放自由曲线补到 4 级。默认盒体、合成曲面和 9a 模型按各自保存的模式完整比较 0–3 级；没有宣称三模型两个模式都已覆盖。

独立数学例题使用边界折痕权重、Loop 三/六价中心峰值 7/16 与 5/8，以及 Catmull–Clark 四价中心峰值 9/16。[Stam 与 Loop 的细分原文](https://www.josstam.com/_files/ugd/cf1fd6_3c2078555e64467497d2d5ace85a4fec.pdf)给出了相关平滑权重。实数公式与新旧共同的 Single 算术差异仍列为 `review`，不会因为与旧程序相同就称数学上精确。新旧有限数值按 JSON 重构 Single 后要求完全一致，不设置容差；拓扑与整数身份分开严格比较。

三个模型先直接观察原 FBM 加载对象，再导出控制网、由真实旧加载逻辑重建，检查输入字段和逐级完整结果自洽。旧对象第 0 级可能残留调试用细分级别字段，这类诊断差异明确列出；不能据此放宽实际坐标、拓扑或属性比较。混合附面/游离控制曲线及跨越无面边连接的附面曲线目前明确拒绝，尚未迁移。当前结果只覆盖独立计算，不代表编辑、导出或桌面验收。

报告分别记录 `pass`、`fail`、`review`、`not_run`、`error`，`comparisons-*` 保存完整差异及数值统计。`--small-only` 明确跳过模型；`--runtime`、`--reuse-probe` 仅复用当前源码、观察器、适配器和二进制哈希都匹配的私有产物，默认从当前源码重建。

### C++ 剖切与交线拼接内核验证

```powershell
python -B run_tests.py --suite qt-native-sections --output reports/<新目录>
python -B native_sections.py --output reports/<新目录>
```

这组构建独立的 `native_sections_tests.exe`，调用候选的平面剖切、拓扑连接和曲线拼接接口，再接入已有原生样条采样。候选仍未接入正式程序。旧源码、模型、旧程序、试点源码及两个交付目录在测试前后核对哈希。默认从当前源码分别构建 C++ 和 Pascal 探针，产物只写入新报告目录；`--runtime`、`--reuse-probe` 只有在源码、适配器、测试和二进制哈希完全匹配时才可复用。

`NativeSectionsProbe.pas` 调用原 `Surface.IntersectPlane`、`CalculateIntersections` 和 `JoinSplineSegments`。原函数的局部交线无法从公开接口读取，因此私有编译副本插入只读观察回调，保存原始交段、按拓扑连接后的链、几何拼接后的链和最终结果。剥除插入内容须逐字节恢复原算法文件，并保存差异文件。测试还用同一二进制关闭全部观察回调，核对最终曲线、样条值、导数和是否有交线都完全相同。

比较保留交段顺序、端点方向、重数、源面和源边身份、折角和采样数，不以排序坐标代替拓扑。整数字段严格比较，浮点值按 JSON 重构 Single 后精确比较，没有数值容差。新增的有符号交段来源另查数量、方向、每个共享点的折角 OR；原记录没有的顶点身份不伪造旧版对照。独立进程重复运行也须给出完全相同的结果。

小例包括平面方形、闭立方体、站线/纵剖/水线/斜面、切点与共面边、共面面内部边排除、分支、位置重合但互不相连的拓扑、图层过滤、微小分量、缝隙和四种端点拼接方向。`1e-5` 阈值附近的相邻 Single、单精度拼接阈值的相等与两侧、包围盒平方阈值 `1e-3` 两侧有明确样本。独立几何预期与旧版兼容分开：原有拼接阈值 `0.01` 比的是距离平方，可能跨过约 `0.1` 的缝；多条结果时的小分量删除、重合不相连拓扑合并都保留为 `review`。

前插拼接丢失共享折角的旧错误有两份最小反例。候选保留两端折角的 OR，坐标、次序与其余字段仍须一致。改变后的每个样条采样值和导数必须与“独立改正的点列交回原 Pascal 样条”完全一致；折角后的水平段还单独检查不再弯出直线。只有这些已证实的差异可列为修正 `review`，其他差异仍会失败。图层样例中实数面心 `Y=4` 与新旧共同的 Single 结果 `4.000000476837158` 也原样保留，不修改解析期望。

默认盒体、合成曲面和 9a 三模型使用各自保存精度对应的细分层级。运行全部已有剖切平面；没有保存剖面的模型补明确标记的三个轴向中间平面，每模型再加外部平面。先比较直接加载原 FBM 与导出控制网后通过原加载器重建的完整结果，再作 C++ 对照。当前不包含上层 `Simplify`、船体镜像显示、静水力或导出流程，不能据此声称这些功能已经迁移。

报告分别列 `pass`、`fail`、`review`、`not_run`、`error`。旧版错误的部分记录 `Move` 分支尚未被合法简单面触发，同边连接距离恰在 `1e-4` 两侧的输入也未构造出来，两项明确为 `not_run`；平面侧值、闭合判定的阈值样本不能替代它们。`--small-only` 另会把真实模型列为未运行。任何 `review` 或未覆盖项都不构成可直接替换生产内核的结论。


## C++ 原生交线简化（独立候选）

```powershell
python -B run_tests.py --suite qt-native-simplify --output reports/<新目录>
python -B native_sections.py --simplify-only --output reports/<新目录>
```

复用已有剖切测试程序和原 Pascal 探针，按当前源码隔离构建，不更新交付程序。
覆盖明确的加权删点入口、端点和折角保留、严格阈值边界、冷/热缓存、无效输入、
失败回滚与三个已有剖切例的回归。`fixtures/native_simplify_models.json` 固定了
三个模型的 30 条原始交线输入（3,874 点），附原报告和模型哈希；每次重新用当前
原 Pascal 计算简化结果，不把候选输出写作预期。模型输入不依赖历史报告目录存续。

比较保留点的顺序、折角、采样段数，以及九处位置和一/二阶导数；Single 重构后
精确比较。只有两个有独立参考曲线证据的例子可记为预期差异：冷缓存不执行简化、
大尺度下删掉折角。旧算法短邻边可能丢掉长折返的例子单列 `review`，不将兼容相同
当成精度保证。候选只提供显式接口，尚未用于正式型线图或导出。

直接入口支持现有 `--runtime`、`--reuse-probe`，仅复用哈希与当前源码匹配的构建。
退出码沿用剖切套件：0 通过、1 失败、2 待看/未测、3 执行错误；统一入口的
`partial` 不等于全部通过。


## C++ 型线图曲线投影与镜像（独立候选）

```powershell
python -B run_tests.py --suite qt-native-linesplan --output reports/<新目录>
python -B native_sections.py --linesplan-only --output reports/<新目录>
```

复用剖切构建器和测试程序，验证 `GetValues` 采样、侧视/前后体/俯视坐标、镜像、
中剖面筛选，以及导出后再次显示。原参考来自当前源码实际活动的 `DrawSpline`
和 `AddSpline`：前者逐字提取并在像素投影前采集坐标，后者加入四个只读回调，
移除回调后必须还原原字节。提取源码及哈希保存在 `oracle-build`。

默认 45 组输入含 15 个小例和已有三个模型的 30 条冻结交线。模型输入取自
`fixtures/native_simplify_models.json`；这组明确指定 100 点采样以核对显示与
旧 DXF 顶点，同时用小例覆盖不同密度及导出状态污染。

原俯视显示将 X 写成 Y、镜像符号使用 0；只有同时符合独立 XY 预期、实际旧
DXF 顶点和明确的旧错误公式，差异才记为有意修正的 `review`。导出改变采样数、
稀疏采样漏过角点另列 `review`。坐标按 Single 重构精确比较，不设新增容差。
该套件只覆盖曲线坐标；不声称完整型线图、完整 DXF 文件或桌面操作已验收。
`--runtime`、`--reuse-probe` 和退出码沿用剖切套件，只有哈希匹配才复用构建。

## C++ 完整型线图与实际 Qt 集成

```powershell
python -B run_tests.py --suite qt-native-app --output reports/<新目录>
python -B native_linesplan_app.py --output reports/<新目录>
```

`native_linesplan_app.py` 调用实际常驻服务和 Qt 应用，对盒体、合成曲面、9a 的原始、
工程设置、单位、图层、批量移动、撤销/重做和保存重开逐阶段比较完整场景及完整 DXF。
旧 Pascal 路径明确关闭原生选项，作为独立参考；生产 Qt 必须报告实际使用 C++。

场景填色的坐标、颜色及绘制次序精确比较；标签内容、字号、颜色和次序精确比较。
俯视投影修正、保留全部折角和文字锚点变化需附明确证据，单列 `review`。DXF 使用
原完整实体比较器，不改数值容差。重新打开后的新文档 ID 不要求不变，但精确几何、
带属性的连接关系和面绕向仍须一致；原有同文档稳定 ID 约束不放宽。

附加样本覆盖斜剖线、系统颜色、无剖面和隐藏全部图层；缓存检查覆盖独立工作进程
之间的命中、损坏重建、坐标/吃水/颜色失效、容量清理和无关文件保留。组件故障在
私有运行副本中注入，核对模型、撤销、完整输出未变，并执行后续修改和撤销。
所属进程已退出时必须停止生成；运行过程中终止所属进程尚无确定性故障注入。

支持 `--reuse-build` 和 `--runtime`，只复用与当前源码、测试及适配器指纹一致的构建。
报告保留这些指纹、全部差异证据和实际运行入口。基础几何仍用原独立套件验证；
`qt-migration` 保留冻结的旧版比较，并新增当前原生场景与旧场景的逐项比较。
状态和退出码与 Qt 迁移套件一致：0 通过，1 失败，2 有待看/未测，3 执行错误。
离屏图和隐藏事件不构成实际桌面体验验收。

## 控制网拓扑与 StudioLight 主界面

```powershell
python -B run_tests.py --suite qt-topology --output reports/<新目录>
python -B qt_topology.py --output reports/<新目录>
```

逐模型执行新增点、拆边、建面、删除、锁点、撤销分支和保存重开，对照旧参考和
C++ 生成路径的完整场景、DXF 及带属性的控制网。中点、有向顶点、边重数、删除
后的编号拒绝和失败回滚另有独立断言。真实 Qt 应用的 `--verify-topology` 走实际
菜单动作、新点/建面/图层对话框、删除确认与取消、锁点和保存链路；最终 FBM 另由
旧参考路径完整回读。`qt-editing` 的旧格式夹具还检查锁定状态的版本提升、历史和保存。
原有 `qt/editing_tests.cpp` 增加稀疏编号的边、面选择测试。

1044 张 StudioLight PNG 与原资源逐张核对，采集主窗口和窄窗口图像，检查视图
收起/恢复和“更多”收纳。用户明确要求保留原 UI，四视图排列因此有对应基准检查；
图像、动画及剩余窗口的整体一致性仍记 `review`，实际桌面输入仍记 `not_run`。
不因顶栏存在就把整套 UI 标为已迁移。

可用 `--reuse-build`、`--runtime` 复用哈希完全匹配的隔离构建。交付复核使用
`--verify-deployment <运行目录> --workflow-reference <已通过的完整报告目录>`，
直接执行指定目录的程序，不复制或修改交付文件。退出码与 Qt 迁移套件一致。

## 剖面线与图层增删

```powershell
python -B run_tests.py --suite qt-sections-editing --output reports/<新目录>
python -B qt_sections_editing.py --output reports/<新目录> --reuse-build <桥接构建> --runtime <Qt运行目录>
```

新增组覆盖四类剖面线的添加、批量添加、改距、删除、清空、重复操作、非法输入回滚、
撤销/重做和保存往返。平面系数及曲线平面方程有独立预期；C++/旧参考路径比较完整
场景与 DXF。图层新建、删空层、清理空层、非空保护及删除后编号不复用也进入测试。
曲线缓存故障注入运行在独占目录，检查重新生成及全部输出一致。

`FreeShipQt.exe --verify-sections` 使用实际顶栏动作、表格编辑、确认/取消和图层
菜单回调，结果写入私有报告。`qt/editing_tests.cpp` 另检查曲线数据的错误平面、
重复/缺失编号、非法数值和不完整路径被拒绝且不替换原数据。

部署复核参数仍为 `--verify-deployment <运行目录> --workflow-reference <报告目录>`。
已知显示差异与 UI 外观记为 `review`；曲率梳尚未迁移，桌面操作未自动执行，分别
记 `not_run`。0/1/2/3 退出码对应通过/失败/部分覆盖/执行错误。

## 四视图辅助线、坐标与动画

```powershell
python -B run_tests.py --suite qt-overlays --output reports/<新目录>
python -B qt_overlays.py --output reports/<新目录> --reuse-build <桥接构建> --runtime <Qt运行目录>
```

默认三种模型执行真实隐藏应用的显示开关、坐标轴、动画中导出、导航中断，再执行
移动、撤销、重做、保存和重开。逐阶段比较原参考的完整场景、DXF、带属性几何和
静水力标记；盒体排水量/浮心另有独立预期。`qt/overlay_tests.cpp` 直接测试产品的
线段空间索引、布局、扩散图、逐点计时、暂停和导出隔离，并采集四视图及密集坐标
图像。性能只记录观测值，坐标平移分开记录首次与后续帧，不声称实际桌面帧率。

入口按动作元数据发现，不写死菜单层级；候选缺失、不唯一或禁用时记 `not_run`。
实际桌面操作未执行、整体视觉一致性待核对，分别记 `not_run` 和 `review`。
完整测试源码保留在 `qt_overlays.py`、`qt/overlay_tests.cpp`，实际动作流程位于
`FreeShip-Qt/src/app/overlay_workflow.cpp`。部署复核仍使用
`--verify-deployment <运行目录> --workflow-reference <完整报告目录>`。

## 诊断脚本与测试输出保留

`maintenance/overlay_history_probe.py` 用于定位静水力缓存与型线图状态污染，保留
四个阶段的原始输入和移动/重做字段差异。它是一次排查工具，完整应用回归已承担
日常检查，因此不默认重复运行；源码和用法继续保留：

```powershell
python -B maintenance/overlay_history_probe.py --runtime <运行目录> --model <FBM> --output <新目录>
```

报告输出可以清理，测试源码、必要模型、被引用的基准和最近有效报告必须保留。
`maintenance/artifact_retention.py` 只生成计划：追踪源码/文档及报告中的引用，保留
各命名测试族的最新运行和最新有效运行，未知结构保守保留。执行器默认也只验证。

```powershell
python -B maintenance/artifact_retention.py --output maintenance/retention-new.json
./maintenance/apply_artifact_retention.ps1 -Plan maintenance/retention-new.json
# 核对清单并获得删除授权后，才追加 -Apply。
```

执行器只接受 `reports` 下已解析的直接子目录，拒绝链接路径，重新核对报告指纹、
文件数及大小，再用 PowerShell 删除并记录实际结果。2026-11-01 的授权清理共
删除 301 个目录，释放 19.18 GiB，保留 554 个目录；计划与执行回执为
`maintenance/retention-20261101.json` 和 `maintenance/retention-20261101.json.result.json`。
这是该次清理的结果，不是后续新报告生成后的实时目录数。

## Qt 背景图片与标定

```powershell
python -B run_tests.py --suite qt-backgrounds --output reports/<新目录>
python -B qt_backgrounds.py --output reports/<新目录> --reuse-build <桥接构建> --runtime <Qt运行目录> --reuse-probe <原核心观察器构建>
```

生产流程走实际背景菜单、文件选择框、标定/混合对话框，覆盖导入、取消、隐藏、
拖动、比例校准、撤销/重做、导出、清除和保存重开。入口按动作属性发现，缺失、
禁用或不唯一时保留候选并记 `not_run`。模型、场景、剖面和完整 DXF 逐阶段比较；
独立原核心探针读取保存的图片字节及全部标定参数，另含 1.80 旧格式升级反例。

服务测试覆盖三个视图、透明色、混合、单位转换、无变化编辑保留重做、过期请求、
解码失败及发布失败回滚。C++ 用例有独立标定公式、中文路径 BMP、JPEG 无损保留、
拖动只提交一次、取消、空图边框保护、缩放、平移和高 DPI 图像。原生平台的 GPU
用例仅创建离屏 OpenGL 表面，检查船体外透明、船体内颜色与不透明度；不可用时
记 `not_run`，不把软件回退算作 GPU 通过。

源码为 `qt_backgrounds.py`、`qt/background_tests.cpp`、`QtEditingFixture.pas` 及
Qt 产品内 `src/app/background_workflow.cpp`。套件复用构建前核对源码和二进制指纹。
窗口与菜单使用离屏平台，显式载入本机 Windows 字体，避免截图缺字；没有桌面操作。
视觉差异保留 `review`，实际鼠标和原生文件对话框验收为 `not_run`。

完整运行包的直接复核：

```powershell
python -B qt_backgrounds.py --verify-deployment <运行包目录> --workflow-reference <完整回归目录> --output reports/<新目录>
```

该命令直接运行目标程序，核对包内容与来源，再比较实际背景工作流的完整场景、
DXF 和剖面文件。源程序、用户模型、旧正式版及 QtPilot 保持原指纹。

Qt four-edge label gravity: `python run_tests.py --suite qt-label-boundaries --output reports/qt-label-boundaries`. Compiles only private copies of production label_layout.cpp/.h, checks both grid label kinds at all four edges, full boundary clipping, retained offscreen history, continuous return and restored full values; JSON and image strips are retained. `python qt_label_boundaries.py --legacy-snap-control --output reports/qt-label-control` injects the previous offscreen snap into the private copy and must fail top/bottom attraction tests. No user model/settings writes; synthetic placements and hidden widgets are not physical mouse/desktop acceptance. Full model overlay regression remains `qt-overlays`.


## Qt 启动恢复、半船和曲率梳

统一入口 `python -B run_tests.py --suite qt-startup --output <新的报告目录>` 与
`python -B run_tests.py --suite qt-display --output <新的报告目录>`。
可传 `--reuse-build ../FreeShip-Qt/build/display-service-01 --runtime ../FreeShip-Qt/build/display-runtime-01`，仅复用哈希完全匹配的构建。
显示套件可加 `--reuse-probe reports/qt-display-20261101-a/oracle-build`，保留直接调用旧核心的独立探针。

- `qt-startup`：独立进程间的上次文件恢复、显式参数优先、另存为、失败打开、移动/删除/损坏、损坏或不可写配置、最近十项去重。全部使用私有配置和模型副本；支持 `--installed <运行包目录>` 直接验证安装路径。
- `qt-display`：真实动作/曲率复选框，切换、撤销、保存重开；三种模型逐阶段完整场景和 DXF 保持原字节。半船分割、填色、选择、框选、整组拖动和懒生成有独立预期。
- 曲率测试直接比较当前 Plus 的 `TFreeSpline.Curvature/Value` 和 C++ 输出，含直线、零长、重复点、折角、空间曲线、四档精度及真实求交样条。共享探针新增 `curvature` 模式，完整源码保存在 `QtEditingFixture.pas` 和 `qt_editing_fixture.py`，C++ 入口为 `qt/display_tests.cpp`。
- `qt-display --verify-deployment <目录> --workflow-reference <完整成功报告目录>` 直接运行完整安装包，按来源清单及阶段产物核对，不复制替换被验目录。

动作缺失或有歧义时仍为 `not_run`，显示样例的完整视觉一致性为 `review`。没有将隐藏窗口和离屏鼠标事件当作真实桌面验收。测试结果和阶段图可清理，测试源码及被引用的旧核心探针需保留。

## Qt 控制曲线与流线

统一入口：`python -B run_tests.py --suite qt-auxiliary --output reports/<新目录>`。
若要复用当前构建，直接调用 `qt_auxiliary.py`，传入
`--reuse-build ../FreeShip-Qt/build/curves-service-03 --runtime ../FreeShip-Qt/build/curves-runtime-01 --reuse-probe reports/qt-curves-core-probe-20261101-d`。
复用前会核对源码和二进制指纹，源码变化后须重新构建。

套件运行实际应用动作和视图取点回调，覆盖建曲线/流线、删除、撤销、文件往返，
并检查闭合链、不相连链、分叉分链、依赖边删除、单位转换、精度切换和失败回滚。
独立旧核心探针直接读取并重算流线；`flow` 模式按请求列表观察三种投影的流线，
`inspect` 模式读取表面重建实际使用的控制曲线样条，不调用另一个同名重建入口。
曲线/流线数据、型线图与 DXF 分别比较，不能用视图截图代替几何一致性。

源码保留于 `qt_auxiliary.py`、`qt/auxiliary_tests.cpp`、`QtEditingFixture.pas`、
`qt_editing_fixture.py` 及 Qt 工程的 `src/app/auxiliary_workflow.cpp`。
入口发现失败为 `not_run`；图片和整体视觉一致性仍为 `review`，不做桌面自动化。
安装包可用 `--verify-deployment <目录> --workflow-reference <完整回归目录>` 直接复核。

## Qt 变换、挤出和镜像

统一入口：`python -B run_tests.py --suite qt-advanced --output reports/<新目录>`。
复用已核验的构建时，直接运行：

```powershell
python -B qt_advanced.py --output reports/<新目录> --reuse-build ../FreeShip-Qt/build/advanced-service-03 --runtime ../FreeShip-Qt/build/advanced-runtime-01 --reuse-probe reports/qt-advanced-core-probe-20261101-g
```

覆盖真实 Qt 动作和五个参数窗口、取消、移动/缩放/旋转、边界挤出、镜像合并、
共享点、面朝向、属性、撤销、保存重开、整船主尺度/标记/四类剖面、约束、失败回滚。
`QtEditingFixture.pas` 的 `advanced_math`、`advanced_core`、`advanced_seed` 模式
直接调用原核心生成独立预期和私有样本，不经新编辑适配器。保存重开后的原生记录
序号不稳定，因此按精确坐标映射关系、保留面的朝向和属性进行比较；会话内的 ID
仍单独检查。动作按元数据发现，不冻结菜单位置、名称或控件布局。
针对保存排序后挤出漏设角点的问题，独立检查新点类型和未涉及旧点的属性，另开
三个应用进程重复同一操作，比较精确几何、完整场景、DXF 和剖面，覆盖跨进程一致性。

`legacy_exports` 模式保留原绘制异常、短采样计数和独立 DXF。当前 9a 编辑样本中
原 `GetValues` 的 `Fragments=1` 导致除零，完整旧画面对照记 `not_run`，其 DXF
仍逐实体精确匹配。未知原核心错误不能套用这一例外。调试探针保留符号，栈定位
写入报告目录；原产品文件不改。

`--verify-deployment <运行包目录> --workflow-reference <完整回归目录>` 直接运行
目标程序，核对来源和阶段产物。整体视觉为 `review`，真实桌面输入为 `not_run`。
完整源码在 `qt_advanced.py`、共享探针和 Qt 的 `src/app/advanced_workflow.cpp`。

## Qt 模型编辑补全

统一入口：`python -B run_tests.py --suite qt-modeling --output reports/<新目录>`。
复用当前构建时直接运行 `qt_modeling.py`，可传 `--reuse-build`、`--runtime`、
`--reuse-probe`；复用前核对源码、编译产物和原核心探针指纹。

`qt_modeling.py`、`qt_modeling_extra.py` 覆盖点折叠、边合并、点挤出、连接点、
凹面的对角线、约束传播与循环、点组、网格、圆柱、平面和图层求交、分组和组装、
负舷删除、法向、标记、Lackenby、撤销、保存回读及发布失败恢复。
矩形船体积分、网格端点、图层交点、旋转轴心使用独立几何预期；原核心探针
另行读取 FBM 的约束、点属性及点组。法向比较保留全部精确向量及其重数，
只忽略原核心按指针遍历产生的顺序，不放宽数值容差。

Qt 的 `--verify-modeling <请求.json> <输出目录>` 观察实际动作及参数窗口。
请求中的选点按顺序逐次送入选择回调，避免把无序集合误当作约束的首尾点。
`qt/migration_tests.cpp` 另测组中心拖动、单点选择、标记显示、曲面法向和只读检查。
截图与元数据保留为证据，视觉一致性记 `review`，实际桌面输入记 `not_run`。

`--service-executable <exe>` 仅供隔离后端诊断，明确将完整运行包与独立旧核心
对照记为未运行；不能拿这个入口的通过替代完整套件。诊断源码也保留在统一目录。

`qt_modeling.py --verify-deployment <完整运行包目录> --workflow-reference <完整回归目录>
--output <新目录>` 直接运行被验目录，复核完整文件清单、来源指纹、实际动作和
逐阶段场景/DXF/剖面。测试仅写入私有输出目录，不复制替换被验程序。

## Qt 静水力与稳性

统一入口：`python -B run_tests.py --suite qt-hydrostatics --output reports/<新目录>`。
直接运行 `qt_hydrostatics.py` 可用 `--reuse-build`、`--runtime`、`--reuse-probe`
复用指纹完全匹配的构建；`--service-executable` 仅为后端诊断，未执行的界面另记。

`qt_hydro_cases.py` 保存箱体的独立体积、浮心、GZ、纵向力矩、坐标平移和单位预期，
以及漏水、失败、参数往返等边界。`QtEditingFixture.pas` 的 `hydro` 模式直接读取
原积分器，支持给定吃水或显式平面；不调用新平衡求解器生成“预期”。
Qt `--verify-hydro <request.json> <输出目录>` 按动作元数据发现实际菜单、参数控件
和按钮，验证计算、取消、旧结果保留、参数保存与四种导出，并采集截图和实际字体。
PDF 在安装 PyMuPDF 时解析文本并渲染首页，否则相应检查为 `not_run`。

`--verify-deployment <完整运行包> --workflow-reference <完整回归目录>` 直接验证
该目录，核对来源、二进制清单、真实回调及完整型线图/DXF。原生桌面输入、字体和
整体视觉仍单独验收；原核心非对称面积数据的已知限制单列 `review`。

## Qt 阻力与推进

2026-11-03：`qt-performance` 默认运行正式 C++ 文档服务，并包含
`qt_native_propulsion.py` 的五项推进回归。真实求解器对照与固定协议样本分别报告；
CALCPROP Task1–3 无输出仍是 `not_run`，固定样本不能证明实际船舶求解正确。
旧 Task3 对固定完整协议的解析失败保留 `review`，新版曲线另外核对独立预期。
覆盖默认值、持久参数、系列联动、11 类真实敞水曲线、八材料强度结论、结果提取、
取消、服务退出时的子进程清理、坏输出及旧适配器隔离。

快速诊断可用 `python -B qt_native_propulsion.py --runtime <运行包> --output
C:/FreeShip26Tests/reports/<新目录> --protocols-only`；完整入口仍用下面的统一套件。
`qt/propulsion_solver_fixture.cpp` 只生成固定协议，不是物理求解器，不随产品发布。
`fixtures/propulsion/schema.json` 是冻结的字段契约，数值从每个 FBM 装载。
`maintenance/generate_propulsion_tables.py --check` 和
`maintenance/port_propulsion_reports.py --check` 核验资源及后处理来源。
生成模式用于维护 C++ 源码，运行时无需 Python 或 Pascal。
本轮安装记录由 `maintenance/record_propulsion_native_delivery.py` 核验后生成。

统一入口：`python run_tests.py --suite qt-performance --output reports/<新目录>`。
直接入口支持复用已验证的构建，减少磁盘占用：

```powershell
python -B qt_performance.py --output reports/<新目录> `
  --reuse-build ../FreeShip-Qt/build/performance-service-12 `
  --runtime ../FreeShip-Qt/dist `
  --oracle reports/qt-performance-oracle-build-c/PerformanceOracle.exe
```

不传复用参数时构建隔离服务、Qt 运行包及独立原计算观察器。
`--service-executable` 只查服务；`--discover-only` 观察参数契约；
`--samples-only` 跑主样本；`--branches-only` 跑滑行艇/螺旋桨系列分支。
主入口包含这些可复用检查及隐藏 Qt 实际菜单/按钮回调、取消、参数保存、导出、
保存重开和只读几何检查。界面入口按动作属性发现，缺失或歧义记 `not_run`。

`qt_performance_cases.py` 保存样本；`qt_performance_checks.py` 保存独立预期及
原求解器对照；`qt_performance_oracle.py`、`PerformanceOracle.pas`、
`PerformanceObserver.pas` 保留独立原计算入口，只改观察/初始化/私有工作目录，
不使用生产适配器计算期望值。原观察器请求使用 UTF-8 JSON，禁止以旧未初始化
数据作数值真值。`solver_launch_probe.py` 的 ASCII 目录诊断已被主套件调用，
也可单独传 `--executable`、`--inputs`、`--output`；`--enter` 用来复现 PropPred
完成后等待回车的问题。重跑前不复制要比较的旧结果，避免假对照。

`qt-performance-final-b` 为 627 pass、0 fail、1 review、4 not_run、0 error。
三个 not_run 是指定 CALCPROP 在独立进程无输出，另一个是真实桌面验收；窗口
整体保真仍 review。其余未实现分支及工程数值认证边界见 Qt 的 `PERFORMANCE.md`。
截图采用隐藏 Windows Qt 窗口，PDF 解析文字并渲染检查，不使用桌面自动化。

正式目录核验：`qt_performance.py --verify-deployment <运行包> --workflow-reference
reports/qt-performance-final-b --output reports/<新目录>`，运行该目录自身并核对
源码/二进制清单、计算坐标、CSV、前后场景和 DXF，不通过复制新二进制代替验收。

## Qt 板材展开

统一入口：`python -B run_tests.py --suite qt-plates --output D:/FreeShip26Tests/reports/<新目录>`。
2026-11-03 起默认核对 C++ 文档服务的展开计算，旧服务仅作独立对照。
可复用构建以节约磁盘：

```powershell
python -B qt_plates.py --output reports/<新目录> `
  --reuse-build D:/FreeShip26Tests/build/plates-native-bridge-01 `
  --runtime ../FreeShip-Qt/dist `
  --oracle reports/qt-plate-oracle-build-c/QtPlateFixture.exe
```

`--service-executable` 只诊断服务，不替代 Qt 窗口验收；所有复用构建仍校验指纹。
`qt_plate_fixture.py` 和 `QtPlateFixture.pas` 保存独立原算法样本与观察入口；
`qt_plates.py` 保存几何预期、剖面退化样本、原结果对照、显隐、取消、文件保护、
完整轮廓、DXF 独立读取和 PDF 物理比例/分页检查。没有临时丢弃的诊断代码。

`qt_native_plates.py` 已接入同一套件：四档精度、确定性、点编辑重算、撤销重做、
私有旧进程退出后继续计算、防止退回 Pascal、运行中取消、输出失败重试和历史
FBM 版本。计时分别记录 C++ 计算及新旧服务的完整请求，不作为硬件无关的门槛。
当前安装凭据为 `maintenance/plates-native-installed-20261103.json`；下列报告是
2026-11-02 窗口迁移的历史记录。

完整报告 `qt-plates-final-c`：423 pass、0 fail、1 review、1 not_run、0 error。
颜色和单位按模型值检查；显示精度不改变输出数值。完整视觉为 review，真实桌面
操作及实体打印为 not_run；只使用隐藏 Windows Qt 实例，不使用桌面自动化。

正式目录核验：`qt_plates.py --verify-deployment <运行包> --workflow-reference
reports/qt-plates-final-c --output reports/<新目录>`，直接运行被验目录，核对完整
清单及实际计算/导出/模型不变，不用复制后的另一份程序代替验收。

## Qt 模型交接

统一入口：`python -B run_tests.py --suite qt-exchange`。
测试源码：`qt_exchange.py`、`qt/exchange_tests.cpp`；实际产品的隐藏回调观察在
`FreeShip-Qt/src/app/exchange_workflow.cpp`。固定文件、错误样本及独立格式解析器都保留，
不能以新读写器互相读通作为唯一正确性依据。

复用已有构建以节约磁盘：

```powershell
python -B qt_exchange.py --output C:/FreeShip26Tests/reports/<新目录> `
  --reuse-build ../FreeShip-Qt/build/exchange-service-04 `
  --runtime ../FreeShip-Qt/dist
```

`--verify-deployment ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/<新目录>` 直接核验并运行
指定目录，不复制整份运行包。输入模型、导入文件和输出仍在隔离环境。

最终报告 `qt-exchange-final`：277 pass、0 fail、1 review、1 not_run、0 error。
涵盖 OBJ 正负索引/凹面、STL 两种编码与伪 `solid` 头、FEF 原控制属性、单位和镜像，
并通过实际菜单完成导入、导出、FBM 保存重开。9a 的 FEF 往返同时检查控制网和生成曲面。
文件选择器、真实桌面及外部 CAD 产品验收未运行；完整视觉仍为 review。

最终安装包补齐 FEF 单位提示后，`qt-exchange-delivery` 为 280 pass、0 fail、
1 review、1 not_run；同一安装目录的基础编辑、撤销、保存重开为 85 pass、0 fail、
4 not_run。详见 `maintenance/exchange-installed-20261102.json`。

## 测试输出搬迁记录

2026-11-02 将 17 个历史报告目录（19.24 GiB）从 D 盘移到 C 盘。
`maintenance/relocate_reports.ps1` 先复制、逐文件核对 SHA-256，再释放原副本；
`maintenance/finalize_report_storage.ps1` 将中转目录移到 `C:/FreeShip26Tests`，
核对文件数量及报告哈希，并重建旧路径联接。两者是保留的一次性存储维护脚本，
不是回归测试，不应在日常测试中自动执行。
清单和原文件哈希保存在 `C:/FreeShip26Tests/maintenance/relocation-20261102`，
本项目保留 `maintenance/test-output-storage-20261102.json` 作为索引。
`maintenance/record_exchange_delivery.py` 核验本轮安装清单、报告和存储路径后生成
两份交付索引，同属一次性维护工具，源码一并保留。

## Qt GHS 船体输出

统一入口：`python -B run_tests.py --suite qt-ghs`。默认位置见本文件开头。
复用构建可运行：

```powershell
python -B qt_ghs.py --output C:/FreeShip26Tests/reports/<新目录> `
  --reuse-build C:/FreeShip26Tests/build/cad-service-02 `
  --runtime ../FreeShip-Qt/dist
```

`--verify-deployment ../FreeShip-Qt/dist` 直接核验和运行正式目录。
测试源码为 `qt_ghs.py`、`ghs_reader.py` 和 `qt/exchange_tests.cpp` 的曲线探针。
箱体体积、断面面积/重心、中心线分段、三次曲线误差、单位、图层、文件保护、
实际导出/取消回调、编辑/撤销及模型不变分别验证。商业 GHS 读取仍为 `not_run`。
`maintenance/ghs_cut_diagnostic.py` 保留此次球鼻艏诊断，调用现有原生剖切探针，
输出各段端点供检查；它不自动判定缺口可接受，不作为正确性回归。

## Qt Archimedes 与 PAM 输出

统一入口：`python -B run_tests.py --suite qt-ship-formats`，默认位置见本文件开头。
测试为 `qt_ship_formats.py` 与独立的 `ship_format_readers.py`，复用 `qt_ghs.py`
的箱体和原生工作进程检查。验证 HLL/XML、APP、PAM 记录、解析面积/重心/体积、
英制/非零基线、纵向标志、中心面/上下分离轮廓、拒绝与文件保护，以及实际 Qt
导出、取消、输入修改和编辑撤销。外部 Archimedes/PAM 接收程序为 `not_run`。

复用构建：

```powershell
python -B qt_ship_formats.py --output C:/FreeShip26Tests/reports/<新目录> `
  --reuse-build C:/FreeShip26Tests/build/cad-service-02 --runtime ../FreeShip-Qt/dist
```

`--verify-deployment ../FreeShip-Qt/dist` 直接验证安装目录，避免复制运行包。
本轮没有修改 Pascal UI；Qt 的窗口验收使用已集成的生产菜单/对话框回调和隐藏
Windows 截图，不把旧 `freeship_ui_test.py` 对 Pascal 窗口的结果算到 Qt。
`maintenance/promote_ship_formats.ps1` 和 `record_ship_formats_delivery.py` 是本次
安装/交付记录工具，保留源码，不作为日常测试或自动升级入口。

## Qt 原生文档核心

`qt-document` 验证原生 FBM 结构读取、原样保存、统一事务、保存点、分支历史、
稳定 ID、故障回滚、适配器被终止后的保存和恢复，以及完整建模的实际 Qt 回调。
源码是 `qt_document.py`、`qt/fbm_codec_tests.cpp`，复用已有建模与几何独立预期。
报告记录 `phase_seconds`，区分编解码、协议、建模和界面回调的运行耗时。

为减少反复构建，统一入口新增显式复用参数（仍核对源码与二进制指纹）：

```powershell
python -B run_tests.py --suite qt-document `
  --runtime C:/FreeShip26Tests/build/document-runtime-02 `
  --reuse-build C:/FreeShip26Tests/build/document-service-01 `
  --output C:/FreeShip26Tests/reports/<新目录>
```

安装后可用 `--verify-deployment ../FreeShip-Qt/dist`，与上述复用参数二选一。
复用选项也适用于统一入口列出的 CAD、GHS、船舶格式、模型交接和基础迁移套件。
早期布局存在版本号歧义；接收格式有效不表示计算可靠或真实桌面已验收。

本次安装回归汇总为 2,999 pass、0 fail、14 review、31 not_run、0 error，
见 `maintenance/document-installed-20261102.json`。其中核心套件为 1,943 pass。
`maintenance/promote_document.ps1` 负责本次可回退的本机替换；
`maintenance/record_document_delivery.py` 核验完整安装清单、源码与报告后生成记录。
两者是一次性交付工具，不是日常回归或自动更新入口。

`maintenance/document_sections_reference.py` 保留剖面线旧基准差异的诊断：旧报告
早于样条侧数据扩展，本次从保留的升级前程序重新采集完整输出，再使用原套件逐字节
对照；没有移除字段或更改数值预期。早期失败报告也保留，用于追溯原因。

## C++ 控制点坐标编辑

`qt_point_edit.py` 已纳入 `qt-document`，验证单点/批量移动、硬锚点与锚点链、
直线投影、锁定依赖、保存重开及旧适配器拒绝代算。C++ 编解码测试新增 0–44
版本分支的坐标写入和缓存失效，对无关块逐字节核对。80 个真实文件均读取通过，
其中 77 个执行坐标写入；一个含未知尾部、两个没有控制点，保留不写。

最终核心套件为 2,193 pass，加上实际安装目录的编辑、剖面、流线、变换和静水力
回归，合计 2,877 pass、0 fail、8 review、10 not_run、0 error。记录见
`maintenance/point-installed-20261102.json`。核心已测试的二进制逐个与安装包核对，
不因复制安装再重复整套测试；受影响窗口在实际安装目录运行。

`maintenance/point_latency.py` 是可重复的诊断入口，用 `--candidate`、`--baseline`
指定完整运行目录，`--output` 指定 C 盘新目录。它交替测量三个模型的完整文档请求，
记录源码和程序指纹；不设受主机负载影响的硬性耗时门槛，不把运算时间当作桌面帧率。
`maintenance/promote_runtime.ps1` 是可回退的本机部署工具，参数是 `-Candidate` 和
`-DeliveryName`；`maintenance/record_point_delivery.py` 为本次交付核验与记录脚本。

## C++ 控制网编辑

`qt_native_topology.py` 已纳入 `qt-document`，覆盖基础拓扑 7 个命令和组合编辑
6 个命令，新旧实现逐步对照完整模型、场景、DXF 和剖面数据。包含直接拆边后
合并新点、删边拆分控制曲线、空组保留、撤销分支 ID、失败回滚及旧读取器回读。
`qt/fbm_codec_tests.cpp` 同时验证控制网索引/名称/组/约束映射及无关块保留。

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-document --runtime C:/FreeShip26Tests/build/topology-runtime-03 --reuse-build C:/FreeShip26Tests/build/topology-service-02 --output C:/FreeShip26Tests/reports/qt-native-topology-final
```

诊断单独运行可用 `python -B FreeShip-Tests/qt_native_topology.py --runtime <完整私有运行包> --output <新的C盘目录>`。
最终安装核验脚本为 `maintenance/record_topology_delivery.py`，记录验证过的报告与
二进制、源码指纹。这是交付审计，不替代回归。桌面输入和视觉一致性仍未由隐藏
Qt 回调测试证明。
这些维护工具保留源码，日常功能回归仍从统一 `qt-document` 入口运行。

## C++ 高级建模与约束

`qt_native_modeling.py`、`qt_native_transforms.py` 已接入 `qt-document`，后者同时
复用 `qt_modeling_extra.py` 的混合操作验证。新增 13 个命令：平移/缩放/旋转、
点/边挤出、镜像、面合并，以及锚点/直线约束/清除约束/共线/重合/全部解锁。
完整模型、场景、DXF、剖面和历史逐步对照原服务，不引入新的数值容差。
独立预期覆盖全部缩放记录、无关记录填充字节、标记、反序约束链、连接船体面合并
及故障恢复。两份脚本也可用 `--runtime <完整运行包> --output <C盘新目录>` 单跑。

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-document --runtime C:/FreeShip26Tests/build/modeling-native-runtime-04 --reuse-build C:/FreeShip26Tests/build/modeling-native-service-02 --output C:/FreeShip26Tests/reports/qt-modeling-native-final
```

本次安装记录由 `maintenance/record_native_modeling_delivery.py` 核对报告、源码和
实际安装二进制后生成。维护记录不替代回归；真实桌面视觉和输入仍单列验收。

## 其余建模编辑的 C++ 回归

`qt_native_attributes.py` 与 `qt_native_face_tools.py` 已接入 `qt-document`。
覆盖点属性、点组创建/编辑/解除、标记增删、按折痕分组、规则面片组装、相邻面
朝向修复，并验证原子拒绝、保存重开、无变化操作、撤销重做和当前编码。
31 张旧字符映射表在统一入口内只读核对。非流形等不能经过生产装载器的样本，
由保留源码的 `qt/face_tools_probe.cpp` 直接检验算法拒绝条件。

旧组装算法按对象地址决定分片顺序；合法的不同划分和对应导出单列 `review`，
不宣称导出完全相同。坐标、面朝向、完整三角面、逐面属性和分片覆盖独立核对。
Qt 工作流补齐三项面整理命令的实际确认、取消、撤销和重做；图层遍历顺序随
历史恢复，浮心等标注坐标也必须精确复现。

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-document --runtime C:/FreeShip26Tests/build/modeling-complete-runtime-06 --reuse-build C:/FreeShip26Tests/build/modeling-complete-service-02 --output C:/FreeShip26Tests/reports/qt-modeling-complete-new
```

两个 Python 专项可独立使用 `--runtime <完整运行包> --output <C盘新目录>`。
`maintenance/generate_fbm_codepages.py` 默认只读审计；`--write` 才生成源码。
`maintenance/record_modeling_completion_delivery.py` 核对完整回归、正式安装回调、
源码及二进制后写交付凭据。它不替代回归，也不部署或删除文件。所有测试源码
继续保留；测试输出和构建放在 C 盘。

## C++ 工程设置、图层和剖面

`qt_native_project.py`、`qt_native_definitions.py` 已接入统一 `qt-document`。
覆盖完整单位换算、材料和显示属性、图层管理、四类剖面、旧格式晋级、无变化
提交、重复字段/非法参数拒绝，以及保存、撤销和输出失败回滚。逐步比较完整
模型、场景、DXF、剖面和历史，独立预期不放宽数值容差。

`qt/project_edit_probe.cpp`、`qt/definition_edit_probe.cpp` 验证早期格式和原始
持久记录。`qt/project_units_math.pas` 核验 FPC 常量及 Single 阈值，同样保留
源码并由统一入口执行。样本、报告和构建放在 `C:/FreeShip26Tests`。
两个 Python 专题也支持 `--runtime <完整运行包> --output <C盘新目录>` 单跑。

Qt 入口复用 `qt-editing`、`qt-sections-editing` 的
`--verify-deployment <正式目录> --workflow-reference <既有有效报告目录>`。
`maintenance/record_settings_delivery.py` 核对并保存交付证据；审计不替代回归，
也不把隐藏窗口回调当作实体鼠标和完整视觉验收。

## C++ 背景图片、显示和曲线定义

`qt_native_background.py`、`qt_native_auxiliary.py` 和两个 `qt/*_edit_probe.cpp`
已接入统一 `qt-document`。完整模型、FSLP、DXF、剖面逐步精确对照；另用独立
二进制样本检验 JPEG 原字节、选中状态、版本晋级、无关记录、命名及计数上限。
覆盖分叉/闭环、稳定 ID、无变化提交、保存重开和输出失败回滚。流线定义原生，
数值追踪仍是旧只读算法，测试不会把它计为原生算法迁移完成。

```powershell
python -B run_tests.py --suite qt-document --runtime C:/FreeShip26Tests/build/auxiliary-final-package-03 --reuse-build C:/FreeShip26Tests/build/auxiliary-service-01 --output C:/FreeShip26Tests/reports/auxiliary-new
```

`qt_backgrounds.py`、`qt_display.py`、`qt_auxiliary.py --verify-deployment` 复用
冻结工作流核验实际安装程序的隐藏 Qt 回调；实体桌面另行验收。交付核验工具
`maintenance/record_auxiliary_delivery.py` 保留包指纹、报告和运行范围，不替代回归。

旧平面切割按堆地址选择顺序的诊断保留为
`maintenance/plane_oracle_replay.py --runtime <旧运行包> --protocol <已记录protocol.jsonl> --target <待核对结果目录> --output <C盘新目录>`。
它重放完整序列并保留每次结果，不改夹具或自动验收；旧实现本来就不确定，不适合
把单次新输出自动当成原生基准。已核实的第三份旧结果由统一 `qt-document` 精确检查。

`maintenance/recheck_document_construction.py --core <完整报告> --output <新目录>`
仅用于生产源/二进制及其他测试完全未变、失败仅在该对照阶段的复核。它运行整个
建模构造阶段，生成新汇总，保留原始报告、被替代的检查和每条证据来源。生产代码
改变或其他模块失败时拒绝使用；它不能替代正常的完整回归。

## C++ 四视图曲面生成

`qt_native_surface.py` 已接入 `run_tests.py --suite qt-document`，验证三角网、法线、
控制曲线样条与旧核心精确一致，包含独立箱体预期、四档精度、编辑历史、隐藏图层、
缓存复用/损坏及发布失败回滚。`qt/auxiliary_tests.cpp` 补充显示产物和快照不一致时
拒绝安装的检查。生成逻辑复用型线图的细分层，不改变几何或导出容差。

```powershell
python -B qt_native_surface.py --runtime C:/FreeShip26Tests/build/surface-package-02 --output C:/FreeShip26Tests/reports/surface-new
```

`maintenance/inspect_surface_normals.py <配对结果目录>` 是保留的只读排查入口，列出
法线差异及重合坐标处三角形；坐标相同不等于拓扑相同，因此不作自动验收或基准更新。
旧 AddPoint 的负 Y 截断在本轮明确保留，详见 Qt 的 `NATIVE-SURFACE-DISPLAY.md`。

`maintenance/record_surface_delivery.py --core <完整文档报告> --installed-report <显示入口报告>
--installed-report <曲线入口报告>` 核对实际安装文件、当前源码和测试来源，写出本机
交付记录；它不启动额外回归，也不代替桌面验收。

## C++ 文档构造和格式升级

`qt_native_create.py` 已接入 `run_tests.py --suite qt-document`。用独立版本夹具
检查全部 45 个 FBM 格式的原样、逐版和跨版转换，保留图片、计算参数和引用。
`qt/document_create_probe.cpp` 直接链接 C++，覆盖 100×100 新建网格、细分预算、
降版及未知尾部拒绝，不借助旧核心产生期望值。

服务对照覆盖现有三个模型、米/英尺/最小网格新建、双材料导入、保存二进制字段、
改点和历史、发布失败回滚。序号 0–6 另查加载、编辑升级及撤销后原字节恢复。
旧版独立路径仍作对照，不提高数值容差。`qt-exchange` 的导入服务检查现用
`FreeShipDocumentService.exe`；启动恢复和原 Qt 导入窗口仍通过生产回调验证。

```powershell
python -B qt_native_create.py --runtime ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/create-new
python -B run_tests.py --suite qt-document --verify-deployment ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/document-new
python -B qt_startup.py --installed ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/startup-new
```

专项可用 `--format-only` 单独运行格式矩阵。结果不是物理桌面验收，也不表示专业
分析已迁移。安装来源核对脚本为 `maintenance/record_create_delivery.py`，参数
`--core <文档报告> --exchange <已安装导入报告> --startup <已安装启动报告>`。
它只核对当前源码、安装文件、测试来源和既有报告，另写交付收据。


## C++ 流线追踪与专业计算设置

`qt_native_flowlines.py` 与 `qt_native_analysis.py` 已纳入 `qt-document`。
前者用独立平面/变化切向样例和修正了未赋值边参数的隔离 Plus 副本核对流线；
后者检查静水力范围、15 种计算器的输入保存、字段精度、旧格式升级、撤销与回滚，
并执行 Qt 静水力和阻力窗口中的保存设置回调。它们不代替桌面验收。

```powershell
python -B run_tests.py --suite qt-document --verify-deployment ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/document-new
python -B qt_native_flowlines.py --runtime ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/flow-new
python -B qt_native_analysis.py --runtime ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/analysis-settings-new
python -B maintenance/generate_analysis_fields.py --check
```

`qt/flowline_probe.cpp`、`qt/analysis_edit_probe.cpp` 是直接链接 C++ 的数值/文件探针。
字段映射生成器的 `--check` 核对声明来源；不带参数才重新生成，需审阅差异。
原 Plus 的 `Lines3DIntersect` 没有给输出 `Param` 赋值，流线原始输出不能直接作为
正确答案；适配器的隔离修正及独立预期详见 Qt 的 `NATIVE-FLOW-ANALYSIS-SETTINGS.md`。

交付核对：`maintenance/record_flow_settings_delivery.py --core <完整文档报告>
--exchange <已安装交接报告> --startup <已安装启动报告>`。它只核对已有证据和安装来源。

## C++ 静水力与稳性内核

`qt_native_hydrostatics.py` 已并入 `qt-document`。复用原方箱解析测试、单位/进水/
取消/设置回归，对四个船型逐字段比较旧核心报告，并停止私有旧适配进程后再次
执行 C++ 计算。固定及自由纵倾记录实际耗时和迭代次数；断言迭代不再生成截线。
原 `qt-hydrostatics` 套件的服务入口也改为 C++ 文档服务，原 Pascal 探针仍是对照。
生产 Qt 回调继续检查单工况、表格、稳性、取消和四种报告，不能据此声称桌面验收。

```powershell
python -B qt_native_hydrostatics.py --runtime ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/hydro-native-new
python -B run_tests.py --suite qt-document --verify-deployment ../FreeShip-Qt/dist --output C:/FreeShip26Tests/reports/document-new
```

`--single-only` 仅排查单工况数值，`--no-production` 跳过 Qt 回调；统一入口不使用
这两个排查开关。非对称截面/稳心指标仍标为未提供，测试容差没有扩大。

## C++ 阻力与推进流程

`qt_native_propulsion.py`、`qt_native_resistance.py` 已接入 `qt-performance`。
核对原始曲线、参数提取、保存、原求解器文件、取消、坏输出和文档版本缓存；
Qt 回调继续执行所有十五个入口并检查型线图及 DXF 不变。真实求解器限制、
原实现不可用的选项与隐藏窗口验收范围均单独记 review/not_run。

```powershell
python -B run_tests.py --suite qt-performance --runtime <候选包> --reuse-build <桥接构建> --output D:/FreeShip26Tests/reports/performance-new
python -B qt_native_resistance.py --runtime ../FreeShip-Qt/dist --output D:/FreeShip26Tests/reports/resistance-new
python -B maintenance/port_resistance.py --check
```

`qt/resistance_solver_fixture.cpp` 只产生故障协议，不是船舶求解器，不随正式程序发布。
`qt/resistance_math.pas` 和 `maintenance/resistance_math.py` 保留编译器运算精度诊断，
用 `--output <新目录>` 运行。它解释 Single/Double 舍入差异，完整曲线回归仍精确比较。
生成器不带 `--check` 时重新生成源码，应审阅差异；报告与临时构建使用当前存储配置。

`maintenance/probe_resistance_repeat.py --executable <CLEMBLOU.EXE> --inputs <solver-files>
--output <新目录> --count 12` 保留同输入重复运行诊断。`--original-executable`
直接启动原位置程序，结合 `--long-workspace` 复现长工作目录导致的非数值结果。
不把多跑几次直到成功当作回归通过；完整测试仍使用单次严格对照。
`maintenance/record_resistance_native_delivery.py` 核对完整回归、静水力、安装后计算与
启动报告，并记录实际安装文件哈希，不负责部署，也不改变未覆盖项的状态。

`maintenance/deduplicate_test_runtime_libraries.ps1` 是存储维护工具：仅把 C 盘测试区
字节相同的 DLL 和旧源码压缩包改成共享的硬链接，保留原路径、测试证据及文件内容。
不处理程序 EXE、设置、模型或构建工作目录；它不是功能回归，也不会自动运行。
`maintenance/relocate_test_output.ps1 -Source <旧测试目录> -Destination <新测试目录>`
迁移已结束的测试输出，逐文件核对后在旧址保留目录联接；不能用于正在运行的任务。

### 模型快照发布

`qt-publication` 对照完整模型快照、原生工作进程输入、场景及 DXF，覆盖三个现有
船模、历史格式、背景图片、名称/约束、新建船型和失败恢复。重复请求必须复用准备
结果；停止私有 Pascal 进程后，快照与编辑不能再发送旧适配器请求。

```powershell
python run_tests.py --suite qt-publication --runtime ../FreeShip-Qt/dist --output D:/FreeShip26Tests/reports/publication-new
python qt_publication.py --runtime <候选包> --oracle-runtime <迁移前冻结包> --output D:/FreeShip26Tests/reports/publication-frozen-new
```

不指定冻结包时使用隔离的 Pascal 入口，早期格式另做独立结构与原样保存检查。
`qt-document` 继续执行所有编辑组合和实际隐藏 Qt 回调；桌面验收另列。
`maintenance/inspect_publication_pairs.py <报告目录> --output <JSON>` 是只读差异诊断，
不修改预期，也不替代回归。图层身份比较把实数字段的 JSON `0`/`0.0` 统一表示，
数值仍精确比较。关闭半船对称后出现的水下开放边单独检查，旧无效标注保留为 review。
# 原生偏好初始化

`run_tests.py --suite qt-preferences --verify-deployment ../FreeShip-Qt/dist --output D:/FreeShip26Tests/reports/preferences-check`
检查配置叠加、主题继承、系统颜色、编码、DTA、PO 绘图文字及错误回滚。
测试逐字段对照私有 Pascal 进程，然后在私有运行包中移除该可执行文件，检查
打开、新建、编辑、保存以及真正的 Qt 启动/重开回调。所有配置修改都在私有目录，
不写入用户偏好；隐藏回调不代替桌面验收。实际安装目录只读核对清单和哈希。
调色板来源生成脚本为 `maintenance/extract_dxf_palette.py`；一次生成的是源文件，
正常运行无需原 Pascal 源码。完整快照/导出回归继续使用 `qt-publication`。


## 建模 UI 回归

`python run_tests.py --suite qt-model-ui --runtime <Qt运行包> --output <隔离报告目录>`

验证图层顺序和透明度的事务/保存、面归属、几何与 DXF 不变、独立透明合成/面积/重心、点击放置控制点、连续流线、标注和当前视图图片。真实桌面、原生打印、多屏与 DPI 单列未测。新源码保存在 `qt_model_ui.py` 和 `qt/model_ui_tests.cpp`，与既有回归一起运行。

## 镜头与光源回归

`python -B run_tests.py --suite qt-viewport --runtime <Qt运行包> --output <隔离报告目录>`

源码在 `qt_viewport.py`、`qt/camera_lighting_tests.cpp`，接入生产程序的
`--verify-viewport` 回调。用独立针孔比例、Lambert 数值和 GPU/软件对照检查
五档镜头与调光；复用透明合成、选择、标注和半船投影测试。窗口入口按动作
属性发现，缺失或歧义记 `not_run`。调光前后要求模型、场景、DXF 及 FBM 不变，
图片输出等于当前视图。真实桌面、系统文件框和纸面打印仍不在自动验收范围。

运行分别使用隐藏 offscreen 和 Windows OpenGL 环境；没有桌面控制操作。
子进程日志旁保留 `process-result.json`，记录退出码、平台和调用参数；异常退出
没有完整报告时也能查到原因。测试采集的图像耗时不是屏幕帧率。

## 特殊着色回归

`python -B run_tests.py --suite qt-shading --runtime <Qt运行包> --output <隔离报告目录>`

`qt_shading.py` 和 `qt/shading_tests.cpp` 覆盖平面/鞍面/圆柱/八面体的独立角亏，
顶点连接身份、未知边界、非流形拒判、法向、GPU/软件插值、四个投影和实际船型。
`--verify-shading` 执行生产菜单和图片导出，检查切换不编辑文档、移动点后刷新、
撤销恢复及 FBM/场景/DXF 不变。入口按动作属性发现，缺失或歧义单列未执行。
软件回退与 Windows OpenGL 均使用私有隐藏实例，不操作用户桌面。

角亏是沿用旧版的角度量，并非面积归一化曲率；边界等未知区域不能算“可展”。
离屏耗时包含图像读回，真实桌面观感和纸面打印仍需另行验收。


### 剩余文件交接回归

2026-11-04 后续扩展：Michlet 单体、单体复制双体、已有双体提取、两种波形区域、
导入原点以及公英制坐标。独立尖艏体积和逐条记录读取器核对旧文件格式；平艏作为明确
拒绝用例，不能再只检查数字便认定 Michlet 可读。当前安装记录为
`maintenance/multihull-installed-20261104.json`，维护记录器使用 `--delivery multihull`。

`python -B run_tests.py --suite qt-legacy-exchange --verify-deployment ../FreeShip-Qt/dist`

源码 `qt_legacy_exchange.py` 复用实际 C++ 交换探针、文档服务与 Qt 回调。
覆盖部件、VRML、Michlet 波高追加，部件输出、Michlet/AddMass 单体计算输入、
矩形型值表、折痕 DXF；检查单位换算、曲线/锁定点、撤销、失败不改变工程、保存重开。
数值预期使用独立箱形船及自然三次曲线，不由输出生成自己的预期。
45 份原配文件只读参与兼容性检查；退化几何明确列为 review，不删面或降精度来通过。
报告和隐藏窗口截图保留。外部接收软件和真实桌面验收另列 not_run。
统一入口按实际余量选盘，并清理本轮自有临时缓存；不用另复制一套运行包。

本批安装记录是 `maintenance/legacy-exchange-installed-20261104.json`。
`maintenance/finalize_legacy_exchange_package.py` 仅同步已测候选包的文档、来源清单与
现有本机求解器路径；`maintenance/record_legacy_exchange_delivery.py` 只核验安装文件与
完成报告后写入交付记录。两者属于保留的交付维护源码，不是功能回归或自动更新器。

### AddMass 实际计算回归

`python -B run_tests.py --suite qt-added-mass --runtime ../FreeShip-Qt/dist`

`qt_added_mass.py` 检查原求解器真实运行、独立椭球协议、矩阵 36 个值、海船手算值、
河船适用范围与零运动、工况回传、错误/取消不覆盖结果、子进程退出、生产 Qt 矩阵表与
CSV/TXT/JSON/PDF、参数文件、公英制，以及用户实际船模的只读对照。
原来的输入导出与计算入口生成的 INA 必须完全一致。测试源和故障替身均留在测试程序内，
故障替身只验证流程，不能代替真实求解器的数值结果。

`maintenance/probe_addmass.py --ellipsoid --solver <ADD_MASS.EXE> --output <空目录> --convert-unit native`
保留原始求解器协议排查过程；省略转换选项可复现本机旧程序的 CONVERT 错误。
这是诊断采集脚本，正常回归使用上面的统一入口。安装记录器为
`maintenance/record_added_mass_delivery.py`，只核验程序、源码与报告，不执行功能测试。
隐藏 Qt 窗口与真实桌面、原计算方法的工程认证分别列为 not_run / review。

### 型值网格预览回归

`python -B run_tests.py --suite qt-grid-preview --runtime ../FreeShip-Qt/dist`

`qt_grid_preview.py` 核对预览与实际文件的全部型值、独立尖艏尺寸、两舷和双体间距、
公英制、文档版本、无效参数、取消及旧结果保留，并调用 Michlet / AddMass 的实际窗口。
`qt/grid_preview_tests.cpp` 验证坐标布局、所有网格线、Qt 旋转/平移/滚轮/全图事件，
以及镜头动作不改变网格。最大网格的绘制耗时仅作隐藏控件测量，不能作为桌面帧率。
结合 `qt-added-mass` 和 `qt-legacy-exchange` 复查计算、参数文件、报告和原导出结果。
报告、截图及源码保留；统一入口只清理本轮临时型线图缓存。

安装记录使用 `maintenance/record_added_mass_delivery.py --delivery grid-preview`，
对应 `maintenance/grid-preview-installed-20261104.json`。记录器仅做交付核验，不能代替功能测试。
## 未保存船模恢复

`python run_tests.py --suite qt-recovery --runtime <Qt运行包> --output <隔离目录>`
验证完整 FBM 恢复副本、实际进程异常退出、重启选择、另存为、关闭取消/放弃、
损坏文件、文件占用导致的原子写入失败及多实例隔离。`qt/recovery_tests.cpp`
保留故障生成器，`qt_recovery.py` 独立解析恢复容器并调用正式 Qt 回调。
不操作用户桌面，不改用户船模、设置或旧程序；真实桌面和电源中断仍单列未测。

交付记录工具为 `maintenance/record_recovery_delivery.py`，只核对已完成的报告、
源码与安装包指纹，不构建、不安装、不清理文件。最终记录为
`maintenance/recovery-installed-20261104.json`。

## 常用窗口布局回归

`python -B run_tests.py --suite qt-dialog-layout --runtime ../FreeShip-Qt/dist`

`qt/dialog_layout_tests.cpp` 直接使用正式工程设置、剖面线和偏好窗口，覆盖
三种字号、正常与较矮窗口、三档缩放，检查字段可到达、分组、按钮及编辑回调。
`qt_dialog_layout.py` 同时复用原属性控件与正式 Qt 编辑/剖面线流程，保护模型、
单位转换及撤销重做结果。设置持久化由 `qt-preferences-ui` 配套核对。
所有截图、报告和测试源码保留；隐藏 Qt 事件不代表真实桌面、多屏或原生焦点验收。

## 历史输出存储约定（2026-11-09）

历史报告目录视为只读。maintenance/deduplicate_historical_outputs.py 对同盘、SHA-256 完全一致的大文件采用硬链接去重，保留所有路径、报告和样本。不要原地改写历史输出；重跑测试必须创建新目录。如确需修改历史副本，先复制到新文件并替换路径，使其脱离硬链接。当前构建、正式程序、源码和目录联接目标不在本次遍历范围内。去重明细见 maintenance/storage-dedup-20261109.jsonl。

## 保存确认与中断

保存确认回归已补充到 `qt-save-protection`：`qt_save_confirmation.py` 覆盖凭据、
实际目标文件和修订号校验；`qt/save_protection_tests.cpp` 丢弃真实服务的两次确认回复，
验证自动重试、迟到回复、暂存内容替换和备份不重复轮换。使用
`--verify-deployment <实际dist>` 可复测正式安装目录。`qt-recovery`、`qt-preview-image`
和 `qt-document` 分别保留恢复副本、预览、完整文档/编辑流程的交叉检查。
协议测试默认携带服务返回的实际凭据；无凭据和伪造凭据另有明确反例，不会自动补成成功。

2026-11-09 文档回归复核：`qt-document` 的平面插入样本由原 FBM 独立补充新增设置字段，
旧文件、数值精度和输出对照不变；Lackenby 新建预览、静水力基准面及新图层默认不透明度
按原代码和独立几何分别验证。已知旧版内存排序变体仍标为 `review`。每个独立子模块的
异常、耗时和失败项分别记录，异常后继续执行后面的模块。统一入口不变：
`python -B run_tests.py --suite qt-document --verify-deployment ../FreeShip-Qt/dist --output <新目录>`。
诊断脚本 `maintenance/audit_construction_regression.py`、原失败报告及复核报告均保留；
结果索引为 `maintenance/document-regression-closure-20261109.json`。
本轮还补齐了新建文件的独立 JPEG 解码及平面投影的有效范围、几何保持、撤销重做检查。
新的旧版排序变体必须经过 `maintenance/plane_oracle_replay.py --preserve-layout` 四次独立
重放，再由 `maintenance/capture_plane_oracle_variant.py --replay <报告>` 校验并收录，
作为 `review` 证据；Qt 固定基准不变。`maintenance/recheck_document_regressions.py`
用 `--core <完整报告> --output <新目录>` 复验本次明确受影响的三个模块，要求其他测试、
程序和原库均未改变，并为每项合并结果记录来源；它用于这次诊断的续跑，不是通用放行规则。
最终为 14385 通过、0 失败、32 待复核、3 未执行、0 错误，实际桌面仍未验收。

## 控制点编辑后的局部细分和填色


`python -B run_tests.py --suite qt-local-geometry --runtime <Qt运行包> --output <C盘新目录>`

源码为 `qt_local_geometry.py` 和 `qt/native_subdivision_tests.cpp`，复用控制网小例、实际
文档服务及原缓存测试。`--verify-deployment <实际dist>` 检查安装目录。
覆盖两种细分模式的 0—4 级、连续单点/批量修改、撤销、负零、拓扑和全局填色条件失效，
以及损坏缓存和无法使用缓存。所有坐标位模式和四份实际输出完整比较，不采用误差容限。
候选与当前安装程序不同时，再对照旧工作程序；正式安装后复测记录保留该边界。
计时使用六对不同编辑，单列无缓存计算与新旧程序均启用缓存的结果。

成功比较后的大输出只留输入、校验值和小报告。统一入口完成后清理本轮已验证的完整
结果缓存、`sections/cuts.fscache` 和 `sections/local-geometry.fscache`；出现未知文件或
目录联接时跳过该缓存，不清理历史结果。对应保全/清理行为也由本套件检查。
真实桌面鼠标操作仍单列未测，不能用隐藏程序通过代替。

## 专业计算窗口布局

图表另有 `python -B run_tests.py --suite qt-calculation-charts --runtime <Qt运行包>`。
测试源码为 `qt_calculation_charts.py`、`qt/calculation_chart_tests.cpp`，样式提取为
`maintenance/generate_calculation_charts.py --check`。覆盖 17 类原数据回放、正式计算入口、
图例/单曲线、数值逐位比较、坐标轴及缓存。`--verify-deployment` 复测实际安装目录。
历史基准路径在脚本 `REFERENCE` 中；保留其输入和数值结果，不重复生成百万点的导出。
推进三项采用原协议样本，不能代替缺失的真实 CALCPROP 结果或桌面验收。

百万点性能与显示精度：`python -B run_tests.py --suite qt-calculation-chart-speed --runtime <Qt运行包>`。
复用上述源码及历史加速样本，记录数据准备、首次描边、组合切换和恢复缓存的实际耗时；
覆盖多个设备缩放倍率、四种线型、单点尖峰、竖直/回头段、断点及新结果缓存失效。
与全量软件绘制双向比较实色覆盖，允许一个物理像素的抗锯齿边缘差异，不允许丢点。
显卡不可用时单列未测，隐藏窗口测试不等于真实桌面验收。
`maintenance/profile_calculation_chart.py --runtime <运行包> --output <新目录>` 用于分段诊断；
加 `--renderers` 对比绘图库，或加 `--production` 测正式加速窗口。诊断不代替上述回归。
可用 `FREESHIP_CHART_SOFTWARE=1` 排查软件回退路径；性能样本不能推出所有显卡均有相同速度。
`maintenance/debug_calculation_chart_shutdown.ps1` 留存退出崩溃的 GDB 命令，依赖本地调试器且
会影响调度，仅作诊断；提前退出和产品清理钩子的检查已纳入性能套件。

`python -B run_tests.py --suite qt-calculation-layout --runtime <Qt运行包> --output <新隔离目录>`

`qt_calculation_layout.py` 覆盖 19 个正式入口的字段、分页、三种字号与两种窗口尺寸，
并复用阻力、推进、水动力和附加质量的计算、取消、保存、参数往返及报告检查。
`maintenance/generate_calculation_pages.py --check` 核对原 LFM 页归属及来源指纹；
页面观察和原版比较分开记录。`--verify-deployment <实际dist>` 跑安装路径代表入口。
直接调用测试脚本可加 `--layout-only` 做布局诊断，不能据此声称计算回归完成。

截图和回调来自隐藏 Windows Qt 窗口；桌面焦点、系统文件框和多屏体验仍为未测。
CALCPROP 缺输出、真实 DBFview 缺失按原证据保留未测，协议替身只用于界面/数据流验证。
本轮报告在 C 盘，复用已有构建；不改写历史报告或其硬链接文件。
# 非对称侧投影及标注回归（2026-11-09）

`python -B run_tests.py --suite qt-lateral-projection --runtime <候选运行包> --output <隔离输出目录>`。
也支持 `--verify-deployment <正式目录>`。源码为 `qt_lateral_projection.py`，复用
已保留的实际棱柱/孔洞样本构造器，独立积分核对 XZ 投影并集面积与形心。
覆盖重叠/分离/相接侧影、月池/贯通孔洞、横倾/纵倾、单位和尺度、图层参与规则、
缓存失效、撤销/重开、零风压面积及取消；隐藏生产回调检查四视图标注与显示开关。
不以截图或隐藏回调代替实际桌面验收。Clipper2 来源和许可证也核对入包。
`maintenance/vendor_clipper2.py` 固定第三方来源，只用于维护，不是几何回归。

