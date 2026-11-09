# 模型交接本机更新

2026-11-02。运行入口：`FreeShip-Qt/dist/FreeShipQt.exe`。
“文件 → 导入模型／导出模型”现有 OBJ、STL、FEF 六个入口。
板材展开、静水力、阻力与推进、模型编辑及已有 Qt 绘图继续保留。
支持范围和未覆盖格式见 [MODEL-EXCHANGE.md](MODEL-EXCHANGE.md)。

计算服务：`build/exchange-service-04`。核心回归使用 `build/exchange-runtime-06`；
最终安装包来自 `build/exchange-package-02`，另修正 FEF 导入窗口的单位提示。
格式解析/输出为 C++；候选模型构建、编辑状态和 FBM 仍使用原 Pascal 核心。
正式入口、旧版和 QtPilot 的区别不变，Win+R `fs` 没有切换。

保留的验证记录：

- `qt-exchange-unified/qt-exchange`：统一入口完整运行，271 pass、0 fail、1 review、1 not_run。
- `qt-exchange-final`：补齐英尺默认水密度换算及服务检查后，277 pass、0 fail、1 review、1 not_run。
  包括三种格式、独立读取器、凹面面积、正负舷/中心面/近中心面、隐藏图层、单位及轴顺序，
  也包括损坏文件、空会话导入、编辑/撤销、FBM 保存重开和真实 9a 模型。
- `qt-migration-exchange-a`：2154 pass、0 fail、27 review、18 not_run、0 error。
  在最后一个只影响新 OBJ/STL 英制导入默认密度的修正之前完成；旧功能路径未随后修改。
- `qt-exchange-package-final`、`qt-exchange-delivery`：候选目录与实际安装目录的同一模型交接套件，
  最终套件增加三项 FEF 窗口使用文件内单位的检查，280 pass、0 fail、1 review、1 not_run。
- `qt-plates-exchange-package`、`qt-performance-exchange-package`：已有专业流程在候选包中的结果对照。
- `qt-migration-exchange-delivery`：实际安装路径的编辑、撤销、保存和重开对照。

后续测试输出改存 `C:/FreeShip26Tests/reports`；部分历史 D 盘报告路径通过目录联接保留。
后四项的实际运行状态及二进制指纹，
以 `FreeShip-Tests/maintenance/exchange-installed-20261102.json` 和相应报告为准。
更新前的运行包保留在 `build/dist-before-exchange-20261102`。

测试源码均在 `FreeShip-Tests`，新组名 `qt-exchange`。
隐藏窗口已检查，不代表系统文件选择器、真实鼠标/焦点、其他 CAD 软件及完整原 UI 已验收。
其他格式、剩余专业工具和原生核心继续单列，没有把六个入口记作整个迁移完成。
