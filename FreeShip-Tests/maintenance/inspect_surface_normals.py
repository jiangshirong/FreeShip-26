"""Read-only diagnostic for a retained qt_native_surface paired case.

Usage: python maintenance/inspect_surface_normals.py CASE_DIRECTORY
Shows exact differences and incident fan triangles; does not change baselines.
Not a pass/fail test: coincident coordinates need not identify the same vertex.
"""
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);a,b=[json.loads((p/x/'model.json').read_text('utf8')) for x in ('native','oracle')]
key=lambda n:(n['face'],n['p'],n['n'])
d=[(x,y) for x,y in zip(sorted(a['surface_normals'],key=key),sorted(b['surface_normals'],key=key)) if x!=y]
d.sort(key=lambda v:max(abs(x-y) for x,y in zip(v[0]['n'],v[1]['n'])),reverse=True)
print(json.dumps(dict(count=len(d),examples=[dict(native=x,oracle=y,incident=[t for t in a['triangles'] if x['p'] in t['p']]) for x,y in d[:3]]),indent=2))
