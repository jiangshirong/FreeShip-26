import tempfile, unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from linesplan_trace import summarize

class TraceTests(unittest.TestCase):
 def read(self,body):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'trace.tsv';p.write_text('elapsed_ms\tevent\tdetail\n'+body,encoding='utf-8')
   return summarize(p)
 def test_native_mismatch_is_review_not_visual_failure(self):
  r=self.read('10\tnative\tstage=present.begin;viewport=100,80/90,70/0,0,90,70;rendering=0\n')
  self.assertEqual(r['status'],'review');self.assertEqual(len(r['native_geometry_mismatches']),1)
  self.assertEqual(r['native_animation_acceptance'],'not_run')
 def test_matching_geometry_and_reentry(self):
  r=self.read('10\tnative\tstage=present.begin;viewport=100,80/100,80/0,0,100,80;rendering=1\n')
  self.assertEqual(r['native_geometry_mismatches'],[]);self.assertEqual(len(r['reentrant_presentations']),1)
 def test_completed_native_output_before_size_commit(self):
  r=self.read('10\tnative\tstage=present.end;viewport=1916,1136/1031,741;buffer=1916,1136;dc_window=0000000000001234\n')
  self.assertEqual(len(r['premature_native_presentations']),1)
 def test_deferred_or_memory_output_is_not_premature_native_output(self):
  r=self.read('10\tnative\tstage=present.deferred;viewport=1916,1136/1031,741;buffer=1916,1136;dc_window=0000000000001234\n'+
   '11\tnative\tstage=present.end;viewport=1916,1136/1031,741;buffer=1916,1136;dc_window=0000000000000000\n')
  self.assertEqual(r['premature_native_presentations'],[])
 def test_pending_composition_duration_is_observation(self):
  r=self.read('10\tnative\tstage=compose.begin;viewport=1916,1136/1418,793\n'+
   '135\tnative\tstage=compose.end;viewport=1916,1136/1418,793\n')
  self.assertEqual(r['pending_native_compositions'][0]['duration_ms'],125)
  self.assertEqual(r['status'],'review')
 def test_committed_composition_is_not_pending(self):
  r=self.read('10\tnative\tstage=compose.begin;viewport=1916,1136/1916,1136\n'+
   '135\tnative\tstage=compose.end;viewport=1916,1136/1916,1136\n')
  self.assertEqual(r['pending_native_compositions'],[])
 def test_old_trace_remains_readable(self):
  r=self.read('10\tfull\tms=8; zoom=1\n20\tclosed\t\n')
  self.assertEqual(r['full_frames'][0]['duration_ms'],8);self.assertTrue(r['has_close_event'])
if __name__=='__main__':unittest.main()
