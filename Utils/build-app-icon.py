"""Build a standalone Win32 .res from the existing application ICO (no GCC dependency)."""
from pathlib import Path
import struct
root=Path(__file__).resolve().parent.parent
ico=(root/'FreeShip.ico').read_bytes();count=struct.unpack_from('<H',ico,4)[0]
def ordinal(n):return struct.pack('<HH',65535,n)
def entry(t,name,data):
    h=ordinal(t)+(ordinal(name) if isinstance(name,int) else name.encode('utf-16le')+b'\0\0')
    h+=b'\0'*((-len(h))%4);h+=struct.pack('<IHHII',0,0x1030 if t else 0,0,0,0)
    return struct.pack('<II',len(data),len(h)+8)+h+data+b'\0'*((-len(data))%4)
b=entry(0,0,b'');group=ico[:6]
for i in range(count):
    e=ico[6+16*i:22+16*i];size,offset=struct.unpack_from('<II',e,8)
    b+=entry(3,i+1,ico[offset:offset+size]);group+=e[:12]+struct.pack('<H',i+1)
b+=entry(14,'MAINICON',group)+entry(14,'APPICON26',group);(root/'AppIcon26.res').write_bytes(b)
