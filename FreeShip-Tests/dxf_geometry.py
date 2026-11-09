"""Exact DXF geometry comparison; no coordinate tolerance or vertex simplification."""
from collections import Counter


def entities(path):
    """Compare complete entities, allowing only entity order and traversal direction.

    Unlike the older segment counter below, this also retains polyline grouping
    and vertex attributes. Coordinates remain exact strings from the same writer.
    This legacy writer's crease-loop order depends on pointer addresses, so a
    changed object allocation sequence is not a valid byte-for-byte DXF oracle.
    """
    lines=path.read_text(encoding='utf-8').splitlines()
    if len(lines)%2:raise ValueError('Odd DXF line count')
    raw=[];current=[]
    for code,value in zip(lines[::2],lines[1::2]):
        if code=='0' and current:raw.append(tuple(current));current=[]
        current.append((code,value))
    if current:raw.append(tuple(current))
    result=[];i=0
    while i<len(raw):
        entity=raw[i];kind=dict(entity).get('0');i+=1
        if kind=='POLYLINE':
            vertices=[]
            while i<len(raw) and dict(raw[i]).get('0')=='VERTEX':vertices.append(raw[i]);i+=1
            if i>=len(raw) or dict(raw[i]).get('0')!='SEQEND':raise ValueError('Unterminated polyline')
            end=raw[i];i+=1
            # Never normalize arcs, widths, mesh indices or unfamiliar attributes.
            if any(k not in ('0','8','10','20','30') for v in vertices for k,_ in v):
                result.append(('RAW',entity,tuple(vertices),end));continue
            closed=(int(dict(entity).get('70','0'))&1)!=0
            if len(vertices)>1 and vertices[-1]==vertices[0]:
                vertices=vertices[:-1];closed=True
            sequence=tuple(vertices);reverse=tuple(reversed(vertices))
            if closed and sequence:
                candidates=[s[n:]+s[:n] for s in (sequence,reverse) for n in range(len(s))]
                sequence=min(candidates)
            else:sequence=min(sequence,reverse)
            result.append(('POLYLINE',entity,end,closed,sequence))
        else:
            # Ordinary LINE records are already emitted deterministically.
            result.append(('RAW',entity))
    return Counter(result)

def records(path):
    lines=path.read_text(encoding='utf-8').splitlines()
    if len(lines)%2:raise ValueError('Odd DXF line count')
    raw=[]; current=[]
    for code,value in zip(lines[::2],lines[1::2]):
        if code=='0' and current:raw.append(tuple(current));current=[]
        current.append((code,value))
    if current:raw.append(tuple(current))
    result=[];i=0
    def point(entity,base):
        d=dict(entity)
        return tuple(d.get(str(base+k),'0') for k in (0,10,20))
    while i<len(raw):
        entity=raw[i];kind=dict(entity).get('0');i+=1
        if kind=='LINE':
            metadata=tuple((k,v) for k,v in entity if k not in ('10','20','30','11','21','31'))
            result.append(('LINE',metadata,tuple(sorted((point(entity,10),point(entity,11))))))
        elif kind=='POLYLINE':
            vertices=[]
            while i<len(raw) and dict(raw[i]).get('0')=='VERTEX':vertices.append(raw[i]);i+=1
            if i>=len(raw) or dict(raw[i]).get('0')!='SEQEND':raise ValueError('Unterminated polyline')
            end=raw[i];i+=1
            # Preserve unsupported vertex attributes instead of assuming straight segments.
            if any(k not in ('0','8','10','20','30') for v in vertices for k,_ in v):
                result.append(('RAW',entity,tuple(vertices),end));continue
            points=[point(v,10) for v in vertices]
            segments=[tuple(sorted((a,b))) for a,b in zip(points,points[1:])]
            if int(dict(entity).get('70','0'))&1 and len(points)>1 and points[-1]!=points[0]:
                segments.append(tuple(sorted((points[-1],points[0]))))
            result.append(('POLYLINE-HEADER',entity,end))
            result.extend(('POLYLINE-SEGMENT',entity,end,segment) for segment in segments)
        else:result.append(('RAW',entity))
    return Counter(result)
