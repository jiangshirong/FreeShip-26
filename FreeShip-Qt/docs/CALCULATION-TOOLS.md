# 外部计算工具

从 0.1.7 起，安装包默认附带上游的 23 个外部计算程序、4 份阻力数据及配置/说明，共 31 个文件（约 4.56 MiB）。当前软件计算入口使用其中 21 个，PROPOL、PROPCALC 保留为上游历史配套程序。不增加菜单，不改动算法。

## 文件位置与自动发现

正式交付源为 `third_party/hydronship`，不再从测试样本目录打包。
公共安装位置为程序下的 `CalculationTools/Exec` 和相邻 `CalculationTools/BD`。
新安装不需要手动选择目录；每次打包默认纳入工具，MSI 与签名增量更新都记录文件指纹。

用户此前安装的 `UserCalculationTools/HydroNShip35` 及其相对配置仍保留。
手动选择的工具目录优先，其次应用配置，最后是随包 CalculationTools；不覆盖用户自备程序。
本机 ftq 当前指向的工作版本已有完整 31 个文件。

## 来源与使用范围

按用户 2026-11-17 请求，依照 Lazarus 维护者已确认的随包分发方式，原样保留二进制及说明。
这是项目的交付决定，不声称已取得无限制授权，也不把这些专有 EXE 标为 GPL 源码。
ADD_MASS 原说明的教育与非商业用途限制保留于随包 NOTICE.md。
依据见 [复查记录](migration/EXTERNAL-TOOLS-LICENSE-FOLLOWUP-20261117.md)。

清单 `tools/calculation-tools-catalog.json` 区分 upstream-bundled 与明确许可 approved，
记录上游分发证据、项目决定、用途限制和每个文件的 SHA-256。
缺少依据、说明、数据依赖或指纹不符的文件不能进入安装包。

## 本机补充安装

维护脚本 `tools/calculation_tools_delivery.py` 可导入经过指纹核对的原包；已有同版文件不覆盖，只补齐缺失项。
存在受保护用户更新测试角色时，拒绝向其导入工具。
安装器和更新回归使用独立产品身份，不操作本机工作版本。

每次实际计算使用独立工作目录，hship 的 CNF 和 BD 在临时目录中使用。
缺文件、错误退出及无效数值仍拒绝；本次没有放宽精度或修改结果。
DBFview 是可选数据库查看器，不属于这 23 个计算模块；外部工具存在不等于其经验算法获得工程认证。
