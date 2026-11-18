# 2026-11-02：文档核心接管

本轮不再补充零散导出格式。Qt 的活动工程、版本、撤销/重做、保存点和持久
FBM 状态改由 C++ 文档服务持有，现有编辑命令统一经过它提交。新增结构读取器
检查历史 FBM，并对未修改文件逐字节保存。范围和保留的 Pascal 依赖见
[原生文档说明](NATIVE-DOCUMENT.md)。

本轮由主代理负责服务、状态交接、集成和测试，一个子代理负责独立的 FBM 编解码
模块。没有给每个小功能再拆测试/文档代理，也没有分别打包这些模块。

核心报告 `C:/FreeShip26Tests/reports/qt-document-b/report.json` 为
1,943 pass、0 fail、2 review、2 not_run、0 error。
首轮故障注入暴露的空闲子进程退出识别问题保存在 `qt-document-a`，已修正。
报告内分别记录编解码、协议、建模服务和 Qt 回调耗时；第二轮合计约 103 秒。
这不是本轮全部开发时间。阅读设计、编码和集成仍占主要时间，不能归到等待测试。

最终安装后的统一核心入口与各窗口回归、程序哈希、旧源码指纹及时间记录见
`FreeShip-Tests/maintenance/document-installed-20261102.json`。
测试源码保留于 `FreeShip-Tests/qt_document.py`、`qt/fbm_codec_tests.cpp`，
跨窗口回归使用已有套件。统一入口现在支持显式复用已核验的构建，避免重新编译
整套工具链；源码、适配器或二进制不匹配时仍拒绝复用。

程序为 `FreeShip-Qt/dist/FreeShipQt.exe`，新服务同目录。旧包保留在
`FreeShip-Qt/build/dist-before-document-20261102`。新增构建和报告继续在 C 盘。
旧 FreeShip、QtPilot、用户模型以及 Win+R 的 `fs` 入口保持不变。

这次尚未替换 Pascal 的几何编辑算法、编辑后完整 FBM 字段编码、四视图网格与
专业计算核心，也未实现整个程序崩溃后的持久恢复或跨重启历史。隐藏 Qt 回调
与数值对照不代替真实桌面验收；本轮没有修改用户认可的主界面设计。
