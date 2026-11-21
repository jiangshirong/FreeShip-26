#!/usr/bin/env bash
# 在 Ubuntu 24.04 x86_64 构建本地测试包。用法见同目录 README.md。
set -euo pipefail
work=$(realpath -m "${1:?请指定构建目录}")
mkdir -p "$work"
cd "$work"
id=io.github.jiangshirong.FreeShip26
commit=e43063dacab127142e624741077fe45c02689a85
wine_hash=cf0c09d4346dc10bc92ab674936292cff47eeb71ca7604b8e6303b7bdb97e2f6
wine_source=https://github.com/Kron4ek/Wine-Builds/releases/download/9.0/wine-9.0-amd64.tar.xz
[[ $(uname -m) == x86_64 ]]
if [[ ! -d source ]]; then git clone --depth 1 --branch v0.1.17 https://github.com/jiangshirong/FreeShip-26.git source; fi
[[ $(git -C source rev-parse HEAD) == "$commit" && -z $(git -C source status --porcelain --untracked-files=all) ]]
qt=$work/source/FreeShip-Qt
# An optional, pinned two-file overlay for the v0.1.17 Linux caption rebuild.
# Use a separate work directory; never apply the overlay to the release checkout.
if [[ -n ${FS26_CAPTION_OVERLAY:-} ]]; then
    overlay=$(realpath "$FS26_CAPTION_OVERLAY")
    (cd "$overlay" && sha256sum -c - <<'SUMS'
6bd796e234db37336d5d8cef01b4c945b0e79d7a45a86c82022852bdac6f4e2a  studio_caption.cpp
437e51c44d03ada6c8c00fc26f1413e075742a5ad99c7f84cfe21f753081acda  studio_ui.cpp
SUMS
    )
    mkdir -p "$work/caption-source"
    git -C source archive "$commit" FreeShip-Qt | tar -x -C "$work/caption-source"
    qt=$work/caption-source/FreeShip-Qt
    install -m644 "$overlay/studio_caption.cpp" "$overlay/studio_ui.cpp" "$qt/src/app/"
fi
programs=(FreeShipQt FreeShipDocumentService FreeShipNativeLinesplan FreeShipGeometryExport)
cmake -S "$qt" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "${BUILD_JOBS:-4}" --target "${programs[@]}"
if [[ ! -f wine9.tar.xz ]]; then curl -fL --retry 2 "$wine_source" -o wine9.tar.xz.part; mv wine9.tar.xz.part wine9.tar.xz; fi
printf '%s  wine9.tar.xz\n' "$wine_hash" | sha256sum -c -
stage=$(mktemp -d "$work/stage.XXXXXXXX")
app=$stage/files
product=$app/lib/freeship26
install -d "$product" "$app/bin" "$app/wine" "$app/share/applications" "$app/share/mime/packages" "$app/share/icons/hicolor/scalable/apps"
install -d "$app/etc" "$app/lib/i386-linux-gnu"
printf '%s\n' /app/lib/i386-linux-gnu > "$app/etc/ld.so.conf"
for program in "${programs[@]}"; do
    install -s "build/$program" "$product/$program"
    patchelf --remove-rpath "$product/$program"
done
cp -r "$qt/runtime-data/bridge" "$product/bridge"
cp -r "$qt/src/app/languages" "$product/languages"
cp -r "$qt/examples" "$product/examples"
cp "$qt/COPYING" "$qt/NOTICE.md" "$product/"
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$qt/tools" python3 - "$qt" "$product" <<'PY'
import json, shutil, sys
from pathlib import Path
from calculation_tools_delivery import approved_payload, validate_public_payload
source, dest = map(Path, sys.argv[1:])
files = approved_payload(source / 'third_party/hydronship')
validate_public_payload(files)
for name, path in files.items():
    target = dest / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, target)
PY
printf '{"release_version":"0.1.17","source_commit":"%s","profile":"flatpak","wine":"9.0","wine_sha256":"%s"}\n' "$commit" "$wine_hash" > "$product/build-manifest.json"
tar -xJf wine9.tar.xz --strip-components=1 --no-same-owner --exclude='*/include/*' --exclude='*.a' -C "$app/wine"
install -Dm644 /usr/share/common-licenses/LGPL-2.1 "$app/share/licenses/wine/COPYING.LIB"
printf '%s\n' "$wine_source" 'https://gitlab.winehq.org/wine/wine/-/tags/wine-9.0' > "$app/share/licenses/wine/SOURCES"
cat > "$app/bin/freeship26" <<'SH'
#!/bin/sh
unset WINEPREFIX
export FREESHIP_WINE=/app/wine/bin/wine
exec /app/lib/freeship26/FreeShipQt "$@"
SH
chmod 755 "$app/bin/freeship26"
cp "$qt/src/app/assets/brand/freeship26.svg" "$app/share/icons/hicolor/scalable/apps/$id.svg"
cat > "$app/share/applications/$id.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=FreeShip 26
GenericName=Ship hull design
GenericName[zh_CN]=船体设计
GenericName[ru]=Проектирование корпуса судна
GenericName[ja]=船体設計
GenericName[de]=Schiffsrumpfentwurf
GenericName[fr]=Conception de coques
GenericName[es]=Diseño de cascos
Comment=Hull modelling, lines plans and naval architecture calculations
Comment[zh_CN]=船体建模、型线图绘制和船舶设计计算
Comment[ru]=Моделирование корпуса, теоретические чертежи и судостроительные расчёты
Comment[ja]=船体モデリング、線図作成、船舶設計計算
Comment[de]=Rumpfmodellierung, Linienrisse und schiffbauliche Berechnungen
Comment[fr]=Modélisation de coques, plans de formes et calculs d’architecture navale
Comment[es]=Modelado de cascos, planos de formas y cálculos de arquitectura naval
Exec=freeship26 %f
Icon=$id
Categories=Science;Engineering;
MimeType=application/x-freeship-fbm;
Terminal=false
EOF
cat > "$app/share/mime/packages/$id.xml" <<'EOF'
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-freeship-fbm">
    <comment>FreeShip model</comment>
    <comment xml:lang="zh_CN">FreeShip 船模</comment>
    <comment xml:lang="ru">Модель судна FreeShip</comment>
    <comment xml:lang="ja">FreeShip 船体モデル</comment>
    <comment xml:lang="de">FreeShip-Schiffsmodell</comment>
    <comment xml:lang="fr">Modèle de navire FreeShip</comment>
    <comment xml:lang="es">Modelo de buque de FreeShip</comment>
    <glob pattern="*.fbm"/>
    <glob pattern="*.FBM"/>
  </mime-type>
</mime-info>
EOF
cat > "$stage/metadata" <<EOF
[Application]
name=$id
runtime=org.kde.Platform/x86_64/6.10
sdk=org.kde.Sdk/x86_64/6.10

[Extension org.freedesktop.Platform.Compat.i386]
directory=lib/i386-linux-gnu
version=25.08
EOF
flatpak build-finish --command=freeship26 --socket=x11 --socket=wayland --share=ipc --device=dri --allow=multiarch "$stage"
flatpak build-export --arch=x86_64 repo "$stage" test
[[ ${EXPORT_ONLY:-0} != 1 ]] || exit 0
bundle="$work/FreeShip26-0.1.17-linux-x86_64.flatpak"
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo repo "$bundle.part" "$id" test
mv "$bundle.part" "$bundle"
sha256sum "$bundle" > "$bundle.sha256"
cat "$bundle.sha256"
