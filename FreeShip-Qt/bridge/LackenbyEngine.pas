{ Headless adaptation of FreeLackenbyDlg.pas, GPL-2.0-or-later.
  Same 40-interval half-body integration and iteration. UI, private undo and
  automatic unlocking removed. Service transaction owns rollback. }
unit LackenbyEngine;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,Math,FreeTypes,FreeGeometry,FreeShipUnit;
const NStations=40;MaxLCBError=5e-5;MaxDisplError=1e-4;
type
  TSimpsonData=record Area,Distance,SF:TFloatType end;
  TBodyProp=record Displacement,LCB,Cp,Length,k,p,z,A,B,C,dCp,dp:TFloatType end;
  TLackenbyEngine=class
  private
    Ship:TFreeShip;FAftShip,FForeShip:TFasterListTFreeIntersection;
    FMainArea:TFloatType;FMin,FMax:T3DCoordinate;FWaterlinePlane:T3DPlane;
    procedure CalculatePart(Wlplane:T3DPlane;MainArea:TFloatType;Stations:TFasterListTFreeIntersection;var Prop:TBodyProp;SAC:TFreeSpline);
  public
    MainLoc:TFloatType;
    constructor Create(Owner:TFreeShip);
    destructor Destroy;override;
    function Measure:TBodyProp;
    function Transform(Points:TFasterListTFreeSubdivisionControlPoint;NewDispl,TargetLCB:TFloatType;MaxIterations:Integer):Boolean;
  end;
implementation
procedure TLackenbyEngine.CalculatePart(Wlplane: T3DPlane;
  MainArea: TFloatType; Stations: TFasterListTFreeIntersection;
  var Prop: TBodyProp; SAC: TFreeSpline);
var
  I, N: integer;
  Station: TFreeIntersection;
  SimpsonData: array of TSimpsonData;
  Area, Prod: TFloatType;
  COG: T3DCoordinate;
  Mom: T2DCoordinate;
  Dist, fie, Y: TFloatType;
begin
  N := Stations.Count;
  Setlength(SimpsonData, N);
  Fillchar(prop, SizeOf(Prop), 0);
  for I := 1 to length(SimpsonData) do
  begin
    Simpsondata[I - 1].Area := 0.0;
    Simpsondata[I - 1].SF := 0.0;
    Simpsondata[I - 1].Distance := 0.0;
  end;
  for I := 1 to N do
  begin
    Station := Stations[I - 1];
    Station.Rebuild;
    Station.CalculateArea(Wlplane, Area, COG, Mom);
    SimpsonData[I - 1].Area := Area;
    SimpsonData[I - 1].Distance := COG.X;
  end;

  I := 0;
  while I < N - 1 do
  begin
    Dist := SimpsonData[I + 2].Distance - SimpsonData[I].Distance;
    if abs(Dist) > 1e-6 then
    begin
      fie := (SimpsonData[I + 1].Distance - SimpsonData[I].Distance) / Dist;
      if abs(Fie) < 1e-2 then
      begin
        if Fie < 0 then
          Fie := -1e-2
        else
          Fie := 1e-2;
      end
      else if abs(1 - Fie) < 1e-2 then
        Fie := 1 - 1e-2;
      SimpsonData[I].SF := SimpsonData[I].SF + 0.5 * Dist * ((3 * fie - 1) / fie);
      SimpsonData[I + 1].SF := SimpsonData[I + 1].SF + 0.5 * Dist * (1 / (fie * (1 - fie)));
      SimpsonData[I + 2].SF := SimpsonData[I + 2].SF + 0.5 * Dist * ((2 - 3 * fie) / (1 - fie));
    end;
    Inc(I, 2);
  end;

  y := 1; // presentation scale is irrelevant to integration
  for I := 1 to N do
  begin
    Prod := SimpsonData[I - 1].SF * SimpsonData[I - 1].Area;
    if SAC <> nil then
      if FMainArea <> 0 then
        SAC.Add(SetPoint(SimpsonData[I - 1].Distance, y * SimpsonData[I - 1].Area / FMainArea, 0.0))
      else
        SAC.Add(SetPoint(SimpsonData[I - 1].Distance, y * SimpsonData[I - 1].Area, 0.0));
    Prop.Displacement := Prop.Displacement + Prod;
    Prop.LCB := prop.LCB + Prod * SimpsonData[I - 1].Distance;
    Prop.k := Prop.k + Prod * SimpsonData[I - 1].Distance * SimpsonData[I - 1].Distance;
  end;

  Prop.Displacement := Prop.Displacement / 3;
  if Prop.Displacement <> 0 then
  begin
    Prop.LCB := prop.LCB / (3 * Prop.Displacement);
    Prop.k := Sqrt(Prop.k / (3 * Prop.Displacement));
  end
  else
  begin
    Prop.Lcb := 0.0;
    Prop.k := 0.0;
  end;

  Prop.Length := SimpsonData[N - 1].Distance - SimpsonData[0].Distance;
  if (MainArea <> 0) and (Prop.Length <> 0) then
    Prop.Cp := Prop.Displacement / (Prop.Length * Mainarea)
  else
    Prop.Cp := 0.0;
  Stations.Capacity := Stations.Capacity;
end;{TLackenbyEngine.CalculatePart}

constructor TLackenbyEngine.Create(Owner:TFreeShip);
var I:Integer;Station:TFreeIntersection;Plane:T3DPlane;P:T3DCoordinate;Mom:T2DCoordinate;Value:TFloatType;
begin
  inherited Create;Ship:=Owner;FAftShip:=TFasterListTFreeIntersection.Create;FForeShip:=TFasterListTFreeIntersection.Create;
  if Ship.ProjectSettings.ProjectDraft<=0 then raise Exception.Create('Set a positive design draft first');
  FWaterlinePlane:=SetPlane(0,0,1,-(Ship.FindLowestHydrostaticsPoint+Ship.ProjectSettings.ProjectDraft));
  Ship.SubmergedHullExtents(FWaterlinePlane,FMin,FMax);MainLoc:=Ship.ProjectSettings.ProjectSplitSectionLocation;
  if (MainLoc<=FMin.X+1e-6) or (MainLoc>=FMax.X-1e-6) or (FMax.Y-FMin.Y<=1e-6) then raise Exception.Create('Midship must be inside the submerged hull');
  Station:=TFreeIntersection.Create(Ship);
  try
    Station.IntersectionType:=fiStation;Station.UseHydrostaticsSurfacesOnly:=True;Station.Plane:=SetPlane(1,0,0,-MainLoc);
    Station.CalculateArea(FWaterlinePlane,FMainArea,P,Mom);
  finally Station.Free end;
  if FMainArea<=1e-8 then raise Exception.Create('No usable submerged midship area');
  for I:=0 to NStations do begin
    Value:=FMin.X+(I/NStations)*(MainLoc-FMin.X);
    Station:=TFreeIntersection.Create(Ship);FAftShip.Add(Station);Station.IntersectionType:=fiStation;
    Station.UseHydrostaticsSurfacesOnly:=True;Station.Plane:=SetPlane(1,0,0,-Value);
    Value:=MainLoc+(I/NStations)*(FMax.X-MainLoc);
    Station:=TFreeIntersection.Create(Ship);FForeShip.Add(Station);Station.IntersectionType:=fiStation;
    Station.UseHydrostaticsSurfacesOnly:=True;Station.Plane:=SetPlane(1,0,0,-Value);
  end;
end;
destructor TLackenbyEngine.Destroy;
var I:Integer;
begin
  if FAftShip<>nil then begin for I:=0 to FAftShip.Count-1 do FAftShip[I].Free;FAftShip.Free end;
  if FForeShip<>nil then begin for I:=0 to FForeShip.Count-1 do FForeShip[I].Free;FForeShip.Free end;
  inherited Destroy;
end;
function TLackenbyEngine.Measure:TBodyProp;
var Aft,Fore:TBodyProp;
begin
  CalculatePart(FWaterlinePlane,FMainArea,FAftShip,Aft,nil);CalculatePart(FWaterlinePlane,FMainArea,FForeShip,Fore,nil);
  FillChar(Result,SizeOf(Result),0);Result.Displacement:=Aft.Displacement+Fore.Displacement;Result.Length:=Aft.Length+Fore.Length;
  if (Result.Displacement<=0) or (Result.Length<=0) then raise Exception.Create('No usable submerged volume');
  Result.LCB:=(Aft.Displacement*Aft.LCB+Fore.Displacement*Fore.LCB)/Result.Displacement;
  Result.Cp:=Result.Displacement/(Result.Length*FMainArea);
end;
function TLackenbyEngine.Transform(Points:TFasterListTFreeSubdivisionControlPoint;NewDispl,TargetLCB:TFloatType;MaxIterations:Integer):Boolean;
var Point:TFreeSubdivisionControlPoint;P3D:T3DCoordinate;Iteration,I:Integer;
    AftProperties,ForeProperties,TotalProp,DesiredData:TBodyProp;
    dz,x,dx,DisplError,LCBError:TFloatType;Modified:Boolean;ConvFactor:Double;
  procedure InitializeData(var Prop: TBodyProp);
  begin
    Prop.A := Prop.Cp * (1 - 2 * Prop.LCB) - Prop.p * (1 - Prop.Cp);
    if Abs(Prop.A)<1e-10 then raise Exception.Create('Lackenby transformation is singular for this hull');
    Prop.B := (Prop.Cp * (2 * Prop.LCB - 3 * Prop.k * Prop.k - Prop.p * (1 - 2 * Prop.LCB)) / Prop.A);
    Prop.C := (Prop.B * (1 - Prop.Cp) - Prop.Cp * (1 - 2 * Prop.LCB)) / (1 - Prop.P);
  end;{InitializeData}

begin
  if (Points.Count=0) or (NewDispl<=0) or (MaxIterations<1) or (MaxIterations>100) then raise Exception.Create('Invalid Lackenby parameters');
  if (TargetLCB<=FMin.X) or (TargetLCB>=FMax.X) then raise Exception.Create('Target LCB must be inside the submerged hull');
  Iteration:=1;Modified:=False;DisplError:=1;LCBError:=1;ConvFactor:=2.5;
    while ((DisplError > MaxDisplError) or (LCBError > MaxLCBError)) and
      (Iteration <= MaxIterations) do
    begin
      // Calculate properties of aftship
      Fillchar(AftProperties, SizeOf(AftProperties), 0);
      ForeProperties := Aftproperties;
      TotalProp := Aftproperties;
      ;
      DesiredData := Aftproperties;
      ;
      CalculatePart(FWaterlinePlane, FMainArea,
        FAftShip, AftProperties, nil);
      CalculatePart(FWaterlinePlane, FMainArea,
        FForeShip, ForeProperties, nil);
      // perform a check
      TotalProp.Displacement := AftProperties.Displacement + ForeProperties.Displacement;
      if TotalProp.Displacement > 0 then
      begin
        TotalProp.LCB := (AftProperties.Displacement * AftProperties.LCB +
          ForeProperties.Displacement * ForeProperties.LCB) / TotalProp.Displacement;
        TotalProp.Length := AftProperties.Length + ForeProperties.Length;
        TotalProp.Cp := TotalProp.Displacement / (Totalprop.Length * FMainArea);
        ////////////////// TEMPORARILY SET TO ZERO!!!!!!!!!
        AftProperties.p := 0;
        ForeProperties.p := 0;
        AftProperties.dp := 0;
        ForeProperties.dp := 0;
        DesiredData := TotalProp;
        DesiredData.Displacement := NewDispl;
        DesiredData.Cp := DesiredData.Displacement / (DesiredData.Length * FMainArea);
        Desireddata.LCB := TargetLCB;
        DisplError := abs((DesiredData.Displacement - TotalProp.Displacement) /
          DesiredData.Displacement);
        if abs(Desireddata.LCB - TotalProp.LCB) <= 2e-3 then
          LCBError := MaxLCBError - 1e-6
        else
          LCBError := abs((Desireddata.LCB - TotalProp.LCB) / Totalprop.Length);
        if (DisplError <= MaxDisplError) and (LCBError <= MaxLCBError) then
        begin
          // Job is done, interrupt
          Iteration := Iteration + 1 - 1;
        end
        else
        begin
          // Make all data dimensionless
          with AftProperties do
          begin
            Displacement :=
              Displacement / (Length * (FMax.Y - FMin.Y) * Ship.ProjectSettings.ProjectDraft);
            LCB := (MainLoc - LCB) / Length;
            k := (MainLoc - k) / Length;
          end;
          with ForeProperties do
          begin
            Displacement :=
              Displacement / (Length * (FMax.Y - FMin.Y) * Ship.ProjectSettings.ProjectDraft);
            LCB := (LCB - MainLoc) / Length;
            k := (k - MainLoc) / Length;
          end;
          with Desireddata do
          begin
            Displacement :=
              Displacement / (Length * (FMax.Y - FMin.Y) * Ship.ProjectSettings.ProjectDraft);
            LCB := (LCB-FMin.X) / Length;
            dCp := Cp - TotalProp.Cp;
          end;
          if (AftProperties.Length<=0) or (ForeProperties.Length<=0) then raise Exception.Create('Invalid underwater half-body length');
          InitializeData(AftProperties);
          InitializeData(ForeProperties);
          TotalProp.z := (TotalProp.LCB - FMin.X - 0.5 * TotalProp.Length) / (0.5 * TotalProp.Length);
          DesiredData.z := (DesiredData.LCB - 0.5) / 0.5;
          // Fraction of Half length of total ship
          dz := DesiredData.z - TotalProp.z;
          ConvFactor := power(ConvFactor, 0.9);
          dz := ConvFactor * dz;// multiply for faster convergence of iteration process
          if Abs(ForeProperties.B+AftProperties.B)<1e-10 then raise Exception.Create('Lackenby transformation is singular for this hull');
          AftProperties.dCp :=
            (2 * (DesiredData.dCp * (ForeProperties.B - TotalProp.z) - dz *
            (Totalprop.Cp + DesiredData.dCp)) - ForeProperties.C * ForeProperties.dp +
            AftProperties.C * AftProperties.dp) / (Foreproperties.B + Aftproperties.B);
          ForeProperties.dCp :=
            (2 * (DesiredData.dCp * (AftProperties.B + TotalProp.z) + dz *
            (Totalprop.Cp + DesiredData.dCp)) + ForeProperties.C * ForeProperties.dp -
            AftProperties.C * AftProperties.dp) / (Foreproperties.B + Aftproperties.B);

          for I := 1 to Points.Count do
          begin
            Point := Points[I - 1];
            if Point.Coordinate.X < MainLoc then
            begin
              // Point is part of the aftship
              // make dimensionless
              P3D := Point.Coordinate;
              x := (MainLoc - P3D.X) / AftProperties.Length;
              dx := (1 - x) * (AftProperties.dp / (1 - AftProperties.p) +
                (x - AftProperties.p) / Aftproperties.A * (Aftproperties.dCp - Aftproperties.dp *
                (1 - Aftproperties.Cp) / (1 - Aftproperties.p)));
              x := x + dX;
              P3D.X := MainLoc - x * AftProperties.Length;
              if IsNan(P3D.X) or IsInfinite(P3D.X) or (Abs(P3D.X)>1e9) then raise Exception.Create('Lackenby produced an invalid coordinate');
              Point.Coordinate := P3D;
            end
            else
            begin
              // Point is part of the foreship
              // make dimensionless
              P3D := Point.Coordinate;
              x := (P3D.X - MainLoc) / ForeProperties.Length;
              dx := (1 - x) * (ForeProperties.dp / (1 - ForeProperties.p) +
                (x - ForeProperties.p) / Foreproperties.A * (Foreproperties.dCp - Foreproperties.dp *
                (1 - Foreproperties.Cp) / (1 - Foreproperties.p)));
              x := x + dX;
              P3D.X := x * ForeProperties.Length + Mainloc;
              if IsNan(P3D.X) or IsInfinite(P3D.X) or (Abs(P3D.X)>1e9) then raise Exception.Create('Lackenby produced an invalid coordinate');
              Point.Coordinate := P3D;
            end;
          end;
          Modified := True;
          Ship.Built := False;
          Ship.RebuildModel;
          for I:=0 to FAftShip.Count-1 do FAftShip[I].Built:=False;
          for I:=0 to FForeShip.Count-1 do FForeShip[I].Built:=False;
          Inc(Iteration);

        end;
      end
      else
        Inc(Iteration);
    end;
  if (DisplError>MaxDisplError) or (LCBError>MaxLCBError) or (Iteration>MaxIterations) then
    raise Exception.Create('Lackenby transformation did not converge; all changes have been discarded');
  Result:=Modified;
end;
end.
