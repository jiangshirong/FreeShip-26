# 2026-11-02：Archimedes 与 PAM

本轮完成 ArchimedesMB `.hll`、Archimedes `.app` 和 PAM 旧版几何模板的 C++ 输出、
Qt 参数窗口、可取消任务和原子提交。断面生成从 GHS 拆为共用入口，GHS 保留原输出。
字段约定、旧代码问题和范围限制见 [船舶格式说明](SHIP-FORMATS.md)。

9a 导出 11 个站，HLL/APP 各 4,331 个点，PAM 为 2,171 个点。统一入口这次运行
的独立进程耗时约 0.18 秒，包含断面生成和文件写出；不是外部软件导入速度，
也不是整体启动时间。没有重绘旧窗口、修改控制网或降低几何精度。

统一测试 `qt-ship-formats` 从源码构建后为 215 pass、0 fail、1 review、
2 not_run、0 error。箱体、英制、非零基线、参考位置、XML 名称、上下分离断面、
闭合中心面、拒绝和文件保护各有独立检查；实际 Qt 菜单、取消、参数编辑、
控制点变化/撤销及场景/DXF 保持均有运行证据。

报告均在 `C:/FreeShip26Tests/reports`：

- `qt-ship-formats-unified/qt-ship-formats`：统一入口完整构建与上述 215 项。
- `qt-ship-formats-delivery`：正式安装目录的新格式测试。
- `qt-ghs-ship-delivery`：共用断面拆分后的 GHS 回归，另核对 9a 与旧文件逐字节一致。
- `qt-cad-ship-delivery`：CAD 五种格式和独立 OpenCascade 对照。
- `qt-migration-ship-delivery`：安装目录的基本编辑、撤销、保存重开。

最初 `qt-ship-formats-a` 的几何和 UI 检查通过，但人工复核发现诊断字段
`half_section_points` 为 null。原因是向同一个 QJsonObject 插入字段时，复用了
失效的值引用。现在先复制值再插入，并把实际点数检查加入套件。这是本次新实现
的问题，未记为上游缺陷。初始报告保留，最终判断采用上述更新后的报告。

运行包：`FreeShip-Qt/dist/FreeShipQt.exe`。候选包在 C 盘构建，旧包保留于
`FreeShip-Qt/build/dist-before-ship-formats-20261102`。安装清单、实际目录报告、
文件哈希及旧版源码指纹保存在
`FreeShip-Tests/maintenance/ship-formats-installed-20261102.json`。
安装和记录脚本也保留在同一测试工程，不自动运行、不删除历史基准。

旧版源码、QtPilot、用户模型和 Win+R 的 `fs` 入口保持不变。新增测试输出和
临时构建都在 C 盘。HLL/APP/PAM 接收软件尚未运行；PAM 计算参数仍须核对，
不等于已迁移 AddMass 求解器。真实桌面和完整 UI 一致性继续单独验收。
Michlet/AddMass、部件交换、其余导入和原生计算核心等迁移工作仍未完成。
