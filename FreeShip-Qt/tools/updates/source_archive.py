"""Archive corresponding build sources and a compact first-install download."""
import argparse,hashlib,json,zipfile
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('--installer',type=Path,required=True)
    p.add_argument('--source-root',type=Path,help='Frozen corresponding FreeShip-Qt source tree')
    p.add_argument('--base-source-archive',type=Path,help='Verified matching published sources for a packaging-only release')
    a=p.parse_args()
    qt=a.source_root.resolve() if a.source_root else Path(__file__).resolve().parents[2];workspace=qt.parent
    info=json.loads((a.installer/'installer-manifest.json').read_text('utf8'))
    manifest=json.loads((Path(info['source_runtime'])/'build-manifest.json').read_text('utf8'))
    frozen=None
    if a.base_source_archive:
        with zipfile.ZipFile(a.base_source_archive) as z:
            frozen={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
    for name,digest in manifest['source_sha256'].items():
        content=frozen['FreeShip-Qt/'+name] if frozen is not None else (qt/name).read_bytes()
        if hashlib.sha256(content).hexdigest()!=digest:raise ValueError('Source changed since compilation: '+name)
    output=a.installer.parent/'downloads';output.mkdir(exist_ok=True)
    version=info['version'];source=output/('FreeShip-26-'+version+'-source.zip')
    roots=[qt/n for n in ('src','third_party','bridge','tools')]+[workspace/'FreeShip-Tests/qt']
    files=[]
    for root in roots:
        for path in root.rglob('*'):
            if not path.is_file() or '__pycache__' in path.parts:continue
            if path.suffix.lower() in ('.pem','.pfx','.p12','.key','.exe','.dll','.pyc','.o','.obj','.a'):continue
            files.append(path)
    files += [qt/n for n in ('CMakeLists.txt','README.md','NOTICE.md','COPYING','build.ps1','package.ps1','package-runtime.ps1','build-installer.ps1') if (qt/n).is_file()]
    with zipfile.ZipFile(source,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        if frozen is None:
            for path in sorted(files):z.write(path,path.relative_to(workspace).as_posix())
        else:
            overlay=[qt/'tools'/n for n in ('package_runtime.py','build_installer.py','calculation_tools_delivery.py','calculation-tools-catalog.json','updates/source_archive.py')]
            reviewed=json.loads((qt/'tools/calculation-tools-catalog.json').read_text('utf8'))
            for name,digest in reviewed['sha256'].items():
                path=qt/'third_party/hydronship'/name
                if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise ValueError('External tool changed: '+name)
                overlay.append(path)
            for path in overlay:
                relative=path.relative_to(qt).as_posix()
                if relative in manifest['source_sha256']:
                    # The old manifest also records Python packaging helpers;
                    # retain those exact build inputs alongside the new helpers.
                    if not relative.startswith('tools/') or path.suffix!='.py':
                        raise ValueError('Packaging overlay changes compiled source: '+relative)
                    frozen['build-inputs-original/FreeShip-Qt/'+relative]=frozen['FreeShip-Qt/'+relative]
                frozen['FreeShip-Qt/'+relative]=path.read_bytes()
            frozen['PACKAGING-ONLY-RELEASE.txt']=b'Calculation binaries are unchanged from the previous release. Qt has a verified Windows icon-resource-only patch; non-resource sections and overlay are unchanged. Exact original compiled source inputs were verified against the runtime manifest. Updated Python packaging tools and external tool notices are included; prior versions of recorded packaging helpers are retained under build-inputs-original. Proprietary external executables in third_party/hydronship are binary dependencies, not GPL source: read their NOTICE.md.\n'
            for name,content in sorted(frozen.items()):z.writestr(name,content)
    setup=Path(info['setup']);download=output/('FreeShip-26-'+version+'-Windows-x64.zip')
    if download.exists():
        with zipfile.ZipFile(download) as z:
            if hashlib.sha256(z.read('FreeShip 26 Setup.exe')).hexdigest()!=info['setup_sha256']:
                raise ValueError('Existing setup archive differs')
    else:
        with zipfile.ZipFile(download,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            z.write(setup,'FreeShip 26 Setup.exe')
            z.writestr('SOURCE.txt','Corresponding source: '+source.name+'\nLicense: GPL-2.0-or-later; see included project notices.\n')
    hashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (source,download)}
    (output/'SHA256SUMS.txt').write_text(''.join(d+'  '+name+'\n' for name,d in hashes.items()),'ascii')
    print(json.dumps(dict(downloads=str(output),sha256=hashes)))

if __name__=='__main__':main()
