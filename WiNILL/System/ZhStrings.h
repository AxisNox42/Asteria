#pragma once
// ─────────────────────────────────────────────────────────────
// Simplified Chinese for the on-screen UI.
//
// The game's text tables hold KR / EN / JP. Chinese reuses the English slot
// (LangIndex() maps ZH to EN) and this table swaps an English UI string for
// its Chinese one at the last moment: the text renderer and the label helpers
// (ShopText, SettingsText, T) call ZhTranslate(). A string with no entry is
// drawn in English, so untranslated long copy (augment / signal descriptions,
// tutorial text) degrades to English instead of breaking.
//
// Keys are the exact English strings, including double spaces and printf
// placeholders (the Chinese text keeps the same placeholders in the same order).
// ─────────────────────────────────────────────────────────────
#include <cstdint>
#include <cwchar>

namespace zh {

struct Entry { const wchar_t* en; const wchar_t* zh; };

inline const Entry kEntries[] = {
    // Lobby / boot
    { L"TUTORIAL", L"教程" }, { L"SINGLE PLAY", L"单人游戏" }, { L"ASTRAL LOG", L"星界记录" },
    { L"SHOP", L"商店" }, { L"SETTINGS", L"设置" }, { L"EXIT", L"退出" },
    { L"> mounting modules ...", L"> 正在挂载模块 ..." },
    { L"> loading assets        [ OK ]", L"> 加载资源        [ OK ]" },
    { L"> init renderer         [ OK ]", L"> 初始化渲染器    [ OK ]" },
    { L"> linking star lattice    [ OK ]", L"> 连接星格        [ OK ]" },
    { L"> verify save data      [ OK ]", L"> 校验存档        [ OK ]" },
    { L"> ready.", L"> 准备完成。" },
    // Pause / confirm
    { L"CONTINUE", L"继续" }, { L"AUGMENTS", L"已有增强" }, { L"ABANDON", L"放弃" },
    { L"MAIN MENU", L"主菜单" }, { L"PLAY PAUSED", L"游戏暂停" }, { L"TUTORIAL PAUSED", L"教程暂停" },
    { L"EXIT THE GAME?", L"退出游戏吗？" }, { L"YOU WILL RETURN TO THE DESKTOP.", L"将返回桌面。" },
    { L"RESTART THIS PLAY?", L"重新开始本局吗？" }, { L"A NEW PLAY STARTS FROM THE BEGINNING.", L"将从头开始新的一局。" },
    { L"ABANDON THIS PLAY?", L"放弃本局吗？" }, { L"YOUR CURRENT PROGRESS ENDS HERE.", L"当前进度将在此结束。" },
    { L"YES", L"是" }, { L"NO", L"否" }, { L"BACK", L"返回" }, { L"Back", L"返回" },
    // Settings
    { L"DISPLAY", L"显示" }, { L"AUDIO", L"声音" }, { L"GAMEPLAY", L"游戏玩法" }, { L"CONTROLS", L"操作" },
    { L"OTHER", L"其他" }, { L"CALIBRATION", L"校准" }, { L"SYSTEM CALIBRATION", L"系统校准" },
    { L"FPS CAP", L"帧率上限" }, { L"VSYNC", L"垂直同步" }, { L"UNLIM", L"无限制" }, { L"GRAPHICS", L"画质" },
    { L"LOW", L"低" }, { L"MEDIUM", L"中" }, { L"HIGH", L"高" }, { L"COMBO HUD", L"连击显示" },
    { L"MENU DIMMING", L"画面暗度" }, { L"MASTER VOL", L"主音量" }, { L"BGM BUS", L"背景音乐" },
    { L"BGM", L"背景音乐" }, { L"SFX BUS", L"音效" }, { L"OUTPUT", L"输出" }, { L"STEREO", L"立体声" },
    { L"MONO", L"单声道" }, { L"AUDIO ENGINE", L"音频引擎" }, { L"AUTO SKILL", L"自动技能" },
    { L"CROSSHAIR", L"准星" }, { L"MY PHOTOS IN PANES", L"首领窗口显示我的照片" },
    { L"DEBUG MODE", L"调试模式" }, { L"ON", L"开" }, { L"OFF", L"关" },
    { L"MOVE UP", L"向上移动" }, { L"MOVE DOWN", L"向下移动" }, { L"MOVE LEFT", L"向左移动" },
    { L"MOVE RIGHT", L"向右移动" }, { L"DASH", L"冲刺" }, { L"SKILL 1", L"技能 1" },
    { L"SKILL 2", L"技能 2" }, { L"SKILL 3", L"技能 3" }, { L"RESTORE DEFAULTS", L"恢复默认" },
    { L"LANGUAGE", L"语言" }, { L"RESET DATA", L"重置数据" }, { L"RESET", L"重置" },
    { L"HOLD TO RESET", L"长按重置" }, { L"ERASING...", L"正在删除..." }, { L"RESET COMPLETE", L"重置完成" },
    { L"SAVE CHANGES", L"保存更改" }, { L"SAVE & BACK", L"保存并返回" }, { L"DON'T SAVE", L"不保存" },
    { L"SAVE SETTINGS BEFORE EXIT?", L"退出前保存设置吗？" }, { L"UNSAVED CHANGES", L"有未保存的更改" },
    { L"PRESS A KEY...", L"请按下按键..." }, { L"THAT KEY IS RESERVED", L"该按键已被保留" },
    { L"ESC OR RIGHT-CLICK TO CANCEL", L"按 ESC 或右键取消" }, { L"SWAPPED KEYS:  ", L"已交换按键：" },
    { L"TERMINATE", L"结束" }, { L"ABANDON PLAY", L"放弃本局" }, { L"CREDITS", L"制作人员" },
    { L"VIEW", L"查看" }, { L"TEAM PINATA", L"制作团队" }, { L"THIRD-PARTY SOFTWARE", L"第三方软件" },
    { L"FONTS", L"字体" }, { L"ESC OR CLICK TO CLOSE", L"按 ESC 或点击关闭" },
    { L"PRECISION · SINGLE TARGET", L"精准 · 单体" }, { L"AREA CONTROL · SUSTAINED", L"范围控制 · 持续" },
    { L"SPEED", L"速度" },
    // Shop
    { L"ALL", L"全部" }, { L"UPGRADES", L"永久升级" }, { L"PROFILES", L"显示配置" }, { L"CODEX", L"图鉴" },
    { L"BASIC", L"基础" }, { L"ADVANCED", L"进阶" }, { L"LEGENDARY", L"传说" }, { L"TIER", L"等阶" },
    { L"RIFLE CONSTELLATION", L"步枪星座" }, { L"FIELD ARRAY", L"卫星阵列" },
    { L"AFTER  %ls STARDUST", L"购买后  %ls 星尘" }, { L"COST  %ls STARDUST", L"费用  %ls 星尘" },
    { L"STARDUST  %ls", L"持有星尘  %ls" }, { L"LEVEL  %d / %d", L"等级  %d / %d" },
    { L"BUY", L"购买" }, { L"UPGRADE", L"升级" }, { L"NOT ENOUGH", L"星尘不足" }, { L"OWNED", L"已拥有" },
    { L"MAX", L"最大" }, { L"MAX LEVEL", L"最高等级" }, { L"PERMANENT UPGRADE", L"永久升级" },
    { L"DISPLAY PROFILE", L"显示配置" }, { L"CODEX · PAYLOAD", L"图鉴 · 载荷" },
    { L"PERMANENT EFFECT APPLIED BEFORE LAUNCH", L"出击前生效的永久效果" },
    { L"APPLIED BEFORE LAUNCH", L"出击前生效" },
    { L"COLOUR USED BY MENUS AND THE PLAYER WINDOW", L"用于菜单和玩家窗口的颜色" },
    { L"IN-PLAY EFFECT · CODEX ONLY", L"游戏内效果 · 仅图鉴" },
    { L"Max HP +20", L"最大生命 +20" }, { L"Attack +6%", L"攻击力 +6%" },
    { L"Move Speed +5%", L"移动速度 +5%" }, { L"Identity Slot +1", L"识别槽位 +1" },
    { L"Cyan", L"青色" }, { L"Matrix", L"矩阵" }, { L"Amber", L"琥珀" }, { L"Magenta", L"品红" },
    { L"Blood", L"血红" }, { L"Violet", L"紫罗兰" }, { L"Gold", L"金色" },
    // Codex (Astral Log)
    { L"SEARCH", L"搜索" }, { L"NO MATCHES", L"无匹配结果" }, { L"TRY A DIFFERENT SEARCH", L"请尝试其他搜索词" },
    { L"%d MATCHES", L"%d 个结果" }, { L"ENTITIES", L"观测体" }, { L"SIGNAL CLASS", L"信号类别" },
    { L"UNDISCOVERED", L"未发现" }, { L"UNDISCOVERED SIGNAL", L"未发现的信号" },
    { L"RECORD SUMMARY", L"记录摘要" }, { L"TIER %ls", L"等阶 %ls" }, { L"TIER ?", L"等阶 ?" },
    { L"TYPE : %ls", L"类型 : %ls" }, { L"TIER : %ls", L"等阶 : %ls" }, { L"TIER : LOCKED", L"等阶 : 未解锁" },
    { L"NEXT : TIER AND OVERVIEW", L"下一项 : 等阶与概览" }, { L"NEXT : COMBAT STATS", L"下一项 : 战斗属性" },
    { L"NEXT : ATTACK PROFILE", L"下一项 : 攻击模式" }, { L"OBSERVED", L"已观测" },
    { L"OBSERVED FORM", L"已观测形态" }, { L"UNKNOWN", L"未知" }, { L"UNASSIGNED", L"未分配" },
    { L"ATTACK : ", L"攻击 : " }, { L"DAMAGE : ", L"伤害 : " }, { L"MOVE SPEED : ", L"移动速度 : " },
    { L"HP : %d (BASE)", L"生命 : %d (基础)" }, { L"KILLS : %ls", L"击杀 : %ls" },
    { L"KILLS : %ls / %ls", L"击杀 : %ls / %ls" },
    // Rarity
    { L"COMMON", L"普通" }, { L"RARE", L"稀有" }, { L"EPIC", L"史诗" }, { L"DEBUFF", L"负面" },
    { L"SPECIAL", L"特殊" }, { L"COMBO", L"组合" }, { L"MYTHIC", L"神话" },
    // Single-play panel
    { L"DIFFICULTY", L"难度" }, { L"CLASS", L"职业" }, { L"EASY", L"简单" }, { L"NORMAL", L"普通" },
    { L"HARD", L"困难" }, { L"NIGHTMARE", L"噩梦" }, { L"NO RECORD", L"无记录" }, { L"BEST", L"最佳" },
    { L"DAMAGE", L"伤害" }, { L"FIRE RATE", L"射速" }, { L"BULLET SPEED", L"弹速" }, { L"SPREAD", L"散布" },
    { L"RANGE", L"射程" }, { L"SCREEN", L"全屏" }, { L"DPS", L"每秒伤害" }, { L"RIFLE", L"步枪" },
    { L"FIELD", L"力场" }, { L"PULSE DAMAGE", L"脉冲伤害" }, { L"PULSE DPS", L"脉冲每秒伤害" },
    { L"PULSES / SEC", L"脉冲 / 秒" }, { L"RADIUS", L"半径" }, { L"COVERAGE", L"覆盖" },
    { L"TRAVEL SPEED", L"飞行速度" }, { L"INSTANT", L"瞬发" }, { L"START", L"开始" }, { L"PLAY", L"游戏" },
    { L"Start Score", L"起始分数" }, { L"CREATIVE", L"创造" }, { L"%ls/s", L"%ls/秒" },
    // Results
    { L"VICTORY", L"胜利" }, { L"RESTART", L"重新开始" }, { L"PLAY STATS", L"游戏统计" },
    { L"PLAYER STATS", L"玩家属性" }, { L"ATTACK", L"攻击力" }, { L"MOVE SPEED", L"移动速度" },
    { L"HP", L"生命" }, { L"HP REGEN", L"生命恢复" }, { L"CRIT CHANCE", L"暴击率" },
    { L"CRIT DAMAGE", L"暴击伤害" }, { L"DMG REDUCTION", L"减伤" }, { L"DEATH CAUSE", L"死亡原因" },
    { L"DEBUFFS", L"负面效果" }, { L"WAVE REACHED", L"到达波次" }, { L"STARDUST GAINED", L"获得星尘" },
    { L"LEVEL", L"等级" },
    // Augment screens
    { L"AUG SELECT", L"选择增强" }, { L"DEBUFF SELECT", L"选择负面效果" }, { L"BUFF AUGMENT", L"增益增强" },
    { L"STAT DELTA", L"属性变化" }, { L"NO DIRECT STAT CHANGE", L"无直接属性变化" },
    { L"ENEMY MODIFIER", L"敌人修正" }, { L"MECHANIC", L"机制" }, { L"MOTION", L"移动" },
    { L"UTILITY", L"辅助" }, { L"ORBITAL", L"环绕" }, { L"WARD", L"守护" }, { L"STRIKE", L"打击" },
    { L"DUAL", L"双重" }, { L"FRACTURED", L"碎裂" }, { L"MAX HP", L"最大生命" }, { L"BULLET SPD", L"弹速" },
    { L"MOVE", L"移动" }, { L"IDENTITY SLOT // REPLACE", L"识别槽位 // 替换" },
    { L"1 / 2 / 3 FOCUS   CLICK / SPACE CONFIRM", L"1 / 2 / 3 选择   点击 / 空格 确认" },
    { L"SELECT A CONSTELLATION OR PRESS 1 / 2 / 3", L"选择一个星座或按 1 / 2 / 3" },
    { L"1-9 SELECT   SPACE REPLACE   ESC CANCEL", L"1-9 选择   空格 替换   ESC 取消" },
    { L"Weapon:", L"武器：" },
    // Tutorial headings
    { L"Tutorial", L"教程" }, { L"OBJECTIVES", L"目标" }, { L"PRACTICE FIELD", L"练习场" },
    { L"STEP", L"步骤" }, { L"Move", L"移动" }, { L"Shoot", L"射击" }, { L"Dash", L"冲刺" },
    { L"Defeat Swarms", L"击败蜂群" }, { L"Contents", L"目录" }, { L"< Prev", L"< 上一页" },
    { L"Next >", L"下一页 >" }, { L"ASTRAL FIELD GUIDE", L"星界指南" },
    // Global strings (Translations.h)
    { L"Press Space to Start", L"按空格键开始" }, { L"ESC  Quit", L"ESC  退出" }, { L"Paused", L"暂停" },
    { L"Space  Resume    /    ESC  Quit", L"空格  继续    /    ESC  退出" },
    { L"Game Over", L"游戏结束" }, { L"Restart", L"重新开始" }, { L"ESC  Restart", L"ESC  重新开始" },
    { L"Choose an Augment", L"选择一个增强" }, { L"Choose a Debuff  (unavoidable)", L"选择一个负面效果（无法避免）" },
    { L"Owned Augments", L"已有增强" }, { L"Score", L"分数" }, { L"FPS", L"帧率" }, { L"Lv.", L"Lv." },
    { L"Final Score", L"最终分数" }, { L"Kills", L"击杀" }, { L"Highest Level", L"最高等级" },
    { L"Start", L"开始" }, { L"Settings", L"设置" }, { L"Shop", L"商店" }, { L"Quit", L"退出" },
    { L"Resume", L"继续" }, { L"Main Menu", L"主菜单" }, { L"Language", L"语言" },
    { L"FPS Cap", L"帧率上限" }, { L"Font Size", L"字体大小" }, { L"Display", L"显示" },
    { L"Unlimited", L"无限制" }, { L"Crosshair", L"准星" }, { L"Damage Numbers", L"伤害数字" },
    { L"Combo Counter", L"连击计数" }, { L"Sound", L"声音" }, { L"Press Space to confirm", L"按空格键确认" },
    { L"* Creative  [ON]", L"* 创造模式  [开]" }, { L"- Creative  [OFF]", L"- 创造模式  [关]" },
    { L"Start at 100k + F-key augments", L"10万分起始 + F键增强" },
    // HUD
    { L"LOW HP", L"生命过低" },
    // Achievements (names)
    { L"Chain Massacre", L"连锁屠杀" }, { L"High Scorer", L"高分者" }, { L"Millionaire", L"百万富翁" },
    { L"Regular", L"常客" }, { L"Risk Taker", L"冒险家" }, { L"Slaughterer", L"屠戮者" },
    { L"Vital Strike", L"致命一击" }, { L"Wanderer", L"漫游者" }, { L"Veteran", L"老兵" },
};

inline constexpr int kEntryCount = (int)(sizeof(kEntries) / sizeof(kEntries[0]));

inline uint32_t Hash(const wchar_t* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) { h ^= (uint32_t)*s; h *= 16777619u; }
    return h;
}

// Open-addressing index over kEntries (built once).
inline const wchar_t* Lookup(const wchar_t* en) {
    constexpr int kSlots = 2048;                   // power of two, > 2 x entries
    static short slots[kSlots];
    static bool built = false;
    if (!built) {
        for (short& s : slots) s = -1;
        for (int i = 0; i < kEntryCount; ++i) {
            uint32_t h = Hash(kEntries[i].en) & (kSlots - 1);
            while (slots[h] >= 0) h = (h + 1) & (kSlots - 1);
            slots[h] = (short)i;
        }
        built = true;
    }
    uint32_t h = Hash(en) & (kSlots - 1);
    for (int probe = 0; probe < kSlots && slots[h] >= 0; ++probe) {
        if (std::wcscmp(kEntries[slots[h]].en, en) == 0) return kEntries[slots[h]].zh;
        h = (h + 1) & (kSlots - 1);
    }
    return nullptr;
}

} // namespace zh

// The Chinese text for an English UI string, or the string itself when there is none.
inline const wchar_t* ZhTranslate(const wchar_t* en) {
    if (!en || !en[0]) return en;
    const wchar_t* z = zh::Lookup(en);
    return z ? z : en;
}
