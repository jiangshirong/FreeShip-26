{ Retained compiler-semantics probe. No model or application state is touched. }
program resistance_math;
{$mode objfpc}{$H+}
uses SysUtils,Math;
var Speed,ConvertedSpeed,Viscosity,WaterViscosity,Rn,Cf,Rf,Density,Wet,Len:Single;I:Integer;
procedure Value(const Name:string;V:Single);
var Bits:LongWord;
begin Move(V,Bits,4);Write(Name,'=',IntToHex(Bits,8),' ');end;
begin
 WriteLn('sqrt=',SizeOf(Sqrt(Speed)),' sin=',SizeOf(Sin(Speed)),' cos=',SizeOf(Cos(Speed)),
  ' tan=',SizeOf(Tan(Speed)),' arctan=',SizeOf(ArcTan(Speed)),' ln=',SizeOf(Ln(Speed)),
  'log10=',SizeOf(Log10(Speed)),' power=',SizeOf(Power(Speed,0.5)));
 Len:=10;Viscosity:=1.189;Density:=1.025;Wet:=22;
 for I:=0 to 10 do begin
  Speed:=2+I*0.5;ConvertedSpeed:=Speed*1852/3600;
  WaterViscosity:=Viscosity*1e-6;
  Rn:=(ConvertedSpeed*0.7*Len)/WaterViscosity;
  Cf:=0.075/sqr(LOG10(Rn)-2);
  Rf:=Cf*0.5*1000*Density*sqr(ConvertedSpeed)*Wet;
  Value('speed',ConvertedSpeed);Value('nu',WaterViscosity);Value('rn',Rn);Value('cf',Cf);Value('rf',Rf);WriteLn;
 end;
end.
