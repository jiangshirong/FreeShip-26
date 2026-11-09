program point_reveal_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,Types,FreeTypes,FreeGeometry,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;c0,c1,c2:TColor;n:Integer;
procedure Check(Ok:Boolean;const Msg:String);
begin if not Ok then begin WriteLn('FAIL ',Msg);Halt(1) end;WriteLn('PASS ',Msg) end;
procedure Save(const Name:String);
begin b.Canvas.Brush.Color:=v.Color;b.Canvas.FillRect(0,0,1000,600);s.DrawToViewport(v);b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\point-reveal-'+Name+'.bmp') end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');s.RebuildModel;
 n:=s.Surface.NumberOfSelectedControlPoints;
 b:=TBitmap.Create;b.SetSize(1000,600);v:=TFreeViewport.Create(nil);
 v.SetBounds(0,0,1000,600);v.DestinationWidth:=1000;v.DestinationHeight:=600;v.DrawingCanvas:=b.Canvas;
 v.Color:=clWhite;v.ViewportMode:=vmWireframe;v.ViewType:=fvProfile;
 s.Visibility.ShowControlNet:=True;v.InitializeViewport(s.Surface.Min,s.Surface.Max);
 v.BeginProgressiveDisplay;c0:=v.RevealControlPointColor(clBlack,0);Check(ColorToRGB(c0)=ColorToRGB(clWhite),'preview begins at background color');
 Sleep(550);Save('preview-end');c0:=v.RevealControlPointColor(clBlack,1);
 v.FinishProgressiveDisplay(False);c1:=v.RevealControlPointColor(clBlack,1);
 Check(Abs((ColorToRGB(c0) and 255)-(ColorToRGB(c1) and 255))<=3,'preview to full geometry color continuity');
 Save('full-start');Sleep(160);Save('mid');c2:=v.RevealControlPointColor(clBlack,1);
 Check((ColorToRGB(c2) and 255)<(ColorToRGB(c1) and 255),'points brighten gradually');
 Sleep(270);Save('complete');Check(ColorToRGB(v.RevealControlPointColor(clBlack,1))=ColorToRGB(clBlack),'normal point color reached');
 Check(s.Surface.NumberOfSelectedControlPoints=n,'animation preserves selection');
 v.FinishProgressiveDisplay(True);Check(v.RevealControlPointColor(clRed,0)=clRed,'skipping animation returns exact original color');
 v.Free;b.Free;MainForm.Free;
end.
