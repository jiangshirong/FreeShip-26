import sys, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from upstream_coverage import coverage_matrix

class CoverageTests(unittest.TestCase):
    def test_groups_by_issue_and_preserves_gaps(self):
        manifest={'cases':[{'id':'01','title':'one','record':'one.md'},{'id':'02','title':'two','record':'two.md'}]}
        rows=coverage_matrix([{'id':'01.ok','status':'pass'},{'id':'01.gap','status':'not_run'}],manifest)
        self.assertEqual(rows[0]['coverage'],'incomplete')
        self.assertEqual(rows[0]['counts'],{'pass':1,'not_run':1})
        self.assertEqual(rows[1]['coverage'],'unknown')
    def test_executed_is_not_full_coverage(self):
        rows=coverage_matrix([{'id':'01.case','status':'fail'}],{'cases':[{'id':'01','title':'one','record':'one.md'}]})
        self.assertEqual(rows[0]['coverage'],'observed')
        self.assertIn('not that all scenarios',rows[0]['note'])
