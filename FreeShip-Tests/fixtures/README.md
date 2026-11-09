# 历史 FreeShip 测试资料

这些资料用于复现旧文件布局、核对外部计算文件格式。它们只在隔离测试中运行，没有安装到正式程序，也没有改动用户的外部程序目录设置。

来源是作者的 [FREE!ship Plus 下载页](https://hydro.ucoz.net/downloads.html)，获取日期 2026-10-30。

| 文件 | 原地址 | 本地位置 | SHA-256 |
| --- | --- | --- | --- |
| 3.5 Delphi 源码包 | [Freeship3.5_Delphi_source.RAR](https://hydro.ucoz.net/download/Freeship3.5_Delphi_source.RAR) | `legacy35/source.rar` | `348d0db5477e4bf79d16c68be498ce46f0b99dd948db59f97d0584c928fe5688` |
| 3.50 完整发行包 | [Freeship350_full_RU.rar](https://hydro.ucoz.net/download/Freeship350_full_RU.rar) | `solvers35/full.rar` | `97467ccc974cd310fe0309a5b2f244a7395e96f74a9802fc4d351f65d168131b` |

源码包整体解压时，`Forms/FreeKeelWizardDlg.pas` 报 CRC 错误。本次依据是另外单独成功解出的 `legacy35/unpacked/Units/FreeShipUnit.pas`，SHA-256 为 `4b5ded1e4f578db54e4b09f46857d4e83fcc2dc20a7d04506a296ba1c6729d8e`。不将整个源码包称为已验证完好，也没有用它重编译历史程序。

该单元保留整块计算记录的读写和版本条件，可用来核对存储方式。它不能单独证明每个早期短布局；短布局规则同时依据当前仓库随附模型的字段偏移、数值和布尔边界。没有取得每一种早期布局对应的独立写入器。

正式对照使用发行包 `solvers35/unpacked/Freeship/Exec` 下六个程序：

| 程序 | SHA-256 |
| --- | --- |
| CLEMPOUP.EXE | `529437b9685eeaf111a7810d1ff11b0a5039fdc0761c61a057c5b27dfd5da80c` |
| CLEMBLOU.EXE | `cd84d3e2b12468844f3fdb33519b82c5a4467bc55ae30cd3087fdccc1b10ab57` |
| BUNKOV.EXE | `7409e884083afc558418ae0e8fd04fc13baa4e4b111c5ea1b8bf9e304ba449fe` |
| COMPTON.EXE | `0cc352b43ad42c1fb90768530ec2d87b42fe81cab716d5198c2ae1cd4ad55863` |
| WOLFSON.EXE | `00394eae55735edaacddc311da170a52c2ec5dfa7542aad1c2e421baf738e42c` |
| RADOJCIC.EXE | `61701443f0c78a8838f67813d35cd416648da94fcb74f04438a69d691b695448` |

在测试目录运行：

```powershell
python -B calculation_contracts.py --solvers fixtures/solvers35/unpacked/Freeship/Exec --output reports/contracts-recheck
```

`solver_protocol_samples.json` 是本项目为文件协议检查整理的输入，六个结果的比较对象是同一程序的直接运行。对照证明包装流程没有改变这六个样本的输出，不证明阻力算法精度或适用范围。

探索时还尝试了 CALCPROP 和操纵性程序：前者测试输入没有生成所需输出，后者超时。它们未计入六个真实程序通过结果。保留的探索记录在 `reports/solver-control-investigation`。
