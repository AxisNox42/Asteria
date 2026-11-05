"""Builds Docs/QA/Asteria_증강기획_시너지_v1.xlsx from the tables the
balance test exports (tests/augment_synergy_test.cpp --csv), so the sheet and
the code (WiNILL/Entity/AugmentSynergy.h) cannot drift apart.

    powershell -File scripts/test_augment_synergy.ps1 -Csv build/synergy_csv > build/syn_out.txt
    python scripts/build_synergy_xlsx.py
"""
import csv
import re
import sys
from pathlib import Path

from openpyxl import Workbook, load_workbook
from openpyxl.styles import Alignment, Font, PatternFill
from openpyxl.utils import get_column_letter

ROOT = Path(__file__).resolve().parent.parent
CSV_DIR = ROOT / "build" / "synergy_csv"
LOG = ROOT / "build" / "syn_out.txt"
OUT = ROOT / "Docs" / "QA" / "Asteria_증강기획_시너지_v1.xlsx"
OLD = ROOT / "Docs" / "QA" / "Asteria_증강기획_RnS기반_100.xlsx"

TAGS = [
    ("PIERCE", "탄 동사", "적을 뚫고 지나감"), ("BOUNCE", "탄 동사", "벽·적에서 튕김"),
    ("SPLIT", "탄 동사", "명중 시 분열"), ("HOMING", "탄 동사", "적 쪽으로 휨"),
    ("BLAST", "탄 동사", "착탄 시 폭발"), ("BEAM", "프레임", "레이저 빔"),
    ("ORBIT", "탄 동사", "플레이어 주위를 도는 탄"), ("CHAIN", "탄 동사", "처치 시 번개가 튐"),
    ("RAPID", "발사", "고연사 (미니건)"), ("CHARGE", "발사", "충전 후 발사 (저격)"),
    ("SPREAD", "발사", "산탄 (샷건·산개탄)"), ("NTH", "발사", "n번째 발사 트리거"),
    ("ON_KILL", "이벤트", "처치 시"), ("ON_CRIT", "이벤트", "치명타 시"),
    ("ON_BOUNCE", "이벤트", "튕길 때"), ("ON_HURT", "이벤트", "피격 시"),
    ("ON_STILL", "이벤트", "정지했을 때"), ("ON_DASH", "이벤트", "대시할 때"),
    ("ON_WAVE", "이벤트", "웨이브 시작 시"), ("SHIELD", "자원", "보호막"),
    ("HP", "자원", "체력을 건드림"), ("TRAIL", "탄 동사", "지나간 자리에 잔상"),
    ("RETURN", "탄 동사", "되돌아옴"), ("GLASS", "자원", "최대 체력 30으로 고정"),
    ("ON_WALL", "이벤트", "벽에 닿을 때"), ("HOVER", "탄 동사", "멈췄다가 다시 출발"),
    ("BURST", "프레임", "점사 (세 발씩 끊어 쏨)"), ("BURN", "상태", "적이 불탐"),
    ("MARK", "상태", "적에게 표식"), ("DICE", "탄 동사", "발사마다 동사를 무작위로 얻음"),
    ("PULL", "탄 동사", "적을 끌어당김"), ("ERASE", "탄 동사", "적탄을 지움"),
    ("TRAP", "탄 동사", "자리에 박혀 기다림"), ("GROW", "탄 동사", "날아갈수록 커짐"),
    ("SWAP", "탄 동사", "맞힌 적과 위치를 바꿈"), ("STORE", "자원", "받은 피해를 저장"),
    ("COMBO", "상태", "연속 처치 콤보"), ("RHYTHM", "발사", "일정한 박자로 쏨"),
    ("LOB", "프레임", "포물선 박격 사격"),
]

TRANSLATE = [
    ("BOUNCE", "벽에서 튕김", "빔이 벽에서 반사 (경로 전체가 판정)", "파편 전체가 튕겨 방이 가득 참", "한 발이 튕기며 길게", "튕기는 탄이 화면을 도배"),
    ("PIERCE", "적을 뚫음", "이미 관통 — 빔 굵기로 변환", "파편마다 관통", "일렬 관통 저격", "약한 관통 탄 다수"),
    ("SPLIT", "명중 시 분열", "빔 끝에서 분기", "분열의 분열 (폭발적)", "명중 시 부채꼴", "분열이 자주 터져 과열"),
    ("HOMING", "적에게 휨", "빔이 가까운 적으로 꺾임", "파편이 각자 추적", "유도 저격", "약한 유도 폭우"),
    ("BLAST", "착탄 폭발", "빔 끝에 폭발", "폭발 중첩", "단일 대폭발", "연쇄 소폭발"),
]

CHANGES = [
    ("겹치거나 수치만 올리던 카드 34장", "삭제 (2026-11-04)", "'정리한 카드' 시트에 카드별 이유가 있음"),
    ("새 카드 31장", "추가", "프레임 2(파동포·박격포), 동사 5, 트리거 8, 증폭 16 — 모두 새 규칙을 가진 카드"),
    ("시너지 카드 109장 게임 연결", "구현 (2026-11-04)", "SynergyRuntime.cpp에 효과 구현, 카탈로그에 SYN_* 로 등록. 저장된 도감 번호가 밀리지 않도록 enum 맨 끝에 붙임"),
    ("태그 64비트 확장", "코드", "태그 39종 (기존 31 → 39). '다양성' 카드가 쓰는 scales 필드 추가"),
    ("체력 칸 방식 (5칸 고정)", "철회 (2026-11-04)", "체력은 100 기준 실수 값으로 되돌림. 재생·최대 체력 성장·체력 카드는 원래대로"),
    ("N_ATTACK / N_RATE / N_CRIT / N_MOVE", "삭제", "가장 낮은 등급의 단순 수치 증가는 골랐을 때 변화를 느끼기 힘들어 제거"),
    ("체력 회복 계열 3종 수치", "조정", "스침 회복 0.5 → 0.3, 별빛 치유 15/쿨 20초 → 12/쿨 24초, 생명 연소 폭발 100% → 70% (소모 체력 1당)"),
    ("위력 과다 카드 26장", "수치 조정 (적용)", "Docs/Design/Augment_Balance_Pass_2026-11-04.md 표 참조"),
    ("탄 동사 5종 (도탄·관통탄·분열탄·유도탄·파열탄)", "추가 (적용)", "게임 본체에 연결됨"),
    ("시너지 표 60장 추가", "추가 (표·시트만)", "프레임 2, 동사 9, 증폭 28, 트리거 6, 대가 3, 보호 6, 신화 6 — 게임에는 아직 연결 안 됨"),
    ("P_GEM_GLASS_STAR", "예정", "유리대포(C_GLASS)로 흡수 — 아직 기존 효과 그대로"),
]

REMOVED_EXTRA = {
    "T_DASH": "T_AFTERIMAGE·T_FIREWORK와 같은 부류 (대시 보너스) — 새 대시 카드 둘로 대체",
    "G_BUFFER": "G_CHARGER와 같음 (보호막 한 덩이)",
    "G_SOFTLAND": "G_PARRY와 같음 (대시 직후 회피)",
}
REMOVED_FILE = ROOT / "build" / "removed_cards.txt"

HEAD_FILL = PatternFill("solid", fgColor="1F2A44")
HEAD_FONT = Font(bold=True, color="FFFFFF")
ROLE_FILL = {
    "프레임": "DCE6F1", "동사": "E2EFDA", "트리거": "FFF2CC", "증폭": "FCE4D6",
    "대가": "F8CBAD", "보호": "D9E1F2",
}
RAR_ORDER = ["일반", "희귀", "에픽", "전설", "신화"]
BANDS = {"일반": "+5 ~ +20%", "희귀": "+20 ~ +45%", "에픽": "+45 ~ +70%", "전설": "+70 ~ +100%", "신화": "+100% 이상"}


def read_csv(name):
    with open(CSV_DIR / name, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def syllables(text):
    return sum(1 for ch in text if ch != " ")


def sheet(wb, title, header, rows, widths, wrap_cols=()):
    ws = wb.create_sheet(title)
    ws.append(header)
    for c in ws[1]:
        c.fill, c.font = HEAD_FILL, HEAD_FONT
        c.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
    for r in rows:
        ws.append(r)
    for i, w in enumerate(widths, 1):
        ws.column_dimensions[get_column_letter(i)].width = w
    for row in ws.iter_rows(min_row=2):
        for c in row:
            c.alignment = Alignment(vertical="top", wrap_text=(c.column in wrap_cols))
    ws.freeze_panes = "A2"
    return ws


def read_log():
    raw = LOG.read_bytes()
    for enc in ("utf-8-sig", "utf-16", "cp949"):
        try:
            return raw.decode(enc)
        except UnicodeError:
            continue
    return raw.decode("utf-8", "replace")


def main():
    if not CSV_DIR.exists():
        sys.exit("run scripts/test_augment_synergy.ps1 -Csv build/synergy_csv first")
    cards = read_csv("cards.csv")
    builds = read_csv("builds.csv")
    gives = {c["code"]: set(filter(None, c["gives"].split("|"))) for c in cards}
    needs = {c["code"]: set(filter(None, c["needs"].split("|"))) for c in cards}
    scales = {c["code"]: set(filter(None, c.get("scales", "").split("|"))) for c in cards}
    partner = {}
    for c in cards:
        k = c["code"]
        feeds = [o for o in gives if o != k and ((needs[o] | scales[o]) & gives[k])]
        fed = [o for o in gives if o != k and ((needs[k] | scales[k]) & gives[o])]
        partner[k] = (len(feeds), len(fed))
    wb = Workbook()
    wb.remove(wb.active)

    # 1. 증강 목록
    rows = []
    for n, c in enumerate(cards, 1):
        is_frame = c["role"] == "프레임"
        feeds, fed = partner[c["code"]]
        note = "필요 태그가 없으면 효과 없음" if c["needs"] else ("가진 종류가 많을수록 강해짐" if c.get("scales") else "")
        rows.append([n, c["code"], c["kr"], syllables(c["kr"]), c["rarity"], c["role"], c["gives"], c["needs"],
                     "가능" if c["stack"] == "1" else "", c["rule"], float(c["single_power"]),
                     "" if is_frame else float(c["gain"]), feeds + fed, note])
    ws = sheet(wb, "증강 목록",
               ["No", "코드명", "이름(KR)", "음절", "등급", "역할", "붙이는 태그", "필요 태그", "중첩",
                "카드 규칙 (자기 규칙만 적음)", "단독 위력(기본 총=1.0)", "켜졌을 때 위력 배수", "파트너 수", "비고"],
               rows, [5, 18, 18, 6, 8, 8, 24, 16, 6, 54, 14, 14, 10, 26], wrap_cols=(7, 8, 10))
    for row in ws.iter_rows(min_row=2):
        fill = ROLE_FILL.get(row[5].value)
        if fill:
            for c in row[:6]:
                c.fill = PatternFill("solid", fgColor=fill)
    ws.auto_filter.ref = ws.dimensions

    # 2. 등급 분포 (수식)
    last = len(cards) + 1
    rows = []
    for i, rar in enumerate(RAR_ORDER, 2):
        rows.append([rar, BANDS[rar],
                     f"=COUNTIFS('증강 목록'!$E$2:$E${last},A{i},'증강 목록'!$F$2:$F${last},\"<>프레임\")",
                     f"=COUNTIFS('증강 목록'!$E$2:$E${last},A{i},'증강 목록'!$F$2:$F${last},\"프레임\")",
                     f"=C{i}+D{i}",
                     f"=IFERROR(AVERAGEIFS('증강 목록'!$D$2:$D${last},'증강 목록'!$E$2:$E${last},A{i},'증강 목록'!$F$2:$F${last},\"<>프레임\"),0)"])
    rows.append(["합계", "", "=SUM(C2:C6)", "=SUM(D2:D6)", "=SUM(E2:E6)", ""])
    sheet(wb, "등급 분포", ["등급", "켜졌을 때 위력 이득 대역", "카드(프레임 제외)", "프레임", "합계", "평균 음절(프레임 제외)"],
          rows, [8, 24, 18, 8, 8, 20])

    # 3. 태그 사전 (공급/요구 개수는 수식)
    rows = []
    for i, (tag, kind, meaning) in enumerate(TAGS, 2):
        rows.append([tag, kind, meaning,
                     f"=COUNTIF('증강 목록'!$G$2:$G${last},\"*\"&A{i}&\"*\")",
                     f"=COUNTIF('증강 목록'!$H$2:$H${last},\"*\"&A{i}&\"*\")"])
    sheet(wb, "태그 사전", ["태그", "종류", "뜻", "붙이는 카드 수", "필요로 하는 카드 수"],
          rows, [14, 10, 36, 14, 18])

    # 4. 시너지 지도: 태그별로 누가 주고 누가 받는가
    by_give, by_need = {}, {}
    for c in cards:
        for t in filter(None, c["gives"].split("|")):
            by_give.setdefault(t, []).append(c["kr"])
        for t in filter(None, (c["needs"] + "|" + c.get("scales", "")).split("|")):
            by_need.setdefault(t, []).append(c["kr"])
    rows = [[t, ", ".join(by_give.get(t, [])), ", ".join(by_need.get(t, [])),
             "받을 곳 없음" if t in by_give and t not in by_need else ("만드는 곳 없음" if t in by_need and t not in by_give else "")]
            for t, _, _ in TAGS if t in by_give or t in by_need]
    sheet(wb, "시너지 지도", ["태그", "붙이는 카드", "이 태그가 있어야 켜지는 카드", "점검"],
          rows, [12, 56, 56, 14], wrap_cols=(2, 3))

    # 5. 프레임 변환표
    sheet(wb, "프레임 변환표",
          ["동사", "기본 탄", "레이저(BEAM)", "샷건(SPREAD)", "저격(CHARGE)", "미니건(RAPID)"],
          [list(r) for r in TRANSLATE], [10, 18, 34, 28, 22, 24], wrap_cols=(2, 3, 4, 5, 6))

    # 6. 빌드 예시
    rows = [[b["name"], b["frame"], b["cards"], int(b["dps"]), float(b["survival"]),
             float(b["power"]), float(b["vs_median"]), "있음" if b["dead"] != "0" else "없음"]
            for b in builds]
    sheet(wb, "빌드 예시",
          ["빌드", "프레임", "구성", "예상 DPS", "생존(1.0=기본 100 HP)", "위력", "무작위 중앙값 대비", "죽은 카드"],
          rows, [18, 10, 80, 10, 18, 8, 16, 10], wrap_cols=(3,))

    # 7. 밸런스 결과 (로그 요약)
    log = read_log().splitlines()
    summary = []
    for line in log:
        s = line.strip()
        if re.match(r"(uniform|biased) draw:", s) or s.startswith(("median biased", "single-card range", "best run", "names ", "avg ", "bare gun", "cards:")):
            summary.append([s])
    checks = [[ln.strip()] for ln in log if ln.strip().startswith(("[ok]", "[FAIL]"))]
    ws = sheet(wb, "밸런스 결과", ["요약 (기대값 모델, 무작위 20만 회 × 프레임 + 카드 6장)"],
               summary + [[""], ["검사 항목"]] + checks, [120])
    ws.append([])
    ws.append([f"통과 {sum(1 for c in checks if c[0].startswith('[ok]'))} / 전체 {len(checks)}"])

    # 8. 작명 원칙 (기존 100개 시트에서 가져옴) + 이 표의 적용
    names = [["R1", "효과를 드러내는 이름", "Keller·Heckler·Houston (1998), J. of Marketing 62(1)", "이점을 드러내는 이름이 그 이점의 기억을 높임", "이름에 효과를 가리키는 말(증폭·폭발·방벽·반응·가속 등)을 넣음", "광고 회상 실험이며 게임 UI 연구는 아님"]]
    if OLD.exists():
        owb = load_workbook(OLD, read_only=True)
        if "작명 원칙" in owb.sheetnames:
            names = []
            for r in owb["작명 원칙"].iter_rows(min_row=2, max_col=6, values_only=True):
                if r[0] and str(r[0]).startswith("R"):
                    names.append(list(r))
    applied = {
        "R1": "카드 이름마다 효과어를 넣음 (프레임은 무기 이름이라 예외)",
        "R2": "한글과 띄어쓰기만, 2~7음절 — 테스트가 검사함 (이전 'N번째 탄'은 '박자 추가탄'으로 바꿈)",
        "R3": "소리만 옮긴 외래어 대신 뜻을 옮긴 이름 (예: 레이저·샷건·미니건 같은 굳은 말만 예외)",
        "R4": "등급이 높을수록 긴 이름 — 평균 음절이 일반 3.9 → 신화 6.8로 늘어남 (테스트가 검사함, 프레임 제외)",
        "R6": "같은 종류는 같은 패턴: 탄 동사 '~탄', 증폭 '~ 증폭/폭발', 트리거 '~ 반응/가속', 보호 '~ 방벽/방호'",
    }
    rows = []
    for r in names:
        rows.append(r + [applied.get(str(r[0]), "")])
    sheet(wb, "작명 원칙", ["ID", "원칙", "근거 연구", "연구 결과 (요약)", "Asteria 적용(기존)", "한계·주의", "이 표에서의 적용"],
          rows, [5, 22, 44, 44, 44, 36, 52], wrap_cols=(2, 3, 4, 5, 6, 7))

    # 8b. 시너지 점검 (카드별 파트너와 효용)
    rows = []
    for c in cards:
        if c["role"] == "프레임":
            continue
        feeds, fed = partner[c["code"]]
        lift = float(c["lift"]) if c.get("lift") else 0.0
        verdict = "" if lift >= 0.97 or lift == 0 else "약함"
        if feeds + fed < 2 and c["role"] not in ("증폭",):
            verdict = (verdict + " 파트너 부족").strip()
        rows.append([c["code"], c["kr"], c["rarity"], c["role"], feeds, fed, feeds + fed, lift or "", verdict])
    sheet(wb, "시너지 점검",
          ["코드명", "이름", "등급", "역할", "이 카드가 켜주는 카드", "이 카드를 켜주는 카드", "파트너 합계",
           "효용 (이 카드를 가진 구성의 위력 ÷ 평균 구성)", "점검"],
          rows, [18, 18, 8, 8, 18, 18, 12, 30, 14])

    # 8c. 정리한 카드
    removed = []
    if REMOVED_FILE.exists():
        for line in REMOVED_FILE.read_text(encoding="utf-8").splitlines():
            if "\t" in line:
                code, why = line.split("\t", 1)
                removed.append([code, why])
    removed += [[k, v] for k, v in REMOVED_EXTRA.items()]
    sheet(wb, "정리한 카드", ["코드명", "삭제 이유"], removed, [20, 80], wrap_cols=(2,))

    # 9. 변경 내역
    sheet(wb, "기존 증강 처리", ["항목", "처리", "내용"], [list(r) for r in CHANGES],
          [38, 18, 80], wrap_cols=(3,))

    # 10. 설계 메모
    total = len(cards)
    notes = [
        ["버전", f"v1 — 시너지 증강 {total}장. 기존 100개 목록(Asteria_증강기획_RnS기반_100.xlsx)을 대체하지 않고 병행. 2026-11-04: 체력 5칸 고정을 철회하고 실수 체력(100)으로 되돌림. 단순 수치 증가 4종(N_*) 제거."],
        ["체력 규칙", "최대 체력 100, 자연 재생, 체력 카드 모두 원래대로. 시너지 모델은 100 HP·평균 피격 10으로 계산하고, 보호막은 피격 1회(10 HP) 단위로 셈."],
        ["데이터 위치", "WiNILL/Entity/AugmentSynergy.h (시트와 테스트가 같은 표를 씀). 시트는 scripts/build_synergy_xlsx.py 로 다시 만듦."],
        ["위력 계산", "위력 = (예상 DPS ÷ 기본 총 DPS) × 생존^0.5. 6배를 넘는 부분은 40%만 인정(과잉 피해). 켜졌을 때 위력 배수 = 그 카드가 켜지는 최소 구성에서 카드를 넣기 전후의 위력 비."],
        ["등급 기준", "켜졌을 때 위력 배수로 등급을 정함: 일반 1.05~1.20, 희귀 1.20~1.45, 에픽 1.45~1.70, 전설 1.70~2.00, 신화 2.00 이상. 대역이 겹치지 않고 테스트가 모든 카드를 검사함."],
        ["기준 상수", "기본 명중 75%, 직선상 적 1.5마리, 튕김 후 재명중 30%, 분열 자식 명중 70%, 폭발 이웃 1.8마리, 탄 1발의 최대 명중 3회, 분당 피격 10회."],
        ["검증 기준", "프레임 단독 0.8~1.35배 / 카드 단독 2.6배 이내(신화 제외) / 무작위 6장 p99 ≤ 중앙값 3.5배, 최고 ≤ 12배 / 빌드 예시 중앙값 1.0~6배 / 증폭은 필요 태그가 있을 때만 켜짐 / 이름 규칙."],
        ["겹침·시너지·효용 검사", "테스트가 (1) 같은 역할·태그·효과 구성이 같은 카드 쌍이 없는지, (2) 모든 카드에 파트너가 있는지 (동사·트리거·대가·보호는 2장 이상, 증폭·프레임은 1장 이상), (3) 켜진 카드가 들어간 구성이 평균 구성보다 약하지 않은지(효용 0.97 이상)를 검사함."],
        ["게임 연결", "표의 109장(프레임 8, 동사·트리거·증폭·대가·보호·신화)이 모두 게임에 들어 있음. 카드 효과는 WiNILL/Entity/SynergyRuntime.cpp, 등록은 SynergyCodes.inc·SynergyCatalogRows.inc(scripts/gen_synergy_catalog.py가 표에서 만듦). 영어·일본어 문구는 scripts/synergy_locale.py."],
        ["남은 것", "카드 아이콘, 카드 드로우 가중(보유 태그가 필요한 카드를 더 자주), 코덱스의 조합 기록. 일부 카드는 규칙 글을 구현 가능한 형태로 바꿨음(저격총은 충전 대신 느린 관통탄, 박자 사격은 4번째 사격마다 강화탄)."],
        ["재현", "powershell -File scripts/test_augment_synergy.ps1 -Csv build/synergy_csv > build/syn_out.txt ; python scripts/build_synergy_xlsx.py"],
    ]
    sheet(wb, "설계 메모", ["항목", "내용"], notes, [18, 130], wrap_cols=(2,))

    OUT.parent.mkdir(parents=True, exist_ok=True)
    wb.save(OUT)
    print("saved", OUT, total, "cards")


if __name__ == "__main__":
    main()
