"""Small topology fixtures and independently calculated subdivision examples.

Expected locations use boundary/Loop/Catmull-Clark stencils. They do not run a
second copy of the candidate's face splitting or averaging implementation.
"""
from __future__ import annotations

import copy
from collections import Counter
import struct


def layer(name='Hull',**changes):
    result=dict(symmetric=True,surface_visible=True,control_net_visible=True,
        use_for_intersections=True,use_in_hydrostatics=True,show_in_linesplan=True,
        developable=False,color=0x4287c4,alpha_blend=255,material_density=7.8,
        thickness=.01,name=name,layer_id=1)
    result.update(changes)
    for key in ('material_density','thickness'):
        result[key]=struct.unpack('<f',struct.pack('<f',result[key]))[0]
    return result


def geometry(positions,faces,*,creases=(),corners=(),curves=(),loose_edges=(),layers=None,face_layers=None):
    incidence=Counter(tuple(sorted((face[i-1],face[i]))) for face in faces for i in range(len(face)))
    edges=[];seen=set();curve_index={}
    for ci,curve in enumerate(curves):
        for a,b in zip(curve['points'],curve['points'][1:]):curve_index[tuple(sorted((a,b)))]=ci
    for face in faces:
        for a,b in zip(face,face[1:]+face[:1]):
            key=tuple(sorted((a,b)))
            if key not in seen:edges.append({'points':[a,b]});seen.add(key)
    for curve in curves:
        for a,b in zip(curve['points'],curve['points'][1:]):
            key=tuple(sorted((a,b)))
            if key not in seen:edges.append({'points':[a,b]});seen.add(key)
    for a,b in loose_edges:
        key=tuple(sorted((a,b)))
        if key not in seen:edges.append({'points':[a,b]});seen.add(key)
    sharp={tuple(sorted(e)) for e in creases};sharp_degree=Counter()
    for edge in edges:
        key=tuple(sorted(edge['points']));edge['crease']=key in sharp or incidence[key]<=1
        edge['curve']=curve_index.get(key,-1)
        if edge['crease']:sharp_degree.update(edge['points'])
    points=[]
    for index,position in enumerate(positions):
        degree=sharp_degree[index]
        kind=3 if index in corners or degree>2 else 1 if degree==2 else 2 if degree==1 else 0
        points.append({'position':position,'type':kind})
    return dict(format='FreeShipGeometryInput',version=1,subdivision_mode=0,
        precision=0,desired_level=2,points=points,edges=edges,
        faces=[{'points':face,'layer':(face_layers or [0]*len(faces))[i]} for i,face in enumerate(faces)],
        layers=layers or [layer()],curves=list(curves))


def cases():
    quad=[[0,0,0],[2,0,0],[2,2,0],[0,2,0]]
    triangle=[[0,0,0],[4,0,0],[0,4,0]]
    cube=[[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]]
    cube_faces=[[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]
    cube_edges={tuple(sorted((f[i-1],f[i]))) for f in cube_faces for i in range(4)}
    fan3=[[1,1,1],[0,0,0],[4,0,0],[0,4,0]]
    fan6=[[0,0,1],[4,0,0],[1,3,0],[-3,2,0],[-4,0,0],[-1,-3,0],[3,-2,0]]
    fan6_faces=[[0,i,1+i%6] for i in range(1,7)]
    examples=[
        ('triangle-open',geometry(triangle,[[0,1,2]])),
        ('triangle-corners',geometry(triangle,[[0,1,2]],corners=[0,1,2])),
        ('quad-open',geometry(quad,[[0,1,2,3]])),
        ('quad-corners',geometry(quad,[[0,1,2,3]],corners=range(4))),
        ('quad-reversed',geometry(quad,[[0,3,2,1]])),
        ('coincident-disconnected-quads',geometry(quad+copy.deepcopy(quad),[[0,1,2,3],[4,5,6,7]])),
        ('pentagon-open',geometry([[0,0,0],[5,0,0],[7,5,0],[2,10,0],[-4,5,0]],[[0,1,2,3,4]])),
        ('quad-pair-bent',geometry([[0,0,0],[2,0,0],[2,2,0],[0,2,0],[4,0,2],[4,2,2]],[[0,1,2,3],[1,4,5,2]])),
        ('quad-pair-crease',geometry([[0,0,0],[2,0,0],[2,2,0],[0,2,0],[4,0,2],[4,2,2]],[[0,1,2,3],[1,4,5,2]],creases=[(1,2)])),
        ('mixed-tri-quad',geometry([[0,0,0],[2,0,0],[2,2,1],[0,2,0],[3,1,-1]],[[0,1,2,3],[2,1,4]])),
        ('closed-cube',geometry(cube,cube_faces)),
        ('closed-cube-sharp',geometry(cube,cube_faces,creases=cube_edges)),
        ('closed-cube-dart',geometry(cube,cube_faces,creases=[(0,1)])),
        ('fan-three',geometry(fan3,[[0,1,2],[0,2,3],[0,3,1]])),
        ('fan-six',geometry(fan6,fan6_faces)),
        ('fan-six-crease',geometry(fan6,fan6_faces,creases=[(0,1),(0,4)])),
        ('fan-six-corner',geometry(fan6,fan6_faces,corners=[0])),
        ('quad-grid-peak',geometry([[-2,-2,0],[0,-2,0],[2,-2,0],[-2,0,0],[0,0,1],[2,0,0],[-2,2,0],[0,2,0],[2,2,0]],
            [[0,1,4,3],[1,2,5,4],[3,4,7,6],[4,5,8,7]])),
        ('fan-nine',geometry([[0,0,1],[4,0,0],[3,2,0],[1,4,0],[-2,4,0],[-4,1,0],[-4,-1,0],[-2,-4,0],[1,-4,0],[3,-2,0]],
            [[0,i,1+i%9] for i in range(1,10)])),
        ('attached-curve',geometry(quad,[[0,1,2,3]],curves=[{'points':[0,1,2],'visible':False}])),
        ('free-curve',geometry([[0,0,0],[1,2,0],[3,1,0],[4,0,0]],[],corners=[0,3],curves=[{'points':[0,1,2,3],'visible':True}])),
        ('free-closed-curve',geometry(quad,[],curves=[{'points':[0,1,2,3,0],'visible':True}])),
        ('loose-points-edges',geometry(quad+[[10,20,30],[11,21,31],[99,99,99]],[[0,1,2,3]],loose_edges=[(4,5)])),
        ('layer-metadata',geometry(quad+[[4,0,2],[4,2,2]],[[0,1,2,3],[1,4,5,2]],
            layers=[layer('船体'),layer('隐藏 Киль',symmetric=False,surface_visible=False,control_net_visible=False,
                use_for_intersections=False,use_in_hydrostatics=False,show_in_linesplan=False,
                developable=True,color=-2147483646,alpha_blend=0,material_density=.75,thickness=.125,layer_id=27)],face_layers=[0,1])),
    ]
    shared=geometry([[0,0,0],[1,2,0],[3,0,0],[1,4,1]],[],
        curves=[{'points':[0,1,2],'visible':True},{'points':[1,3],'visible':True}])
    # Keep intentionally unnormalised raw fields. The original free-curve
    # crease setter can affect a shared point before the next curve is copied.
    for point in shared['points']:point['type']=0
    for edge in shared['edges']:edge['crease']=False
    examples.append(('shared-free-curves-raw',shared))
    result=[]
    for name,original in examples:
        for mode in (0,1):
            value=copy.deepcopy(original);value['subdivision_mode']=mode
            levels=([0,1,2,3,4] if name in ('closed-cube','free-curve') else
                    [0,1,2,3] if name.startswith('free-') else [0,1,2])
            result.append({'id':f'{name}-mode-{mode}','example':name,'geometry':value,'levels':levels})
    return {'format':'FreeShipNativeSubdivisionCases','version':1,'cases':result}


def independent_expectations(case,level):
    """Small mathematical consequences, with vertex roles located topologically."""
    name=case.get('example');mode=case['geometry']['subdivision_mode']
    if level==4 and name=='closed-cube':
        return {'point_count':1538,'face_count':1536,'edge_count':3072,'face_arity':4,'boundary_edge_count':0}
    if level==4 and name=='free-curve':return {'curve_point_counts':[25]}
    if level!=1:return None
    if name=='quad-open':
        return {'vertices':{'root0.child0.vertex0':[.25,.25,0],
                            'root0.child0.vertex1':[1,0,0],
                            'root0.child0.vertex2':[1,1,0],
                            'root0.child0.vertex3':[0,1,0]},
                'point_count':9,'face_count':4,'edge_count':12,'face_arity':4,'boundary_edge_count':8,'orientation_sign':1}
    if name=='quad-corners':
        return {'vertices':{'root0.child0.vertex0':[0,0,0],'root0.child0.vertex1':[1,0,0],
                            'root0.child0.vertex2':[1,1,0],'root0.child0.vertex3':[0,1,0]},
                'point_count':9,'face_count':4,'edge_count':12,'face_arity':4,'boundary_edge_count':8}
    if name=='quad-reversed':
        return {'vertices':{'root0.child0.vertex0':[.25,.25,0],'root0.child0.vertex1':[0,1,0],
                            'root0.child0.vertex2':[1,1,0],'root0.child0.vertex3':[1,0,0]},
                'point_count':9,'face_count':4,'edge_count':12,'face_arity':4,'boundary_edge_count':8,'orientation_sign':-1}
    if name=='coincident-disconnected-quads':
        return {'point_count':18,'face_count':8,'edge_count':24,'face_arity':4,'boundary_edge_count':16,
                'vertices':{'root0.child0.vertex0':[.25,.25,0],'root1.child0.vertex0':[.25,.25,0]},'disjoint_roots':True}
    if name=='pentagon-open':
        # Boundary vertex: 3/4 P + 1/8(previous + next). The five original
        # coordinates have arithmetic mean (2,4,0); subdivision preserves it
        # at the face point in real arithmetic, including its later averaging.
        return {'vertices':{'root0.child0.vertex0':[1/8,5/8,0],
                            'root0.child0.vertex1':[2.5,0,0],
                            'root0.child0.vertex2':[2,4,0],
                            'root0.child0.vertex3':[-2,2.5,0]},
                'point_count':11,'face_count':5,'edge_count':15,'face_arity':4,
                'boundary_edge_count':10,'orientation_sign':1}
    if name in ('triangle-open','triangle-corners'):
        corner=[0,0,0] if name.endswith('corners') else [.5,.5,0]
        if mode==0:
            return {'vertices':{'root0.child0.vertex0':[0,2,0],'root0.child0.vertex1':corner,'root0.child0.vertex2':[2,0,0]},
                    'point_count':6,'face_count':4,'edge_count':9,'face_arity':3,'boundary_edge_count':6}
        return {'vertices':{'root0.child0.vertex0':corner,'root0.child0.vertex1':[2,0,0],
                            'root0.child0.vertex2':[4/3,4/3,0],'root0.child0.vertex3':[0,2,0]},
                'point_count':7,'face_count':3,'edge_count':9,'face_arity':4,'boundary_edge_count':6}
    if name in ('fan-three','fan-six') and mode==0:
        expected=[19/16,19/16,7/16] if name=='fan-three' else [0,0,5/8]
        return {'vertices':{'root0.child0.vertex1':expected}}
    if name=='fan-six-corner':
        return {'vertices':{f'root0.child0.vertex{1 if mode==0 else 0}':[0,0,1]}}
    if name=='quad-grid-peak':
        return {'vertices':{'root0.child2.vertex2':[0,0,9/16]}}
    if name=='closed-cube-sharp':
        return {'vertices':{'root0.child0.vertex0':[-1,-1,-1]},'point_count':26,'face_count':24,'edge_count':48,'face_arity':4,'boundary_edge_count':0}
    if name=='closed-cube':
        return {'vertices':{'root0.child0.vertex0':[-5/9,-5/9,-5/9]},'point_count':26,'face_count':24,'edge_count':48,'face_arity':4,'boundary_edge_count':0}
    return None
