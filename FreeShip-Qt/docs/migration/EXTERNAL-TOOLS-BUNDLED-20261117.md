# 外部计算工具补齐：0.1.7

2026-11-17，用户在看过上游随包分发证据后要求补回全部外部工具。

## 实际改动

- 正式源目录：third_party/hydronship；不再依赖测试目录交付。
- 共 23 个原版 EXE，含当前入口使用的 21 个，以及 PROPOL、PROPCALC 两个历史配套程序。
- 加上 4 份 BD、CONF.PRO、RESIST.CNF、原版 ReadMe1st 和本次来源/限制说明，共 31 个文件，4,782,349 字节。
- 公共包安装至 CalculationTools/Exec 和 CalculationTools/BD；程序已有自动发现逻辑，无需新用户填目录。
- package_runtime 默认纳入清单，MSI 和签名增量更新按文件指纹交付。缺少数据、说明或指纹不符仍拒绝。
- catalog 使用 upstream-bundled 表示本次项目交付决定，不伪称已经找到无限制许可。
  随包保留 ADD_MASS 教育/非商业用途限制，不将 EXE 称为 GPL 源码。
- 计算程序原始字节不变。主程序计算二进制沿用 0.1.6，仅 FreeShipQt.exe 保留 UI 对话已验证的 Windows 图标资源修复；非资源段和 COFF 附加数据与 0.1.6 相同。

## 验证

1. calculation-tools-delivery：266 pass、0 fail/error。独立产品身份的 MSI 首装、升级、12 个计算入口及 6 个实际 Qt 窗口导出，不指定测试求解器目录；升级结果一致。
   报告：D:/FreeShip26Tests/reports/runs/calculator-bundled-20261117/calculation-tools-delivery/report.json
2. 加入图标资源后的最终运行包：207 pass、0 fail/error。
   报告：D:/FreeShip26Tests/reports/runs/calculator-final-bundle-20261117/calculation-tools-delivery/report.json
3. calculation-tools-release：10 pass、0 fail/error。签名、全部工具进入 MSI、计算二进制/Qt 非资源段、使用 0.1.6 MSI 的增量重建与完整安装器哈希一致，以及本机文件只读核对。
   报告：D:/FreeShip26Tests/reports/runs/calculator-release-final-20261117/calculation-tools-release/report.json

增量重建需 12,548,144 字节，完整压缩分块共 57,887,480 字节。实际客户端缓存情况不同会影响下载量。
安装器回归是本机隔离身份和非输入桌面，不是全新 Windows 虚拟机，也不代替用户手动接受更新。

首轮发布检查 6 pass、1 error：角色记录在 UI 对话按用户要求删除开发版时被删除。
保留失败报告 calculator-release-20261117；按只读核对当前安装调整后重跑，不伪造旧开发版继续存在。

## 本机入口与其他对话的合并

本轮先将 31 个文件补到原开发目录。随后 UI 对话按用户指示合并到 FreeShip26Qt 并删除开发目录；
复查确认 31 个文件已完整迁入其 UserCalculationTools/HydroNShip35。
用户进一步澄清：ftq 是本地工作版；ftq-dev 才是想删除的临时试验版。
目前 ftq 与桌面更新测试快捷方式仍指向同一目录，不能称为两份隔离安装或原始服务器测试基线。
本轮没有重建开发目录、修改快捷方式或运行本机正式版安装器；没有再安装/升级本机正式版。

## 发布产物

最终包位于 D:/FreeShip26Releases/0.1.7-tools，版本 0.1.7。
先前 D:/FreeShip26Releases/0.1.7 是未保留图标修复的候选，不得发布。
源码包基于校验通过的 0.1.6 对应源代码，更新打包脚本、供应商二进制和说明；
旧打包脚本保留于 build-inputs-original。未将其他对话尚未发布的计算改动混入本次包。
服务器发布结果另记 server-verification.json；没有该记录时不要声称线上已更新。

发布完成：北京时间 2026-11-17 10:45，通过 HTTPS 读取 stable/latest.json 与本地签名清单逐字节一致，
RSA 签名有效，版本为 0.1.7；两个公开下载链接均返回 200 且长度一致。
上传端已在服务器逐块校验全部 1163 个分块及 3 个下载文件的 SHA-256。
回执：D:/FreeShip26Releases/0.1.7-tools/server-verification.json。
没有运行本机正式版安装器，也没有替用户点击更新。三份成功回归的产物已上传夸克并凭成功回执清理，未下载回来校验。
