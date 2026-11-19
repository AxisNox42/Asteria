# Builds Docs/QA/2026-11-18_스킬검증/스킬검증.xlsx from findings.json, the per-card smoke
# captures (shots_active / shots_passive, scripts/test_skill_each_smoke.ps1) and their summary.csv.
#   python -I scripts/skill_qa_sheet.py
import csv
import json
import os
import re
import sys

from openpyxl import Workbook
from openpyxl.drawing.image import Image as XLImage
from openpyxl.styles import Alignment, Font, PatternFill
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QA = os.path.join(ROOT, 'Docs', 'QA', '2026-11-18_스킬검증')
THUMBS = os.path.join(QA, 'thumbs')
os.makedirs(THUMBS, exist_ok=True)

findings = json.load(open(os.path.join(QA, 'findings.json'), encoding='utf-8'))
cards = json.load(open(sys.argv[1], encoding='utf-8')) if len(sys.argv) > 1 else []
names = {c['code']: c for c in cards}

# Which capture to show per card (default t3 = 2.5 s after the pick) and an optional crop
# (x, y, w, h in the 1280x720 capture; default the middle of the field round the ship).
PICK = findings.get('shots', {})
DEFAULT_CROP = (400, 200, 480, 300)


def shot_dir(code):
    for sub in ('shots_v2_active', 'shots_v2_passive', 'shots_fixed', 'shots_active', 'shots_passive'):
        d = os.path.join(QA, sub, code)
        if os.path.isdir(d):
            return d
    return None


def first_code(text):
    m = re.search(r'\b(A_[A-Z_]+|SYN_[A-Z]_[A-Z0-9_]+|S_[A-Z_]+)\b', text)
    return m.group(1) if m else None


def thumb(code):
    if not code:
        return None
    d = shot_dir(code)
    if not d:
        return None
    pick = PICK.get(code, {})
    x, y, w, h = pick.get('crop', DEFAULT_CROP)
    shot = pick.get('shot')
    if not shot:
        # The capture where the most is drawn round the ship (the effect at its biggest).
        best = -1
        for cand in ('t1', 't2', 't3', 't4'):
            p = os.path.join(d, cand + '.jpg')
            if not os.path.exists(p):
                continue
            g = Image.open(p).crop((x, y, x + w, y + h)).convert('L')
            lit = sum(1 for v in g.getdata() if v > 40)
            if lit > best:
                best, shot = lit, cand
    src = os.path.join(d, (shot or 't3') + '.jpg')
    if not os.path.exists(src):
        return None
    out = os.path.join(THUMBS, f'{code}_{shot}.png')
    im = Image.open(src).crop((x, y, x + w, y + h))
    scale = 420.0 / im.width
    im = im.resize((420, int(im.height * scale)), Image.LANCZOS)
    im.save(out)
    return out


HEAD = Font(bold=True, color='FFFFFF')
HEAD_FILL = PatternFill('solid', fgColor='334155')
WRAP = Alignment(wrap_text=True, vertical='top')


def sheet(wb, title, headers, widths):
    ws = wb.create_sheet(title)
    ws.append(headers)
    for i, w in enumerate(widths):
        ws.column_dimensions[chr(65 + i)].width = w
        ws.cell(1, i + 1).font = HEAD
        ws.cell(1, i + 1).fill = HEAD_FILL
    ws.freeze_panes = 'A2'
    return ws


def label(code_text, name):
    return f'{name}\n({code_text})' if name else code_text


wb = Workbook()
wb.remove(wb.active)

# Ambiguous items: one sheet per group, the four columns asked for plus the capture.
order = ['액티브', '신화', '시너지', '스탯']
by_sheet = {k: [] for k in order}
for a in findings['ambiguous']:
    by_sheet.setdefault(a['sheet'], []).append(a)
for key in order:
    ws = sheet(wb, key, ['증강명', '문제', '추천 변경안', '내 의견', '인게임 화면', '처리 결과'], [24, 60, 46, 40, 52, 50])
    for r, a in enumerate(by_sheet.get(key, []), start=2):
        ws.append([label(a['code'], a['name']), a['problem'], a['rec'], a.get('opinion', ''), '', a.get('done', '')])
        for c in (1, 2, 3, 4, 6):
            ws.cell(r, c).alignment = WRAP
        t = thumb(a.get('shot_code') or first_code(a['code']))
        if t:
            img = XLImage(t)
            ws.add_image(img, f'E{r}')
            ws.row_dimensions[r].height = max(120, img.height * 0.78)
        else:
            ws.row_dimensions[r].height = 90

# Fixed bugs.
ws = sheet(wb, '수정함', ['증강명', '문제', '수정 내용', '인게임 화면'], [28, 60, 56, 52])
for r, f in enumerate(findings['fixed'], start=2):
    ws.append([label(f['code'], f['name']), f['problem'], f['fix']])
    for c in range(1, 4):
        ws.cell(r, c).alignment = WRAP
    t = thumb(f.get('shot_code') or first_code(f['code']))
    if t:
        img = XLImage(t)
        ws.add_image(img, f'D{r}')
        ws.row_dimensions[r].height = max(120, img.height * 0.78)
    else:
        ws.row_dimensions[r].height = 75

# Every card that was run: smoke damage over 5 s and where it ended up.
flagged = {}
for f in findings['fixed']:
    for c in re.findall(r'\b(A_[A-Z_]+|SYN_[A-Z]_[A-Z0-9_]+|S_[A-Z_]+)\b', f['code']):
        flagged[c] = '수정함'
for a in findings['ambiguous']:
    for c in re.findall(r'\b(A_[A-Z_]+|SYN_[A-Z]_[A-Z0-9_]+|S_[A-Z_]+)\b', a['code']):
        flagged.setdefault(c, '애매 (' + a['sheet'] + ' 시트)')
ws = sheet(wb, '전체 실측', ['증강명', '등급', '5초 피해', '5초 처치', '결과', '비고'], [34, 12, 12, 12, 22, 60])
notes = findings.get('notes', {})
for sub, gun in (('shots_active', '총 끔 (스킬 피해만)'), ('shots_passive', '총 켬')):
    path = os.path.join(QA, sub, 'summary.csv')
    if not os.path.exists(path):
        continue
    last = {}   # the last run of a card wins (re-runs append to summary.csv)
    for row in csv.DictReader(open(path, encoding='utf-8-sig')):
        if row['code'] != 'code':
            last[row['code']] = row
    for code, row in last.items():
        c = names.get(code, {})
        nm = c.get('name', '카드 없음 (기준)' if code == 'BASE' else '')
        ws.append([label(code, nm), c.get('rarity', ''), row.get('dealt', ''), row.get('kills', ''),
                   flagged.get(code, 'OK' if code != 'BASE' else '기준'), notes.get(code, gun)])

wb.save(os.path.join(QA, '스킬검증.xlsx'))
print('saved', os.path.join(QA, '스킬검증.xlsx'))
