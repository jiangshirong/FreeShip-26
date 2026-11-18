# FBM 单字节编码表

`fbm_codepages.h` 由本机 Lazarus LazUtils 的 `commoncodepages.inc` 生成，
保留 31 张表的全部 7,936 个字节到 Unicode 映射。原文件属于 LazUtils，
适用随附 `COPYING.modifiedLGPL.txt`；生成头文件没有改写字符映射。

来源：`FreeShip-MCP/.toolchain/lazarus/components/lazutils/commoncodepages.inc`。
文件 SHA256 写在生成头文件开头。生成和审计脚本保留在
`FreeShip-Tests/maintenance/generate_fbm_codepages.py`：默认只核对，
显式 `--write` 才重新生成。统一 `qt-document` 测试会执行只读核对。

FBM 字符串没有文件内编码声明；编辑使用当前会话的 `FbmEncoding`，
拒绝无法往返的字符。UTF-8/UCS-2 独立处理；四种亚洲双字节代码页使用
Windows 编码 API，其他平台用 iconv。当前交付只验证了 Windows 构建。
