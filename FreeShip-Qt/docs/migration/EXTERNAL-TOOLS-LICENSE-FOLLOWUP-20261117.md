# 外部计算工具：上游分发依据复查

核查日期：2026-11-17。只调查公开资料与本机原包，没有联系作者，没有改变用户更新测试版，也没有发布服务器版本。

后续状态：用户随后明确要求补回全部外部工具，项目决定按上游随包方式保留原始
二进制、数据和说明，采用 upstream-bundled 分类纳入 0.1.7。下文是该决定前的
调查记录，不把项目交付决定改写成已经发现无限制授权。实现与验收见
[完整交付记录](EXTERNAL-TOOLS-BUNDLED-20261117.md)。

## 已确认的事实

1. Lazarus 维护者确实随包分发这些 EXE，并非只提供接口让用户另外寻找。
   2024-04-23，markmal 在 [issue 102 的回复](https://github.com/markmal/freeship-plus-in-lazarus/issues/102#issuecomment-2062874995) 中确认全部 EXE 已打包，位于安装目录的 Exec。
   上游 [instpkg/wix/exec.wxs](https://github.com/markmal/freeship-plus-in-lazarus/blob/master/instpkg/wix/exec.wxs)
   列出 ADD_MASS、CALCPROP、hship、RVRSSHIP 等文件；Debian 与 ZIP 打包脚本也复制 Exec。
   本机只读核对基于 origin/master（1de17e99fb2b1cc559cdf27c5cafb595a627a641），不是 FS26 修改后的安装代码。

2. 维护者没有把这些外部程序解释为自己的 GPL 源码。
   [issue 96 作者回复](https://github.com/markmal/freeship-plus-in-lazarus/issues/96#issuecomment-1368580141)
   明确称这些程序是前苏联/乌克兰科研机构的专有程序，据其了解用 Fortran 编写，以 EXE 形式调用。
   [后续回复](https://github.com/markmal/freeship-plus-in-lazarus/issues/96#issuecomment-1370305748)
   再次区分免费外部二进制模块与源码。
   因而闭源并未阻止其实际随包分发；但回复没有交代授权文件、授权范围或第三方再分发条件。

3. 原作者网站分别标注许可证。
   [程序列表](https://hydro.ucoz.net/programsE.html)将主程序列为 Freeware、GNU GPL，
   hship、RVRSSHIP、ADD_MASS 等列为 Freeware。
   [下载页](https://hydro.ucoz.net/downloads.html)同时提供 3.5 完整包、源码包与外部模块，
   并说明放入 EXEC 后重启。这里的免费提供事实成立，但页面未给出统一再分发条款。
   [issue 100](https://github.com/markmal/freeship-plus-in-lazarus/issues/100#issuecomment-2271000553)
   指向该镜像，并说明它被作者 ORCID 页面引用；本轮未另外核验 ORCID。

4. Lazarus 的整体许可声明与外部模块的具体说明不能简单合并。
   上游 [copyright](https://github.com/markmal/freeship-plus-in-lazarus/blob/master/copyright)
   对 Files: * 写 GPL-2.0+，列出 van Engeland、Timoshenko、Malakanov 等版权人。
   但上面作者的专有程序说明，以及下面 ADD_MASS 的用途限制，使“全部 EXE 均为 GPL 且不限制商用”这一解释不可靠。
   本轮没有找到这些外部程序的对应 Fortran 源码或单独授权信；不能据此证明不存在私下授权。

5. ADD_MASS 有需要单独处理的具体限制，不能被笼统 Freeware 掩盖。
   上游 Languages/English.ini 的 User1093–1095 写明教育及非商业用途，版权为 Dudchenko 与 Timoshenko。
   本机原版 3.5 的 FreeShip-Tests/fixtures/solvers35/unpacked/Freeship/Languages/English.ini
   也包含该说明（User1093–1094 跨行，约 2724–2725 行）。
   [上游英文说明](https://github.com/markmal/freeship-plus-in-lazarus/blob/master/Languages/English.ini)
   标题写 v1.03，而作者网页写 v1.04；仍需区分说明沿用与版本许可变化，不能自行推定 v1.04 已取消限制。

## 判断

上一轮若给人“上游没有随包分发，或者这些工具已确定禁止分发”的印象，需要纠正。
上游随包分发有作者回复和安装清单双重证据，FS26 保持原样、保留作者和原条款的兼容分发方案有实际先例。
但先例不是我们已经取得不受限制再分发及商用授权的证明；尤其 ADD_MASS 存在实际用途限制。
目前问题应具体表述为“上游已分发，但第三方分发条款和个别模块商用范围尚不清楚”，不是“因为没看到 LICENSE 所以不能分发”。

本轮没有改 catalog 为 approved，没有把专有二进制冒称 GPL，没有改软件计算逻辑。
如果继续确认授权，问题应直接针对“FS26 免费发行中原样随附 EXE/BD，以及 ADD_MASS 能否用于商业工程”，
而不是询问是否可以开源其源码。未经用户明确要求，不代用户发送邮件或论坛消息。
