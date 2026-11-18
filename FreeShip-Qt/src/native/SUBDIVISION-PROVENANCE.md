# 控制网与细分计算来源

本目录的 `subdivision.h/.cpp` 是独立验证用的 C++17 计算模块。它不拥有正式
文档，不接入绘图、FBM 读写、剖切或静水力。`GeometryInput` 描述当前坐标下的
静态控制网，保留点型、边、面方向、图层和控制曲线；它没有锁点、锚点和编辑
约束，不能作为完整可编辑文档的替代品。

## 采用了什么

数值和拓扑兼容基准是本工作区当前的 FreeShip Plus：

- `FreeSubdivisionSurface.inc` 的逐级 `SubDivide → Average` 流程。
- `FreeSubdivisionPoint.inc` 的 `Averaging` 规则；
  `FreeSubdivisionEdge.inc` 的中点和硬边行为。
- `FreeSubdivisionFace.inc`、`FreeSubdivisionControlFace.inc` 的三角四分与
  四边/n 边分面顺序。
- `FreeSubdivisionControlCurve.inc` 的附面曲线和自由曲线流程。

C++ 的容器、索引、边界检查、结果编号和缓存单独编写，没有移入旧对象体系、
界面、内存池、文件操作或编辑控制器。数学规则按上述实际执行代码重写，
不是把 ShipCAD 全部细分类直接换名后接入。

另对照阅读了固定提交
`4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd` 的
[ShipCAD](https://github.com/gpgreen/ShipCAD/tree/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd)
`subdivpoint.cpp`、`subdivedge.cpp`、`subdivface.cpp`、`subdivsurface.cpp`。
其 C++ 表达有参考价值，但未作为当前数值真值：例如全三角邻域系数用了
`sqrt(...)`，当前 Plus 是 `sqr(...)`，二者不能互换。本模块采用当前 Plus
的平方公式和相应 Single 落点。若以后另选算法，要单独建立精度和结果标准，
不会以“已有 C++ 代码”作为替换理由。

## 署名与许可

当前 Plus 所属 `FreeGeometry.pas` 保留 Martijn van Engeland（2005）、
Timoshenko V.F.（2006–2011）、Mark Malakanov（2015，FPC/Lazarus 转换）
的署名；采用部分是 **GPL-2.0-or-later**。新文件保留这条来源与许可。
完整 GPL 第 2 版文本见 [COPYING](../../COPYING)。

作为交叉参考阅读的 ShipCAD 文件由 Greg Green（2015）移植，仍保留
Martijn van Engeland 的原始署名，同样声明 GPL 第 2 版或后续版本。
参考仓原许可副本见 [third_party/shipcad/LICENSE](../../third_party/shipcad/LICENSE)。

2026-11-01 核对的当前 Plus 文件 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `FreeGeometry.pas` | `22a8e8bfe3c0808622aef49dc3536752f410dc1cd8799983c53588d7b05e1c40` |
| `FreeTypes.pas` | `9d3783848296bf5b6c1f05c98ef2fb54fe662ae0a6e9fdfb70bfb6c2b90bbe69` |
| `FreeSubdivisionSurface.inc` | `0b3980acf6403481b5e263d231740f2314df762105e7e94f0301c9e674a4b723` |
| `FreeSubdivisionPoint.inc` | `34a649ef68eff34a26833eeacbecc3fbe48fbcd14d28346e9af9134bd7b568c7` |
| `FreeSubdivisionEdge.inc` | `4b50111c2a3f9745f0defa8ff93c88d0a36ea331d788bead6ed42ef1fa57fb78` |
| `FreeSubdivisionFace.inc` | `4c65d3e4e0423d80d4b7894d869b39b622074c55b96cde239d7915a46613f7dc` |
| `FreeSubdivisionControlFace.inc` | `936ca09df41aedfe645b58b29bd4ef04903106ec29532b633b0cb33ab73acae5` |
| `FreeSubdivisionControlCurve.inc` | `7378ed9b94e433518dd18a823a2eb56a6dc312db96a82016dbc8e2721c24643d` |
| `FreeSubdivisionControlEdge.inc` | `a7945b0dce481fa3f65e8460addee17b00f9155838bc5addcf2b10646ab56c4c` |
| `FreeSubdivisionControlPoint.inc` | `3e1cc8aa37cedefe69c9d6ed6bb3d8ea4e1e03ed416e0192e9da7571362e9dce` |

## 容易迁错的行为

- 两种模式为 `fmQuadTriangle=0`、`fmCatmullClark=1`。前者保留三角四分；
  后者对三角也生成面心和三个四边面。n 边面先分成 n 个四边面。
- `Surface.Rebuild` 中最终 `LastAveraging` 的整块调用目前被注释。
  本模块不因函数存在而启用它。所有曲面层级均先分面，再同时平均一次。
- 平均必须先计算所有新坐标，再统一替换；不能让后一个点读到前一个点
  已经平均过的坐标。
- 四边子面的循环起点按 `Index0=(I-1) mod 4` 旋转。虽然统一起点仍是
  同一个有向多边形，却会改变后续 Single 累加顺序，所以保留旧顺序。
  点的面邻接和边邻接也按真实创建顺序保存。
- 初始控制网是事实输入。旧 `LoadBinary` 直接读取点型和边硬度，不能
  仿照编辑 setter 根据邻接重新猜测类型。只有自由曲线首轮执行旧流程
  本来就有的端点 Corner、内部 Crease 及边硬度调整。
- 拓扑边界看邻面数，而不是旧 `IsBoundaryEdge` 的 y 坐标阈值；后者
  还含船中线漏边检查的含义。
- 自由曲线在 surface 层级递增前处理：第一轮只复制控制点并平滑，
  第二轮才开始插中点。附面曲线沿曲面顶点和边的后代更新，方向不变。
  曲线样条的折角标记来自最终点型 Corner；本模块不重建或输出样条文件。

坐标、权重、邻接累加和输出均保留 Single/float 落点。编译禁用
fast-math 和浮点融合。尚未遇到的差异不预先用容差吞掉；实际差分、独立
解析例及当前编译器的结果保存在统一测试报告中。

原 Pascal 的私有测试构建也直接核对了系数表和指令。`AA[1..8]` 的
float32 位模式依次为 `3f200000`、`3fd00000`、`3fc00000`、`3fa55555`、
`3f8f84e3`、`3f800000`、`3f6a4615`、`3f5ad2dc`；更大三角价数的动态公式
在 Double 内求余弦，先计算 `sqr(term)` 再乘 `8/3`，最后转回 Single；
C++ 保留该分组，不用左结合的 `8/3*term*term` 替代。证据位于统一测试目录的
`reports/native-subdivision-oracle-20261101-d/build/Point-Averaging-asm.txt`
及同次构建的 `FreeGeometry.o`。这份观察用于核对原运算，不代替数值差分。

## 输出、缓存和边界

第 0 级保留所有输入控制点及其原 ID，控制面的 `childIndex=-1`；便于直接
审计原控制网。更高层级的输出点 ID 按控制面顺序、其子面顺序、面内有向
点顺序首次遇到时分配，再补控制曲线链中的点。编号不依赖内存地址，也不按坐标排序；重合坐标
仍可以是不同顶点。每个面保留根控制面、子面编号和图层；边保留相邻面、
硬度、控制边后代标记和曲线归属。

没有面或曲线引用的控制点、无面且无曲线的控制边另列为
`looseControlPoints/looseControlEdges`，不假装它们不存在。图层的可见性、
对称和计算标记原样保留，不据此删除控制网或自动创建镜像副本。
`minimum/maximum` 只包围实际面/曲线引用的输出点，不冒充旧 Surface
最后覆盖为控制网范围的 `Min/Max`；第 0 级中未被引用的原控制点同样不参与范围。

层级 0–4 与当前 Plus 的可选细分层级一致。查询按需生成并缓存；请求
已经生成的层级不会再算，向上请求复用上一层计算网。改变一个控制点时
使派生层级失效，下次查询再重建；本轮尚未实现邻域局部增量更新。
设置相同坐标不会失效。内部只保留用于继续细分的最新计算网，既有层级
以只读规范结果保存。失败不会发布一半新层级。

有限值、索引、面内重复点、重复边、曲线所有权、层号及资源数量均检查；
非有限结果拒绝，不输出 NaN/Infinity。几何重合但 ID 不同的点不会被合并。
当前最多两百万个点/边/面项，每个输入面最多 4096 个点。

以下输入目前明确拒绝，而非悄悄用近似结果替代：

- 非流形边（超过两个邻面）。
- 一条曲线同时经过附面和游离控制点，或用无面边跨接两个曲面上的点。
- 一条曲线重复经过同一条边，或曲线链与边记录的归属不一致。

这些限制是候选模块的覆盖边界，不代表旧程序都禁止这些输入。
