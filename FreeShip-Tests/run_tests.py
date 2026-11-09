"""Unified entry for existing isolated FreeShip regression suites."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
import test_artifacts

ROOT = Path(__file__).resolve().parent
SUITES = {
    'test-storage': ('test_storage.py', []),
    'qt-label-boundaries': ('qt_label_boundaries.py', []),
    'native-icon': ('native_icon_test.py', []),
    'icons': ('icon_integration_test.py', []),
    'label-reveal': ('overlay_performance.py', ['--label-reveal']),
    'native-animation': ('native_animation_test.py', []),
    'animation-base': ('overlay_performance.py', ['--cache-check']),
    'overlay-animation': ('overlay_performance.py', []),
    'fixes': ('improvement_regression.py', []),
    'ui': ('freeship_ui_test.py', []),
    'linesplan': ('linesplan_performance.py', []),
    'window': ('freeship_window_test.py', []),
    'historical': ('historical_regression.py', ['--run-reviewed']),
    'historical-assets': ('historical_assets.py', []),
    'dialog-opening': ('dialog_opening.py', []),
    'lines-incremental': ('section_incremental.py', []),
    'qt-linesplan': ('qt_linesplan_pilot.py', []),
    'qt-migration': ('qt_migration.py', []),
    'qt-editing': ('qt_editing.py', []),
    'qt-native-geometry': ('native_geometry.py', []),
    'qt-native-subdivision': ('native_subdivision.py', []),
    'qt-native-sections': ('native_sections.py', []),
    'qt-native-simplify': ('native_sections.py', ['--simplify-only']),
    'qt-native-linesplan': ('native_sections.py', ['--linesplan-only']),
    'qt-native-app': ('native_linesplan_app.py', []),
    'qt-topology': ('qt_topology.py', []),
    'qt-sections-editing': ('qt_sections_editing.py', []),
    'qt-overlays': ('qt_overlays.py', []),
    'qt-backgrounds': ('qt_backgrounds.py', []),
    'qt-startup': ('qt_startup.py', []),
    'qt-display': ('qt_display.py', []),
    'qt-auxiliary': ('qt_auxiliary.py', []),
    'qt-advanced': ('qt_advanced.py', []),
    'qt-modeling': ('qt_modeling.py', []),
    'qt-hydrostatics': ('qt_hydrostatics.py', []),
    'qt-performance': ('qt_performance.py', []),
    'qt-plates': ('qt_plates.py', []),
    'qt-exchange': ('qt_exchange.py', []),
    'qt-polycad': ('qt_polycad.py', []),
    'qt-curve-import': ('qt_curve_import.py', []),
    'qt-legacy-exchange': ('qt_legacy_exchange.py', []),
    'qt-cad': ('qt_cad.py', []),
    'qt-ghs': ('qt_ghs.py', []),
    'qt-ship-formats': ('qt_ship_formats.py', []),
    'qt-document': ('qt_document.py', []),
    'qt-publication': ('qt_publication.py', []),
    'qt-preferences': ('qt_preferences.py', []),
    'qt-viewport': ('qt_viewport.py', []),
    'qt-shading': ('qt_shading.py', []),
    'qt-hydrodynamics': ('qt_hydrodynamics.py', []),
    'qt-added-mass': ('qt_added_mass.py', []),
    'qt-grid-preview': ('qt_grid_preview.py', []),
    'qt-recovery': ('qt_recovery.py', []),
    'qt-save-protection': ('qt_save_protection.py', []),
    'qt-lines-cache': ('qt_lines_cache.py', []),
    'qt-local-geometry': ('qt_local_geometry.py', []),
    'qt-surface-serialization': ('qt_surface_serialization.py', []),
    'qt-dialog-layout': ('qt_dialog_layout.py', []),
    'qt-preview-image': ('qt_preview_image.py', []),
    'qt-project-hydro': ('qt_project_hydro.py', []),
    'qt-underwater': ('qt_underwater.py', []),
    'qt-hydro-auxiliary': ('qt_hydro_auxiliary.py', []),
    'qt-asymmetric-hydro': ('qt_asymmetric_hydro.py', []),
    'qt-lateral-projection': ('qt_lateral_projection.py', []),
    'qt-project-completion': ('qt_project_completion.py', []),
    'qt-calculation-layout': ('qt_calculation_layout.py', []),
    'qt-calculation-charts': ('qt_calculation_charts.py', []),
    'qt-calculation-chart-speed': ('qt_calculation_charts.py', ['--performance-only']),
    'qt-model-ui': ('qt_model_ui.py', []),
    'qt-preferences-ui': ('qt_preferences_ui.py', []),
    'qt-preferences-import': ('qt_preferences_ui.py', ['--import-only']),
}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--suite', choices=SUITES, action='append')
    p.add_argument('--list', action='store_true')
    p.add_argument('--output', type=Path)
    p.add_argument('--keep-transient-cache', action='store_true', help='Retain this run\'s native lines-plan cache for cache diagnostics; published evidence is always retained')
    p.add_argument('--runtime', type=Path, help='Reuse a fingerprint-verified Qt runtime for the selected supported suites')
    p.add_argument('--reuse-build', type=Path, help='Reuse a fingerprint-verified Qt bridge build')
    p.add_argument('--verify-deployment', type=Path, help='Run supported suites against this actual installed Qt directory')
    p.add_argument('--workflow-reference', type=Path, help='Completed qt-performance or qt-plates report folder for installed verification')
    p.add_argument('--oracle', type=Path, help='Reuse the fingerprint-verified qt-plates fixture observer')
    a = p.parse_args()
    if a.list:
        print(json.dumps(SUITES, ensure_ascii=False, indent=2))
        return 0
    reusable={'qt-document','qt-publication','qt-preferences','qt-preferences-ui','qt-model-ui','qt-viewport','qt-shading','qt-hydrodynamics','qt-added-mass','qt-grid-preview','qt-recovery','qt-save-protection','qt-lines-cache','qt-surface-serialization','qt-dialog-layout','qt-preview-image','qt-ghs','qt-ship-formats','qt-cad','qt-exchange','qt-polycad','qt-curve-import','qt-legacy-exchange','qt-migration','qt-performance','qt-plates'}
    reusable.add('qt-project-hydro')
    reusable.add('qt-underwater')
    reusable.add('qt-hydro-auxiliary')
    reusable.add('qt-asymmetric-hydro')
    reusable.add('qt-lateral-projection')
    reusable.add('qt-project-completion')
    reusable.add('qt-calculation-layout')
    reusable.add('qt-calculation-charts')
    reusable.add('qt-calculation-chart-speed')
    reusable.add('qt-local-geometry')
    reusable.add('qt-preferences-import')
    if a.workflow_reference and (a.suite not in (['qt-performance'],['qt-plates']) or not a.verify_deployment):p.error('Workflow reference requires one qt-performance or qt-plates deployment suite')
    if a.verify_deployment and a.suite and any(s in a.suite for s in ('qt-performance','qt-plates')) and not a.workflow_reference:p.error('Workflow deployment verification requires --workflow-reference')
    if a.oracle and (a.suite!=['qt-plates'] or a.verify_deployment):p.error('--oracle requires one qt-plates calculation suite')
    if a.runtime or a.reuse_build or a.verify_deployment:
        if not a.suite or any(s not in reusable for s in a.suite):p.error('Reuse options require explicit suites supporting the shared Qt runtime contract')
        if a.verify_deployment and (a.runtime or a.reuse_build):p.error('Choose deployment verification or build reuse')
    location=ROOT/'output-location.json'
    config=json.loads(location.read_text('utf8')) if location.exists() else {'report_root':str(ROOT/'reports')}
    report_root,space = test_artifacts.choose_report_root(config) if not a.output else (None,None)
    out = (a.output or report_root/('combined-'+time.strftime('%Y%m%d-%H%M%S'))).resolve()
    if out == ROOT.parent/'FreeShip-Plus-Lazarus' or ROOT.parent/'FreeShip-Plus-Lazarus' in out.parents:
        p.error('Output must be outside product source')
    out.mkdir(parents=True, exist_ok=False)
    retention_token = test_artifacts.claim(out)
    if space:(out/'storage-location.json').write_text(json.dumps(space,ensure_ascii=False,indent=2),encoding='utf8')
    cases = []
    for name in dict.fromkeys(a.suite or SUITES):
        script, extra = SUITES[name]
        cmd = [sys.executable, '-B', str(ROOT/script), '--output', str(out/name), *extra]
        for flag,value in (('--runtime',a.runtime),('--reuse-build',a.reuse_build),('--verify-deployment',a.verify_deployment),('--workflow-reference',a.workflow_reference),('--oracle',a.oracle)):
            if value:cmd += [flag,str(value.resolve())]
        case = {'id': name, 'status': 'error', 'command': cmd}
        try:
            with (out/(name+'.log')).open('wb') as log:
                r = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            report = json.loads((out/name/'report.json').read_text('utf-8-sig'))
            status = report.get('status', 'error')
            if status not in ('pass','fail','error','review','partial','not_run'):
                raise ValueError('Unknown child status')
            if status == 'pass' and r.returncode != 0:
                raise ValueError('Child exit disagrees with pass')
            case.update(status=status, exit_code=r.returncode, report=str(out/name/'report.json'))
        except Exception as e:
            case['error'] = str(e)
        cases.append(case)
        statuses = [c['status'] for c in cases]
        status = 'error' if 'error' in statuses else 'fail' if 'fail' in statuses else 'partial' if any(s != 'pass' for s in statuses) else 'pass'
        (out/'report.json').write_text(json.dumps({'status':status,'cases':cases,
            'limitations':['Child reports retain coverage boundaries; historical review is not integration or acceptance.']},ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(case,ensure_ascii=False),flush=True)
    try:
        retention = test_artifacts.finish(out, retention_token, keep=a.keep_transient_cache)
        print(json.dumps({'artifact_retention':str(out/'artifact-retention.json'),
                          'removed_cache_files':retention['removed_files'],
                          'retired_logical_mib':round(retention['logical_bytes']/2**20,2)}),flush=True)
    except Exception as error:
        # Cleanup failure must not rewrite product test results or delete more.
        (out/'artifact-retention-error.json').write_text(json.dumps({'status':'error','reason':str(error)}),encoding='utf8')
        print(json.dumps({'artifact_retention_error':str(error)}),flush=True)
    return {'pass':0,'partial':2,'fail':1,'error':3}[status]

if __name__ == '__main__':
    raise SystemExit(main())
