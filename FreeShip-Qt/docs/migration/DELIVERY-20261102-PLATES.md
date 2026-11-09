# 板材展开本机更新

2026-11-02。入口为 `FreeShip-Qt/dist/FreeShipQt.exe` 的“计算 → 板材展开”。
功能、旧缺陷和输出边界见 [PLATES.md](PLATES.md)。这是本机开发包，不代表
所有模块、完整视觉或公开发行已经验收。

计算服务为 `build/plates-service-05`。复用原展开算法，不调用旧版展开窗口；
Qt 负责布局、显示和导出。旧版 fs、QtPilot、用户船模及旧设置没有切换。

保留证据：

- `qt-plates-final-c`：423 pass、0 fail、1 review、1 not_run、0 error。
- `qt-migration-plates-a`：751 pass、0 fail、9 review、7 not_run、0 error。
  完整基本编辑/型线图回归之后，只调整板材的工程颜色读取、文字对比度及孔内名称可读性；新板材
  套件已在该调整后重跑。
- `qt-plates-package-b` / `qt-plates-delivery-b`：候选目录和实际安装目录的完整
  文件清单、板材数据、DXF/TXT 及操作前后场景对照。
- `qt-performance-plates-package-b`：本次包的阻力计算和报告与上轮有效结果对照。
- `qt-migration-plates-delivery-b`：实际安装包的编辑、撤销、保存和重新打开。

报告均在 `FreeShip-Tests/reports`。安装记录为
`FreeShip-Tests/maintenance/plates-installed-20261102.json`；实际状态以该记录和
对应报告为准。更新前的运行包保留在 `build/dist-before-plates-20261102`。

独立样本、原观察器、诊断和可复用检查均保存在 `FreeShip-Tests`，接入统一组
`qt-plates`。展开仍为 Pascal，实体打印、真实桌面和整体视觉单独验收；自动排料、
材料补偿和 CAM 路径没有并入完成范围。
