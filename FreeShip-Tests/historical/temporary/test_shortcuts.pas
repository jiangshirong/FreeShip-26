program test_shortcuts;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces, Forms, SysUtils, Classes, Controls, LCLTranslator, FreeStringsUnit, Main, FreeGeometry, freehullformwindow_panel;
var f:TMainForm;w:TFreeHullWindow;k:Word;t:qword;
procedure check(ok:boolean;const text:string);begin if not ok then begin WriteLn('FAIL ',text); Halt(1) end;WriteLn('PASS ',text) end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);f:=MainForm;
 f.FormShow(f);t:=GetTickCount64;f.LoadNamedFile(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');WriteLn('load_named_ms=',GetTickCount64-t);
 check(f.FreeShip.ModelIsLoaded and (f.MDIChildCount=4),'model copy and four views created');w:=f.GetMDIChildren(0);
 check(w.ZoomExtents.ShortCut=0,'view zoom fit does not steal Ctrl+A');
 f.FreeShip.Surface.ClearSelection;f.UpdateMenu;k:=Ord('A');w.ViewportKeyDown(w.Viewport,k,[ssCtrl]);
 check((k=0) and (f.FreeShip.NumberOfSelectedControlPoints>0),'Ctrl+A selects visible points');
 f.FreeShip.Preferences.QuickLanguageEnabled:=True;f.FreeShip.Preferences.QuickLanguageFirst:='Chinese';f.FreeShip.Preferences.QuickLanguageSecond:='Russian';
 f.FreeShip.Preferences.Language:='Chinese';SetDefaultLang('cn',GetTempDir+'FreeShipPathCheck\current-validation\locale','FreeShip',True);SetUserStringsLang;
 f.FreeShip.Preferences.LanguagesDirectory:=GetTempDir+'FreeShipPathCheck\current-validation\locale';
 k:=Ord('L');Application.NotifyKeyDownBeforeHandler(w.Viewport,k,[ssCtrl,ssAlt]);check((k=0) and (f.FreeShip.Preferences.Language='Russian'),'application pre-handler switches Chinese to Russian');
 k:=Ord('L');Application.NotifyKeyDownBeforeHandler(f,k,[ssCtrl,ssAlt,ssLeft]);check((k=0) and (f.FreeShip.Preferences.Language='Chinese'),'pre-handler tolerates mouse button state');
 f.FreeShip.Preferences.QuickLanguageEnabled:=False;k:=Ord('L');Application.NotifyKeyDownBeforeHandler(w.Viewport,k,[ssCtrl,ssAlt]);check((k=Ord('L')) and (f.FreeShip.Preferences.Language='Chinese'),'disabled switch does not consume key');
 f.Free;
end.
