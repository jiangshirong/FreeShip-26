# 2026-11-02：GHS 船体输出

本轮在 Qt 文件菜单接入 GHS 对称船体导出。具体范围、坐标约定、原程序问题及
尚未覆盖的情况见 [GHS 输出](GHS-EXPORT.md)。新的断面生成和文件写出全部为 C++。
已有型线图、四视图、FBM 和 CAD 出口不改计算规则。

真实 9a 样本生成 11 个站、2,171 个点；独立进程耗时约 0.19 秒，是这台机器上
的一次测量，包含断面求交和文件生成，不是接收软件导入或整体启动时间。
最前站的球鼻艏和上部轮廓分别保留，没有跨空隙补出面积。

验收覆盖解析箱体面积/重心/体积、闭合中心面、上下分段、非静水力图层排除、
原点与英制密度、自然三次曲线误差、自交和横向缺口、控制点修改和撤销、
重复生成、实际 Qt 导出与取消，以及原模型和原场景/DXF 不变。

报告存于 `C:/FreeShip26Tests/reports`：

- `qt-ghs-final`：90 pass、0 fail、1 review、1 not_run、0 error。
- `qt-cad-ghs`：461 pass、0 fail、1 review、1 not_run、0 error。
- `qt-exchange-ghs`：280 pass、0 fail、1 review、1 not_run、0 error。
- `qt-ghs-unified/qt-ghs`：从源码构建并验证统一入口；后续另补闭合中心面回归。
- 安装目录的最终运行报告为 `qt-ghs-delivery`、`qt-cad-ghs-delivery` 和
  `qt-migration-ghs-delivery`，以这些报告和交付索引中的实测结果为准。

`qt-ghs-a` 保留最初未处理球鼻艏分段的失败记录；`qt-ghs-closed-negative`
保留闭合中心面修正前的失败例。两者是本次迁移发现的问题，不记为上游缺陷。
所有测试和诊断源码保留在 `FreeShip-Tests`，没有只归档报告。

运行包为 `FreeShip-Qt/dist/FreeShipQt.exe`。打包、安装和实际目录核验的哈希与
报告索引保存在 `FreeShip-Tests/maintenance/ghs-installed-20261102.json`。
旧包留在 `FreeShip-Qt/build/dist-before-ghs-20261102`；旧版源码、QtPilot、模型
及 Win+R 的 `fs` 入口保持不变。新增构建和测试输出均在 C 盘。

商业 GHS 读取、实体打印和真实桌面操作未验收；隐藏窗口的回调和截图不能代替它们。
当前格式依据及未取得的完整格式手册已在实现记录中说明。
其余船舶专用格式、剩余导入和计算核心的迁移仍继续，不表示整套迁移完成。
