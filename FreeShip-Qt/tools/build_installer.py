"""Build a per-user x64 setup EXE with verified automatic MSI recovery.

Uses a separately installed WiX 3.14 toolchain. Does not install, modify the
source runtime, register file associations, or copy a machine's solver settings.
The internal MSI is not a standalone delivery entry. Setup keeps the previous
verified package and recovers product registration after an MSI rollback error.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from datetime import datetime,timezone
from pathlib import Path
import package_runtime as package

WIX='http://schemas.microsoft.com/wix/2006/wi'
ET.register_namespace('',WIX)
UPGRADE_CODE=uuid.UUID('ad4aa28d-89c6-5dad-8ead-46843a810838')
DEFAULT_VERSION='0.1.2'
FLAGS=getattr(subprocess,'CREATE_NO_WINDOW',0)

def element(parent,tag,text=None,**attrs):
    node=ET.SubElement(parent,'{'+WIX+'}'+tag,{k:str(v) for k,v in attrs.items()})
    node.text=text;return node

def identity(version,test_id=None):
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)',version):raise ValueError('Use a three-part MSI version')
    if any(n>limit for n,limit in zip(map(int,version.split('.')),(255,255,65535))):raise ValueError('MSI version is out of range')
    if test_id is not None and not re.fullmatch(r'[a-f0-9]{12}',test_id):raise ValueError('Test ID must be twelve lowercase hex digits')
    upgrade=uuid.uuid5(UPGRADE_CODE,'test:'+test_id) if test_id else UPGRADE_CODE
    return dict(upgrade_code=str(upgrade).upper(),product_code=str(uuid.uuid5(upgrade,'version:'+version)).upper(),
        version=version,name='FreeShip 26'+(' Test '+test_id if test_id else ''),
        folder='FreeShip26Qt'+('-Test-'+test_id if test_id else ''),
        registry='Software\\FreeShip26\\QtInstaller'+('Test\\'+test_id if test_id else ''),test_id=test_id)

def identifier(prefix,name):return prefix+hashlib.sha256(name.casefold().encode()).hexdigest()[:32]

def license_rtf(source):
    text=source.read_text('utf8').replace('\\','\\\\').replace('{','\\{').replace('}','\\}')
    text=''.join(c if ord(c)<128 else '\\u'+str(ord(c) if ord(c)<32768 else ord(c)-65536)+'?' for c in text)
    return '{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Consolas;}}\\f0\\fs16 '+text.replace('\n','\\par\n')+'}'

def author(payload,output,version,test_id=None):
    ids=identity(version,test_id);manifest,files=package.verify(payload)
    from calculation_tools_delivery import validate_public_payload
    validate_public_payload(files)
    ns=uuid.UUID(ids['upgrade_code'])
    root=ET.Element('{'+WIX+'}Wix')
    product=element(root,'Product',Id=ids['product_code'],UpgradeCode=ids['upgrade_code'],Name=ids['name'],
        Version=version,Manufacturer='FreeShip 26',Language='2052',Codepage='936')
    element(product,'Package',InstallerVersion='500',Compressed='yes',InstallScope='perUser',InstallPrivileges='limited',
        Description=ids['name']+' local development build',SummaryCodepage='936')
    element(product,'MajorUpgrade',Schedule='afterInstallInitialize',DowngradeErrorMessage='A newer FreeShip 26 version is already installed.')
    # Keep cabinet data stable for content-defined update reuse. Compression is
    # performed per network chunk, after delta selection, not across all files.
    # Large files have their own cabinet, so inserting text in one executable
    # cannot shift CAB checksum/block boundaries in every Qt DLL after it.
    groups={name:('large:'+name if path.stat().st_size>=262144 else 'small:'+hashlib.sha256(name.encode()).hexdigest()[0]) for name,path in files.items()}
    media={key:index+1 for index,key in enumerate(sorted(set(groups.values())))}
    for key,disk in media.items():
        element(product,'Media',Id=disk,Cabinet='u'+hashlib.sha256(key.encode()).hexdigest()[:16]+'.cab',EmbedCab='yes',CompressionLevel='none')
    element(product,'Property',Id='ARPCOMMENTS',Value='Qt development build. User models and preferences are retained on uninstall.')
    element(product,'Property',Id='ARPNOREPAIR',Value='1') # repair remains available through msiexec /fa
    element(product,'Property',Id='ARPSYSTEMCOMPONENT',Value='1')
    element(product,'Property',Id='FREESHIP_SETUP',Secure='yes')
    element(product,'Condition','Installed OR FREESHIP_SETUP = "1"',Message='Please run FreeShip 26 Setup.exe to install or update this application.')
    element(product,'Property',Id='MSIRESTARTMANAGERCONTROL',Value='Disable')
    element(product,'Property',Id='REBOOT',Value='ReallySuppress')
    element(product,'Condition','Installed OR VersionNT64',Message='64-bit Windows is required.')
    # Keep the installation location when upgrading; never import old Lazarus
    # paths or an unrelated product's registry keys.
    saved=element(product,'Property',Id='INSTALLDIR',Secure='yes')
    element(saved,'RegistrySearch',Id='PreviousQtLocation',Root='HKCU',Key=ids['registry'],Name='InstallDir',Type='raw',Win64='yes')
    target=element(product,'Directory',Id='TARGETDIR',Name='SourceDir')
    local=element(target,'Directory',Id='LocalAppDataFolder')
    programs=element(local,'Directory',Id='LocalPrograms',Name='Programs')
    install=element(programs,'Directory',Id='INSTALLDIR',Name=ids['folder'])
    menus=element(target,'Directory',Id='ProgramMenuFolder')
    menu=element(menus,'Directory',Id='QtMenu',Name=ids['name'])
    directories={'':install};directory_ids={'':'INSTALLDIR'}
    for name in sorted(files):
        parent=Path(name).parent.as_posix()
        if parent=='.':continue
        current=''
        for part in parent.split('/'):
            child=(current+'/' if current else '')+part
            if child not in directories:
                directory_ids[child]=identifier('D',child)
                directories[child]=element(directories[current],'Directory',Id=directory_ids[child],Name=part)
            current=child
    feature=element(product,'Feature',Id='Runtime',Title=ids['name'],Level='1',Absent='disallow')
    for name,path in sorted(files.items()):
        parent=Path(name).parent.as_posix();parent='' if parent=='.' else parent
        cid=identifier('C',name)
        component=element(directories[parent],'Component',Id=cid,Guid=str(uuid.uuid5(ns,'file:'+name.casefold())).upper(),Win64='yes')
        element(component,'File',Id=identifier('F',name),Name=path.name,Source=str(path),Vital='yes',DiskId=media[groups[name]])
        element(component,'RegistryValue',Root='HKCU',Key=ids['registry']+'\\Files',Name=cid,Type='integer',Value='1',KeyPath='yes')
        element(feature,'ComponentRef',Id=cid)
    component=element(install,'Component',Id='InstallMetadata',Guid=str(uuid.uuid5(ns,'metadata')).upper(),Win64='yes')
    element(component,'RegistryValue',Root='HKCU',Key=ids['registry'],Name='InstallDir',Type='string',Value='[INSTALLDIR]',KeyPath='yes')
    # RemoveFolder only removes empty directories. No wildcard file removals:
    # user-created models inside INSTALLDIR survive upgrade and uninstall.
    for name,did in sorted(directory_ids.items(),reverse=True):
        element(component,'RemoveFolder',Id=identifier('R',name),Directory=did,On='uninstall')
    element(component,'RemoveFolder',Id='RemoveEmptyLocalPrograms',Directory='LocalPrograms',On='uninstall')
    element(feature,'ComponentRef',Id='InstallMetadata')
    shortcut=element(menu,'Component',Id='StartMenu',Guid=str(uuid.uuid5(ns,'shortcut')).upper(),Win64='yes')
    element(shortcut,'Shortcut',Id='LaunchQt',Name=ids['name'],Target='[INSTALLDIR]FreeShipQt.exe',WorkingDirectory='INSTALLDIR',Advertise='no')
    element(shortcut,'RegistryValue',Root='HKCU',Key=ids['registry'],Name='Shortcut',Type='integer',Value='1',KeyPath='yes')
    element(shortcut,'RemoveFolder',Id='RemoveQtMenu',On='uninstall')
    element(feature,'ComponentRef',Id='StartMenu')
    element(product,'UIRef',Id='WixUI_Minimal')
    element(product,'WixVariable',Id='WixUILicenseRtf',Value=str(output/'license.rtf'))
    (output/'license.rtf').write_text(license_rtf(payload/'COPYING'),'ascii')
    tree=ET.ElementTree(root);ET.indent(tree);tree.write(output/'installer.wxs',encoding='utf-8',xml_declaration=True)
    return ids,manifest,files

def compile_msi(output,wix):
    output=Path(output);wix=Path(wix)
    for name in ('candle.exe','light.exe','WixUIExtension.dll'):
        if not (wix/name).is_file():raise ValueError('Missing WiX tool: '+str(wix/name))
    commands=[
        [str(wix/'candle.exe'),'-nologo','-arch','x64','-out',str(output/'installer.wixobj'),str(output/'installer.wxs')],
        [str(wix/'light.exe'),'-nologo','-ext',str(wix/'WixUIExtension.dll'),'-cultures:zh-cn','-sice:ICE91',
         '-out',str(output/'FreeShip_26-Qt-x64.msi'),str(output/'installer.wixobj')]]
    for name,command in zip(('compile','link'),commands):
        result=subprocess.run(command,cwd=output,capture_output=True,creationflags=FLAGS,timeout=180)
        (output/(name+'.log')).write_bytes(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError('Installer '+name+' failed; see '+str(output/(name+'.log')))
    return output/'FreeShip_26-Qt-x64.msi'

def compile_setup(output,info,fault=''):
    """Compile an asInvoker WinForms shell using the Windows .NET Framework.

    The maintenance EXE is small and MSI-free. Only the download EXE embeds
    the MSI; the cache retains at most the current and previous verified MSI.
    """
    if fault and (not info.get('test_id') or fault!='after-msi'):raise ValueError('Fault injection requires a private test identity')
    output=Path(output);source=Path(__file__).parent/'installer/Setup.cs'
    language_source=source.with_name('InstallerLanguage.cs')
    view_source=source.with_name('InstallerView.cs')
    brand=source.parents[2]/'src/app/assets/brand'
    brand_icon=brand/'freeship26.ico'
    brand_image=brand/'freeship26.png'
    # Only complete, context-specific installer catalogs are offered. English
    # is embedded in maintenance as well, so recovery needs no installed files.
    literals=[]
    for text in (path.read_text('utf8') for path in (source,language_source,view_source)):
        literals.extend(json.loads(m[1]) for m in re.finditer(r'\b(?:T|Text)\(\s*("(?:\\.|[^"\\])*")\s*\)',text))
    required=set(literals)
    catalogs=[dict(id='zh',name='简体中文',messages={})]
    seen_catalog_ids={'zh'}
    for path in sorted((source.parents[2]/'src/app/languages').glob('*.json')):
        catalog=json.loads(path.read_text('utf8'));messages=catalog['messages']
        # Chinese is the source language already embedded above. Its complete
        # catalog must not add a second dictionary entry during initialization.
        if catalog['id']=='zh':continue
        if catalog['id'] in seen_catalog_ids:raise ValueError('Duplicate installer language: '+catalog['id'])
        seen_catalog_ids.add(catalog['id'])
        selected={key:messages.get('installer\x04'+key,'') for key in required}
        missing=sorted(key for key,value in selected.items() if not isinstance(value,str) or not value.strip())
        if missing:raise ValueError(f"Incomplete installer captions for {catalog['id']}: {', '.join(missing)}")
        catalogs.append(dict(id=catalog['id'],name=catalog['name'],messages=selected))
    missing_languages={'zh','en','ru','ja','de','fr','es'}-{c['id'] for c in catalogs}
    if missing_languages:raise ValueError('Missing installer languages: '+', '.join(sorted(missing_languages)))
    language_resource=output/'installer-languages.json'
    language_resource.write_text(json.dumps(dict(version=1,languages=catalogs),ensure_ascii=False),'utf8')
    csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    if not csc.is_file():raise ValueError('The Windows .NET Framework C# compiler is required')
    ids='static class Identity { public const string Family='+json.dumps(info['upgrade_code'])+'; public const string TestId='+json.dumps(info.get('test_id') or '')+'; public const string Name='+json.dumps(info['name'])+'; public const string Fault='+json.dumps(fault)+'; }'
    (output/'Identity.cs').write_text(ids,'utf8')
    (output/'setup.manifest').write_text('<?xml version="1.0" encoding="utf-8"?><assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges><requestedExecutionLevel level="asInvoker" uiAccess="false"/></requestedPrivileges></security></trustInfo><application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings><dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware></windowsSettings></application></assembly>','utf8')
    bundle={k:info[k] for k in ('product_code','upgrade_code','version','name','folder','registry','test_id','msi_sha256','payload_sha256')}
    common=[str(csc),'/nologo','/target:winexe','/platform:x64','/optimize+',
        '/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Web.Extensions.dll',
        '/resource:'+str(language_resource)+',installer-languages.json',
        '/resource:'+str(brand_image)+',installer-brand.png',
        '/resource:'+str(brand_icon)+',installer-brand.ico',
        '/win32icon:'+str(brand_icon), '/win32manifest:'+str(output/'setup.manifest')]
    sources=[str(source),str(language_source),str(view_source),str(output/'Identity.cs')]
    engine=output/'maintenance.exe';setup=output/'FreeShip 26 Setup.exe'
    for name,command in [('maintenance',common+['/out:'+str(engine)]+sources),('setup',common+['/out:'+str(setup),
        '/resource:'+str(output/'setup-bundle.json')+',manifest.json','/resource:'+info['msi']+',payload.msi','/resource:'+str(engine)+',maintenance.exe']+sources)]:
        if name=='setup':
            bundle['maintenance_sha256']=package.digest(engine)
            (output/'setup-bundle.json').write_text(json.dumps(bundle,ensure_ascii=False),'utf8')
        result=subprocess.run(command,capture_output=True,creationflags=FLAGS,timeout=90)
        (output/(name+'-compile.log')).write_bytes(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError('Setup compilation failed: '+str(output/(name+'-compile.log')))
    info.update(setup=str(setup),setup_sha256=package.digest(setup),
        setup_source_sha256=package.digest(source),installer_view_source_sha256=package.digest(view_source), installer_brand_sha256={p.name:package.digest(p) for p in (brand_icon,brand_image)},installer_language_source_sha256=package.digest(language_source),installer_languages_sha256=package.digest(language_resource),installer_languages=[c['id'] for c in catalogs],maintenance_sha256=package.digest(engine),
        installer_profile='verified-msi-recovery',delivery_status='local-development',
        boundary='Setup EXE automatically restores verified prior MSI after failure. Raw MSI is internal. Unsigned; desktop and public release acceptance remain separate.')
    return info

def build(runtime,output,wix,version=DEFAULT_VERSION,test_id=None):
    identity(version,test_id) # validate before creating anything
    runtime=Path(runtime).absolute();output=Path(output).absolute();wix=Path(wix).absolute()
    if output.exists():raise ValueError('Use a new installer output directory')
    manifest,files=package.verify(runtime)
    if manifest.get('runtime_profile') not in ('native-local','native-portable'):raise ValueError('Use a native runtime, not a development package')
    executables={n for n in files if n.lower().endswith('.exe')}
    from calculation_tools_delivery import validate_public_payload
    approved=validate_public_payload(files)
    if not package.PROGRAMS<=executables<=package.PROGRAMS|package.OPTIONAL_PROGRAMS|approved:raise ValueError('Unexpected executable in native runtime')
    # The portable payload excludes calculation-tools.json even when the source
    # is a local runtime. Preferences already use QStandardPaths outside it.
    package.package(runtime,output/'payload',False)
    version_manifest=output/'payload/build-manifest.json'
    # Copy public build provenance only. A development role would disable
    # downloads in the installed product; local paths and QA receipts are not public metadata.
    packaged=json.loads(version_manifest.read_text('utf8'))
    stamped={key:packaged[key] for key in ('configuration','created_utc','source_sha256',
        'test_source_sha256','binaries_sha256','runtime_profile','packaged_utc',
        'package_source_manifest_sha256','runtime_packager_sha256') if key in packaged}
    stamped['release_version']=version
    stamped['settings_profile']='installed'
    version_manifest.write_text(json.dumps(stamped,ensure_ascii=False,indent=2)+'\n','utf8')
    ids,portable,files=author(output/'payload',output,version,test_id)
    msi=compile_msi(output,wix)
    info=dict(format='FreeShipQtInstaller',format_version=1,created_utc=datetime.now(timezone.utc).isoformat(),
        **ids,msi=str(msi),msi_sha256=package.digest(msi),source_runtime=str(runtime),
        source_manifest_sha256=package.digest(runtime/'build-manifest.json'),
        payload_sha256={n:package.digest(p) for n,p in files.items()},
        author_sha256=package.digest(Path(__file__)),package_author_sha256=package.digest(Path(package.__file__)),
        wix_sha256={n:package.digest(wix/n) for n in ('candle.exe','light.exe','WixUIExtension.dll')},
        delivery_status='internal-msi')
    compile_setup(output,info)
    (output/'installer-manifest.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n','utf8')
    return info

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--wix',type=Path,default=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/wix314')
    p.add_argument('--version',default=DEFAULT_VERSION);p.add_argument('--test-id')
    a=p.parse_args();info=build(a.runtime,a.output,a.wix,a.version,a.test_id)
    print(json.dumps(dict(setup=info['setup'],sha256=info['setup_sha256'],product_code=info['product_code']),ensure_ascii=False))

if __name__=='__main__':main()
