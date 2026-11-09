{ GPL-2.0-or-later. Exact constant provenance: Units/FreeGeometry.pas and
  FreeProjectSettings.FSetProjectUnits. No project or application is loaded. }
program project_units_math;
{$mode objfpc}{$H+}
uses SysUtils;
const Foot=0.3048;PoundKilograms=0.45359237;MetricTonneKilograms=1000.0;LongTonPounds=2240.0;
  DensityConversionFactor=MetricTonneKilograms*Foot*Foot*Foot/PoundKilograms;
  MetricTonneToLongTon=MetricTonneKilograms/(LongTonPounds*PoundKilograms);
procedure Value(const Name:string;D:Double);
var Bits:QWord;
begin Move(D,Bits,8);WriteLn(Name,' ',IntToHex(Bits,16));end;
procedure Threshold(const Name:string;Val,Old:Single);
begin WriteLn(Name,' ',Ord(Abs(Val-Old)>1e-7),Ord(Abs(Val-Old)>1e-6));end;
begin
  Value('metric_length',Foot);Value('imperial_length',1/Foot);
  Value('imperial_density',DensityConversionFactor);Value('metric_density',1/DensityConversionFactor);
  Value('imperial_mass',MetricTonneToLongTon);Value('metric_mass',1/MetricTonneToLongTon);
  Value('metric_thickness',25.4);Value('imperial_thickness',1/25.4);
  Threshold('threshold_dimension',1e-7,0);
  Threshold('threshold_density',1e-6,0);
end.
