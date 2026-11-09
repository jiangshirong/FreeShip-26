program progressive_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Graphics,FreeTypes,FreeGeometry,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;t:qword;previewMs,fullMs:qword;i,w,h:integer;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 MainForm.StatusBar.Visible:=False;w:=MainForm.MainClientPanel.Width;h:=MainForm.MainClientPanel.Height;
 s.Edit.ProgressBar:=MainForm.ProgressBarMain;
 for i:=1 to 100 do begin s.Edit.OnFaceRebuilt(s,i,100);
 if MainForm.StatusBar.Visible or (MainForm.MainClientPanel.Width<>w) or (MainForm.MainClientPanel.Height<>h) then Halt(6) end;
 s.Edit.ProgressBar:=nil;WriteLn('PASS hidden progress callbacks preserve client bounds');
 s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');
 if s.NumberOfSelectedControlPoints<>0 then Halt(7);
 if not s.ModelIsLoaded then Halt(1);s.RebuildModel;
 v:=TFreeViewport.Create(nil);b:=TBitmap.Create;
 v.SetBounds(0,0,800,500);v.DestinationWidth:=800;v.DestinationHeight:=500;b.SetSize(800,500);
 v.DrawingCanvas:=b.Canvas;v.ViewportMode:=vmWireframe;v.ViewType:=fvProfile;
 v.InitializeViewport(s.Surface.Min,s.Surface.Max);
 v.BeginProgressiveDisplay;if not v.ProgressivePreview then Halt(2);
 t:=GetTickCount64;Application.ProcessMessages;
 if not v.ProgressivePreview then Halt(3);
 b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,800,500);
 t:=GetTickCount64;s.DrawToViewport(v);previewMs:=GetTickCount64-t;
 b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\progressive-preview.bmp');
 if not v.ProgressivePreview then Halt(4);
 t:=GetTickCount64;while v.ProgressivePreview and (GetTickCount64-t<1000) do begin Sleep(5);Application.ProcessMessages end;
 if v.ProgressivePreview then Halt(5);
 b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,800,500);
 t:=GetTickCount64;s.DrawToViewport(v);fullMs:=GetTickCount64-t;
 b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\progressive-full.bmp');
 WriteLn('PASS preview persists until drawn then advances by timer; preview_ms=',previewMs,' full_ms=',fullMs);
 v.Free;b.Free;MainForm.Free;
end.
