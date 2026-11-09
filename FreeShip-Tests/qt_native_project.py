"""Native project settings: exact Plus oracle and independent unit/FBM checks.

The fixture encoder/observer is deliberately independent of the C++ codec.
No tolerances are applied to Single fields, calculator data or geometry.
"""
import argparse,io,json,os,shutil,struct,subprocess,uuid
from pathlib import Path
from PIL import Image
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as nla
import qt_native_transforms as affine

f32=e.f32
FOOT=.3048;DENSITY=1000*FOOT*FOOT*FOOT/.45359237;MASS=1000/(2240*.45359237)
RECORD_VERSIONS=[16,16,22,22,22,22,22,23,24,25,26,30,33,35,38,38,40,40]


def fixture(path,version=44,default_split=False,rich=True,relations=False):
    """Versioned fixed control box, two materials, every persisted ancillary kind."""
    data=bytearray()
    def pack(fmt,*v):data.extend(struct.pack('<'+fmt,*v))
    def floats(v):pack('f'*len(v),*v)
    def text(v):
        v=v.encode('ascii') if isinstance(v,str) else v
        pack('i',len(v));data.extend(v)
    jpeg=io.BytesIO();Image.new('RGB',(8,6),(45,90,180)).save(jpeg,format='JPEG',quality=90);jpeg=jpeg.getvalue()
    def image():pack('iii',8,6,len(jpeg));data.extend(jpeg)
    text('FREE!ship');pack('Bi',version,1)
    pack('i',1);data.extend(bytes([1,0,1,1,1,1,1,0,0,0]));pack('f',1)
    if version>=12:pack('B',1)
    if version>=16:pack('f',.1)
    if version>=17:data.extend(bytes(6))
    if version>=20:pack('B',0)
    if version>=2:
        text('Native project oracle');text('Independent unit fixture');floats([10,4,1]);pack('BffBii',0,1.025,1.0625,0,0x5080c0,0)
        if version>=6:pack('Bf',int(default_split),2.125)
        if version>=7:pack('B',0)
        text('Preserve all settings, annotations and calculation bytes');text('Project regression')
    if version>=16:
        pack('iB',1,int(rich))
        if rich:image()
    if version>=18:pack('B',1)
    if version>=20:
        floats([.25,2,.125,.375]);pack('i',2);floats([4,6]);floats([2,8,.5]);pack('Bi',0,2);floats([0,20]);pack('i',2);floats([.1,.2]);pack('Bf',0,1.25)
    if version>=37:pack('B',1)
    if version>=39:pack('B',1)
    if version>=44:pack('i',123)
    pack('i',2)
    for index,name in enumerate(('Hull','Separate material')):
        text(name);pack('iiBBB',7+index,0x80a0c0+index,1,1,0)
        if version>=9:pack('BB',1,1)
        if version>=11:pack('ff',7.85+index,.004+index*.001)
        if version>=15:pack('B',1)
        if version>=21:pack('i',234+index)
    pack('i',0)
    points=[(0,0,0),(10,0,0),(10,2,0),(0,2,0),(0,0,3),(10,0,3),(10,2,3),(0,2,3)]
    faces=[[0,3,2,1],[4,5,6,7],[3,7,6,2],[0,4,7,3],[1,2,6,5]]
    edges=sorted({tuple(sorted((a,b))) for face in faces for a,b in zip(face,face[1:]+face[:1])})
    pack('i',len(points))
    for index,p in enumerate(points):
        floats(p);pack('iB',3,0)
        if version>=13:pack('B',int(relations and index==7))
    pack('i',len(edges))
    for a,b in edges:pack('iiBB',a,b,1,0)
    if version>=12:pack('iiiiiB',1,3,0,1,2,0)
    pack('i',len(faces))
    for index,face in enumerate(faces):pack('i',len(face));pack('i'*len(face),*face);pack('iB',int(index==1),0)
    for kind,plane in enumerate([(1,0,0,-5),(0,1,0,-1),(0,0,1,-1.5),(0,.70710678,.70710678,-1)],1):
        if kind==4 and version<9:break
        pack('ii',1,kind)
        if version>=11:pack('B',1)
        floats(plane);pack('Bi',0,0)
    if version>=11:
        pack('i',1);pack('B',1)
        if version>=21:pack('B',1)
        pack('Bfi',1,.125,2)
        for p in ((1.25,2.5,3.75),(2,3,4)):floats(p);pack('B',1)
    def record(index):
        _,count,flags=affine.RECORDS[index];floats([.375+i*.125+index*.03125 for i in range(count)]);data.extend(bytes(flags)+bytes([0xa5])*(4-flags))
    for index in range(2):
        if version>=RECORD_VERSIONS[index]:record(index)
    if version>=20:
        pack('i',int(rich))
        if rich:pack('iBiiifiBii',1,1,90,7,-11,.125,127,0,0x050607,11);image()
        pack('i',int(rich))
        if rich:pack('ffiBBi',1.25,2.5,1,0,1,0)
    for index in range(2,len(affine.RECORDS)):
        if version>=RECORD_VERSIONS[index]:record(index)
    if version>=41:
        for name in ('Point label','Edge label','Face label','Curve label'):pack('ii',1,0);text(name)
        pack('i',int(relations))
        if relations:pack('iii',2,0,1)
    if version>=42:pack('i',1);text('Named group');pack('i',2);text('0');text('7')
    if version>=43:
        pack('i',int(relations))
        if relations:pack('iiB',6,5,1)
    path.write_bytes(data);return observe(path)


def observe(path):
    """Independent explicit SaveBinary traversal for these standard test fixtures."""
    data=path.read_bytes();at=0;blocks={};offsets={};spans={}
    def take(fmt):
        nonlocal at
        size=struct.calcsize('<'+fmt);result=struct.unpack_from('<'+fmt,data,at);at+=size
        return result[0] if len(result)==1 else result
    def skip(n):
        nonlocal at
        if n<0 or n>len(data)-at:raise ValueError('Invalid fixture span')
        at+=n
    def text():n=take('i');start=at;skip(n);return data[start:at]
    def array():return [take('f') for _ in range(take('i'))]
    def image():w,h,n=take('iii');start=at;skip(n);return dict(size=[w,h],data=data[start:at])
    def block(name,start):blocks[name]=data[start:at];spans[name]=(start,at-start)
    def mark(key):offsets[key]=at
    if text()!=b'FREE!ship':raise ValueError('Fixture signature')
    version=take('B');precision=take('i');block('header',0);start=at
    skip(4+10+4+(version>=12)+(4 if version>=16 else 0)+(6 if version>=17 else 0)+(version>=20));block('visibility',start);start=at
    settings={};hydro={};preview=None
    if version>=2:
        for key in ('name','designer'):mark(key);settings[key]=text()
        for key in ('length','beam','draft'):mark(key);settings[key]=take('f')
        mark('particulars');settings['particulars']=take('B')
        for key in ('water_density','appendage_coefficient'):mark(key);settings[key]=take('f')
        mark('shade_underwater');settings['shade_underwater']=take('B');mark('underwater_color');settings['underwater_color']=take('i');mark('units');settings['units']=take('i')
        if version>=6:
            mark('default_split');settings['default_split']=take('B');mark('split');settings['split']=take('f')
        if version>=7:settings['disable_model_check']=take('B')
        for key in ('comment','created_by'):mark(key);settings[key]=text()
    if version>=16:
        settings['coefficients']=take('i');settings['save_preview']=take('B')
        if settings['save_preview']:preview=image()
    if version>=18:mark('simplify_intersections');settings['simplify_intersections']=take('B')
    if version>=20:
        hydro['drafts']=list(take('ffff'));hydro['displacements']=array();hydro['ranges']=list(take('fff'));hydro['use_steps']=take('B');hydro['heels']=array();hydro['trims']=array();hydro['free_trim']=take('B');hydro['vcg']=take('f')
    if version>=37:settings['auto_move']=take('B')
    if version>=39:settings['bonjean']=take('B')
    if version>=44:settings['underwater_alpha']=take('i')
    block('project_settings',start);start=at;layers=[]
    for _ in range(take('i')):
        name=text();identifier,color=take('ii');attrs=list(take('BBB'))
        if version>=9:attrs+=list(take('BB'))
        density,thickness=take('ff') if version>=11 else (0,0)
        if version>=15:attrs.append(take('B'))
        if version>=21:attrs.append(take('i'))
        layers.append(dict(name=name,id=identifier,color=color,attrs=attrs,density=density,thickness=thickness))
    active=take('i');block('layers',start);start=at;points=[];point_attrs=[]
    for _ in range(take('i')):
        points.append(list(take('fff')));attrs=list(take('iB'))
        if version>=13:attrs.append(take('B'))
        point_attrs.append(attrs)
    block('control_points',start);start=at;skip(take('i')*10);block('control_edges',start)
    if version>=12:
        start=at
        for _ in range(take('i')):skip(take('i')*4);skip(1)
        block('control_curves',start)
    start=at
    for _ in range(take('i')):skip(take('i')*4);skip(5)
    block('control_faces',start);sections={}
    for name in ('stations','buttocks','waterlines','diagonals'):
        if name=='diagonals' and version<9:continue
        start=at;items=[]
        for _ in range(take('i')):
            kind=take('i');curvature=take('B') if version>=11 else 0;plane=list(take('ffff'));built=take('B');curves=[]
            for _ in range(take('i')):
                n=take('i');begin=at;skip(n*(9 if version>=6 and kind in (1,2,3) else 13));curves.append(data[begin:at])
            items.append(dict(kind=kind,curvature=curvature,plane=plane,built=built,curves=curves))
        sections[name]=items;block(name,start)
    markers=[]
    if version>=11:
        start=at
        for _ in range(take('i')):
            attrs=[take('B'),take('B') if version>=21 else 0,take('B'),take('f')];p=[];knuckles=[]
            for _ in range(take('i')):p.append(list(take('fff')));knuckles.append(take('B'))
            markers.append(dict(attrs=attrs,points=p,knuckles=knuckles))
        block('markers',start)
    calculations={};backgrounds=[];flows=[]
    for index,(name,count,flags) in enumerate(affine.RECORDS):
        if index==2 and version>=20:
            start=at
            for _ in range(take('i')):
                view,visible,quality,x,y,scale,blending,transparent,color,tolerance=take('iBiiifiBii')
                backgrounds.append(dict(view=view,visible=visible,quality=quality,origin=[x,y],scale=scale,blending=blending,transparent=transparent,color=color,tolerance=tolerance,image=image()))
            block('background_images',start);start=at
            for _ in range(take('i')):
                x,y,view,built,selected=take('ffiBB');p=[];knuckles=[]
                for _ in range(take('i')):p.append(list(take('fff')));knuckles.append(take('B'))
                flows.append(dict(seed=[x,y],view=view,built=built,selected=selected,points=p,knuckles=knuckles))
            block('flowlines',start)
        if version>=RECORD_VERSIONS[index]:
            start=at;values=[take('f') for _ in range(count)];begin=at;skip(4);calculations[name]=dict(values=values,flags=data[begin:at]);block(name,start)
    if version>=41:
        for name in ('point_names','edge_names','face_names','curve_names'):
            start=at
            for _ in range(take('i')):skip(4);text()
            block(name,start)
        start=at;skip(take('i')*12);block('linear_constraints',start)
    if version>=42:
        start=at
        for _ in range(take('i')):
            text()
            for _ in range(take('i')):text()
        block('point_groups',start)
    if version>=43:start=at;skip(take('i')*9);block('anchors',start)
    return dict(version=version,precision=precision,settings=settings,hydro=hydro,layers=layers,active=active,points=points,point_attrs=point_attrs,
                sections=sections,markers=markers,calculations=calculations,backgrounds=backgrounds,flows=flows,preview=preview,blocks=blocks,offsets=offsets,spans=spans,tail=data[at:])


def remap_saved_points(observed,expected_points):
    """Normalize only SaveBinary's pointer-sorted point indices, using exact XYZ.

    All connection order, orientation, names, attributes and group membership
    still compare exactly. A changed/duplicate coordinate cannot be matched.
    """
    lookup={tuple(p):i for i,p in enumerate(expected_points)}
    if len(lookup)!=len(expected_points) or len({tuple(p) for p in observed['points']})!=len(expected_points):raise ValueError('Independent unit fixture requires unique exact coordinates')
    mapping=[lookup[tuple(p)] for p in observed['points']]
    result=dict(observed,points=[None]*len(mapping),point_attrs=[None]*len(mapping),blocks=dict(observed['blocks']))
    for old,new in enumerate(mapping):result['points'][new]=observed['points'][old];result['point_attrs'][new]=observed['point_attrs'][old]
    for name,raw in observed['blocks'].items():
        if name not in ('control_edges','control_curves','control_faces','point_names','linear_constraints','point_groups','anchors'):continue
        data=bytearray(raw);at=0
        def integer():
            nonlocal at
            value=struct.unpack_from('<i',data,at)[0];at+=4;return value
        def remap():
            nonlocal at
            old=integer();struct.pack_into('<i',data,at-4,mapping[old])
        def text():
            nonlocal at
            n=integer();value=data[at:at+n];at+=n;return bytes(value)
        count=integer()
        for _ in range(count):
            if name=='control_edges':remap();remap();at+=2
            elif name in ('control_curves','control_faces'):
                for _ in range(integer()):remap()
                at+=5 if name=='control_faces' else 1
            elif name=='point_names':remap();text()
            elif name=='linear_constraints':remap();remap();remap()
            elif name=='anchors':remap();remap();at+=1
        if name=='point_groups':
            # Group references are decimal point indices stored as strings.
            data=bytearray(raw);at=4;out=bytearray(struct.pack('<i',count))
            def write_text(value):out.extend(struct.pack('<i',len(value)));out.extend(value)
            for _ in range(count):
                write_text(text());members=integer();out.extend(struct.pack('<i',members))
                for _ in range(members):write_text(str(mapping[int(text())]).encode('ascii'))
            data=out
        result['blocks'][name]=bytes(data)
    return result


def unit_expectations(r,prefix,before,after,imperial,allow_saved_index_remap=False):
    factor=1/FOOT if imperial else FOOT;single=f32(factor);density=DENSITY if imperial else 1/DENSITY;mass=MASS if imperial else 1/MASS;thickness=1/25.4 if imperial else 25.4
    def case(name,actual,expected):
        details={} if actual==expected else json.loads(json.dumps(dict(actual=actual,expected=expected),default=lambda v:v.hex() if isinstance(v,bytes) else str(v)))
        r.case(prefix+'.'+name,actual==expected,**details)
    expected_points=[[f32(v*single) for v in p] for p in before['points']]
    if allow_saved_index_remap:
        try:after=remap_saved_points(after,expected_points)
        except (KeyError,ValueError) as error:r.case(prefix+'.exact-point-index-map',False,reason=str(error))
    case('point-coordinates',after['points'],expected_points);case('point-flags',after['point_attrs'],before['point_attrs'])
    a,b=before['settings'],after['settings']
    for key in ('length','beam','draft'):
        next=f32(abs(a[key]*single));expected=next if abs(f32(next-a[key]))>1e-7 else a[key]
        case(key,b[key],expected)
    case('water-density',b['water_density'],f32(a['water_density']*density))
    if 'split' in a:case('stored-split',b['split'],f32(a['split']*(factor if a['default_split'] else single)))
    changed={'length','beam','draft','water_density','units','particulars','split'}
    case('other-project-values',{k:v for k,v in b.items() if k not in changed},{k:v for k,v in a.items() if k not in changed})
    case('preview-jpeg',after['preview'],before['preview']);case('active-layer',after['active'],before['active'])
    for index,(a,b) in enumerate(zip(before['layers'],after['layers'])):
        expected=dict(a,density=f32(a['density']*density),thickness=f32(a['thickness']*thickness));case('layer-'+str(index),b,expected)
    for key,rows in before['sections'].items():
        expected=[dict(row,plane=row['plane'][:3]+[f32(row['plane'][3]*factor)],built=0,curves=[]) for row in rows]
        # Persisted derived caches may be rebuilt by a snapshot; physical planes
        # and all attributes have independent expectations, regardless of cache.
        for index,(a,b) in enumerate(zip(expected,after['sections'][key])):case(key+'-'+str(index),{k:v for k,v in b.items() if k not in ('built','curves')},{k:v for k,v in a.items() if k not in ('built','curves')})
    case('markers',after['markers'],[dict(row,points=[[f32(v*single) for v in p] for p in row['points']]) for row in before['markers']])
    for name,record in before['calculations'].items():
        values=affine.expected_calculation(name,record['values'],[single]*3)
        if name in ('kaper','planing'):values[4]=f32(record['values'][4]*mass)
        case('calculation-'+name,after['calculations'][name],dict(record,values=values))
    if before['hydro']:
        expected=dict(before['hydro'])
        for key in ('drafts','trims'):expected[key]=[f32(v*factor) for v in expected[key]]
        for key in ('displacements','ranges'):expected[key]=[f32(v*mass) for v in expected[key]]
        expected['vcg']=f32(expected['vcg']*factor);case('hydro-ranges',after['hydro'],expected)
    case('backgrounds',after['backgrounds'],[dict(row,scale=f32(row['scale']*factor)) for row in before['backgrounds']])
    case('flow-seeds',[(row['seed'],row['view'],row['selected']) for row in after['flows']],
         [([f32(v*factor) for v in row['seed']],row['view'],row['selected']) for row in before['flows']])
    for name in ('visibility','control_edges','control_curves','control_faces','point_names','edge_names','face_names','curve_names','linear_constraints','point_groups','anchors'):
        if name in before['blocks']:case('unrelated-'+name,after['blocks'][name],before['blocks'][name])


def probe(r,runtime,out,executable=None):
    out.mkdir(parents=True,exist_ok=True);exe=executable or runtime/'native_project_edit_probe.exe'
    if not exe.is_file():r.case('project.probe',status='not_run',reason='native_project_edit_probe.exe missing');return
    env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','')
    step=0
    def invoke(path,changes,*,bindings=None,minimum_only=False):
        nonlocal step
        step+=1;request=out/f'{step:03}-request.json';target=out/f'{step:03}-result.fbm'
        values=dict(fbm_encoding='cp1251',default_underwater_color=0x800000,resolved_colors={'-2147483633':0xabcdef})
        if bindings:values.update(bindings)
        request.write_text(json.dumps(dict(request=dict(method='update_project',changes=changes),bindings=values,minimum_only=minimum_only),ensure_ascii=False),'utf8')
        p=subprocess.run([str(exe),str(path),str(request),str(target)],env=env,capture_output=True,timeout=30)
        (out/f'{step:03}-probe.log').write_bytes(p.stdout+p.stderr)
        if p.returncode:raise RuntimeError('Project probe process failed: '+str(p.returncode))
        result=json.loads(p.stdout);return result,target
    for version in (0,1,2,5,6,7,9,11,12,13,15,16,18,20,21,22,23,30,40,41,42,43,44):
        path=out/f'version-{version}.fbm';before=fixture(path,version=version,rich=version>=20)
        result,target=invoke(path,{})
        r.case(f'project.probe.v{version}.noop',result.get('ok') and not result['changed'] and result['byte_identical'] and result['minimum_version']==version,actual=result)
        for changes,minimum in [({'name':'Changed'},max(version,2)),({'split_section_location':-2},max(version,6)),({'simplify_intersections':False},max(version,18)),({'precision':2},max(version,2)),({'units':'imperial'},max(version,2))]:
            result,_=invoke(path,changes,minimum_only=True);r.case(f'project.probe.v{version}.minimum-'+next(iter(changes)),result.get('ok') and result['minimum_version']==minimum,actual=result)
        result,_=invoke(path,{'name':'Must not commit','length':-1},minimum_only=True);r.case(f'project.probe.v{version}.invalid-before-promotion',not result['ok'])
        if version>=2:
            result,target=invoke(path,{'units':'imperial'})
            r.case(f'project.probe.v{version}.convert',result.get('ok') and result['changed'] and result['bindings_unchanged'],actual=result)
            if result['ok']:unit_expectations(r,f'project.probe.v{version}.imperial',before,observe(target),True)
    path=out/'rich.fbm';before=fixture(path,relations=True)
    result,target=invoke(path,{'units':'imperial'});after=observe(target);unit_expectations(r,'project.probe.rich.imperial',before,after,True)
    result,back=invoke(target,{'units':'metric'});unit_expectations(r,'project.probe.rich.metric',after,observe(back),False)
    r.case('project.probe.constraints-locks-preserved',after['point_attrs']==before['point_attrs'] and all(after['blocks'][k]==before['blocks'][k] for k in ('anchors','linear_constraints','point_groups')))
    hidden=out/'hidden.fbm';before=fixture(hidden,default_split=True);result,target=invoke(hidden,{'units':'imperial'});unit_expectations(r,'project.probe.hidden',before,observe(target),True)
    # Known raw system color resolves to an RGB that the request may repeat.
    raw=bytearray(path.read_bytes());struct.pack_into('<i',raw,observe(path)['offsets']['underwater_color'],-2147483633);system=out/'system.fbm';system.write_bytes(raw)
    result,target=invoke(system,{'underwater_color':0xabcdef});r.case('project.probe.system-color-noop',result.get('ok') and not result['changed'] and result['byte_identical'],actual=result)
    result,target=invoke(system,{'underwater_color':0x112233});r.case('project.probe.system-color-replace',result.get('ok') and result['changed'] and observe(target)['settings']['underwater_color']==0x112233)
    tail=out/'opaque.fbm';tail.write_bytes(path.read_bytes()+b'unknown-extension')
    result,_=invoke(tail,{});r.case('project.probe.opaque-noop',result.get('ok') and result['byte_identical'])
    result,target=invoke(tail,{'name':'Changed'});r.case('project.probe.opaque-rejection',not result['ok'] and not target.exists())
    result,target=invoke(path,{'name':'A\0B'});r.case('project.probe.nul-rejection',not result['ok'] and not target.exists())
    encoding_source=out/'empty-name.fbm';raw=path.read_bytes();at=observe(path)['offsets']['name'];size=struct.unpack_from('<i',raw,at)[0]
    encoding_source.write_bytes(raw[:at]+struct.pack('<i',0)+raw[at+4+size:])
    for encoding,text in [('cp1251','Проект'),('cp1252','École'),('utf8','项目 🚢'),('ucs2le','项目')]:
        result,target=invoke(encoding_source,{'name':text},bindings={'fbm_encoding':encoding})
        r.case('project.probe.encoding-'+encoding,result.get('ok'),actual=result)
        if result.get('ok'):
            result,second=invoke(target,{'name':text},bindings={'fbm_encoding':encoding});r.case('project.probe.encoding-'+encoding+'-noop',result.get('ok') and not result['changed'] and result['byte_identical'])
    # Cached station and flow geometry are unrelated to text/material edits.
    # Draft invalidates flow geometry; simplify invalidates intersections;
    # precision invalidates both. Combining these must remain one transaction.
    original=observe(path);raw=path.read_bytes();patches={}
    for name in ('stations','buttocks','waterlines','diagonals'):
        block=original['blocks'][name];width='ffB' if name!='diagonals' else 'fffB'
        point=struct.pack('<'+width,*(([1,2] if name!='diagonals' else [1,2,3])+[1]))
        patches[name]=block[:25]+struct.pack('<Bii',1,1,2)+point*2
    block=original['blocks']['flowlines'];patches['flowlines']=block[:16]+struct.pack('<BBi',1,1,2)+struct.pack('<fffB',1,2,3,1)*2
    cached=out/'cached.fbm';parts=[];at=0
    for name,(offset,size) in sorted(original['spans'].items(),key=lambda item:item[1][0]):
        if name not in patches:continue
        parts+=[raw[at:offset],patches[name]];at=offset+size
    cached.write_bytes(b''.join(parts)+raw[at:]);before=observe(cached)
    for label,changes,sections,flows in [('text',{'comment':'New comment'},False,False),('density',{'water_density':1.02},False,False),('draft',{'draft':1.5},False,True),('simplify',{'simplify_intersections':False},True,False),('precision',{'precision':2},True,True),('combined',{'draft':1.5,'precision':2,'simplify_intersections':False},True,True)]:
        result,target=invoke(cached,changes);r.case('project.probe.cache-'+label+'.valid',result.get('ok'),actual=result)
        if not result.get('ok'):continue
        after=observe(target)
        r.case('project.probe.cache-'+label+'.sections',all((not rows[0]['built'] and not rows[0]['curves']) if sections else after['blocks'][name]==before['blocks'][name] for name,rows in after['sections'].items()))
        r.case('project.probe.cache-'+label+'.flows',(not after['flows'][0]['built'] and not after['flows'][0]['points']) if flows else after['blocks']['flowlines']==before['blocks']['flowlines'])
    for version in (0,1):
        path=out/f'version-{version}.fbm'
        for label,changes in [('defaults',dict(name='',designer='',comment='',units='metric',length=1,beam=1,draft=1,water_density=1.025,appendage_coefficient=1,use_default_split_section_location=True,split_section_location=0,simplify_intersections=True,shade_underwater=True,underwater_color=0x800000,precision=1)),('tiny',{'length':1+1e-12})]:
            result,target=invoke(path,changes);r.case(f'project.probe.v{version}.{label}-no-upgrade',result.get('ok') and not result['changed'] and result['minimum_version']==version,actual=result)
    zero=out/'zero-scalars.fbm';fixture(zero);values=observe(zero);raw=bytearray(zero.read_bytes())
    for key in ('length','beam','draft','water_density','appendage_coefficient'):struct.pack_into('<f',raw,values['offsets'][key],0)
    zero.write_bytes(raw)
    for key,epsilon in [('length',1e-7),('beam',1e-7),('draft',1e-7),('appendage_coefficient',1e-7),('water_density',1e-6)]:
        for factor in (.9,1,1.1):
            value=f32(epsilon*factor);changed=value>epsilon;result,target=invoke(zero,{key:epsilon*factor})
            r.case(f'project.probe.zero-threshold-{key}-{factor}',result.get('ok') and result['changed']==changed and observe(target)['settings'][key]==(value if changed else 0),actual=result)


def math_constants(r,out):
    """Retained original-language constant evaluator; requires exact binary64."""
    out.mkdir(parents=True,exist_ok=True)
    fpc=q.common.junction_for(q.common.DEFAULT_LAZARUS,out)/'fpc/3.2.2';compiler=fpc/'bin/x86_64-win64/fpc.exe'
    if not compiler.is_file():r.case('project.unit-constants',status='not_run',reason='FPC 3.2.2 not installed');return
    source=out/'project_units_math.pas';shutil.copy2(Path(__file__).parent/'qt/project_units_math.pas',source)
    process=subprocess.run([str(compiler),'-n','-O2','-Fu'+str(fpc/'units/x86_64-win64/rtl'),'-Fu'+str(fpc/'units/x86_64-win64/rtl-objpas'),'-FE'+str(out),'-FU'+str(out),str(source)],cwd=out,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
    (out/'build.log').write_bytes(process.stdout+process.stderr)
    if process.returncode:raise RuntimeError('Unit constant probe build failed')
    process=subprocess.run([str(out/'project_units_math.exe')],cwd=out,capture_output=True,timeout=10,creationflags=q.common.FLAGS);(out/'factors.txt').write_bytes(process.stdout)
    if process.returncode:raise RuntimeError('Unit constant probe failed')
    actual=dict(line.split() for line in process.stdout.decode('ascii').splitlines())
    thresholds={key:actual.pop(key) for key in ('threshold_dimension','threshold_density')}
    values=dict(metric_length=FOOT,imperial_length=1/FOOT,imperial_density=DENSITY,metric_density=1/DENSITY,imperial_mass=MASS,metric_mass=1/MASS,metric_thickness=25.4,imperial_thickness=1/25.4)
    r.case('project.unit-constants.exact-binary64',actual=={key:struct.pack('>d',value).hex().upper() for key,value in values.items()},actual=actual)
    r.case('project.setter-thresholds.original-single-comparison',thresholds==dict(threshold_dimension='10',threshold_density='10'),actual=thresholds)


def run(r,runtime,source,out):
    out.mkdir(parents=True);probe(r,runtime,out/'probe');math_constants(r,out/'math');timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='update_project',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                result=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields));results.append((result,json.loads(Path(result['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;prefix=f'project.{step:03}-{label}';ga,gb=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ga==gb,different_fields=[k for k in ga if ga[k]!=gb.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='update_project':
                r.case(prefix+'.native-owner',a.get('edit_engine')=='cpp-project' and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append(dict(edit_ms=a.get('project_edit_ms'),total_ms=a['document_total_ms']))
            current=am;return am
        def load(path=source):return both('load','load',path=str(path),document_id=str(uuid.uuid4()))
        def change(label,**changes):return both(label,changes=changes)
        def reject(label,changes,**extra):
            for tag,service in [('native',native),('oracle',oracle)]:
                state=dict(service.state);result=service.command('update_project',output=out/f'reject-{label}-{tag}',changes=changes,**extra)
                r.case('project.reject-'+label+'.'+tag,not result['ok'] and service.state==state,error=result.get('error'))
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('project.'+label+'.undo',modeling.geometry(undone)==modeling.geometry(before))
            redone=both(label+'-redo','redo');r.case('project.'+label+'.redo',modeling.geometry(redone)==modeling.geometry(after))
        def save(label):return Path(q.require_ok(native.command('save',output=out/(label+'-save')))['saved_file'])
        base=load();patch=dict(name='Native project',designer='Native designer',comment='First line\r\nSecond line',length=12.125,beam=4.25,draft=1.125,water_density=1.0125,appendage_coefficient=1.03125,split_section_location=-2.5,use_default_split_section_location=False,simplify_intersections=False,precision=2,shade_underwater=True,underwater_color=0x234567)
        after=change('all-fields',**patch);r.case('project.all-fields.values',all(after['settings'][key]==(f32(value) if type(value)==float else value) for key,value in patch.items()))
        r.case('project.dimension-only-keeps-points',e.points(after)==e.points(base));history('all-fields',base,after)
        state=dict(native.state);change('all-fields-noop',**patch);r.case('project.noop-history',native.state==state)
        before=both('undo-before-noop','undo');state=dict(native.state);change('empty-noop');r.case('project.noop-keeps-redo',native.state==state and native.state['can_redo']);both('redo-after-noop','redo')
        for label,changes in [('unknown',{'name':'Must rollback','unknown':1}),('negative',{'name':'Must rollback','length':-1}),('huge',{'units':'imperial','draft':1e10}),('units',{'units':'feet'}),('precision',{'precision':4}),('fraction',{'precision':1.5}),('color',{'underwater_color':0x1000000}),('boolean',{'shade_underwater':'true'}),('string-number',{'beam':'2'}),('unencodable',{'name':'中文'})]:reject(label,changes)
        reject('stale',{'length':12},expected_revision=native.state['revision']-1)
        old=dict(native.state);result=native.command('update_project',output=out/'nul-native',changes={'name':'A\0B','length':11})
        r.case('project.nul-native-atomic',not result['ok'] and native.state==old)
        # Original FPC scanner drops U+0000 before its text validator. Preserve
        # the real compatibility difference as evidence rather than weakening it.
        legacy=q.require_ok(oracle.command('update_project',output=out/'nul-oracle',changes={'name':'A\0B'}));legacy=json.loads(Path(legacy['model']).read_text('utf8'))
        r.case('project.legacy-nul-loss-evidence',legacy['settings']['name']=='AB')
        rich=out/'rich-unit-source.fbm';before_raw=fixture(rich,relations=True);base=load(rich)
        after=change('imperial',units='imperial');saved=save('imperial');raw=observe(saved);unit_expectations(r,'project.independent.imperial',before_raw,raw,True,True)
        history('unit-conversion',base,after)
        before_raw=raw;after=change('metric',units='metric');saved=save('metric');raw=observe(saved);unit_expectations(r,'project.independent.metric',before_raw,raw,False,True)
        loaded=load(saved);r.case('project.save-reopen',nla.reopened_geometry(loaded)==nla.reopened_geometry(after))
        after=change('units-with-explicit-targets',units='imperial',length=9.25,beam=2.75,draft=.75,water_density=70.0,split_section_location=-4.25)
        r.case('project.explicit-destination-values',all(after['settings'][k]==v for k,v in dict(length=9.25,beam=2.75,draft=.75,water_density=70.0,split_section_location=-4.25).items()))
        hidden=out/'hidden-unit-source.fbm';fixture(hidden,default_split=True);base=load(hidden)
        after=change('hidden-split',split_section_location=-5);r.case('project.hidden-split-effective-unchanged',after['settings']['split_section_location']==base['settings']['split_section_location'])
        after=change('hidden-unit-conversion',units='imperial');after=change('hidden-split-revealed',use_default_split_section_location=False)
        r.case('project.hidden-split-double-conversion',after['settings']['split_section_location']==f32(-5/FOOT))
        system=out/'system-color-source.fbm';raw=bytearray(rich.read_bytes());struct.pack_into('<i',raw,observe(rich)['offsets']['underwater_color'],-2147483633);system.write_bytes(raw)
        base=load(system);old=dict(native.state);change('resolved-system-color-noop',underwater_color=base['settings']['underwater_color'])
        r.case('project.system-color-noop-history',native.state==old)
        unchanged=save('system-color-noop');r.case('project.system-color-noop-file-exact',unchanged.read_bytes()==system.read_bytes())
        # The old loader cannot load pre-1.65 binary layouts; those remain in
        # the standalone all-version writer tests. Its readable older formats
        # still exercise the real private promotion transaction here.
        for version in (9,16):
            legacy_source=out/f'legacy-v{version}.fbm';fixture(legacy_source,version=version,rich=False);base=load(legacy_source)
            old=dict(native.state);change(f'v{version}-noop',name=base['settings']['name'],simplify_intersections=True)
            r.case(f'project.v{version}.noop-no-promotion',native.state==old and save(f'v{version}-noop').read_bytes()==legacy_source.read_bytes())
            reject(f'v{version}-invalid-before-promotion',dict(simplify_intersections=False,name='Must rollback',draft=-1))
            after=change(f'v{version}-promote-simplify',simplify_intersections=False)
            promoted=save(f'v{version}-promoted');r.case(f'project.v{version}.minimum-promotion',promoted.read_bytes()[13]==18)
            history(f'v{version}-promotion',base,after)
        # Setter thresholds act on the difference of two Singles, not Double
        # input deltas. Test both sides near zero and Single rounding near one.
        base=load();reject('singular-zero-output',dict(length=0,beam=0,draft=0,water_density=0,appendage_coefficient=0))
        restored=both('singular-output-rollback','snapshot');r.case('project.singular-output-model-restored',modeling.geometry(restored)==modeling.geometry(base))
        origin=.125;change('threshold-origin',length=origin,beam=origin,draft=origin,water_density=origin,appendage_coefficient=origin)
        for key,epsilon in [('length',1e-7),('beam',1e-7),('draft',1e-7),('appendage_coefficient',1e-7),('water_density',1e-6)]:
            old=dict(native.state);before=current['settings'][key];after=change('below-threshold-'+key,**{key:origin+epsilon*.9})
            r.case('project.threshold-'+key+'-unchanged',native.state==old and after['settings'][key]==before)
            after=change('above-threshold-'+key,**{key:origin+epsilon*1.1});r.case('project.threshold-'+key+'-single',after['settings'][key]==f32(origin+epsilon*1.1))
        change('rounding-origin',length=1);old=dict(native.state);change('same-single',length=1+1e-12);r.case('project.same-single-noop',native.state==old)
        for path in (q.DEFAULT_MODELS[1],q.DEFAULT_MODELS[2]):
            if not path.is_file():r.case('project.supplied-'+path.stem,status='not_run',reason='Model missing');continue
            load(path);opposite='imperial' if current['settings']['units']=='metric' else 'metric';change('supplied-'+path.stem,units=opposite)
        base=load();state=dict(native.state);blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        response=native.command('update_project',output=blocked,changes={'name':'Must rollback'});r.case('project.publication-rollback',not response['ok'] and native.state==state)
        after=both('after-publication-failure','snapshot');r.case('project.publication-model-restored',modeling.geometry(after)==modeling.geometry(base))
        response=oracle.command('update_project',external_document=True,changes={'name':'Old dispatch forbidden'},output=out/'forbid-old')
        r.case('project.forbid-old-dispatch',not response['ok'] and 'C++' in response.get('error',''))
    r.data['native_project_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);p.add_argument('--probe-only',action='store_true');p.add_argument('--probe',type=Path);a=p.parse_args();r=q.Report()
    try:
        if a.probe_only:probe(r,a.runtime.resolve(),a.output.resolve(),a.probe.resolve() if a.probe else None)
        else:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('project.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2,default=lambda v:v.hex() if isinstance(v,bytes) else str(v)),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
