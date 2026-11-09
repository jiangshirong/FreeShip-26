"""Read-only storage inventory, with no junction traversal or deletion.

Explicit --root directories only. Retained diagnostic, not a product test.
"""
import argparse
import json
import os
import stat
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def inventory(root):
    root = root.absolute()
    if root.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise ValueError('Select the physical root, not a junction: ' + str(root))
    folders = defaultdict(lambda: dict(bytes=0, files=0))
    extensions = defaultdict(lambda: dict(bytes=0, files=0))
    links, errors, largest = [], [], []
    pending = [(str(root), ())]
    while pending:
        base, prefix = pending.pop()
        try:
            entries = list(os.scandir(base))
        except OSError as ex:
            errors.append(str(ex))
            continue
        for entry in entries:
            try:
                s = entry.stat(follow_symlinks=False)
                if s.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                    links.append(entry.path)
                    continue
                parts = prefix + (entry.name,)
                if entry.is_dir(follow_symlinks=False):
                    pending.append((entry.path, parts))
                    continue
                for n in range(min(3, len(parts))):
                    key = '/'.join(parts[:n]) or '.'
                    folders[key]['bytes'] += s.st_size
                    folders[key]['files'] += 1
                key = os.path.splitext(entry.name)[1].lower() or '<none>'
                extensions[key]['bytes'] += s.st_size
                extensions[key]['files'] += 1
                if s.st_size >= 20 * 1024**2:
                    largest.append(dict(path=entry.path, bytes=s.st_size))
            except OSError as ex:
                errors.append(str(ex))
    return dict(root=str(root), folders=dict(folders), extensions=dict(extensions), links=links,
                errors=errors, large_files=sorted(largest, key=lambda x: -x['bytes']))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, action='append', required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    results = []
    for root in a.root:
        result = inventory(root)
        results.append(result)
        a.output.write_text(json.dumps(dict(created_utc=datetime.now(timezone.utc).isoformat(), roots=results),
                                      ensure_ascii=False, indent=2), encoding='utf8')
        ordered = sorted(result['folders'].items(), key=lambda x: -x[1]['bytes'])
        print(json.dumps(dict(root=result['root'], GiB=round(result['folders'].get('.', {}).get('bytes', 0)/2**30, 3),
                              top=[(k, round(v['bytes']/2**30, 3), v['files']) for k, v in ordered[:20]],
                              links=len(result['links']), errors=result['errors']), ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
