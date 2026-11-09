import importlib.util
from concurrent.futures import ThreadPoolExecutor
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('freeship_ui_test', ROOT / 'freeship_ui_test.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class HarnessTests(unittest.TestCase):
    def test_instrumentation_whitespace_fallback_keeps_literals_and_rejects_ambiguity(self):
        with tool.temporary_workspace('fs-ui-anchor-') as temp:
            file=temp/'sample.pas'
            file.write_text("begin\n Result . Visible := False ; Result . Collisions := 0 ;\nend;", encoding='utf8')
            tool.replace_once(file,'Result.Visible:=False; Result.Collisions:=0;',
                              'Result.Visible:=False; Result.Collisions:=0; Probe;',)
            self.assertIn('Probe;',file.read_text(encoding='utf8'))
            self.assertEqual(tool.INSTRUMENTATION_MATCHES[-1]['strategy'],'token_whitespace')
            file.write_text("Writeln('a  b');",encoding='utf8')
            with self.assertRaises(RuntimeError):
                tool.replace_once(file,"Writeln('a b');",'changed;')
            file.write_text('A := 1; A:=1;',encoding='utf8')
            with self.assertRaises(RuntimeError):
                tool.replace_once(file,'A := 1 ;', 'B:=1;')

    def test_callback_policy_requires_matching_source_and_safe_identity(self):
        with tool.temporary_workspace('fs-ui-policy-') as temp:
            source = temp / 'source'
            (source / 'Forms').mkdir(parents=True)
            file = source / 'Forms' / 'probe.pas'
            file.write_text('reviewed', encoding='utf8')
            rule = {'source':'Forms/probe.pas', 'sha256':tool.digest_file(file)}
            policy = {'schema':1, 'menu_adapters':{'main_overflow':rule, 'hull_popup':rule},
                      'dropdown_adapters':[dict(rule, owner_class='TProbe', handler='Populate')]}
            path = temp / 'policy.json'
            path.write_text(json.dumps(policy), encoding='utf8')
            accepted = tool.dynamic_policy(source, path)
            self.assertTrue(accepted['main_overflow_approved'])
            self.assertEqual(accepted['dropdowns_approved'], ['TProbe.Populate'])
            file.write_text('modified', encoding='utf8')
            refused = tool.dynamic_policy(source, path)
            self.assertFalse(refused['main_overflow_approved'])
            self.assertEqual(refused['dropdowns_approved'], [])
            policy['dropdown_adapters'][0]['handler'] = 'Populate;Other'
            path.write_text(json.dumps(policy), encoding='utf8')
            with self.assertRaises(RuntimeError):
                tool.dynamic_policy(source, path)

    def test_interface_position_is_observation_but_behavior_failure_remains_failure(self):
        cases = [{'id':'toolbar.redo-shortcut-only','status':'fail'},
                 {'id':'history.redo','status':'fail'},
                 {'id':'toolbar.point-mode-on','status':'fail','actual':'missing'}]
        tool.mark_design_observations(cases)
        self.assertEqual((cases[0]['status'], cases[0]['legacy_status']), ('review','fail'))
        self.assertEqual(cases[1]['status'], 'fail')
        self.assertEqual(cases[2]['status'], 'not_run')

    def test_geometry_review_reports_observed_anomaly_without_asserting_design(self):
        cases = [{'id':'labels.hull.chinese.profile.800.overlap','status':'pass'},
                 {'id':'labels.hull.chinese.plan.800.overlap','status':'fail'}]
        tool.mark_design_observations(cases)
        self.assertEqual(cases[0]['status'],'pass')
        self.assertEqual((cases[1]['status'],cases[1]['legacy_status']),('review','fail'))

    def test_inventory_diff_reports_changes_without_deciding_design(self):
        snapshots = [{'state':'model_loaded','nodes':[{'kind':'action','component_path':'main/save','enabled':False}]},
                     {'state':'point_selected','nodes':[{'kind':'action','component_path':'main/save','enabled':True}]}]
        diffs = tool.inventory_diffs(snapshots)
        self.assertEqual(diffs[0]['changed'][0]['fields']['enabled'], {'before':False,'after':True})
        self.assertFalse(diffs[0]['added'])

    def test_issue_index_keeps_failures_and_unrun_cases_with_source_identity(self):
        result = {'status':'partial', 'source_sha256':'abc', 'source_unchanged_during_run':True,
                  'cases':[{'id':'ok','status':'pass'},
                           {'id':'frame','status':'pass','image':'frame.bmp'},
                           {'id':'bad','status':'fail','expected':'shown','actual':'missing','image':'a.bmp'},
                           {'id':'pending','status':'not_run','actual':'focus required'},
                           {'id':'frame.bounds','status':'review','actual':'outside'}]}
        index = tool.issue_summary(result)
        self.assertEqual(index['source_sha256'], 'abc')
        self.assertEqual([item['id'] for item in index['issues']], ['bad','pending','frame.bounds'])
        self.assertEqual(index['issues'][0]['image'], 'a.bmp')
        self.assertEqual(index['issues'][2]['image'], 'frame.bmp')

    def test_status_classification_distinguishes_infrastructure_error(self):
        self.assertEqual(tool.classify_statuses(['pass']), 'pass')
        self.assertEqual(tool.classify_statuses(['pass', 'review']), 'partial')
        self.assertEqual(tool.classify_statuses(['pass', 'not_run']), 'partial')
        self.assertEqual(tool.classify_statuses(['fail', 'not_run']), 'fail')
        self.assertEqual(tool.classify_statuses(['fail', 'error']), 'error')
        self.assertEqual(tool.classify_statuses([]), 'error')
        self.assertEqual(tool.classify_statuses(['unknown']), 'error')
        self.assertEqual(tool.classify_statuses(iter(['pass', 'fail'])), 'fail')
        self.assertEqual(tool.classify_statuses(iter(['pass', 'not_run'])), 'partial')
        self.assertEqual(tool.classify_statuses(iter([])), 'error')

    def test_missing_draws_are_not_passes(self):
        cases = []
        with tool.temporary_workspace('fs-ui-analysis-') as temp:
            tool.analyze_draws(cases, temp, ('perspective',))
        self.assertEqual(len(cases), 15)
        self.assertTrue(all(c['status'] == 'not_run' for c in cases))

    def test_missing_placement_trace_does_not_pass_or_fail_target_label(self):
        cases = [{'id':'view.perspective.draw.label.1', 'status':'pass',
                  'text_trace':[{'text':'20.000','x':1,'y':1}]}]
        with tool.temporary_workspace('fs-ui-missing-trace-') as temp:
            tool.analyze_draws(cases, temp, ('perspective',), (1,))
        self.assertEqual(cases[0]['status'], 'not_run')
        self.assertEqual(cases[0]['legacy_status'], 'pass')
        self.assertTrue(all(c['status'] == 'not_run' for c in cases))

    def test_panel_overlap_is_reported_for_review(self):
        controls = [
            {'name': 'a', 'class': 'TLabel', 'visible': True, 'leaf': True,
             'x': 5, 'y': 5, 'width': 20, 'height': 20},
            {'name': 'b', 'class': 'TEdit', 'visible': True, 'leaf': True,
             'x': 20, 'y': 5, 'width': 20, 'height': 20},
        ]
        native = {'panel': {'client_width': 100, 'client_height': 100, 'controls': controls}}
        cases = []
        with tool.temporary_workspace('fs-ui-layout-') as temp:
            tool.analyze_panel(cases, native, temp)
        overlap = next(c for c in cases if c['id'] == 'panel.layout-overlaps')
        self.assertEqual(overlap['status'], 'review')
        self.assertEqual(overlap['actual'], [['a', 'b']])

    def test_clipped_bounds_do_not_create_false_overlaps_or_crossings(self):
        a = {'x':0,'y':0,'width':20,'height':20, 'clip_rect':{'left':0,'top':0,'right':20,'bottom':8}}
        b = {'x':0,'y':10,'width':20,'height':20, 'clip_rect':{'left':0,'top':10,'right':20,'bottom':30}}
        self.assertFalse(tool.rectangles_overlap(a,b))
        self.assertFalse(tool.segment_hits_text({'x1':0,'y1':15,'x2':20,'y2':15},a))
        self.assertTrue(tool.segment_hits_text({'x1':0,'y1':5,'x2':20,'y2':5},a))
        controls=[dict(a,name='a',**{'class':'TEdit','visible':True,'leaf':True}),
                  dict(b,name='b',**{'class':'TEdit','visible':True,'leaf':True})]
        cases=[]
        with tool.temporary_workspace('fs-ui-parentclip-') as temp:
            tool.analyze_panel(cases,{'panel':{'controls':controls,'client_width':100,'client_height':100}},temp)
        by_id={c['id']:c for c in cases}
        self.assertEqual(by_id['panel.layout-overlaps']['status'],'pass')
        self.assertEqual(by_id['panel.ancestor-clipping']['status'],'review')

    def test_unsettled_label_layout_is_not_reported_as_final(self):
        sample={'id':'labels.hull.chinese.profile.800','width':800,'height':500,'layout_settled':False,
                'placement_trace':[], 'text_trace':[], 'segment_trace':[]}
        cases=[sample]; tool.analyze_labels(cases)
        for case in cases:
            if case['id'].startswith(sample['id']+'.') and not case['id'].endswith('.coverage'):
                self.assertEqual(case['status'],'not_run',case)

    def test_coordinate_trace_detects_wrong_point_prefix_and_omissions(self):
        values = ['0.000','0.000','20.000','20.000','0.000','20.000','0.000','20.000']
        def sample():
            placements = [{'category': 'point', 'point_index': i, 'text': value, 'visible': True,
                           'x': 30 + i * 60, 'y': 50, 'width': 40, 'height': 12}
                          for i, value in enumerate(values)]
            return [{'id': 'view.perspective.draw.label.1', 'placement_trace': placements,
                     'text_trace': [{'text': p['text'], 'x': p['x'], 'y': p['y']} for p in placements]}]
        with tool.temporary_workspace('fs-ui-coord-check-') as temp:
            for defect in ('none', 'wrong_point', 'prefix', 'omitted', 'missing_draw'):
                cases = sample()
                placement = cases[0]['placement_trace'][5]
                if defect == 'wrong_point': placement['text'] = '3.000'
                if defect == 'prefix': placement['text'] = 'X=20.000'
                if defect == 'omitted': placement['visible'] = False
                if defect == 'missing_draw': cases[0]['text_trace'].pop(5)
                tool.analyze_draws(cases, temp, ('perspective',), (1,))
                result = next(c for c in cases if c['id'] == 'view.perspective.draw.all-labels.1')
                with self.subTest(defect=defect):
                    self.assertEqual(result['status'], 'pass' if defect == 'none' else 'review' if defect == 'omitted' else 'fail')

    def test_coordinate_omission_does_not_hide_wrong_values_or_missing_draws(self):
        values = ['0.000','0.000','20.000','20.000','0.000','20.000','0.000','20.000']
        placements = [{'category':'point', 'point_index':i, 'text':v, 'visible':True,
                       'x':i*60, 'y':50, 'width':40, 'height':12} for i,v in enumerate(values)]
        placements[5]['visible'] = False
        placements[5]['omission_reason'] = 'no unobstructed clipping region'
        draw = {'id':'view.perspective.draw.label.1', 'status':'fail', 'layout_settled':True,
                'placement_trace':placements, 'text_trace':[{'text':p['text'],'x':p['x'],'y':p['y']}
                                                          for p in placements if p['visible']]}
        with tool.temporary_workspace('fs-ui-omission-') as temp:
            cases = [draw]
            tool.analyze_draws(cases, temp, ('perspective',), (1,))
            self.assertEqual(draw['status'], 'review')
            result = next(c for c in cases if c['id'].endswith('draw.all-labels.1'))
            self.assertEqual(result['status'], 'review')
            self.assertEqual(result['actual']['omitted_details'][0]['omission_reason'], 'no unobstructed clipping region')
            draw['layout_settled'] = False
            cases = [draw]
            tool.analyze_draws(cases, temp, ('perspective',), (1,))
            self.assertEqual(draw['status'], 'not_run')
            self.assertEqual(next(c for c in cases if c['id'].endswith('draw.all-labels.1'))['status'], 'not_run')
            draw['layout_settled'] = True
            # A different visible point that never reaches the drawing call remains a failure.
            draw['text_trace'] = [t for t in draw['text_trace'] if t['x'] != placements[2]['x']]
            cases = [draw]
            tool.analyze_draws(cases, temp, ('perspective',), (1,))
            self.assertEqual(next(c for c in cases if c['id'].endswith('draw.all-labels.1'))['status'], 'fail')
            draw['placement_trace'][5]['text'] = 'WRONG'
            cases = [draw]
            tool.analyze_draws(cases, temp, ('perspective',), (1,))
            self.assertEqual(draw['status'], 'fail')
            self.assertEqual(next(c for c in cases if c['id'].endswith('draw.all-labels.1'))['status'], 'fail')

    def test_label_audit_detects_missing_important_label_overlap_and_clipping(self):
        def placement(category, text, x, y, visible=True):
            return {'category': category, 'text': text, 'x': x, 'y': y, 'width': 20, 'height': 10,
                    'anchor_x': x, 'anchor_y': y, 'visible': visible}
        items = [placement('point','1.000',10,10), placement('grid_tick','1',60,10),
                 placement('grid_named','基线 0',5,50,False),
                 placement('grid_named','DWL设计水线 4000',5,80)]
        cases = [{'id':'labels.hull.chinese.profile.800', 'width':100, 'height':100,
                  'placement_trace':items,
                  'text_trace':[{'text':p['text'],'x':p['x'],'y':p['y']} for p in items if p['visible']]}]
        # A visible text rectangle is now clipped and overlaps another one.
        items[1]['x'] = 15
        items[3]['y'] = 95
        tool.analyze_labels(cases)
        by_id = {c['id']: c for c in cases}
        base = 'labels.hull.chinese.profile.800.'
        self.assertEqual(by_id[base+'baseline']['status'], 'fail')
        self.assertEqual(by_id[base+'overlap']['status'], 'fail')
        self.assertEqual(by_id[base+'bounds']['status'], 'fail')
        self.assertEqual(by_id[base+'drawn']['status'], 'fail')
        self.assertEqual(by_id[base+'omissions']['status'], 'review')

    def test_label_audit_flags_line_crossing_without_claiming_full_geometry_coverage(self):
        text = {'x': 20, 'y': 20, 'width': 40, 'height': 20}
        self.assertTrue(tool.segment_hits_text({'x1': 0, 'y1': 30, 'x2': 90, 'y2': 30}, text))
        self.assertFalse(tool.segment_hits_text({'x1': 0, 'y1': 10, 'x2': 90, 'y2': 10}, text))
        self.assertFalse(tool.segment_hits_text({'x1': 0, 'y1': 20, 'x2': 90, 'y2': 20}, text))
        self.assertTrue(tool.segment_hits_text({'x1': 40, 'y1': 0, 'x2': 40, 'y2': 90}, text))

    def test_coordinate_regression_is_detected_in_rebuilt_desktop_source(self):
        with tool.temporary_workspace('fs-ui-mutation-') as temp:
            source = temp / 'source'
            source.mkdir()
            for folder in tool.FOLDERS:
                shutil.copytree(tool.DEFAULT_SOURCE / folder, source / folder)
            for p in tool.DEFAULT_SOURCE.iterdir():
                if p.is_file() and p.suffix.lower() in tool.ROOT_SUFFIXES:
                    shutil.copy2(p, source / p.name)
            target = source / 'Units' / 'FreeSubdivisionControlPoint.inc'
            original = target.read_text(encoding='utf8')
            needle = "1: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.X);"
            self.assertEqual(original.count(needle), 1)
            target.write_text(original.replace(needle, needle.replace('FCoordinate.X', 'FCoordinate.Y')),
                              encoding='utf8', newline='\n')
            output = temp / 'output'
            process = subprocess.run([sys.executable, str(ROOT / 'freeship_ui_test.py'),
                                      '--source', str(source), '--case', 'coordinates', '--view', 'perspective',
                                      '--axis', 'x', '--output', str(output)],
                                     capture_output=True, timeout=240)
            report = json.loads((output / 'report.json').read_text(encoding='utf8'))
            self.assertEqual(process.returncode, 1, report)
            self.assertEqual(report['status'], 'fail')
            self.assertIn('view.perspective.draw.all-labels.1', {c['id'] for c in report['cases'] if c['status'] == 'fail'})

    def test_view_and_axis_selection_limits_the_executed_scenarios(self):
        with tool.temporary_workspace('fs-ui-select-') as temp:
            output = temp / 'output'
            process = subprocess.run([sys.executable, str(ROOT / 'freeship_ui_test.py'),
                                      '--case', 'coordinates', '--view', 'profile', '--axis', 'x',
                                      '--output', str(output)], capture_output=True, timeout=240)
            report = json.loads((output / 'report.json').read_text(encoding='utf8'))
            self.assertNotEqual(process.returncode, 3, report)
            self.assertEqual(report['selection']['axis'], 'x')
            ids = {case['id'] for case in report['cases']}
            self.assertIn('view.profile.draw.label.0', ids)
            self.assertIn('view.profile.draw.label.1', ids)
            self.assertNotIn('view.profile.draw.label.2', ids)
            self.assertFalse(any(item.startswith(('panel.', 'view.plan.', 'view.bodyplan.',
                                                  'view.perspective.')) for item in ids))

    def test_discovery_survives_renamed_widgets(self):
        with tool.temporary_workspace('fs-ui-renamed-') as temp:
            source = temp / 'source'
            source.mkdir()
            for folder in tool.FOLDERS:
                shutil.copytree(tool.DEFAULT_SOURCE / folder, source / folder)
            for file in tool.DEFAULT_SOURCE.iterdir():
                if file.is_file() and file.suffix.lower() in tool.ROOT_SUFFIXES:
                    shutil.copy2(file, source / file.name)
            for filename, names in [
                ('Forms/FreeLayerVisibilityDlg.pas', ['sgLayers', 'CheckBox1', 'CheckBox2']),
                ('Forms/FreeLayerVisibilityDlg.lfm', ['sgLayers', 'CheckBox1', 'CheckBox2']),
                ('Forms/FreeControlPointFrm.pas', ['EditName', 'EditX', 'EditY', 'EditZ']),
                ('Forms/FreeControlPointFrm.lfm', ['EditName', 'EditX', 'EditY', 'EditZ']),
            ]:
                file = source / filename
                text = file.read_text(encoding='utf8')
                for name in names:
                    text, count = re.subn(r'\b' + name + r'\b', 'ProbeRenamed' + name, text)
                    self.assertGreater(count, 0, (filename, name))
                file.write_text(text, encoding='utf8', newline='\n')
            file = source / 'Forms/Main.pas'
            text = file.read_text(encoding='utf8')
            for name in ('StudioOverflowMenu', 'StudioToolbar'):
                text = text.replace("'" + name + "'", "'ProbeRenamed" + name + "'")
            file.write_text(text, encoding='utf8', newline='\n')
            policy = json.loads((ROOT / 'dynamic_callbacks.json').read_text(encoding='utf8'))
            for rule in policy['menu_adapters'].values():
                rule['sha256'] = tool.digest_file(source / rule['source'])
            policy_file = temp / 'policy.json'
            policy_file.write_text(json.dumps(policy), encoding='utf8')
            for suite, required in [('layers', ['layers.hide-surface', 'layers.bulk.hide-surfaces']),
                                    ('panel', ['panel.coordinates', 'panel.switch-point']),
                                    ('dynamic', ['dynamic.synthetic-combo'])]:
                output = temp / suite
                process = subprocess.run([sys.executable, str(ROOT / 'freeship_ui_test.py'),
                    '--source', str(source), '--case', suite, '--output', str(output),
                    '--callback-policy', str(policy_file)], capture_output=True, timeout=240)
                report = json.loads((output / 'report.json').read_text(encoding='utf8'))
                self.assertIn(process.returncode, (0, 2), report)
                cases = {c['id']:c['status'] for c in report['cases']}
                for ident in required: self.assertEqual(cases.get(ident), 'pass', report)
                if suite == 'dynamic':
                    callbacks = json.loads((output / 'dynamic_inventory.json').read_text(encoding='utf8'))['callbacks']
                    self.assertTrue(any(c['adapter']=='overflow-popup' and c['status']=='executed' for c in callbacks))

    def test_layer_discovery_survives_swapped_checkbox_columns(self):
        with tool.temporary_workspace('fs-ui-columns-') as temp:
            source = temp / 'source'
            source.mkdir()
            for folder in tool.FOLDERS:
                shutil.copytree(tool.DEFAULT_SOURCE / folder, source / folder)
            for file in tool.DEFAULT_SOURCE.iterdir():
                if file.is_file() and file.suffix.lower() in tool.ROOT_SUFFIXES:
                    shutil.copy2(file, source / file.name)
            pas = source / 'Forms/FreeLayerVisibilityDlg.pas'
            text = pas.read_text(encoding='utf8')
            pairs = [('Cells[0,', 'Cells[1,'), ('aCol=0', 'aCol=1'),
                     ('sgLayersCheckboxToggled(Sender, 0,', 'sgLayersCheckboxToggled(Sender, 1,')]
            for left, right in pairs:
                self.assertIn(left, text); self.assertIn(right, text)
                text = text.replace(left, 'PROBE_SWAP_MARKER').replace(right, left).replace('PROBE_SWAP_MARKER', right)
            pas.write_text(text, encoding='utf8', newline='\n')
            lfm = source / 'Forms/FreeLayerVisibilityDlg.lfm'
            text = lfm.read_text(encoding='utf8')
            pattern = r'(Columns = <\s*)(item\s+ButtonStyle = cbsCheckboxColumn.*?end\s*)(item\s+ButtonStyle = cbsCheckboxColumn.*?end)'
            text, count = re.subn(pattern, lambda m:m.group(1)+m.group(3)+'    '+m.group(2), text, count=1, flags=re.S)
            self.assertEqual(count, 1)
            lfm.write_text(text, encoding='utf8', newline='\n')
            policy = json.loads((ROOT / 'dynamic_callbacks.json').read_text(encoding='utf8'))
            for rule in policy['menu_adapters'].values():
                rule['sha256'] = tool.digest_file(source / rule['source'])
            policy_file = temp / 'policy.json'
            policy_file.write_text(json.dumps(policy), encoding='utf8')
            output = temp / 'out'
            process = subprocess.run([sys.executable, str(ROOT / 'freeship_ui_test.py'),
                '--source', str(source), '--case', 'layers', '--output', str(output),
                '--callback-policy', str(policy_file)], capture_output=True, timeout=240)
            report = json.loads((output / 'report.json').read_text(encoding='utf8'))
            self.assertEqual(process.returncode, 2, report)
            cases = {c['id']:c['status'] for c in report['cases']}
            for ident in ('layers.hide-surface', 'layers.hide-control-net', 'layers.sorted-row-target'):
                self.assertEqual(cases.get(ident), 'pass', report)
            effects = report['layer_discovery']['column_effects']
            self.assertEqual([(x['surface_after'], x['net_after']) for x in effects], [(True,False),(False,True)])

    def test_new_suites_catch_independent_regressions(self):
        mutations = [
            ('coordinates', 'Units/FreeSubdivisionControlPoint.inc',
             "2: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.Y);",
             "2: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.X);",
             'view.perspective.draw.all-labels.2'),
            ('coordinates', 'Units/FreeSubdivisionControlPoint.inc',
             "3: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.Z);",
             "3: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.X);",
             'view.perspective.draw.all-labels.3'),
            ('coordinates', 'Units/FreeSubdivisionControlPoint.inc',
             "1: PointLabel := PointLabel + FormatFloat('0.000', FCoordinate.X);",
             "1: PointLabel := PointLabel + 'X=' + FormatFloat('0.000', FCoordinate.X);",
             'view.perspective.draw.all-labels.1'),
            ('toolbar', 'Forms/Main.pas',
             'FreeShip.Visibility.ShowControlNet:=not FreeShip.Visibility.ShowControlNet;',
             'FreeShip.Visibility.ShowControlNet:=FreeShip.Visibility.ShowControlNet;',
             'toolbar.control-net-toggle'),
            ('display', 'Forms/freehullformwindow_panel.pas',
             'Viewport.ViewType:=fvProfile;', 'Viewport.ViewType:=fvPlan;', 'display.view.profile'),
            ('layers', 'Forms/FreeLayerVisibilityDlg.pas',
             'if CheckBox1.Checked then S := cbChecked else S := cbUnChecked;\n  for i:=1 to sgLayers.RowCount-1 do',
             'if CheckBox1.Checked then S := cbChecked else S := cbUnChecked;\n  for i:=1 to sgLayers.RowCount-2 do',
             'layers.bulk.hide-surfaces'),
            ('language', 'Units/FreeSafeLanguage.pas',
             '((Instance is TCustomEdit) or (Instance is TCustomComboBox)) then Exit;',
             "((Instance is TCustomEdit) or (Instance is TCustomComboBox)) then begin Content := 'CORRUPTED'; Exit; end;",
             'language.russian-preserves-input'),
            ('shortcuts', 'Forms/Main.pas',
             'Application.AddOnKeyDownBeforeHandler(StudioKeyDownBefore, True);',
             '{ test mutation: registration removed }', 'shortcuts.no-model'),
            ('shortcuts', 'Forms/Main.pas',
             '  StudioQuickLanguageClick(nil);',
             '  StudioQuickLanguageClick(nil); StudioQuickLanguageClick(nil);', 'shortcuts.no-model'),
            ('history', 'Forms/Main.pas',
             '   FreeShip.Edit.Redo;\n   UpdateMenu;',
             '   { mutation: redo omitted }\n   UpdateMenu;', 'history.redo'),
        ]

        def exercise(mutation):
            suite, filename, needle, replacement, expected = mutation
            with tool.temporary_workspace('fs-ui-suite-mutation-') as temp:
                source = temp / 'source'
                source.mkdir()
                for folder in tool.FOLDERS:
                    shutil.copytree(tool.DEFAULT_SOURCE / folder, source / folder)
                for p in tool.DEFAULT_SOURCE.iterdir():
                    if p.is_file() and p.suffix.lower() in tool.ROOT_SUFFIXES:
                        shutil.copy2(p, source / p.name)
                target = source / filename
                original = target.read_text(encoding='utf8')
                if original.count(needle) != 1:
                    return suite, f'mutation anchor changed: {filename}'
                target.write_text(original.replace(needle, replacement), encoding='utf8', newline='\n')
                # These mutations are deliberately reviewed test fixtures, never the user's live app.
                policy = json.loads((ROOT / 'dynamic_callbacks.json').read_text(encoding='utf8'))
                for rule in policy['menu_adapters'].values():
                    rule['sha256'] = tool.digest_file(source / rule['source'])
                policy_file = temp / 'test-policy.json'
                policy_file.write_text(json.dumps(policy), encoding='utf8')
                output = temp / 'output'
                process = subprocess.run([sys.executable, str(ROOT / 'freeship_ui_test.py'),
                    '--source', str(source), '--case', suite, '--output', str(output),
                    '--callback-policy', str(policy_file)],
                    capture_output=True, timeout=240)
                report = json.loads((output / 'report.json').read_text(encoding='utf8'))
                failed = {c['id'] for c in report['cases'] if c['status'] == 'fail'}
                observed = {c['id'] for c in report['cases'] if c.get('status') == 'review' and c.get('legacy_status') == 'fail'}
                if process.returncode not in (1, 2) or expected not in (failed | observed):
                    return suite, f'not detected: exit={process.returncode}; failed={failed}; error={report.get("error")}'
                return suite, None

        with ThreadPoolExecutor(max_workers=3) as pool:
            for suite, error in pool.map(exercise, mutations):
                with self.subTest(suite=suite):
                    self.assertIsNone(error)


if __name__ == '__main__':
    unittest.main()
