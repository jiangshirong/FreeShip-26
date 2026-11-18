"""Create the local release signing identity outside the source/output trees.

The private key never goes onto the download server. Public configuration only
is committed and compiled into the update client. Keep an offline key backup.
"""
import argparse,json,os,subprocess,sys
from pathlib import Path
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from update_release import FAMILY,public_xml

def main():
    p=argparse.ArgumentParser();p.add_argument('--endpoint',required=True)
    p.add_argument('--backup-dir',type=Path)
    a=p.parse_args()
    if not a.endpoint.startswith('https://'):raise ValueError('HTTPS required')
    root=Path(os.environ['LOCALAPPDATA'])/'FreeShip26ReleaseIdentity';root.mkdir(exist_ok=True)
    if root.is_symlink() or root.stat().st_file_attributes & 0x400:raise ValueError('Identity directory must not be a reparse point')
    account=os.environ['USERDOMAIN']+'\\'+os.environ['USERNAME']
    subprocess.run(['icacls',str(root),'/inheritance:r','/grant:r',account+':(OI)(CI)F','*S-1-5-18:(OI)(CI)F'],check=True,capture_output=True,creationflags=0x08000000)
    path=root/'release-signing.pem'
    if path.exists():key=serialization.load_pem_private_key(path.read_bytes(),password=None)
    else:
        key=rsa.generate_private_key(public_exponent=65537,key_size=3072)
        with path.open('xb') as f:f.write(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
    if a.backup_dir:
        backup=a.backup_dir.resolve()
        workspace=Path(__file__).resolve().parents[3]
        if backup==workspace or workspace in backup.parents or any('freeship26tests' in part.lower() for part in backup.parts):
            raise ValueError('Keep key backups outside source and test storage')
        backup.mkdir(parents=True,exist_ok=True)
        if backup.is_symlink() or backup.stat().st_file_attributes & 0x400:raise ValueError('Backup must not be a reparse point')
        subprocess.run(['icacls',str(backup),'/inheritance:r','/grant:r',account+':(OI)(CI)F','*S-1-5-18:(OI)(CI)F'],check=True,capture_output=True,creationflags=0x08000000)
        target=backup/'release-signing.pem'
        if not target.exists():
            with target.open('xb') as f:f.write(path.read_bytes())
        recovered=serialization.load_pem_private_key(target.read_bytes(),password=None)
        if public_xml(recovered)!=public_xml(key):raise ValueError('Backup belongs to another signing identity; not overwritten')
        (backup/'README.txt').write_text('FreeShip 26 release signing key backup. PRIVATE: never publish or include in source/test archives.\nPrimary: '+str(path)+'\nKeep a separate offline backup; two drives on one PC do not protect against loss of the PC.\n','utf8')
    config=dict(endpoint=a.endpoint,public_key_xml=public_xml(key),family=FAMILY)
    (Path(__file__).parent/'channel.json').write_text(json.dumps(config,indent=2)+'\n','utf8')
    print('Public update channel configured; private signing key remains in the protected local identity directory.')

if __name__=='__main__':main()
