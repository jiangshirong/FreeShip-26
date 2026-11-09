program LabelRevealProbe;
{$mode objfpc}{$H+}
uses SysUtils,Types,FreeLabelLayout;
var L:TFreeLabelLayout;P:TLabelPlacement;K:TLabelKind;I,First,FinalWidth,Prior:Integer;
procedure Check(OK:Boolean;const Message:string);
begin if not OK then begin WriteLn('FAIL ',Message);Halt(1) end;WriteLn('PASS ',Message) end;
begin
 for K:=Low(TLabelKind) to High(TLabelKind) do begin
  L:=TFreeLabelLayout.Create;L.AnimationEnabled:=True;
  L.BeginFrame(Rect(0,0,400,300),'reveal');P:=L.Place('number','286.08',Point(180,150),Point(180,150),60,16,K);L.EndFrame;
  First:=P.Rect.Right-P.Rect.Left;Check(P.Visible and (First>0) and (First<60),'all kinds start with partial glyph width '+IntToStr(Ord(K)));
  Prior:=First;
  for I:=1 to 40 do begin Sleep(10);L.BeginFrame(Rect(0,0,400,300),'reveal');P:=L.Place('number','286.08',Point(180,150),Point(180,150),60,16,K);L.EndFrame;
    FinalWidth:=P.Rect.Right-P.Rect.Left;Check(FinalWidth>=Prior,'stable reveal never jumps backward');Prior:=FinalWidth end;
  Check(FinalWidth=60,'full text eventually shown');
  L.AnimationEnabled:=False;L.BeginFrame(Rect(0,0,400,300),'reveal');P:=L.Place('other','12345',Point(30,30),Point(30,30),60,16,K);L.EndFrame;
  Check(P.Visible and (P.Rect.Right-P.Rect.Left=60),'animation disabled keeps complete text');
  L.Free;
 end;
end.
