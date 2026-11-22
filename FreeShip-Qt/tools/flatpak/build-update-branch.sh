#!/usr/bin/env bash
# Make a signed private Flatpak commit, without changing the original /test repo.
set -euo pipefail
work=$(realpath "${1:?build directory}"); mode=${2:?starter or latest}
cd "$work"
case "$mode" in starter|latest);; *) exit 2;; esac
if [[ $mode == latest ]]; then
    : "${FS26_TARGET_VERSION:?Windows working version}" "${FS26_FLATPAK_REVISION:?increasing test revision}"
    [[ $FS26_TARGET_VERSION =~ ^[0-9]+(\.[0-9]+){2,3}$ && $FS26_FLATPAK_REVISION =~ ^[1-9][0-9]*$ ]]
    (( FS26_FLATPAK_REVISION > 1 ))
fi
id=io.github.jiangshirong.FreeShip26
old=${CAPTION_REPO:?verified caption repository}
base=0a20a7fd1d29b3bd61e44c67c40a202963fcb51f2c019c6930877775b0b31c5e
repo=${FS26_TEST_REPO:?private repository}
qt="$work/source/FreeShip-Qt"
stage=$(mktemp -d "$work/$mode.stage.XXXXXXXX"); rmdir "$stage"
ostree --repo="$old" checkout --user-mode --force-copy "$base" "$stage"
p="$stage/files/lib/freeship26"
program(){ install -s "$1" "$p/$2"; patchelf --remove-rpath "$p/$2"; }
program "$work/build/FreeShipFlatpakUpdater" FreeShipFlatpakUpdater
install -m755 "$work/update-test-launcher.sh" "$stage/files/bin/freeship26"
if [[ $mode == starter ]]; then
    program "$work/starter-bin" FreeShipQt
    python3 - "$p/languages" "$qt/src/app/languages" <<'PY'
from pathlib import Path
import json,sys
old,new=map(Path,sys.argv[1:])
for f in old.glob('*.json'):
    data=json.loads(f.read_bytes()); curr=json.loads((new/f.name).read_bytes())
    for k in ['更新已就绪','正在更新…','重新启动']:data['messages'][k]=curr['messages'][k]
    f.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
PY
else
    for prog in FreeShipQt FreeShipDocumentService FreeShipNativeLinesplan FreeShipGeometryExport; do program "build/$prog" "$prog"; done
    # Reconstruct complete resource directories, rather than retaining deleted files from the base.
    for dir in bridge languages examples CalculationTools; do mv "$p/$dir" "$stage/previous-$dir"; done
    cp -r "$qt/runtime-data/bridge" "$p/bridge"
    cp -r "$qt/src/app/languages" "$p/languages"
    cp -r "$qt/examples" "$p/examples"
    cp "$qt/COPYING" "$qt/NOTICE.md" "$p/"
    PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$qt/tools" python3 - "$qt" "$p" <<'PY'
from pathlib import Path
import shutil,sys
from calculation_tools_delivery import approved_payload,validate_public_payload
src,dest=map(Path,sys.argv[1:]); files=approved_payload(src/'third_party/hydronship'); validate_public_payload(files)
for name,p in files.items():
    t=dest/name;t.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,t)
PY
fi
python3 - "$p/build-manifest.json" "$mode" "$work/source/sources.json" <<'PY'
import json,sys,hashlib,os
from pathlib import Path
path=Path(sys.argv[1]); starter=sys.argv[2]=='starter'; m=json.loads(path.read_bytes())
if not starter:m.pop('source_commit',None);m['source_snapshot_sha256']=hashlib.sha256(Path(sys.argv[3]).read_bytes()).hexdigest()
m.update(release_version='0.1.17' if starter else os.environ['FS26_TARGET_VERSION'],flatpak_updates=True,flatpak_revision=1 if starter else int(os.environ['FS26_FLATPAK_REVISION']),profile='flatpak-update-test')
path.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n')
PY
# Avoid shadowing the ordinary "FreeShip 26" menu and its FBM file association.
f="$stage/files/share/applications/$id.desktop"
sed -e 's/^Name=.*/Name=FreeShip 26 (Update Test)/' -e '/^MimeType=/d' "$f" > "$stage/files/share/applications/$id.UpdateTest.desktop"
unlink "$f"
assets="$stage/files/share/icons/hicolor/scalable/apps"
mv "$assets/$id.svg" "$assets/$id.UpdateTest.svg"
install -Dm644 "$qt/src/app/assets/brand/freeship26.png" "$stage/files/share/icons/hicolor/256x256/apps/$id.UpdateTest.png"
sed -i -e "s/^Icon=$id\$/Icon=$id.UpdateTest/" -e '/^Name\[zh_CN\]=/cName[zh_CN]=FS26 更新测试' "$stage/files/share/applications/$id.UpdateTest.desktop"
mv "$stage/files/share/mime/packages/$id.xml" "$work/$mode.sample-mime.xml"
# Metadata from the exported baseline already includes its finish args.
python3 - "$stage/metadata" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text();assert 'shared=ipc;' in s and 'network;' not in s
p.write_text(s.replace('shared=ipc;', 'shared=ipc;network;'))
PY
# build-export consumes the existing export/ tree; rebuild it to avoid
# exporting the original desktop entry or replacing its file associations.
mv "$stage/export" "$work/${stage##*/}.original-export"
flatpak build-finish --command=freeship26 "$stage"
test ! -e "$stage/export/share/applications/$id.desktop"
test -f "$stage/export/share/applications/$id.UpdateTest.desktop"
flatpak build-export --arch=x86_64 --gpg-sign="${FS26_TEST_KEY:?signing fingerprint}" --gpg-homedir="${GNUPGHOME:?test keyring}" "$repo" "$stage" update-test
flatpak build-update-repo --gpg-sign="$FS26_TEST_KEY" --gpg-homedir="$GNUPGHOME" "$repo"
ostree --repo="$repo" rev-parse app/$id/x86_64/update-test > "$work/$mode.commit"
