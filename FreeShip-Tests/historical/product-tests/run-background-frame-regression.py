"""Compile and run the hidden viewport regression; no desktop interaction."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
tools = next(Path(tempfile.gettempdir()).glob('fs-ui-tools-*'))
tree = ET.parse(repo / 'FreeShip.lpi')
for parent in tree.iter():
    for child in list(parent):
        if child.tag in ('ExecuteBefore', 'ExecuteAfter'):
            parent.remove(child)
units = tools / 'fpc/3.2.2/units/x86_64-win64'
for node in tree.iter('OtherUnitFiles'):
    node.set('Value', node.get('Value', '') + ';' + ';'.join(
        str(p) for p in units.iterdir() if p.is_dir() and p.name != 'fv'))
for node in tree.findall('.//CompilerOptions/Target/Filename'):
    node.set('Value', 'Tests/FreeBackgroundFrameRegression')
for node in tree.findall('.//Units/Unit0/Filename'):
    node.set('Value', 'Tests/FreeBackgroundFrameRegression.pas')
project = repo / '.background-regression.lpi'
tree.write(project, encoding='utf-8', xml_declaration=True)
try:
    result = subprocess.run([
        str(tools / 'lazbuild.exe'), '--lazarusdir=' + str(tools),
        '--compiler=' + str(tools / 'fpc/3.2.2/bin/x86_64-win64/fpc.exe'),
        '--pcp=.lazarus-config', '--build-mode=Win32 Debug', '--ws=win32',
        '--cpu=x86_64', '--os=win64', str(project)], cwd=repo,
        capture_output=True, timeout=180)
    (repo / 'Tests/background-frame-build.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit('Build failed: see Tests/background-frame-build.log')
    result = subprocess.run([str(repo / 'Tests/FreeBackgroundFrameRegression.exe')],
                            cwd=repo, capture_output=True, timeout=25)
    (repo / 'Tests/background-frame-result.log').write_bytes(result.stdout + result.stderr)
    print((result.stdout + result.stderr).decode('utf-8', errors='replace'))
    raise SystemExit(result.returncode)
finally:
    project.unlink(missing_ok=True)
