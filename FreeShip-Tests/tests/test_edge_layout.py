import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('freeship_ui_test', ROOT / 'freeship_ui_test.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

class EdgeLayoutTests(unittest.TestCase):
    def test_edge_effect_visibility_collision_and_animation(self):
        src = tool.DEFAULT_SOURCE / 'Units' / 'FreeLabelLayout.pas'
        original = tool.digest_file(src)
        tools = tool.junction_for(tool.DEFAULT_LAZARUS, Path(tempfile.gettempdir()))
        compiler = tools / 'fpc' / '3.2.2' / 'bin' / 'x86_64-win64' / 'fpc.exe'
        runtime = tools / 'fpc' / '3.2.2' / 'units' / 'x86_64-win64'
        with tool.temporary_workspace('fs-edge-layout-') as temp:
            shutil.copy2(src, temp / src.name)
            self.assertEqual(tool.digest_file(temp / src.name), original)
            cmd = [str(compiler), '-B', '-Fu'+str(temp), '-FE'+str(temp), '-FU'+str(temp)]
            cmd += ['-Fu'+str(p) for p in runtime.iterdir() if p.is_dir() and p.name != 'fv']
            cmd.append(str(ROOT / 'tests' / 'edge_layout_probe.pas'))
            build = subprocess.run(cmd, capture_output=True, timeout=60, creationflags=tool.FLAGS)
            self.assertEqual(build.returncode, 0, (build.stdout+build.stderr).decode('utf8','replace'))
            run = subprocess.run([str(temp / 'edge_layout_probe.exe')], capture_output=True, timeout=60, creationflags=tool.FLAGS)
            self.assertEqual(run.returncode, 0, (run.stdout+run.stderr).decode('utf8','replace'))
            self.assertEqual(tool.digest_file(src), original, 'source changed during test')

if __name__ == '__main__':
    unittest.main()
