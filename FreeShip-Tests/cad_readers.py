"""Independent CAD readers/checks; never import production serialization code.

IGES record/128 layout: NIST NBSIR 88-3813, sections 2 and 3.17.
DXF entities are read by ezdxf, then compared to complete source geometry.
"""
from pathlib import Path


def tokens(text,width):
    result=[];i=0
    while i<len(text):
        while i<len(text) and text[i].isspace():i+=1
        j=i
        while j<len(text) and text[j].isdigit():j+=1
        if j>i and j<len(text) and text[j]=='H':
            length=int(text[i:j]);i=j+1;result.append(text[i:i+length]);i+=length
            assert i<=len(text)
        else:
            j=i
            while j<len(text) and text[j] not in ',;':j+=1
            assert i//width==(j-1)//width, 'Numeric IGES parameter split across records'
            result.append(text[i:j].strip());i=j
        assert i<len(text) and text[i] in ',;'
        end=text[i]==';';i+=1
        if end:
            assert not text[i:].strip();return result
    raise ValueError('Unterminated IGES parameter data')


def iges(path):
    lines=Path(path).read_text('ascii').splitlines();sections={k:[] for k in 'SGDPT'}
    order=[]
    for line in lines:
        assert len(line)==80
        key=line[72];assert key in sections
        assert int(line[73:])==len(sections[key])+1
        sections[key].append(line[:72]);order.append('SGDPT'.index(key))
    assert order==sorted(order) and len(sections['T'])==1
    end=sections['T'][0]
    for i,k in enumerate('SGDP'):
        assert end[i*8]==k and int(end[i*8+1:i*8+8])==len(sections[k])
    glob=tokens(''.join(sections['G']),72);assert glob[:2]==[',',';'] and float(glob[12])==1
    ds=sections['D'];ps=sections['P'];assert len(ds)%2==0
    entities=[];used=set()
    for i in range(0,len(ds),2):
        first=[ds[i][j:j+8] for j in range(0,72,8)];second=[ds[i+1][j:j+8] for j in range(0,72,8)]
        kind=int(first[0]);assert kind==int(second[0]);start=int(first[1])-1;count=int(second[3]);assert count>0
        raw=[]
        for j in range(start,start+count):
            assert j not in used;used.add(j);assert int(ps[j][64:])==i+1;raw.append(ps[j][:64])
        values=tokens(''.join(raw),64);assert int(values[0])==kind
        data={'type':kind,'de':i+1,'level':int(first[4]),'color':int(second[2]),'values':values}
        if kind==128:
            cols,rows,du,dv=[int(x) for x in values[1:5]];cols+=1;rows+=1
            assert values[5:10]==['0','0','1','0','0'];a=10;b=a+cols+du+1;c=b+rows+dv+1;d=c+cols*rows
            u=list(map(float,values[a:b]));v=list(map(float,values[b:c]));weights=list(map(float,values[c:d]))
            assert u==sorted(u) and v==sorted(v) and all(w==1 for w in weights)
            points=[tuple(map(float,values[j:j+3])) for j in range(d,d+3*cols*rows,3)];domain=list(map(float,values[d+3*cols*rows:]))
            assert domain==[u[du],u[cols],v[dv],v[rows]]
            data.update(cols=cols,rows=rows,degree_u=du,degree_v=dv,u=u,v=v,points=points,domain=domain)
        entities.append(data)
    assert used==set(range(len(ps)))
    colors={e['de'] for e in entities if e['type']==314}
    assert all(-e['color'] in colors for e in entities if e['type']==128)
    return {'global':glob,'entities':entities,'patches':[e for e in entities if e['type']==128]}


def offsets(path):
    curves=[]
    for line in Path(path).read_text('utf8').splitlines():
        if line.startswith('#'):continue
        if line.startswith(('Section_','ControlCurve_')):curves.append({'name':line,'points':[],'knuckles':[]});continue
        row=line.split();assert len(row) in (3,4) and (len(row)==3 or row[3]=='KNUCKLE')
        curves[-1]['points'].append(tuple(map(float,row[:3])));curves[-1]['knuckles'].append(len(row)==4)
    return curves
