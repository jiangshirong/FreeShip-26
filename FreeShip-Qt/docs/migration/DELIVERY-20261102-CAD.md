# CAD 输出本机交付

2026-11-02。入口为 `dist/FreeShipQt.exe` 的“文件 → 导出模型”。
新增 IGES 曲面、DXF 三维剖面与控制曲线、DXF 三维面、型值及坐标点。
支持范围、旧缺陷与本次迁移修复见 [CAD-EXPORT.md](CAD-EXPORT.md)。

完整计算/写出回归记录：`C:/FreeShip26Tests/reports/qt-cad-c`，
461 pass、0 fail、1 review、1 not_run、0 error。
`qt-exchange-cad` 是原 OBJ/STL/FEF 流程回归，280 pass、0 fail、1 review、1 not_run。
`qt-migration-cad` 保留编辑、保存、撤销、型线图与历史基准的完整回归。
`qt-cad-unified` 为统一测试入口从源码构建的独立运行。
候选包和安装目录的实际验证分别放在 `qt-cad-package` 与 `qt-cad-delivery`。
各报告最终状态、程序指纹和替换前目录以
`FreeShip-Tests/maintenance/cad-installed-20261102.json` 为准。

IGES 的 NURBS 转换继续使用隔离的原计算核心，写出器是 C++。
没有迁移全部 FBM/编辑/专业计算核心，没有替换旧 Win+R `fs` 或 QtPilot。
OpenCascade 和 ezdxf 仅作为测试依赖，不随运行包发行。
完整原 UI 保真、真实鼠标/文件选择器、商业 CAD 互通和工程制造验收仍待完成。

测试、故障样本和诊断源码全部保存在 `FreeShip-Tests`；输出与新构建均在 C 盘。
