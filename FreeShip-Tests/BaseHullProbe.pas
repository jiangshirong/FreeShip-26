unit BaseHullProbe;
{$mode objfpc}{$H+}
interface
procedure Run;
implementation
uses SysUtils, Classes, fpjson, FreeShipUnit, FreeGeometry, FreeFileBuffer, FreeTypes;
procedure Run;
var S:TFreeShip; B:TFreeFileBuffer; H:TFreeHydrostaticCalc; T:TStringList;
    I,K,N:Integer; E:TFreeSubdivisionControlEdge; Root,O:TJSONObject; Cases:TJSONArray;
begin
  Root:=TJSONObject.Create; Cases:=TJSONArray.Create; Root.Add('cases',Cases);
  S:=TFreeShip.Create(nil); B:=TFreeFileBuffer.Create;
  try
    S.IsRebuildAsyncDisabled:=True;
    B.LoadFromFile(GetEnvironmentVariable('FS_BASEHULL_MODEL')); S.LoadProject(B);
    for K:=0 to 1 do begin
      N:=0;
      if K=1 then begin
        for I:=0 to S.Surface.NumberOfControlEdges-1 do begin
          E:=S.Surface.ControlEdge[I];
          if (Abs(E.StartPoint.Coordinate.X)<1e-6) and (Abs(E.EndPoint.Coordinate.X)<1e-6) then begin
            E.Crease:=True; Inc(N)
          end;
        end;
        S.Surface.Rebuild;
      end;
      O:=TJSONObject.Create; Cases.Add(O); O.Add('creased',K=1); O.Add('changed_edges',N);
      H:=TFreeHydrostaticCalc.Create(S); T:=TStringList.Create;
      try
        try
          H.Draft:=S.ProjectSettings.ProjectDraft; H.Calculate;
          O.Add('volume_m3',H.Data.Volume); O.Add('calculation_error',H.ErrorString);
          H.AddData(T,fhSingleCalculation,' '); O.Add('report_lines',T.Count); O.Add('status','completed');
        except on Ex:Exception do begin O.Add('status','exception'); O.Add('exception',Ex.ClassName+': '+Ex.Message); O.Add('address',BackTraceStrFunc(ExceptAddr)) end end;
      finally T.Free; H.Free end;
    end;
  finally
    B.Free; S.Free; T:=TStringList.Create;
    try T.Text:=Root.FormatJSON; T.SaveToFile(GetEnvironmentVariable('FS_BASEHULL_REPORT')) finally T.Free; Root.Free end;
  end;
end;
end.
