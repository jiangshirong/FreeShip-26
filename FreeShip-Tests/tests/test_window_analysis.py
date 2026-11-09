import importlib.util
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('freeship_window_test',ROOT/'freeship_window_test.py')
window=importlib.util.module_from_spec(spec);spec.loader.exec_module(window)

class WindowAnalysisTests(unittest.TestCase):
    def samples(self):
        old={'left':0,'top':0,'right':10,'bottom':10}
        new={'left':5,'top':0,'right':15,'bottom':10}
        sibling={'left':0,'top':0,'right':8,'bottom':10}
        req={'result':True,'flags':0x185,'rect':{'left':0,'top':0,'right':15,'bottom':10}}
        return [{'iteration':i,'old_bounds':old.copy(),'new_bounds':new.copy(),
                 'sibling_bounds':[sibling.copy()],'requests':[dict(req,rect=req['rect'].copy())],
                 'move_ms':float(i),'four_paints_ms':float(i)*2,'cache_copy_ms':0.5,'model_redraw_ms':0.0}
                for i in range(1,41)]
    def test_themes_distinguish_incomplete_from_failure(self):
        summary=window.summarize([{'id':'window.motion-start-all','status':'pass'},
                                  {'id':'window.capture-cancel-focus','status':'not_run'},
                                  {'id':'window.new-view-lifecycle','status':'fail'}])
        self.assertEqual(summary['themes']['motion']['status'],'partial')
        self.assertEqual(summary['themes']['new_view']['status'],'fail')
        self.assertEqual(summary['status_counts'],{'pass':1,'not_run':1,'fail':1})

    def test_dirty_union_and_flags_are_required(self):
        result={'cases':[],'move_samples':self.samples()}
        with tempfile.TemporaryDirectory(prefix='fs-window-analysis-') as tmp:
            window.enrich(result,Path(tmp))
        cases={x['id']:x for x in result['cases']}
        self.assertEqual(cases['window.redraw-request-coverage']['status'],'pass')
        self.assertEqual(cases['window.overlap-exposure-scenario']['status'],'pass')
        self.assertEqual(cases['window.sibling-visible-repaint']['status'],'not_run')
        self.assertEqual(result['timings_ms']['move_ms']['p95'],38.0)
        result={'cases':[],'move_samples':self.samples()}
        result['move_samples'][8]['requests'][0]['rect']['left']=2
        result['move_samples'][9]['requests'][0]['flags'] &= ~0x100
        with tempfile.TemporaryDirectory(prefix='fs-window-analysis-') as tmp:
            window.enrich(result,Path(tmp))
        case=next(x for x in result['cases'] if x['id']=='window.redraw-request-coverage')
        self.assertEqual(case['status'],'fail')
        self.assertEqual([m['iteration'] for m in case['actual']['missing']],[9,10])

if __name__=='__main__': unittest.main()
