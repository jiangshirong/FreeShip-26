program projection_probe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Types,FreeGeometry,FreeTypes,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;i,j:Integer;c:T3DCoordinate;p:TPoint;f:TextFile;
begin Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
s.Edit.File_Load(GetEnvironmentVariable('FREESHIP_TEST_MODEL'));s.RebuildModel;
AssignFile(f,GetTempDir+'FreeShipLabelIntegration\projection.csv');Rewrite(f);WriteLn(f,'view,id,px,py,x,y,z');
for i:=0 to 3 do begin v:=TFreeViewport.Create(nil);v.SetBounds(0,0,960,500);v.ViewType:=TFreeViewType(i);v.InitializeViewport(s.Surface.Min,s.Surface.Max);
for j:=0 to s.Surface.NumberOfControlPoints-1 do begin
 c:=s.Surface.ControlPoint[j].Coordinate;if (v.ViewType=fvBodyplan) and (c.X<=s.Surface.SplitSectionLocation) then c.Y:=-c.Y;p:=v.Project(c);c:=s.Surface.ControlPoint[j].Coordinate;
WriteLn(f,i,',',j,',',p.X,',',p.Y,',',c.X:0:6,',',c.Y:0:6,',',c.Z:0:6);
end;v.Free end;CloseFile(f);WriteLn('points=',s.Surface.NumberOfControlPoints);MainForm.Free;end.
