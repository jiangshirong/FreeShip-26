# JPG 背景图释放后仍读取属性

状态：已修复，验证范围见下文及第二批总记录。优先级：P1。2026-10-29 核对。

以下问题描述为修复前核对，修改进展见文末。

打开 JPG 背景图时，程序先把图片复制进位图，然后调用 `FreeAndNil(JPEGImage)`，紧接着又读取 `JPEGImage.CompressionQuality`。此时引用已经被清空，这条路径存在明确的空引用访问。

应在释放前保存压缩质量，并用 `try…finally` 管理图片对象。新背景图读取成功后再替换旧图，读取失败时保留原图。验证正常 JPG、损坏 JPG 和重复更换背景图，检查异常恢复与对象释放。

代码：`Units/FreeBackgroundImage.inc`，`TFreeBackgroundImage.Open`，第 335–342 行。见[固定上游版本](https://github.com/markmal/freeship-plus-in-lazarus/blob/1de17e99fb2b1cc559cdf27c5cafb595a627a641/Units/FreeBackgroundImage.inc#L335-L342)。当前源码仍是同样的顺序。

这是沿着资料 U08、GH-101 的背景图反馈另外查到的问题，不能说原帖已经报告了这次空引用。已核对上游和当前代码，尚未做桌面导入复现。

## 第二批修复（2026-10-30）

`FreeBackgroundImage.inc/TFreeBackgroundImage.Open` 先把新图片完整解码到临时位图，在释放 JPEG 前读取质量属性，并在 `finally` 中释放对象。实际菜单入口 `FreeEdit.inc/TFreeEdit.BackgroundImage_Open` 同样先准备新背景数据和尺度，再替换旧图，避免只修一个入口。

入口：`python -B FreeShip-Tests/undo_background_safety.py`。两个入口均检查 JPG/BMP 的像素、尺寸、质量和重复更换；损坏 JPEG 或取消选择后，旧图、项目背景数据及修改状态应保持。文件选择由隔离适配器提供，未做桌面导入验收。最终源码及报告由第二批总记录统一归档。

最终验证、剩余范围与程序更新见[第二批修复记录](第二批修复-计算与异常处理.md)。
