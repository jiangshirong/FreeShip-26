"""Independent self-checks for upstream regression result integrity.

Set FS_RUN_UPSTREAM_INTEGRATION=1 for the isolated compiler/injection test.
"""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import freeship_ui_test as common
import upstream_regression as upstream

class UpstreamReportTests(unittest.TestCase):
    def test_subcase_error_preserves_independent_results_and_coverage(self):
        cases=[{'id':'01.coverage','status':'not_run'},
               {'id':'02.coverage','status':'not_run'},
               {'id':'05.metric','status':'pass'}]
        upstream.record_subcase_error(cases,'01','01.probe',RuntimeError('injection unavailable'))
        self.assertEqual([x['id'] for x in cases],['02.coverage','05.metric','01.probe'])
        self.assertEqual(cases[-1]['status'],'error')
        self.assertIn('injection unavailable',cases[-1]['actual'])
        # Repeated infrastructure failure for the same case cannot create duplicate IDs.
        upstream.record_subcase_error(cases,'01','01.probe',ValueError('second failure'))
        self.assertEqual(sum(x['id']=='01.probe' for x in cases),1)
        self.assertEqual(cases[0]['id'],'02.coverage')
        self.assertEqual(common.classify_statuses([x['status'] for x in cases]),'error')

    def test_machine_states_do_not_turn_missing_or_error_into_pass(self):
        self.assertEqual(common.classify_statuses(['pass','not_run']),'partial')
        self.assertEqual(common.classify_statuses(['pass','review']),'partial')
        self.assertEqual(common.classify_statuses(['fail','error']),'error')
        self.assertEqual(common.classify_statuses([]),'error')

    def test_issue_manifest_has_unique_full_range_and_real_documents(self):
        data=json.loads((ROOT/'upstream_issue_manifest.json').read_text(encoding='utf8'))['cases']
        self.assertEqual({entry['id'] for entry in data},{f'{i:02}' for i in range(1,18)})
        self.assertEqual(len(data),17)
        docs=common.DEFAULT_SOURCE/'Docs'/'上游问题记录'
        for entry in data:
            self.assertTrue((docs/entry['record']).is_file(),entry)

    @unittest.skipUnless(os.environ.get('FS_RUN_UPSTREAM_INTEGRATION')=='1','set FS_RUN_UPSTREAM_INTEGRATION=1 for isolated build')
    def test_density_correction_passes_while_unfixed_intersection_fails(self):
        # A known, independent SI conversion is injected into a disposable copy.
        # This checks a positive control and an unrelated negative control in the
        # same real execution, not only the Python report formatter.
        with common.temporary_workspace('fs-upstream-selftest-') as temp:
            source=temp/'source'; source.mkdir()
            for name in common.FOLDERS:
                shutil.copytree(common.DEFAULT_SOURCE/name,source/name)
            for path in common.DEFAULT_SOURCE.iterdir():
                if path.is_file() and path.suffix.lower() in common.ROOT_SUFFIXES:
                    shutil.copy2(path,source/path.name)
            p=source/'Units'/'FreeGeometry.pas'; text=p.read_text(encoding='utf8')
            old='Lbs = 0.44642857;'
            if text.count(old)!=1:
                self.skipTest('baseline density anchor changed; re-evaluate mutation fixture')
            p.write_text(text.replace(old,'Lbs = 0.45359237;'),encoding='utf8')
            out=temp/'report'
            proc=subprocess.run([sys.executable,str(ROOT/'upstream_regression.py'),'--source',str(source),
                                 '--output',str(out),'--timeout','120'],capture_output=True,timeout=240)
            self.assertTrue((out/'report.json').exists(),proc.stderr.decode(errors='replace'))
            report=json.loads((out/'report.json').read_text(encoding='utf8'))
            self.assertNotEqual(report['status'],'error',report.get('error'))
            cases={x['id']:x for x in report['cases']}
            self.assertEqual(cases['05.water-density-metric-to-imperial']['status'],'pass')
            self.assertEqual(cases['06.station-metric-to-imperial']['status'],'fail')
            self.assertTrue(report.get('source_unchanged_during_run'))

if __name__=='__main__': unittest.main()
