"""Retain evidence, retire only transient caches from an owned completed run.

The unified runner creates the ownership token in its new output directory and
calls finish after child processes exit. Standalone suites remain unchanged.
Unknown files and links are retained. Published scenes/models/reports stay put.
"""
import json
import os
import re
import shutil
import stat
import uuid
from datetime import datetime, timezone
from pathlib import Path

MARKER = '.freeship-test-run.json'
CACHE_FILES = {'entry.json', 'scene.fslp', 'reference.dxf', 'sections.json', 'surface.json'}


def choose_report_root(config, free_space=None):
    """Pick available capacity only for an unspecified output location."""
    def available(root):
        parent = root
        while not parent.exists() and parent != parent.parent:
            parent = parent.parent
        return shutil.disk_usage(parent).free
    free_space = free_space or available
    roots = config.get('report_roots') or [config['report_root']]
    candidates = []
    for name in roots:
        root = Path(name).absolute()
        try:
            candidates.append((free_space(root), root))
        except OSError:
            continue
    if not candidates:
        raise ValueError('No configured test output drive is available')
    capacity, root = max(candidates, key=lambda pair: pair[0])
    if capacity < config.get('minimum_free_gib', 1)*2**30:
        raise ValueError('测试盘余量不足，请先清理测试输出或显式选择其他 --output 目录。')
    return root, dict(selected=str(root), free_bytes=capacity,
                      candidates=[dict(root=str(p), free_bytes=n) for n,p in candidates])


def is_link(info):
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def claim(root):
    root = Path(root).absolute()
    if is_link(root.lstat()) or not root.is_dir():
        raise ValueError('Output must be a plain new directory')
    if any(root.iterdir()):
        raise ValueError('Claim only an empty new output directory')
    root = root.resolve(strict=True)
    token = uuid.uuid4().hex
    with (root/MARKER).open('x', encoding='utf8') as f:
        json.dump(dict(format='FreeShipTestRun', token=token, root=str(root)), f)
    return token


def cache_files(folder):
    """Reject a whole cache with unknown content rather than guess its purpose."""
    files, dirs = [], [folder]
    for key in folder.iterdir():
        if is_link(key.lstat()) or not key.is_dir() or (key.name != 'sections' and not re.fullmatch('[0-9a-f]{64}', key.name)):
            return None
        dirs.append(key)
        allowed = {'cuts.fscache', 'local-geometry.fscache'} if key.name == 'sections' else CACHE_FILES
        for f in key.iterdir():
            info = f.lstat()
            if is_link(info) or not f.is_file() or f.name not in allowed:
                return None
            files.append((f, info.st_size, info.st_mtime_ns))
    return files, dirs


def finish(root, token, *, keep=False):
    root = Path(root).absolute()
    if is_link(root.lstat()):
        raise ValueError('Output root became a link')
    root = root.resolve(strict=True)
    marker = root/MARKER
    if is_link(marker.lstat()):
        raise ValueError('Output marker is a link')
    owner = json.loads(marker.read_text('utf8'))
    if owner != dict(format='FreeShipTestRun', token=token, root=str(root)):
        raise ValueError('Output ownership mismatch')
    report = root/'report.json'
    if is_link(report.lstat()) or not report.is_file():
        raise ValueError('Complete run report must exist first')
    record = dict(created_utc=datetime.now(timezone.utc).isoformat(), status='kept' if keep else 'complete',
                  removed_files=0, logical_bytes=0, caches=[], skipped=[],
                  retained='Source, reports, logs, screenshots, published models/scenes/exports and external baselines. Cache-hit checks finished before retirement.')
    pending, selected = [root], []
    if not keep:
        while pending:
            folder = pending.pop()
            with os.scandir(folder) as entries:
                for entry in entries:
                    if is_link(entry.stat(follow_symlinks=False)):
                        record['skipped'].append(dict(path=entry.path, reason='link'))
                        continue
                    if not entry.is_dir(follow_symlinks=False):
                        continue
                    path = Path(entry.path)
                    if entry.name == 'native-lines-cache':
                        result = cache_files(path)
                        if result is None:
                            record['skipped'].append(dict(path=str(path), reason='unknown-cache-content'))
                        else:
                            selected.append((path, result))
                    else:
                        pending.append(path)
        # Fully validate absolute targets before any deletion. Only the runner's
        # newly created output is eligible; no directory recursion in deletion.
        for folder, (files, dirs) in selected:
            for path in dirs + [f[0] for f in files]:
                if root not in path.resolve(strict=True).parents or is_link(path.lstat()):
                    raise ValueError('Cache escaped private output')
            for path, size, mtime in files:
                info = path.stat()
                if info.st_size != size or info.st_mtime_ns != mtime:
                    raise ValueError('Cache changed while finalizing')
        for folder, (files, dirs) in selected:
            for path, size, _ in files:
                path.unlink()
                record['removed_files'] += 1
                record['logical_bytes'] += size
            for path in reversed(dirs):
                path.rmdir()  # Empty directories only; never recursive deletion.
            record['caches'].append(str(folder.relative_to(root)))
    (root/'artifact-retention.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf8')
    return record
