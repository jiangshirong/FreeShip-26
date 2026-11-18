# 型线图恢复独立窗口

2026-11-10。原版 FreeLinesplanFrm.pas 使用 Show 打开独立非模态窗口。Qt 原实现却把型线图放在隐藏标签栏的第二页，顶栏动作直接切页；没有返回入口，覆盖了四视图。

改回独立非模态 QDialog。标题栏关闭和 Esc 均隐藏窗口，四视图始终留在主窗口。重复打开复用同一窗口，保留平移和缩放；模型更新仍通过原有 setScene 传递当前快照。关闭主窗口时一并关闭型线图。

工具栏保留填色和 PNG、DXF 两个格式按钮。F 键适配视图，滚轮缩放。主窗口图片导出针对四视图，型线图窗口的 PNG 按钮保存当前型线图。

统一测试：FreeShip-Tests/run_tests.py --suite qt-lines-window --runtime <运行包>。
测试脚本保存在 FreeShip-Tests/qt_lines_window.py；生产窗口动作探针在 src/app/startup_workflow.cpp。
验证重复打开、窗口关闭与 Esc、相机保留、四视图保留、模型更新以及工具栏构成。测试从实际 PNG 和 DXF 动作打开保存对话框，并核对像素和文件字节。报告包含 Windows Qt 隐藏窗口的工具栏绘制图。
PNG 比较统一为 RGBA8888，消除解码后格式与预乘 ARGB 缓存的表示差异；仍逐像素精确比较。

工具栏改版通过 43 项，零失败、零错误，另有两项桌面验收未运行。
改版报告：C:/FreeShip26Tests/reports/runs/combined-20261110-210923/qt-lines-window/report.json。
