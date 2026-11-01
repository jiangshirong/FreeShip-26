"""Rebuild embedded PNG resources from the reviewed icon handoff; no SVG rasterizer needed."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent
source = root / 'Themes' / 'StudioLight' / 'icons'
parts = []
for folder in sorted(p for p in source.iterdir() if p.is_dir() and p.name.isdigit()):
    for path in sorted(folder.glob('*.png')):
        nums = ['#' + str(value) for value in path.read_bytes()]
        chunks = [''.join(nums[i:i+32]) for i in range(0,len(nums),32)]
        parts.append("LazarusResources.Add('FreeShip_26/" + path.stem + "/" + folder.name + "','PNG',[\n  " + '\n  +'.join(chunks) + ']);\n')
(root/'Units'/'FreeShip_26Icons.lrs').write_text(''.join(parts),encoding='utf8')
print('Embedded',len(parts),'PNG resources')
