"""Targeted simplification checks using the existing section driver and oracle."""
from collections import Counter
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FIXTURE = ROOT / 'fixtures/native_simplify_models.json'


def configure_report(report):
    report.data.update(scope='Standalone explicit spline simplification; no application or export integration',
        acceptance_policy={
            'compatibility': 'Retained points, order, flags, fragments and sampled values/derivatives compare exactly after Single reconstruction; no tolerance.',
            'exceptions': 'Only the named cold-cache and large-knuckle cases can become intentional review, after comparison with separately supplied original-core reference curves.',
            'independent': 'Endpoints, flagged points, subsequence order, strict criterion boundary and zero-length behavior have independent expectations.',
            'gate': 'A heuristic simplification is not an accuracy bound. Review does not authorize production replacement.'},
        limitations=[
            'Frozen model curves are inputs from original Pascal sections, not a native full-model scene or export test.',
            'The legacy weighted-removal criterion is not a positional-error tolerance; its short-span loss example remains review.',
            'This does not establish UI speed, full scenes, mirroring, hydrostatics or export compatibility.'])


def cases():
    def item(name, points, *, flags=None, criterion=2, prebuild=True, count=None):
        value={'id':name, 'kind':'simplify', 'criterion':criterion, 'prebuild':prebuild,
            'chain':{'points':points,'knuckles':flags or [False]*len(points),'fragments':64}}
        if count is not None:value['expected_count']=count
        return value
    dense=[[i/128, 0, 0] for i in range(129)]
    arch=[[i/64, (i/64)*(1-i/64), (i/64)**3/20] for i in range(65)]
    corner=[[0,0,0],[100000,100000,0],[200000,0,0]]
    small=[
        item('dense',dense), item('dense-cold',dense,prebuild=False),
        item('dense-disabled',dense,criterion=0,count=len(dense)),
        item('curved',arch), item('curved-knuckles',arch,flags=[i in (12,32,48) for i in range(65)]),
        item('two-points',[[0,0,0],[1,2,3]],count=2),
        item('coincident',[[1,2,3]]*3,count=3),
        item('large-knuckle',corner,flags=[False,True,False],count=3),
        item('large-knuckle-disabled',corner,flags=[False,True,False],criterion=0,count=3),
        item('large-knuckle-endpoints',[corner[0],corner[-1]],count=2),
        item('tiny-neighbor-span',[[0,0,0],[1,0,0],[1e-6,0,0]],count=2),
    ]
    # A 3-4-5 triangle has total chord length 10 and an exact removal weight
    # 1e8 * 4^2 * 6 / 10^2 = 96000000. Adjacent Single values differ by 8.
    for suffix,criterion,count in [('below',95999992,3),('equal',96000000,3),('above',96000008,2)]:
        small.append(item('criterion-'+suffix,[[0,0,0],[3,4,0],[6,0,0]],criterion=criterion,count=count))
    saved=json.loads(FIXTURE.read_text('utf8'))
    if saved['format']!='FreeShipSimplifyModelInputs' or saved['version']!=1:raise ValueError('Unknown fixture')
    for curve in saved['curves']:
        small.append({'id':curve['id'],'kind':'simplify','criterion':2,'prebuild':True,'chain':curve['chain']})
    import native_sections_cases
    regress=[c for c in native_sections_cases.cases()['cases'] if c['id'] in ('square','cube','layers')]
    return {'format':'FreeShipNativeSectionsCases','version':1,'cases':small+regress}


def payload(value):
    return {k:value[k] for k in ('curves','simplified')}


def exercise(report,inputs,native,pascal,folder):
    import native_sections as sections
    folder.mkdir(parents=True,exist_ok=False)
    wanted=[c['id'] for c in inputs['cases']]
    report.case('simplify.case-order',wanted==[c['id'] for c in native['cases']]==[c['id'] for c in pascal['cases']])
    na={c['id']:c for c in native['cases']};pa={c['id']:c for c in pascal['cases']}
    removals=[]
    for item in inputs['cases']:
        if item['kind']!='simplify':continue
        name=item['id'];a=na[name];b=pa[name]
        if a.get('status')!='ok' or b.get('status')!='ok':
            report.case(name,status='error',native=a,pascal=b);continue
        sections.compare(report,name+'.pascal',payload(a),payload(b),folder,role='native-pascal')
        comparison=report.data['cases'][-1]
        if name=='dense-cold':
            original=payload(pa['dense-disabled']);original['simplified']=False
            justified=sections.exact(payload(a),payload(na['dense'])) and sections.exact(payload(b),original)
            reason='Cold native equals warm native; cold legacy returns false with the original unsimplified points and samples. The original warm curve is compared separately.'
        elif name=='large-knuckle':
            justified=sections.exact(payload(a),payload(pa['large-knuckle-disabled'])) and sections.exact(payload(b),payload(pa['large-knuckle-endpoints']))
            reason='Native retains the explicitly marked corner; the finite legacy sentinel deletes it. Both resulting curves match separately supplied original-core reference curves.'
        else:justified=False;reason=None
        if justified and comparison['status']=='fail':
            comparison.update(status='review',intentional_change=True,reason=reason)
            sections.leaf.dump(folder/(name+'.intentional.json'),{'native':a,'pascal':b,'reason':reason})
        original=item['chain'];current=a['curves'][0]
        before=[(sections.vector_key(p),k) for p,k in zip(original['points'],original['knuckles'])]
        after=[(sections.vector_key(p),k) for p,k in zip(current['points'],current['knuckles'])]
        cursor=iter(before)
        subsequence=all(any(old==new for old in cursor) for new in after)
        marked_before=Counter(pair for pair in before if pair[1]);marked_after=Counter(pair for pair in after if pair[1])
        invariants={'subsequence':subsequence,'endpoints':before[0]==after[0] and before[-1]==after[-1],
            'knuckles':marked_before==marked_after,'fragments':current['fragments']==original['fragments'],
            'expected_count':'expected_count' not in item or len(after)==item['expected_count'],
            'return_value':a['simplified']==(name!='coincident')}
        report.case(name+'.independent',all(invariants.values()),checks=invariants)
        removals.append({'id':name,'before':len(before),'after':len(after)})
    report.case('dense.actual-removal',len(na['dense']['curves'][0]['points'])<129)
    report.case('tiny-neighbor-span.shared-geometric-loss',status='review',
        reason='Both implementations remove the middle point at x=1 when its neighbors are only 1e-6 apart. This inherited criterion can erase a long excursion and is not an error bound.')
    regression_ids=[c['id'] for c in inputs['cases'] if c['kind']!='simplify']
    def subset(data):
        result=copy.copy(data);result['cases']=[c for c in data['cases'] if c['id'] in regression_ids];return result
    sections.exercise(report,'section-regression',subset(inputs),subset(native),subset(pascal),folder/'sections')
    saved=json.loads(FIXTURE.read_text('utf8'))
    report.data['simplify_coverage']={'small_cases':sum(not r['id'].startswith('model-') for r in removals),
        'model_curves':len(saved['curves']),'models':saved['models'],'fixture_provenance':saved['provenance'],
        'point_counts':removals}
