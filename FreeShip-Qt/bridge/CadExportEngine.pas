{ GPL-2.0-or-later. Original FreeShip NURBS construction, no live model mutation. }
unit CadExportEngine;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Classes,fpjson,FreeShipUnit;
function BuildCadPatches(FreeShip:TFreeShip;Ids:TFPList;const CancelFile:string):TJSONObject;
implementation
uses SysUtils,Math,FreeGeometry,FreeTypes;
function BuildCadPatches(FreeShip:TFreeShip;Ids:TFPList;const CancelFile:string):TJSONObject;
const SendTriangles=False;
var Layers:TFasterListTFreeSubdivisionLayer;Layer:TFreeSubdivisionLayer;
    Assembled:TFreeFaceArray;NAssembled,I,J,K,Cols,Rows,LayerId:Integer;
    FaceData:TFreeFaceGrid;Grid:TFreeSubdivisionPointGrid;
    CtrlFace:TFreeSubdivisionControlFace;Face:TFreeSubdivisionFace;
    Patches:TJSONArray;PointCount:Int64;Started:QWord;
  procedure Checkpoint;
  begin
    if FileExists(CancelFile) then raise Exception.Create('已取消曲面导出。');
    if GetTickCount64-Started>90000 then raise Exception.Create('曲面生成超时。');
  end;
  procedure AddPatch(Nurb:TFreeNURBsurface;Id:Integer);
  var Obj:TJSONObject;U,V,Points:TJSONArray;X,Y:Integer;P:T3DCoordinate;
  begin
    Checkpoint;Inc(PointCount,Int64(Nurb.ColCount)*Nurb.RowCount);
    if (PointCount>2000000) or (Patches.Count>=100000) then raise Exception.Create('曲面数据超出导出限额。');
    Obj:=TJSONObject.Create;Patches.Add(Obj);Obj.Add('layer',Id);
    Obj.Add('cols',Nurb.ColCount);Obj.Add('rows',Nurb.RowCount);
    Obj.Add('degree_u',Nurb.ColDegree);Obj.Add('degree_v',Nurb.RowDegree);
    U:=TJSONArray.Create;V:=TJSONArray.Create;Points:=TJSONArray.Create;
    Obj.Add('u',U);Obj.Add('v',V);Obj.Add('points',Points);
    for X:=0 to Length(Nurb.ColKnotVector)-1 do U.Add(Double(Nurb.ColKnotVector[X]));
    for X:=0 to Length(Nurb.RowKnotVector)-1 do V.Add(Double(Nurb.RowKnotVector[X]));
    for Y:=0 to Nurb.RowCount-1 do for X:=0 to Nurb.ColCount-1 do begin
      P:=Nurb.Point[X,Y];
      if IsNan(P.X) or IsNan(P.Y) or IsNan(P.Z) or IsInfinite(P.X) or IsInfinite(P.Y) or IsInfinite(P.Z) then raise Exception.Create('曲面包含无效坐标。');
      Points.Add(TJSONArray.Create([Double(P.X),Double(P.Y),Double(P.Z)]));
    end;
  end;
  procedure CanonicalGrid(var Cols,Rows:Integer;var Grid:TFreeSubdivisionPointGrid);
  var Rotation,Best,BestCols,BestRows,C,R,X,Y,Compare:Integer;
      A,B:T3DCoordinate;Source,Next:TFreeSubdivisionPointGrid;
    function At(Rotation,X,Y:Integer):TFreeSubdivisionPoint;
    begin
      case Rotation of
        0:Result:=Source[Y][X];
        1:Result:=Source[Rows-1-X][Y];
        2:Result:=Source[Rows-1-Y][Cols-1-X];
        else Result:=Source[X][Cols-1-Y];
      end;
    end;
    function Less(A,B:T3DCoordinate):Integer;
    begin
      if A.X<B.X then Exit(-1);if A.X>B.X then Exit(1);
      if A.Y<B.Y then Exit(-1);if A.Y>B.Y then Exit(1);
      if A.Z<B.Z then Exit(-1);if A.Z>B.Z then Exit(1);Result:=0;
    end;
  begin
    // Rotations only: retain the original oriented face, fix the order in which
    // row/column Single knot insertion occurs. No pointer address tie-breaks.
    Source:=Grid;Best:=0;BestCols:=Cols;BestRows:=Rows;
    for Rotation:=1 to 3 do begin
      if Odd(Rotation) then begin C:=Rows;R:=Cols end else begin C:=Cols;R:=Rows end;
      Compare:=0;
      if C<BestCols then Compare:=-1 else if C>BestCols then Compare:=1;
      if Compare=0 then for Y:=0 to R-1 do begin
        for X:=0 to C-1 do begin
          A:=At(Rotation,X,Y).Coordinate;B:=At(Best,X,Y).Coordinate;Compare:=Less(A,B);
          if Compare<>0 then Break;
        end;
        if Compare<>0 then Break;
      end;
      if Compare<0 then begin Best:=Rotation;BestCols:=C;BestRows:=R end;
    end;
    SetLength(Next,BestRows,BestCols);
    for Y:=0 to BestRows-1 do for X:=0 to BestCols-1 do Next[Y][X]:=At(Best,X,Y);
    Cols:=BestCols;Rows:=BestRows;Grid:=Next;
  end;
  {$I CadNurbs.inc}
begin
  Started:=GetTickCount64;PointCount:=0;Result:=TJSONObject.Create;
  Layers:=TFasterListTFreeSubdivisionLayer.Create;
  try
    try
      Result.Add('schema',1);Patches:=TJSONArray.Create;Result.Add('patches',Patches);
      FreeShip.IsRebuildAsyncDisabled:=True;
      FreeShip.Surface.DesiredSubdivisionLevel:=1;
      FreeShip.Surface.SubdivisionMode:=fmCatmullClark;
      if not FreeShip.Surface.Built then FreeShip.Surface.Rebuild;
      for I:=0 to FreeShip.NumberOfLayers-1 do
        if FreeShip.Layer[I].Count>0 then Layers.Add(FreeShip.Layer[I]);
      Layers.Sort;Checkpoint;
      FreeShip.Surface.AssembleFacesToPatches(Layers,amNURBS,Assembled,NAssembled);
      for I:=0 to NAssembled-1 do begin
        Checkpoint;FaceData:=Assembled[I];
        if (FaceData.NCols<1) or (FaceData.NRows<1) then raise Exception.Create('曲面网格为空。');
        CtrlFace:=FaceData.Faces[0][0];Layer:=CtrlFace.Layer;
        LayerId:=Ids.IndexOf(Layer)+1;if LayerId<1 then raise Exception.Create('曲面图层身份丢失。');
        if (FaceData.NCols>1) or (FaceData.NRows>1) or (CtrlFace.NumberOfPoints=4) then begin
          FreeShip.Surface.ConvertToGrid(FaceData,Cols,Rows,Grid);
          if (Cols<2) or (Rows<2) then raise Exception.Create('无法生成完整曲面网格。');
          CanonicalGrid(Cols,Rows,Grid);ProcessGrid(Cols,Rows,Grid,LayerId,False);
        end else begin
          SetLength(Grid,2,2);
          for J:=0 to CtrlFace.ChildCount-1 do begin
            Face:=CtrlFace.Child[J];if Face.NumberOfPoints<>4 then raise Exception.Create('非四边形曲面片无法转换。');
            Grid[0][1]:=Face.Point[0];Grid[0][0]:=Face.Point[1];
            Grid[1][0]:=Face.Point[2];Grid[1][1]:=Face.Point[3];
            Cols:=2;Rows:=2;CanonicalGrid(Cols,Rows,Grid);ProcessGrid(Cols,Rows,Grid,LayerId,False);
          end;
        end;
      end;
      if Patches.Count=0 then raise Exception.Create('没有可导出的曲面。');
    except Result.Free;Result:=nil;raise end;
  finally Layers.Free end;
end;
end.
