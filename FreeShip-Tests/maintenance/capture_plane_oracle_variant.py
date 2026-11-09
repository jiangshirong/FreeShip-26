"""Retain independently replayed legacy output as review evidence, never a native expectation.

Use --replay <plane_oracle_replay/report.json>. Requires four independent exact
replays, unchanged original source and prior fixtures. Native variant 0 is
immutable. Run only after any current document regression has finished.
"""
import argparse
import json
import shutil
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q
from qt_native_construction import attributed_model

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))
def digest(path):return q.common.digest_file(Path(path))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--replay',type=Path,required=True);a=p.parse_args()
    replay=read(a.replay);matches=[v for v in replay['attempts'] if v['matches_target']]
    if len(matches)<4:raise RuntimeError('Four independent matching legacy processes are required')
    target=Path(replay['target']);fixture=ROOT/'fixtures/native-construction/plane-box';path=fixture/'provenance.json';provenance=read(path)
    if digest(replay['service'])!=replay['service_sha256']:raise RuntimeError('Legacy service changed')
    if q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]!=provenance['legacy_source_sha256']:raise RuntimeError('Original Pascal source changed')
    if digest(q.DEFAULT_MODELS[0])!=provenance['model_sha256']:raise RuntimeError('Original input changed')
    for i,variant in enumerate(provenance['variants']):
        for name,value in variant['sha256'].items():
            if digest(fixture/str(i)/name)!=value:raise RuntimeError('Existing baseline changed')
    expected=attributed_model(read(target/'model.json'))
    for match in matches:
        if attributed_model(read(match['model']))!=expected:raise RuntimeError('Replay model differs')
        for key in ('scene','dxf','sections'):
            if digest(match['artifacts'][key])!=match['sha256'][key]:raise RuntimeError('Replay artifact changed')
            name={'scene':'scene.fslp','dxf':'reference.dxf','sections':'sections.json'}[key]
            if digest(target/name)!=match['sha256'][key]:raise RuntimeError('Replay export differs')
    # Control-net geometry must remain exactly the same. Derived traversal and
    # Single rounding are kept verbatim in the recorded output, not normalized.
    original=attributed_model(read(fixture/'0/model.json'))
    for key in ('control_points','control_edges','control_faces','layers','bounds'):
        if expected[key]!=original[key]:raise RuntimeError('Changed legacy control net: '+key)
    index=len(provenance['variants']);dest=fixture/str(index)
    dest.mkdir(exist_ok=False)
    names=('model.json','scene.fslp','reference.dxf','sections.json')
    for name in names:shutil.copy2(target/name,dest/name)
    provenance['variants'].append(dict(original_output=str(target),sha256={n:digest(dest/n) for n in names},
        independent_replay=dict(report=str(a.replay.resolve()),sha256=digest(a.replay),service=replay['service'],
                                service_sha256=replay['service_sha256'],matching_attempts=len(matches)),
        reason='2026-11-09: four independent original-service replays preserve path layout and allocation sizes. Native geometry and variant 0 remain unchanged; this legacy variant is review evidence only.'))
    path.write_text(json.dumps(provenance,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'variant':index,'matches':len(matches),'path':str(dest)},ensure_ascii=False))

if __name__=='__main__':main()
