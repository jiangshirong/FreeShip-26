#!/usr/bin/env python3
"""Assemble the FreeShip 26 Ubuntu test .deb from an existing Linux build.

Runs on Ubuntu 24.04 x86_64. It compiles nothing: it copies the four runtime
programs from a CMake build directory, bundles the Qt 6.8 libraries and plugins
they need, adds the same inherited resources and documents as the Windows
runtime package, and writes a fingerprinted build manifest. External Windows
solvers are never included. The destination must be a new directory.

Example:
  python3 tools/package_deb.py --build /srv/build --source FreeShip-Qt \
      --resources runtime-resources --qt /opt/Qt/6.8.3/gcc_64 \
      --version 0.1.2~ubuntu24.04.test1 --output /srv/out/deb-test1
"""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

PROGRAMS = ('FreeShipQt', 'FreeShipDocumentService', 'FreeShipNativeLinesplan', 'FreeShipGeometryExport', 'FreeShipUpdater')
LANGUAGES = ('de', 'en', 'es', 'fr', 'ja', 'ru', 'zh')  # Same catalogs as the Windows runtime.
RESOURCE_ITEMS = ('bridge', 'docs', 'licenses', 'third_party', 'COPYING', 'NOTICE.md', 'README.md')
BRIDGE_REQUIRED = ('Themes', 'Languages', 'locale', 'legacy-source.zip', 'source-id.txt')
PLUGINS = {
    'platforms': ('libqxcb.so', 'libqwayland-egl.so', 'libqwayland-generic.so', 'libqoffscreen.so', 'libqminimal.so'),
    'xcbglintegrations': ('libqxcb-glx-integration.so', 'libqxcb-egl-integration.so'),
    'wayland-shell-integration': None, 'wayland-decoration-client': None,
    'wayland-graphics-integration-client': None,
    'platforminputcontexts': ('libcomposeplatforminputcontextplugin.so', 'libibusplatforminputcontextplugin.so'),
    'platformthemes': ('libqgtk3.so', 'libqxdgdesktopportal.so'),
    'imageformats': ('libqgif.so', 'libqico.so', 'libqjpeg.so'),
    'tls': ('libqopensslbackend.so', 'libqcertonlybackend.so'),  # HTTPS for FreeShipUpdater
    'printsupport': ('libcupsprintersupport.so',),
}
PREFIX = Path('opt/freeship26')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def run(*cmd, **kw):
    return subprocess.run([str(c) for c in cmd], check=True, capture_output=True, text=True, **kw).stdout


def elf(path):
    with open(path, 'rb') as f:
        return f.read(4) == b'\x7fELF'


def needed(path):
    out = run('patchelf', '--print-needed', path)
    return [line.strip() for line in out.splitlines() if line.strip()]


def bundle_libraries(roots, qt_lib, dest_lib):
    """Copy every library from the Qt prefix reachable from roots (transitively)."""
    available = {p.name: p for p in qt_lib.iterdir() if p.is_file() or p.is_symlink()}
    queue, seen = list(roots), set()
    while queue:
        item = queue.pop()
        for name in needed(item):
            if name in seen or name not in available:
                continue
            seen.add(name)
            source = available[name].resolve()
            target = dest_lib / name
            shutil.copy2(source, target)
            queue.append(target)
    return sorted(seen)


def write(path, text, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    path.chmod(mode)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--build', type=Path, required=True, help='Existing Linux CMake build directory')
    p.add_argument('--source', type=Path, required=True, help='FreeShip-Qt source root used by that build')
    p.add_argument('--resources', type=Path, required=True,
                   help='Folder with bridge/, docs/, licenses/, third_party/, COPYING, NOTICE.md, README.md')
    p.add_argument('--qt', type=Path, required=True, help='Qt 6.8 gcc_64 prefix used by the build')
    p.add_argument('--version', required=True,
                   help='Debian version: the release version, optionally followed by ~suffix (e.g. 0.1.12~ubuntu24.04.test5)')
    p.add_argument('--output', type=Path, required=True, help='New output directory')
    p.add_argument('--source-manifest', type=Path, help='Snapshot manifest recording the source baseline')
    p.add_argument('--patch', type=Path, action='append', default=[], help='Source patch applied to that snapshot')
    p.add_argument('--calculation-tools', choices=('bundled', 'none'), default='bundled',
                   help='bundled: the original calculators cleared by tools/calculation-tools-catalog.json '
                        '(same rule as the Windows release) under CalculationTools/; none: leave them out')
    a = p.parse_args()

    if not re.fullmatch(r'[0-9][A-Za-z0-9.+~]*', a.version):
        p.error('Invalid Debian version')
    # The updater compares this three-part release version with the signed feed.
    release_version = re.match(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?=$|~)', a.version)
    if not release_version:
        p.error('The Debian version must start with the three-part release version, e.g. 0.1.12~ubuntu24.04.test5')
    release_version = release_version.group(0)
    out = a.output.resolve()
    if out.exists():
        p.error('Output must be a new directory')
    build, source, resources, qt = (x.resolve(strict=True) for x in (a.build, a.source, a.resources, a.qt))
    for name in PROGRAMS:
        if not elf(build / name):
            p.error(f'Missing Linux program: {build / name}')
    for name in RESOURCE_ITEMS:
        if not (resources / name).exists():
            p.error(f'Missing runtime resource: {name}')
    for name in BRIDGE_REQUIRED:
        if not (resources / 'bridge' / name).exists():
            p.error(f'Missing bridge resource: {name}')
    if shutil.disk_usage(out.parent if out.parent.exists() else Path('/')).free < 3 * 2**30:
        p.error('Insufficient capacity: keep at least 3 GiB free')

    package = 'freeship26'
    stage = out / f'{package}_{a.version}_amd64'
    opt = stage / PREFIX
    (opt / 'lib').mkdir(parents=True)

    # Programs: stripped copies that load the bundled Qt from ./lib.
    for name in PROGRAMS:
        target = opt / name
        shutil.copy2(build / name, target)
        run('strip', '--strip-unneeded', target)
        run('patchelf', '--set-rpath', '$ORIGIN/lib', target)
        target.chmod(0o755)

    plugin_files = []
    for folder, names in PLUGINS.items():
        src = qt / 'plugins' / folder
        if not src.is_dir():
            continue
        chosen = sorted(src.glob('*.so')) if names is None else [src / n for n in names]
        for plugin in chosen:
            if not plugin.is_file():
                p.error(f'Missing Qt plugin: {plugin}')
            target = opt / 'plugins' / folder / plugin.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(plugin, target)
            run('patchelf', '--set-rpath', '$ORIGIN/../../lib', target)
            plugin_files.append(target)
    libraries = bundle_libraries([opt / n for n in PROGRAMS] + plugin_files, qt / 'lib', opt / 'lib')
    for lib in (opt / 'lib').iterdir():
        run('patchelf', '--set-rpath', '$ORIGIN', lib)
        lib.chmod(0o644)
    write(opt / 'qt.conf', '[Paths]\nPrefix=.\nLibraries=lib\nPlugins=plugins\n')

    # Inherited resources and documents, identical to the Windows runtime package.
    for name in RESOURCE_ITEMS:
        src = resources / name
        if src.is_dir():
            shutil.copytree(src, opt / name)
        else:
            shutil.copy2(src, opt / name)
    tools = {}
    if a.calculation_tools == 'bundled':
        # Same catalog, hashes and project distribution decision as the Windows package.
        sys.path.insert(0, str(source / 'tools'))
        import calculation_tools_delivery as delivery
        tools = delivery.reviewed_files(source / 'third_party/hydronship', public=True)
        for name, path in tools.items():
            target = opt / 'CalculationTools' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    (opt / 'languages').mkdir()
    for lang in LANGUAGES:
        shutil.copy2(source / 'src/app/languages' / f'{lang}.json', opt / 'languages' / f'{lang}.json')
    qt_version = re.search(r'#define QT_VERSION_STR "([^"]+)"', (qt / 'include/QtCore/qconfig.h').read_text()).group(1)
    lgpl = Path('/usr/share/common-licenses/LGPL-3')
    (opt / 'licenses/Qt').mkdir(parents=True)
    if lgpl.is_file():
        shutil.copy2(lgpl, opt / 'licenses/Qt/LGPL-3.0.txt')
    write(opt / 'licenses/Qt/README.md',
          f'Qt {qt_version} libraries and plugins in lib/ and plugins/ are the unmodified '
          'official Qt binaries (LGPL-3.0, see LGPL-3.0.txt). Source: https://download.qt.io/archive/qt/\n'
          'The bundled ICU 73 libraries (libicu*) are distributed under the Unicode License, '
          'https://www.unicode.org/license.txt\n')
    write(opt / 'runtime-profile.json', json.dumps({
        'version': 1, 'profile': 'native-linux-deb', 'legacy_executable': False,
        'diagnostic_executables': False, 'resource_layout': 'bridge',
        'calculation_tools': 'bundled-upstream-binaries' if tools else 'not-included',
        'notes': [('Original HydroNShip calculators bundled under CalculationTools per the catalog; '
                   'see CalculationTools/NOTICE.md for their separate terms.') if tools else
                  'External numerical solvers are separate and are not redistributed.',
                  'Legacy Windows solvers run through a separately installed Wine.',
                  'Local Ubuntu test package; public distribution licenses remain separate.']}, indent=2) + '\n')

    # Desktop integration.
    write(stage / 'usr/share/applications/freeship26.desktop', '\n'.join([
        '[Desktop Entry]', 'Type=Application', 'Name=FreeShip 26', 'GenericName=Ship hull design',
        'GenericName[zh_CN]=船体设计', 'GenericName[ru]=Проектирование корпуса судна',
        'GenericName[ja]=船体設計', 'GenericName[de]=Schiffsrumpfentwurf',
        'GenericName[fr]=Conception de coques', 'GenericName[es]=Diseño de cascos',
        'Comment=Hull modelling, lines plans and naval architecture calculations',
        'Comment[zh_CN]=船体建模、型线图绘制和船舶设计计算',
        'Comment[ru]=Моделирование корпуса, теоретические чертежи и судостроительные расчёты',
        'Comment[ja]=船体モデリング、線図作成、船舶設計計算',
        'Comment[de]=Rumpfmodellierung, Linienrisse und schiffbauliche Berechnungen',
        'Comment[fr]=Modélisation de coques, plans de formes et calculs d’architecture navale',
        'Comment[es]=Modelado de cascos, planos de formas y cálculos de arquitectura naval',
        'Exec=/opt/freeship26/FreeShipQt %f', 'Icon=freeship26', 'Terminal=false',
        'Categories=Graphics;Engineering;Science;', 'MimeType=application/x-freeship-fbm;',
        'StartupWMClass=FreeShipQt', '']))
    icon = stage / 'usr/share/icons/hicolor/256x256/apps/freeship26.png'
    icon.parent.mkdir(parents=True)
    shutil.copy2(source / 'src/app/assets/brand/freeship26.png', icon)
    write(stage / 'usr/share/mime/packages/freeship26.xml', '\n'.join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">',
        '  <mime-type type="application/x-freeship-fbm">', '    <comment>FreeShip model</comment>',
        '    <comment xml:lang="zh_CN">FreeShip 船模</comment>',
        '    <comment xml:lang="ru">Модель судна FreeShip</comment>',
        '    <comment xml:lang="ja">FreeShip 船体モデル</comment>',
        '    <comment xml:lang="de">FreeShip-Schiffsmodell</comment>',
        '    <comment xml:lang="fr">Modèle de navire FreeShip</comment>',
        '    <comment xml:lang="es">Modelo de buque de FreeShip</comment>',
        '    <glob pattern="*.fbm"/>',
        '    <glob pattern="*.FBM"/>', '  </mime-type>', '</mime-info>', '']))
    (stage / 'usr/bin').mkdir(parents=True)
    os.symlink('/opt/freeship26/FreeShipQt', stage / 'usr/bin/freeship26')
    doc = stage / 'usr/share/doc/freeship26'
    doc.mkdir(parents=True)
    write(doc / 'copyright', 'FreeShip 26 is GPL-2.0-or-later; see /opt/freeship26/COPYING, NOTICE.md,\n'
          'licenses/ and third_party/. Qt and ICU notices are in /opt/freeship26/licenses/Qt.\n' +
          ('The unmodified HydroNShip calculation programs in /opt/freeship26/CalculationTools keep\n'
           "their authors' copyrights and are not GPL; read CalculationTools/NOTICE.md. ADD_MASS is\n"
           'limited to education and noncommercial use. No unrestricted license is granted.\n'
           if tools else 'External calculation programs are not included.\n'))

    # Debian ownership/permission conventions; resources copied from a Windows
    # runtime otherwise keep their archive modes (e.g. 0666).
    for path in [stage, *stage.rglob('*')]:
        if path.is_symlink():
            continue
        if path.is_dir():
            path.chmod(0o755)
        elif path.parent == opt and path.name in PROGRAMS:
            path.chmod(0o755)
        else:
            path.chmod(0o644)

    # Manifest of every installed payload file.
    files = {}
    for path in sorted(stage.rglob('*')):
        if path.is_file() and not path.is_symlink():
            files[str(path.relative_to(stage))] = sha256(path)
    manifest = {
        'package': package, 'version': a.version, 'release_version': release_version, 'architecture': 'amd64',
        'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'build_dir': str(build), 'source_root': str(source), 'qt_prefix': str(qt), 'qt_version': qt_version,
        'compiler': run('c++', '--version').splitlines()[0],
        'packager_sha256': sha256(Path(__file__).resolve()),
        'source_manifest_sha256': sha256(a.source_manifest) if a.source_manifest else None,
        'patches_sha256': {str(x): sha256(x) for x in a.patch},
        'bundled_qt_libraries': libraries,
        'calculation_tools': sorted(tools),
        'programs_sha256_unstripped': {n: sha256(build / n) for n in PROGRAMS},
        'files_sha256': files,
    }
    write(opt / 'build-manifest.json', json.dumps(manifest, ensure_ascii=False, indent=1) + '\n')

    # Dependencies of the programs and bundled Qt, resolved against the system.
    work = out / 'shlibdeps'
    (work / 'debian').mkdir(parents=True)
    write(work / 'debian/control', f'Source: {package}\n\nPackage: {package}\nArchitecture: amd64\n')
    elves = [x for x in opt.rglob('*') if x.is_file() and not x.is_symlink() and elf(x)]
    deps = run('dpkg-shlibdeps', '-O', '--ignore-missing-info', f'-l{opt / "lib"}', *elves, cwd=work)
    depends = deps.split('shlibs:Depends=', 1)[1].strip()
    size = sum(f.stat().st_size for f in stage.rglob('*') if f.is_file() and not f.is_symlink()) // 1024
    write(stage / 'DEBIAN/control', '\n'.join([
        f'Package: {package}', f'Version: {a.version}', 'Architecture: amd64',
        'Maintainer: FreeShip 26 local test build <freeship26@localhost>', f'Installed-Size: {size}',
        f'Depends: {depends}',
        # Chinese UI and labels; Ubuntu Desktop installs it by default, minimal systems do not.
        'Recommends: fonts-noto-cjk, pkexec',  # pkexec: administrator prompt for online updates
        # Separately supplied legacy Windows calculation programs run through Wine.
        'Suggests: wine, wine32:i386', 'Section: graphics', 'Priority: optional',
        'Description: FreeShip 26 hull design (Ubuntu test build)',
        ' Subdivision-surface hull modelling, four-view editing, lines plan,',
        ' hydrostatics, resistance and propulsion, plate development and CAD export.',
        ' Separately supplied legacy Windows calculation programs run through Wine.', '']))
    md5 = []
    for path in sorted(stage.rglob('*')):
        if path.is_file() and not path.is_symlink() and 'DEBIAN' not in path.parts:
            md5.append(f'{hashlib.md5(path.read_bytes()).hexdigest()}  {path.relative_to(stage)}')
    write(stage / 'DEBIAN/md5sums', '\n'.join(md5) + '\n')
    deb = out / f'{package}_{a.version}_amd64.deb'
    run('dpkg-deb', '--root-owner-group', '-Zxz', '--build', stage, deb)
    summary = {'deb': str(deb), 'sha256': sha256(deb), 'bytes': deb.stat().st_size, 'depends': depends,
               'installed_kib': size, 'file_count': len(files), 'qt_libraries': libraries}
    write(out / 'package-summary.json', json.dumps(summary, ensure_ascii=False, indent=1) + '\n')
    print(json.dumps(summary, ensure_ascii=False, indent=1))


if __name__ == '__main__':
    sys.exit(main())
