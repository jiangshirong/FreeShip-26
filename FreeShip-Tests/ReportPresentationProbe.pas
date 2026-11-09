program ReportPresentationProbe;
{$mode objfpc}{$H+}
uses Classes,Interfaces,Forms,StdCtrls,SysUtils,LCLIntf,FreeReportPresentation,FreeGeometry;
var F:TForm;M:TMemo;I,P,N,X,First,FirstValue:Integer;S:string;Plain:TStringList;
procedure Check(B:Boolean;const Name:string);
begin if not B then begin WriteLn('FAIL ',Name);Halt(1) end;WriteLn('PASS ',Name) end;
begin
 Application.Initialize;
 Check(ReportUnitPowers('m^2 m^3 m^4 t/m^3')='m² m³ m⁴ t/m³','Unicode unit powers');
 Check(AreaStr(fuMetric)='m²','metric area getter');
 Check(VolStr(fuMetric)='m³','metric volume getter');
 Check(InertiaStr(fuMetric)='m⁴','metric inertia getter');
 Check(DensityStr(fuMetric)='t/m³','metric density getter');
 F:=TForm.CreateNew(nil);F.SetBounds(0,0,1500,600);M:=TMemo.Create(F);M.Parent:=F;M.SetBounds(0,0,1400,500);M.Font.Name:='SimHei';M.Font.Size:=11;
 M.Lines.Add('工程                : T2');
 M.Lines.Add('从船首 (FP) 到受风面积重心的距离 : 95.559 米');
 M.Lines.Add('Продольный центр величины : 97.635 м');
 M.Lines.Add('面积 : 4331.7 m^2');
 PrepareReportMemo(M);First:=-1;FirstValue:=-1;
 for I:=0 to M.Lines.Count-1 do begin
  S:=M.Lines[I];P:=Pos(#9,S);Check(P>0,'property tab '+IntToStr(I));
  N:=SendMessage(M.Handle,$00BB,I,0)+Length(UTF8Decode(Copy(S,1,P-1)))+1;
  X:=SmallInt(SendMessage(M.Handle,$00D6,N,0) and $FFFF);
  WriteLn('COLON_X ',I,' ',X);
  if First<0 then First:=X;
  Check((X>=0) and (X=First),'native colon alignment '+IntToStr(I));
  X:=SmallInt(SendMessage(M.Handle,$00D6,N+2,0) and $FFFF);
  if FirstValue<0 then FirstValue:=X;
  Check((X>First) and (X=FirstValue),'native value alignment '+IntToStr(I));
 end;
 Check(Pos('95.559',M.Lines.Text)>0,'numeric value preserved');
 Check(Pos('m²',M.Lines.Text)>0,'memo superscript');
 Plain:=ReportPlainText(M.Lines);
 try
  Check(Pos(#9,Plain.Text)=0,'plain export has no custom tab stops');
  Check(Pos('95.559',Plain.Text)>0,'plain export preserves number');
  Check(Pos('m²',Plain.Text)>0,'plain export superscript');
 finally Plain.Free end;F.Free;
end.
