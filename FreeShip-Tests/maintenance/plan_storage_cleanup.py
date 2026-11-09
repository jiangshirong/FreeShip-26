"""Plan removal of generated caches and superseded compiled binaries only.

Never deletes. Leaves reports, screenshots, source, fixtures, model/scene/export
evidence and manifests intact. Reparse points are not traversed. Execute the
reviewable plan with apply_storage_cleanup.ps1; historical runtimes whose EXEs
are retired remain documented but cannot be rerun without rebuilding.
"""
import argparse
import hashlib
import json
import os
import stat
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

TESTS = Path(__file__).resolve().parents[1]
WORKSPACE = TESTS.parent
ROOTS = [Path('C:/FreeShip26Tests/reports'), Path('D:/FreeShip26Tests/reports'), TESTS/'reports',
         Path('C:/FreeShip26Tests/build'), Path('D:/FreeShip26Tests/build'), WORKSPACE/'FreeShip-Qt/build',
         Path('C:/FreeShip26-HydrodynamicsTests'), Path('C:/FreeShip26-ShadingTests'), Path('C:/FreeShip26-ViewportTests')]
KEEP = [WORKSPACE/'FreeShip-Qt/build/accepted-m1-20261101',
        WORKSPACE/'FreeShip-Qt/build/dist-before-hydrodynamics-20261104',
        WORKSPACE/'FreeShip-Qt/build/dist-before-native-linesplan-20261101',
        Path('D:/FreeShip26Tests/build/plates-native-qt'),
        Path('D:/FreeShip26Tests/build/publication-native-bridge-01'),
        Path('C:/FreeShip26-HydrodynamicsTests/package-final'),
        Path('C:/FreeShip26-ShadingTests/package-final'),
        TESTS/'reports/qt-startup-delivery-20261101']
CACHE = 'native-lines-cache'
COMPILED = {'.exe', '.dll', '.o', '.obj', '.a', '.ppu', '.pdb', '.ilk', '.lib'}


def below(path, root):
    return path == root or root in path.parents


def candidates(root, keep):
    pending = [(root, False)]
    build_root = root.name == 'build'
    while pending:
        folder, cache = pending.pop()
        with os.scandir(folder) as iterator:
            for entry in iterator:
                info = entry.stat(follow_symlinks=False)
                if info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                    continue
                path = Path(entry.path)
                if any(below(path, p) for p in keep):
                    continue
                if entry.is_dir(follow_symlinks=False):
                    pending.append((path, cache or entry.name == CACHE))
                    continue
                reason = 'rebuildable-native-lines-cache' if cache else None
                # Only dated/reviewed build roots, not reports or user models.
                if build_root and path.suffix.lower() in COMPILED:
                    reason = 'superseded-compiled-build'
                if reason:
                    yield dict(path=str(path), bytes=info.st_size, mtime_ns=info.st_mtime_ns, reason=reason)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    files = []
    for root in ROOTS:
        root = root.absolute()
        if not root.exists():
            continue
        if root.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
            raise ValueError('Root is a link: '+str(root))
        found = list(candidates(root, [p.absolute() for p in KEEP]))
        files.extend(found)
        print(json.dumps(dict(root=str(root), files=len(found), GiB=round(sum(f['bytes'] for f in found)/2**30,3))), flush=True)
    totals = defaultdict(int)
    for entry in files:
        totals[Path(entry['path']).drive + ':' + entry['reason']] += entry['bytes']
    record = dict(format='FreeShipStorageCleanup', version=1, created_utc=datetime.now(timezone.utc).isoformat(),
                  roots=[str(r.absolute()) for r in ROOTS], protected=[str(p.absolute()) for p in KEEP],
                  files=files, totals=dict(totals), bytes=sum(f['bytes'] for f in files),
                  planner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    a.output.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(plan=str(a.output), GiB=round(record['bytes']/2**30,3), totals={k:round(v/2**30,3) for k,v in totals.items()})), flush=True)


if __name__ == '__main__':
    main()
