"""Publish verified chunks first and atomically switch the signed feed last.

Credentials are read from the terminal (or stdin for a private pipe), never
arguments or files. Server-side hashes avoid downloading the release again.
"""
import argparse,base64,concurrent.futures,getpass,hashlib,json,posixpath,re,shlex,sys
from pathlib import Path
import paramiko
import xml.etree.ElementTree as ET
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa,padding

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--release',type=Path,required=True);p.add_argument('--host',required=True);p.add_argument('--port',type=int,default=22)
    p.add_argument('--user',default='root');p.add_argument('--fingerprint',required=True)
    p.add_argument('--channel',default='stable');p.add_argument('--password-stdin',action='store_true')
    p.add_argument('--downloads',type=Path)
    p.add_argument('--reuse-channel',help='Reuse immutable blocks already uploaded to stable or validation/name')
    a=p.parse_args()
    if not re.fullmatch(r'stable|validation/[a-z0-9-]+',a.channel):raise ValueError('Unsupported channel path')
    if a.reuse_channel and not re.fullmatch(r'stable|validation/[a-z0-9-]+',a.reuse_channel):raise ValueError('Unsupported reuse channel')
    raw=(a.release/'latest.json').read_bytes();signed=json.loads(raw);data=base64.b64decode(signed['payload'])
    config=json.loads((Path(__file__).parent/'channel.json').read_text('utf8'))
    xml=ET.fromstring(config['public_key_xml'])
    number=lambda name:int.from_bytes(base64.b64decode(xml.findtext(name)),'big')
    public=rsa.RSAPublicNumbers(number('Exponent'),number('Modulus')).public_key()
    public.verify(base64.b64decode(signed['signature']),data,padding.PKCS1v15(),hashes.SHA256())
    manifest=json.loads(data)
    if manifest['family']!=config['family']:raise ValueError('Release product family mismatch')
    expected={c['hash']+'.gz':c['packed_hash'] for c in manifest['chunks']}
    for name,digest in expected.items():
        if not re.fullmatch(r'[a-f0-9]{64}\.gz',name) or hashlib.sha256((a.release/'chunks'/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('Local release chunk mismatch')
    class Policy(paramiko.MissingHostKeyPolicy):
        def missing_host_key(self,client,hostname,key):
            if base64.b64encode(hashlib.sha256(key.asbytes()).digest()).decode()!=a.fingerprint.removeprefix('SHA256:'):
                raise ValueError('SSH server host key changed')
    client=paramiko.SSHClient();client.set_missing_host_key_policy(Policy())
    password=sys.stdin.readline().rstrip('\r\n') if a.password_stdin else getpass.getpass('SSH password: ')
    client.connect(a.host,port=a.port,username=a.user,password=password,timeout=15,look_for_keys=False,allow_agent=False)
    password=None;client.get_transport().set_keepalive(15)
    root='/var/www/freeship26';channel=root+'/'+a.channel;chunks=root+'/chunks' if a.channel=='stable' else channel+'/chunks'
    def command(text,data=None):
        stdin,out,err=client.exec_command(text,timeout=120)
        if data is not None:
            stdin.write(data);stdin.flush();stdin.channel.shutdown_write()
        value=out.read().decode();error=err.read().decode()
        if out.channel.recv_exit_status():raise RuntimeError(error or value)
        return value
    try:
        command('mkdir -p -- '+shlex.quote(channel)+' '+shlex.quote(chunks))
        if a.reuse_channel:
            reference=root+'/chunks' if a.reuse_channel=='stable' else root+'/'+a.reuse_channel+'/chunks'
            code="""import sys,json,pathlib,os,hashlib
s=json.load(sys.stdin);src=pathlib.Path(s['source']);dst=pathlib.Path(s['target']);count=0
for name,digest in s['hashes'].items():
    old=src/name;new=dst/name
    if new.exists() or not old.is_file():continue
    if hashlib.sha256(old.read_bytes()).hexdigest()!=digest:raise ValueError('Reference chunk hash mismatch')
    try:os.link(old,new);count+=1
    except FileExistsError:pass
print('Reused verified server blocks:',count)
"""
            print(command('python3 -c '+shlex.quote(code),json.dumps(dict(source=reference,target=chunks,hashes=expected))).strip())
        listing=client.open_sftp()
        try:existing={entry.filename:entry.st_size for entry in listing.listdir_attr(chunks)}
        finally:listing.close()
        missing=[]
        for name,digest in expected.items():
            if name in existing:
                if existing[name]!=(a.release/'chunks'/name).stat().st_size:raise ValueError('Conflicting remote chunk: '+name)
            else:missing.append((name,digest))
        def upload(item):
            name,digest=item;remote=chunks+'/'+name;sftp=client.open_sftp()
            try:
                # Matching names are immutable; hash all existing/new objects
                # below before activating the manifest. Do not trust size alone.
                try:
                    if sftp.stat(remote).st_size==(a.release/'chunks'/name).stat().st_size:return
                    raise ValueError('Conflicting remote chunk: '+name)
                except FileNotFoundError:pass
                temporary=remote+'.incoming';sftp.put(str(a.release/'chunks'/name),temporary);sftp.posix_rename(temporary,remote)
            finally:sftp.close()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(upload,missing))
        spec=json.dumps(dict(root=chunks,hashes=expected))
        code="import sys,json,pathlib,hashlib; s=json.load(sys.stdin); assert all(hashlib.sha256((pathlib.Path(s['root'])/n).read_bytes()).hexdigest()==h for n,h in s['hashes'].items()),'Remote chunk mismatch'; print('Remote hashes verified:',len(s['hashes']))"
        print(command('python3 -c '+shlex.quote(code),spec).strip())
        if a.downloads:
            version=manifest['version']
            if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise ValueError('Invalid release version')
            names=['FreeShip-26-'+version+'-source.zip','FreeShip-26-'+version+'-Windows-x64.zip','SHA256SUMS.txt']
            folder=root+'/releases/'+version;command('mkdir -p -- '+shlex.quote(folder))
            sums={name:hashlib.sha256((a.downloads/name).read_bytes()).hexdigest() for name in names}
            sftp=client.open_sftp()
            try:
                for name in names:
                    target=folder+'/'+name;local=a.downloads/name;temporary=target+'.incoming'
                    def remote_hash(path):
                        script="import sys,hashlib,pathlib; print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())"
                        return command('python3 -c '+shlex.quote(script)+' '+shlex.quote(path)).strip()
                    try:
                        existing_size=sftp.stat(target).st_size
                    except FileNotFoundError:existing_size=None
                    if existing_size is not None:
                        if existing_size!=local.stat().st_size or remote_hash(target)!=sums[name]:
                            raise ValueError('Published download differs; use a new version: '+name)
                        continue
                    try:offset=sftp.stat(temporary).st_size
                    except FileNotFoundError:offset=0
                    if offset>local.stat().st_size:raise ValueError('Partial upload exceeds source size')
                    if offset:
                        with local.open('rb') as source:prefix=hashlib.sha256(source.read(offset)).hexdigest()
                        if remote_hash(temporary)!=prefix:raise ValueError('Partial upload does not match local file')
                    with local.open('rb') as source,sftp.open(temporary,'ab' if offset else 'wb') as dest:
                        source.seek(offset);dest.set_pipelined(True)
                        while True:
                            block=source.read(1024*1024)
                            if not block:break
                            dest.write(block)
                    if remote_hash(temporary)!=sums[name]:raise ValueError('Completed upload hash mismatch')
                    sftp.posix_rename(temporary,target)
            finally:sftp.close()
            print(command('python3 -c '+shlex.quote(code),json.dumps(dict(root=folder,hashes=sums))).strip())
        sftp=client.open_sftp()
        try:
            with sftp.open(channel+'/latest.json.incoming','wb') as f:f.write(raw)
            sftp.posix_rename(channel+'/latest.json.incoming',channel+'/latest.json')
        finally:sftp.close()
        print(json.dumps(dict(channel=a.channel,version=manifest['version'],unique_chunks=len(expected),local_manifest_sha256=hashlib.sha256(raw).hexdigest())))
    finally:client.close()

if __name__=='__main__':main()
