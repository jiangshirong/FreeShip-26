"""Offline latest-source audit; never turn inherited evidence into new approval.

Capture a new immutable revision. Validate it via the unified qt-context-audit
suite. No credentials, model calls, catalog application or runtime installation.
"""
import argparse
from collections import Counter, defaultdict
import copy
import json
from pathlib import Path
import re
import sys

QT=Path(__file__).resolve().parents[1]
TOOL_ROOT=QT
sys.path.insert(0,str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import inventory, compare, digest, CJK

APPROVED={'model_reviewed','source_reviewed','delta_revalidated'}

INSTALLER={
 '安装目录和缓存不能经过链接：':('The installation directory and cache must not contain links: ','Error raised when an installation or cache path traverses a filesystem link. The offending path follows the colon.'),
 '无效的安装文件名':('Invalid installation filename','Error rejecting an unsafe filename supplied by the installation manifest.'),
 '安装文件超出目标目录':('An installation file is outside the target directory','Error rejecting a payload path that escapes the selected installation directory.'),
 '安装包身份不一致':('Installation package identity mismatch','Error when the recovery package identity does not match the expected product identity.'),
 '恢复安装包的校验失败；现有程序未改动':('Recovery package verification failed; the existing application was left unchanged','Error when the saved MSI recovery package fails its checksum check before installation proceeds.'),
 '恢复维护程序的校验失败；现有程序未改动':('Recovery maintenance program verification failed; the existing application was left unchanged','Error when the saved recovery maintenance executable fails its checksum check.'),
 '安装包内部身份不一致':('Inconsistent identity within the installation package','Error when the embedded MSI metadata differs from the outer installation bundle.'),
 '缺少安装文件清单':('Missing installation file manifest','Error when the bundle does not provide the payload file list needed to verify installation.'),
 '安装登记未恢复到已安装状态':('Installation registration was not restored to the installed state','Recovery verification error: Windows Installer does not report the expected installed product state.'),
 '安装文件校验失败：':('Installed file verification failed: ','Error prefix identifying an installed file whose content does not match the manifest. Preserve the following filename.'),
 '安装位置登记不一致':('Installation location registration mismatch','Verification error when the registered installation directory differs from the expected directory.'),
 '开始菜单入口未恢复':('The Start menu shortcut was not restored','Recovery verification error when the application Start menu shortcut is still missing.'),
 'Windows Installer 返回 ':('Windows Installer returned ','Error prefix followed by the numeric Windows Installer return code. Preserve the code.'),
 '升级失败时自动恢复此前安装版本。用户模型和偏好在卸载后保留。':('If an upgrade fails, the previous installed version is restored automatically. User models and preferences are retained after uninstallation.','Installer description explaining upgrade recovery and retention of user data during uninstallation.'),
 '正在恢复此前安装状态，请稍候……':('Restoring the previous installation state. Please wait…','Progress message while the installer repairs or restores the previously installed version.'),
 '恢复：移除失败版本':('Recovery: removing the failed version','Progress phase while uninstalling the failed candidate package during rollback.'),
 '恢复：修复旧登记':('Recovery: repairing the previous registration','Progress phase while repairing the previous Windows Installer product registration.'),
 '恢复：还原旧版本':('Recovery: restoring the previous version','Progress phase while reinstalling the previously saved package.'),
 '失败版本仍留有安装文件：':('Files from the failed version remain: ','Recovery verification error followed by a path to a file left by the failed version.'),
 '恢复后仍有多余产品登记':('Unexpected product registrations remain after recovery','Recovery verification error when additional registrations for the product family remain.'),
 '首次安装失败后仍有残留':('Remnants remain after the failed initial installation','Error when cleanup after a failed first installation leaves files or product registration behind.'),
 '请选择有效的程序目录':('Select a valid application directory','Validation error requiring a suitable directory for installing the program.'),
 '发现本产品的其他安装登记，请先恢复原安装记录':('Other installation registrations for this product were found. Restore the original installation record first.','Error blocking a new installation while conflicting product-family registrations require recovery.'),
 '当前安装登记不完整，无法开始新升级':('The current installation registration is incomplete. A new upgrade cannot start.','Error blocking an upgrade because the existing installation cannot be reliably identified.'),
 '目标目录已有未由本安装器管理的同名文件：':('The target directory contains a file with the same name that this installer does not manage: ','Error preventing overwrite of an unmanaged file; the conflicting filename follows.'),
 '请先关闭安装目录中正在运行的程序':('Close programs running from the installation directory first','Error requiring applications using the destination directory to be closed before maintenance.'),
 '安装磁盘至少需要 300 MiB 可用空间':('The installation drive requires at least 300 MiB of free space','Error when the destination drive has less than the required free space. Preserve 300 MiB.'),
 '另一个安装或恢复任务正在运行':('Another installation or recovery task is running','Error when the product maintenance mutex is already held by another task.'),
 '没有可维护的安装':('No installation is available for maintenance','Error when a repair or recovery request has no registered installation to maintain.'),
 '同版本安装包已改变，不能覆盖已有恢复包':('The installation package for this version has changed. The existing recovery package cannot be overwritten.','Error preserving the original recovery MSI when a different package reuses the same version identity.'),
 '同版本维护程序已改变，不能覆盖已有恢复程序':('The maintenance program for this version has changed. The existing recovery program cannot be overwritten.','Error preserving the original recovery executable when different content reuses the same version identity.'),
 '没有可卸载的安装':('No installation is available to uninstall','Error when uninstallation is requested without a registered installed product.'),
 '正在卸载程序……':('Uninstalling the application…','Progress message while Windows Installer removes the installed program.'),
 '正在安装程序……':('Installing the application…','Progress message while Windows Installer installs or upgrades the application.'),
 '卸载后仍有程序文件或安装登记':('Program files or installation registration remain after uninstallation','Error when post-uninstallation verification finds retained program-owned files or registration.'),
 '仅测试身份允许指定缓存目录':('Only test builds may specify a cache directory','Error rejecting a custom cache directory in a normal installer build.'),
 '未知安装参数：':('Unknown installation argument: ','Command-line validation error followed by the unrecognized argument.'),
 '检查安装包需要完整安装器及报告路径':('Package inspection requires a complete installer and a report path','Diagnostic command error when the embedded package or output report path is unavailable.'),
 ' 安装':(' Setup','Suffix added to the product name to form the installer window title.'),
 '卸载程序并保留船模和偏好设置。':('Uninstall the application and retain ship models and preferences.','Initial explanatory message in uninstall mode, before the user starts uninstallation.'),
 '安装或更新 FreeShip 26。升级失败时将自动恢复此前版本。':('Install or update FreeShip 26. If the upgrade fails, the previous version will be restored automatically.','Initial installer message explaining installation or upgrade and rollback behavior.'),
 '卸载':('Uninstall','Button that starts uninstallation when the installer is in uninstall mode.'),
 '开始':('Start','Button that begins the selected installation or upgrade operation.'),
 '取消':('Cancel','Button that closes the installer before an installation or maintenance operation begins.'),
 '目录无效：':('Invalid directory: ','Validation message shown after the destination cannot be resolved as a valid path; the exception detail follows.'),
 '关闭':('Close','Installer button caption after the background operation has finished; closes the installer window.'),
 '上次安装被中断，已恢复此前版本。':('The previous installation was interrupted. The earlier version has been restored.','Completion feedback after recovery of an interrupted installation.'),
 '卸载完成，用户文件和设置已保留。':('Uninstallation completed. User files and settings have been retained.','Successful uninstall completion message describing retention of user data.'),
 '操作完成。':('Operation completed.','Successful completion feedback for installation or maintenance.'),
 '本次安装未完成，已恢复此前安装状态。':('This installation did not complete. The previous installation state has been restored.','Failure feedback when rollback has successfully recovered the previous installation state.'),
 '操作未完成，请查看安装日志：':('The operation did not complete. See the installation log: ','Failure feedback followed by the path to the installer JSON log.'),
 'FreeShip 26 安装':('FreeShip 26 Setup','Title of the installer exception dialog.'),
 '语言':('Language','Field label for the installer language selector. Switching it updates installer-owned captions and feedback; it does not change the destination path or run an installation.'),
 '不支持的安装语言：':('Unsupported installer language: ','Error prefix when the requested installer language is not embedded in this installer. The requested language identifier follows unchanged.'),
}

def load(p):return json.loads(p.read_text('utf8'))
def write(p,value):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def jsonl(p,rows):p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf8')

def apply_site_review(record,site,decision,source_files,is_new):
    """Bind explicit dispositions to exact source; a file review cannot approve new copy."""
    explicit=decision.get('site_reviews',{}).get(site['candidate_id'])
    if explicit is None:
        if is_new and decision.get('require_explicit_new_site_reviews',True):
            record.update(status='review',gaps_en=['New lexical site lacks an explicit per-site functional disposition.'])
        return
    if (explicit.get('source_text')!=site['text'] or explicit.get('file')!=site['file']
        or explicit.get('file_sha256')!=source_files[site['file']]):
        raise ValueError('Explicit site review is stale: '+site['candidate_id'])
    if explicit.get('status') not in APPROVED|{'technical','reference_only','review'}:
        raise ValueError('Invalid explicit site disposition')
    if not explicit.get('reason_en') or CJK.search(explicit['reason_en']):
        raise ValueError('Explicit site review requires an English functional reason')
    dependencies=explicit.get('dependency_files',{})
    if any(source_files.get(f)!=h for f,h in dependencies.items()):
        raise ValueError('Explicit context dependency is stale: '+site['candidate_id'])
    if 'mt_context_en' in explicit and record.get('mt_context_en')!=explicit['mt_context_en']:
        record['previous_mt_context_en']=record.get('mt_context_en')
    for key in ('status','role','canonical_en','mt_context_en','gaps_en','constraints','catalog_context','runtime_reachability'):
        if key in explicit:record[key]=copy.deepcopy(explicit[key])
    record['explicit_site_review']=explicit
    record['dependency_files'].update(dependencies)

def deduplicate_units(records):
    """Reuse a translation only when English, context, role and constraints agree."""
    grouped={}
    for record in records:
        from qt_text_audit import TOKENS
        constraints=record.get('constraints') or dict(placeholders=sorted(TOKENS.findall(record['canonical_en'])))
        identity=digest(json.dumps([record['canonical_en'],record['mt_context_en'],record['role'],constraints],sort_keys=True,ensure_ascii=False).encode())
        unit=grouped.setdefault(identity,dict(id='qt.mt.'+identity[:24],source_language='en',text=record['canonical_en'],context=record['mt_context_en'],role=record['role'],constraints=constraints,use_site_ids=[],source_bindings=[],dependency_files={},evidence_refs=[]))
        unit['use_site_ids'].append(record['id'])
        unit['source_bindings'].append(dict(use_site_id=record['id'],source_key=record['source_text'],catalog_context=record.get('catalog_context')))
        for path,value in record['dependency_files'].items():
            if path in unit['dependency_files'] and unit['dependency_files'][path]!=value:raise ValueError('Conflicting dependency hashes')
            unit['dependency_files'][path]=value
        unit['evidence_refs']=sorted(set(unit['evidence_refs'])|set(record['evidence_refs']))
    return list(grouped.values())

def main():
    global QT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous',type=Path,required=True)
    parser.add_argument('--decisions',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--source-root',type=Path,help='Optional captured source tree; later live edits remain a separate increment.')
    args=parser.parse_args();previous=args.previous.resolve();out=args.output.resolve()
    if args.source_root:QT=args.source_root.resolve()
    if out.exists():raise ValueError('Use a new immutable audit revision')
    cur=inventory(QT)
    if cur['errors']:raise ValueError(cur['errors'])
    decision=load(args.decisions);old=load(previous/'inventory.json');delta=compare(cur,old)
    for f,item in decision['reviewed_files'].items():
        if cur['files'].get(f)!=item['sha256']:raise ValueError('Delta review is stale: '+f)
    out.mkdir();write(out/'inventory.json',cur);write(out/'delta.json',delta);write(out/'source_delta_decisions.json',decision)
    sources={}
    for f,h in cur['files'].items():
        b=(QT/f).read_bytes()
        if digest(b)!=h:raise ValueError('Source changed during capture: '+f)
        p=out/'source'/f;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);sources[f]=b.decode('utf-8-sig')
    packfile=QT/'src/app/languages/en.json';packdata=packfile.read_bytes();pack=json.loads(packdata)['messages'];(out/'english.snapshot.json').write_bytes(packdata)
    prior={r['candidate_id']:r for r in map(json.loads,(previous/'contexts.jsonl').read_text('utf8').splitlines())}
    oldsites={s['candidate_id']:s for s in old['candidates']}
    excluded=({r['candidate_id']:r for r in load(previous/'excluded.json')} if (previous/'excluded.json').exists() else
        {r['candidate_id']:r for r in map(json.loads,(previous/'site_audit.jsonl').read_text('utf8').splitlines()) if r['classification']=='prior_exclusion_retained'})
    resolved={}
    if decision.get('reachability_resolution'):
        proof=load(Path(decision['reachability_resolution']))
        if any(digest((QT/f).read_bytes())!=h for f,h in proof['source_sha256'].items()):raise ValueError('Reachability proof is stale')
        resolved={r['id']:r for r in proof['records']}
    bytext=defaultdict(list)
    for r in prior.values():bytext[r['source']['file'],r['source_text']].append(r)
    records=[];audit=[];gaps=[];corrections=[];evidence={};ready=[];runtime=[]
    for site in cur['candidates']:
        cid=site['candidate_id'];f=site['file'];text=site['text'];r=prior.get(cid)
        if r and r['source_text']!=text:r=None
        change='new_use_site' if cid not in oldsites else 'modified_text' if oldsites[cid]['text']!=text else 'owner_changed' if oldsites[cid]['file_sha256']!=site['file_sha256'] else 'unchanged'
        if r is None and f!='tools/installer/Setup.cs' and cid in excluded and cid not in decision.get('site_reviews',{}):
            audit.append(dict(candidate_id=cid,file=f,line=site['line'],source_text=text,change=change,classification='prior_exclusion_retained',exclusion_provenance=excluded[cid],semantic_acceptance=False));continue
        original=copy.deepcopy(r) if r else None
        if r:r=copy.deepcopy(r)
        else:
            r=dict(id='qt.site.'+cid[:24],candidate_id=cid,source_text=text,canonical_en=None,mt_context_en=None,role='technical',status='technical',dependency_files={f:site['file_sha256']},gaps_en=[],writer='repository_assistant',reviewer='repository_assistant')
            relocated=[p for p in bytext[f,text] if p['status'] in APPROVED]
            if relocated:
                # New production references need a source decision, not text equality.
                r['related_prior_site_ids']=[p['id'] for p in relocated]
        stale={p for p,h in r['dependency_files'].items() if cur['files'].get(p)!=h}
        unreviewed=stale-set(decision['reviewed_files'])
        if unreviewed:
            r['status']='review';r['gaps_en']=['Changed dependency has no exact source-delta review: '+p for p in sorted(unreviewed)]
        elif stale:
            r['previous_dependency_files']=r['dependency_files'].copy()
            r['dependency_files']={p:cur['files'][p] for p in r['dependency_files']}
            r['delta_review_files']=sorted(stale)
            if r['status'] in APPROVED:r['status']='delta_revalidated'
        if original is None:
            if f in decision['reviewed_files'] and relocated and len({(p.get('canonical_en'),p.get('mt_context_en'),p.get('role')) for p in relocated})==1:
                chosen=relocated[0]
                r.update(canonical_en=chosen['canonical_en'],mt_context_en=chosen['mt_context_en'],role=chosen['role'],status='source_reviewed',dependency_files={key:cur['files'][key] for key in chosen['dependency_files']})
                r['relocated_source_review']='Exact literal and unambiguous prior meaning retained in a file with a recorded localization-only source delta.'
            if f=='tools/installer/Setup.cs' and text.startswith('C:\\'):
                r['mt_context_en']='Synthetic Chinese model/log path used only by the private installer language observer to verify that user data is preserved; not product copy.'
            elif f.startswith('tools/installer/') and CJK.search(text):
                if text not in INSTALLER:raise ValueError('New installer wording needs an explicit context: '+text)
                en,context=INSTALLER[text];r.update(canonical_en=en,mt_context_en=context,role='message',status='source_reviewed')
                r['delivery_surface']='installer';r['runtime_localization']='missing_installer_language_route'
                r['catalog_context']='installer'
                if text in {'开始','取消','卸载'}:r['role']='action'
                if text=='语言':r['role']='field'
            elif f.endswith('close_controls_workflow.cpp'):
                r['mt_context_en']='Synthetic observation or test fixture used only by the isolated window-layout verification workflow; not normal application copy.'
            elif f.endswith('hydro_workflow.cpp'):
                r['mt_context_en']='Diagnostic result text or observer metadata used only by the isolated hydrostatics verification workflow.'
            elif f.endswith('studio_ui.cpp'):
                r['mt_context_en']='Stylesheet declaration controlling tab appearance; preserve as code, not natural-language UI text.'
            elif f.endswith('studio_motion.cpp'):
                r['mt_context_en']='Internal property identifier/value controlling tab motion and style ownership.'
            elif f.endswith('hydro_dialog.cpp') and text=='已跳过高于船体最低漏点的水线。':
                r['mt_context_en']='Exact source-message comparison used to suppress a duplicate auxiliary warning. The displayed warning is built separately from elevation and unit placeholders.'
            elif r['status'] in APPROVED:
                pass
            elif f in decision['reviewed_files'] or f.startswith('tools/installer/'):
                r['mt_context_en']='Internal object, schema, file-format, fixed-unit or control-state token. Preserve its exact spelling; it is not prose to translate.'
            else:
                r.update(status='review',gaps_en=['New lexical site has no recorded functional review.'])
        # Values emitted indirectly through metadata remain user-visible text.
        if f=='src/app/main_window.cpp' and text in {'当前船模','退出程序','程序','全部四个视图','界面语言','型线图窗口'} and original and original['status']=='technical':
            contexts={
                '当前船模':'Scope column of the shortcut-reference table for file commands operating on the current ship model.',
                '退出程序':'Shortcut description for closing the main application through the Quit shortcut.',
                '程序':'Scope column identifying a shortcut that applies to the entire application.',
                '全部四个视图':'Scope of the F shortcut: fits the model within all four hull viewports.',
                '界面语言':'Scope description attached to the interface-language action; the action is currently disabled.',
                '型线图窗口':'Scope of the shortcut that fits the ship lines in the separate lines-plan window.',
            }
            r.update(canonical_en=pack[text],mt_context_en=contexts[text],role='description',status='source_reviewed')
            r['dependency_files']['src/app/shortcuts_dialog.h']=cur['files']['src/app/shortcuts_dialog.h']
            corrections.append(dict(id=r['id'],source_text=text,previous_status='technical',new_status=r['status'],reason='Metadata is rendered in the shortcut table.'))
        if f=='src/app/ui_language.cpp' and text in {'打印预览','下一页','上一页','首页','末页'} and original and original['status']=='technical':
            r.update(canonical_en=pack[text],mt_context_en={
                '打印预览':'Title of the standard Qt print-preview dialog.',
                '下一页':'Print-preview toolbar command that advances to the next page.',
                '上一页':'Print-preview toolbar command that returns to the previous page.',
                '首页':'Print-preview toolbar command that displays the first page of the document.',
                '末页':'Print-preview toolbar command that displays the last page of the document.',
            }[text],role='action' if text!='打印预览' else 'heading',status='source_reviewed')
            corrections.append(dict(id=r['id'],source_text=text,previous_status='technical',new_status=r['status'],reason='A Qt translator mapping supplies visible print-preview copy.'))
        if f=='src/document/native_fung.inc' and text.startswith('Rf 采用 ITTC-1957'):
            r.update(canonical_en='Rf uses ITTC-1957; Rr=0.5ρSV²Cr; Rt=Rf+Rr+0.5ρSV²ΔCf; Pe=Rt×V.',mt_context_en='Formula-definition note in the Fung–Leibman resistance results. Rf is frictional resistance, Rr residuary resistance, Rt total resistance and Pe effective power. Preserve every formula and symbol.',role='explanation',status='source_reviewed',runtime_localization='native_report_literal_not_routed')
            r['dependency_files']['src/document/resistance.cpp']=cur['files']['src/document/resistance.cpp'];r['dependency_files']['src/app/performance_dialog.cpp']=cur['files']['src/app/performance_dialog.cpp']
            if any(p['status']=='technical' for p in bytext[f,text]):
                corrections.append(dict(id=r['id'],source_text=text,previous_status='technical',new_status='source_reviewed',reason='Owned resultsmemo prose reaches the visible results report.'))
        if f=='tools/installer/InstallerLanguage.cs' and text in {'Missing or invalid embedded installer languages','Missing required installer language'}:
            r.update(canonical_en=text,mt_context_en='Installer startup error when embedded language data cannot be read or lacks a required Chinese/English catalog. English is used as the bootstrap fallback because the language loader itself is unavailable.',role='message',status='source_reviewed')
        if r['id'] in resolved:
            r.update(status='reference_only',runtime_reachability='inactive_retained_legacy_resource',mt_context_en='Retained historical caption-table resource with no current Qt consumer. It is not part of the present application interface.',reachability_resolution=decision['reachability_resolution'])
        apply_site_review(r,site,decision,cur['files'],original is None)
        if decision.get('runtime_routes_repaired'):
            if r.get('runtime_localization') and (f.startswith('tools/installer/') or f.startswith('src/document/native_')):
                r['previous_runtime_localization']=r.pop('runtime_localization');r['runtime_route_status']='owned_literal_route_repaired_pending_individual_runtime_coverage'
        if f=='src/app/hydro_dialog.cpp' and text=='计算' and r['status'] in APPROVED:
            r['mt_context_en']='Button that starts the selected hydrostatics calculation. In the single-condition mode it also generates the Bonjean, sectional-area, layer-weight and waterline-area auxiliary tables in the same operation.'
        key=(r.get('catalog_context','')+'\x04' if r.get('catalog_context') else '')+text
        if r['status'] in APPROVED and r.get('canonical_en') and key in pack:
            if pack[key]!=r['canonical_en']:
                if pack[key].casefold()==r['canonical_en'].casefold():
                    r['previous_canonical_en']=r['canonical_en'];r['canonical_en']=pack[key];r['capitalization_aligned_to_catalog']=True
                else:r.update(status='review',gaps_en=['English catalog and source-context decision disagree beyond capitalization.'])
        r.update(source=dict(file=f,line=site['line'],sha256=site['file_sha256'],locator=site['locator']),source_text_sha256=site['text_sha256'],change=change)
        r['previous_revision']=str(previous);r['evidence_boundary']='Current source and recorded delta review. No new model review or complete runtime acceptance.'
        if decision.get('source_context_author') and r.get('explicit_site_review'):
            r['current_context_author']=decision['source_context_author']
            r['current_context_review']=r['explicit_site_review'].get('full_context_review') or dict(reviewer=decision['source_context_author'],reason_en=r['explicit_site_review']['reason_en'])
            r['evidence_boundary']='Exact source/context reviewed by the current chat model. Historical provider fields retain their provenance; this is not independent expert or complete runtime acceptance.'
        # Current evidence is attached even for inherited records; old receipts stay intact.
        lines=sources[f].splitlines();line=site['line'] or 1;excerpt='\n'.join(lines[max(0,line-5):line+5]) if site['line'] else site['locator']+' = '+text
        eid=digest((f+'\n'+excerpt).encode());evidence[eid]=dict(file=f,file_sha256=site['file_sha256'],line=site['line'],excerpt=excerpt)
        r['previous_evidence_refs']=r.get('evidence_refs',[]);r['evidence_refs']=[eid]
        if r['status'] in APPROVED:
            if not r.get('canonical_en') or not r.get('mt_context_en') or CJK.search(r['canonical_en']) or CJK.search(r['mt_context_en']):
                r.update(status='review',gaps_en=['Missing or non-English source/context in an otherwise approved record.'])
        if r['status']=='review':gaps.append(dict(kind='meaning_or_evidence',id=r['id'],source=r['source'],source_text=text,reasons=r['gaps_en']))
        if CJK.search(text) and text not in pack and f not in {'src/app/about_dialog.h','tools/installer/Setup.cs'} and not f.endswith('_workflow.cpp'):
            kind='english_catalog_missing' if r['status'] in APPROVED else 'held_meaning' if r['status']=='review' else 'reference_reachability_unconfirmed'
            proven_non_display=r.get('runtime_reachability') in {'proven_unit_normalization_alias','retained_backend_help_not_rendered','proven_legacy_control_caption_reference','proven_invariant_symbol_metadata'}
            if kind=='reference_reachability_unconfirmed' and r['id'] not in resolved and not proven_non_display:gaps.append(dict(kind=kind,id=r['id'],source=r['source'],source_text=text,reasons=['No English catalog entry; prior technical/reference classification does not establish a current visible consumer. Trace reachability before claiming UI completeness.']))
        if r.get('runtime_localization'):
            runtime.append(dict(id=r['id'],source=r['source'],source_text=text,canonical_en=r.get('canonical_en'),issue=r['runtime_localization']))
        records.append(r);audit.append(dict(candidate_id=cid,file=f,line=site['line'],source_text=text,classification=r['status'],change=change,semantic_acceptance=r['status'] in APPROVED))
        if r['status'] in APPROVED:ready.append(r)
    # Discover native report/formatting routes independently of old classification.
    report_sites=[]
    for site in cur['candidates']:
        if site['file'] in {'src/document/native_fung.inc','src/document/native_hollenbach.inc','src/document/native_oortmerssen.inc'} and site['has_cjk']:
            ex=site['excerpt']
            if ('resultsmemo' in ex or 'warning(' in ex) and 'uiText(' not in ex:
                report_sites.append(dict(candidate_id=site['candidate_id'],file=site['file'],line=site['line'],source_text=site['text'],issue='owned_native_report_or_warning_requires_runtime_route_check',formatted=bool(re.search(r'QString\s*\(',ex))))
    for f,h in cur['files'].items():
        if digest((QT/f).read_bytes())!=h:raise ValueError('Source changed during review: '+f)
    if packfile.read_bytes()!=packdata:raise ValueError('English catalog changed during review')
    jsonl(out/'contexts.jsonl',records);jsonl(out/'site_audit.jsonl',audit);jsonl(out/'translation_sources.jsonl',ready)
    jsonl(out/'review_queue.jsonl',[r for r in records if r['status']=='review'])
    jsonl(out/'evidence_insufficient.jsonl',gaps);write(out/'classification_corrections.json',corrections);write(out/'runtime_localization_gaps.json',runtime);write(out/'native_report_route_candidates.json',report_sites);write(out/'evidence.json',evidence)
    units=[]
    for r in ready:
        units.append(dict(id=r['id'],source_language='en',text=r['canonical_en'],context=r['mt_context_en'],role=r['role'],source_key=r['source_text'],catalog_context=r.get('catalog_context'),dependency_files=r['dependency_files'],evidence_refs=r['evidence_refs'],delivery_surface=r.get('delivery_surface','application')))
    jsonl(out/'translation_units.jsonl',units)
    jsonl(out/'translation_units.deduplicated.jsonl',deduplicate_units(ready))
    generated=[json.loads(x) for x in (previous/'generated_translation_units.jsonl').read_text('utf8').splitlines()]
    for r in generated:
        changed=[f for f,h in r['dependency_files'].items() if cur['files'].get(f)!=h]
        if any(f not in decision['reviewed_files'] for f in changed):raise ValueError('Generated caption evidence changed without source review')
        if changed:
            r['previous_dependency_files']=r['dependency_files'].copy();r['dependency_files']={f:cur['files'][f] for f in r['dependency_files']};r['delta_review_files']=changed
        explicit=decision.get('generated_site_reviews',{}).get(r['id'])
        if explicit:
            if explicit['source_key']!=r['source_key'] or any(cur['files'].get(f)!=h for f,h in explicit['dependency_files'].items()):raise ValueError('Stale generated-caption source review')
            if not explicit.get('context_en') or CJK.search(explicit['context_en']):raise ValueError('Generated caption requires English functional context')
            r['previous_context']=r['context'];r['context']=explicit['context_en'];r['explicit_source_review']=explicit
    jsonl(out/'generated_translation_units.jsonl',generated)
    summary=dict(source_files=len(cur['files']),lexical_candidates=len(cur['candidates']),selected_context_sites=len(records),status_counts=dict(Counter(r['status'] for r in records)),change_counts=dict(Counter(r['change'] for r in records)),delta_counts={k:len(v) for k,v in delta.items() if isinstance(v,list)},translation_ready_sites=len(ready),translation_units=len(units),supplemental_generated_units=len(generated),classification_corrections=len(corrections),capitalization_alignments=sum(r.get('capitalization_aligned_to_catalog',False) for r in records),evidence_insufficient_records=len(gaps),evidence_insufficient_by_kind=dict(Counter(r['kind'] for r in gaps)),runtime_localization_gap_records=len(runtime),native_report_route_candidate_sites=len(report_sites),english_entries=len(pack),translation_api_calls=0,overall_status='review',boundary='All lexical candidates accounted for, not all visible UI proven. Inherited semantics, uncertain reachability, installer localization, native reports and framework text remain distinct.')
    write(out/'summary.json',summary);write(out/'manifest.json',dict(schema_version=2,audit_kind='latest_ui_source_context_audit',previous_revision=str(previous),source_root=str(QT),source_files=cur['files'],english_sha256=digest(packdata),decisions_sha256=digest(args.decisions.read_bytes()),authoring_tool_sha256=digest(Path(__file__).read_bytes()),scanner_sha256=digest((TOOL_ROOT.parent/'FreeShip-Tests/qt_text_audit.py').read_bytes()),scope='First-party application C++/headers/inc/UI/JSON plus updater and installer C# sources; language resources captured separately; external framework/OS messages not exhaustively enumerated.',new_model_calls=0))
    print(json.dumps(summary,ensure_ascii=False))

if __name__=='__main__':main()
