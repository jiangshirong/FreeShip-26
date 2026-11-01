# FreeShip UI testing

For changes involving the control-point coordinate menu, labels, or point editor,
run the companion pilot at `../FreeShip-Tests/freeship_ui_test.py` and inspect
its `report.json` and relevant images. It also has `--case toolbar`, `--case layers`,
`--case chinese`, `--case display`, and `--case language` for related UI changes.
See the companion README for usage.
It builds an instrumented copy of the current working tree, not this checkout's
normal executable. A `partial`, `review`, or `not_run` result is not a full pass.
Native mouse input, focus, real window placement, and display behavior are not
covered. Do not use Computer Use or another desktop-operation tool to accept
software changes. Preserve existing user changes and do not change test
expectations merely to match the implementation.

## 可复用测试的积累

开发、修复或排查 FreeShip 时产生的可复用测试、探针、故障注入和样本，应并入同级 `../FreeShip-Tests` 现有测试程序，接入合适的命令行入口和机器可读报告，更新运行说明，并验证整合后的测试。优先扩展已有通用能力；不要把依赖固定控件名称、菜单位置或某次环境的临时脚本当作通用测试。界面测试应采集功能入口、状态和行为证据供 AI 判断；有独立依据的行为预期可以断言。

交付时说明新增或复用的测试、运行入口、实际结果及未覆盖范围。仅适用于当次排查且不适合长期复用的诊断脚本应明确标注；遇到无法可靠整合的测试，应如实说明原因和未完成部分，不得声称已覆盖。继续遵守隔离源码、保护用户数据和 `pass` / `fail` / `review` / `not_run` / `error` 状态的现有约定。
