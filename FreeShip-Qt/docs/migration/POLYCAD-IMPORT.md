# Polycad 控制网导入

2026-11-04。入口是“文件 → 导入模型 → Polycad 控制网”。解析、文档创建、
编辑和保存均走 C++。现有 OBJ/STL/FEF 入口保持，旧 Lazarus 工程未修改。

## 读取范围

支持旧版读取的四种段落：`TBSplineSurface`、`TBSplineSurfaceList`、
`TYachtLines`、`TShipLines`。读取 NumU/NumV 坐标网格、多曲面列表和曲面名称；
Yacht 的 Loa/BMax/Tc、Ship 的 Lbp/Beam/Draught 用于主尺度。
输入文本使用 UTF-8；FBM 名称仍遵守偏好中的文件编码，不可表示的文字会报错，
不会静默替换成问号。中文名称可以在选择 UTF-8 编码后保存和重开。

单位由用户明确选择 m、mm、cm、ft 或 in；英尺保留英制，其余换算为米。
坐标按文件中的 X/Y/Z 读取。半船镜像默认开启，与旧入口一致；完整船体应取消勾选。
多曲面共享坐标只有在原计算精度下完全相同时才合并，不用距离容差吞掉近邻点。
内部边保持平滑、边界保留折痕，退化极点可形成三角面。

这里读取的是细分控制网。阶次、节点和船型生成参数不参与曲面重建，
因此不能把导入后的细分曲面视为原 Polycad B 样条曲面的无损副本。
Gmsh 也使用 `.geo` 扩展名，但不属于该格式；项目现有两份 `OUT.GEO`
就是 Gmsh 文件，会明确拒绝。

未知对象、未知参数、缺失坐标、曲面数量不符、非有限数、重复面和非流形连接
都会拒绝整个导入。输入先完成校验和独立文档构建，成功后才替换当前工程。
导入后是新未保存工程，可编辑、撤销、保存 FBM 并重新打开。

## 旧流程中处理的问题

位置均相对 `FreeShip-Plus-Lazarus`；修正只在新 Qt 工程中实现。

- `Units/FreeEdit.inc::File_ImportPolycad`：读到段落后先 `FreeShip.Clear`，
  后面才验证坐标。新流程先读完整文件，失败时活动模型、场景和 DXF 保持不变。
- 同一函数的 Yacht/Ship 分支把循环终点设到整个文件末尾，可能吃掉后续段落；
  NumU/NumV 也没有按段落独立初始化。新解析器在每个标题处结束并核对上一段。
- SurfaceList 分支读取 `NumSurfaces`，但未据此核对实际曲面数。
  新实现要求数量完全吻合，缺少或多余数据均报错。
- `NextParameter` 依赖空格位置，最后一个数字及制表符的处理容易出错。
  新解析器明确要求每行三个有限坐标，允许常规空格、制表符与 CRLF。

代码：`src/core/model_exchange.cpp::polycad`；入口和参数窗口为
`src/app/main_window.cpp`、`src/app/exchange_dialog.cpp`。
转换后的控制网沿用既有原生文档创建和保存流程。

## 验证

`FreeShip-Tests/qt_polycad.py` 已接入统一入口：

```powershell
python -B run_tests.py --suite qt-polycad --verify-deployment ../FreeShip-Qt/dist
```

167 项通过，失败与错误为 0；1 项视觉复核和 2 项未执行单列。
独立坐标样本覆盖四种段落、非平面四边形、拼接缝、极点、近邻点、单位、
主尺度、编码、数量上限和坏文件。实际 Qt 回调覆盖空白启动、已有模型、
导入失败、导入成功及保存重开。文档服务还检查点编辑、撤销、完整派生曲面
往返以及无 Pascal 调用。窗口截图已检查，未操作用户桌面。

原 `qt-exchange` 的 280 项通过，保留 OBJ/STL/FEF 解析、导出几何和生产回调回归。
报告保存在 `C:/FreeShip26-PolycadTests`，安装核对记录为
`FreeShip-Tests/maintenance/polycad-installed-20261104.json`。
外部 Polycad 软件及真实导出样本尚未提供；不能据合成样本宣称所有 Polycad 版本已兼容。
