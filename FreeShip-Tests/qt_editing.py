"""Private M2 project/layer/batch editing, production Qt workflow and save tests.

The protocol driver uses the production persistent service. Fixtures observe
original-core objects with the isolated bridge adaptations; physical unit
expectations are calculated independently. No desktop automation is used.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import shutil
import struct
import time
import uuid

import qt_migration as q
import qt_editing_fixture as fixture

ROOT=q.ROOT
PHASES=('original','project','units','units_undo','layer','batch','batch_undo','batch_redo','reopened')
FOOT=0.3048
DENSITY=1000*FOOT**3/0.45359237


def f32(value): return struct.unpack('<f',struct.pack('<f',value))[0]


def write_report(report,out):
    report.finish()
    (out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')


def points(model): return {p['id']:p['p'] for p in model['control_points']}


def take(report,prefix,server,folder,name,method='snapshot',**args):
    output=folder/name
    if method=='load':args.setdefault('document_id',str(uuid.uuid4()))
    return q.validate_snapshot(report,prefix+'.'+name,server.command(method,output=output,**args),output)


def same(report,name,a,b):
    report.case(name+'.model',q.geometry(a['model'])==q.geometry(b['model']))
    for kind in ('scene','dxf'):report.compare(name+'.'+kind,a['paths'][kind],b['paths'][kind])


def change(report,prefix,server,folder,name,method,**args):
    previous=dict(server.state)
    result=take(report,prefix,server,folder,name,method,**args)
    report.case(prefix+'.'+name+'.one-revision',server.state['revision']==previous['revision']+1)
    report.case(prefix+'.'+name+'.dirty',server.state['modified'] and server.state['can_undo'])
    return result


def no_op(report,prefix,server,folder,name,current,method,**args):
    previous=dict(server.state)
    result=take(report,prefix,server,folder,name,method,**args)
    report.case(prefix+'.'+name+'.same-state-and-redo',server.state==previous)
    same(report,prefix+'.'+name+'.unchanged',result,current)
    return result


def reject(report,prefix,server,folder,name,current,method,**args):
    previous=dict(server.state)
    response=server.command(method,output=folder/(name+'-output'),**args)
    report.case(prefix+'.'+name+'.rejected',response.get('ok') is False,error=response.get('error'))
    report.case(prefix+'.'+name+'.state-preserved',server.state==previous)
    result=take(report,prefix,server,folder,name+'-after')
    same(report,prefix+'.'+name+'.atomic',result,current)


def saved_copy(server,folder):
    folder.mkdir(parents=True,exist_ok=True)
    response=q.require_ok(server.command('save',output=folder/'export'))
    exported=q.artifact(response,'saved_file',folder/'export')
    target=folder/'saved.fbm';shutil.copy2(exported,target)
    q.require_ok(server.command('mark_saved'))
    return target


def jpeg_streams(path):
    """The seeded FBM contains one known background JPEG, no preview JPEG."""
    data=Path(path).read_bytes();result=[];start=0
    while (start:=data.find(b'\xff\xd8',start))>=0:
        end=data.find(b'\xff\xd9',start+2)
        if end<0:raise ValueError('Unterminated embedded JPEG')
        result.append(data[start:end+2]);start=end+2
    return result


def encoded_package(executable,folder):
    folder.mkdir()
    shutil.copy2(executable,folder/executable.name)
    for name in ('locale','Themes','Languages'):shutil.copytree(executable.parent/name,folder/name)
    settings=folder/'.coordinate-settings';settings.mkdir()
    (settings/'FreeShip.ini').write_text('[General]\nFbmEncoding=utf8\n',encoding='utf8')
    return folder/executable.name


def hidden_state_matrix(report,exe,probe,seed,folder):
    prefix='raw';raw_file=Path(seed['artifacts']['system-colors.fbm'])
    raw=json.loads(Path(seed['artifacts']['system-colors-state.json']).read_text('utf8'))
    with q.Service(exe,folder/'raw-service') as server:
        initial=take(report,prefix,server,folder,'raw-initial','load',path=str(raw_file))
        lid=initial['model']['layers'][0]['id'];layer=initial['model']['layers'][0]
        change(report,prefix,server,folder,'raw-redo-seed','update_project',changes={'designer':'Redo must survive'})
        take(report,prefix,server,folder,'raw-redo-undo','undo')
        no_op(report,prefix,server,folder,'raw-layer-noop',initial,'update_layer',layer_id=lid,
              changes={'name':layer['name'],'color':layer['color'],'thickness':layer['thickness'],'material_density':layer['material_density']})
        no_op(report,prefix,server,folder,'raw-underwater-noop',initial,'update_project',changes={'underwater_color':initial['model']['settings']['underwater_color']})
        target=saved_copy(server,folder/'raw-noop-save');observed=fixture.run(probe,target,folder/'raw-noop-inspect')
        report.case(prefix+'.system-colors-preserved-in-fbm',observed['settings']['raw_underwater_color']==raw['settings']['raw_underwater_color'] and observed['layers'][0]['raw_color']==raw['layers'][0]['raw_color'])
        report.case(prefix+'.empty-name-preserved-in-fbm',observed['layers'][0]['raw_name']==raw['layers'][0]['raw_name']=='')
        hidden=change(report,prefix,server,folder,'hidden-split','update_project',changes={'use_default_split_section_location':True,'split_section_location':-5})
        report.case(prefix+'.hidden-split-getter-default',hidden['model']['settings']['split_section_location']==hidden['model']['settings']['length']/2)
        imperial=change(report,prefix,server,folder,'hidden-imperial','update_project',changes={'units':'imperial'})
        target=saved_copy(server,folder/'hidden-imperial-save');observed=fixture.run(probe,target,folder/'hidden-imperial-inspect')
        report.case(prefix+'.hidden-stored-split-converted-and-saved',observed['settings']['stored_split_location']==f32(-5/FOOT),actual=observed['settings']['stored_split_location'],expected=f32(-5/FOOT))
        manual=change(report,prefix,server,folder,'hidden-reveal','update_project',changes={'use_default_split_section_location':False})
        report.case(prefix+'.hidden-manual-reveals-converted-value',manual['model']['settings']['split_section_location']==f32(-5/FOOT))
        restored=take(report,prefix,server,folder,'hidden-off-undo','undo');same(report,prefix+'.hidden-off-undo',restored,imperial)
        restored=take(report,prefix,server,folder,'hidden-units-undo','undo');same(report,prefix+'.hidden-units-undo',restored,hidden)
        target=saved_copy(server,folder/'hidden-undo-save');observed=fixture.run(probe,target,folder/'hidden-undo-inspect')
        report.case(prefix+'.hidden-undo-stored-value',observed['settings']['stored_split_location']==-5)
    utf8_exe=encoded_package(exe,folder/'utf8-bridge')
    utf8_probe=encoded_package(probe,folder/'utf8-probe')
    with q.Service(utf8_exe,folder/'unicode-service') as server:
        current=take(report,'unicode',server,folder,'unicode-initial','load',path=str(raw_file));lid=current['model']['layers'][0]['id']
        report.case('unicode.config-selected',current['model']['settings']['text_encoding'].lower() in ('utf8','utf-8'))
        patch={'name':'船舶 Корабль','designer':'设计者 Конструктор','comment':'第一行\r\nВторая строка'}
        change(report,'unicode',server,folder,'unicode-project','update_project',changes=patch)
        change(report,'unicode',server,folder,'unicode-layer','update_layer',layer_id=lid,changes={'name':'外板 Обшивка'})
        target=saved_copy(server,folder/'unicode-save')
    with q.Service(utf8_exe,folder/'unicode-reopen-service') as server:
        reopened=take(report,'unicode',server,folder,'unicode-reopened','load',path=str(target))
        report.case('unicode.project-text-new-process',all(reopened['model']['settings'][k]==v for k,v in patch.items()))
        report.case('unicode.layer-text-new-process',reopened['model']['layers'][0]['name']=='外板 Обшивка')
        # An explicit default label -> empty raw name changes saved state even
        # though the effective display label is identical.
        label=current['model']['layers'][0]['name']
        named=change(report,'unicode',server,folder,'explicit-default-name','update_layer',layer_id=lid,changes={'name':label})
        target=saved_copy(server,folder/'explicit-default-save');observed=fixture.run(utf8_probe,target,folder/'explicit-default-inspect')
        report.case('unicode.explicit-default-raw-name-stored',observed['layers'][0]['raw_name']==label)
        emptied=change(report,'unicode',server,folder,'empty-default-name','update_layer',layer_id=lid,changes={'name':''})
        report.case('unicode.empty-default-same-display',emptied['model']['layers'][0]['name']==named['model']['layers'][0]['name'])
        target=saved_copy(server,folder/'empty-default-save');observed=fixture.run(utf8_probe,target,folder/'empty-default-inspect')
        report.case('unicode.empty-default-raw-name-cleared',observed['layers'][0]['raw_name']=='')
        take(report,'unicode',server,folder,'empty-default-undo','undo')
        target=saved_copy(server,folder/'empty-undo-save');observed=fixture.run(utf8_probe,target,folder/'empty-undo-inspect')
        report.case('unicode.empty-default-undo-restores-raw-name',observed['layers'][0]['raw_name']==label)


def property_matrix(report,exe,probe,model,folder):
    """One independent patch covers every persisted project/layer property."""
    prefix='properties'
    with q.Service(exe,folder/'property-service') as server:
        initial=take(report,prefix,server,folder,'property-initial','load',path=str(model))
        values=initial['model']['settings']
        patch={'name':'All persisted fields','designer':'Independent property fixture','comment':'First\r\nSecond',
            'length':values['length']+1.25,'beam':values['beam']+.5,'draft':values['draft']+.25,
            'water_density':1.1,'appendage_coefficient':1.075,'split_section_location':-2.5,
            'use_default_split_section_location':False,'simplify_intersections':not values['simplify_intersections'],
            'precision':1 if values['precision']!=1 else 0,'shade_underwater':not values['shade_underwater'],'underwater_color':0xA07010}
        changed=change(report,prefix,server,folder,'all-project','update_project',changes=patch)
        expected={k:f32(v) if type(v) is float else v for k,v in patch.items()}
        report.case(prefix+'.all-project-values',all(changed['model']['settings'][k]==v for k,v in expected.items()))
        report.case(prefix+'.dimensions-do-not-resize-control-hull',points(changed['model'])==points(initial['model']))
        restored=take(report,prefix,server,folder,'all-project-undo','undo');same(report,prefix+'.all-project-single-undo',restored,initial)
        redone=take(report,prefix,server,folder,'all-project-redo','redo');same(report,prefix+'.all-project-redo',redone,changed)
        no_op(report,prefix,server,folder,'same-single-project',changed,'update_project',changes={'length':expected['length']+1e-12})
        lid=initial['model']['layers'][0]['id'];original_layer=initial['model']['layers'][0]
        layer_patch={'name':'All layer fields','color':0x3377AA,'visible':False,'symmetric':False,'developable':not original_layer['developable'],
            'use_in_hydrostatics':False,'show_in_linesplan':False,'use_for_intersections':False,'thickness':8.125,'material_density':7.85}
        layer_changed=change(report,prefix,server,folder,'all-layer','update_layer',layer_id=lid,changes=layer_patch)
        expected_layer={k:f32(v) if type(v) is float else v for k,v in layer_patch.items()}
        report.case(prefix+'.all-layer-values',all(layer_changed['model']['layers'][0][k]==v for k,v in expected_layer.items()))
        restored=take(report,prefix,server,folder,'all-layer-undo','undo');same(report,prefix+'.all-layer-single-undo',restored,changed)
        take(report,prefix,server,folder,'all-layer-redo','redo')
        hydro=change(report,prefix,server,folder,'hydro-auto-symmetry','update_layer',layer_id=lid,changes={'use_in_hydrostatics':True})
        report.case(prefix+'.hydro-enables-symmetry',hydro['model']['layers'][0]['symmetric'] and hydro['model']['layers'][0]['use_in_hydrostatics'])
        report.case(prefix+'.independent-layer-flags-preserved',all(not hydro['model']['layers'][0][k] for k in ('visible','show_in_linesplan','use_for_intersections')))
        change(report,prefix,server,folder,'hydro-off-sym-off','update_layer',layer_id=lid,changes={'use_in_hydrostatics':False,'symmetric':False})
        explicit=change(report,prefix,server,folder,'hydro-explicit-symmetry','update_layer',layer_id=lid,changes={'use_in_hydrostatics':True,'symmetric':False})
        report.case(prefix+'.explicit-symmetry-takes-precedence',not explicit['model']['layers'][0]['symmetric'] and explicit['model']['layers'][0]['use_in_hydrostatics'])
        # Hidden points are deliberately legal through the service; visibility
        # only filters picking and selection in the Qt view.
        free=[p for p in explicit['model']['control_points'] if not p['locked'] and p['batch_movable']]
        moves=[{'point_id':p['id'],'position':[p['p'][0],p['p'][1],p['p'][2]+.025]} for p in free[:2]]
        current=change(report,prefix,server,folder,'hidden-api-batch','move_points',moves=moves)
        for name,method,params in (
            ('layer','update_layer',{'layer_id':lid,'changes':{'thickness':9.75}}),
            ('batch','move_points',{'moves':[{'point_id':free[0]['id'],'position':[0,0,1]},{'point_id':free[1]['id'],'position':[1,1,2]}]})):
            blocked=folder/(name+'-late-failure');blocked.mkdir();(blocked/'model.json').mkdir()
            state=dict(server.state);response=server.command(method,output=blocked,**params)
            report.case(prefix+'.'+name+'-late-failure-rejected',not response['ok'])
            after=take(report,prefix,server,folder,name+'-late-failure-after')
            report.case(prefix+'.'+name+'-late-failure-history',state==server.state)
            same(report,prefix+'.'+name+'-late-failure-atomic',after,current)
        target=saved_copy(server,folder/'all-properties-save')
    with q.Service(exe,folder/'property-reopen-service') as server:
        reopened=take(report,prefix,server,folder,'all-properties-reopen','load',path=str(target))
        report.case(prefix+'.all-project-persisted',all(reopened['model']['settings'][k]==v for k,v in expected.items()))
        expected_layer.update(use_in_hydrostatics=True,symmetric=False)
        report.case(prefix+'.all-layer-persisted',all(reopened['model']['layers'][0][k]==v for k,v in expected_layer.items()))
        for kind in ('scene','dxf'):report.compare(prefix+'.all-properties-reopen-'+kind,reopened['paths'][kind],current['paths'][kind])
    with q.Service(exe,folder/'case-only-service') as server:
        initial=take(report,prefix,server,folder,'case-only-initial','load',path=str(model));lid=initial['model']['layers'][0]['id']
        upper=change(report,prefix,server,folder,'case-only-upper','update_layer',layer_id=lid,changes={'name':'Hull ABC'})
        lower=change(report,prefix,server,folder,'case-only-lower','update_layer',layer_id=lid,changes={'name':'hull abc'})
        report.case(prefix+'.case-only-snapshot-exact',lower['model']['layers'][0]['name']=='hull abc')
        undone=take(report,prefix,server,folder,'case-only-undo','undo');same(report,prefix+'.case-only-undo-exact',undone,upper)
        redone=take(report,prefix,server,folder,'case-only-redo','redo');same(report,prefix+'.case-only-redo-exact',redone,lower)
        target=saved_copy(server,folder/'case-only-save')
    observed=fixture.run(probe,target,folder/'case-only-inspect')
    report.case(prefix+'.case-only-original-loader-saved-text',observed['layers'][0]['raw_name']=='hull abc')


def recipe(report,prefix,exe,model,folder,native_linesplan=None):
    folder.mkdir(parents=True,exist_ok=True);stages={}
    with q.Service(exe,folder/'service',native_linesplan=native_linesplan) as service:
        stages['original']=take(report,prefix,service,folder,'original','load',path=str(model))
        initial=stages['original']['model'];settings=initial['settings']
        patch={'name':'Qt M2 project','designer':'Qt M2 designer','comment':'Qt M2 comment','draft':settings['draft']+0.125}
        stages['project']=change(report,prefix,service,folder,'project','update_project',changes=patch)
        report.case(prefix+'.project-explicit-values',all(stages['project']['model']['settings'][k]==(f32(v) if isinstance(v,float) else v) for k,v in patch.items()))
        report.case(prefix+'.draft-does-not-rescale-hull',points(stages['project']['model'])==points(initial))
        opposite='imperial' if initial['units']=='metric' else 'metric'
        stages['units']=change(report,prefix,service,folder,'units','update_project',changes={'units':opposite})
        factor=1/FOOT if opposite=='imperial' else FOOT
        # Original Model_Scale stores its coordinate scale vector as Single.
        expected={i:[f32(v*f32(factor)) for v in p] for i,p in points(initial).items()}
        report.case(prefix+'.units-independent-control-points',points(stages['units']['model'])==expected)
        report.case(prefix+'.units-lock-flags-preserved',[p['locked'] for p in initial['control_points']]==[p['locked'] for p in stages['units']['model']['control_points']])
        stages['units_undo']=take(report,prefix,service,folder,'units_undo','undo')
        same(report,prefix+'.units-one-undo',stages['units_undo'],stages['project'])
        no_op(report,prefix,service,folder,'noop-project',stages['project'],'update_project',changes=patch)
        lid=initial['layers'][0]['id']
        layer_patch={'name':'Qt M2 layer','color':0x112233,'thickness':12.5,'material_density':7.5}
        stages['layer']=change(report,prefix,service,folder,'layer','update_layer',layer_id=lid,changes=layer_patch)
        actual=next(l for l in stages['layer']['model']['layers'] if l['id']==lid)
        report.case(prefix+'.layer-explicit-values',all(actual[k]==v for k,v in layer_patch.items()))
        free=[p for p in stages['layer']['model']['control_points'] if not p['locked'] and p['batch_movable']]
        if len(free)<2:raise ValueError('Recipe requires two free points')
        moves=[{'point_id':p['id'],'position':[p['p'][0],p['p'][1],p['p'][2]+0.01]} for p in free[:2]]
        stages['batch']=change(report,prefix,service,folder,'batch','move_points',moves=moves)
        expected=points(stages['layer']['model']).copy()
        for move in moves:expected[move['point_id']]=[f32(v) for v in move['position']]
        report.case(prefix+'.batch-only-requested-points-once',points(stages['batch']['model'])==expected)
        stages['batch_undo']=take(report,prefix,service,folder,'batch_undo','undo')
        same(report,prefix+'.batch-one-undo',stages['batch_undo'],stages['layer'])
        no_op(report,prefix,service,folder,'noop-layer',stages['layer'],'update_layer',layer_id=lid,changes=layer_patch)
        no_op(report,prefix,service,folder,'noop-batch',stages['layer'],'move_points',moves=[{'point_id':p['id'],'position':p['p']} for p in free[:2]])
        stages['batch_redo']=take(report,prefix,service,folder,'batch_redo','redo')
        same(report,prefix+'.batch-redo-exact',stages['batch_redo'],stages['batch'])
        current=stages['batch']
        errors=[('unknown-project','update_project',{'changes':{'draft':5,'unsupported':1}}),
                ('temperature-not-stored','update_project',{'changes':{'name':'Must not commit','water_temperature':20}}),
                ('negative-length','update_project',{'changes':{'designer':'Must not commit','length':-1}}),
                ('invalid-units','update_project',{'changes':{'units':'yards'}}),
                ('invalid-precision','update_project',{'changes':{'precision':4}}),
                ('stale-project','update_project',{'changes':{'draft':5},'expected_revision':service.state['revision']-1}),
                ('unknown-layer','update_layer',{'layer_id':99999999,'changes':layer_patch}),
                ('negative-layer-thickness','update_layer',{'layer_id':lid,'changes':{'name':'Must not commit','thickness':-1}}),
                ('unknown-layer-key','update_layer',{'layer_id':lid,'changes':{'color':1,'unknown':1}}),
                ('duplicate-batch-id','move_points',{'moves':[moves[0],moves[0]]}),
                ('unknown-batch-id','move_points',{'moves':[moves[0],{'point_id':99999999,'position':[0,0,0]}]}),
                ('nonfinite-batch','move_points',{'moves':[moves[0],{'point_id':moves[1]['point_id'],'position':['EXPONENT_OVERFLOW',0,0]}],'raw_nonfinite':True})]
        for name,method,args in errors:reject(report,prefix,service,folder,name,current,method,**args)
        encoding=current['model']['settings']['text_encoding']
        for label,text in (('chinese','船舶设计'),('russian','Корабль')):
            try: representable=text.encode(encoding).decode(encoding)==text
            except UnicodeError:representable=False
            if not representable:
                reject(report,prefix,service,folder,'text-'+label,current,'update_project',changes={'name':text,'draft':6})
                reject(report,prefix,service,folder,'layer-text-'+label,current,'update_layer',layer_id=lid,changes={'name':text,'color':123})
            else:report.case(prefix+'.text-'+label+'-rejection',status='not_run',reason='Text is representable; persistence checked separately')
        blocked=folder/'late-failure-output';blocked.mkdir();(blocked/'model.json').mkdir()
        previous=dict(service.state)
        response=service.command('update_project',output=blocked,changes={'draft':current['model']['settings']['draft']+0.5})
        report.case(prefix+'.late-publish-failure-rejected',not response['ok'])
        after=take(report,prefix,service,folder,'after-late-failure')
        report.case(prefix+'.late-failure-history-preserved',service.state==previous)
        same(report,prefix+'.late-failure-rollback',after,current)
        saved=saved_copy(service,folder/'saved')
        report.case(prefix+'.save-clean',service.state['modified'] is False)
    with q.Service(exe,folder/'reopen-service',native_linesplan=native_linesplan) as service:
        stages['reopened']=take(report,prefix,service,folder,'reopened','load',path=str(saved))
        for kind in ('scene','dxf'):report.compare(prefix+'.reopened-'+kind,stages['reopened']['paths'][kind],stages['batch']['paths'][kind])
        report.case(prefix+'.reopened-project-text',all(stages['reopened']['model']['settings'][k]==patch[k] for k in ('name','designer','comment')))
        report.case(prefix+'.reopened-layer-values',all(stages['reopened']['model']['layers'][0][k]==v for k,v in layer_patch.items()))
    return stages


def fixture_matrix(report,exe,probe,model,folder):
    folder.mkdir();prefix='fixture'
    seed=fixture.run(probe,model,folder/'seed','seed')
    seeded=Path(seed['artifacts']['seed.fbm'])
    original=fixture.run(probe,seeded,folder/'seed-inspect')
    embedded=jpeg_streams(seeded)
    report.case(prefix+'.known-background-jpeg-present',len(embedded)==1)
    with q.Service(exe,folder/'units-service') as server:
        initial=take(report,prefix,server,folder,'initial','load',path=str(seeded))
        target=saved_copy(server,folder/'initial-save')
        report.case(prefix+'.initial-save-jpeg-exact',jpeg_streams(target)==embedded)
        imperial=change(report,prefix,server,folder,'imperial','update_project',changes={'units':'imperial'})
        imp_file=saved_copy(server,folder/'imperial-save');imp=fixture.run(probe,imp_file,folder/'imperial-inspect')
        report.case(prefix+'.imperial-save-jpeg-exact',jpeg_streams(imp_file)==embedded)
        for field in ('length','beam','draft','hydro_start_draft','hydro_end_draft','hydro_draft_step','hydro_trim'):
            report.case(prefix+'.imperial-'+field,imp['settings'][field]==f32(original['settings'][field]/FOOT),actual=imp['settings'][field],expected=f32(original['settings'][field]/FOOT))
        report.case(prefix+'.imperial-water-density',imp['settings']['water_density']==f32(original['settings']['water_density']*DENSITY))
        for a,b in zip(original['layers'],imp['layers']):
            report.case(prefix+'.imperial-layer-'+str(a['id']),b['thickness']==f32(a['thickness']/25.4) and b['material_density']==f32(a['material_density']*DENSITY))
        for a,b in zip(original['backgrounds'],imp['backgrounds']):
            report.case(prefix+'.background-scale',b['scale']==f32(a['scale']/FOOT) and a['origin']==b['origin'] and (a['width'],a['height'])==(b['width'],b['height']))
        for a,b in zip(original['flowlines'],imp['flowlines']):
            report.case(prefix+'.flow-seed-length-units',b['projection_point']==[f32(v/FOOT) for v in a['projection_point']])
        for name,sections in original['intersections'].items():
            converted=imp['intersections'][name]
            report.case(prefix+'.'+name+'-count',len(sections)==len(converted))
            for index,(a,b) in enumerate(zip(sections,converted)):
                # Intersection planes use the Double unit factor directly;
                # unlike hull coordinates they do not use Model_Scale's vector.
                expected=f32(a['plane'][3]/FOOT)
                report.case(prefix+'.'+name+'-'+str(index)+'-plane',a['plane'][:3]==b['plane'][:3] and b['plane'][3]==expected,actual=b['plane'],expected=a['plane'][:3]+[expected])
        restored=take(report,prefix,server,folder,'units-undo','undo');same(report,prefix+'.units-undo-exact',restored,initial)
        target=saved_copy(server,folder/'units-undo-save');report.case(prefix+'.undo-save-jpeg-exact',jpeg_streams(target)==embedded)
        take(report,prefix,server,folder,'units-redo','redo')
        target=saved_copy(server,folder/'units-redo-save');report.case(prefix+'.redo-save-jpeg-exact',jpeg_streams(target)==embedded)
        metric=change(report,prefix,server,folder,'metric','update_project',changes={'units':'metric'})
        metric_file=saved_copy(server,folder/'metric-save');back=fixture.run(probe,metric_file,folder/'metric-inspect')
        report.case(prefix+'.metric-save-jpeg-exact',jpeg_streams(metric_file)==embedded)
        report.case(prefix+'.aux-roundtrip',back['backgrounds'][0]['scale']==original['backgrounds'][0]['scale'] and back['flowlines']==original['flowlines'])
        report.case(prefix+'.reverse-control-point-scale',Counter(tuple(p['p']) for p in back['control_points'])==Counter(tuple(f32(v*f32(FOOT)) for v in p['p']) for p in imp['control_points']))
        report.case(prefix+'.reverse-water-density',back['settings']['water_density']==f32(imp['settings']['water_density']/DENSITY))
        for a,b in zip(imp['layers'],back['layers']):
            report.case(prefix+'.reverse-layer-'+str(a['id']),b['thickness']==f32(a['thickness']*25.4) and b['material_density']==f32(a['material_density']/DENSITY))
        for name,sections in imp['intersections'].items():
            report.case(prefix+'.reverse-'+name+'-planes',all(a['plane'][:3]==b['plane'][:3] and b['plane'][3]==f32(a['plane'][3]*FOOT) for a,b in zip(sections,back['intersections'][name])))
        # An explicit value accompanying units belongs to the target unit.
        explicit=change(report,prefix,server,folder,'explicit-target','update_project',changes={'units':'imperial','length':9.25/FOOT,'water_density':70.0})
        report.case(prefix+'.explicit-target-single',explicit['model']['settings']['length']==f32(9.25/FOOT) and explicit['model']['settings']['water_density']==70.0)
    with q.Service(exe,folder/'jpeg-reopen-service') as server:
        take(report,prefix,server,folder,'jpeg-reopen','load',path=str(metric_file))
        target=saved_copy(server,folder/'jpeg-reopen-save');report.case(prefix+'.reopen-save-jpeg-exact',jpeg_streams(target)==embedded)
    constraints=Path(seed['artifacts']['constraints.fbm'])
    relation_oracle=json.loads(Path(seed['artifacts']['constraints-state.json']).read_text('utf8'))
    with q.Service(exe,folder/'constraints-service') as server:
        current=take(report,prefix,server,folder,'constraints','load',path=str(constraints))
        observed={tuple(p['p']):p for p in current['model']['control_points']}
        independent={p['id']:p for p in relation_oracle['control_points']}
        if len(observed)!=len(current['model']['control_points']) or len({tuple(p['p']) for p in independent.values()})!=len(independent):
            raise ValueError('Constraint fixture needs unique coordinates for independent point matching')
        matched={identifier:observed[tuple(p['p'])] for identifier,p in independent.items()}
        report.case(prefix+'.batch-eligibility-independent-original-object-relations',
            all((matched[i]['batch_movable'],matched[i]['locked'])==(p['batch_movable'],p['locked']) for i,p in independent.items()),
            oracle=str(seed['artifacts']['constraints-state.json']),comparison='Original object forward and reverse anchor/linear relations, matched by unique coordinates')
        # Select cases from the independent original objects. A missing service
        # dependency flag must not silently classify an invalid point as free.
        free=[matched[i] for i,p in independent.items() if p['batch_movable'] and not p['locked']]
        related=[matched[i] for i,p in independent.items() if not p['batch_movable'] and not p['locked']]
        locked=[matched[i] for i,p in independent.items() if p['locked']]
        report.case(prefix+'.fixture-has-independent-related-locked',len(free)>=2 and bool(related) and bool(locked))
        move=lambda p:{'point_id':p['id'],'position':[p['p'][0],p['p'][1],p['p'][2]+.01]}
        for index,p in enumerate(related+locked):
            reject(report,prefix,server,folder,'related-or-locked-'+str(index),current,'move_points',moves=[move(free[0]),move(p)])
            reject(report,prefix,server,folder,'reverse-related-or-locked-'+str(index),current,'move_points',moves=[move(p),move(free[0])])
        good=change(report,prefix,server,folder,'free-in-constrained-model','move_points',moves=[move(p) for p in free[:2]])
        restored=take(report,prefix,server,folder,'free-undo','undo');same(report,prefix+'.free-undo',restored,current)
        dependent=next(p for p in independent.values() if p['anchor'] and not p['locked'])
        anchor=matched[dependent['anchor']];child=matched[dependent['id']]
        position=list(anchor['p']);position[2]=f32(position[2]+.25)
        single=change(report,prefix,server,folder,'single-related-propagation','move_point',point_id=anchor['id'],position=position)
        expected=points(current['model']).copy();expected[anchor['id']]=position
        delta=f32(position[2]-anchor['p'][2]);expected[child['id']]=list(child['p']);expected[child['id']][2]=f32(child['p'][2]+delta)
        report.case(prefix+'.single-anchor-moves-dependent-exactly-once',points(single['model'])==expected)
        restored=take(report,prefix,server,folder,'single-anchor-undo','undo');same(report,prefix+'.single-anchor-one-undo',restored,current)
    legacy=Path(seed['artifacts']['legacy.fbm'])
    with q.Service(exe,folder/'legacy-lock-service') as server:
        initial=take(report,prefix,server,folder,'legacy-lock-load','load',path=str(legacy))
        point=next(p for p in initial['model']['control_points'] if not p['locked'])
        locked=change(report,prefix,server,folder,'legacy-lock','topology',operation='set_locked',ids=[point['id']],locked=True)
        report.case(prefix+'.legacy-point-locked',next(p for p in locked['model']['control_points'] if p['id']==point['id'])['locked'])
        undone=take(report,prefix,server,folder,'legacy-lock-undo','undo');same(report,prefix+'.legacy-lock-undo-exact',undone,initial)
        redone=take(report,prefix,server,folder,'legacy-lock-redo','redo');same(report,prefix+'.legacy-lock-redo-exact',redone,locked)
        target=saved_copy(server,folder/'legacy-lock-save')
    inspection=fixture.run(probe,target,folder/'legacy-lock-inspect')
    persisted=next(p for p in inspection['control_points'] if p['p']==point['p'])
    report.case(prefix+'.legacy-lock-persists-original-reader',persisted['locked'])
    with q.Service(exe,folder/'legacy-service') as server:
        current=take(report,prefix,server,folder,'legacy','load',path=str(legacy));lid=current['model']['layers'][0]['id']
        patch={'name':'Legacy material','thickness':9.5,'material_density':7.85,'show_in_linesplan':False}
        updated=change(report,prefix,server,folder,'legacy-layer','update_layer',layer_id=lid,changes=patch)
        undone=take(report,prefix,server,folder,'legacy-undo','undo');same(report,prefix+'.legacy-undo-exact',undone,current)
        redone=take(report,prefix,server,folder,'legacy-redo','redo');same(report,prefix+'.legacy-redo-exact',redone,updated)
        target=saved_copy(server,folder/'legacy-save')
    inspection=fixture.run(probe,target,folder/'legacy-inspect');old=json.loads(Path(seed['artifacts']['legacy-state.json']).read_text('utf8'))
    report.case(prefix+'.legacy-format-promoted',inspection['file_version']>old['file_version'],before=old['file_version'],after=inspection['file_version'])
    report.case(prefix+'.legacy-fields-persist',all(inspection['layers'][0][k]==(f32(v) if isinstance(v,float) else v) for k,v in patch.items()))
    with q.Service(exe,folder/'no-lines-service') as server:
        initial=take(report,prefix,server,folder,'no-lines','load',path=seed['artifacts']['no-intersections.fbm'])
        for layer in initial['model']['layers']:
            current=take(report,prefix,server,folder,'hide-lines-'+str(layer['id']),'update_layer',layer_id=layer['id'],changes={'show_in_linesplan':False})
        report.case(prefix+'.all-layers-excluded-finite',all(not l['show_in_linesplan'] for l in current['model']['layers']))
        saved_copy(server,folder/'no-lines-save')
    hidden_state_matrix(report,exe,probe,seed,folder)
    property_matrix(report,exe,probe,Path(seed['artifacts']['no-intersections.fbm']),folder)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=q.QT)
    parser.add_argument('--model',type=Path,action='append')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--reuse-build',type=Path);parser.add_argument('--runtime',type=Path)
    parser.add_argument('--protocol-only',action='store_true',help='Explicit partial run without Qt workflow/widgets')
    parser.add_argument('--fixtures-only',action='store_true',help='Run only original-object/property fixtures; other paths are explicitly not_run')
    parser.add_argument('--skip-fixtures',action='store_true',help='Explicit partial run without auxiliary-format fixtures')
    parser.add_argument('--verify-deployment',type=Path);parser.add_argument('--workflow-reference',type=Path)
    args=parser.parse_args();args.source=args.source.resolve();args.qt_source=args.qt_source.resolve()
    if args.fixtures_only and args.skip_fixtures:parser.error('--fixtures-only cannot be combined with --skip-fixtures')
    out=args.output.resolve();models=[p.resolve(strict=True) for p in (args.model or q.DEFAULT_MODELS)]
    for protected in (args.source,args.qt_source,q.PILOT):
        if protected==out or protected in out.parents:parser.error('Output must be outside product source')
    if args.verify_deployment:
        if not args.workflow_reference:parser.error('--verify-deployment requires --workflow-reference')
        return q.verify_deployment(args,models,out,'--verify-editing',PHASES)
    out.mkdir(parents=True,exist_ok=False)
    report=q.Report();report.data['suite']='qt-editing';report.data['model_sha256']={str(p):q.common.digest_file(p) for p in models}
    source=q.common.fingerprint(args.source)[0];qt_source=q.current_sources(args.qt_source)
    try:
        bridge=q.prepare_bridge(args,out,q.bridge_builder(args.qt_source),report)
        if args.protocol_only or args.fixtures_only:
            runtime=out/'runtime';(runtime/'bridge').mkdir(parents=True)
            for name in ('FreeShipGeometryBridge.exe','source-id.txt'):shutil.copy2(bridge/name,runtime/'bridge'/name)
            for name in ('locale','Themes','Languages'):shutil.copytree(bridge/name,runtime/'bridge'/name)
            report.case('qt-production-and-widget-paths',status='not_run',reason='Explicit protocol/fixture-only run')
        else:
            runtime=q.prepare_runtime(args,out,bridge,report)
            q.validate_test_manifest(report.data['qt_build_manifest'],editing=True)
            if 'qt_editing_tests.exe' not in report.data['qt_build_manifest']['binaries_sha256']:
                raise RuntimeError('Missing editing test executable provenance')
        exe=runtime/'bridge/FreeShipGeometryBridge.exe'
        if args.fixtures_only:report.case('three-model-protocol-recipes',status='not_run',reason='Explicit --fixtures-only run')
        for index,model in enumerate([] if args.fixtures_only else models,1):
            prefix='model-'+str(index);folder=out/prefix;folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                stages=recipe(report,prefix,exe,private,folder/'protocol')
                entry['protocol']={name:{k:str(v) for k,v in stage['paths'].items()} for name,stage in stages.items()}
                if not args.protocol_only:
                    child=q.run_child(report,prefix+'.production',runtime/'FreeShipQt.exe',['--verify-editing',private],folder/'production')
                    entry['production']=child
                    for phase in PHASES:
                        for kind in ('scene','dxf'):report.compare(prefix+'.production-'+phase+'-'+kind,child['artifacts'][phase+'_'+kind],stages[phase]['paths'][kind])
                    if index==1:entry['widgets']=q.run_child(report,'widgets',runtime/'qt_editing_tests.exe',[stages['original']['paths']['model']],folder/'widgets')
            except Exception as error:report.case(prefix+'.execution',status='error',reason=f'{type(error).__name__}: {error}')
            write_report(report,out)
        if args.skip_fixtures:report.case('auxiliary-format-fixtures',status='not_run',reason='Explicit --skip-fixtures exploration')
        else:
            probe=fixture.build(args.source,args.qt_source,out/'fixture-build')
            fixture_matrix(report,exe,probe,models[0],out/'fixtures')
    except Exception as error:report.case('suite.execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        report.case('original-source-unchanged',q.common.fingerprint(args.source)[0]==source)
        report.case('qt-source-unchanged-during-test',q.current_sources(args.qt_source)==qt_source)
        for model,digest in report.data['model_sha256'].items():report.case('original-model-unchanged-'+str(model),q.common.digest_file(Path(model))==digest)
        report.case('native-desktop-input',status='not_run',reason='Hidden production Qt actions/events only; no desktop automation')
        write_report(report,out)
    print(json.dumps({'status':report.data['status'],'counts':report.data.get('counts'),'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]


if __name__=='__main__':raise SystemExit(main())
