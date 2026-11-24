#pragma once
// ─────────────────────────────────────────────────────────────
// 직업(클래스) 시스템 (업적은 2026-11-24 제거됨, 파일 이름만 남음)
//   직업: 현재는 소총/전기장 무기 선택만 제공한다.
//   구매 상태(g_JobBought)는 SaveSystem 으로 영구 저장.
// ─────────────────────────────────────────────────────────────
#include "Meta.h"      // g_Coins, g_Language, LANG_COUNT
#include "Weapons.h"   // StartWeapon (직업별 고정 무기)

// ── 직업(클래스) ───────────────────────────────────
enum JobId {
    JOB_NONE,              // Standard rifle.
    JOB_RESERVED_1,
    JOB_RESERVED_2,
    JOB_RESERVED_3,
    JOB_STATIC_FIELD,      // Static field; retains the old SMG save slot.
    JOB_RESERVED_4,
    JOB_RESERVED_5,
    JOB_COUNT
};
// Only the rifle and static field are reachable from a new run. Reserved
// slots keep old save-file indices stable without exposing retired jobs.
inline constexpr int JOB_PLAYABLE = 2;
inline bool IsPlayableJob(int j) {
    return j == JOB_NONE || j == JOB_STATIC_FIELD;
}

struct JobDef {
    const wchar_t* name[3];
    const wchar_t* desc[3];
    int            fixedWeapon;        // StartWeapon 인덱스 (-1=해당 없음)
};
inline const JobDef JOB_DEFS[JOB_COUNT] = {
    /* JOB_NONE */ {
        { L"방랑자", L"Wanderer", L"放浪者" },
        { L"소총 — 표준 연사, 균형 잡힌 범용 무기",
          L"Rifle — standard fire rate, balanced all-rounder",
          L"ライフル — 標準連射、バランス型汎用武器" },
        (int)StartWeapon::RIFLE },
    /* JOB_RESERVED_1 */ {
        { L"REMOVED", L"REMOVED", L"REMOVED" },
        { L"This job is no longer available", L"This job is no longer available", L"This job is no longer available" },
        -1 },
    /* JOB_RESERVED_2 */ {
        { L"REMOVED", L"REMOVED", L"REMOVED" },
        { L"This job is no longer available", L"This job is no longer available", L"This job is no longer available" },
        -1 },
    /* JOB_RESERVED_3 */ {
        { L"REMOVED", L"REMOVED", L"REMOVED" },
        { L"This job is no longer available", L"This job is no longer available", L"This job is no longer available" },
        -1 },
    /* JOB_STATIC_FIELD */ {
        { L"정전기장", L"STATIC FIELD", L"静電場" },
        { L"플레이어 주변을 지속 전기장으로 제압하는 범위형 무기",
          L"Area weapon that controls nearby space with a sustained electric field",
          L"プレイヤー周囲を持続電場で制圧する範囲武器" },
        (int)StartWeapon::SMG },
    /* JOB_RESERVED_4 */ {
        { L"REMOVED", L"REMOVED", L"REMOVED" },
        { L"This job is no longer available", L"This job is no longer available", L"This job is no longer available" },
        -1 },
    /* JOB_RESERVED_5 */ {
        { L"REMOVED", L"REMOVED", L"REMOVED" },
        { L"This job is no longer available", L"This job is no longer available", L"This job is no longer available" },
        -1 },
};

// ── 영구/런타임 상태 ────────────────────────────────
inline int       g_SelectedJob    = JOB_NONE; // 현재 판 선택 직업 (저장 안 함)
inline bool      g_JobBought[JOB_COUNT] = {}; // 골드로 구매한 직업 해금 (저장됨)

// 로컬라이즈 헬퍼
inline const wchar_t* JobName(int id) {
    int li = LangIndex();
    return JOB_DEFS[id].name[li];
}
inline const wchar_t* JobDesc(int id) {
    int li = LangIndex();
    return JOB_DEFS[id].desc[li];
}

// 직업 해금 여부 (항상 해금 or 골드 구매)
inline bool JobUnlocked(int j) {
    if (j < 0 || j >= JOB_COUNT) return false;
    if (!IsPlayableJob(j)) return false;
    if (j == JOB_NONE || j == JOB_STATIC_FIELD) return true;
    return g_JobBought[j];
}
