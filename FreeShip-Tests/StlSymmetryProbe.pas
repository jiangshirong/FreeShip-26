unit StlSymmetryProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunStlSymmetryProbe(Form: TMainForm);
implementation
uses Classes, SysUtils, fpjson, Windows, FreeShipUnit, FreeGeometry, FreeTypes;

function Vec(X,Y,Z: Double): T3DCoordinate;
begin Result.X:=X; Result.Y:=Y; Result.Z:=Z end;

procedure Triangle(Ship:TFreeShip; Layer:TFreeSubdivisionLayer; A,B,C:T3DCoordinate);
var P:TFasterListTFreeSubdivisionControlPoint;
begin
  P:=TFasterListTFreeSubdivisionControlPoint.Create;
  try
    P.Add(Ship.Surface.AddControlPoint(A));
    P.Add(Ship.Surface.AddControlPoint(B));
    P.Add(Ship.Surface.AddControlPoint(C));
    if Ship.Surface.AddControlFace(P,True,Layer)=nil then
      raise Exception.Create('STL fixture face was rejected');
  finally P.Free end;
end;

function NewLayer(Ship:TFreeShip; const Name:string; Symmetric,Visible:Boolean):TFreeSubdivisionLayer;
begin
  Result:=Ship.Surface.AddNewLayer;
  Result.Name:=Name; Result.Symmetric:=Symmetric; Result.SurfaceVisible:=Visible;
end;

function CoordinateJSON(C:T3DCoordinate):TJSONArray;
begin Result:=TJSONArray.Create; Result.Add(C.X);Result.Add(C.Y);Result.Add(C.Z) end;

procedure CaptureAndExport(Ship:TFreeShip; Cases:TJSONArray; const Name,Folder:string);
var C,L,F:TJSONObject; Layers,Faces,Vertices:TJSONArray; I,J,K,N:Integer;
  Layer:TFreeSubdivisionLayer; Child:TFreeSubdivisionFace;
begin
  Ship.Surface.DesiredSubdivisionLevel:=1;
  Ship.Surface.Rebuild;
  C:=TJSONObject.Create; C.Add('name',Name); Layers:=TJSONArray.Create; C.Add('layers',Layers);
  for I:=0 to Ship.Surface.NumberOfLayers-1 do begin
    Layer:=Ship.Layer[I]; L:=TJSONObject.Create; Layers.Add(L);
    L.Add('name',Layer.Name);L.Add('symmetric',Layer.Symmetric);L.Add('visible',Layer.SurfaceVisible);
    Faces:=TJSONArray.Create;L.Add('polygons',Faces);
    for J:=0 to Layer.Count-1 do
      for K:=0 to Layer.Items[J].ChildCount-1 do begin
        Child:=Layer.Items[J].Child[K];Vertices:=TJSONArray.Create;Faces.Add(Vertices);
        for N:=0 to Child.NumberOfPoints-1 do Vertices.Add(CoordinateJSON(Child.Point[N].Coordinate));
      end;
  end;
  Cases.Add(C);
  Windows.SetEnvironmentVariable('FS_STL_TARGET',PChar(IncludeTrailingPathDelimiter(Folder)+Name+'.stl'));
  Ship.Edit.File_ExportSTL;
end;

procedure RunStlSymmetryProbe(Form:TMainForm);
var R:TJSONObject; Cases:TJSONArray; Ship:TFreeShip; L,M,N:TFreeSubdivisionLayer;
  Folder:string; S:TStringList;
begin
  R:=TJSONObject.Create;Cases:=TJSONArray.Create;R.Add('fixtures',Cases);
  Ship:=TFreeShip.Create(nil);
  try
    try
      Folder:=SysUtils.GetEnvironmentVariable('FS_STL_OUTPUT');
      Ship.IsRebuildAsyncDisabled:=True;
      Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'half',False,True);
      Triangle(Ship,L,Vec(1,2,0),Vec(4,2,0),Vec(1,3,2));
      CaptureAndExport(Ship,Cases,'half-asymmetric',Folder);
      L.Symmetric:=True;
      CaptureAndExport(Ship,Cases,'half-symmetric',Folder);

      Ship.Clear;Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'center',True,True);
      Triangle(Ship,L,Vec(10,0,0),Vec(13,0,0),Vec(10,0,2));
      CaptureAndExport(Ship,Cases,'center-only',Folder);

      Ship.Clear;Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'half',True,True);
      Triangle(Ship,L,Vec(1,2,0),Vec(4,2,0),Vec(1,3,2));
      M:=NewLayer(Ship,'single-attachment',False,True);
      Triangle(Ship,M,Vec(10,-4,0),Vec(13,-4,0),Vec(10,-5,2));
      N:=NewLayer(Ship,'center',True,True);
      Triangle(Ship,N,Vec(20,0,0),Vec(23,0,0),Vec(20,0,2));
      L:=NewLayer(Ship,'hidden',True,False);
      Triangle(Ship,L,Vec(30,7,0),Vec(33,7,0),Vec(30,8,2));
      CaptureAndExport(Ship,Cases,'mixed-layers',Folder);

      Ship.Clear;Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'explicit-whole-model',False,True);
      Triangle(Ship,L,Vec(1,2,0),Vec(4,2,0),Vec(1,3,2));
      Triangle(Ship,L,Vec(1,-3,2),Vec(4,-2,0),Vec(1,-2,0));
      CaptureAndExport(Ship,Cases,'whole-model',Folder);

      Ship.Clear;Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'negative-half',True,True);
      Triangle(Ship,L,Vec(1,-3,2),Vec(4,-2,0),Vec(1,-2,0));
      CaptureAndExport(Ship,Cases,'negative-half',Folder);

      Ship.Clear;Ship.ProjectSettings.DisableModelCheck:=True;
      L:=NewLayer(Ship,'near-center',True,True);
      Triangle(Ship,L,Vec(1,0.0001,0),Vec(4,0.0001,0),Vec(1,0.0001,2));
      CaptureAndExport(Ship,Cases,'near-center',Folder);
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    S:=TStringList.Create;
    try S.Text:=R.FormatJSON;S.SaveToFile(IncludeTrailingPathDelimiter(Folder)+'geometry.json')
    finally S.Free end;
  finally Ship.Free; R.Free end;
end;
end.
