"""Curve-only lines-plan projection: compare actual Pascal display and DXF routes."""
import copy
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parent


def configure_report(report):
    report.data.update(scope='Standalone curve sampling and lines-plan coordinates, not a complete scene/export or application integration',
        acceptance_policy={
            'numbers':'Finite positions compare exactly after Single reconstruction; no tolerance.',
            'oracle':'Active DrawSpline is copied verbatim. Actual AddSpline has four read-only callbacks; removing them restores its exact original bytes.',
            'plan_correction':'A plan-view display difference is intentional only when all native coordinates match an independent XY projection and the original DXF output, and all old display coordinates match the observed X-from-Y defect.',
            'state':'Native export preserves the original density and following display; actual Pascal state mutation is separately recorded.',
            'gate':'Review is not pass. The candidate is not installed or connected to the document.'},
        limitations=[
            'No triangle fill, labels, diagonal projection, origin calculation, document lifecycle or complete DXF file comparison.',
            'The sampler retains the original GetValues insertion rule; sparse sampling can skip intermediate knuckles.',
            'DXF sampling has the inherited fixed count of 100. Display uses the input count; this is not an accuracy or UI speed claim.'])


def cases():
    def item(name,points,view,*,knuckles=None,fragments=100,split=1,mirror=False,diagonals=False):
        return {'id':name,'kind':'linesplan','chain':{'points':points,'knuckles':knuckles or [False]*len(points),'fragments':fragments},
            'view':view,'origin':[11,-7,0],'split':split,'mirror_plan':mirror,'has_diagonals':diagonals}
    straight=[[2,1,3],[8,4,9]]
    output=[item('straight-'+str(view),straight,view,split=5) for view in range(4)]
    section=[[1,0,0],[1,2,1],[1,3,4]]
    for view in (1,2):
        for name,split in [('below',1-2**-24),('equal',1),('above',1+2**-23)]:
            output.append(item('split-'+str(view)+'-'+name,section,view,split=split,knuckles=[False,True,False]))
    output.extend([
        item('plan-mirror',straight,3,mirror=True),
        item('plan-diagonals-suppress-mirror',straight,3,mirror=True,diagonals=True),
        item('density-eight',[[0,0,0],[1,2,3],[3,0,2]],0,fragments=8,knuckles=[False,True,False]),
        item('sparse-knuckles',[[0,0,0],[1,2,0],[2,-2,0],[3,0,0],[4,1,0]],0,fragments=2,knuckles=[False,True,True,True,False]),
        item('repeated-point',[[0,0,0],[1,1,2],[1,1,2],[3,2,1]],0,knuckles=[False,True,True,False]),
    ])
    fixture=json.loads((ROOT/'fixtures/native_simplify_models.json').read_text('utf8'))
    for index,curve in enumerate(fixture['curves']):
        name=curve['id'];chain=copy.deepcopy(curve['chain']);chain['fragments']=100
        view=3 if '.waterline-' in name or '.fixture-z.' in name else 0 if '.buttock-' in name or '.fixture-y.' in name else 1+index%2
        xs=[p[0] for p in chain['points']]
        case=item(name,chain['points'],view,knuckles=chain['knuckles'],split=(min(xs)+max(xs))/2,mirror=view==3)
        output.append(case)
    return {'format':'FreeShipNativeSectionsCases','version':1,'cases':output}


def single(value):return struct.unpack('<f',struct.pack('<f',value))[0]


def plan_reference(samples,origin,mirror,*,legacy=False):
    # Independent XY projection. The legacy branch is the specifically observed
    # false X-axis/zero-sign formula, never a general mismatch allowance.
    if legacy:
        signs=[0,1] if mirror else [1]
        return [[[single(origin[0]+sign*p[1]),single(origin[1]+p[1]),0] for p in samples] for sign in signs]
    return [[[single(origin[0]+p[0]),single(origin[1]+sign*p[1]),0] for p in samples] for sign in ([1,-1] if mirror else [1])]


def exercise(report,inputs,native,pascal,folder):
    import native_sections as sections
    folder.mkdir(parents=True,exist_ok=False)
    ids=[c['id'] for c in inputs['cases']]
    report.case('linesplan.order',ids==[c['id'] for c in native['cases']]==[c['id'] for c in pascal['cases']])
    na={c['id']:c for c in native['cases']};pa={c['id']:c for c in pascal['cases']}
    plan_changes=[];state_changes=[];sample_count=0;export_count=0
    for item in inputs['cases']:
        name=item['id'];a=na[name];b=pa[name]
        if a.get('status')!='ok' or b.get('status')!='ok':
            report.case(name,status='error',native=a,pascal=b);continue
        for field in ('samples','display_paths','dxf_paths'):
            stats=sections.leaf.compare_values(a[field],b[field]);different=stats['numeric_difference_count'] or stats['structure_difference_count']
            status='fail' if different else 'pass';reason=None;intentional=False
            if field=='display_paths' and item['view']==3 and different:
                mirror=item['mirror_plan'] and not item['has_diagonals']
                expected=plan_reference(b['samples'],item['origin'],mirror)
                legacy=plan_reference(b['samples'],item['origin'],mirror,legacy=True)
                justified=item['chain']['fragments']==100 and sections.exact(a[field],expected) and sections.exact(b[field],legacy) and sections.exact(a[field],b['dxf_paths'])
                if justified:
                    status='review';intentional=True;plan_changes.append(name)
                    reason='Correct XY coordinates and optional Y reflection exactly match original DXF vertices; original display instead uses Y for X and sign zero for its mirror.'
            differences=stats.pop('differences')
            evidence=folder/(name+'.'+field+'.json')
            sections.leaf.dump(evidence,{'differences':differences,'native':a[field] if different else None,'pascal':b[field] if different else None})
            report.case(name+'.'+field,status=status,comparison_role='native-pascal',intentional_change=intentional,
                **stats,reason=reason,evidence=str(evidence),first_differences=differences[:3])
        density=item['chain']['fragments']
        report.case(name+'.export-preserves-display',a['fragments_after_export']==density and a['display_after_export']==a['display_paths'])
        report.case(name+'.sample-endpoints',sections.exact(a['samples'][0],item['chain']['points'][0]) and sections.exact(a['samples'][-1],item['chain']['points'][-1]))
        if item['view'] in (1,2):
            xs=[single(p[0]) for p in item['chain']['points']];split=single(item['split'])
            visible=max(xs)<=split if item['view']==1 else min(xs)>=split
            paths=a['display_paths'];origin=item['origin']
            expected=[[[single(origin[0]+sign*p[1]),single(origin[1]+p[2]),0] for p in a['samples']] for sign in (-1,1)] if visible else []
            report.case(name+'.independent-body-reflection',sections.exact(paths,expected))
        if density!=100 and b['fragments_after_export']==100 and not sections.exact(b['display_after_export'],b['display_paths']):state_changes.append(name)
        sample_count+=len(a['samples']);export_count+=sum(len(path) for path in a['dxf_paths'])
    report.case('plan-defect-minimal-reproduction','straight-3' in plan_changes and 'plan-mirror' in plan_changes)
    report.case('legacy-export-mutation-reproduced',set(state_changes)=={'density-eight','sparse-knuckles'})
    report.case('legacy-export-mutates-display',status='review',fixtures=state_changes,
        reason='Actual original AddSpline changes the live fragments field to 100; the following DrawSpline changes. Native export keeps its sampling density local.')
    sparse=na['sparse-knuckles']['samples'];original=next(c for c in inputs['cases'] if c['id']=='sparse-knuckles')['chain']['points']
    missing=[i for i in (1,2,3) if not any(sections.exact(original[i],p) for p in sparse)]
    report.case('sparse-knuckle-loss-reproduced',missing==[1,2])
    report.case('sparse-knuckle-inherited-loss',status='review',missing_indices=missing,
        reason='With two samples, GetValues inserts only the knuckle at the final sampled interval; two earlier marked corners are skipped. Native currently preserves this compatibility behavior.')
    for check in native.get('api_checks',[]):report.case('api.'+check['id'],status=check['status'])
    report.data['linesplan_coverage']={'case_count':len(ids),'model_curve_count':sum(c.startswith('model-') for c in ids),
        'sample_points':sample_count,'export_vertices':export_count,'confirmed_plan_display_changes':plan_changes,
        'density_changes':state_changes,'fixture_sha256':sections.common.digest_file(ROOT/'fixtures/native_simplify_models.json')}
