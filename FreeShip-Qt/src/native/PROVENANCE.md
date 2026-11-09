# 平面、样条计算来源

这批代码只供独立数值验证。没有接入正式模型、绘图、FBM 保存或 DXF
导出，也没有搬入 ShipCAD 的界面、QObject、OpenGL、文件操作、Eigen 或内存池。
库本身仅依赖 C++17 标准库。

## 原始代码与许可

`geometry.h`、`plane.cpp`、`spline.cpp` 改编自
[gpgreen/ShipCAD](https://github.com/gpgreen/ShipCAD/tree/4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd)
的 `ShipCADlib/plane.{h,cpp}`、`ShipCADlib/spline.{h,cpp}`，固定提交为
`4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd`。本地参考副本是工作区内的
`References/ShipCAD`，本轮未修改。

这些源文件保留 Greg Green（2015）的移植署名，以及 Martijn van Engeland
（2005）的 FREE!ship 原始署名；文件头许可是 **GPL-2.0-or-later**。
本目录的改编文件保留上述署名和许可通知。完整 GPL 第 2 版文本见
[COPYING](../../COPYING)；ShipCAD 仓库原始许可文件副本见
[third_party/shipcad/LICENSE](../../third_party/shipcad/LICENSE)。

兼容基准是当前工作区的 FreeShip Plus，不是 ShipCAD 随附的 FREE!ship 2.6。
核对了 `Units/FreeSpline.inc`、`Units/FreeGometry_Functions.inc`、
`Units/FreeTypes.pas` 和所属单元 `Units/FreeGeometry.pas`。
`FreeGeometry.pas` 还注明 Timoshenko V.F.（2006–2011）与 Mark Malakanov
（2015，FPC/Lazarus 转换）的贡献，本目录文件头一并记录。
旧样条源码自身注明其三次样条解法来源于 *Numerical Recipes in Fortran 77*
第 107 页，并为弦长参数化与折角做了修改；这里保留这一算法来源记录，
没有另行引入该书代码或库。

2026-11-01 核对时的 SHA-256：

| 源文件 | SHA-256 |
| --- | --- |
| ShipCAD `plane.h` | `f1c04d85c6287137e0fe2e942a661eb9c41d02f7669b83a5c08179a7f939aead` |
| ShipCAD `plane.cpp` | `6b09f9b2b129ad64523dbe026d2662519232807821bcbea3d65f16b3d078101c` |
| ShipCAD `spline.h` | `6b73a06e1f2db98fd61e1b9cbd13545f39529545a6a46415a61d6c0b010211f8` |
| ShipCAD `spline.cpp` | `d5019b40722842437ef874c16650a73a0ef352cc1b508339319f21b8651bcfcc` |
| ShipCAD `LICENSE` | `ab3fe34d3c62e3348015a4dd0128b09a5da2024d1467a478311003373c18a3f0` |
| Plus `FreeSpline.inc` | `0286282e55fe6de01fd5d8f52b2cc1928030d4ee3721659a32880e8ddb824c10` |
| Plus `FreeGometry_Functions.inc` | `a8a3db0cc3ababfe6e4878a29b02047b16170a542454d00bd27645319780b492` |
| Plus `FreeTypes.pas` | `9d3783848296bf5b6c1f05c98ef2fb54fe662ae0a6e9fdfb70bfb6c2b90bbe69` |
| Plus `FreeGeometry.pas` | `22a8e8bfe3c0808622aef49dc3536752f410dc1cd8799983c53588d7b05e1c40` |

## 保留与调整

平面部分采用原有系数、投影、镜像、八角盒判断和直线求交结构。
平面约定始终为 `a*x+b*y+c*z+d=0`；`evaluate` 返回代数值，非单位法向量时
不能将它当作真实距离。三点构造仅归一化一次，共线时返回无效平面。
直线返回四种明确状态：`Invalid`、`Parallel`、`Coplanar`、`Point`。
平行时用绝对物理距离判断共面，避免 ShipCAD 中负侧远处也被判为共面；
容差默认 `1e-5` 个坐标单位。直线求交没有对应的当前 Plus 函数，
由独立数学用例验证，不计作 Pascal 差分覆盖。这一新 API 的点积、
代数值、法向量长度、求交参数和坐标在 double 中计算，最终输出仍为 float。
这样 `normal=(1e-18,0,0)`、`direction=(1e-30,0,0)` 的非零点积不会因
Single 乘法下溢被误判成平行；坐标使用未舍入的参数计算，最后再转为 float。
点积与代数值的短求和还保留舍入余量，避免 `1+极小值-1` 丢失中间的小项。
独立用例用二进制幂构造：法向量 `(2^60,2^-60,2^60)`、方向
`(2^-60,2^-100,-2^-60)`、起点 `(0,1,0)`，其参数恰为 `-2^100`，
交点恰为 `(-2^40,0,2^40)`；这些输入输出均可由 float 精确表示。
此处未改变旧投影、镜像、盒相交与样条的运算精度。

样条保留弦长参数化、自然边界三对角求解、折角输入、求值、旧式一阶有限差分、
二阶导插值和采样求交。按当前 Plus 对齐了以下差异：

- 弦长采用欧氏距离，移除 ShipCAD 多包的一层平方根。
- `Value` 的 `A`、`B` 分别计算；不以 `1-A` 替代 `B`。
- 保留 `A3`、`B3`、`H2` 的 Single 中间赋值，双端折角的区间按线段计算。
- 反代时将折角处二阶导清零，重复点处沿用 Plus 的间距阈值分支。
- 一阶导仍为参数两侧 `1e-3` 的有限差分，端点为单侧差分，未替换成解析导数。
- 求交每次清空传入数组；保留 Plus 的采样顺序和近零/跨号可能产生的重复项，
  不采用 ShipCAD 的近参数去重。采样交点仍是近似交点，没有偷偷改成新求根算法。
- `Value` 不截断参数到 `[0,1]`，保留原来的外推行为。

`Vec3`、系数、节点参数与曲线二阶导使用 `float`，对应 Plus 的 `Single`。
实际基准 FPC 3.2.2 Win64 的 `Extended` 是 8 字节，故 `Value` 参数使用
`double`，不采用 MinGW 的 16 字节 `long double`。这不是通用平台精度声明；
移植到其他编译器/平台后必须重新核对。

首轮差分确实查出了 C++ 字面量的中间精度差异：`Rebuild` 中默认 double
的 `2.0`、`1.0`、`6.0` 会抬高部分运算精度。改成 float 常量后，与旧式
Single 运算相符。例如固定样本 `spline-seeded-1` 的 `second[1].x`
从 `0.007363606709986925` 回到 Pascal 的 `0.0073636057786643505`。
这里修的是运算精度，不是扩大测试容差。编译禁用 fast-math 与浮点融合。
私有 Pascal 构建的反汇编进一步确认：`Rebuild` 中相关运算和 `H2=H*H/6`
使用 `mulss/addss/subss/divss`，而一阶差分的 `t±1e-3` 使用 double
加减后再存回 Single。证据保存在统一测试目录
`reports/native-geometry-oracle-20261101-b/build/FreeSpline-Rebuild-asm.txt`
与 `FreeSpline-Value-FirstDerive-asm.txt`，未为迁移改动旧源码。
完整验证结论、测试输入和构建指纹由统一测试报告记录，不以这份来源说明替代。

## 明确的边界行为

这些属于候选 API 的边界处理，不冒充旧实现逐位一致：

- 非有限输入拒绝；输入数量最多一百万个点/采样段。计算溢出拒绝，
  不将 NaN/Infinity 当成有效坐标返回。浮点存储精度未改为 double。
- 全零法向量、法向量平方和在 Single 内溢出或下溢到零时，平面无效。
  投影/镜像返回空值；求交返回 `Invalid`；求代数值、盒相交拒绝无效平面。
  盒子的 min 大于 max 时拒绝，零方向直线返回 `Invalid`。
- 直线求交最终参数或坐标超出 float 有限范围时拒绝；非零 double 结果
  转为 float 时若丢失为零，也明确拒绝。不会静默返回错误的平行关系或
  无法表示的求交参数。
- 空/单点样条的求值与导数为零；单点的只读参数数组是 `{0}`。
  少于两个点的求交返回空，避免对虚构的原点给出交点。
- 默认采样段数保留为零（Plus `Clear` 的默认值），求值不需要采样段数。
  两点及以上的采样求交要求显式设置正段数，零段数抛错，避免仅查起点却
  返回看似完整的求交结果。调用者复用的输出在拒绝前也会清空。
- 一阶有限差分遇到零长度参数区间会抛错；不会把除零结果返回给调用者。
- 修改点或折角只标记系数失效，下次读取时重建一次。设置相同值不重建；
  修改采样段数不使曲线系数失效。`built`、`rebuildCount` 可用于验证这一点。
  重建先在局部数组内完成，失败不会发布半份结果。

上述第一批未改写样条简化、曲率、曲线拼接、细分曲面或局部增量算法；
后续模块的迁移范围另见 `docs/migration/STATUS.md`。

## 后续：显式样条简化

`Spline::simplify` 直接按当前 Plus 的 `Units/FreeSpline.inc` 中
`TFreeSpline.Simplify` 移植加权删点顺序、严格阈值与相邻权重更新。
本方法没有使用 ShipCAD 的简化实现。来源继续遵循 GPL-2.0-or-later。

候选有两处有意不同：自行取得弦长，不依赖调用者先读取曲线；用不可删除标记
保护折角，避免原有限权重在大尺度下失效。无缓存时只累加弦长，不提前解马上
会失效的样条系数。非法阈值和浮点溢出抛错，失败保留原对象。
该方法仍是显式、未接入应用的候选，原权重并非几何误差上界。对照与差异见
`docs/migration/NATIVE-SIMPLIFY.md`。
