program DialogOpeningProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Windows,Main,FreeShipUnit,FreeFileBuffer,
 FreeProjectSettingsDlg,FreeIntersectionDlg,FreeLinesplanFrm,FreeSafeLanguage;
var S,Other:TFreeShip; B:TFreeFileBuffer; P:TFreeProjectSettingsDialog;
 I:TFreeIntersectionDialog; L:TFreeLinesplanForm; Start,Freq,NowTick:Int64; N:Integer; Cached:TObject;
procedure Mark(const Name:string);
begin QueryPerformanceCounter(NowTick);WriteLn(Name,'=',1000*(NowTick-Start)/Freq:0:3);Start:=NowTick end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);S:=MainForm.FreeShip;
 B:=TFreeFileBuffer.Create;B.LoadFromFile(SysUtils.GetEnvironmentVariable('FREESHIP_TEST_MODEL'));
 S.LoadProject(B);B.Free;S.RebuildModel;
 QueryPerformanceFrequency(Freq);
 for N:=1 to 3 do begin
 WriteLn('trial=',N);QueryPerformanceCounter(Start);
 P:=TFreeProjectSettingsDialog.Create(nil);Mark('project.create');P.HandleNeeded;P.Realign;Mark('project.handles');P.Free;
 if N=1 then ApplyLanguageSafely('en',ExtractFilePath(ParamStr(0))+'locale')
 else if N=2 then ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale')
 else ApplyLanguageSafely('ru',ExtractFilePath(ParamStr(0))+'locale');
 S.ProjectSettings.ProjectName:='opening-probe-'+IntToStr(N);
 QueryPerformanceCounter(Start);S.ProjectSettings.Edit;Mark('project.open.cancel');
 if S.ProjectSettings.ProjectName<>'opening-probe-'+IntToStr(N) then Halt(20);
 I:=TFreeIntersectionDialog.Create(S);Mark('sections.create');I.Execute(S);Mark('sections.open.cancel');I.Free;
 QueryPerformanceCounter(Start);L:=TFreeLinesplanForm.Create(nil);Mark('lines.create');
 L.LinesplanFrame.FreeShip:=S;Mark('lines.bind');L.PrepareForDisplay;Mark('lines.prepare');
 L.Viewport.GetDrawingBuffer.SaveToFile('cold-'+IntToStr(N)+'.bmp');
 L.LinesplanFrame.FreeShip:=nil;L.Free;
 Cached:=S.LinesplanFillCache;if Cached=nil then Halt(30);
 L:=TFreeLinesplanForm.Create(nil);QueryPerformanceCounter(Start);L.OpenForShip(S);Mark('lines.request');
 if L.LinesplanFrame.FreeShip<>nil then Halt(10);
 if not L.DisplaysShip(S) then Halt(12);
 Application.ProcessMessages;Mark('lines.complete');
 if L.LinesplanFrame.FreeShip<>S then Halt(11);
 if S.LinesplanFillCache<>nil then Halt(31);
 L.Viewport.GetDrawingBuffer.SaveToFile('warm-'+IntToStr(N)+'.bmp');
 L.LinesplanFrame.FreeShip:=nil;L.Free;
 if S.LinesplanFillCache<>Cached then Halt(32);
 S.Draw;if S.LinesplanFillCache<>nil then Halt(33);
 L:=TFreeLinesplanForm.Create(nil);L.OpenForShip(S);L.Free;Application.ProcessMessages;
 end;
 Other:=TFreeShip.Create(nil);L:=TFreeLinesplanForm.Create(nil);L.OpenForShip(Other);Other.Free;Application.ProcessMessages;
 if L.LinesplanFrame.FreeShip<>nil then Halt(13);L.Free;
 Other:=TFreeShip.Create(nil);L:=TFreeLinesplanForm.Create(nil);L.LinesplanFrame.FreeShip:=Other;Other.Free;
 if L.LinesplanFrame.FreeShip<>nil then Halt(34);L.Free;
 WriteLn('PASS cache-transfer, detached-invalidation, cancellation-rebind, deferred-binding, pending-dedup, close-cancellation, model-lifetime');
 Halt(0);
end.

