"""Independent FBM preview envelope reader for regression evidence.

Offsets follow FreeProjectSettings.SaveBinary / FreeVisibility.SaveBinary.
This helper never writes a user's file; synthetic copies may replace the image.
"""
import struct
HYDRO_FIELDS=('hydro_displacement','hydro_lateral_area','hydro_sectional_areas','hydro_metacentric_height','hydro_lcf')

def envelope(data,default_underwater_alpha=255):
    pos=0
    def skip(n):
        nonlocal pos
        if n<0 or pos+n>len(data):raise ValueError('Truncated FBM')
        pos+=n
    def integer():
        value=struct.unpack_from('<i',data,pos)[0];skip(4);return value
    def string():
        n=integer();start=pos;skip(n);return data[start:pos]
    if string()!=b'FREE!ship':raise ValueError('Not an FBM')
    v=data[pos];skip(1)
    if v>44:raise ValueError('Unsupported FBM version')
    visibility=pos+4
    skip(4+4+10+4)  # Precision, model view, visibility flags, curvature scale.
    if v>=12:skip(1)
    if v>=16:skip(4)
    if v>=17:skip(6)
    if v>=20:skip(1)
    if v>=2:
        string();string();skip(12+1+8+1+4+4)
        if v>=6:skip(5)
        if v>=7:skip(1)
        string();string()
    fields={key:bool(data[visibility+24+i]) if v>=17 else True for i,key in enumerate(HYDRO_FIELDS)}
    base=dict(version=v,hydro_fields=fields,hydro_offsets={key:visibility+24+i for i,key in enumerate(HYDRO_FIELDS)} if v>=17 else {},coefficient_basis=1)
    base['underwater_color_alpha']=default_underwater_alpha
    base['generate_auxiliary_reports']=False
    if v<16:return dict(base,enabled=False,supported=False)
    base.update(coefficient_offset=pos,coefficient_basis=integer())
    flag=pos;enabled=bool(data[pos]);skip(1)
    result=dict(base,supported=True,enabled=enabled,flag=flag,begin=pos,end=pos,jpeg=b'')
    if enabled:
        w,h,n=struct.unpack_from('<iii',data,pos);skip(12);start=pos;skip(n)
        result.update(width=w,height=h,jpeg=data[start:pos],end=pos)
    if v>=18:skip(1)
    if v>=20:
        skip(16);skip(integer()*4);skip(12+1);skip(integer()*4);skip(integer()*4);skip(1+4)
    if v>=37:skip(1)
    if v>=39:
        result.update(auxiliary_reports_offset=pos,generate_auxiliary_reports=bool(data[pos]));skip(1)
    if v>=44:result.update(underwater_alpha_offset=pos,underwater_color_alpha=integer())
    return result

def replace(data,jpeg=None,enabled=True):
    info=envelope(data)
    if not info['supported']:return data
    body=struct.pack('<iii',400,300,len(jpeg))+jpeg if enabled else b''
    return data[:info['flag']]+bytes([enabled])+body+data[info['end']:]

def without_image(data):
    """Comparison only: retain every byte except the preview envelope/payload."""
    info=envelope(data)
    return data[:info['begin']]+data[info['end']:] if info['enabled'] else data
