# 阻力与推进本机更新

2026-11-02。运行入口为 `FreeShip-Qt/dist/FreeShipQt.exe`，使用“计算”菜单。
本次接入范围、旧缺陷、求解器依赖及仍未实现的分支见 [PERFORMANCE.md](PERFORMANCE.md)。
这是本机开发包，不能据此宣称所有专业计算、完整视觉或公开发行已验收。

计算服务构建：`build/performance-service-12`。Qt 沿用现有编译目录；15 个计算
入口由 Qt 管理，公式仍在原 Pascal 单元及外部工具中。旧版 fs、QtPilot、用户
船模和用户配置未切换。本机工具配置只引用已有 Exec 目录，不附送其二进制。

## 验证证据

- `FreeShip-Tests/reports/qt-performance-final-b`：627 pass，0 fail，1 review，
  4 not_run，0 error。包含主方法、九个滑行艇分支、十一类螺旋桨系列、七个原
  入口坐标对照、独立求解器输出、实际 Qt 回调、保存/撤销/重开及全部报告导出。
- `FreeShip-Tests/reports/qt-migration-performance-a`：751 pass，0 fail，9 review，
  7 not_run，0 error。箱体的四视图、型线图、编辑、保存与原场景对照回归。
- 随后的唯一代码改动是消除一处误导缩进警告，没有改变条件或执行顺序。
- `qt-performance-package-a` / `qt-performance-delivery-a`：运行候选目录及正式目录
  自身，核对清单、计算、CSV、场景、DXF 和实际菜单回调。
- `qt-hydro-performance-package-a`：候选包静水力/稳性流程与上轮已保留结果对照。
- `qt-migration-performance-delivery-a`：正式包模型编辑基本流程与本轮回归对照。
- `qt-startup-performance-delivery-a`：正式包的多进程启动恢复、最近文件和保存路径检查。

以上路径均在 `FreeShip-Tests/reports`。正式安装核对记录为
`FreeShip-Tests/maintenance/performance-installed-20261102.json`；该记录及对应报告
决定最终安装状态，不以本说明代替安装完成证据。更新前的运行包保留在
`build/dist-before-performance-20261102`。

## 保留的限制

三个 CALCPROP 任务没有完整数值结果，属于 `not_run`，没有伪装成已通过。
其余一个 `not_run` 是真实桌面操作，`review` 是完整窗口视觉。B 系列的 DOS
求解器、Almeter 空实现、多体船的三个空分派及 Hydrodyn 专用工具仍待处理。
隐藏窗口图像和 PDF 已查看，但不代替用户对鼠标、焦点、多屏和视觉保真的验收。

测试源码、样本、独立观察器和诊断入口都保留在 `FreeShip-Tests`，归入
`qt-performance`；失败的早期诊断报告不作为发布基准。复用构建时仍核对源码
指纹，源码变化后应重新生成运行包，不能强行沿用旧清单。
