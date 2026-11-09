unit UiAnimationProfile;
{$mode objfpc}{$H+}
interface
uses Classes;
var ProfileRows:TStringList;ProfileEnabled:Boolean=False;
procedure ProfileStage(View:PtrUInt;const Stage:string);
implementation
uses Windows,SysUtils;
procedure ProfileStage(View:PtrUInt;const Stage:string);
var Q:Int64;
begin if not ProfileEnabled then Exit;QueryPerformanceCounter(Q);ProfileRows.Add(IntToStr(View)+','+Stage+','+IntToStr(Q)) end;
initialization ProfileRows:=TStringList.Create;
finalization ProfileRows.Free;
end.
