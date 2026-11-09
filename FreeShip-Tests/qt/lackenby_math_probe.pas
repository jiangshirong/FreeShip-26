{ Retained RTL numerical diagnostic for native Lackenby, GPL-2.0-or-later.
  Compile with the bundled Win64 FPC 3.2.2. Output is the exact IEEE binary64
  sequence used by LackenbyEngine's bounded convergence acceleration. }
program LackenbyMathProbe;
{$mode objfpc}{$H+}
uses SysUtils,Math;
function SimpsonA(Dist,Fie,SF:Single):Single;
begin Result:=SF+0.5*Dist*((3*Fie-1)/Fie) end;
function SimpsonB(Dist,Fie,SF:Single):Single;
begin Result:=SF+0.5*Dist*(1/(Fie*(1-Fie))) end;
function SimpsonC(Dist,Fie,SF:Single):Single;
begin Result:=SF+0.5*Dist*((2-3*Fie)/(1-Fie)) end;
var I:Integer;Value:Double;Bits:QWord;
begin
  Value:=2.5;
  for I:=1 to 100 do begin
    Value:=Power(Value,0.9);Move(Value,Bits,SizeOf(Bits));
    WriteLn(I,' ',IntToHex(Bits,16));
  end;
  if ParamCount>0 then WriteLn(SimpsonA(0.2,0.45,0.1),' ',SimpsonB(0.2,0.45,0.1),' ',SimpsonC(0.2,0.45,0.1));
end.
