"""Read-only exact comparison of a construction snapshot and frozen baseline.

Diagnosis only: prints the first differences and counts without changing any
expectation. Run from FreeShip-Tests with --actual/--expected model.json paths
and --output pointing at a new report file.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from qt_native_construction import attributed_model
from qt_publication import differences
from qt_migration import common

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('actual','expected','output'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args()
    left=attributed_model(json.loads(a.actual.read_text('utf8')))
    right=attributed_model(json.loads(a.expected.read_text('utf8')))
    result={'actual':str(a.actual),'actual_sha256':common.digest_file(a.actual),
            'expected':str(a.expected),'expected_sha256':common.digest_file(a.expected),'fields':{}}
    for key in left.keys()|right.keys():
        if left.get(key)==right.get(key):continue
        av,bv=left.get(key),right.get(key)
        changes=differences(av,bv)
        result['fields'][key]={'actual_size':len(av) if isinstance(av,(list,dict)) else None,
                              'expected_size':len(bv) if isinstance(bv,(list,dict)) else None,
                              'differences':len(changes),'first':changes[:8]}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    if a.output.exists():raise RuntimeError('Output already exists')
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps(result['fields'],ensure_ascii=False))

if __name__=='__main__':main()
