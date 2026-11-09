"""Read-only diagnosis of retained native/oracle snapshot pairs.

Usage: python maintenance/inspect_publication_pairs.py REPORT_ROOT --output FILE
This explains mismatches; it never changes expectations or marks tests passed.
"""
import argparse,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from qt_publication import differences,load_json
from qt_modeling import geometry


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=[];count=0
    for native in sorted(a.root.rglob('native/model.json')):
        oracle=native.parent.parent/'oracle/model.json'
        if not oracle.is_file():continue
        count+=1;diff=differences(geometry(load_json(native)),geometry(load_json(oracle)))
        if diff:result.append(dict(native=str(native),oracle=str(oracle),differences=diff))
    data=dict(pairs=count,mismatches=len(result),results=result)
    a.output.write_text(json.dumps(data,ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(dict(pairs=count,mismatches=len(result),examples=result[:4]),ensure_ascii=False))


if __name__=='__main__':main()
