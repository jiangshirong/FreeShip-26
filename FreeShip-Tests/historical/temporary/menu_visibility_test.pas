program menu_visibility_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,FreeTypes,FreeGeometry,FreeShipUnit,Main,freehullformwindow_panel;
var w:TFreeHullWindow;i,j:Integer;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);MainForm.FormShow(MainForm);
 MainForm.LoadNamedFile(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');
 w:=MainForm.GetMDIChildren(0);
 for i:=0 to 3 do for j:=0 to 1 do begin
  w.Viewport.ViewType:=TFreeViewType(i);
  if j=0 then w.Viewport.ViewportMode:=vmWireframe else w.Viewport.ViewportMode:=vmShade;
  w.UpdateMenu;
  if (w.Camera1.Visible<>(w.Viewport.ViewType=fvPerspective)) or (w.Light.Visible<>(j=1)) then Halt(1);
 end;
 WriteLn('PASS camera and light visibility across four views and both modes, including restoration');
 MainForm.Free;
end.
