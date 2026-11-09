# C++ IGES 曲面准备

2026-11-03。沿用“文件 → 导出模型 → IGES 曲面”窗口和现有写出器；
将控制网到 NURBS 曲面片的准备步骤迁入 C++。

`src/document/cad_patches.cpp` 从只读 FBM 快照读取控制网，在私有细分曲面上
进行一级 Catmull–Clark 细分。矩形控制网直接映射到细分后的顶点、边中点和面中心，
不再像旧 `ConvertToGrid` 那样逐个换起点、重试拼接。非四边形仍按一级子面生成曲面片。
图层稳定 ID、节点向量、控制点按原协议交给 `src/core/cad_export.cpp`。

保留原转换的边界补点、极限点、三次曲面及节点插入顺序。计算仍使用原 Single
存储和原有中间舍入；没有降低曲面精度，也没有把网格面冒充 NURBS 曲面。
这仍是原算法的曲面近似，不保证非常规顶点附近与极限细分曲面精确等价，
也不把“CAD 内核能读回”当作片间连续性、缝合实体或加工精度验收。

两个旧问题有明确行为变化：

- `FreeSubdivisionSurface.inc::AssembleFacesToPatches` 先排除了不可见图层，
  使写出器的“包含隐藏图层”拿不到这些曲面。现在准备所有图层，写出时按选项筛选。
- 同一函数沿光滑边遍历时不检查图层，可能把相邻图层拼成一块，再用第一面的
  图层属性表示整块。现在跨图层停止拼接，分别保留颜色、可见性和对称属性。
  光滑边的边界补点仍从完整曲面读取，不人为改成折痕。

请求入口为 `FreeShipDocumentService::cad_iges`，响应标记 `analysis_engine=cpp-cad`。
取消、90 秒限制、曲面数量限制及非有限值检查均在发布前执行，结果原子写入。
Qt 在取消后也拒绝已经到达队列的成功结果，避免取消后继续写 IGES。
桥接服务拒绝外部文档的 `cad_iges`，独立旧版入口只用于兼容性对照。

测试保存在 `FreeShip-Tests/qt_native_cad.py`，并入统一入口 `qt-cad`。
包括原曲面控制点/节点对照、解析平面面积、非四边形、孔洞、图层边界、隐藏层、
编辑/撤销/重做、停止私有 Pascal 适配器后生成、失败后的恢复，以及延迟结果取消。
既有 `qt_cad.py` 的实际 Qt 回调、单位/镜像、独立 IGES/DXF 解析和 OpenCascade
读回继续使用。隐藏窗口回调不代替用户桌面操作与商业 CAD/加工验收。

来源为 FREE!ship / Plus 的 `FreeEdit.inc::File_ExportIGES.ProcessGrid`、
`FreeSubdivisionSurface.inc::AssembleFacesToPatches`、`FreeSubdivisionPoint.inc::FGetLimitPoint`
和 `FreeNURBSurface.inc`，沿用 GPL-2.0-or-later 及原作者署名；本模块未引用 ShipCAD。

完整回归报告：`D:/FreeShip26Tests/reports/cad-native-core-01/qt-cad/report.json`，
562 pass、0 fail、1 review、1 not_run、0 error。三个现有船型及五个独立几何样例的
曲面片布局、全部控制点和节点逐值一致，无比较容差。9a 的曲面准备请求本次约
11.7 ms，独立旧版入口约 53.2 ms；含 JSON 写盘和进程通信，不含 IGES 文件写出，
单次数据不作为稳定帧率或性能保证。通用编辑/保存回归为
751 pass、0 fail、9 review、7 not_run、0 error。

安装与启动证据由 `FreeShip-Tests/maintenance/cad-native-installed-20261103.json`
记录；各计数中的 review/not_run 保留其实际边界。
