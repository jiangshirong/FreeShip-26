"""Build README section headers, feature board, stat tiles and framed screenshots.

Usage:
  python build_readme_assets.py --lang zh-CN
  python build_readme_assets.py --frames      # re-frame raw screenshots (language independent)

Inputs (all real project material):
  source/body-plan-stations.json   body-plan stations extracted from the lines-plan DXF
  source/icons/*.svg               StudioLight toolbar icons shipped with FreeShip 26 (ISC, Lucide-derived)
  raw/*.png                        screenshots captured by FreeShip-Tests qt-readme-media
Outputs: <lang>/section-*.svg, <lang>/features.svg, <lang>/stats.svg, framed *.png
"""
import argparse
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ASSETS = HERE.parent

INK, MUTED, ACCENT, RULE, BG, PAPER, TILE = '#183b36', '#50645f', '#176c5c', '#cfdcd7', '#f3f6f5', '#ffffff', '#e3eeea'
SANS = "'Microsoft YaHei UI','Microsoft YaHei','PingFang SC','Noto Sans CJK SC','Segoe UI',Arial,sans-serif"
SERIF = "Georgia,'Times New Roman',serif"

TEXT = {
    'zh-CN': {
        'sections': {
            'features': ('01', '功能', '从建模到出图，一套几何贯穿始终'),
            'improvements': ('02', '相比原版', '更快的交互，更安全的文件，更准确的结果'),
            'start': ('03', '快速开始', '安装，打开一艘船，开始编辑'),
            'develop': ('04', '开发与参与', '从源码构建，提交问题与改进'),
            'credits': ('05', '致谢与许可', '二十年来，让这款软件延续下来的人'),
        },
        'features': [
            ('ControlNet', '船体建模', ['细分曲面控制网', '点、边、面编辑与约束', 'Lackenby 船型变换']),
            ('Sections', '形状检查', ['横剖、纵剖、水线、斜剖', '曲率梳与法向显示', '模型检查与朝向修复']),
            ('LinesPlan', '型线图', ['由模型几何直接生成', '标注文字自动避让', '导出 PNG 和 DXF']),
            ('Hydrostatics', '静水力与稳性', ['静水力与吃水表', 'KN、GZ 稳性曲线', 'Bonjean 与横剖面面积']),
            ('Calculate', '阻力与推进', ['Holtrop、Delft 等方法', 'B 系列敞水计算', '螺旋桨与主机匹配']),
            ('Face', '板材展开', ['展开可展图层', '边长与面积误差', '导出 DXF 和 PDF']),
            ('Open', '文件交换', ['FBM 读写，FTM 读取', 'IGES、DXF、STL、OBJ', 'GHS、PAM 等船舶格式']),
            ('Languages', '七种界面语言', ['中文、English、Русский', 'Deutsch、Français', 'Español、日本語']),
        ],
        'features_title': 'FreeShip 26 的八项主要功能',
        'stats': [
            ('型线图平移缩放', '4.7×', '44.6 ms → 9.4 ms'),
            ('密集板片拖动', '1776×', '803 ms → 0.45 ms'),
            ('确认工程设置', '23×', '1238 ms → 53 ms'),
        ],
        'stats_title': '三个交互场景的提速',
        'stats_note': '每帧或单次响应耗时的中位数',
    },
    'en': {
        'sections': {
            'features': ('01', 'Features', 'One hull model, from the first edit to the final drawing'),
            'improvements': ('02', 'What’s improved', 'Faster interaction, safer files and more accurate results'),
            'start': ('03', 'Quick start', 'Install FreeShip, open a hull and start editing'),
            'develop': ('04', 'Development and contributions', 'Build from source, report issues and contribute improvements'),
            'credits': ('05', 'Credits and licensing', 'The people behind two decades of development'),
        },
        'features': [
            ('ControlNet', 'Hull modeling', ['Subdivision control net', 'Edit and constrain geometry', 'Lackenby hull transformation']),
            ('Sections', 'Shape inspection', ['Stations, buttocks, waterlines', 'Diagonals, curvature, normals', 'Model and face checks']),
            ('LinesPlan', 'Lines plan', ['From hull geometry', 'Automatic label placement', 'PNG and DXF export']),
            ('Hydrostatics', 'Hydrostatics & stability', ['Hydrostatics and draft tables', 'KN and GZ curves', 'Bonjean and sectional areas']),
            ('Calculate', 'Resistance & propulsion', ['Holtrop, Delft and more', 'B-series open-water curves', 'Engine–propeller matching']),
            ('Face', 'Plate development', ['Unfold developable layers', 'Edge length and area errors', 'DXF and PDF export']),
            ('Open', 'File exchange', ['Read/write FBM, read FTM', 'IGES, DXF, STL and OBJ', 'GHS, PAM and more']),
            ('Languages', 'Seven UI languages', ['中文, English, Русский', 'Deutsch, Français', 'Español, 日本語']),
        ],
        'features_title': 'Eight FreeShip 26 features',
        'stats': [
            ('Lines-plan pan and zoom', '4.7×', '44.6 ms → 9.4 ms'),
            ('Dragging dense plate meshes', '1776×', '803 ms → 0.45 ms'),
            ('Confirming project settings', '23×', '1238 ms → 53 ms'),
        ],
        'stats_title': 'Measured speedups in three workflows',
    },
    'ru': {
        'sections': {
            'features': ('01', 'Возможности', 'Одна модель корпуса — от построения до выпуска чертежа'),
            'improvements': ('02', 'Что улучшилось', 'Быстрее отклик, надёжнее сохранение, точнее расчёты'),
            'start': ('03', 'Быстрый старт', 'Установите программу, откройте модель и начните работу'),
            'develop': ('04', 'Разработка и участие', 'Сборка из исходников, сообщения об ошибках и улучшения'),
            'credits': ('05', 'Авторы и лицензия', 'Люди, которые развивали программу два десятилетия'),
        },
        'features': [
            ('ControlNet', 'Модель корпуса', ['Сетка подразделения', 'Точки, рёбра, грани, связи', 'Преобразование Лакенби']),
            ('Sections', 'Проверка обводов', ['Шпангоуты, батоксы, ВЛ', 'Рыбины, кривизна, нормали', 'Проверка модели и граней']),
            ('LinesPlan', 'Теоретический чертёж', ['По геометрии корпуса', 'Размещение подписей', 'Экспорт PNG и DXF']),
            ('Hydrostatics', 'Гидростатика', ['Таблицы по осадкам', 'Кривые KN и GZ', 'Бонжан и площади сечений']),
            ('Calculate', 'Ходкость', ['Методы Холтропа, Делфта', 'Серия B в свободной воде', 'Подбор двигателя и винта']),
            ('Face', 'Развёртка листов', ['Развёртываемые слои', 'Погрешности развёртки', 'Экспорт DXF и PDF']),
            ('Open', 'Обмен файлами', ['Чт./зап. FBM, чтение FTM', 'IGES, DXF, STL и OBJ', 'GHS, PAM и др.']),
            ('Languages', 'Семь языков', ['中文, English, Русский', 'Deutsch, Français', 'Español, 日本語']),
        ],
        'features_title': 'Восемь возможностей FreeShip 26',
        'stats': [
            ('Панорамирование и масштаб', '4,7×', '44,6 мс → 9,4 мс'),
            ('Перетаскивание листов', '1776×', '803 мс → 0,45 мс'),
            ('Подтверждение настроек', '23×', '1238 мс → 53 мс'),
        ],
        'stats_title': 'Измеренное ускорение трёх операций',
    },
}

# Lucide "languages" icon (ISC), the same family as the StudioLight toolbar set.
LANG_ICON = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#315B55" '
             'stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"><path d="m5 8 6 6"/>'
             '<path d="m4 14 6-6 2-3"/><path d="M2 5h12"/><path d="M7 2h1"/><path d="m22 22-5-10-5 10"/>'
             '<path d="M14 18h6"/></svg>')


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def icon(name, x, y, size):
    raw = LANG_ICON if name == 'Languages' else (HERE / 'icons' / f'{name}.svg').read_text('utf8')
    vb = re.search(r'viewBox="([^"]+)"', raw).group(1)
    inner = re.sub(r'^.*?<svg[^>]*>|</svg>\s*$', '', raw, flags=re.S)
    attrs = re.search(r'<svg([^>]*)>', raw).group(1)
    keep = ' '.join(re.findall(r'(?:fill|stroke|stroke-width|stroke-linecap|stroke-linejoin|color)="[^"]*"', attrs))
    keep = keep.replace('#315B55', INK).replace('#315b54', INK).replace('#315B54', INK)
    inner = inner.replace('#315B55', INK).replace('#315b54', INK)
    return f'<svg x="{x}" y="{y}" width="{size}" height="{size}" viewBox="{vb}" {keep}>{inner}</svg>'


def svg_open(w, h, title, desc):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" '
            f'role="img" aria-labelledby="title desc">',
            f'<title id="title">{esc(title)}</title>', f'<desc id="desc">{esc(desc)}</desc>']


def station_motif(x0, y0, height, flip=False):
    """A few real body-plan stations, drawn small: the recurring project motif."""
    data = json.loads((HERE / 'body-plan-stations.json').read_text('utf8'))
    cx, base, top = data['centre'], data['base'], data['deck']
    k = height / (top - base)
    out = []
    for pts in data['stations']:
        d = 'M' + ' L'.join(f'{x0 + (x - cx) * k:.1f} {y0 + height - (y - base) * k:.1f}' for x, y in pts)
        out.append(f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="0.9" opacity="0.55"/>')
    out.append(f'<line x1="{x0 - 9.6 * k:.1f}" y1="{y0 + height:.1f}" x2="{x0 + 9.6 * k:.1f}" y2="{y0 + height:.1f}" stroke="{INK}" stroke-width="1.4" opacity="0.6"/>')
    out.append(f'<line x1="{x0:.1f}" y1="{y0 - 6:.1f}" x2="{x0:.1f}" y2="{y0 + height + 6:.1f}" stroke="{MUTED}" stroke-width="1" stroke-dasharray="8 3 2 3" opacity="0.7"/>')
    return out


def section(lang, key):
    num, title, sub = TEXT[lang]['sections'][key]
    W, H = 1200, 150
    separator = '：' if lang == 'zh-CN' else ': '
    s = svg_open(W, H, f'{num} {title}', f'{title}{separator}{sub}')
    s.append(f'<rect width="{W}" height="{H}" rx="20" fill="{BG}"/>')
    s.append(f'<text x="56" y="92" font-family="{SANS}" font-size="44" font-weight="700" fill="{ACCENT}">{num}</text>')
    s.append(f'<line x1="140" y1="44" x2="140" y2="106" stroke="{RULE}" stroke-width="2"/>')
    s.append(f'<text x="168" y="80" font-family="{SANS}" font-size="40" font-weight="700" fill="{INK}">{esc(title)}</text>')
    s.append(f'<text x="170" y="114" font-family="{SANS}" font-size="21" fill="{MUTED}">{esc(sub)}</text>')
    s += station_motif(1070, 26, 98)
    s.append('</svg>')
    return '\n'.join(s) + '\n'


def features(lang):
    t = TEXT[lang]
    cols, rows, cw, ch, gap, m = 4, 2, 264, 250, 20, 36
    W = m * 2 + cols * cw + (cols - 1) * gap
    H = m * 2 + rows * ch + (rows - 1) * gap
    alt = ('；' if lang == 'zh-CN' else '; ').join(
        f'{name}{("：" if lang == "zh-CN" else ": ")}{("、" if lang == "zh-CN" else ", ").join(lines)}'
        for _, name, lines in t['features']
    )
    title_size, line_size = {'zh-CN': (26, 20), 'en': (18, 16), 'ru': (18, 15)}[lang]
    s = svg_open(W, H, t['features_title'], alt)
    s.append(f'<rect width="{W}" height="{H}" rx="24" fill="{BG}"/>')
    for i, (ic, name, lines) in enumerate(t['features']):
        x = m + (i % cols) * (cw + gap)
        y = m + (i // cols) * (ch + gap)
        s.append(f'<g id="feature-{i + 1}">')
        s.append(f'<rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="16" fill="{PAPER}" stroke="{RULE}"/>')
        s.append(f'<rect x="{x + 24}" y="{y + 24}" width="56" height="56" rx="14" fill="{TILE}"/>')
        s.append(icon(ic, x + 36, y + 36, 32))
        s.append(f'<text x="{x + 24}" y="{y + 122}" font-family="{SANS}" font-size="{title_size}" font-weight="700" fill="{INK}">{esc(name)}</text>')
        for j, line in enumerate(lines):
            s.append(f'<text x="{x + 24}" y="{y + 162 + j * 30}" font-family="{SANS}" font-size="{line_size}" fill="{MUTED}">{esc(line)}</text>')
        s.append('</g>')
    s.append('</svg>')
    return '\n'.join(s) + '\n'


def stats(lang):
    t = TEXT[lang]
    W, H, m, gap = 1200, 252, 36, 20
    tw = (W - 2 * m - 2 * gap) / 3
    desc = ('；'.join(f'{a}：{b}（{c}）' for a, b, c in t['stats']) if lang == 'zh-CN'
            else '; '.join(f'{a}: {b} ({c})' for a, b, c in t['stats']))
    s = svg_open(W, H, t['stats_title'], desc)
    s.append(f'<rect width="{W}" height="{H}" rx="24" fill="{BG}"/>')
    for i, (label, value, detail) in enumerate(t['stats']):
        x = m + i * (tw + gap)
        s.append(f'<rect x="{x:.1f}" y="{m}" width="{tw:.1f}" height="{H - 2 * m}" rx="16" fill="{PAPER}" stroke="{RULE}"/>')
        s.append(f'<text x="{x + 28:.1f}" y="{m + 44}" font-family="{SANS}" font-size="21" fill="{MUTED}">{esc(label)}</text>')
        s.append(f'<text x="{x + 26:.1f}" y="{m + 112}" font-family="{SANS}" font-size="60" font-weight="600" fill="{INK}">{esc(value)}</text>')
        s.append(f'<rect x="{x + 28:.1f}" y="{m + 128}" width="40" height="3" rx="1.5" fill="{ACCENT}"/>')
        s.append(f'<text x="{x + 28:.1f}" y="{m + 164}" font-family="{SANS}" font-size="20" fill="{MUTED}">{esc(detail)}</text>')
    s.append('</svg>')
    return '\n'.join(s) + '\n'


def frame(src, dst, crop_bottom=None, autocrop=False, pad=56):
    from PIL import Image, ImageChops, ImageDraw, ImageFilter
    shot = Image.open(src).convert('RGB')
    if crop_bottom:
        shot = shot.crop((0, 0, shot.width, crop_bottom))
    if autocrop:  # trim the empty margin around the lines plan, keep 24 px
        box = ImageChops.difference(shot, Image.new('RGB', shot.size, shot.getpixel((2, 2)))).getbbox()
        shot = shot.crop((max(box[0] - 24, 0), max(box[1] - 24, 0), min(box[2] + 24, shot.width), min(box[3] + 24, shot.height)))
    rgb = lambda c: tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))
    w, h = shot.width + 2 * pad, shot.height + 2 * pad
    base = Image.new('RGBA', (w, h), rgb(BG) + (255,))
    # Soft, low-opacity shadow only to lift the window off the panel.
    sm = Image.new('L', (w, h), 0)
    ImageDraw.Draw(sm).rounded_rectangle((pad, pad + 10, pad + shot.width, pad + shot.height + 10), 18, fill=40)
    base = Image.composite(Image.new('RGBA', (w, h), rgb(INK) + (255,)), base, sm.filter(ImageFilter.GaussianBlur(16)))
    smask = Image.new('L', shot.size, 0)
    ImageDraw.Draw(smask).rounded_rectangle((0, 0, shot.width - 1, shot.height - 1), 18, fill=255)
    base.paste(shot, (pad, pad), smask)
    ImageDraw.Draw(base).rounded_rectangle((pad - 1, pad - 1, pad + shot.width, pad + shot.height), 19, outline=rgb(RULE) + (255,), width=2)
    panel = Image.new('L', (w, h), 0)
    ImageDraw.Draw(panel).rounded_rectangle((0, 0, w - 1, h - 1), 40, fill=255)
    base.putalpha(panel)
    base.save(dst, optimize=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--lang', choices=TEXT)
    p.add_argument('--frames', action='store_true')
    a = p.parse_args()
    if a.lang:
        out = ASSETS / a.lang
        out.mkdir(exist_ok=True)
        for key in TEXT[a.lang]['sections']:
            (out / f'section-{key}.svg').write_text(section(a.lang, key), 'utf8')
        (out / 'features.svg').write_text(features(a.lang), 'utf8')
        (out / 'stats.svg').write_text(stats(a.lang), 'utf8')
        print('built', out)
    if a.frames:
        raw = ASSETS / 'raw'
        frame(raw / 'main-window.png', ASSETS / 'main-window.png')
        frame(raw / 'linesplan.png', ASSETS / 'linesplan.png', autocrop=True)
        frame(raw / 'plates.png', ASSETS / 'plates.png')
        frame(raw / 'about.png', ASSETS / 'about.png')
        print('framed screenshots')


if __name__ == '__main__':
    main()
