program shared_resize_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Types,FreeShipUnit,Main,freehullformwindow_panel,MDIPanel;
var a,b:TFreeHullWindow;l,t,w,h:Integer;
procedure Check(Ok:Boolean;const Msg:String);
begin if not Ok then begin WriteLn('FAIL ',Msg);Halt(1) end;WriteLn('PASS ',Msg) end;
procedure Setup;
begin a.WindowState:=wsNormal;b.WindowState:=wsNormal;a.Visible:=True;b.Visible:=True;
a.SetBounds(0,0,400,300);b.SetBounds(0,300,600,300);end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);
 MainForm.FormShow(MainForm);MainForm.LoadNamedFile(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');
 a:=MainForm.GetMDIChildren(0);b:=MainForm.GetMDIChildren(1);Setup;
 a.BeginSharedResize(wrszsS,Point(200,300));l:=0;t:=0;w:=400;h:=350;a.ApplySharedResize(l,t,w,h);a.SetBounds(l,t,w,h);
 Check((a.Height=350) and (b.Top=350) and (b.Height=250),'shared horizontal edge resizes both without gap');
 Setup;b.BeginSharedResize(wrszsN,Point(500,300));l:=0;t:=250;w:=600;h:=350;b.ApplySharedResize(l,t,w,h);b.SetBounds(l,t,w,h);
 Check((a.Height=300) and (b.Top=250),'exposed part changes only dragged window');
 Setup;a.BeginSharedResize(wrszsS,Point(200,300));l:=0;t:=0;w:=400;h:=590;a.ApplySharedResize(l,t,w,h);
 Check((b.Height>=b.Constraints.MinHeight) and (h+b.Height=600),'neighbor minimum height clamps common edge');
 Setup;a.SetBounds(0,0,400,300);b.SetBounds(400,0,400,300);
 a.BeginSharedResize(wrszsE,Point(400,150));l:=0;t:=0;w:=450;h:=300;a.ApplySharedResize(l,t,w,h);a.SetBounds(l,t,w,h);
 Check((a.Width=450) and (b.Left=450) and (b.Width=350),'shared vertical edge resizes both');
 Setup;b.Visible:=False;a.BeginSharedResize(wrszsS,Point(200,300));l:=0;t:=0;w:=400;h:=350;a.ApplySharedResize(l,t,w,h);
 Check(b.Top=300,'hidden window does not participate');
 MainForm.Free;
end.
