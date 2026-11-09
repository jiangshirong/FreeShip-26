program point_motion_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,Types,FreeTypes,FreeGeometry,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;p0,p1,p2:TPoint;n:Integer;
procedure Check(Ok:Boolean;const Msg:String);
begin if not Ok then begin WriteLn('FAIL ',Msg);Halt(1) end;WriteLn('PASS ',Msg) end;
procedure Save(const Name:String);
begin b.Canvas.Brush.Color:=v.Color;b.Canvas.FillRect(0,0,1000,600);s.DrawToViewport(v);b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\point-motion-'+Name+'.bmp') end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');s.RebuildModel;
 n:=s.Surface.NumberOfSelectedControlPoints;
 b:=TBitmap.Create;b.SetSize(1000,600);v:=TFreeViewport.Create(nil);
 v.SetBounds(0,0,1000,600);v.DestinationWidth:=1000;v.DestinationHeight:=600;v.DrawingCanvas:=b.Canvas;
 v.Color:=clWhite;v.ViewportMode:=vmWireframe;v.ViewType:=fvProfile;
 s.Visibility.ShowControlNet:=True;v.InitializeViewport(s.Surface.Min,s.Surface.Max);
 v.BeginProgressiveDisplay;Save('lines-start');Sleep(550);Save('lines-complete');
 v.FinishProgressiveDisplay(False);
 Check(not v.ControlPointRevealed(Point(100,200)),'left point initially hidden');
 Sleep(120);
 Check(v.ControlPointRevealed(Point(900,200)),'right point revealed first');
 Check(not v.ControlPointRevealed(Point(500,200)),'center still hidden');
 Sleep(480);Check(v.ControlPointRevealed(Point(500,200)),'center revealed next');
 Check(not v.ControlPointRevealed(Point(100,200)),'left still hidden');
 Sleep(450);Check(v.ControlPointRevealed(Point(100,200)),'left revealed last');Save('sweep-complete');
 Check(s.Surface.NumberOfSelectedControlPoints=n,'selection preserved');
 v.FinishProgressiveDisplay(True);Check(v.ControlPointRevealed(Point(-100,200)),'skip reveals all markers');
 v.FinishProgressiveDisplay(False);
 Check(v.ControlPointEntranceColor(Point(1000,200),clBlack)<>clBlack,'new points receive entrance highlight');
 Save('highlight-start');Sleep(350);
 Check(v.ControlPointEntranceColor(Point(1000,200),clBlack)=clBlack,'highlight returns to original color');
 v.FinishProgressiveDisplay(True);
 v.Free;b.Free;MainForm.Free;
end.
