import importlib.util
from pathlib import Path
import unittest

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('freeship_text_visibility',root/'freeship_ui_test.py')
tool=importlib.util.module_from_spec(spec); spec.loader.exec_module(tool)

class TextEvidenceTests(unittest.TestCase):
    def test_candidates_are_review_not_failure(self):
        node={'kind':'text_control','text':'long','visible_excluding_hidden_form':True,
              'bounds':{'width':20,'height':10}, 'ancestor_clip':{'left':0,'top':0,'right':15,'bottom':10},
              'measured_width':30,'measured_height':8,'word_wrap':True}
        evidence,cases=tool.analyze_text_visibility([{'state':'x','language':'Russian','nodes':[node]}])
        self.assertEqual(cases[0]['status'],'review')
        self.assertEqual(set(evidence['candidates'][0]['candidate_reasons']),
                         {'control_bounds_clipped_by_ancestor','text_width_exceeds_control_width'})
        self.assertEqual(evidence['candidates'][0]['language'],'Russian')
        self.assertEqual(cases[1]['status'],'not_run')
    def test_hidden_or_unmeasured_does_not_become_pass(self):
        node={'kind':'text_control','text':'x','visible_excluding_hidden_form':False,
              'bounds':{'width':1,'height':1},'ancestor_clip':{'left':0,'top':0,'right':1,'bottom':1},'measured_width':99}
        evidence,cases=tool.analyze_text_visibility([{'nodes':[node]}])
        self.assertFalse(evidence['candidates']); self.assertEqual(cases[0]['status'],'not_run')
        self.assertEqual(len(evidence['observations']),1)
if __name__=='__main__': unittest.main()
