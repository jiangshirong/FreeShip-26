"""Audit/regenerate the native single-byte FBM tables from installed LazUtils.

Run with --write to regenerate; without it this is a read-only provenance check.
The original table and license stay in the Lazarus toolchain, never modified.
"""
import argparse, hashlib, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'FreeShip-MCP/.toolchain/lazarus/components/lazutils/commoncodepages.inc'
TARGET = ROOT / 'FreeShip-Qt/third_party/lazarus/fbm_codepages.h'

def generate():
    text = SOURCE.read_text('utf8')
    tables = []
    for name, body in re.findall(r'Array(\w+)ToUTF8\s*:\s*TCharToUTF8Table\s*=\s*\((.*?)\);', text, re.S):
        values = []
        for line in body.splitlines():
            line = line.split('//', 1)[0].strip().removesuffix(',')
            if not line: continue
            tokens = re.findall(r"'(?:[^']|'')*'|#\$[0-9a-fA-F]+|#\d+", line)
            if ''.join(tokens) != line.replace(' ', '') and not (len(tokens) == 1 and tokens[0] == line):
                raise ValueError('Unparsed Pascal table entry: ' + line)
            raw = b''.join(t[1:-1].replace("''", "'").encode('latin1') if t[0] == "'" else bytes([int(t[2:], 16) if t[1] == '$' else int(t[1:])]) for t in tokens)
            decoded = raw.decode('utf8')
            if len(decoded) != 1: raise ValueError('Expected one Unicode scalar')
            values.append(ord(decoded))
        if len(values) != 256: raise ValueError((name, len(values)))
        tables.append((name.lower().replace('_', ''), values))
    if len(tables) != 31: raise ValueError(('Unexpected table count', len(tables)))
    lines = ['// Generated from Lazarus LazUtils/commoncodepages.inc; modified LGPL,',
             '// see COPYING.modifiedLGPL.txt. Do not hand-edit; regenerate with',
             '// FreeShip-Tests/maintenance/generate_fbm_codepages.py --write.',
             '// Source SHA256: ' + hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
             '#pragma once', '#include <array>', '#include <cstdint>',
             'namespace fs::document::codepages {',
             'struct Table { const char *name; std::array<char32_t,256> unicode; };',
             'inline constexpr Table tables[] = {']
    for name, values in tables:
        lines.append('    {"' + name + '", {')
        for i in range(0, 256, 16): lines.append('        ' + ','.join(f'0x{n:04x}' for n in values[i:i+16]) + ',')
        lines.append('    }},')
    lines += ['};', '}']
    return '\n'.join(lines) + '\n'

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true'); args = parser.parse_args()
    expected = generate()
    if args.write:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(expected, encoding='utf8', newline='\n')
    elif TARGET.read_text('utf8') != expected: raise SystemExit('Native codepage table differs from its recorded LazUtils source')
    print('31 codepages / 7936 byte mappings verified')
