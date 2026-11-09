"""Native background import/update/remove: exact Plus oracle and raw FBM checks.

Reuses qt_backgrounds' existing semantics and qt_native_project's independent
FBM encoder/observer. This covers the new document owner, not a second UI suite.
"""
import argparse,base64,json,os,struct,subprocess,uuid
from pathlib import Path
from PIL import Image
import qt_migration as q
import qt_modeling as modeling
import qt_editing as e
import qt_backgrounds as backgrounds
import qt_native_project as project
import native_linesplan_app as nla


def images(out):
    out.mkdir(parents=True,exist_ok=True);result={}
    for label,size,mode,options in [('first',(128,96),'RGB',{}),('second',(17,11),'RGB',{}),('progressive',(19,13),'RGB',{'progressive':True}),('gray',(23,7),'L',{}),('cmyk',(29,9),'CMYK',{})]:
        image=Image.new('RGB',size,(45,90,180));image.paste((180,25,75),(0,0,size[0]//2,size[1]//2));image=image.convert(mode)
        path=out/(label+'.jpg');image.save(path,quality=83,**options);result[label]=path
    exif=Image.Exif();exif[274]=6;Image.new('RGB',(31,7),(45,90,180)).save(out/'oriented.jpg',quality=83,exif=exif)
    result['oriented']=out/'oriented.jpg'
    raw=result['first'].read_bytes()
    result['same-bytes']=out/'same-bytes-different-path.jpg';result['same-bytes'].write_bytes(raw)
    for label,data in [('broken',raw[:50]),('empty',b''),('wrong-signature',b'not a JPEG')]:
        path=out/(label+'.jpg');path.write_bytes(data);result[label]=path
    for label,width,height in [('wide',16385,1),('area',8192,4097)]:
        data=bytearray(raw);at=data.index(b'\xff\xc0');struct.pack_into('>HH',data,at+5,height,width)
        path=out/(label+'.jpg');path.write_bytes(data);result[label]=path
    for kind in ('PNG','BMP'):
        path=out/(kind.lower()+'.jpg');Image.new('RGB',(4,3)).save(path,format=kind);result[kind.lower()]=path
    oversized=out/'oversized.jpg'
    with oversized.open('wb') as file:file.write(raw);file.truncate(32*1024*1024+1)
    result['oversized']=oversized
    return result


def probe(r,runtime,out,executable=None):
    out.mkdir(parents=True,exist_ok=True);exe=executable or runtime/'native_background_edit_probe.exe'
    if not exe.is_file():r.case('background.probe',status='not_run',reason='native_background_edit_probe.exe missing');return
    env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','');env['QT_PLUGIN_PATH']=str(runtime)
    assets=images(out/'images');step=0;bindings={'points':[1,2,3,4,5,6,7,8],'arbitrary_metadata':{'preserve':True}}
    def invoke(path,request,minimum_only=False):
        nonlocal step
        step+=1;req=out/f'{step:03}-request.json';target=out/f'{step:03}-result.fbm'
        req.write_text(json.dumps(dict(bindings=bindings,request=dict(method='background',**request),minimum_only=minimum_only),ensure_ascii=False),'utf8')
        process=subprocess.run([str(exe),str(path),str(req),str(target)],env=env,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
        (out/f'{step:03}-probe.log').write_bytes(process.stdout+process.stderr)
        if process.returncode:raise RuntimeError('Background probe failed: '+str(process.returncode))
        return json.loads(process.stdout),target
    def compare_unrelated(label,before,after):
        r.case('background.probe.'+label+'.other-blocks-exact',all(after['blocks'][key]==value for key,value in before['blocks'].items() if key!='background_images'))
    for version in (0,2,9,16,19,20,44):
        path=out/f'v{version}.fbm';before=project.fixture(path,version=version,rich=False)
        result,target=invoke(path,dict(operation='remove',view=0));r.case(f'background.probe.v{version}.remove-noop',result.get('ok') and not result['changed'] and result['byte_identical'] and result['minimum_version']==version,actual=result)
        result,_=invoke(path,dict(operation='update',view=0,changes={}));r.case(f'background.probe.v{version}.missing-update',not result['ok'])
        request=dict(operation='import',view=0,path=str(assets['first']),scale=.125)
        result,_=invoke(path,request,True);r.case(f'background.probe.v{version}.minimum',result.get('ok') and result['minimum_version']==max(20,version),actual=result)
        result,_=invoke(path,dict(request,path=str(assets['broken'])),True);r.case(f'background.probe.v{version}.invalid-before-promotion',not result['ok'])
        if version<20:continue
        result,target=invoke(path,request);r.case(f'background.probe.v{version}.import',result.get('ok') and result['changed'] and result['bindings_unchanged'],actual=result)
        if result.get('ok'):
            after=project.observe(target);compare_unrelated(f'v{version}.import',before,after)
            b=after['backgrounds'][0];r.case(f'background.probe.v{version}.exact-defaults',b==dict(view=0,visible=1,quality=100,origin=[0,96],scale=.125,blending=255,transparent=0,color=0,tolerance=3,image={'size':[128,96],'data':assets['first'].read_bytes()}))
            result,second=invoke(target,request);r.case(f'background.probe.v{version}.repeat-noop',result.get('ok') and not result['changed'] and result['byte_identical'])
    rich=out/'rich.fbm';before=project.fixture(rich);request=dict(operation='update',view=1,changes={'origin':[-17,31],'scale':.375,'alpha':19,'transparent':True,'transparent_color':0xabcdef,'tolerance':211,'visible':False})
    result,updated=invoke(rich,request);r.case('background.probe.rich.update',result.get('ok') and result['changed'] and result['bindings_unchanged'],actual=result)
    after=project.observe(updated);compare_unrelated('rich.update',before,after)
    expected=dict(before['backgrounds'][0],origin=[-17,31],scale=.375,blending=19,transparent=1,color=0xabcdef,tolerance=211,visible=0)
    r.case('background.probe.rich.attributes-jpeg-quality',after['backgrounds']==[expected])
    result,same=invoke(updated,request);r.case('background.probe.rich.update-noop',result.get('ok') and not result['changed'] and result['byte_identical'])
    result,removed=invoke(updated,dict(operation='remove',view=1));removed_data=project.observe(removed);compare_unrelated('rich.remove',after,removed_data)
    r.case('background.probe.rich.remove-only-one-block',result.get('ok') and not removed_data['backgrounds'])
    current=rich
    for view in (0,2,1):
        result,current=invoke(current,dict(operation='import',view=view,path=str(assets['first']),scale=.25))
    r.case('background.probe.replacement-order',[b['view'] for b in project.observe(current)['backgrounds']]==[0,2,1])
    for label in ('broken','empty','wrong-signature','wide','area','png','bmp','oversized'):
        result,target=invoke(current,dict(operation='import',view=0,path=str(assets[label]),scale=.125))
        r.case('background.probe.image-reject-'+label,not result['ok'] and not target.exists(),actual=result)
    for label in ('progressive','gray','cmyk','oriented'):
        result,target=invoke(current,dict(operation='import',view=0,path=str(assets[label]),scale=.125))
        r.case('background.probe.image-accept-'+label,result.get('ok'),actual=result)
        if result.get('ok'):
            item=project.observe(target)['backgrounds'][-1]
            with Image.open(assets[label]) as image:size=image.size
            r.case('background.probe.image-raw-'+label,item['image']=={'size':list(size),'data':assets[label].read_bytes()} and item['origin']==[0,size[1]])
    result,target=invoke(current,dict(operation='import',view=0,path=str(assets['first'])+'\0',scale=.125));r.case('background.probe.nul-path',not result['ok'] and not target.exists())
    tail=out/'tail.fbm';tail.write_bytes(current.read_bytes()+b'Unknown background extension')
    result,_=invoke(tail,dict(operation='update',view=1,changes={}));r.case('background.probe.opaque-noop',result.get('ok') and result['byte_identical'])
    result,target=invoke(tail,dict(operation='update',view=1,changes={'alpha':12}));r.case('background.probe.opaque-edit-rejected',not result['ok'] and not target.exists())
    # Exceeding cardinality and duplicate-view ambiguity cannot slip through a
    # structurally valid FBM just because this operation addresses another view.
    data=project.observe(current);offset,size=data['spans']['background_images'];block=data['blocks']['background_images'];records=block[4:]
    duplicate=bytearray(block);struct.pack_into('<i',duplicate,4+46+len(assets['first'].read_bytes()),0)
    for label,payload in [('too-many',struct.pack('<i',6)+records+records),('duplicate',bytes(duplicate))]:
        bad=out/(label+'.fbm');bad.write_bytes(current.read_bytes()[:offset]+payload+current.read_bytes()[offset+size:])
        result,target=invoke(bad,dict(operation='remove',view=1));r.case('background.probe.invalid-'+label,not result['ok'] and not target.exists())


def run(r,runtime,source,out):
    out.mkdir(parents=True);probe(r,runtime,out/'probe');assets=images(out/'images');timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='background',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                response=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields));results.append((response,json.loads(Path(response['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'background.{step:03}-{label}';ga,gb=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ga==gb,different_fields=[key for key in ga if ga[key]!=gb.get(key)])
            r.case(prefix+'.history',all(a[key]==b[key] for key in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='background':
                r.case(prefix+'.native-owner',a.get('edit_engine')=='cpp-background' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append({'operation':fields['operation'],'edit_ms':a['background_edit_ms'],'total_ms':a['document_total_ms']})
            current=am;return am
        def load(path=source):return both('load','load',path=str(path),document_id=str(uuid.uuid4()))
        def op(operation,view,**fields):return both(operation,operation=operation,view=view,**fields)
        def image(model,view):return next(row for row in model['backgrounds'] if row['view']==view)
        def reject(label,**fields):
            for tag,service in [('native',native),('oracle',oracle)]:
                state=dict(service.state);result=service.command('background',output=out/f'reject-{label}-{tag}',**fields)
                r.case('background.reject-'+label+'.'+tag,not result['ok'] and service.state==state,error=result.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('background.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('background.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        original=load()
        for view in range(3):op('remove',view)
        cleared=current
        for view in range(3):
            current=op('import',view,path=str(assets['first']),scale=.125*(view+1));item=image(current,view)
            expected=dict(view=view,visible=True,origin=[0,96],scale=.125*(view+1),alpha=255,transparent=False,transparent_color=0,tolerance=3,quality=100,jpeg=base64.b64encode(assets['first'].read_bytes()).decode('ascii'))
            r.case('background.import-defaults-'+str(view),item==expected)
            r.case('background.import-keeps-hull-'+str(view),q.geometry(current)==q.geometry(cleared))
        old=dict(native.state);op('import',0,path=str(assets['same-bytes']),scale=.125)
        r.case('background.repeat-import-noop-order',native.state==old and [b['view'] for b in current['backgrounds']]==[0,1,2])
        before=current;patch=dict(origin=[7,-11],scale=.125,visible=False,alpha=96,transparent=True,transparent_color=0xffffff,tolerance=7)
        after=op('update',1,changes=patch);r.case('background.update-exact-appearance',all(image(after,1)[key]==value for key,value in patch.items()));history('appearance',before,after)
        old=dict(native.state);op('update',1,changes=dict(patch,scale=.125+1e-12));r.case('background.single-noop',native.state==old)
        both('undo-noop-seed','undo');old=dict(native.state);op('update',1,changes={});r.case('background.empty-update-retains-redo',native.state==old and native.state['can_redo']);both('redo-noop-seed','redo')
        after=op('import',1,path=str(assets['first']),scale=.125);r.case('background.reimport-resets-and-appends',[b['view'] for b in after['backgrounds']]==[0,2,1] and image(after,1)['visible'] and image(after,1)['alpha']==255 and image(after,1)['origin']==[0,96])
        before=after;after=op('import',0,path=str(assets['second']),scale=.75)
        r.case('background.replace-dimensions-order',[b['view'] for b in after['backgrounds']]==[2,1,0] and image(after,0)['origin']==[0,11] and base64.b64decode(image(after,0)['jpeg'])==assets['second'].read_bytes())
        history('replacement',before,after)
        after=op('remove',2);old=dict(native.state);op('remove',2);r.case('background.remove-missing-noop',native.state==old)
        invalid=[('missing-image',dict(operation='update',view=2,changes={})),('view-negative',dict(operation='remove',view=-1)),('view-perspective',dict(operation='import',view=3,path=str(assets['first']),scale=1)),('unknown-operation',dict(operation='toggle',view=1)),('zero-scale',dict(operation='update',view=1,changes={'alpha':7,'scale':0})),('negative-scale',dict(operation='update',view=1,changes={'scale':-1})),('single-below-minimum',dict(operation='update',view=1,changes={'scale':1e-9})),('too-large-scale',dict(operation='update',view=1,changes={'scale':1e10})),('fractional-origin',dict(operation='update',view=1,changes={'origin':[1.5,2]})),('large-origin',dict(operation='update',view=1,changes={'origin':[1e10,2]})),('short-origin',dict(operation='update',view=1,changes={'origin':[1]})),('alpha',dict(operation='update',view=1,changes={'alpha':256})),('negative-alpha',dict(operation='update',view=1,changes={'alpha':-1})),('fraction-alpha',dict(operation='update',view=1,changes={'alpha':1.5})),('color',dict(operation='update',view=1,changes={'transparent_color':-1})),('tolerance',dict(operation='update',view=1,changes={'tolerance':256})),('boolean',dict(operation='update',view=1,changes={'visible':'true'})),('quality-readonly',dict(operation='update',view=1,changes={'alpha':17,'quality':30})),('missing-path',dict(operation='import',view=1,path=str(out/'missing.jpg'),scale=1)),('stale',dict(operation='remove',view=1,expected_revision=native.state['revision']-1))]
        for label,fields in invalid:reject(label,**fields)
        for label in ('broken','empty','wrong-signature','wide','area','png','bmp','oversized'):reject(label,operation='import',view=1,path=str(assets[label]),scale=1)
        after_rejections=both('after-rejections','snapshot');r.case('background.rejections-keep-images',backgrounds.backgrounds(after_rejections)==backgrounds.backgrounds(after))
        for label in ('progressive','gray','cmyk','oriented'):
            after=op('import',0,path=str(assets[label]),scale=.375);item=image(after,0)
            with Image.open(assets[label]) as picture:height=picture.height
            r.case('background.jpeg-'+label+'-unaltered',base64.b64decode(item['jpeg'])==assets[label].read_bytes() and item['origin']==[0,height] and item['quality']==100)
        after=op('update',1,changes={'origin':[-1000000000,1000000000],'alpha':0,'tolerance':255,'transparent_color':0xffffff,'scale':1e-9*1.1})
        r.case('background.accepted-limits',image(after,1)['scale']==e.f32(1e-9*1.1) and image(after,1)['origin']==[-1000000000,1000000000])
        op('update',1,changes={'scale':.125,'origin':[7,-11]});before=current
        for tag,service in [('native',native),('oracle',oracle)]:
            q.require_ok(service.command('save',output=out/'confirmed-save'/tag));q.require_ok(service.command('mark_saved'))
        saved=Path(q.require_ok(native.command('save',output=out/'saved'))['saved_file']);reopened=load(saved)
        r.case('background.save-reopen-exact',backgrounds.backgrounds(reopened)==backgrounds.backgrounds(before) and nla.reopened_geometry(reopened)==nla.reopened_geometry(before))
        # Native unit conversion was separately migrated; this crossing checks
        # that imported calibration still follows the original Double factor.
        target='imperial' if reopened['settings']['units']=='metric' else 'metric';factor=1/e.FOOT if target=='imperial' else e.FOOT
        after=both('unit-conversion','update_project',changes={'units':target})
        r.case('background.unit-calibration',all(b['scale']==e.f32(a['scale']*factor) and {k:v for k,v in b.items() if k!='scale'}=={k:v for k,v in a.items() if k!='scale'} for a,b in zip(reopened['backgrounds'],after['backgrounds'])))
        both('unit-conversion-undo','undo')
        # Native writer handles the older metadata envelope. Promotion is only
        # needed on a validated import, never remove/no-op/failed image decode.
        legacy=out/'legacy.fbm';project.fixture(legacy,version=9,rich=False);base=load(legacy);old=dict(native.state)
        op('remove',1);r.case('background.legacy-remove-no-upgrade',native.state==old)
        reject('legacy-invalid-before-promotion',operation='import',view=1,path=str(assets['broken']),scale=.25)
        after=op('import',1,path=str(assets['first']),scale=.25);history('legacy-import',base,after)
        saved=Path(q.require_ok(native.command('save',output=out/'legacy-save'))['saved_file']);r.case('background.minimum-version-20',saved.read_bytes()[13]==20)
        before=current;old=dict(native.state);blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        response=native.command('background',operation='update',view=1,changes={'alpha':17},output=blocked)
        r.case('background.publication-failure-atomic',not response['ok'] and native.state==old)
        after=both('publication-rollback','snapshot');r.case('background.publication-model-restored',modeling.geometry(after)==modeling.geometry(before))
        for operation in ('import','update','remove'):
            result=oracle.command('background',external_document=True,operation=operation,view=1,path=str(assets['first']),scale=.25,changes={},output=out/('forbid-'+operation))
            r.case('background.forbid-old-'+operation,not result['ok'] and 'C++' in result.get('error',''))
    r.data['native_background_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);p.add_argument('--probe-only',action='store_true');p.add_argument('--probe',type=Path);a=p.parse_args();r=q.Report()
    try:
        if a.probe_only:probe(r,a.runtime.resolve(),a.output.resolve(),a.probe.resolve() if a.probe else None)
        else:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as error:r.case('background.execution',status='error',reason=f'{type(error).__name__}: {error}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts']);raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
