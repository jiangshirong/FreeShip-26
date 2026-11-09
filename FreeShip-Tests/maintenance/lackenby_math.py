"""Generate/check the bounded original RTL Lackenby convergence constants.

Default mode compiles the retained Pascal probe in --output, executes it, and
requires exact binary64 equality with C++ hexadecimal literals. --write-source
is an explicit maintenance action, never used by regression tests.
"""
import argparse,json,re,shutil,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import freeship_ui_test as common


def audit(out,write_source=False):
    out.mkdir(parents=True,exist_ok=True)
    toolchain=common.junction_for(common.DEFAULT_LAZARUS,out)
    fpc=toolchain/'fpc/3.2.2';compiler=fpc/'bin/x86_64-win64/fpc.exe'
    probe=out/'lackenby_math_probe.pas';shutil.copy2(ROOT/'qt/lackenby_math_probe.pas',probe)
    command=[str(compiler),'-n','-O2','-Fu'+str(fpc/'units/x86_64-win64/rtl'),
             '-Fu'+str(fpc/'units/x86_64-win64/rtl-objpas'),'-FE'+str(out),'-FU'+str(out),str(probe)]
    process=subprocess.run(command,cwd=out,capture_output=True,timeout=30,creationflags=common.FLAGS)
    (out/'build.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError('Lackenby math probe build failed')
    process=subprocess.run([str(out/'lackenby_math_probe.exe')],cwd=out,capture_output=True,timeout=10,creationflags=common.FLAGS)
    (out/'convergence.txt').write_bytes(process.stdout)
    if process.returncode:raise RuntimeError('Lackenby math probe execution failed')
    rows=[line.split() for line in process.stdout.decode('ascii').splitlines()]
    if len(rows)!=100 or [int(row[0]) for row in rows]!=list(range(1,101)):raise RuntimeError('Invalid convergence sequence')
    expected=[row[1].upper() for row in rows]
    literals=[struct.unpack('>d',bytes.fromhex(bits))[0].hex() for bits in expected]
    path=ROOT.parent/'FreeShip-Qt/src/document/lackenby_edit.cpp';text=path.read_text('utf8')
    pattern=r'// BEGIN FPC LACKENBY FACTORS\n.*?// END FPC LACKENBY FACTORS'
    block=re.search(pattern,text,re.S)
    if not block:raise RuntimeError('C++ convergence table markers changed')
    if write_source:
        body='\n'.join('    '+', '.join(literals[i:i+4])+',' for i in range(0,100,4))
        replacement='// BEGIN FPC LACKENBY FACTORS\nstatic constexpr std::array<double,100> ConvergenceFactors{\n'+body+'\n};\n// END FPC LACKENBY FACTORS'
        path.write_text(text[:block.start()]+replacement+text[block.end():],'utf8')
        block=re.search(pattern,path.read_text('utf8'),re.S)
    actual=[struct.pack('>d',float.fromhex(value)).hex().upper() for value in re.findall(r'0x[0-9a-f]+\.[0-9a-f]+p[+-][0-9]+',block.group(),re.I)]
    result={'ok':actual==expected,'values':len(expected),'exact_binary64':actual==expected,
            'source':str(path),'pascal_probe':str(probe),'write_source':write_source,
            'compiler_sha256':common.digest_file(compiler)}
    (out/'report.json').write_text(json.dumps(result,indent=2),'utf8')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--write-source',action='store_true');a=p.parse_args()
    result=audit(a.output.resolve(),a.write_source);print(json.dumps(result));raise SystemExit(0 if result['ok'] else 1)
