"""Compile a console-free updater; its trust key is embedded, never fetched remotely."""
import argparse, json, os, subprocess
from pathlib import Path

def build(output, config, test=False):
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    # No test endpoint/key/path overrides are accepted by a production executable.
    values=dict(Endpoint=config['endpoint'],PublicKey=config['public_key_xml'],Family=config['family'])
    source='static class Channel {\n'+''.join('public const string '+k+'='+json.dumps(v)+';\n' for k,v in values.items())
    source+='public const bool Test='+str(test).lower()+';\n}'
    generated=output.with_suffix('.channel.cs');generated.write_text(source,'utf8')
    csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    cmd=[str(csc),'/nologo','/target:winexe','/platform:x64','/optimize+',
         '/r:System.Web.Extensions.dll','/out:'+str(output),
         str(Path(__file__).parent/'updates/Updater.cs'),str(generated)]
    r=subprocess.run(cmd,capture_output=True,creationflags=0x08000000,timeout=90)
    output.with_suffix('.compile.log').write_bytes(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stdout.decode(errors='replace')+r.stderr.decode(errors='replace'))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--config',type=Path,default=Path(__file__).parent/'updates/channel.json')
    a=p.parse_args();build(a.output,json.loads(a.config.read_text('utf8')))
