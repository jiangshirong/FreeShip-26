"""Independent reader for the single HULL / HULL.C GF subset, not a GHS engine.

Record structure follows the upstream interchange contract. Polygon orientation,
units, area, centroid and volume expectations are checked by qt_ghs separately.
"""
import math
from pathlib import Path

def read(path):
    lines=Path(path).read_text('ascii').splitlines();i=0
    def line():
        nonlocal i
        if i>=len(lines):raise ValueError('Unexpected end of GF')
        s=lines[i];i+=1;return s
    def require(text):
        actual=line()
        if actual!=text:raise ValueError(f'Expected {text!r}, got {actual!r}, line {i}')
    def nums(s,count):
        v=list(map(float,s.split(',')))
        if len(v)!=count or not all(math.isfinite(x) for x in v):raise ValueError('Invalid numeric record')
        return v
    title=line();header={}
    while (s:=line())!='*':
        if s.startswith('\\'):continue
        key,value=s.split(':',1);header[key]=value
    require('HULL');count=int(line());stations=[]
    if count<2:raise ValueError('Insufficient stations')
    for _ in range(count):
        x,n=nums(line(),2)
        if n<3 or int(n)!=n:raise ValueError('Invalid point count')
        points=[nums(line(),2) for _ in range(int(n))]
        if any(a==b for a,b in zip(points,points[1:]+points[:1])):raise ValueError('Repeated endpoint')
        a=0;yc=0;zc=0
        for (y,z),(v,w) in zip(points,points[1:]+points[:1]):
            c=y*w-v*z;a+=c;yc+=(y+v)*c;zc+=(z+w)*c
        if a<=0:raise ValueError('Not a positive CCW polygon')
        stations.append(dict(x=x,points=points,area=a/2,centroid=[yc/(3*a),zc/(3*a)]))
    require('0,0,0');require('**');require('HULL.C');require('0');require('1.0');require('0,0,0');require('HULL')
    require('***');require('HULL');require('WATER');require('1');density=nums(line(),1)[0]
    require('0,0,0');require('1');require('HULL.C');require('****')
    if i!=len(lines):raise ValueError('Trailing records')
    if header['P'] not in ('M','F'):raise ValueError('Unknown units')
    if any(b['x']<=a['x'] for a,b in zip(stations,stations[1:])):raise ValueError('Unordered stations')
    return dict(title=title,header=header,stations=stations,density=density)
