"""Install user-owned calculators separately; package only explicitly cleared files.

Local use is not a redistribution approval. The checked-in catalog pins both
executables and data, so a different download requires a separate review.
"""
import argparse
import json
import os
import shutil
import uuid
from pathlib import Path
import package_runtime as package

CATALOG = Path(__file__).with_name('calculation-tools-catalog.json')
LOCAL_FOLDER = 'UserCalculationTools/HydroNShip35'
VENDOR = Path(__file__).resolve().parents[1] / 'third_party/hydronship'


def catalog():
    return json.loads(CATALOG.read_text('utf8'))


def distributable(tool):
    if tool['redistribution'] == 'approved':
        return bool(tool.get('permission_evidence'))
    # Project release decision based on upstream's binary distribution practice.
    # This is deliberately distinct from an unrestricted copyright permission.
    return (tool['redistribution'] == 'upstream-bundled'
            and tool.get('project_distribution_decision') == '2026-11-17-user-request'
            and bool(tool.get('upstream_distribution_evidence'))
            and 'NOTICE.md' in tool['files'])


def reviewed_files(source, public=False):
    source = Path(source).resolve(strict=True)
    data = catalog()
    selected = {}
    for tool in data['tools']:
        if public and not distributable(tool):
            continue
        for name in tool['files']:
            path = package.name_path(source, name)
            for parent in [path, *path.parents]:
                package.plain(parent)
                if parent == source:
                    break
            if package.digest(path) != data['sha256'][name]:
                raise ValueError('Calculator differs from reviewed version: ' + name)
            selected[name] = path
    if not selected:
        raise ValueError('No calculators have confirmed redistribution permission')
    return selected


def install_local(source, application):
    """Copy complete user tools atomically; no compiler/test runtime is installed.

    This intentionally leaves the original numerical test baseline intact.
    The application-relative reference survives moving the entire installation.
    MSI updates leave these user-owned files alone.
    """
    application = Path(application).absolute()
    roles_file=Path(os.environ.get('LOCALAPPDATA',str(application)))/'FreeShip26/installation-roles.json'
    if roles_file.is_file():
        roles=json.loads(roles_file.read_text('utf8'))
        if roles.get('status')=='active' and application.resolve()==Path(roles['user_directory']).resolve():
            raise ValueError('Protected user update-test installation: only the normal server update flow may change it')
    for p in [application, *application.parents]:
        package.plain(p)
    if not (application / 'FreeShipQt.exe').is_file():
        raise ValueError('Not a FreeShip application directory')
    files = reviewed_files(source)
    target = application / LOCAL_FOLDER
    config = application / 'calculation-tools.json'
    expected = {'directory': LOCAL_FOLDER, 'scope': 'user-installed'}
    if config.exists():
        package.plain(config)
        if json.loads(config.read_text('utf8')) != expected:
            raise ValueError('An existing calculator configuration must not be overwritten')
    if target.exists():
        actual = package.inventory(target)
        if set(actual) - (set(files) | {'installation.json'}) or any(package.digest(target/n) != package.digest(files[n]) for n in actual if n != 'installation.json'):
            raise ValueError('An existing calculator installation must not be overwritten')
        for name,path in files.items():
            dest=target/name
            if dest.exists():continue
            dest.parent.mkdir(parents=True,exist_ok=True)
            with dest.open('xb') as f, path.open('rb') as source_file:
                shutil.copyfileobj(source_file,f)
            if package.digest(dest)!=catalog()['sha256'][name]:
                raise ValueError('Calculator changed during installation: '+name)
        receipt=dict(version=1,use='local-user-owned',redistribution_approved=False,
                     catalog_sha256=package.digest(CATALOG),files_sha256={n:package.digest(target/n) for n in files})
        (target/'installation.json').write_text(json.dumps(receipt,indent=2)+'\n','utf8')
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        package.plain(target.parent)
        stage = target.parent / ('.install-' + uuid.uuid4().hex)
        stage.mkdir()
        for name, path in files.items():
            dest = stage / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
            if package.digest(dest) != catalog()['sha256'][name]:
                raise ValueError('Calculator changed during installation: ' + name)
        receipt = dict(version=1, use='local-user-owned', redistribution_approved=False,
                       catalog_sha256=package.digest(CATALOG),
                       files_sha256={n:package.digest(stage/n) for n in files})
        (stage/'installation.json').write_text(json.dumps(receipt,indent=2)+'\n','utf8')
        stage.rename(target)
    if not config.exists():
        # Exclusive creation never replaces another installer/user's reference.
        with config.open('x', encoding='utf8') as f:
            json.dump(expected, f, indent=2)
            f.write('\n')
    return dict(directory=str(target), config=str(config), files=len(files),
                bytes=sum(p.stat().st_size for p in files.values()),
                test_directory_required=False, redistribution_approved=False)


def approved_payload(source):
    return {'CalculationTools/'+n:p for n,p in reviewed_files(source, public=True).items()}


def validate_public_payload(files):
    """Fail closed even when a caller bypasses package_runtime's selector."""
    data=catalog()
    allowed={n for t in data['tools'] if distributable(t) for n in t['files']}
    found={n.removeprefix('CalculationTools/'):p for n,p in files.items() if n.startswith('CalculationTools/')}
    for n,p in found.items():
        if n not in allowed or package.digest(p)!=data['sha256'].get(n):
            raise ValueError('Unapproved public calculator file: '+n)
    for tool in data['tools']:
        if 'Exec/'+tool['program'] in found and not set(tool['files']) <= set(found):
            raise ValueError('Incomplete public calculator dependencies: '+tool['program'])
    return {'CalculationTools/'+n for n in found}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--application',type=Path,required=True)
    a=p.parse_args()
    print(json.dumps(install_local(a.source,a.application),ensure_ascii=False))


if __name__=='__main__':
    main()
