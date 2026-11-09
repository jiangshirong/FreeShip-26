program test_layout;
{$mode objfpc}{$H+}
uses SysUtils, Types, FreeLabelLayout;
var l:TFreeLabelLayout; a,b,c:TLabelPlacement; first:TPoint; i:integer;
procedure Check(ok:boolean; const name:string);
begin if not ok then begin WriteLn('FAIL ',name); Halt(1) end; WriteLn('PASS ',name) end;
function overlaps(const a,b:TRect):boolean;
begin Result:=(a.Left<b.Right) and (a.Right>b.Left) and (a.Top<b.Bottom) and (a.Bottom>b.Top) end;
begin
 Check(TFreeLabelLayout.SegmentHitsRect(Rect(10,10,30,30),Point(0,20),Point(40,20)), 'crossing segment');
 Check(TFreeLabelLayout.SegmentHitsRect(Rect(10,10,30,30),Point(0,10),Point(40,10)), 'collinear boundary');
 Check(not TFreeLabelLayout.SegmentHitsRect(Rect(10,10,30,30),Point(0,9),Point(40,9)), 'parallel outside');
 Check(TFreeLabelLayout.SegmentHitsRect(Rect(10,10,30,30),Point(20,20),Point(20,20)), 'zero length inside');
 Check(not TFreeLabelLayout.SegmentHitsRect(Rect(10,10,30,30),Point(9,9),Point(9,9)), 'zero length outside');
 l:=TFreeLabelLayout.Create;
 try
  l.BeginFrame(Rect(0,0,400,300)); l.Reserve(Rect(145,95,156,106));
  a:=l.Place('a','A',Point(150,100),Point(158,103),40,20,lkPoint);
  b:=l.Place('b','B',Point(150,100),Point(158,103),40,20,lkPoint);
  Check(a.Visible and b.Visible and not overlaps(a.Rect,b.Rect), 'coincident points have separate labels');
  Check((abs(a.Position.X-150)<=70) and (abs(a.Position.Y-100)<=50), 'point remains near anchor');
  first:=Point(a.Position.X-150,a.Position.Y-100);
  l.BeginFrame(Rect(0,0,400,300)); l.Reserve(Rect(146,96,157,107));
  a:=l.Place('a','A',Point(151,101),Point(159,104),40,20,lkPoint);
  Check((a.Position.X-151=first.X) and (a.Position.Y-101=first.Y), 'relative offset stable on anchor movement');
  l.BeginFrame(Rect(0,0,400,300));
  a:=l.Place('right','right',Point(398,100),Point(406,103),60,20,lkPoint);
  Check(a.Visible and (a.Rect.Left>=0) and (a.Rect.Right<=400), 'right edge point remains visible nearby');
  l.BeginFrame(Rect(0,0,400,300)); l.Reserve(Rect(95,90,135,115));
  a:=l.Place('tick','12',Point(100,100),Point(100,100),25,12,lkVerticalTick);
  Check(a.Visible and (a.Position.X=100), 'vertical tick moves only along its line');
  l.BeginFrame(Rect(0,0,400,300)); l.Reserve(Rect(95,100,135,115));
  a:=l.Place('tick-out','12',Point(100,100),Point(100,100),25,12,lkVerticalTick,-1);
  Check(a.Visible and (a.Position.X=100) and (a.Position.Y<100), 'tick moves outward along its own line');
  l.BeginFrame(Rect(0,0,400,300)); l.Reserve(Rect(0,0,400,300));
  a:=l.Place('tick2','13',Point(100,100),Point(100,100),25,12,lkVerticalTick);
  Check(not a.Visible, 'unplaceable tick is omitted rather than misassigned');
  a:=l.Place('named','Baseline',Point(100,100),Point(100,100),55,12,lkNamedLine);
  Check(not a.Visible, 'unplaceable name is omitted rather than overlapping');
  l.BeginFrame(Rect(0,0,600,400)); l.Reserve(Rect(0,0,350,400));
  a:=l.Place('fallback','42',Point(100,100),Point(108,103),40,20,lkPoint);
  Check(a.Visible and (a.Rect.Left>=350), 'distant candidate retains point label');
  l.BeginFrame(Rect(0,0,400,300));
  l.AddSegment(Point(100,80),Point(100,160));
  a:=l.Place('line','point',Point(100,110),Point(100,110),45,15,lkPoint);
  Check(a.Visible and (a.Collisions=0) and not TFreeLabelLayout.SegmentHitsRect(a.Rect,Point(100,80),Point(100,160)), 'point label avoids segment');
 finally l.Free end;
end.
