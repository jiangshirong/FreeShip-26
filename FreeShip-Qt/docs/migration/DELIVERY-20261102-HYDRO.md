# 静水力与稳性本地交付

2026-11-02。实际使用入口为 `FreeShip-Qt/dist/FreeShipQt.exe` 的“计算”菜单。
设计静水力、吃水表、固定/自由纵倾、KN/GZ 和报告使用当前文档的同一版本；
参数保存和计算取消已接入完整流程。实现及数值边界见 [HYDROSTATICS.md](HYDROSTATICS.md)。

计算适配构建为 `build/hydro-service-03`。本轮 Qt 源码保留模型编辑方向的全部功能，
旧版 `fs`、试点、用户模型和用户设置没有切换。安装前版本保留在
`build/dist-before-hydro-20261102`，交付核对记录在
`FreeShip-Tests/maintenance/hydro-installed-20261102.json`。

## 验证记录

- `FreeShip-Tests/reports/qt-hydro-20261102-e`：941 pass，0 fail，2 review，
  1 not_run，0 error。包括三个船型的原积分逐项对照，固定/自由纵倾，解析箱体、
  坐标基准、英制、漏水、中途取消、参数事务、保存重开及真实 Qt 回调。
- `FreeShip-Tests/reports/qt-modeling-hydro-20261102-a`：2781 pass，0 fail，
  2 review，2 not_run，0 error。确认共享核心的释放修正与新计算入口没有破坏
  建模、撤销、保存和型线图/DXF。随后仅调整测试的静水力字体后端与积分说明文字。
- `FreeShip-Tests/reports/qt-hydro-package-20261102-a`：直接运行完整候选目录。
- `FreeShip-Tests/reports/qt-hydro-delivery-20261102-a`：直接运行正式 `dist`，
  核对完整清单、源码指纹、计算/导出回调及前后完整场景、DXF。
- `FreeShip-Tests/reports/qt-startup-hydro-delivery-20261102-a`：正式目录的启动恢复验证。

PDF 不只检查文件头，还解析文字并渲染首页。复核发现离屏插件把微软雅黑 UI
替换成了本机 CAD 字体，现已改用 Windows 字体后端运行隐藏的静水力窗口；
窗口和 PDF 中的中文、数值、单位、KN/GZ 及图例可读。没有操作用户桌面窗口。

## 明确保留的边界

`review` 中一项为非对称船体的旧面积算法：当前相关字段返回 null 并说明原因，
不把错误数值输出为成功。另一项为整套窗口的视觉一致性；隐藏窗口不等于实际
桌面鼠标、焦点、多屏与 DPI 验收。算法仍调用原 Pascal 积分，C++ 核心替换、
天气衡准和规范判定没有因为本轮界面接入而被标为完成。

所有新测试源码和独立预期都保留在统一测试程序；过时报告的清理由既有保留策略
处理，不删除测试代码、必要样本或仍被引用的基准。
