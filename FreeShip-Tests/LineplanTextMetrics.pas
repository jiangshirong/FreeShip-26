unit LineplanTextMetrics;
{$mode objfpc}{$H+}
interface
uses Types, Math, Graphics;
type TPlacementSample=record
 Valid:Boolean; Text:string; Position:TPoint; Bounds:TRect;
end;
var CurrentSamples,PreviousSamples:array of TPlacementSample;
var Calls, MinSize, MaxSize, BusyCalls: Integer;
var CaptureFill:Boolean; FillImage:TBitmap;
procedure RecordFill(Canvas:TCanvas; W,H:Integer);
procedure Reset;
procedure RecordText(Size: Integer; Busy: Boolean);
procedure RecordPlacement(Index:Integer; const Text:string; X,Y,L,T,R,B:Integer);
procedure SavePlacements;
procedure ObservePan(DX,DY,W,H:Integer; out Checked,Errors:Integer);
implementation
procedure RecordFill(Canvas:TCanvas; W,H:Integer);
begin
 if not CaptureFill then Exit;
 if FillImage=nil then FillImage:=TBitmap.Create;
 FillImage.SetSize(W,H);FillImage.Canvas.CopyRect(Rect(0,0,W,H),Canvas,Rect(0,0,W,H));
end;
procedure Reset; begin Calls:=0; MinSize:=0; MaxSize:=0; BusyCalls:=0; SetLength(CurrentSamples,0) end;
procedure RecordText(Size: Integer; Busy: Boolean);
begin
 if Calls=0 then begin MinSize:=Size; MaxSize:=Size end
 else begin if Size<MinSize then MinSize:=Size; if Size>MaxSize then MaxSize:=Size end;
 Inc(Calls); if Busy then Inc(BusyCalls);
end;
procedure RecordPlacement(Index:Integer; const Text:string; X,Y,L,T,R,B:Integer);
begin
 if Index>=Length(CurrentSamples) then SetLength(CurrentSamples,Index+1);
 CurrentSamples[Index].Valid:=True; CurrentSamples[Index].Text:=Text;
 CurrentSamples[Index].Position:=Point(X,Y); CurrentSamples[Index].Bounds:=Rect(L,T,R,B);
end;
procedure SavePlacements;
begin PreviousSamples:=Copy(CurrentSamples,0,Length(CurrentSamples)) end;
procedure ObservePan(DX,DY,W,H:Integer; out Checked,Errors:Integer);
var I,N:Integer; A,B:TPlacementSample;
 function Interior(const R:TRect):Boolean;
 var Margin:Integer;
 begin
  // Exclude the edge-attraction radius plus its maximum displacement.
  // The production formula is max(24,2*height), strength=max(8,.75*height).
  Margin:=Ceil(Max(24,2*(R.Bottom-R.Top))+Max(8,0.75*(R.Bottom-R.Top)));
  Result:=(R.Left>Margin) and (R.Top>Margin) and (R.Right<W-Margin) and (R.Bottom<H-Margin);
 end;
begin
 Checked:=0; Errors:=0; N:=Length(CurrentSamples);
 if Length(PreviousSamples)<N then N:=Length(PreviousSamples);
 for I:=0 to N-1 do begin
  A:=PreviousSamples[I]; B:=CurrentSamples[I];
  if not A.Valid or not B.Valid or (A.Text<>B.Text) then Continue;
  if not Interior(A.Bounds) or not Interior(B.Bounds) then Continue;
  Inc(Checked);
  if (B.Position.X<>A.Position.X+DX) or (B.Position.Y<>A.Position.Y+DY) or
     (B.Bounds.Left<>A.Bounds.Left+DX) or (B.Bounds.Right<>A.Bounds.Right+DX) or
     (B.Bounds.Top<>A.Bounds.Top+DY) or (B.Bounds.Bottom<>A.Bounds.Bottom+DY) then begin
   Inc(Errors);
   WriteLn('PANDETAIL ',I,' ',A.Text,' pos ',A.Position.X,',',A.Position.Y,' -> ',B.Position.X,',',B.Position.Y,
     ' bounds ',A.Bounds.Left,',',A.Bounds.Top,',',A.Bounds.Right,',',A.Bounds.Bottom,
     ' -> ',B.Bounds.Left,',',B.Bounds.Top,',',B.Bounds.Right,',',B.Bounds.Bottom,' delta ',DX,',',DY);
  end;
 end;
 SavePlacements;
end;
finalization
 FillImage.Free;
end.
