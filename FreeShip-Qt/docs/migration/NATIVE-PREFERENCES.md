# 偏好与绘图文字的原生初始化

打开、新建、导入船模时，C++ 文档服务直接读取随程序附带的配置、主题和翻译。
这条路径不再为取得颜色、编码或标注文字启动 `FreeShipGeometryBridge.exe`。
几何计算、界面布局、曲线精度和导出算法没有改动。

实现位于 `src/document/preferences.cpp`。读取范围与旧桥接的 `document_context`
相同：FBM 编码、撤销内存、默认图层和水下颜色、字号、点大小、剖面及辅助线颜色、
DXF 调色板、型线图文字和板材名称。Windows 系统颜色按当前系统解析，保留原始
TColor 与解析后 RGB；DXF 色号仍按原始颜色选择，不能混用。

配置按默认值、`bridge/FreeShip.ini`、`bridge/.coordinate-settings/FreeShip.ini`
的次序叠加，再读取选定主题的一层父主题和主题本身。用户目录的同名主题优先。
未初始化的 Default 主题继续转到 StudioLight。有旧 DTA 且没有 INI 时，保留旧
DTA 的读取次序。读取不会创建配置文件，也不会写入旧版的设置目录。

有两处容易误改的行为保留了下来：

- 空船模的初始图层、水下颜色在读取主题前已确定；后续新增图层才使用主题值。
- FPC INI 的布尔值写成 `1`/`0`，读取时判断首字符。手写的 `True` 不等同于 `1`。

绘图翻译沿用现有 Qt 桥接固定的 `FreeShip.cn.po`，支持上下文、续行、转义和
未翻译文字回退。读取翻译不会修改船模里的名称、注释或用户正在编辑的内容。
这轮仅完成初始化。后续已接入 [Qt 偏好设置窗口及显式旧配置导入](PREFERENCES-UI.md)，语言支持由
用户指定的翻译模型负责。资源暂时保留在 `bridge` 目录，诊断程序也继续随本地开发包
提供，便于独立对照；正常文档路径不需要运行它。

无法读取、超大或损坏的配置明确报错；非法主题路径、无效颜色、无法往返的编码、
超范围字号等在接纳新文档之前拒绝。当前模型、撤销状态和保存文件不因此改变。
相较旧版在后台弹出配置错误窗口，这里直接返回可显示的错误，不留隐藏模态窗口。

测试入口：

```powershell
python -B run_tests.py --suite qt-preferences --verify-deployment ../FreeShip-Qt/dist --output D:/FreeShip26Tests/reports/preferences-check
```

测试源码 `qt_preferences.py` 保存在测试项目。它逐字段比较旧版和 C++ 结果，
再在私有运行包中移除 Pascal 可执行文件，执行打开、新建、编辑、撤销/重做、
保存、静水力默认值及 Qt 启动/再次打开回调。还检查配置损坏后的事务回滚。
实际安装目录的清单和文件哈希另行核对。隐藏窗口回调不代表真实桌面视觉、
鼠标、焦点或多设备验收。

完整文档回归继续使用 `qt-document`；三个现有船型的完整快照、场景输入、
型线图和 DXF 对照使用 `qt-publication`，不放宽几何或数值预期。
DXF 调色板生成脚本 `maintenance/extract_dxf_palette.py` 同样保留，输出包含
来源指纹和原作者声明。没有使用 ShipCAD 的配置或翻译代码。
