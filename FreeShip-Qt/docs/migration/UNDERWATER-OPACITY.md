# 水下颜色的混合比例

2026-11-05。

工程设置的常规页增加“水下不透明度”。0% 使用原图层颜色，100% 使用水下颜色；
中间值混合两种颜色。它不改变船体的透明度，后者仍由图层设置控制。

四视图、视图图片和 FBM 预览使用同一规则，只有参与静水力计算的图层着色。
混合在光照前进行。GPU 在模型上传时计算每个面的水下颜色，镜头移动只更新相机；
CPU 后备路径也按面计算，不在像素循环中重复混合。原曲面、交线精度不变。

文件保存 0–255 的整数，界面按原有百分比方式显示。未修改控件时保留原整数，
例如 234 显示为 91%，直接确认仍保存 234，不会变成 232。关闭水下着色不会丢失此值。
改动支持取消、撤销、重做和保存重开；旧格式只有实际改动该参数时才升级至格式 44。

来源是 `FreeProjectSettings.inc` 和 `FreeSubdivisionControlFace.inc` 的设置与绘制流程。
原绘制代码把水下蓝色通道读成了绿色；Qt 使用真实蓝色通道，没有照搬这个错误。
原源码和原程序作为对照保留。

型线图是另一套填色约定：旧 `FreeLinesplanFrme.pas` 直接采用水下颜色，不读取这个比例。
本次保留该约定，型线图场景、DXF、曲面和剖面输出逐项比较，不因新增控件改变导出。

复现入口：

```powershell
python -B FreeShip-Tests/run_tests.py --suite qt-underwater --runtime FreeShip-Qt/dist
```

测试保存在 `FreeShip-Tests/qt_underwater.py`、`qt/underwater_tests.cpp`，并接入统一入口。
覆盖 45 个格式版本、逐字节保存、无操作不升级、错误与过期请求、撤销、真实 Qt 工程设置
动作、预览刷新，以及独立 RGB 预期的 CPU/GPU 四视图绘制。字体布局沿用 `qt-dialog-layout`。
安装与最终报告指纹见 `FreeShip-Tests/maintenance/underwater-installed-20261105.json`。

这些是隐藏窗口的事件与图片检查，真实桌面的鼠标、焦点和多屏体验仍待验收。
