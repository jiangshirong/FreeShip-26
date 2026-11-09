"""C++ document creation and FBM transitions, checked against the Plus oracle.

Private service processes only. Exact model values and full scene/DXF outputs;
independent versioned fixtures also cover formats the old loader cannot open.
"""
import argparse,copy,io,json,struct,subprocess,uuid
from pathlib import Path
from PIL import Image
import qt_migration as q
import qt_modeling as modeling
import qt_native_project as project


CONTEXT=dict(fbm_encoding='CP1251',layer_name_prefix='Layer',initial_layer_id=0,
             initial_layer_color=0xffff00,default_layer_color=0xffff00,
             initial_underwater_color=0x800000,default_underwater_color=0x800000,
             initial_underwater_alpha=255,default_underwater_alpha=173,resolved_colors={})


def format_checks(r,runtime,out):
    out.mkdir(parents=True);jobs=[];observations={}
    def add(name,operation,**fields):jobs.append(dict(name=name,operation=operation,**fields))
    for version in range(45):
        path=out/f'source-{version}.fbm';observations[version]=project.fixture(path,version=version,rich=True,relations=version>=43)
        add(f'empty-{version}','empty',version=version)
        add(f'same-{version}','promote',path=str(path),version=version)
        add(f'upgrade-{version}','promote',path=str(path),version=44)
        add(f'next-{version}','promote',path=str(path),version=min(version+1,44))
    add('downgrade','promote',path=str(out/'source-44.fbm'),version=43)
    add('invalid-version','empty',version=45)
    add('maximum-grid','new',request=dict(length=10,beam=4,draft=1,columns=100,rows=100,units=0))
    over=exchange_fixture();over['project']['precision']=3;over['points']=[];over['edges']=[];over['faces']=[]
    for i in range(2605):
        start=len(over['points'])
        over['points'] += [dict(p=p,vertex_type=3) for p in ((i,0,0),(i+1,0,0),(i,1,0))]
        over['edges'] += [[start,start+1,True],[start+1,start+2,True],[start+2,start,True]]
        over['faces'].append(dict(points=[start,start+1,start+2],layer=0))
    add('triangle-budget','import',request=over)
    opaque=out/'opaque.fbm';opaque.write_bytes((out/'source-44.fbm').read_bytes()+b'unknown extension')
    add('opaque','promote',path=str(opaque),version=44)
    # Pre-1.6 cached intersections stored XYZ; after 1.6 only two coordinates.
    # Changing the format must invalidate that cache, not reinterpret its bytes.
    old=observations[5];raw=(out/'source-5.fbm').read_bytes();start,size=old['spans']['stations']
    block=struct.pack('<ii4fBii3fB',1,1,1,0,0,-5,1,1,1,5,1,2,1)
    cached=out/'old-cache.fbm';cached.write_bytes(raw[:start]+block+raw[start+size:])
    add('old-cache','promote',path=str(cached),version=44)
    # Loading allocates each layer before reading its stored ID. In particular,
    # loading layer 0 leaves counter 1; a later unnamed layer must be number 2.
    for label,ids in [('zero-layer-id',[0,0]),('descending-layer-ids',[8,7])]:
        original=observations[44];raw=bytearray((out/'source-44.fbm').read_bytes());at=original['spans']['layers'][0]+4
        for identifier in ids:
            length=struct.unpack_from('<i',raw,at)[0];at+=4+length;struct.pack_into('<i',raw,at,identifier);at+=26
        path=out/(label+'.fbm');path.write_bytes(raw);add(label,'load',path=str(path))
    request=out/'request.json';request.write_text(json.dumps(dict(context=CONTEXT,jobs=jobs)),'utf8')
    run=subprocess.run([str(runtime/'native_document_create_probe.exe'),str(request),str(out/'result')],capture_output=True,timeout=90,creationflags=q.common.FLAGS)
    (out/'stdout.json').write_bytes(run.stdout);(out/'stderr.txt').write_bytes(run.stderr)
    root=json.loads(run.stdout);r.case('create.format.probe',run.returncode==0 and root['ok'])
    results={item['name']:item for item in root['results']}
    for job in jobs:
        name=job['name'];reply=results[name];expected=name not in ('downgrade','invalid-version','opaque','triangle-budget')
        r.case('create.format.'+name+'.accepted',reply['ok']==expected,error=reply.get('error'))
        if not expected or not reply['ok']:continue
        path=Path(reply['path']);after=project.observe(path)
        r.case('create.format.'+name+'.complete',not after['tail'])
        if name.startswith('empty-'):
            r.case('create.format.'+name+'.empty',not after['points'] and len(after['layers'])==1 and after['active']==0)
            if name=='empty-44':r.case('create.format.initial-opacity',after['settings']['underwater_alpha']==255)
        elif name.startswith('same-'):
            r.case('create.format.'+name+'.original-bytes',path.read_bytes()==Path(job['path']).read_bytes())
        elif name.startswith(('upgrade-','next-')):
            before=observations[int(name.split('-')[1])]
            r.case('create.format.'+name+'.points',after['points']==before['points'] and all(a[:len(b)]==b for a,b in zip(after['point_attrs'],before['point_attrs'])))
            r.case('create.format.'+name+'.settings',all(after['settings'][k]==v for k,v in before['settings'].items()) and after['preview']==before['preview'])
            r.case('create.format.'+name+'.layers',all(all(a[k]==b[k] for k in ('name','id','color','density','thickness')) and a['attrs'][:len(b['attrs'])]==b['attrs'] for a,b in zip(after['layers'],before['layers'])) and after['active']==before['active'])
            r.case('create.format.'+name+'.annotations',after['markers']==before['markers'] and after['backgrounds']==before['backgrounds'] and after['flows']==before['flows'])
            r.case('create.format.'+name+'.sections',all(after['sections'][k]==v for k,v in before['sections'].items()))
            fixed=['control_edges','control_faces','control_curves','point_names','edge_names','face_names','curve_names','linear_constraints','point_groups','anchors',*before['calculations']]
            r.case('create.format.'+name+'.unchanged-blocks',all(after['blocks'][k]==before['blocks'][k] for k in fixed if k in before['blocks']))
            if before['hydro']:r.case('create.format.'+name+'.hydro',after['hydro']==before['hydro'])
            if name.startswith('upgrade-'):
                defaults=dict(default_split=1,split=0,disable_model_check=0,coefficients=1,save_preview=0,simplify_intersections=1,auto_move=0,bonjean=0,underwater_alpha=173)
                r.case('create.format.'+name+'.defaults',all(after['settings'][k]==v for k,v in defaults.items() if k not in before['settings']))
        elif name=='old-cache':
            r.case('create.format.old-cache-invalidated',after['sections']['stations']==[dict(kind=1,curvature=0,plane=[1,0,0,-5],built=0,curves=[])])
        elif name=='maximum-grid':
            r.case('create.format.maximum-grid-counts',len(after['points'])==10000 and struct.unpack_from('<i',after['blocks']['control_faces'])[0]==9801)
        elif name in ('zero-layer-id','descending-layer-ids'):
            r.case('create.format.'+name+'.allocation-counter',reply['bindings']['last_used_layer_id']==(2 if name=='zero-layer-id' else 9))


def exchange_fixture():
    points=[(0,0,0),(2,0,0),(2,1,0),(0,1,0),(4,0,0),(4,1,0)]
    return dict(format='FreeShipExchange',version=1,creator='Independent fixture',particulars_set=True,
        project=dict(name='Two materials',length=4,beam=2,draft=1,units='metric',precision=1),
        layers=[dict(name='First',color=0x112233,symmetric=False),dict(name='Second',color=0x456789,material_density=7.85,thickness=.01)],
        points=[dict(p=p,vertex_type=1 if i in (1,2) else 3) for i,p in enumerate(points)],
        edges=[[0,1,False],[1,2,False],[2,3,False],[3,0,False],[1,4,True],[4,5,True],[5,2,True]],
        faces=[dict(points=[0,1,2,3],layer=0),dict(points=[1,4,5,2],layer=1)])


def run(r,runtime,models,out):
    out.mkdir(parents=True);timings=[]
    r.data['limitations'][0]='Native document construction, version conversion and snapshot publication; legacy preference/translation bootstrap remains. Hidden tests do not establish desktop acceptance.'
    format_checks(r,runtime,out/'format')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;new_template=False
        def both(label,method='snapshot',**fields):
            nonlocal step,new_template
            step+=1;values=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                reply=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                values.append((reply,json.loads(Path(reply['model']).read_text('utf8'))))
            (a,am),(b,bm)=values;prefix=f'create.{step:03}-{label}'
            ga,gb=modeling.geometry(am),modeling.geometry(bm)
            if method=='new':new_template=True
            elif method in ('load','import_model'):new_template=False
            if new_template:
                # The real application enables previews for new hulls. The
                # headless Plus adapter explicitly disables viewport thumbnails.
                r.case(prefix+'.new-preview-default',am['settings']['save_preview'] is True and bm['settings']['save_preview'] is False)
                gb['settings']=dict(gb['settings'],save_preview=True)
            different={k:dict(actual=ga[k],expected=gb.get(k)) for k in ga if ga[k]!=gb.get(k)}
            if different:(out/f'{step:03}-{label}'/'differences.json').write_text(json.dumps(different,ensure_ascii=False,indent=2),'utf8')
            r.case(prefix+'.model',not different,different_fields=list(different))
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method in ('new','load','import_model'):
                r.case(prefix+'.native-ownership',a['creation_engine']=='cpp-document-create' and a['adapter_operation']=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append(dict(label=label,creation_ms=a['creation_ms']))
            return am,a
        for i,path in enumerate(models):
            both(f'load-{i}','load',path=str(path),document_id=str(uuid.uuid4()))
            saved=q.require_ok(native.command('save',output=out/f'load-{i}-save'))
            r.case(f'create.load-{i}.original-bytes',Path(saved['saved_file']).read_bytes()==path.read_bytes())
        for label,parameters in [
            ('metric',dict(length=10,beam=4,draft=1,columns=7,rows=5,units=0)),
            ('imperial',dict(length=32.5,beam=12.5,draft=3.25,columns=9,rows=6,units=1)),
            ('minimum',dict(length=1,beam=1,draft=1,columns=2,rows=2,units=0)),
        ]:
            model,reply=both(label,'new',document_id=str(uuid.uuid4()),**parameters)
            saved=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                saved.append(project.observe(Path(q.require_ok(service.command('save',output=out/f'{label}-save-{tag}'))['saved_file'])))
            a,b=saved;b=project.remap_saved_points(b,a['points'])
            r.case('create.new-'+label+'.binary-preview-flags',a['settings']['save_preview']==1 and b['settings']['save_preview']==0 and b['preview'] is None)
            image=Image.open(io.BytesIO(a['preview']['data']));image.load()
            r.case('create.new-'+label+'.binary-preview-readable',image.format=='JPEG' and image.size==tuple(a['preview']['size'])==(400,300))
            # Compare every remaining setting and binary attribute, keeping
            # preview validation separate from the adapter's intentional omission.
            b['settings']=dict(b['settings'],save_preview=1)
            r.case('create.new-'+label+'.binary-fields',all(a[k]==b[k] for k in ('settings','layers','active','point_attrs','calculations','hydro')))
            r.case('create.new-'+label+'.binary-topology',all(a['blocks'][k]==b['blocks'][k] for k in ('control_edges','control_faces','control_curves')))
            p=model['control_points'][0];position=list(p['p']);position[2]+=.125
            both(label+'-edit','move_point',point_id=p['id'],position=position)
            both(label+'-undo','undo');both(label+'-redo','redo')
        fixture=out/'fixture.fbm';project.fixture(fixture,rich=False)
        both('fixture','load',path=str(fixture),document_id=str(uuid.uuid4()))
        # Historical files may omit layers or leave a face unassigned. The
        # effective default layer must match LoadProject, including its color.
        observed=project.observe(fixture);blocks=dict(observed['blocks']);blocks['layers']=struct.pack('<ii',0,0)
        faces=bytearray(blocks['control_faces']);offset=4
        for i in range(struct.unpack_from('<i',faces)[0]):
            count=struct.unpack_from('<i',faces,offset)[0];offset+=4+count*4;struct.pack_into('<i',faces,offset,-1);offset+=5
        blocks['control_faces']=bytes(faces);no_layers=out/'no-layers.fbm';no_layers.write_bytes(b''.join(blocks.values()))
        both('no-layers','load',path=str(no_layers),document_id=str(uuid.uuid4()))
        both('no-layers-create','manage_layers',operation='create',changes={})
        data=exchange_fixture();exchange=out/'exchange.json';exchange.write_text(json.dumps(data),'utf8')
        imported,reply=both('import','import_model',path=str(exchange),document_id=str(uuid.uuid4()))
        r.case('create.import.independent-net',len(imported['control_points'])==6 and len(imported['control_faces'])==2 and len(imported['control_edges'])==7 and imported['settings']['created_by']=='Independent fixture')
        e=next(e for e in imported['control_edges'] if {e['a'],e['b']}=={2,3})
        r.case('create.import.interior-crease',not e['crease'] and all(e['crease'] for e in imported['control_edges'] if {e['a'],e['b']}!={2,3}))
        # Every rejected replacement must keep the current document and history.
        state=dict(native.state);saved=q.require_ok(native.command('save',output=out/'before-rejections'))
        saved_bytes=Path(saved['saved_file']).read_bytes()
        cases=[('missing-edge',lambda x:x['edges'].pop()),('duplicate-edge',lambda x:x['edges'].append(x['edges'][0])),
               ('point-type',lambda x:x['points'][0].update(vertex_type=4)),('bad-index',lambda x:x['faces'][0]['points'].__setitem__(0,999)),
               ('bad-units',lambda x:x['project'].update(units='unknown')),('nonmanifold',lambda x:x['faces'].extend([x['faces'][0]]*2))]
        for label,mutate in cases:
            bad=copy.deepcopy(data);mutate(bad);path=out/(label+'.json');path.write_text(json.dumps(bad),'utf8')
            response=native.command('import_model',path=str(path),document_id=str(uuid.uuid4()),output=out/label)
            r.case('create.reject.'+label,not response['ok'] and native.state==state,error=response.get('error'))
        params=dict(length=10,beam=4,draft=1,columns=7,rows=5,units=0)
        for label,change in [('negative-length',dict(length=-1)),('bad-grid',dict(rows=101)),('bad-units',dict(units=2)),('fractional-grid',dict(columns=3.5))]:
            response=native.command('new',document_id=str(uuid.uuid4()),output=out/('new-'+label),**(params|change))
            r.case('create.reject.new-'+label,not response['ok'] and native.state==state)
        blocked=out/'blocked-publication';(blocked/'surface.json').mkdir(parents=True)
        response=native.command('new',document_id=str(uuid.uuid4()),output=blocked,**params)
        r.case('create.publication-rollback',not response['ok'] and native.state==state)
        after=q.require_ok(native.command('save',output=out/'after-rejections'))
        r.case('create.rejected-replacements-preserve-bytes',Path(after['saved_file']).read_bytes()==saved_bytes)
        both('after-rejections')
        # Versions 0..6 are decoded by C++; only the analysis transport is
        # upgraded to the old adapter's earliest readable project layout.
        for version in range(7):
            path=out/'format'/f'source-{version}.fbm';expected=project.observe(path)
            reply=q.require_ok(native.command('load',path=str(path),document_id=str(uuid.uuid4()),output=out/f'early-{version}'))
            model=json.loads(Path(reply['model']).read_text('utf8'))
            r.case(f'create.early-{version}.coordinates',[p['p'] for p in model['control_points']]==expected['points'])
            saved=q.require_ok(native.command('save',output=out/f'early-{version}-save'))
            r.case(f'create.early-{version}.unchanged-save',Path(saved['saved_file']).read_bytes()==path.read_bytes())
            changed=q.require_ok(native.command('update_project',changes=dict(simplify_intersections=False),output=out/f'early-{version}-promote'))
            saved=q.require_ok(native.command('save',output=out/f'early-{version}-promoted-save'))
            r.case(f'create.early-{version}.minimum-upgrade',project.observe(Path(saved['saved_file']))['version']==18)
            q.require_ok(native.command('undo',output=out/f'early-{version}-undo'))
            saved=q.require_ok(native.command('save',output=out/f'early-{version}-undo-save'))
            r.case(f'create.early-{version}.undo-original',Path(saved['saved_file']).read_bytes()==path.read_bytes())
        for method in ('load','new','import_model','promote_document'):
            reply=oracle.command(method,external_document=True,document_id=str(uuid.uuid4()),output=out/('forbidden-'+method))
            r.case('create.external-adapter-rejects-'+method,not reply['ok'] and 'C++' in reply.get('error',''))
    r.data['native_create_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,action='append');p.add_argument('--format-only',action='store_true');a=p.parse_args();r=q.Report()
    try:
        if a.format_only:format_checks(r,a.runtime.resolve(),a.output.resolve())
        else:run(r,a.runtime.resolve(),[x.resolve() for x in (a.model or q.DEFAULT_MODELS)],a.output.resolve())
    except Exception as ex:r.case('create.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
