#!/usr/bin/env bash
# Assemble a stable bundle with a verified Flatpak runtime tree and native binaries.
set -euo pipefail
base_repo=$(realpath "${1:?verified Flatpak base repository}")
base_commit=${2:?verified base commit}
build=$(realpath "${3:?native Linux build directory}")
out=$(realpath -m "${4:?new output directory}")
version=${FS26_TARGET_VERSION:?release version}
revision=${FS26_FLATPAK_REVISION:?Flatpak revision}
pub_url=${FS26_FLATPAK_REPO_URL:?public update repository URL}
public_key=$(realpath "${FS26_FLATPAK_PUBLIC_KEY:?public signing key file}")
: "${GNUPGHOME:?release signing keyring}" "${FS26_FLATPAK_SIGN_KEY:?release signing key ID}"
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && $revision =~ ^[1-9][0-9]*$ ]]
[[ $base_commit =~ ^[a-f0-9]{64}$ && $pub_url == https://* ]]
qt=$(realpath "$(dirname "${BASH_SOURCE[0]}")/../..")
mkdir "$out"
id=io.github.jiangshirong.FreeShip26
ostree --repo="$base_repo" checkout --user-mode --force-copy "$base_commit" "$out/stage"
payload="$out/stage/files/lib/freeship26"
for name in FreeShipQt FreeShipDocumentService FreeShipNativeLinesplan FreeShipGeometryExport FreeShipFlatpakUpdater; do
    install -s "$build/$name" "$payload/$name"
    patchelf --remove-rpath "$payload/$name"
done
mv "$payload/languages" "$out/previous-languages"
cp -a "$qt/src/app/languages" "$payload/languages"
install -m644 "$qt/COPYING" "$qt/NOTICE.md" "$payload"
# The matching .flatpak base supplies its Qt/Wine runtime, bridge resources,
# examples, calculator dependencies, and desktop metadata.
python3 - "$payload/build-manifest.json" "$qt" "$version" "$revision" <<'PY'
from pathlib import Path
import json, subprocess, sys
p, qt, version, rev = sys.argv[1:]
m = json.loads(Path(p).read_bytes())
commit = subprocess.check_output(['git','-C',qt,'rev-parse','HEAD'], text=True).strip()
if subprocess.check_output(['git','-C',qt,'status','--porcelain']).strip():
    raise ValueError('Build from a clean, identified Git checkout')
m.pop('source_snapshot_sha256',None)
m.update(release_version=version, flatpak_revision=int(rev), source_commit=commit)
Path(p).write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
PY
mv "$out/stage/export" "$out/previous-export"
flatpak build-finish --command=freeship26 "$out/stage"
flatpak build-export --arch=x86_64 --gpg-sign="$FS26_FLATPAK_SIGN_KEY" \
    --gpg-homedir="$GNUPGHOME" "$out/repo" "$out/stage" stable
flatpak build-update-repo --gpg-sign="$FS26_FLATPAK_SIGN_KEY" --gpg-homedir="$GNUPGHOME" "$out/repo"
flatpak build-bundle --gpg-keys="$public_key" --repo-url="$pub_url" \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    "$out/repo" "$out/FreeShip-26-Linux-x86_64.flatpak" "$id" stable
