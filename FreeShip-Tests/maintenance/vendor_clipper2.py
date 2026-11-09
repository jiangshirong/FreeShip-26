"""Fetch the pinned Clipper2 engine and headers, retaining license and hashes.

One-off dependency maintenance, not a regression test. Does not update the pin
or overwrite differing local files. Runtime builds do not need network access.
"""
import hashlib
import json
import urllib.request
from pathlib import Path

PIN='21ebba05db8894f0c7217ad35ea518080f324946'
ROOT=Path(__file__).resolve().parents[2]/'FreeShip-Qt/third_party/clipper2'
REPO='https://github.com/AngusJohnson/Clipper2'


def get(url):
    request=urllib.request.Request(url,headers={'User-Agent':'FreeShip26-dependency-audit'})
    with urllib.request.urlopen(request,timeout=30) as response:return response.read()


def main():
    tree=json.loads(get(f'https://api.github.com/repos/AngusJohnson/Clipper2/git/trees/{PIN}?recursive=1'))
    assert not tree.get('truncated')
    paths=[entry['path'] for entry in tree['tree'] if entry['type']=='blob' and
           (entry['path'].startswith('CPP/Clipper2Lib/include/clipper2/') or entry['path'] in ('CPP/Clipper2Lib/src/clipper.engine.cpp','LICENSE'))]
    hashes={}
    for source in paths:
        relative=source.removeprefix('CPP/Clipper2Lib/')
        target=ROOT/relative;data=get(f'https://raw.githubusercontent.com/AngusJohnson/Clipper2/{PIN}/{source}')
        if target.exists() and target.read_bytes()!=data:raise RuntimeError('Local dependency differs: '+str(target))
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        hashes[relative]=hashlib.sha256(data).hexdigest()
    (ROOT/'origin.json').write_text(json.dumps(dict(repository=REPO,tag='Clipper2_2.0.1',commit=PIN,
        license='BSL-1.0',scope='Unmodified engine and required upstream headers; lateral projection union only',
        files_sha256=hashes),indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(root=str(ROOT),commit=PIN,files=len(hashes))))


if __name__=='__main__':main()
