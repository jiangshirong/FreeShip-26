"""Reviewed English mappings for the main-logic 2026-11-13 source delta.

Offline, task-specific migration. Validate exact JSON pointers and fingerprints;
do not use this terminology table as a generic translator.
"""
import argparse
import json
from pathlib import Path

import review_english_contexts as tool


WORDS={
    '运动黏度':'Kinematic viscosity',
    '附体湿表面积':'Appendage wetted areas',
    '艉鳍':'Skeg',
    '舭龙骨':'Bilge keels',
    '艉鳍／船艉后方的舵':'Rudder behind skeg/stern',
    '双桨船舵':'Twin-screw rudders',
    '轴支架':'Shaft brackets',
    '支撑式轴包套':'Strut bossing',
    '稳定鳍':'Stabilizer fins',
    '船体固定式轴包套':'Hull bossing',
    '圆顶附体':'Dome',
    '外露轴（与纵剖线约成 10°）':'Exposed shafts (about 10° to buttock lines)',
    '外露轴（与纵剖线约成 20°）':'Exposed shafts (about 20° to buttock lines)',
    '水的运动黏度':'Water kinematic viscosity',
    '圆顶附体湿表面积':'Dome wetted area',
    '与纵剖线约成 10°':'About 10° to buttock lines',
    '与纵剖线约成 20°':'About 20° to buttock lines',
    '额定主机功率（90%负荷）':'Rated engine power (90% load)',
    '主机可用功率':'Available engine power',
}


def table_at(path):
    return next(json.loads(value) for _,value,kind,_,_ in tool.cpp_literals(path.read_text('utf-8'))
                if kind=='raw_literal' and value.lstrip().startswith('{'))


def at_pointer(obj,pointer):
    for key in pointer.strip('/').split('/'):
        key=key.replace('~1','/').replace('~0','~')
        obj=obj[int(key)] if isinstance(obj,list) else obj[key]
    return obj


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only',action='store_true',help='Refresh the receipt without writing the frozen language file')
    args=parser.parse_args()
    changes_file=tool.QT/'docs/migration/CALCULATION-COPY-CHANGES-20261113.json'
    delta=tool.load(changes_file)
    target=tool.QT/'src/app/languages/en.json'
    before=tool.load(target)
    after=json.loads(json.dumps(before))
    tables={f:table_at(tool.QT/f) for f in delta['source_sha256']}
    for file,expected in delta['source_sha256'].items():
        if tool.digest((tool.QT/file).read_bytes())!=expected:
            raise ValueError('Source changed after the reviewed delta: '+file)
    resolved=[]
    for row in delta['changes']:
        source=row['new']
        if at_pointer(tables[row['file']],row['pointer'])!=source:
            raise ValueError('Delta does not match the current JSON pointer')
        if source in WORDS:
            english=WORDS[source]
        elif source.startswith("'") and source.endswith("'") and source[1:-1] in WORDS:
            english="'"+WORDS[source[1:-1]]+"'"
        elif row['pointer'].endswith('/rs_resistance_holtrop_note_p5'):
            english=('5. AT is the immersed transom area; ABT is the immersed transverse bulbous-bow area at the forward perpendicular.\n'
                "h_b is the height of that bulbous-bow section's centroid above the baseline.")
        elif row['pointer'].endswith('/rs_resistance_holtrop_note_p6'):
            english=('6. The propeller count Np is used for power prediction and hull–propeller interaction estimates.\n'
                'The Np=0 branch uses w=0.3*Cb+10*Cv*Cb-0.1, t=0.1 and relative rotative efficiency ηR=0.98.')
        elif row['pointer'].endswith(('/rs_resistance_hollen_note_p5','/rs_propeller_task4_note')):
            # Both paragraphs were individually source-reviewed in the prior
            # revision; only the Chinese source was brought into agreement.
            english=before['messages'][row['old']]
        elif not tool.CJK.search(source):
            english=source
        else:
            raise ValueError('An English mapping needs source review: '+row['pointer'])
        if tool.CJK.search(english) or tool.parameter_tokens(source)!=tool.parameter_tokens(english):
            raise ValueError('Invalid English source or token contract')
        resolved.append(row|dict(english=english))
        if tool.CJK.search(source):
            after['messages'][source]=english
    previous_hash=tool.digest(target.read_bytes())
    if tool.load(target)!=before:
        raise ValueError('English changed concurrently')
    if args.verify_only:
        if after!=before:
            raise ValueError('English requires a change; notify the build owner before writing it')
    else:
        tool.write(target,after)
    tool.write(tool.QT/'docs/migration/CALCULATION-ENGLISH-SYNC-20261113.json',dict(
        source_delta_sha256=tool.digest(changes_file.read_bytes()),
        source_sha256=delta['source_sha256'],before_sha256=previous_hash,
        after_sha256=tool.digest(target.read_bytes()),entries=len(after['messages']),
        mapping_changes=[dict(source=k,before=before['messages'].get(k),after=v)
            for k,v in after['messages'].items() if before['messages'].get(k)!=v],
        reviewed_changes=resolved,translation_api_calls=0,
        provenance='Repository assistant reviewed the exact source delta against the main-logic investigation; no new model call.'))
    print('English stable:',len(after['messages']),'entries;',len(resolved),'source delta locations verified.')


if __name__=='__main__':
    main()
