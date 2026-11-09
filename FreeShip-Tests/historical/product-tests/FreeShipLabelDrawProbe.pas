program FreeShipLabelDrawProbe;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces, Forms, SysUtils, Types, Graphics, Classes, FreeTypes, FreeGeometry, FreeShipUnit, Main;
type
  TCall = record Text: string; Area: TRect end;
  TProbeViewport = class(TFreeViewport)
    procedure TextOut(X, Y: integer; Val: string); override;
  end;
var Calls: array of TCall;
procedure TProbeViewport.TextOut(X,Y:integer;Val:string);
var I:integer;
begin
  I:=Length(Calls); SetLength(Calls,I+1); Calls[I].Text:=Val;
  Calls[I].Area:=Rect(X,Y,X+TextWidth(Val),Y+TextHeight(Val));
  inherited TextOut(X,Y,Val);
end;
const Views:array[0..3] of TFreeViewType=(fvPerspective,fvProfile,fvPlan,fvBodyplan);
      ZeroValue = '0.000';
      PositiveValue:array[1..3] of string=('20.000','3.000','8.000');
var Ship:TFreeShip; VP:TProbeViewport; BM:TBitmap; I,J,K,Axis,View,Count,Overlaps:integer;
    Found:boolean; A,B:TRect; Model:string; ZeroCount,PositiveCount:integer;
begin
  Model:=GetEnvironmentVariable('FREESHIP_TEST_MODEL');
  if not FileExists(Model) then begin WriteLn('FAIL missing fixture model'); Halt(2) end;
  Application.Initialize; Application.CreateForm(TMainForm,MainForm);
  Ship:=MainForm.FreeShip; Ship.Edit.File_Load(Model);
  if Ship.Surface.NumberOfControlPoints<>8 then begin WriteLn('FAIL expected 8 point fixture'); Halt(3) end;
  Ship.Surface.ControlPoint[5].Name:='P5'; Ship.Visibility.ShowControlNet:=True;
  Ship.Visibility.ShowGrid:=False; Ship.Visibility.ShowHydrostaticData:=False;
  VP:=TProbeViewport.Create(nil); BM:=TBitmap.Create;
  try
    VP.SetBounds(0,0,800,500); VP.DestinationWidth:=800;VP.DestinationHeight:=500;
    VP.ViewportMode:=vmWireframe; BM.SetSize(800,500); VP.DrawingCanvas:=BM.Canvas;
    for View:=0 to 3 do for Axis:=1 to 3 do begin
      VP.ViewType:=Views[View]; VP.ControlPointCoordinateAxis:=Axis;
      VP.InitializeViewport(Ship.Surface.Min,Ship.Surface.Max);
      BM.Canvas.Brush.Color:=clWhite; BM.Canvas.FillRect(0,0,800,500);
      SetLength(Calls,0); Ship.DrawToViewport(VP);
      Count:=0; Found:=False; Overlaps:=0; ZeroCount:=0; PositiveCount:=0;
      for I:=0 to High(Calls) do if (Calls[I].Text=ZeroValue) or (Calls[I].Text=PositiveValue[Axis]) then begin
        Inc(Count); if Calls[I].Text=ZeroValue then Inc(ZeroCount) else Inc(PositiveCount);
        for J:=I+1 to High(Calls) do if (Calls[J].Text=ZeroValue) or (Calls[J].Text=PositiveValue[Axis]) then begin
          A:=Calls[I].Area; B:=Calls[J].Area;
          if (A.Left<B.Right) and (A.Right>B.Left) and (A.Top<B.Bottom) and (A.Bottom>B.Top) then Inc(Overlaps);
        end;
      end;
      WriteLn('view=',View,' axis=',Axis,' labels=',Count,' overlaps=',Overlaps,' zero=',ZeroCount,' positive=',PositiveCount);
      if (Count<>8) or (ZeroCount<>4) or (PositiveCount<>4) or (Overlaps<>0) then Halt(4);
    end;
  finally BM.Free; VP.Free; MainForm.Free end;
end.
