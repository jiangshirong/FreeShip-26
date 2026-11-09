"""One-off issue 17 diagnostic: split settings witnesses around a native round trip.

Uses an isolated source build and temporary model copies. This collects evidence,
always reports review, and does not weaken the production or acceptance checks.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import freeship_ui_test as common
import file_safety
import legacy_compatibility

ROOT = Path(__file__).resolve().parent
HELPER = r'''
procedure SaveSettingsPieces(Ship:TFreeShip; const Prefix:string);
var B:TFreeFileBuffer; I:Integer;
  procedure Flush(const Name:string);
  begin
    if not B.SaveToFile(Prefix+Name+'.bin') then raise Exception.Create('settings piece write failed');
    B.Free; B:=nil; B:=TFreeFileBuffer.Create;
    B.Version:=Ship.FileVersion; B.Encoding:=Ship.Preferences.FbmEncoding;
  end;
begin
  B:=TFreeFileBuffer.Create;
  try
    B.Version:=Ship.FileVersion; B.Encoding:=Ship.Preferences.FbmEncoding;
    B.Add(Ship.FileVersion); B.Add(Ord(Ship.Precision)); Flush('header');
    Ship.ProjectSettings.SaveBinary(B); Flush('project');
    Ship.Visibility.SaveBinary(B); Flush('visibility');
    B.Add(Ship.NumberOfLayers); Flush('layer-count');
    for I:=0 to Ship.NumberOfLayers-1 do begin Ship.Layer[I].SaveBinary(B); Flush('layer-'+IntToStr(I)) end;
    B.Add(Ship.ActiveLayer.LayerIndex); Flush('active-layer');
    B.Add(Ship.NumberOfStations); for I:=0 to Ship.NumberOfStations-1 do Ship.Station[I].SaveBinary(B); Flush('stations');
    B.Add(Ship.NumberOfButtocks); for I:=0 to Ship.NumberOfButtocks-1 do Ship.Buttock[I].SaveBinary(B); Flush('buttocks');
    B.Add(Ship.NumberOfWaterlines); for I:=0 to Ship.NumberOfWaterlines-1 do Ship.Waterline[I].SaveBinary(B); Flush('waterlines');
    B.Add(Ship.NumberOfDiagonals); for I:=0 to Ship.NumberOfDiagonals-1 do Ship.Diagonal[I].SaveBinary(B); Flush('diagonals');
    B.Add(Ship.NumberOfMarkers); for I:=0 to Ship.NumberOfMarkers-1 do Ship.Marker[I].SaveBinary(B); Flush('markers');
    B.Add(Ship.NumberOfFlowlines); for I:=0 to Ship.NumberOfFlowlines-1 do Ship.Flowline[I].SaveBinary(B); Flush('flowlines');
    B.Add(Ship.NumberOfBackgroundImages); Flush('background-count');
    for I:=0 to Ship.NumberOfBackgroundImages-1 do begin
      Ship.BackgroundImage[I].SaveBinary(B); Flush('background-'+IntToStr(I));
    end;
  finally B.Free end;
end;

'''


def compare_pieces(trial: Path) -> list[dict]:
    rows = []
    for before in sorted(trial.glob('before-*.bin')):
        name = before.name.removeprefix('before-')
        after = trial / ('after-' + name)
        a, b = before.read_bytes(), after.read_bytes()
        row = {'piece': name, 'equal': a == b, 'before_bytes': len(a), 'after_bytes': len(b),
               'before_sha256': common.digest_file(before), 'after_sha256': common.digest_file(after)}
        if name.startswith('background-') and name != 'background-count.bin':
            row['parameters_equal'] = a[:34] == b[:34]
            row['dimensions_equal'] = a[34:42] == b[34:42]
            row['before_jpeg_bytes'] = struct.unpack_from('<i', a, 42)[0]
            row['after_jpeg_bytes'] = struct.unpack_from('<i', b, 42)[0]
            row['jpeg_payload_equal'] = a[46:] == b[46:]
            for stage, blob in [('before', a), ('after', b)]:
                (trial / (stage + '-' + name.removesuffix('.bin') + '.jpg')).write_bytes(blob[46:])
            try:
                from PIL import Image, ImageChops, ImageStat
                before_image = trial / ('before-' + name.removesuffix('.bin') + '.jpg')
                after_image = trial / ('after-' + name.removesuffix('.bin') + '.jpg')
                with Image.open(before_image) as image_a, Image.open(after_image) as image_b:
                    image_a, image_b = image_a.convert('RGB'), image_b.convert('RGB')
                    difference = ImageChops.difference(image_a, image_b)
                    row['jpeg_decoded_comparison'] = {
                        'status': 'review', 'size': list(image_a.size),
                        'decoded_pixels_equal': image_a.tobytes() == image_b.tobytes(),
                        'mean_absolute_error_rgb': ImageStat.Stat(difference).mean,
                        'max_absolute_error_rgb': [extent[1] for extent in difference.getextrema()],
                    }
            except ImportError:
                row['jpeg_decoded_comparison'] = {'status': 'not_run', 'reason': 'Pillow is not installed'}
        rows.append(row)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus', type=Path, default=common.DEFAULT_LAZARUS)
    parser.add_argument('--model', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output.exists() or output == source or source in output.parents:
        raise SystemExit('Output must be new and outside production source')
    output.mkdir(parents=True)
    report = {'status': 'review', 'diagnostic_only': True, 'cases': [],
              'limitations': ['Settings are serialized an additional time for diagnostic pieces.',
                              'This is not a desktop test or a repair; JPEG changes require separate assessment.']}
    fixtures = {str(model.resolve()): common.digest_file(model) for model in args.model}
    report['fixture_sha256'] = fixtures
    report['diagnostic_code_sha256'] = common.digest_file(Path(__file__))
    try:
        tools = common.junction_for(args.lazarus.resolve(), Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-legacy-settings-') as run:
            work, profile = run / 'source', run / 'profile'
            work.mkdir(); profile.mkdir()
            project, exe, snapshot = file_safety.prepare(source, work, tools, False)
            report['source_snapshot_sha256'] = snapshot
            for name in legacy_compatibility.FILES:
                if name.endswith('.pas'):
                    shutil.copy2(ROOT / name, work / 'Units' / name)
            probe = work / 'Units' / 'LegacyCompatibilityProbe.pas'
            text = probe.read_text(encoding='utf-8')
            key = 'procedure RunLegacyProbe(Form:TMainForm);\nvar'
            if text.count(key) != 1:
                raise RuntimeError('Native diagnostic entry changed')
            text = text.replace(key, HELPER + key)
            key = 'BeforeSettings:=SettingsWitness(Ship);'
            if text.count(key) != 1:
                raise RuntimeError('Native before-witness site changed')
            text = text.replace(key, key + "\n        SaveSettingsPieces(Ship,Output+'before-');")
            key = 'After:=GeometryWitness(Reload);'
            if text.count(key) != 1:
                raise RuntimeError('Native after-witness site changed')
            text = text.replace(key, key + "\n        SaveSettingsPieces(Reload,Output+'after-');")
            probe.write_text(text, encoding='utf-8')
            file_safety.replace(work / 'FreeShip.dpr', "  Main in 'Forms/Main.pas' {MainForm},",
                                "  LegacyCompatibilityProbe in 'Units/LegacyCompatibilityProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
            file_safety.replace(work / 'FreeShip.dpr', '  Application.Run;', '  LegacyCompatibilityProbe.RunLegacyProbe(MainForm);')
            env = os.environ.copy()
            env.update(TEMP=str(run), TMP=str(run), APPDATA=str(profile), LOCALAPPDATA=str(profile))
            report['compiler_sha256'] = file_safety.compile_source(project, tools, run, env, output, 240)
            for index, model in enumerate(args.model):
                trial = run / f'trial-{index}'
                trial.mkdir(); local = trial / 'model.fbm'; shutil.copy2(model, local)
                child_env = env.copy(); child_env.update(FS_LEGACY_MODEL=str(local), FS_LEGACY_OUTPUT=str(trial), FS_LEGACY_ALPHA='')
                child = subprocess.run([str(exe)], cwd=work, env=child_env, capture_output=True,
                                       timeout=240, creationflags=common.FLAGS)
                (trial / 'process.log').write_bytes(child.stdout + b'\n' + child.stderr)
                if child.returncode:
                    raise RuntimeError('Native process failed ' + str(child.returncode))
                case = {'fixture': str(model.resolve()), 'pieces': compare_pieces(trial),
                        'native': json.loads((trial / 'probe.json').read_text(encoding='utf-8-sig'))}
                report['cases'].append(case)
                target = output / f'trial-{index}'; target.mkdir()
                for artifact in trial.iterdir():
                    if artifact.is_file() and artifact != local:
                        shutil.copy2(artifact, target / artifact.name)
        if fixtures != {name: common.digest_file(Path(name)) for name in fixtures}:
            raise RuntimeError('Original fixture changed')
    except Exception as exc:
        report['status'] = 'error'; report['error'] = str(exc)
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'status': report['status'], 'cases': len(report['cases']), 'error': report.get('error')}, ensure_ascii=False))
    return 2 if report['status'] == 'review' else 1


if __name__ == '__main__':
    raise SystemExit(main())
