# 背景图片与标定

2026-11-01。三种正交视图的背景图片已接入 Qt 的文档、撤销和保存流程。
旧 Lazarus 工程、QtPilot、用户模型与 Win+R `fs` 入口保持原样。

## 使用与保存

在横剖、纵剖或俯视图内右键 → **背景影像**，可导入 JPEG/BMP、调整原点、
设置参考比例、调整图片边框、选择透明色、修改混合及容差、显示/隐藏、保存图片
和清除。各视图各自持有一张图；透视图不接受背景图。

原点和边框拖动只改视图内的预览，松开提交一次命令。Esc 或右键结束调整，取消
不增加撤销记录。提交期间保留预览，收到新版本后再切换，避免释放鼠标时跳回旧位置。
解码、参数验证或结果发布失败时，旧图片和历史保持不变。没有可见图片时不能进入
边框调整，移除图片后也不能继续操作旧对象。

JPEG 保留原始压缩数据，调整标定、撤销、保存和重开不重新压缩。BMP 首次导入时
转成 JPEG，之后同样保留那份数据。保存背景图片为 JPEG 时直接写原数据；保存为
BMP 时写解码像素。FBM 中的原点仍为整数像素，比例仍为 Single 的模型单位/像素。
单位切换只换算比例，图片与像素原点不变。

旧格式升级统一使用 `ModelEdits.RequireVersion`。早于 2.10 的文件没有缩略图记录，
新增背景图所需的 2.50 格式不能把默认的“保存缩略图”误当成一张已有图片。
本轮新增的 1.80 文件反例曾触发 `No retained source preview`；现已复用同一升级处理，
保存后再由独立原核心读取器检查图片、参数和版本。

## 计算与绘图边界

背景编辑不改变控制网，不重建船体或静水力结果；仍通过现有事务发布完整模型快照，
C++ 型线图使用几何内容缓存。背景图平移、缩放与相机移动不参与模型计算或 DXF。
测试逐阶段严格比较船体数据、型线图场景、剖面结果和 DXF 原字节。

解码像素和透明色蒙版按内容缓存，共享不可变数据，缓存上限 128 MiB。导入限制为
32 MiB 压缩数据、单边 16384 像素及总计 32 Mi 像素；超限明确拒绝，旧图不变。
绘制按当前视口截取图片源区域，避免极大缩放的整幅目标矩形影响绘制。船壳的 GPU
和软件填色层仅覆盖船体，外部保持透明，背景图片不会被整幅不透明底色遮住。

比例校准、像素原点及四条边的调整沿用当前 Plus 的规则。边框拖动从本次按下时的
状态计算累计位移，避免逐个鼠标消息取整后产生漂移。Qt 平滑采样及不透明度除以 255
的混合与旧 GDI 的整数舍入有差异，显示像素不宣称完全相同；几何和保存参数单独核对。
动画中间状态、调整框和拖动预览不会进入图片导出。

## 源码依据

- `FreeShip-Plus-Lazarus/Units/FreeBackgroundImageData.inc`：JPEG、标定参数及 FBM 保存。
- `Units/FreeBackgroundImage.inc`：目标矩形、原点、参考比例、透明色和边框调整。
- `Units/FreeEdit.inc::BackgroundImage_Open` 与
  `Forms/freehullformwindow_panel.pas::Background*Execute`：视图归属和操作语义。
- Qt 实现：`src/core/background_image.*`、`src/render/background_geometry.*`、
  `src/render/hull_view.*`、`src/app/background_actions.cpp` 及 `bridge/BackgroundEdits.inc`。

这是基于当前本地 Plus 的适配；没有使用 ShipCAD 的背景图窗口。原来源与 GPL 声明
继续保留，当前本地工作树包含既往 FreeShip_26 修复，不能全部归为原上游代码。

## 测试与交付状态

可复用测试已接入 `FreeShip-Tests/run_tests.py --suite qt-backgrounds`。
`qt_backgrounds.py` 驱动实际应用菜单和对话框，`qt/background_tests.cpp` 检查独立
标定公式、拖动提交/取消、空图保护、缩放和平移、透明缓存和合成；`QtEditingFixture.pas`
通过原对象直接读取 FBM 图片及参数，不使用新服务的序列化代码作预期。

测试使用盒体、合成船型和 9a。三个正交视图、全部图像属性、坏 JPEG、非法参数、
过期版本、发布失败回滚、撤销分支、单位换算及早期文件升级都有保留用例。
文件选择框通过公共 API 选取文件并点击实际确认按钮，取消和接受分别验证。
没有通过跳过对话框而声称覆盖该流程。

测试期间的 Qt 窗口使用离屏平台；字体来自已安装 Windows 字体目录，避免测试
截图缺字。GPU 合成检查单独创建原生平台的离屏 OpenGL 表面，不显示控件或操作桌面。
整体视觉一致性保留 `review`，真实鼠标、焦点、原生文件选择框及多屏体验保留 `not_run`。

当前构建使用 `background-service-03` 和 `topology-qt-01`。完整回归：
[qt-backgrounds-20261101-f](../../../FreeShip-Tests/reports/qt-backgrounds-20261101-f/report.json)，
**877 pass、0 fail、13 review、8 not_run、0 error**，总状态 `partial`。
包含原编辑控件和型线图绘图回归，实际 GPU 表面合成三项检查通过。旧程序、旧源码、
QtPilot、输入模型及测试期间的源码指纹保持一致。

运行包在核对后安装至 `FreeShip-Qt/dist/FreeShipQt.exe`。该路径的直接运行复核记录为
[qt-backgrounds-delivery-20261101](../../../FreeShip-Tests/reports/qt-backgrounds-delivery-20261101/report.json)，
执行状态以报告为准；比较三种模型十个阶段的完整场景、DXF 与剖面文件。
替换前保留运行包备份，不强制关闭用户进程，也不切换 Win+R `fs`。

原有辅助线与动画回归：
[qt-overlays-backgrounds-20261101](../../../FreeShip-Tests/reports/qt-overlays-backgrounds-20261101/report.json)，
**755 pass、0 fail、46 review、8 not_run、0 error**。使用相同 Qt 绘图代码及
`background-service-02`；之后的桥接变化只把背景导入的版本提升接回已有辅助函数。

下一批仍是半船/分舷、曲率梳、控制曲线/流线等流程；FBM 读写、编辑状态和专业计算
尚未全部替换为 C++，本轮不能视为整个迁移完成。
