# 关于窗口：恢复本地旧版布局与内容

2026-11-11 按用户新的截图要求，删除贡献者标题与表头，改为年份、名字、工作内容三列。年份使用 14 pt 海青色半粗体，名字 11 pt 深色常规字重，工作内容 10.5 pt 灰绿色常规字重。按年份保留分组留白，AI 厂商标识位于名字前。顶部将原创船体线形标识与 FreeShip_26 组合居中，标识的 SVG 与高分辨率 PNG 位于 `src/app/assets/brand`，PNG 嵌入 Qt 资源。本轮 `qt-about` 检查三列内容、字体角色、隐藏表头、标识渲染、标题组合居中与详情窗口，Windows 绘制图另行查看。

基准是 FreeShip-Plus-Lazarus/Forms/FreeAboutDlg.pas，不是未经用户修改的上游窗口。
Qt 原先仅显示 QMessageBox 简介，遗漏了贡献者表、年份分组、FreeShip_26 修改署名和两级详情窗口。

恢复七条贡献记录、三个年份分组空行、2026 年浅绿色行、FreeShip_26 标题及底部版本/许可按钮。
版权详情保留原四条记录和两个邮件联系入口；窗口支持 Esc 关闭。主窗口与详情窗口使用旧版配色和尺寸结构。
版本详情显示当前 C++ 构建时间、编译器、CPU 与系统，不将原 Lazarus 版本和编译器伪装为 Qt 运行版本；未记录的 Git 字段显示破折号。

实现：src/app/about_dialog.h；入口：src/app/studio_actions.cpp。
回归：FreeShip-Tests/qt_about.py，经 run_tests.py --suite qt-about --runtime <运行包> 调用真实菜单动作、贡献表、两个详情窗口和 Esc；连续打开两次。
隐藏窗口测试与真实桌面视觉验收分开记录，不据此宣称整体 UI 迁移完成。

2026-11-11 根据用户新文案，将贡献统一为名字与工作内容的一行记录，保留年份分组。加入六条 AI 分工和 Jiang Shirong 的提示词署名，补充中、英、俄对应文字。GPT、DeepSeek、DeepL、Qwen 条目前显示厂商标识，来源与指纹保存在 `src/app/assets/ai-vendors/sources.json`，图像嵌入 Qt 资源。默认窗口调整为 1040×800。统一 `qt-about` 回归覆盖准确文案、同年只显示一次、默认窗口完整显示、六个标识、详情和重复打开。Windows 隔离实例的 Qt 绘制图用于检查排版，桌面鼠标输入与用户最终观感继续单独验收。

2026-11-11 字体复核后，关于页继承 QApplication 的字体家族。名字与工作内容均使用 Regular 400 字重，通过颜色、字号和列位置区分。qt-about 记录请求与实际解析的字体家族、字形和字重，防止界面文本正确而实际字体不符。
