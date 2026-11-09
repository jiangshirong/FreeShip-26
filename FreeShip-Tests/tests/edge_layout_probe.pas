program EdgeLayoutProbe;
{$mode objfpc}{$H+}
uses SysUtils, Types, Math, FreeLabelLayout;
var Layout:TFreeLabelLayout; P,Q:TLabelPlacement; I,J:Integer; A,Pos0,Shift:TPoint; B:TRect;
    Edges:array[0..3] of TPoint=((X:788;Y:100),(X:12;Y:100),(X:400;Y:12),(X:400;Y:490));
procedure Require(Condition:Boolean; const MessageText:string);
begin if not Condition then begin WriteLn('FAIL ',MessageText); Halt(1) end end;
function Overlap(const X,Y:TRect):Boolean;
begin Result:=(X.Left<Y.Right) and (X.Right>Y.Left) and (X.Top<Y.Bottom) and (X.Bottom>Y.Top) end;
begin
 B:=Rect(0,0,800,500);
 Shift:=TFreeLabelLayout.EdgeAttraction(B,Rect(760,100,790,120)); Require(Shift.X>0,'right edge attraction');
 Shift:=TFreeLabelLayout.EdgeAttraction(B,Rect(10,100,40,120)); Require(Shift.X<0,'left edge attraction');
 Shift:=TFreeLabelLayout.EdgeAttraction(B,Rect(100,10,140,30)); Require(Shift.Y<0,'top edge attraction');
 Shift:=TFreeLabelLayout.EdgeAttraction(B,Rect(100,470,140,490)); Require(Shift.Y>0,'bottom edge attraction');
 WriteLn('PASS edge attraction direction');
 for I:=0 to 3 do begin
   Layout:=TFreeLabelLayout.Create;
   try
     for J:=0 to 25 do begin
       if J>0 then Sleep(10);
       Layout.BeginFrame(B,'edge'); A:=Edges[I];
       P:=Layout.Place('first','20.000',A,Point(A.X+7,A.Y+9),43,20,lkPoint);
       Require(P.Visible,'edge point disappears '+IntToStr(I)+' frame '+IntToStr(J));
       Require((P.Rect.Left<P.Rect.Right) and (P.Rect.Top<P.Rect.Bottom),'nonempty clip');
       if (I=0) or (I=3) then Require((P.Rect.Right-P.Rect.Left=43) and (P.Rect.Bottom-P.Rect.Top=20),'vanishing candidate recovers full label');
       Q:=Layout.Place('second','3.000',A,Point(A.X+7,A.Y+9),35,20,lkPoint);
       if not Q.Visible or Overlap(P.Rect,Q.Rect) then WriteLn('DETAIL edge=',I,' frame=',J,' first=',P.Rect.Left,',',P.Rect.Top,',',P.Rect.Right,',',P.Rect.Bottom,' second-visible=',Q.Visible,' rect=',Q.Rect.Left,',',Q.Rect.Top,',',Q.Rect.Right,',',Q.Rect.Bottom);
       Require(Q.Visible and not Overlap(P.Rect,Q.Rect),'coincident edge labels '+IntToStr(I));
       Layout.EndFrame;
     end;
   finally Layout.Free end;
 end;
 WriteLn('PASS visible and nonoverlapping at four edges across frames');
 Layout:=TFreeLabelLayout.Create;
 try
   Layout.BeginFrame(B,'intentional-edge-clip'); A:=Point(1,100);
   P:=Layout.Place('clip','20.000',A,Point(0,109),43,20,lkPoint); Layout.EndFrame;
   Require(P.Visible and (P.Position.X<0) and (P.Rect.Left=0) and (P.Rect.Right-P.Rect.Left<43),
     'existing partial edge clipping remains available');
   WriteLn('PASS existing partial edge clipping');
 finally Layout.Free end;
 Layout:=TFreeLabelLayout.Create;
 try
   Layout.BeginFrame(B,'movement'); A:=Point(730,250);
   P:=Layout.Place('moving','20.000',A,Point(A.X+7,A.Y+9),43,20,lkPoint); Layout.EndFrame;
   Require(P.Visible,'initial moving point');
   for I:=1 to 100 do begin
     Sleep(12); Pos0:=P.Position;
     if I<=50 then A.X:=730+I else A.X:=830-I;
     Layout.BeginFrame(B,'movement');
     P:=Layout.Place('moving','20.000',A,Point(A.X+7,A.Y+9),43,20,lkPoint); Layout.EndFrame;
     Require(P.Visible,'moving point disappears frame '+IntToStr(I));
     Require(Hypot(P.Position.X-Pos0.X,P.Position.Y-Pos0.Y)<=18,'moving point jumps frame '+IntToStr(I));
   end;
   for I:=1 to 100 do begin
     Sleep(12); Layout.BeginFrame(B,'movement');
     P:=Layout.Place('moving','20.000',A,Point(A.X+7,A.Y+9),43,20,lkPoint); Layout.EndFrame;
     Require(P.Visible,'settling point disappears');
   end;
   Require(not Layout.Animating,'moving point animation settles');
   WriteLn('PASS bounded continuous edge movement and settling');
 finally Layout.Free end;
 Layout:=TFreeLabelLayout.Create;
 try
   Q.Visible:=False;
   for I:=0 to 180 do begin
     Sleep(12);
     if I<90 then A:=Point(720+I*2,200) else A:=Point(1080-I*2,200);
     Layout.BeginFrame(B,'named-exit-return');
     P:=Layout.Place('waterline','DWL 10850',A,A,80,20,lkNamedLine);Layout.EndFrame;
     if P.Visible then begin
       if not Q.Visible then Require(P.Rect.Right-P.Rect.Left<=4,'reappearing name must open progressively');
       if Q.Visible then Require(Hypot(P.Position.X-Q.Position.X,P.Position.Y-Q.Position.Y)<=10,'named label jumps across boundary');
     end;
     Q:=P;
   end;
   WriteLn('PASS named label offscreen roundtrip without full-width pop');
 finally Layout.Free end;
 Layout:=TFreeLabelLayout.Create;
 try
   Layout.AnimationEnabled:=False;
   Layout.BeginFrame(B,'reference-value');
   P:=Layout.Place('dwl','DWL 10850',Point(100,100),Point(100,100),90,20,lkNamedLine);
   Require(P.Visible,'reference name visible');
   Q:=Layout.Place('near','10850',Point(100,100),Point(100,100),40,20,lkHorizontalTick);
   Require(not Q.Visible,'DWL replaces same nearby numeric tick');
   Q:=Layout.Place('far','10850',Point(600,100),Point(600,100),40,20,lkHorizontalTick);
   Require(Q.Visible,'opposite endpoint value retained');
   Q:=Layout.Place('other','8680',Point(100,160),Point(100,160),40,20,lkHorizontalTick);
   Require(Q.Visible,'different value retained');Layout.EndFrame;
   Layout.BeginFrame(B,'baseline-value');
   P:=Layout.Place('base','Base 0',Point(100,300),Point(100,300),55,20,lkNamedLine);
   Q:=Layout.Place('zero','0',Point(100,300),Point(100,300),10,20,lkHorizontalTick);
   Require(P.Visible and not Q.Visible,'baseline replaces nearby zero');Layout.EndFrame;
   Layout.BeginFrame(B,'reference-hidden');
   P:=Layout.Place('hidden','DWL 10850',Point(-300,100),Point(-300,100),90,20,lkNamedLine);
   Q:=Layout.Place('available','10850',Point(100,100),Point(100,100),40,20,lkHorizontalTick);
   Require(not P.Visible and Q.Visible,'hidden name does not suppress ordinary tick');Layout.EndFrame;
   WriteLn('PASS semantic reference-value deduplication');
 finally Layout.Free end;
 Layout:=TFreeLabelLayout.Create;
 try
   Layout.BeginFrame(B,'coordinate-dedup');
   Require(Layout.ClaimCoordinateLabel(Point(100,100),'0.000'),'selected-first claim owns projection/value');
   Require(not Layout.ClaimCoordinateLabel(Point(100,100),'0.000'),'same projection and value deduplicated');
   Require(Layout.ClaimCoordinateLabel(Point(100,100),'1.000'),'different value at same projection retained');
   Require(Layout.ClaimCoordinateLabel(Point(101,100),'0.000'),'different pixel position retained');
   Layout.EndFrame;Layout.BeginFrame(B,'coordinate-dedup');
   Require(Layout.ClaimCoordinateLabel(Point(100,100),'0.000'),'new frame permits new selected representative');
   Layout.EndFrame;
   WriteLn('PASS projected coordinate value deduplication');
 finally Layout.Free end;
end.
