"""Receipt/target/revision fault cases using the actual native document service.

Invoked by qt-save-protection. Files are private copies under the current run;
model bytes, dirty state, undo/redo and recovery are checked independently.
"""
import copy
import json
from pathlib import Path
import qt_migration as q


def run(r,runtime,out,model):
    out.mkdir(parents=True);root=out/'recovery';counter=0
    target=out/'confirmed-船模.fbm';target.write_bytes(model.read_bytes())
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as service:
        def command(method,**fields):
            nonlocal counter
            counter+=1
            return service.command(method,output=out/f'operation-{counter}',**fields)
        def ok(method,**fields):return q.require_ok(command(method,**fields))
        def recovery_hashes():return {p.name:q.common.digest_file(p) for p in root.glob('*.fsr')}
        def rejected(name,**fields):
            before=dict(service.state);capsules=recovery_hashes();fbm=ok('document_status')['fbm_sha256']
            answer=command('mark_saved',**fields)
            r.case('receipt.'+name+'.rejected',not answer['ok'])
            r.case('receipt.'+name+'.preserves-state',service.state==before and recovery_hashes()==capsules and ok('document_status')['fbm_sha256']==fbm)
        loaded=ok('load',path=str(target),document_id=str(q.uuid.uuid4()),recovery_root=str(root),recovery_source=str(target))
        data=json.loads(Path(loaded['model']).read_text('utf8'))
        point=next(p for p in data['control_points'] if p['batch_movable']);position=list(point['p']);position[0]+=.125
        ok('move_point',point_id=point['id'],position=position)
        rejected('without-prepare',save_receipt={},saved_path=str(target))
        stage=ok('save');receipt=stage['save_receipt'];body=Path(stage['saved_file']).read_bytes()
        r.case('receipt.preparation-binds-exact-bytes',receipt['document_id']==service.state['document_id'] and receipt['revision']==service.state['revision'] and receipt['bytes']==len(body) and receipt['sha256']==q.common.digest_file(Path(stage['saved_file'])))
        r.case('receipt.preparation-keeps-dirty-and-recovery',service.state['modified'] and bool(recovery_hashes()))
        rejected('missing-receipt',save_receipt=None,saved_path=str(target))
        for field,value in [('token',str(q.uuid.uuid4())),('sha256','0'*64),('bytes',len(body)+1),('revision',receipt['revision']+1),('document_id',str(q.uuid.uuid4()))]:
            forged=copy.deepcopy(receipt);forged[field]=value
            rejected('wrong-'+field,save_receipt=forged,saved_path=str(target))
        rejected('destination-still-old',save_receipt=receipt,saved_path=str(target))
        rejected('missing-destination',save_receipt=receipt,saved_path=str(out/'absent.fbm'))
        rejected('directory-destination',save_receipt=receipt,saved_path=str(root))
        target.write_bytes(body+b'changed-after-write')
        rejected('destination-changed',save_receipt=receipt,saved_path=str(target))
        target.unlink();rejected('destination-deleted',save_receipt=receipt,saved_path=str(target))
        # Read-only operations and a rejected stale request do not invalidate a
        # valid preparation. A real revision change does.
        ok('snapshot');stale=command('save',expected_revision=1)
        r.case('receipt.stale-prepare-rejected',not stale['ok'])
        target.write_bytes(body);before=dict(service.state);mtime=target.stat().st_mtime_ns
        confirmed=ok('mark_saved',save_receipt=receipt,saved_path=str(target))
        r.case('receipt.valid-confirmation',confirmed['save_confirmed'] and not service.state['modified'] and not recovery_hashes())
        r.case('receipt.confirmation-does-not-change-history',all(service.state[k]==before[k] for k in q.STATE_KEYS if k!='modified'))
        r.case('receipt.confirmation-does-not-write-target',target.read_bytes()==body and target.stat().st_mtime_ns==mtime)
        clean=dict(service.state);again=ok('mark_saved',save_receipt=receipt,saved_path=str(target))
        r.case('receipt.repeat-is-idempotent',again['save_confirmed'] and service.state==clean and target.stat().st_mtime_ns==mtime)
        alias=out/'different-destination.fbm';alias.write_bytes(body)
        rejected('repeat-different-destination',save_receipt=receipt,saved_path=str(alias))
        ok('undo');r.case('receipt.undo-from-save-is-dirty',service.state['modified'])
        rejected('old-receipt-after-undo',save_receipt=receipt,saved_path=str(target))
        ok('redo');r.case('receipt.redo-to-save-is-clean',not service.state['modified'])
        position[0]+=.125;ok('move_point',point_id=point['id'],position=position)
        rejected('old-receipt-after-edit',save_receipt=receipt,saved_path=str(target))
        first=ok('save');second=ok('save');target.write_bytes(Path(second['saved_file']).read_bytes())
        r.case('receipt.each-prepare-gets-new-token',first['save_receipt']['token']!=second['save_receipt']['token'])
        rejected('superseded-preparation',save_receipt=first['save_receipt'],saved_path=str(target))
        ok('mark_saved',save_receipt=second['save_receipt'],saved_path=str(target))
        # Old document IDs cannot confirm a new document with renewed request
        # guards, even if its initial file bytes happen to match.
        loaded=ok('load',path=str(target),document_id=str(q.uuid.uuid4()),recovery_root=str(root),recovery_source=str(target))
        data=json.loads(Path(loaded['model']).read_text('utf8'));point=next(p for p in data['control_points'] if p['batch_movable'])
        position=list(point['p']);position[0]+=.125;ok('move_point',point_id=point['id'],position=position)
        rejected('receipt-from-previous-document',save_receipt=second['save_receipt'],saved_path=str(target))
        last=ok('save');bad=command('save',preview_jpeg='invalid%')
        r.case('receipt.invalid-preparation-rejected',not bad['ok'])
        rejected('cancelled-preparation',save_receipt=last['save_receipt'],saved_path=str(target))
        final=ok('save');target.write_bytes(Path(final['saved_file']).read_bytes())
        ok('mark_saved',saved_path=str(target))
        r.case('receipt.save-after-rejections-succeeds',not service.state['modified'] and not recovery_hashes())
