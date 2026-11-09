"""Qt preferences: real hidden actions, persistence, model isolation and history.

No desktop input. Each process uses a private preference file and model copy.
The backend cases independently check that presentation changes preserve FBM and
geometry, including rejected publication and an undo history trimmed at index 0.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing

def geometry(path):
    return q.geometry(json.loads(Path(path).read_text('utf8')))


def native(r, runtime, out, model):
    with q.Service(runtime/'FreeShipDocumentService.exe', out/'service', native_linesplan=True) as s:
        def call(name, method, **params):
            return q.require_ok(s.command(method, output=out/name, **params))
        first=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()))
        original=call('original','save')['saved_file']
        before=dict(s.state)
        settings={'undo_memory_mb':64,'fbm_encoding':'cp1252','graphic':{'pointsize':9,'fontsize':12,'stationcolor':0x123456,'normalcolor':0x112233}}
        changed=call('changed','configure_preferences',preferences=settings)
        r.case('native.preferences-do-not-edit-document',not s.state['modified'] and not s.state['can_undo'] and s.state['revision']==before['revision']+1)
        r.case('native.no-pascal-process',changed.get('adapter_pid')==0 and changed.get('adapter_requests')==0)
        r.case('native.exact-model-geometry',geometry(first['model'])==geometry(changed['model']))
        r.compare('native.exact-fbm-after-preferences',call('after','save')['saved_file'],original)
        state=dict(s.state);call('repeat','configure_preferences',preferences=settings)
        r.case('native.same-settings-are-noop',s.state==state)
        snapshot=json.loads(Path(changed['model']).read_text('utf8'));point=snapshot['control_points'][0];position=list(point['p']);position[0]+=0.125
        moved=call('move','move_point',point_id=point['id'],position=position)
        modified=call('modified','save')['saved_file'];call('undo','undo')
        state=dict(s.state);settings['graphic']['pointsize']=7
        rebound=call('rebind-with-redo','configure_preferences',preferences=settings)
        r.case('native.rebind-keeps-clean-and-redo',not s.state['modified'] and s.state['can_redo'] and s.state['revision']==state['revision']+1)
        redo=call('redo','redo');r.case('native.redo-keeps-new-settings',json.loads(Path(redo['model']).read_text('utf8'))['overlay']['point_size']==7)
        r.compare('native.redo-exact-fbm',call('redo-saved','save')['saved_file'],modified)
        r.case('native.redo-exact-geometry',geometry(moved['model'])==geometry(redo['model']))
        state=dict(s.state)
        bad_output=out/'blocked';bad_output.write_text('do not replace',encoding='utf8')
        invalid=[('invalid-size',{'graphic':{'pointsize':0}}),('invalid-theme',{'theme':'../outside'}),('invalid-encoding',{'fbm_encoding':'not-a-codec'}),('invalid-type',[])]
        for name,values in invalid:
            result=s.command('configure_preferences',preferences=values,output=out/name)
            r.case(name+'.rejected-without-state-change',not result['ok'] and s.state==state)
        result=s.command('configure_preferences',preferences={'graphic':{'pointsize':10}},output=bad_output)
        r.case('native.failed-publication-preserves-state',not result['ok'] and s.state==state and bad_output.read_text('utf8')=='do not replace')
        recovered=call('recovered','snapshot');r.case('native.failed-publication-preserves-presentation',json.loads(Path(recovered['model']).read_text('utf8'))['overlay']['point_size']==7)
        r.compare('native.failed-publication-preserves-fbm',call('recover-saved','save')['saved_file'],modified)
        # Several states, then reduce the memory limit while at the oldest state.
        for i in range(4):
            position[0]+=0.125;call('extra-'+str(i),'move_point',point_id=point['id'],position=position)
        while s.state['can_undo']:call('back-'+str(s.counter),'undo')
        oldest=call('oldest','save')['saved_file'];settings['undo_memory_mb']=0
        call('trim-at-oldest','configure_preferences',preferences=settings)
        r.case('native.trim-at-oldest-keeps-current-and-redo',not s.state['can_undo'] and s.state['can_redo'])
        r.compare('native.trim-preserves-current-fbm',call('trim-saved','save')['saved_file'],oldest)
        call('trim-redo','redo');r.case('native.trim-keeps-nearest-redo',s.state['can_undo'] and not s.state['can_redo'])


def ui(r,runtime,out,model):
    settings=out/'settings/preferences.json';settings.parent.mkdir(parents=True)
    seed={'format':'FreeShipQtPreferences','version':1,'owner_extension':{'keep':42},'values':{'external_component':{'keep':True}}}
    settings.write_text(json.dumps(seed),encoding='utf8');seed_bytes=settings.read_bytes()
    paths={key:str(out/('path-'+key)) for key in ('open','save','import','export','solver','temporary')}
    for path in paths.values():Path(path).mkdir()
    def run(name,commands):
        request=out/(name+'.json');request.write_text(json.dumps({'settings_file':str(settings),'commands':commands},ensure_ascii=False),encoding='utf8')
        result=q.run_child(r,name,runtime/'FreeShipQt.exe',['--verify-preferences',request],out/name,platform='offscreen')
        r.data.setdefault('production_runs',[]).append(result)
        if result['status'] in ('error','fail'):raise RuntimeError(name+' production probe failed: '+str(result.get('error')))
        if 'observations' not in result.get('artifacts',{}):raise RuntimeError(name+' probe has no observations; see not_run cases')
        rows=json.loads(Path(result['artifacts']['observations']).read_text('utf8'));r.case(name+'.all-commands-observed',len(rows)==len(commands));return rows
    def load():return dict(kind='load',path=str(model))
    def prefs(fields,**extra):return dict(kind='preferences',fields=fields,**extra)
    rows=run('cancel',[load(),prefs({'graphic.pointsize':9,'undo_memory_mb':31,'directories.export':paths['export']},action='cancel')])
    r.case('cancel.exact-preference-file',settings.read_bytes()==seed_bytes)
    r.case('cancel.no-model-or-appearance-change',rows[0]['revision']==rows[1]['revision'] and rows[0]['point_size']==rows[1]['point_size'] and rows[0]['toolbar_height']==rows[1]['toolbar_height'])
    colors={f['field']:f.get('color') for f in rows[1]['fields']}
    r.case('colors.preview-matches-accepted-qt-colors',colors['graphic.selectcolor']=='#e06519' and colors['graphic.viewportcolor']=='#e8efec' and colors['graphic.creaseedgecolor']=='#a04322')
    fields={'graphic.pointsize':7,'graphic.fontsize':12,'tool_icon_size':32,'undo_memory_mb':64,'fbm_encoding':'cp1252',**{'directories.'+k:v for k,v in paths.items()}}
    rows=run('apply',[load(),{'kind':'move'},{'kind':'draft','value':17.875},prefs(fields),{'kind':'undo'},{'kind':'redo'},{'kind':'save','path':str(out/'edited.fbm')}])
    draft,applied=rows[2:4]
    r.case('apply.live-graphics-and-toolbar',applied['point_size']==7 and applied['font_size']==12 and applied['toolbar_height']==70)
    r.case('apply.dirty-and-undo-preserved',applied['modified'] and applied['undo'] and applied['revision']==draft['revision']+1)
    r.case('apply.selection-camera-coordinate-draft-preserved',applied['selected'] and applied['camera_origin']==draft['camera_origin'] and applied['coordinate_draft']==17.875 and applied['coordinate_changed'])
    r.case('apply.geometry-preserved',geometry(applied['model'])==geometry(draft['model']))
    r.compare('apply.dxf-preserved',applied['dxf'],draft['dxf'])
    r.case('apply.directories-wired',Path(applied['export_start'])==Path(paths['export'])/'example.dxf' and Path(applied['open_start'])==Path(paths['open']))
    persisted=applied['persisted'];r.case('apply.foreign-fields-preserved',persisted['owner_extension']==seed['owner_extension'] and persisted['values']['external_component']==seed['values']['external_component'])
    r.case('apply.undo-restores-geometry-with-new-preferences',not rows[4]['modified'] and rows[4]['point_size']==7 and rows[4]['font_size']==12 and geometry(rows[4]['model'])==geometry(rows[0]['model']))
    r.case('apply.redo-restores-edit-with-new-preferences',rows[5]['modified'] and rows[5]['point_size']==7 and geometry(rows[5]['model'])==geometry(applied['model']))
    restart=run('restart',[load()])[0]
    r.case('restart.restores-preferences',restart['point_size']==7 and restart['font_size']==12 and restart['toolbar_height']==70 and restart['active']==persisted['values'])
    r.case('restart.uses-configured-temporary-directory',Path(paths['temporary']) in Path(restart['workspace_model']).parents)
    before=settings.read_bytes()
    for name,field in [('bad-codec',{'fbm_encoding':'not-a-codec'}),('bad-directory',{'directories.open':str(out/'absent')})]:
        result=run(name,[load(),prefs(field,reject_after_apply=True)])
        r.case(name+'.error-shown-without-change',bool(result[1]['message']) and result[0]['revision']==result[1]['revision'] and settings.read_bytes()==before)
    # Same production file may be owned by another open Qt instance/component.
    concurrent=json.loads(before);concurrent['values']['other_instance']=123
    result=run('conflict',[load(),prefs({'tool_icon_size':48},concurrent_write=concurrent,reject_after_apply=True)])
    r.case('conflict.preserve-other-writer-and-live-state',json.loads(settings.read_text('utf8'))==concurrent and result[1]['toolbar_height']==70 and '其他窗口' in result[1]['message'])
    before=json.loads(settings.read_text('utf8'))
    rows=run('backend-failure',[load(),prefs({'graphic.pointsize':10},block_next_request=True,reject_after_apply=True)])
    r.case('backend-failure.rolls-back-file-and-appearance',json.loads(settings.read_text('utf8'))==before and rows[1]['point_size']==7 and rows[1]['revision']==rows[0]['revision'] and bool(rows[1]['message']))
    rows=run('theme',[load(),prefs({'theme':'Original'}),{'kind':'save','path':str(out/'theme-only.fbm')}])
    r.case('theme.applied-without-model-edit',rows[1]['active']['theme']=='Original' and not rows[1]['modified'] and geometry(rows[1]['model'])==geometry(rows[0]['model']))
    r.case('theme.changes-rendering',q.common.digest_file(Path(rows[1]['image']))!=q.common.digest_file(Path(rows[0]['image'])))
    r.compare('theme.preserves-fbm',out/'theme-only.fbm',model)
    before=settings.read_bytes();rows=run('reset-cancel',[load(),prefs({},action='reset_graphics')])
    r.case('reset-cancel.keeps-saved-theme',settings.read_bytes()==before and rows[1]['active']['theme']=='Original')
    # No loaded model is necessary to save future-document preferences.
    result=run('empty-window',[prefs({'tool_icon_size':24})])[0]
    r.case('empty-window.preferences-save',result['toolbar_height']==62 and result['persisted']['values']['tool_icon_size']==24)
    custom=json.loads(settings.read_text('utf8'));custom['values']['graphic']['viewportcolor']=0x123456
    settings.write_text(json.dumps(custom),encoding='utf8')
    row=run('custom-color',[load()])[0]
    from PIL import Image
    pixels=Image.open(row['image']).convert('RGB')
    r.case('colors.saved-bgr-becomes-correct-rgb',row['appearance']['viewportcolor']==0x563412 and sum(n for n,c in pixels.getcolors(pixels.width*pixels.height) if c==(0x56,0x34,0x12))>10000)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--import-only',action='store_true')
    a=p.parse_args();runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for protected in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==protected or protected in out.parents:p.error('Use a private output directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-preferences-ui',runtime=str(runtime),limitations=['Hidden production actions do not certify desktop mouse, focus or visual acceptance.','Language support is owned by the separate translation workstream and is not implemented here.'])
    if a.import_only:r.data['suite']='qt-preferences-import'
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in ('qt_preferences_ui.py','qt_preferences_import.py','qt_migration.py','qt_editing.py','run_tests.py')}
    sources=q.current_sources(q.QT);original=q.DEFAULT_MODELS[0];original_hash=q.common.digest_file(original);start=time.perf_counter()
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        for n,h in manifest['binaries_sha256'].items():
            if q.common.digest_file(runtime/n)!=h:raise RuntimeError('Runtime file mismatch: '+n)
        r.case('runtime-manifest',True);model=out/'模型-Женя.fbm';shutil.copy2(original,model)
        if not a.import_only:
            native(r,runtime,out/'native',model);ui(r,runtime,out/'ui',model)
        from qt_preferences_import import legacy_ui
        legacy_ui(r,runtime,out/'legacy',model)
        r.case('protected-model-unchanged',q.common.digest_file(original)==original_hash)
        r.case('runtime-unchanged',all(q.common.digest_file(runtime/n)==h for n,h in manifest['binaries_sha256'].items()))
        r.case('source-stable',q.current_sources(q.QT)==sources)
    except Exception as e:r.case('execution',status='error',reason=f'{type(e).__name__}: {e}')
    r.case('desktop-acceptance',status='not_run',reason='No desktop input or visual acceptance.');r.data['elapsed_s']=time.perf_counter()-start;editing.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
