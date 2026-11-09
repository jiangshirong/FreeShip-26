"""Small, deterministic native-geometry inputs and independent exact examples.

The rational cubic below is a closed-form natural cubic through three equally
spaced points; it is not a second copy of the implementation's tridiagonal solve.
"""
from __future__ import annotations

import random
import struct


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def neighbors32(value):
    """The exact float32 predecessor/successor, used to query both knot sides."""
    value=f32(value)
    if value==0:return [-struct.unpack('<f',struct.pack('<I',1))[0],0.0,struct.unpack('<f',struct.pack('<I',1))[0]]
    raw=struct.unpack('<I',struct.pack('<f',value))[0]
    return [struct.unpack('<f',struct.pack('<I',raw+offset))[0] for offset in (-1,0,1)]


def cases():
    result=[]
    queries=[[0,0,0],[1,2,3],[-2,3,-4],[0,0,2],[0,0,-2]]
    boxes=[{'min':[-1,-1,-1],'max':[1,1,3]},
           {'min':[0,0,3],'max':[2,2,4]},
           {'min':[0,0,-4],'max':[2,2,-3]},
           {'min':[0,0,2],'max':[2,2,2]}]
    lines=[{'point':[1,2,8],'direction':[0,0,-2]},
           {'point':[1,2,2],'direction':[1,0,0]},
           {'point':[1,2,-8],'direction':[1,0,0]},
           {'point':[1,2,8],'direction':[1,0,0]},
           {'point':[1,2,8],'direction':[0,0,0]}]
    for name,definition in [
        ('axis',{'coefficients':[0,0,1,-2]}),
        ('axis-scaled',{'coefficients':[0,0,4,-8]}),
        ('axis-negative-scaled',{'coefficients':[0,0,-2,4]}),
        ('point-normal',{'point':[0,0,2],'normal':[0,0,4]}),
        ('three-points',{'points':[[0,0,2],[2,0,2],[0,3,2]]}),
        ('oblique',{'coefficients':[2,-3,4,-5]}),
        ('diagonal-exact',{'coefficients':[1,1,0,-2]}),
        ('oblique-three-points',{'points':[[1,0,1],[4,1,2],[1,2,3]]}),
        ('zero-normal',{'coefficients':[0,0,0,1],'policy':'invalid-plane'}),
        ('collinear',{'points':[[0,0,0],[1,2,3],[2,4,6]],'policy':'invalid-plane'}),
    ]:
        result.append(dict(id='plane-'+name,kind='plane',query_points=queries,
                           boxes=boxes,lines=lines,**definition))

    ts=[i/8 for i in range(9)]
    def spline(name,points,**kwargs):
        value={'id':'spline-'+name,'kind':'spline','points':points,
               'knuckles':[False]*len(points),'fragments':32,'parameters':ts,
               'planes':[[1,0,0,-.5],[0,1,0,-.5],[0,0,1,1],[0,0,1,0]]}
        value.update(kwargs);result.append(value)
    spline('linear',[[0,0,0],[2,4,-2]])
    spline('natural-v',[[0,0,0],[1,1,0],[2,0,0]])
    spline('knuckle-v',[[0,0,0],[1,1,0],[2,0,0]],knuckles=[False,True,False])
    spline('all-knuckles',[[0,0,0],[1,1,0],[2,0,0]],knuckles=[True,True,True])
    spline('unequal-chords',[[0,0,0],[1,0,0],[1,4,0]],parameters=[0,.1,.2,.25,.5,.75,1])
    spline('repeated-start',[[0,0,0],[0,0,0],[1,1,0],[2,0,0]])
    spline('repeated-middle',[[0,0,0],[1,1,0],[1,1,0],[2,0,0]])
    spline('zero-length',[[2,3,4],[2,3,4],[2,3,4]],planes=[[1,0,0,-2],[1,0,0,5]])
    spline('empty',[],planes=[[0,0,1,0]],policy='short-spline')
    spline('single',[[9,8,7]],planes=[[0,0,1,0]],policy='short-spline')
    spline('outside',[[0,0,0],[1,1,0],[2,0,0]],parameters=[-.25,-.125,0,.5,1,1.125,1.25])
    spline('fragments-zero',[[0,0,0],[1,1,0],[2,0,0]],fragments=0,policy='zero-fragment-cut')
    spline('invalid-plane-cut',[[0,0,0],[1,1,0],[2,0,0]],planes=[[0,0,0,0]],policy='invalid-plane-cut')
    spline('tiny-first-chord',[[0,0,0],[0.0000001,0,0],[1,1,0]],parameters=[0,1e-8,1e-7,1e-6,.125,.5,1])
    spline('small-first-chord',[[0,0,0],[0.000005,0,0],[1,1,0]],parameters=[0,1e-7,1e-6,1e-5,.125,.5,1])
    spline('end-knuckles',[[0,0,0],[1,1,0],[2,0,0]],knuckles=[True,False,True])
    spline('consecutive-knuckles',[[0,0,0],[1,1,0],[3,-1,0],[4,1,0]],knuckles=[False,True,True,False])
    spline('mutations',[[0,0,0],[1,1,0],[2,0,0]],mutations=[
        {'op':'set_point','index':1,'point':[1,2,0]},
        {'op':'set_knuckle','index':1,'value':True},
        {'op':'set_fragments','value':8},
        {'op':'set_point','index':1,'point':[1,1,0]}])
    rng=random.Random(2601)
    for index in range(72):
        scale=(0.001,1,1000)[index%3] if index>=6 else 1
        count=3+index if index<6 else 3+index%10
        pts=[[f32(scale*(i*.375+rng.random()*.1)),f32(scale*rng.uniform(-2,2)),f32(scale*rng.uniform(-1,1))]
             for i in range(count)]
        spline('seeded-'+str(index),pts,knuckles=[i>0 and i%3==0 for i in range(len(pts))],
               parameters=[-.125,0,*neighbors32(.001),.03125,.25,.499,.5,.501,.75,.999,1,1.125],fragments=50,
               planes=[[1,0,0,-scale*.5],[0,1,0,-scale*.5],[0,0,1,0]])
    for index in range(18):
        scale=(0.01,1,100)[index%3]
        normal=[f32(scale*rng.uniform(-2,2)) for _ in range(3)]
        point=[f32(rng.uniform(-5,5)) for _ in range(3)]
        definition=({'point':point,'normal':normal} if index%2==0 else
                    {'points':[[f32(rng.uniform(-5,5)) for _ in range(3)] for _ in range(3)]})
        result.append(dict(id='plane-seeded-'+str(index),kind='plane',query_points=queries,boxes=boxes,lines=lines,**definition))
    return {'format':'FreeShipNativeGeometryCases','version':1,'cases':result}


def analytic_examples(input_case):
    """Return independently specified expected fields for simple exact cases."""
    name=input_case['id']
    if name in ('plane-axis','plane-axis-scaled','plane-axis-negative-scaled','plane-point-normal','plane-three-points'):
        return {'projections':[[p[0],p[1],2] for p in input_case['query_points']],
                'mirrors':[[p[0],p[1],4-p[2]] for p in input_case['query_points']],
                'boxes':[True,False,False,True],
                'line_relations':['point','coplanar','parallel','parallel','invalid'],
                'first_line_point':[1,2,2],'first_line_parameter':3}
    if name=='plane-diagonal-exact':
        return {'evaluations':[p[0]+p[1]-2 for p in input_case['query_points']],
                'projections':[[(p[0]-p[1]+2)/2,(p[1]-p[0]+2)/2,p[2]] for p in input_case['query_points']],
                'mirrors':[[2-p[1],2-p[0],p[2]] for p in input_case['query_points']]}
    ts=input_case.get('analytic_parameters',input_case.get('parameters',[]))
    if name=='spline-linear':
        return {'knots':[0,1], 'values':[[2*t,4*t,-2*t] for t in ts],
                'first':[[2,4,-2] for _ in ts],
                'second':[[0,0,0] for _ in ts]}
    if name=='spline-natural-v':
        return {'knots':[0,.5,1],
                'values':[[2*t,3*min(t,1-t)-4*min(t,1-t)**3,0] for t in ts],
                'second':[[0,-24*min(t,1-t),0] for t in ts]}
    if name in ('spline-knuckle-v','spline-all-knuckles'):
        return {'knots':[0,.5,1], 'values':[[2*t,2*min(t,1-t),0] for t in ts],
                'second':[[0,0,0] for _ in ts]}
    if name=='spline-unequal-chords':
        return {'knots':[0,f32(1/5),1], 'total_length':5}
    if name=='spline-zero-length':
        return {'knots':[0,.5,1],'values':[[2,3,4] for _ in ts],
                'first':[[0,0,0] for _ in ts],'second':[[0,0,0] for _ in ts],'total_length':0}
    if name in ('spline-empty','spline-single'):
        return {'values':[[0,0,0] for _ in ts], 'first':[[0,0,0] for _ in ts],
                'second':[[0,0,0] for _ in ts]}
    return {}
