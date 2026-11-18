{ GPL-2.0-or-later. Original unfolding core, disposable calculation document. }
unit PlateEngine;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses fpjson,FreeShipUnit;
function DevelopPlates(Ship:TFreeShip;const CancelFile:string):TJSONObject;
implementation
uses Classes,SysUtils,Graphics,FreeGeometry,FreeTypes,PlateHooks;
function DevelopPlates(Ship:TFreeShip;const CancelFile:string):TJSONObject;
var Plates:TFasterListTFreeDevelopedPatch;I,J:Integer;P:TFreeDevelopedPatch;A:TJSONArray;
begin
  Result:=nil;Plates:=TFasterListTFreeDevelopedPatch.Create;
  PlateCancelFile:=CancelFile;PlateStarted:=GetTickCount64;
  try
    PlateCheckpoint;Ship.IsRebuildAsyncDisabled:=True;Ship.RebuildModel;
    for I:=0 to Ship.NumberOfLayers-1 do if Ship.Layer[I].Developable then begin
      PlateCheckpoint;Ship.Layer[I].Unroll(Plates);
    end;
    if Plates.Count=0 then raise Exception.Create('没有可展开的板片。请在图层属性中勾选“可展开”，并确认图层含有曲面。');
    Result:=TJSONObject.Create;
    try
      Result.Add('schema',1);Result.Add('units',Ord(Ship.ProjectSettings.ProjectUnits));
      Result.Add('waterline',Double(Ship.ProjectSettings.ProjectDraft+Ship.FindLowestHydrostaticsPoint));
      Result.Add('underwater_color',LongInt(ColorToRGB(Ship.ProjectSettings.ProjectUnderWaterColor)));
      Result.Add('shade_submerged',Ship.ProjectSettings.ProjectShadeUnderwaterShip);
      A:=TJSONArray.Create;Result.Add('plates',A);
      for I:=0 to Plates.Count-1 do begin
        PlateCheckpoint;P:=Plates[I];
        for J:=0 to Ship.NumberOfStations-1 do P.IntersectPlane(Ship.Station[J].Plane,Ship.Station[J].Color);
        for J:=0 to Ship.NumberOfButtocks-1 do P.IntersectPlane(Ship.Buttock[J].Plane,Ship.Buttock[J].Color);
        for J:=0 to Ship.NumberOfWaterlines-1 do P.IntersectPlane(Ship.Waterline[J].Plane,Ship.Waterline[J].Color);
        for J:=0 to Ship.NumberOfDiagonals-1 do P.IntersectPlane(Ship.Diagonal[J].Plane,Ship.Diagonal[J].Color);
        A.Add(P.ServicePlateData);
      end;
      PlateCheckpoint;
    except Result.Free;Result:=nil;raise end;
  finally
    for I:=0 to Plates.Count-1 do Plates[I].Free;
    Plates.Free;PlateCancelFile:='';PlateStarted:=0;
  end;
end;
end.
