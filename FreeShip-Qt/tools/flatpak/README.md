# FreeShip 26 v0.1.17：本地 Flatpak 测试版

适用于 x86_64。产品从公开发布标签 v0.1.17（e43063dacab127142e624741077fe45c02689a85）构建；不使用开发工作树的未提交内容。应用标识 `io.github.jiangshirong.FreeShip26`，分支 `test`。只有这一个测试分支，没有公开软件源。包内带有 Wine 9.0、32/64 位组件及产品发布清单中的外部程序；不调用宿主机 Wine。

## 安装、启动、卸载

先在 Linux 桌面上装好 Flatpak。精简的 Debian/Xfce 系统还应装 `xdg-desktop-portal` 和 `xdg-desktop-portal-gtk`，再重新登录桌面，使文件选择器和文档访问服务可用。进入安装文件所在目录，用普通用户执行：

```sh
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.kde.Platform/x86_64/6.10 org.freedesktop.Platform.Compat.i386/x86_64/25.08
flatpak install --user ./FS26-v0.1.17-x86_64.flatpak
flatpak run io.github.jiangshirong.FreeShip26//test
```

第一次安装共享运行库需要联网；模型计算在禁止应用联网时也可用。已经有相同共享库的用户可以复用它们。应用显示安装大小约 513 MB；本次首轮外部计算另产生约 435 MiB 的用户 Wine 目录。运行库大小不算在安装文件内。测试包不调用 Ubuntu `.deb` 更新辅助程序，“关于”中不能从现有 Windows 或 Ubuntu 更新服务器升级这个包。后续测试包需要单独安装；尚未设置 Flatpak 更新源。

```sh
flatpak uninstall --user io.github.jiangshirong.FreeShip26//test
```

卸载命令保留用户设置和文件。应用安装在 `~/.local/share/flatpak/app/io.github.jiangshirong.FreeShip26`（系统安装则在 `/var/lib/flatpak/app`）；设置与私有 Wine 目录在 `~/.var/app/io.github.jiangshirong.FreeShip26/` 下。

## 复现构建

在干净的 Ubuntu 24.04 x86_64 构建目录中，准备 Git、G++ 13、CMake、Ninja、Python 3、curl、xz、patchelf、Flatpak，以及 Qt 6.8.3 开发组件。Qt 前缀用本机实际路径替换。构建目录应预留数 GB 空间；工作目录中同名 `source` 必须是指定标签的干净检出。

```sh
export CMAKE_PREFIX_PATH=/srv/freeship-lab/qt/6.8.3/gcc_64
bash build.sh /tmp/fs26-flatpak-build
```

脚本检查产品提交和 Wine 下载的 SHA-256，按发布清单校验求解器，创建独立仓库并导出本地包。脚本输出 `FreeShip26-0.1.17-linux-x86_64.flatpak`；本次交付文件名为 `FS26-v0.1.17-x86_64.flatpak`。本轮测试先导出仓库验证，最终用同一仓库提交执行 `flatpak build-bundle`。构建用 Qt 6.8.3、CMake 3.28.3、Ninja 1.11.1、G++ 13.3；运行用 KDE 6.10 中的 Qt 6.10.3。重新导出可能产生新的时间戳与包哈希。

## 复现自动验证

安装本包，准备 Xvfb、xauth、Python 3 和 dbus-run-session。交付目录的 `qa-0117.tar.gz` 内含统一测试入口、三份船模副本及冻结的数值基准。以普通测试用户运行，输出目录需全新且剩余空间超过 3 GiB。

```sh
mkdir qa-0117 && tar -xf qa-0117.tar.gz -C qa-0117
sudo mkdir -p /srv/freeship-lab/reports
# 由管理员给专用测试用户分配一处可写的报告父目录
sudo install -d -o "$(id -un)" -g "$(id -gn)" /srv/freeship-lab/reports/fs26-flatpak-test
xvfb-run -a dbus-run-session -- python3 qa-0117/run_tests.py \
  --suite flatpak-calculations --suite flatpak-core --suite flatpak-solvers --suite flatpak-native \
  --output /srv/freeship-lab/reports/fs26-flatpak-test/run-001
```

回归运行时只额外开放本轮模型副本及输出目录。`flatpak-calculations` 经过界面计算回调，`flatpak-solvers` 和 `flatpak-native` 通过安装包内的产品文档服务；四者均执行包内真实程序。界面回调不等于人工桌面验收。测试详情见交付目录的功能支持表及原始记录。

## v0.1.17 单标题栏测试修订

这个可选修订保留原发布包和 test 分支名。Linux 主窗口使用自绘标题栏，其他对话框仍由系统装饰。Debian 13.7、Xfwm4/X11 的独立虚拟显示器上，旧版相同环境顶部装饰为 24 像素；修订版为 0，双击还原、拖动、边角缩放和再次最大化通过。未做 Wayland 桌面验收。未配置 Flatpak 在线更新源；原有 Windows 和 .deb 更新渠道不适用于本包。

完整重建：使用与原包相同的 Qt、Wine 和基础构建要求，选择全新工作目录，设置 FS26_CAPTION_OVERLAY 指向含本次 studio_caption.cpp 与 studio_ui.cpp 的目录，再执行 build.sh。脚本按 SHA-256 确认这两个文件，在独立源码副本中覆盖，其他源码严格使用 v0.1.17。示例：

```sh
export CMAKE_PREFIX_PATH=/srv/freeship-lab/qt/6.8.3/gcc_64
FS26_CAPTION_OVERLAY="$PWD" bash build.sh /tmp/fs26-caption-clean
```

本次产物实际复用了原包验证过的编译对象，仅重新编译两个变更源文件、重新链接主程序，并从原提交 dbf5b5ac4b4f5a8756e2d1bece5bfd8fa08281fad9ade6323bcfbc642b79a09e 复制安装树。定向构建记录见 caption-build2.sh。新测试提交为 0a20a7fd1d29b3bd61e44c67c40a202963fcb51f2c019c6930877775b0b31c5e。

验收入口：在具备 Xvfb、xfwm4、xdotool、xprop 且已完成首次语言设置的测试账户下，运行 `python3 run_tests.py --suite flatpak-caption --output /srv/freeship-lab/reports/未使用的目录名`。独立安装时先设置 FLATPAK_USER_DIR 指向该安装目录。检查结果见 caption-report.json 和 caption-geometry.log。

## 从 0.1.17 升级到 0.1.18 的桌面测试

香港服务器的 Debian 测试桌面上有一个单独的 **FS26 更新测试** 图标。它启动用户范围的 `io.github.jiangshirong.FreeShip26//update-test`。测试起点沿用 0.1.17 的船模功能，补入更新按钮和辅助程序。原来的 `test` 分支和单标题栏安装仍可单独启动。

在更新测试窗口打开 **关于 → 更新**。确认更新后，首次使用可能需要在系统对话框中允许更新。看到 **重新启动** 后点击，并按提示处理未保存的船模。重启完成后在 **关于 → 版本信息** 核对程序版本 **0.1.18**、目标操作系统 **Linux**。早期起点处理过未保存模型时，可能会在重启后列出恢复副本，可以选择 **稍后处理**。

构建时设定 `-DFS26_FLATPAK=ON`。发布测试分支使用 `build-update-branch.sh`，最新版本由 `FS26_TARGET_VERSION` 指定，`FS26_FLATPAK_REVISION` 随测试包递增。脚本从已验证的单标题栏安装树复用 Wine 和运行依赖，再放入本轮编译的程序、工具和资源。签名仓库与原 `test` 仓库独立。用 `write-update-index.py` 生成版本清单时，它会核对签名仓库的提交、包内版本和 Windows 对应版本的签名说明。

测试清单位于 `https://updates.freeship26.work/freeship/flatpak/update-test/x86_64/latest.json`。香港测试环境可以读取该地址。实际安装由 Flatpak 的桌面更新接口完成。只有部署后的程序身份与清单中的提交、版本和分支一致时，应用才提示重启。

统一测试入口包含 `flatpak-update-desktop` 和 `flatpak-work-features`。桌面用例在专用测试账户中点击真实按钮，验证重启、版本信息及从第二个进程打开船模。产品用例可配合 `flatpak-core`、`flatpak-native`、`flatpak-calculations` 和 `flatpak-solvers` 运行。交付目录随附完整构建输入、命令、截图与结果。
