"""Optional independent OpenCascade IGES consumer, runs in a private Python env.

Usage: <OCP python> cad_occt_oracle.py input.igs result.json
No GUI, source/model mutation or production geometry library is involved.
"""
import json,sys
from pathlib import Path
from OCP.IGESControl import IGESControl_Reader
from OCP.IFSelect import IFSelect_RetDone
from OCP.TopAbs import TopAbs_FACE
from OCP.TopExp import TopExp_Explorer
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps

reader=IGESControl_Reader();status=reader.ReadFile(sys.argv[1]);result={'read_ok':status==IFSelect_RetDone}
if result['read_ok']:
    result['roots']=reader.NbRootsForTransfer();result['transferred']=0;failed=[]
    for i in range(1,result['roots']+1):
        if reader.TransferOneRoot(i):result['transferred']+=1
        else:failed.append(i)
    result['untransferred_roots']=failed
    shape=reader.OneShape();result['null']=shape.IsNull()
    if not shape.IsNull():
        result['valid']=BRepCheck_Analyzer(shape).IsValid();faces=TopExp_Explorer(shape,TopAbs_FACE);count=0
        while faces.More():count+=1;faces.Next()
        result['faces']=count;box=Bnd_Box();BRepBndLib.AddOptimal_s(shape,box,False,False);result['bounds']=list(box.Get())
        props=GProp_GProps();BRepGProp.SurfaceProperties_s(shape,props);result['area_mm2']=props.Mass()
Path(sys.argv[2]).write_text(json.dumps(result,indent=2),encoding='utf8')
