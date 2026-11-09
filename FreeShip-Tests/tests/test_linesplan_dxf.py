import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from dxf_geometry import records
class DxfGeometryTests(unittest.TestCase):
    def compare(self,a,b):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'a.dxf';q=Path(td)/'b.dxf';p.write_text(a);q.write_text(b)
            return records(p)==records(q)
    def line(self,a,b):return f'0\nLINE\n8\nHull\n10\n{a}\n20\n0\n11\n{b}\n21\n0\n'
    def loop(self,values):return '0\nPOLYLINE\n8\nHull\n'+''.join(f'0\nVERTEX\n8\nHull\n10\n{x}\n20\n0\n' for x in values)+'0\nSEQEND\n'
    def test_reversed_line(self):self.assertTrue(self.compare(self.line(0,1),self.line(1,0)))
    def test_changed_coordinate(self):self.assertFalse(self.compare(self.line(0,1),self.line(0,1.000001)))
    def test_closed_loop_start(self):self.assertTrue(self.compare(self.loop([0,1,2,0]),self.loop([1,2,0,1])))
    def test_missing_connection(self):self.assertFalse(self.compare(self.loop([0,1,2,0]),self.loop([0,1,2])))

    def test_equal_segments_with_different_chunk_boundaries(self):
        self.assertTrue(self.compare(self.loop([0,1])+self.loop([1,2,3]),self.loop([0,1,2])+self.loop([2,3])))
    def test_rewired_segments(self):
        self.assertFalse(self.compare(self.loop([0,1,2]),self.loop([0,2,1])))
