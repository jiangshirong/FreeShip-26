program test_all_labels;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Types,Graphics,FreeTypes,FreeGeometry,FreeShipUnit,Main;
type TCall=record text:string; area:TRect end; TV=class(TFreeViewport) procedure TextOut(x,y:integer;val:string);override;end;
var calls:array of TCall;
procedure TV.TextOut(x,y:integer;val:string);var n:integer;begin n:=length(calls);setlength(calls,n+1);calls[n].text:=val;calls[n].area:=Rect(x,y,x+TextWidth(val),y+TextHeight(val));inherited TextOut(x,y,val) end;
var s:TFreeShip;v:TV;b:TBitmap;i,j,axis,view,n,pointlabels,inside:integer;a,c:TRect; q:T3DCoordinate; qp:TPoint;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');
 n:=0;for i:=0 to s.Surface.NumberOfControlPoints-1 do if s.Surface.ControlPoint[i].Visible then inc(n);WriteLn('visible_points=',n);
 s.Visibility.ShowControlNet:=true;s.Visibility.ShowGrid:=true;s.Visibility.ShowHydrostaticData:=true;
 v:=TV.Create(nil);b:=TBitmap.Create;try
  v.SetBounds(0,0,950,500);v.DestinationWidth:=950;v.DestinationHeight:=500;b.SetSize(950,500);v.DrawingCanvas:=b.Canvas;v.ViewportMode:=vmWireframe;
  for view:=0 to 3 do for axis:=0 to 1 do begin
   v.ViewType:=TFreeViewType(view);v.InitializeViewport(s.Surface.Min,s.Surface.Max);v.ControlPointCoordinateAxis:=axis*2;
   inside:=0;
   for i:=0 to s.Surface.NumberOfControlPoints-1 do if s.Surface.ControlPoint[i].Visible then begin
    q:=s.Surface.ControlPoint[i].Coordinate;if (v.ViewType=fvBodyplan) and (q.X<=s.Surface.SplitSectionLocation) then q.Y:=-q.Y;
    qp:=v.Project(q);if (qp.X>=0) and (qp.X<950) and (qp.Y>=0) and (qp.Y<500) then inc(inside);
   end;
   b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,950,500);setlength(calls,0);s.DrawToViewport(v);n:=0;
   for i:=0 to high(calls) do for j:=i+1 to high(calls) do begin a:=calls[i].area;c:=calls[j].area;if (a.Left<c.Right) and (a.Right>c.Left) and (a.Top<c.Bottom) and (a.Bottom>c.Top) then begin inc(n);if n<4 then WriteLn('OVERLAP ',calls[i].text,' / ',calls[j].text) end end;
   pointlabels:=0;for i:=0 to high(calls) do if (Pos('.',calls[i].text)>0) and (Length(calls[i].text)-Pos('.',calls[i].text)=3) then inc(pointlabels);
   WriteLn('view=',view,' axis=',axis*2,' labels=',length(calls),' coordinate_labels=',pointlabels,' anchors_inside=',inside,' overlap=',n);
   if (view=1) and (axis=1) then b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\repulsion-profile.bmp');
   if n>0 then Halt(3);
  end;
 finally b.Free;v.Free;MainForm.Free end;
end.
