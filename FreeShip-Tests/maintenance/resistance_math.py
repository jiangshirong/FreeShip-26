"""Retained diagnostic for original Win64 compiler arithmetic and assembly.

Used to distinguish expression promotion from formula changes. Full resistance
regression compares real curves; this probe is an opt-in compiler diagnostic.
"""
import argparse,subprocess,sys,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import freeship_ui_test as common
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 fpc=common.junction_for(common.DEFAULT_LAZARUS,out)/'fpc/3.2.2';probe=out/'resistance_math.pas';shutil.copy2(ROOT/'qt/resistance_math.pas',probe)
 args=[str(fpc/'bin/x86_64-win64/fpc.exe'),'-n','-O2','-al','-Fu'+str(fpc/'units/x86_64-win64/rtl'),'-Fu'+str(fpc/'units/x86_64-win64/rtl-objpas'),'-FE'+str(out),'-FU'+str(out),str(probe)]
 c=subprocess.run(args,cwd=out,capture_output=True,timeout=30,creationflags=common.FLAGS);(out/'build.log').write_bytes(c.stdout+c.stderr)
 if c.returncode:raise RuntimeError('Math probe compilation failed')
 c=subprocess.run([str(out/'resistance_math.exe')],capture_output=True,timeout=5,creationflags=common.FLAGS);(out/'values.txt').write_bytes(c.stdout);print(c.stdout.decode())
if __name__=='__main__':main()
