"""Assemble a native runtime from a verified development package, without rebuilding.

No compiler, solver, user preference, test binary or Pascal executable is copied.
The optional local solver setting contains only a reference to separately installed
tools. Existing destinations are never overwritten. Source/license archives stay.
"""
import argparse,hashlib,json,os,re,shutil,stat,time,uuid,sys
from datetime import datetime,timezone
from pathlib import Path,PurePosixPath
if str(Path(__file__).parent) not in sys.path:
    sys.path.insert(0,str(Path(__file__).parent))

PROGRAMS={'FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe','FreeShipGeometryExport.exe'}
OPTIONAL_PROGRAMS={'FreeShipUpdater.exe'}
PLUGINS={'platforms','imageformats','iconengines','networkinformation','styles','tls'}
RESOURCES={'Themes','Languages','locale'}
TEXT_FILES={'COPYING','NOTICE.md','README.md'}
PROVENANCE={'legacy-source.zip','source-id.txt','report.json'}
MINIMUM_FREE=3*1024**3

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()

def plain(path):
    info=path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info,'st_file_attributes',0)&stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise ValueError('Links are not allowed in the package: '+str(path))

def name_path(root,name):
    p=PurePosixPath(name)
    if not name or '\\' in name or ':' in name or p.is_absolute() or any(s in ('','.','..') for s in name.split('/')):
        raise ValueError('Invalid package-relative path: '+name)
    path=root.joinpath(*p.parts)
    if root not in path.resolve().parents:raise ValueError('Package path escapes its root: '+name)
    return path

def inventory(root):
    plain(root);found={}
    for parent,dirs,files in os.walk(root,followlinks=False):
        for name in dirs+files:plain(Path(parent)/name)
        for name in files:
            p=Path(parent)/name;found[p.relative_to(root).as_posix()]=p
    return found

def verify(source):
    source=source.absolute();files=inventory(source)
    manifest=json.loads((source/'build-manifest.json').read_text('utf-8-sig'))
    recorded=manifest['binaries_sha256'];folded=set()
    for name,value in recorded.items():
        path=name_path(source,name)
        if name.casefold() in folded:raise ValueError('Case-colliding package path: '+name)
        folded.add(name.casefold())
        if not path.is_file() or digest(path)!=value:raise ValueError('Source package hash mismatch: '+name)
    if set(files)-{'build-manifest.json'}!=set(recorded):raise ValueError('Source package inventory differs from its manifest')
    if not PROGRAMS<=set(files):raise ValueError('A required native program is missing')
    for required in ('COPYING','NOTICE.md','README.md','bridge/legacy-source.zip','bridge/source-id.txt',
                     'bridge/report.json','bridge/Themes/StudioLight/icons/manifest.json',
                     'bridge/locale/FreeShip.cn.po','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','platforms/qwindows.dll'):
        if required not in files:raise ValueError('Required runtime resource is missing: '+required)
    return manifest,files

def select(name,local_solvers):
    parts=PurePosixPath(name).parts
    # Resource folders and provenance must not smuggle extra runnable programs.
    if name.lower().endswith(('.exe','.bat','.cmd','.ps1')) and name not in PROGRAMS|OPTIONAL_PROGRAMS:return False
    if len(parts)==1:return name in PROGRAMS|OPTIONAL_PROGRAMS|TEXT_FILES or name.endswith('.dll') or (local_solvers and name=='calculation-tools.json')
    if parts[0] in PLUGINS:return name.endswith('.dll')
    if parts[0]=='languages':
        return len(parts)==2 and re.fullmatch(r'[a-z]{2}\.json',parts[1]) is not None
    if parts[0] in ('docs','third_party','licenses'):return True
    if parts[0]=='bridge':return parts[1] in RESOURCES or len(parts)==2 and parts[1] in PROVENANCE
    return False

def package(source,output,local_solvers=False,calculation_tools=None):
    source=Path(os.path.abspath(source));output=Path(os.path.abspath(output))
    if output.exists():raise ValueError('Use a new destination; existing files are not overwritten')
    if output==source or source in output.parents or output in source.parents:raise ValueError('Source and destination must be disjoint')
    # Verify every existing ancestor before creating a directory. This also
    # rejects an output redirected through a Windows junction.
    for parent in [output.parent,*output.parent.parents]:
        if parent.exists():plain(parent)
    manifest,files=verify(source)
    from calculation_tools_delivery import validate_public_payload, approved_payload, VENDOR
    approved=validate_public_payload(files)
    if calculation_tools is None and not approved:
        calculation_tools=VENDOR
    selected={n:p for n,p in files.items() if select(n,local_solvers) or n in approved}
    extra=approved_payload(calculation_tools) if calculation_tools else {}
    extra_hashes={n:digest(p) for n,p in extra.items()}
    if set(extra)&set(selected):raise ValueError('Calculator payload already exists in source runtime')
    selected.update(extra)
    if local_solvers and 'calculation-tools.json' in selected:
        cfg=json.loads(selected['calculation-tools.json'].read_text('utf8'))
        if set(cfg)-{'directory','scope'} or not isinstance(cfg.get('directory'),str) or not Path(cfg['directory']).is_absolute() or not Path(cfg['directory']).is_dir():
            raise ValueError('Invalid local calculation-tools reference')
    size=sum(p.stat().st_size for p in selected.values())
    nearest=next(p for p in [output.parent,*output.parent.parents] if p.exists())
    if shutil.disk_usage(nearest).free<size+MINIMUM_FREE:raise ValueError('Insufficient capacity: keep at least 3 GiB free after packaging')
    output.parent.mkdir(parents=True,exist_ok=True)
    stage=output.parent/('.native-package-'+uuid.uuid4().hex);stage.mkdir()
    parent_hash=digest(source/'build-manifest.json')
    for name,path in selected.items():
        target=name_path(stage,name);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
        expected=extra_hashes[name] if name in extra else manifest['binaries_sha256'][name]
        if digest(target)!=expected:raise ValueError('Source changed while packaging: '+name)
    if digest(source/'build-manifest.json')!=parent_hash:raise ValueError('Source manifest changed while packaging')
    validate_public_payload(inventory(stage))
    if approved or extra:
        from calculation_tools_delivery import catalog
        reviewed=catalog()
        notice=stage/'licenses/calculation-tools-catalog.json';notice.parent.mkdir(parents=True,exist_ok=True)
        notice.write_text(json.dumps(dict(version=1,tools=[t for t in reviewed['tools'] if 'CalculationTools/Exec/'+t['program'] in selected],
            sha256={n.removeprefix('CalculationTools/'):digest(p) for n,p in selected.items() if n.startswith('CalculationTools/')}),ensure_ascii=False,indent=2)+'\n','utf8')
    profile=dict(version=1,profile='native-local' if local_solvers else 'native-portable',
                 legacy_executable=False,diagnostic_executables=False,resource_layout='bridge',
                 source_manifest_sha256=parent_hash,
                 notes=['The bridge directory now holds inherited resources and source provenance only.',
                        'Only explicitly cleared external calculator files may be redistributed; user-installed tools are excluded.',
                        'Local delivery preparation; installer, public distribution licenses and desktop acceptance remain separate.'])
    (stage/'runtime-profile.json').write_text(json.dumps(profile,ensure_ascii=False,indent=2)+'\n','utf8')
    result=dict(manifest)
    result.update(runtime_profile=profile['profile'],packaged_utc=datetime.now(timezone.utc).isoformat(),
                  package_source=str(source),package_source_manifest_sha256=parent_hash,
                  runtime_packager_sha256=digest(Path(__file__)),
                  package_scope='Native local runtime; public release and desktop acceptance are not certified.',
                  excluded_files=sorted(set(files)-set(selected)-{'build-manifest.json'}),
                  binaries_sha256={n:digest(p) for n,p in inventory(stage).items()})
    (stage/'build-manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n','utf8')
    # Only this newly created sibling may be renamed. Never merge into an
    # existing runtime, and never perform recursive deletion on failure.
    if output.exists() or stage.resolve().parent!=output.parent.resolve() or output.resolve().parent!=output.parent.resolve():
        raise ValueError('Unexpected staging/destination path')
    # Virus scanning or a concurrent report-size walk may briefly hold a Windows
    # directory handle without delete sharing. Retry only that bounded case;
    # a destination created meanwhile must never be replaced.
    for attempt in range(12):
        if output.exists():raise ValueError('Destination appeared during packaging; it was not overwritten')
        plain(stage);plain(output.parent)
        try:stage.rename(output);break
        except PermissionError as error:
            if getattr(error,'winerror',None) not in (5,32,33) or attempt==11:raise
            time.sleep(.25)
    return dict(output=str(output),files=len(result['binaries_sha256']),bytes=size,
                excluded_files=len(result['excluded_files']),source_bytes=sum(p.stat().st_size for p in files.values()))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--local-solvers',action='store_true',help='Keep only the existing local path reference; never copy solver binaries')
    p.add_argument('--calculation-tools',type=Path,help='Add only hash-pinned calculators with confirmed redistribution permission')
    a=p.parse_args();print(json.dumps(package(a.source,a.output,a.local_solvers,a.calculation_tools),ensure_ascii=False))

if __name__=='__main__':main()
