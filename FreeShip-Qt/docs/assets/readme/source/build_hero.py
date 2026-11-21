"""Build the README hero SVG from a real FreeShip 26 lines-plan DXF export.

Usage: python build_hero.py <reference.dxf|body-plan-stations.json> <out.svg> [--lang zh-CN|en|ru]

The body-plan stations come from the DXF that FreeShipNativeLinesplan.exe
exported for the public sample "FREE!ship demo 3+deck.fbm" (FREE!ship Plus
sample set, GPL). Nothing in the drawing is hand-drawn or invented.
Requires ezdxf.
"""
import argparse
import hashlib
from html import escape
import json
from pathlib import Path

import ezdxf

TEXT = {
    'zh-CN': {
        'kicker': '开源船体设计与分析软件',
        'line1': '从船体曲面到型线图、静水力、阻力与推进',
        'line2': 'FREE!ship Plus 的 C++ / Qt 新版',
        'meta': 'C++ · Qt 6 · Windows / Linux · GPLv3+',
        'caption': '横剖线图 · 样例船 FREE!ship demo 3 · 导出自本软件',
        'title': 'FreeShip 26：开源船体设计与分析软件',
        'desc': '左侧为项目名称与简介，右侧为 FreeShip 26 对公开样例船导出的真实横剖线图。',
        'aft': '尾', 'fore': '首',
    },
    'en': {
        'kicker': 'OPEN-SOURCE HULL DESIGN & ANALYSIS',
        'line1': 'From hull surface to lines plan, hydrostatics,',
        'line1b': 'resistance and propulsion',
        'line2': 'FREE!ship Plus, rebuilt in C++ and Qt',
        'meta': 'C++ · Qt 6 · Windows / Linux · GPLv3+',
        'caption': 'Body plan · FREE!ship demo 3 sample',
        'title': 'FreeShip 26: open-source hull design and analysis',
        'desc': 'Project name and summary on the left; a real body plan exported by FreeShip 26 for a public sample hull on the right.',
        'aft': 'AFT', 'fore': 'FWD',
    },
    'ru': {
        'kicker': 'ПРОЕКТИРОВАНИЕ СУДОВ · ОТКРЫТЫЙ КОД',
        'line1': 'От обводов корпуса к теоретическому',
        'line1b': 'чертежу, гидростатике и расчёту ходкости',
        'line2': 'Новая версия FREE!ship Plus на C++ и Qt',
        'meta': 'C++ · Qt 6 · Windows / Linux · GPLv3+',
        'caption': 'Проекция «Корпус» · FREE!ship demo 3',
        'title': 'FreeShip 26 — открытая программа для проектирования корпусов судов',
        'desc': 'Слева — название и описание проекта, справа — проекция «Корпус», экспортированная из FreeShip 26 для общедоступной модели.',
        'aft': 'КОРМА', 'fore': 'НОС',
    },
}

INK, MUTED, ACCENT, RULE, BG, PAPER = '#183b36', '#50645f', '#176c5c', '#cfdcd7', '#f3f6f5', '#ffffff'
SANS = "'Microsoft YaHei UI','Microsoft YaHei','PingFang SC','Noto Sans CJK SC','Segoe UI',Arial,sans-serif"
SERIF = "Georgia,'Times New Roman',serif"
MONO = "Consolas,'SFMono-Regular',Menlo,monospace"


def stations(path, centre, half):
    doc = ezdxf.readfile(path)
    out = []
    for e in doc.modelspace().query('POLYLINE[layer=="stations"]'):
        pts = [(v.dxf.location.x, v.dxf.location.y) for v in e.vertices]
        if all(abs(x - centre) <= half for x, _ in pts):
            out.append(pts)
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument('dxf', type=Path)
    p.add_argument('out', type=Path)
    p.add_argument('--lang', default='zh-CN', choices=TEXT)
    a = p.parse_args()
    t = {key: escape(value) for key,value in TEXT[a.lang].items()}

    # Body plan of the sample: centre line x=30.650385, base y=-18, DWL y=-10.5, deck y=-3.
    cx, base, dwl, deck, half = 30.650385, -18.0, -10.5, -3.0, 10.0
    if a.dxf.suffix.lower() == '.json':
        data = json.loads(a.dxf.read_text('utf8'))
        cx, base, dwl, deck = (data[key] for key in ('centre', 'base', 'dwl', 'deck'))
        curves = data['stations']
    else:
        curves = stations(str(a.dxf), cx, half)
    if len(curves) < 10:
        raise SystemExit('Body-plan stations not found in DXF')

    W, H = 1200, 400
    k = 17.0                      # SVG units per metre
    ox, oy = 930.0, 318.0         # centre line / base line in SVG
    X = lambda x: ox + (x - cx) * k
    Y = lambda y: oy - (y - base) * k

    s = []
    s.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
             f'role="img" aria-labelledby="title desc">')
    s.append(f'<title id="title">{t["title"]}</title>')
    s.append(f'<desc id="desc">{t["desc"]}</desc>')
    s.append(f'<rect width="{W}" height="{H}" rx="24" fill="{BG}"/>')
    # Drawing sheet: paper panel for the body plan.
    s.append(f'<rect x="700" y="28" width="460" height="344" rx="14" fill="{PAPER}" stroke="{RULE}"/>')

    # Title block.
    s.append('<g id="title-block">')
    s.append(f'<text x="64" y="104" font-family="{SANS}" font-size="20" font-weight="700" '
             f'letter-spacing="1.5" fill="{ACCENT}">{t["kicker"]}</text>')
    s.append(f'<text x="60" y="190" font-family="{SERIF}" font-size="88" fill="{INK}">FreeShip 26</text>')
    s.append(f'<text x="64" y="244" font-family="{SANS}" font-size="27" fill="{INK}">{t["line1"]}</text>')
    y = 244
    if 'line1b' in t:
        y += 36
        s.append(f'<text x="64" y="{y}" font-family="{SANS}" font-size="27" fill="{INK}">{t["line1b"]}</text>')
    s.append(f'<text x="64" y="{y + 40}" font-family="{SANS}" font-size="22" fill="{MUTED}">{t["line2"]}</text>')
    s.append(f'<line x1="64" y1="{y + 76}" x2="300" y2="{y + 76}" stroke="{ACCENT}" stroke-width="3"/>')
    s.append(f'<text x="64" y="{y + 108}" font-family="{MONO}" font-size="19" fill="{MUTED}">{t["meta"]}</text>')
    s.append('</g>')

    # Body plan with waterline grid, DWL, base line and centre line.
    s.append('<g id="project-proof" fill="none">')
    for wl in range(0, 16, 2):
        yy = Y(base + wl)
        s.append(f'<line x1="{X(cx - 9.6):.1f}" y1="{yy:.1f}" x2="{X(cx + 9.6):.1f}" y2="{yy:.1f}" stroke="{RULE}" stroke-width="1"/>')
    for bx in (-8, -6, -4, -2, 2, 4, 6, 8):
        xx = X(cx + bx)
        s.append(f'<line x1="{xx:.1f}" y1="{Y(base):.1f}" x2="{xx:.1f}" y2="{Y(deck):.1f}" stroke="{RULE}" stroke-width="1"/>')
    s.append(f'<line x1="{X(cx):.1f}" y1="{Y(base) + 12:.1f}" x2="{X(cx):.1f}" y2="{Y(deck) - 14:.1f}" '
             f'stroke="{MUTED}" stroke-width="1.2" stroke-dasharray="14 4 3 4"/>')
    s.append(f'<line x1="{X(cx - 9.9):.1f}" y1="{Y(dwl):.1f}" x2="{X(cx + 9.9):.1f}" y2="{Y(dwl):.1f}" '
             f'stroke="{ACCENT}" stroke-width="1.6" stroke-dasharray="8 5"/>')
    s.append(f'<line x1="{X(cx - 9.9):.1f}" y1="{Y(base):.1f}" x2="{X(cx + 9.9):.1f}" y2="{Y(base):.1f}" '
             f'stroke="{INK}" stroke-width="1.6"/>')
    for pts in curves:
        d = 'M' + ' L'.join(f'{X(x):.1f} {Y(y):.1f}' for x, y in pts)
        s.append(f'<path d="{d}" stroke="{INK}" stroke-width="1.3" stroke-linejoin="round" opacity="0.88"/>')
    s.append('</g>')
    s.append(f'<text x="{X(cx + 9.9) + 6:.1f}" y="{Y(dwl) + 6:.1f}" font-family="{MONO}" font-size="18" fill="{ACCENT}">DWL</text>')
    s.append(f'<text x="{X(cx - 7):.1f}" y="{Y(deck) - 10:.1f}" text-anchor="middle" font-family="{SANS}" font-size="18" fill="{MUTED}">{t["aft"]}</text>')
    s.append(f'<text x="{X(cx + 7):.1f}" y="{Y(deck) - 10:.1f}" text-anchor="middle" font-family="{SANS}" font-size="18" fill="{MUTED}">{t["fore"]}</text>')
    s.append(f'<text x="930" y="356" text-anchor="middle" font-family="{SANS}" font-size="18" fill="{MUTED}">{t["caption"]}</text>')
    s.append('</svg>')
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text('\n'.join(s) + '\n', encoding='utf-8')
    print(len(curves), 'stations; source sha256', hashlib.sha256(a.dxf.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
