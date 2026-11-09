"""Read-only legacy settings import through the real Qt preferences callbacks.

Fixtures follow FreePreferences.inc INI and DTA wire formats. Expected values
are independent literals, not a round trip through the new parser. Only owned
offscreen dialogs are driven; this does not certify desktop focus/input.
"""
import json
from pathlib import Path
import qt_migration as q


def legacy_ui(r, runtime, out, model):
    out.mkdir(parents=True)
    settings = out/'settings/preferences.json'
    settings.parent.mkdir()
    kept = out/'kept'
    kept.mkdir()
    folder = out/'旧目录-Женя'
    folder.mkdir()
    seed = {'format': 'FreeShipQtPreferences', 'version': 1,
            'owner_extension': {'keep': 42}, 'values': {
                'external_component': {'keep': True}, 'theme': 'Original',
                'graphic': {'normalcolor': 0x010A0B}, 'fbm_encoding': 'cp1252',
                'directories': {'export': str(kept), 'import': str(kept)}}}
    settings.write_text(json.dumps(seed), encoding='utf8')
    seed_bytes = settings.read_bytes()
    source = out/'old-portable'
    def write(name, data):
        path = source/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode('utf8'))
        return path
    write('FreeShip.ini', '[General]\nMaxUndoMemory=18\nFbmEncoding=UTF-8\n'
          '[Graphic]\nToolIconSize=20\n[Directories]\nSaveDirectory='+str(folder)+'\n')
    ini = write('.coordinate-settings/FreeShip.ini',
        '\ufeff[GENERAL]\nMaxUndoMemory=48\nmaxundomemory=999\nFbmEncoding="cp1252"\nLanguage=xx\n'
        '[Graphic]\nTheme=Custom\nStudioThemeInitialized=1\nToolIconSize=28\n'
        '[Directories]\nOpenDirectory="'+str(folder)+'"\nImportDirectory=../relative\n'
        'ExportDirectory='+str(out/'absent')+'\nExecDirectory='+str(folder)+'\nTempDirectory='+str(folder)+'\n'
        '[Window]\nLeft=9999\n[RecentFiles]\nFile1=do-not-open.fbm\n')
    write('Themes/Parent/theme.ini', '[Theme]\nParentTheme=Grand\n'
          '[Graphic]\nPointSize=4\nFontSize=13\nStationColor=$010203\nGridColor=clWhite\n')
    write('Themes/Grand/theme.ini', '[Graphic]\nMarkerColor=0\n')
    write('Themes/Custom/theme.ini', '[Graphic]\nPointSize=2\n')
    write('.coordinate-settings/Themes/Custom/theme.ini', '[Theme]\nParentTheme=Parent\n'
          '[Graphic]\nPointSize=9\nFontSize=150\nStationColor=clRed\nViewportColor=$123456\n'
          'UnderWaterColorAlpha=127\nZebraStripeColor=12\nIntersectionLineWidth=3\n')
    load = {'kind': 'load', 'path': str(model)}
    def prefs(files, **kwargs):
        return dict(kind='preferences', legacy_imports=[str(p) if p else None for p in files], **kwargs)
    def run(name, commands):
        request = out/(name+'.json')
        request.write_text(json.dumps({'settings_file': str(settings), 'commands': commands}, ensure_ascii=False), encoding='utf8')
        result = q.run_child(r, 'import.'+name, runtime/'FreeShipQt.exe',
                             ['--verify-preferences', request], out/name, platform='offscreen')
        r.data.setdefault('production_runs', []).append(result)
        if result['status'] in ('error', 'fail'):
            raise RuntimeError(name+' production callback failed: '+str(result.get('error')))
        evidence = result.get('artifacts', {}).get('observations')
        if not evidence:
            raise RuntimeError(name+' has no observation; see callback discovery not_run')
        rows = json.loads(Path(evidence).read_text('utf8'))
        r.case('import.'+name+'.commands-observed', len(rows)==len(commands))
        return rows
    def case(name, condition):
        r.case('import.'+name, condition)
    def geom(row):
        result = q.geometry(json.loads(Path(row['model']).read_text('utf8')))
        # Snapshot settings includes this preference-only display field. Its
        # requested 255 -> 127 transition is asserted separately below. Every
        # other setting and every model coordinate remains exactly compared.
        result['settings'].pop('underwater_color_alpha')
        return result
    def noncolor_dxf(row):
        lines = Path(row['dxf']).read_text('utf8').splitlines()
        assert len(lines)%2==0
        return [(code, value) for code, value in zip(lines[::2], lines[1::2]) if code.strip()!='62']

    before_hashes = {str(p): q.common.digest_file(p) for p in source.rglob('*') if p.is_file()}
    rows = run('cancel', [load, prefs([ini], action='cancel')])
    imported = rows[1]['imports'][0]
    draft = imported['draft']
    report = imported['report']
    graphic = draft['graphic']
    case('cancel.keeps-file-and-live-state', settings.read_bytes()==seed_bytes and rows[0]['active']==rows[1]['active'] and rows[0]['revision']==rows[1]['revision'])
    case('draft.no-write-before-confirm', imported['persisted']==seed)
    case('ini.user-over-global-first-duplicate', draft['undo_memory_mb']==48 and draft['fbm_encoding']=='cp1252' and draft['tool_icon_size']==28)
    case('ini.user-theme-over-global-one-parent', graphic['pointsize']==9 and graphic['fontsize']==150 and graphic['gridcolor']==0xffffff and graphic['markercolor']==0x8000ff)
    case('ini.color-formats-and-alpha', graphic['stationcolor']==255 and graphic['viewportcolor']==0x123456 and graphic['underwatercoloralpha']==127)
    fields = {f['field']: f for f in rows[1]['fields']}
    case('draft.controls-refreshed-without-clamping', fields['graphic.fontsize']['value']==150 and fields['graphic.pointsize']['value']==9 and fields['undo_memory_mb']['value']==48 and fields['directories.open']['value']==str(folder).replace('\\', '/'))
    case('draft.color-preview-is-rgb', fields['graphic.viewportcolor']['color']=='#563412' and fields['graphic.stationcolor']['color']=='#ff0000')
    case('ini.missing-relative-paths-retain-existing', Path(draft['directories']['import'])==kept and Path(draft['directories']['export'])==kept)
    case('ini.valid-directories-and-global-inheritance', all(Path(draft['directories'][key])==folder for key in ('open','save','solver','temporary')))
    case('ini.partial-import-reported', '../relative' in '\n'.join(report['notes']) and str(out/'absent') in '\n'.join(report['notes']) and 'zebrastripecolor' in '\n'.join(report['notes']))
    case('ini.source-provenance', len(report['sources'])==4 and all(Path(path).is_file() for path in report['sources']))
    case('ini.language-and-ui-state-not-adopted', set(draft)<=set(seed['values'])|{'graphic','directories','tool_icon_size','undo_memory_mb','fbm_encoding'} and not any(k in draft for k in ('language','window','recent_files')))
    rows = run('picker-cancel', [load, prefs([None], action='cancel')])
    case('file-picker-cancel-keeps-draft-and-file', rows[1]['draft']==seed['values'] and settings.read_bytes()==seed_bytes)

    # All other commands use the already tested preferences save transaction.
    rows = run('apply', [load, {'kind':'move'}, {'kind':'draft','value':17.875},
        {'kind':'save','path':str(out/'before.fbm')},
        prefs([ini], fields={'graphic.fontsize':8}),
        {'kind':'save','path':str(out/'after.fbm')}, {'kind':'undo'}, {'kind':'redo'}])
    before, applied = rows[3:5]
    case('apply.live-values-and-persisted-draft', applied['point_size']==9 and applied['font_size']==8 and applied['active']==applied['draft']==applied['persisted']['values'])
    case('apply.keeps-foreign-fields', applied['persisted']['owner_extension']==seed['owner_extension'] and applied['active']['external_component']==seed['values']['external_component'])
    case('apply.geometry-exact', geom(before)==geom(applied))
    case('apply.only-requested-display-alpha',
         json.loads(Path(before['model']).read_text('utf8'))['settings']['underwater_color_alpha']==255 and
         json.loads(Path(applied['model']).read_text('utf8'))['settings']['underwater_color_alpha']==127)
    case('apply.dxf-exact-except-selected-colors', noncolor_dxf(before)==noncolor_dxf(applied))
    r.compare('import.apply.fbm-exact', out/'after.fbm', out/'before.fbm')
    case('apply.preserves-camera-selection-coordinate-draft', applied['selected'] and applied['camera_origin']==before['camera_origin'] and applied['coordinate_draft']==before['coordinate_draft'] and applied['coordinate_changed']==before['coordinate_changed'])
    case('apply.no-model-edit-or-extra-undo', not applied['modified'] and applied['undo']==before['undo'] and geom(rows[6])==geom(rows[0]) and geom(rows[7])==geom(before))
    case('apply.undo-redo-keeps-palette', rows[6]['appearance']==applied['appearance']==rows[7]['appearance'])
    saved = settings.read_bytes()
    # Remove the external theme from the lookup location: saved palette must be
    # self-contained. Preserve the test source as evidence, do not delete it.
    theme_path = source/'.coordinate-settings/Themes/Custom/theme.ini'
    theme_path.rename(theme_path.with_suffix('.retained'))
    restart = run('restart', [load])[0]
    theme_path.with_suffix('.retained').rename(theme_path)
    case('restart.independent-of-old-theme', restart['active']==applied['active'] and restart['appearance']==applied['appearance'] and restart['point_size']==9 and settings.read_bytes()==saved)
    case('restart.temporary-directory', folder in Path(restart['workspace_model']).parents)

    # Malformed files cannot partially overwrite a successful imported draft.
    bads = {
        'utf8': b'[General]\nMaxUndoMemory=12\n\xc3',
        'nul': b'[General]\nMaxUndoMemory=12\n\x00',
        'oversize': b';'*(4*1024*1024+1),
        'unrelated': '[Editor]\nTabSize=4\n',
        'theme-path': '[General]\nMaxUndoMemory=12\n[Graphic]\nTheme=../escape\n',
    }
    bad_files = [write('bad-'+name+'.ini', data) for name, data in bads.items()]
    bad_files += [write('short.dta','2\n0\n'), write('count.dta','\n'.join(['2']+['0']*17+['']*4+['0','100001'])+'\n')]
    commands = [load]+[prefs([ini, path], action='cancel') for path in bad_files]
    rows = run('invalid-files', commands)
    for path, row in zip(bad_files, rows[1:]):
        first, second = row['imports']
        case('reject.'+path.stem, bool(second['message']) and second['draft']==first['draft'] and second['persisted']==first['persisted'])
    case('invalid-files.no-persistence', settings.read_bytes()==saved)

    missing = write('missing-theme.ini','[General]\nMaxUndoMemory=33\n[Graphic]\nTheme=MissingCustom\n')
    parent_missing = write('missing-parent.ini','[General]\nMaxUndoMemory=34\n[Graphic]\nTheme=Orphan\n')
    write('Themes/Orphan/theme.ini','[Theme]\nParentTheme=Absent\n[Graphic]\nPointSize=99\n')
    invalid_values = write('invalid-values.ini', '[General]\nMaxUndoMemory=36\nFbmEncoding=not-a-codec\n'
                           '[Graphic]\nToolIconSize=-3\n[Directories]\nOpenDirectory=relative\n')
    rows = run('partial', [load]+[prefs([path], action='cancel') for path in (missing,parent_missing,invalid_values)])
    for row, undo in zip(rows[1:], (33,34,36)):
        case('partial.'+str(undo)+'.only-recognized-valid-values', row['draft']['undo_memory_mb']==undo and row['draft']['graphic']==applied['active']['graphic'] and row['draft']['directories']==applied['active']['directories'] and bool(row['imports'][0]['report']['notes']))
    case('partial.invalid-encoding-and-size-not-adopted', rows[3]['draft']['fbm_encoding']=='cp1252' and rows[3]['draft']['tool_icon_size']==28)

    # Legacy DTA is ordered, including optional fields and recent-file count.
    dta_values = [6]+[101+i for i in range(17)]
    tail = [str(folder)]*4 + ['201','1','never-open.fbm'] + ['202','203','204','205','206','207','1 2 3 4 0','Russian','44']
    dta = write('FreeShip.dta','\n'.join(map(str,dta_values+tail))+'\n')
    rows = run('dta', [load,prefs([dta]),{'kind':'save','path':str(out/'dta.fbm')}])
    graphic = rows[1]['active']['graphic']
    case('dta.fixed-and-optional-order', rows[1]['point_size']==6 and graphic['buttockcolor']==101 and graphic['viewportcolor']==117 and graphic['diagonalcolor']==201 and graphic['leakpointcolor']==202 and graphic['hydrostaticsfontcolor']==206 and rows[1]['active']['undo_memory_mb']==44)
    case('dta.absent-fields-retain-current', graphic['fontsize']==8 and graphic['underwatercoloralpha']==127 and rows[1]['active']['fbm_encoding']=='cp1252' and rows[1]['active']['tool_icon_size']==28)
    case('dta.directory-order', all(Path(rows[1]['active']['directories'][k])==folder for k in ('open','save','import','export')))
    r.compare('import.dta.fbm-unchanged', out/'dta.fbm', model)
    case('dta.model-geometry-unchanged', geom(rows[0])==geom(rows[1]))
    minimal = write('minimal.dta','\n'.join(map(str,dta_values))+'\n')
    row = run('minimal', [load,prefs([minimal],action='cancel')])[1]
    case('dta.earliest-short-layout-supported', not row['message'] and row['draft']['graphic']['pointsize']==6)
    codec = write('codec.ini','[General]\nFbmEncoding=UTF-8\n')
    rows = run('encoding-ownership', [load, prefs([codec]), {'kind':'save','path':str(out/'encoding.fbm')}])
    case('encoding.future-document-choice-saved', rows[1]['active']['fbm_encoding']=='UTF-8')
    r.compare('import.encoding.current-document-bytes-unchanged', out/'encoding.fbm', model)
    case('encoding.current-geometry-unchanged', geom(rows[0])==geom(rows[1]))
    case('legacy-source-bytes-unchanged', all(q.common.digest_file(Path(p))==h for p,h in before_hashes.items()))
