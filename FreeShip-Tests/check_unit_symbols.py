"""User-approved unit policy, separate from layout/translation-quality checks.

Checks real Lazarus PO lookups using the existing loader probe when supplied.
Does not launch or modify the user's app.
"""
from pathlib import Path
import argparse, json, subprocess

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--loader', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    rows = []
    for language in ('en', 'cn', 'ru', 'fr', 'de', 'es', 'ja'):
        for source in ('degr', '°'):
            rows.append(dict(language=language, identifier='freestringsunit.rs_degr', source=source, expected='°'))
        if language == 'cn':
            for identifier, source, expected in (
                ('freestringsunit.rs_m', 'm', 'm'),
                ('freestringsunit.rs_tonnes', 'tonnes', 't'),
                ('freestringsunit.rs_meters', 'Meters', 'm'),
                ('tdxfexport2ddialog.combobox1.items[0]', 'Meters', 'm'),
                ('tdxfexport2ddialog.combobox1.text', 'Meters', 'm'),
                ('tfreenewmodeldialog.combobox1.items[0]', 'Meters', 'm'),
                ('tfreenewmodeldialog.combobox1.text', 'Meters', 'm')):
                rows.append(dict(language=language, identifier=identifier, source=source, expected=expected))
    args.output.mkdir(parents=True, exist_ok=True)
    checks = args.output / 'checks.json'
    checks.write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
    result = subprocess.run([str(args.loader.resolve()), str(root/'FreeShip-Plus-Lazarus/locale'), str(checks.resolve())], capture_output=True)
    report = dict(status='pass' if result.returncode == 0 else 'fail', checks=len(rows),
                  scope='Lazarus TPOFile lookup; old and new degree source strings; not live report rendering',
                  returncode=result.returncode, output=result.stdout.decode('utf-8', errors='replace'),
                  stderr=result.stderr.decode('utf-8', errors='replace'))
    (args.output/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
