# Balance sheet -> balance.cfg (the override file BalanceTuning.h reads).
# Takes the "최종 수치" sheet: column A = field name, D..G = EASY..NIGHTMARE.
#   python scripts/balance_from_sheet.py Docs/QA/밸런스_초안.xlsx build/windows/x64/Release/balance.cfg
# Put balance.cfg next to the exe, or point ASTERIA_BALANCE at it.
import sys

import openpyxl

SHEET = "최종 수치"


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: balance_from_sheet.py <sheet.xlsx> <out.cfg>")
    ws = openpyxl.load_workbook(sys.argv[1], data_only=True)[SHEET]
    lines = [f"# exported from {sys.argv[1]} ({SHEET}): EASY, NORMAL, HARD, NIGHTMARE"]
    for row in ws.iter_rows(min_row=2, values_only=True):
        key, values = row[0], row[3:7]
        if not isinstance(key, str) or not all(isinstance(v, (int, float)) for v in values):
            continue
        lines.append(f"{key} = " + ", ".join(f"{v:g}" for v in values))
    with open(sys.argv[2], "w", encoding="utf-8") as out:
        out.write("\n".join(lines) + "\n")
    print(f"{len(lines) - 1} rows -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
