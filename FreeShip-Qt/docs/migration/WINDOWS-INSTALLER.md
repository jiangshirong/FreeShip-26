# Windows 安装、升级与自动恢复

支持的入口是 `FreeShip 26 Setup.exe`（0.1.2）。它安装当前用户版程序，默认目录为
`%LOCALAPPDATA%\Programs\FreeShip26Qt`，开始菜单名称为 `FreeShip 26`。
旧 `fs`、FBM 文件关联、模型和偏好设置的位置保持原样。

## 升级失败时怎么处理

安装器在动程序之前，先核对原版本的安装包、产品标识和恢复清单。包丢失、被改写、
同名用户文件冲突、程序仍在使用或已有其他本产品登记时，拒绝开始升级。

安装事务会记录原版本、目标版本和安装位置。发生本次复现的 MSI 回滚登记错误时，
安装器自动移除失败版本及残留登记，使用已核验的旧包恢复原版本，再检查：

- MSI 已安装状态和登记的安装位置；
- 清单内每个文件的 SHA-256，以及失败新版独有文件是否清走；
- 开始菜单入口、外层安装登记和维护程序；
- 恢复记录是否已完成，能否再升级、修复和卸载。

恢复成功后仍返回安装失败，界面明确说明已恢复原状态；不会把升级没成功写成成功。
恢复本身出错时保留事务和恢复包，留下具体错误。下次运行安装器先处理未完成事务。
硬件断电、磁盘损坏，以及 MSI 服务仍在执行时被终止的场景需要单独验收。

## 包和缓存

构建使用现有原生运行包，不重编船体计算或绘图代码：

```powershell
./build-installer.ps1 -FromRuntime ./dist -OutputDir C:/FreeShip26Installer/0.1.1
```

`-OutputDir` 必须为新目录。默认 WiX 目录为
`%LOCALAPPDATA%\FreeShip26QtTools\wix314`；安装器外层使用 Windows .NET Framework 4.x，
应用本身仍是 C++/Qt。构建还需要系统中的 Framework C# 编译器。
安装器按当前用户权限运行，不要求为了回滚改变系统策略或注册表权限。

输出 EXE 内嵌 MSI。MSI 是内部载荷，直接双击会提示使用 Setup EXE；Windows 应用列表
只显示外层维护入口。`installer-manifest.json` 记录 EXE、内部 MSI、完整载荷、源运行包、
打包代码和工具的指纹。程序资源、图标、主题、源码和许可记录保留；本机计算工具的
绝对路径配置不打进安装包。

恢复缓存位于 `%LOCALAPPDATA%\FreeShip26\InstallerCache\<产品系列标识>`。
正常完成后只保留当前与上一版的安装包及小型维护程序。卸载会清掉大安装包缓存，
正在运行的维护 EXE和最后一次结果可能保留少量空间；回归要求总计小于 1 MiB。
程序目录只由 MSI 按文件登记卸载，不递归删除整目录，不按通配符删除用户船模。

同一发行版本不得更换内容；后续包递增三段版本号。缓存损坏时拒绝升级，保留当前程序，
不能跳过校验或先删除原程序。需要恢复失败或损坏缓存时，保留原包和日志再排查。

## 回归与证据

统一入口：

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-installer --runtime FreeShip-Qt/dist
```

`qt_setup.py` 执行真实安装器 EXE，使用独立测试身份、中文/俄文/空格路径及私有缓存。
覆盖首次安装、资源修复、升级、降级拒绝、损坏缓存保护，以及两种实际故障：

1. 在 MSI `InstallExecute` 执行之后失败，复现此前“文件恢复、安装登记损坏”的问题。
2. MSI 已成功安装新版，且外层登记与状态已写入后，再触发失败。

两种故障都必须自动恢复。测试再从实际 Qt 回调打开原模型和偏好，重新升级，并经
安装器维护入口卸载。模型、笔记、设置、快捷方式和产品登记分别检查。
发布 EXE 另核对嵌入包指纹，确保没有测试故障开关或测试产品身份。

最新本机验收记录：`FreeShip-Tests/maintenance/setup-recovery-delivery-20261110.json`。
静默安装和隐藏 Qt 回调不等于真实安装向导、不同账户/机器、高 DPI或签名发行验收。
测试源码和失败证据保留；重复的安装包、解包文件和中间文件按核对过的清单退役。

## 原始 MSI 缺陷仍保留记录

2026-11-09 的原始 MSI 回归发现：失败升级返回 1603，旧文件恢复，但旧产品变成
advertised 状态，新产品登记仍在；直接卸载又被残留新版阻止。日志包含回滚登记时的
1403/1406 和系统错误 5。旧报告保留在
`FreeShip-Tests/maintenance/windows-installer-assessment-20261109.json`。

[WiX 问题 #8574](https://github.com/wixtoolset/issues/issues/8574)有同类报告。
本次修复的是 FreeShip 安装器的完整恢复流程，没有声称修好了 Windows Installer 内部。
`--suite qt-msi-diagnostic` 保留直接运行裸 MSI 的原始故障回归，仅显式调用，预期仍可能失败。
默认验收走受支持的 Setup EXE，仍检查真实 MSI 失败和完整恢复，没有移早故障点或取消断言。

依据：[MsiInstallProduct](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiinstallproductw)、
[MsiConfigureProductEx](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiconfigureproductexw)、
[ARPSYSTEMCOMPONENT](https://learn.microsoft.com/en-us/windows/win32/msi/arpsystemcomponent)。
