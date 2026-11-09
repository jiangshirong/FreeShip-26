program coordinate_bug_probe;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,Types,Menus,FreeTypes,FreeGeometry,FreeShipUnit,Main,freehullformwindow_panel;
var s:TFreeShip;w:TFreeHullWindow;v:TFreeViewport;b:TBitmap;i,j:Integer;m:TMenuItem;
begin Application.Initialize;Application.CreateForm(TMainForm,MainForm);MainForm.FormShow(MainForm);MainForm.LoadNamedFile(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');s:=MainForm.FreeShip;
b:=TBitmap.Create;b.SetSize(1000,600);
for i:=0 to 3 do begin w:=MainForm.GetMDIChildren(i);v:=w.Viewport;v.FinishProgressiveDisplay(True);v.SetBounds(0,0,1000,600);v.DestinationWidth:=1000;v.DestinationHeight:=600;v.DrawingCanvas:=b.Canvas;v.InitializeViewport(s.Surface.Min,s.Surface.Max);
WriteLn('view=',i,' frozen=',v.PresentationFrozen,' grid=',s.Visibility.ShowControlNet);
v.BeginProgressiveDisplay;v.PresentationFrozen:=True;s.Visibility.ShowControlNet:=False;
for j:=0 to w.PopupMenuHull.Items.Count-1 do begin m:=w.PopupMenuHull.Items[j];if (m.Count=4) and (m.Items[1].Caption='X') then m.Items[2].Click end;
WriteLn('axis=',v.ControlPointCoordinateAxis);if (v.ControlPointCoordinateAxis<>2) or v.PresentationFrozen or v.ProgressivePreview or not s.Visibility.ShowControlNet then Halt(1);for j:=0 to 10 do begin b.Canvas.Brush.Color:=v.Color;b.Canvas.FillRect(0,0,1000,600);s.DrawToViewport(v) end;b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\coordinate-bug-'+IntToStr(i)+'.bmp');end;
b.Free;MainForm.Free;end.
