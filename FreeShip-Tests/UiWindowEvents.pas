unit UiWindowEvents;
{$mode objfpc}{$H+}
interface
uses LCLType, Types, Graphics, fpjson;
procedure RecordDrawnText(Sender:TObject; const Value:string);
procedure ClearDrawnText(Sender:TObject);
function HasDrawnText(Sender:TObject; const Value:string):Boolean;
procedure RecordPaint(Sender:TObject);
procedure TimedCacheCopy(Sender:TObject; Canvas:TCanvas; const Dest:TRect; Buffer:Graphics.TBitmap);
function PaintCount(Sender:TObject):Integer;
function CacheCopyCount(Sender:TObject):Integer;
function CacheCopyMilliseconds(Sender:TObject):Double;
procedure ResetWindowEvents;
function TakeWindowEvents:TJSONArray;
function RedrawWindowProbe(Wnd:HWND; UpdateRect:PRect; Region:HRGN; Flags:UINT):Boolean;
implementation
uses LCLIntf, Classes, Windows;
type TPaintCount=class
  ObjectRef:TObject; Paints,Copies:Integer; CopyMilliseconds:Double; Texts:TStringList;
end;
var Events:TJSONArray; Counters:TFPList;
function Counter(Sender:TObject):TPaintCount;
var I:Integer;
begin
  if Counters=nil then Counters:=TFPList.Create;
  for I:=0 to Counters.Count-1 do if TPaintCount(Counters[I]).ObjectRef=Sender then Exit(TPaintCount(Counters[I]));
  Result:=TPaintCount.Create; Result.ObjectRef:=Sender; Result.Texts:=TStringList.Create; Counters.Add(Result);
end;
procedure RecordDrawnText(Sender:TObject; const Value:string); begin Counter(Sender).Texts.Add(Value) end;
procedure ClearDrawnText(Sender:TObject); begin Counter(Sender).Texts.Clear end;
function HasDrawnText(Sender:TObject; const Value:string):Boolean; begin Result:=Counter(Sender).Texts.IndexOf(Value)>=0 end;
procedure RecordPaint(Sender:TObject); begin Inc(Counter(Sender).Paints) end;
procedure TimedCacheCopy(Sender:TObject; Canvas:TCanvas; const Dest:TRect; Buffer:Graphics.TBitmap);
var A,B,F:Int64; C:TPaintCount;
begin
  C:=Counter(Sender); Inc(C.Copies); QueryPerformanceFrequency(F); QueryPerformanceCounter(A);
  try Canvas.StretchDraw(Dest,Buffer)
  finally QueryPerformanceCounter(B); if F>0 then C.CopyMilliseconds:=C.CopyMilliseconds+(B-A)*1000.0/F end;
end;
function PaintCount(Sender:TObject):Integer; begin Result:=Counter(Sender).Paints end;
function CacheCopyCount(Sender:TObject):Integer; begin Result:=Counter(Sender).Copies end;
function CacheCopyMilliseconds(Sender:TObject):Double; begin Result:=Counter(Sender).CopyMilliseconds end;
procedure ResetWindowEvents;
begin if Events<>nil then Events.Free; Events:=TJSONArray.Create end;
function TakeWindowEvents:TJSONArray;
begin Result:=Events; Events:=nil end;
function RedrawWindowProbe(Wnd:HWND; UpdateRect:PRect; Region:HRGN; Flags:UINT):Boolean;
var O,R:TJSONObject;
begin
  Result:=LCLIntf.RedrawWindow(Wnd,UpdateRect,Region,Flags);
  if Events=nil then Exit;
  O:=TJSONObject.Create; O.Add('flags',Int64(Flags)); O.Add('result',Result);
  if Assigned(UpdateRect) then begin
    R:=TJSONObject.Create; R.Add('left',UpdateRect^.Left); R.Add('top',UpdateRect^.Top);
    R.Add('right',UpdateRect^.Right); R.Add('bottom',UpdateRect^.Bottom); O.Add('rect',R);
  end;
  Events.Add(O);
end;
end.
