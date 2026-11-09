program overlay_model_probe;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,FreeTypes,FreeGeometry,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;i:Integer;
procedure Save(const Name:string);
begin b.Canvas.Brush.Color:=v.Color;b.Canvas.FillRect(0,0,1000,600);s.DrawToViewport(v);b.SaveToFile(GetTempDir+'FreeShipLabelIntegration\bfs-model-'+Name+'.bmp') end;
begin Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;s.Edit.File_Load(GetEnvironmentVariable('FREESHIP_TEST_MODEL'));s.RebuildModel;b:=TBitmap.Create;b.SetSize(1000,600);v:=TFreeViewport.Create(nil);v.SetBounds(0,0,1000,600);v.DestinationWidth:=1000;v.DestinationHeight:=600;v.DrawingCanvas:=b.Canvas;v.Color:=RGBToColor(232,239,236);v.ViewType:=fvProfile;v.ViewportMode:=vmWireframe;s.Visibility.ShowGrid:=True;s.Visibility.ShowControlNet:=True;v.InitializeViewport(s.Surface.Min,s.Surface.Max);v.FinishProgressiveDisplay(False);Save('start');Sleep(350);Save('mid');Sleep(350);Save('late');Sleep(700);Save('complete');v.Free;b.Free;MainForm.Free;end.
