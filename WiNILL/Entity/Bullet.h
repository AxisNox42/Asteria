#pragma once
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <cmath>

// Behaviour flags of a shot fired by a synergy card (Bullet::synFlags).
namespace synf {
inline constexpr uint32_t Mark = 1u << 0, Burn = 1u << 1, Grow = 1u << 2, Hover = 1u << 3,
    Return = 1u << 4, Returning = 1u << 5, Trail = 1u << 6, Gravity = 1u << 7, Swap = 1u << 8,
    Absorb = 1u << 9, Erase = 1u << 10, Lob = 1u << 11, Mine = 1u << 12, Orbit = 1u << 13,
    Seek = 1u << 14, Slowed = 1u << 15, Hovering = 1u << 16, Hovered = 1u << 17;
}
inline constexpr uint32_t kSynNoHit = synf::Lob | synf::Mine;   // a shell or a mine does not hit on the way

class Bullet {
public:
    float x, y;
    float dirX, dirY;
    float speed = 1200.0f;
    bool active = true;
    bool isEnemy = false;

    // 유도탄 — 적 탄환은 플레이어 쪽, 플레이어 탄환은 가장 가까운 적 쪽으로 방향 보정
    bool  homing       = false;
    float homingTurn   = 5.0f;   // 라디안/초
    float homingTurnStart = 0.0f; // 0이면 homingTurn을 고정값으로 사용
    float homingTurnRampStart = 0.0f;
    float homingDuration = 0.0f; // 0이면 기존처럼 수명 제한 없이 유도
    float homingElapsed  = 0.0f;
    // 개별 데미지 배율
    float dmgMult      = 1.0f;
    // 적 탄환과 충돌해도 유지되는 관통 잔량
    float pierceRemaining = 0.0f;
    // 사거리 제한 발사체. 0이면 무제한.
    float maxRange  = 0.0f;
    float traveled  = 0.0f;
    // 렌더 크기 배율
    float sizeScale = 1.0f;
    // 적 총알 데미지 (>0 이면 플레이어 피격 시 이 값. 0 = 기본 10). 반사 총알용.
    float enemyDmg = 0.0f;
    // 플레이어 탄으로 지울 수 있는 적 탄환. 일부 보스 패턴 전용.
    bool  shootableEnemy = false;
    float shootableHp    = 1.0f;
    // 직전 프레임 위치 — 스윕(레이캐스트) 충돌 판정용
    float prevX = 0.0f, prevY = 0.0f;
    // 발사 가속(SAM 미사일 느낌) — launchRamp<1 이면 느리게 출발해 점점 빨라짐.
    //   launchAccel(초당 증가)이 0 이면 비활성(기존 동작). launchRamp 1.0 으로 수렴.
    float launchRamp  = 1.0f;
    float launchAccel = 0.0f;
    // Passive augments: a piercing shot keeps flying after a hit, a blast
    // shot explodes where it lands, and a grazed enemy shot paid its graze.
    bool  piercing   = false;
    bool  blastOnHit = false;
    bool  grazed     = false;
    // Active-skill shot: skill damage multipliers instead of the gun's, no
    // gun procs. skillTag says which skill (ActiveSkills.cpp).
    bool  skillShot  = false;
    bool  forceCrit  = false;
    int   skillTag   = 0;
    // Gun verbs (Docs/Design/Augment_Synergy_Redesign.md, 동사): a shot
    // that survives pierceLeft hits, turns back at the arena edge bouncesLeft
    // times, splits into splitCount children on its first hit, and bursts for
    // blastPct of the attack where it lands. Children inherit the rest.
    int   pierceLeft   = 0;
    int   bouncesLeft  = 0;
    int   splitCount   = 0;
    float splitDmg     = 0.4f;
    float blastPct     = 0.0f;
    mutable bool splitSpent = false;   // set by the hit hook (it only sees a const shot)
    // Synergy cards (SynergyRuntime.cpp): behaviour flags and the little state they need.
    uint32_t synFlags = 0;
    // Each flag keeps its own state: Grow, Hover, Trail and Orbit can sit on the same shot,
    // and a shared field let Hover's stored speed become Grow's base size.
    float synGrowBase  = 0.0f;         // Grow: sizeScale when fired
    float synTrailAt   = 0.0f;         // Trail: distance of the last dropped zone
    float synHoverSpeed = 0.0f;        // Hover: speed to restore after the stop
    float synHoverLeft = 0.0f;         // Hover: seconds of stop left
    float synOrbitAngle = 0.0f;        // Orbit: angle around the player
    float synOrbitLeft = 0.0f;         // Orbit: seconds left
    float synTx = 0.0f, synTy = 0.0f;  // a fixed point: where a mortar shell lands, where a mine sits
    int   synBounces = 0;              // arena-edge bounces so far
    int   synRicochet = 0;             // enemy-to-enemy bounces left
    int   synPierced = 0;              // enemies passed so far
    // Targets a piercing shot already hit, so it hits each one once.
    static constexpr int kHitMemory = 8;
    const void* recentHits[kHitMemory] = {};
    int recentHitNext = 0;
    bool HasHit(const void* target) const {
        for (const void* hit : recentHits)
            if (hit == target) return true;
        return false;
    }
    void ClearHits() {
        for (const void*& hit : recentHits) hit = nullptr;
        recentHitNext = 0;
    }
    // Called when a hit lands: true if the shot flies on (and remembers the
    // target), false if it ends here.
    bool SurvivesHit() {
        if (piercing) return true;
        if (pierceLeft > 0) { --pierceLeft; return true; }
        return false;
    }
    // Turns the shot back at the edge of the arena rectangle; true if it did.
    bool BounceInRect(float minX, float maxX, float minY, float maxY) {
        if (bouncesLeft <= 0) return false;
        bool bounced = false;
        if (x < minX)      { x = minX + (minX - x); dirX = std::fabs(dirX);  bounced = true; }
        else if (x > maxX) { x = maxX - (x - maxX); dirX = -std::fabs(dirX); bounced = true; }
        if (y < minY)      { y = minY + (minY - y); dirY = std::fabs(dirY);  bounced = true; }
        else if (y > maxY) { y = maxY - (y - maxY); dirY = -std::fabs(dirY); bounced = true; }
        if (bounced) {
            x = std::min(std::max(x, minX), maxX);   // a long step must not carry it out the other side
            y = std::min(std::max(y, minY), maxY);
            --bouncesLeft; ClearHits(); prevX = x; prevY = y;
        }
        return bounced;
    }
    void RememberHit(const void* target) {
        recentHits[recentHitNext] = target;
        recentHitNext = (recentHitNext + 1) % kHitMemory;
    }
    // Magnetar whose broken ward face this shot already passed through.
    const void* wardPassed = nullptr;
    bool  crescentBlade = false; // 초승달 검기 — arc 모양 렌더
    // NORMA window space the shot was fired in (SpaceRules.h); -1 until its
    // first update. It dissolves on reaching any other space.
    int   space = -1;
    // space::g_Clock when the shot was first updated; panes opened later do not stop it.
    float firedAt = 0.0f;
    // NORMA's shot: a small square with a short tail (placeholder look).
    bool  normaShot = false;
    // Stopped by every pane it flies into, even one opened after it was fired
    // (a blown pane's burst meeting the panes Detonate reopens).
    bool  anyPaneStops = false;

    glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f);

    Bullet(float startX, float startY, float targetX, float targetY) {
        x = startX; y = startY;
        prevX = startX; prevY = startY;
        float dx = targetX - startX;
        float dy = targetY - startY;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.0f) { dirX = dx / dist; dirY = dy / dist; }
        else              { dirX = 0.0f;      dirY = -1.0f;     }
    }

    void Update(float dt) {
        prevX = x;  prevY = y;          // 이동 전 위치 저장 (스윕 판정)
        if (launchAccel > 0.0f && launchRamp < 1.0f) {   // SAM 미사일식 발사 가속
            launchRamp += launchAccel * dt;
            if (launchRamp > 1.0f) launchRamp = 1.0f;
        }
        float step = speed * launchRamp * dt;
        x += dirX * step;
        y += dirY * step;
        traveled += step;   // distance flown (growing, hovering and returning shots read it)
        if (maxRange > 0.0f && traveled > maxRange) active = false;
    }
};

// Upper limit of a player shot's sizeScale. Size boosts (warhead, sniper,
// crossbow, growth, ...) multiply together; without a cap a stack covers the
// screen. Skill shots and enemy bullets keep their own sizes.
inline constexpr float kPlayerBulletMaxSizeScale = 4.0f;

inline void ClampPlayerBulletSize(Bullet& bullet) {
    if (bullet.isEnemy || bullet.skillShot) return;
    if (bullet.sizeScale > kPlayerBulletMaxSizeScale) bullet.sizeScale = kPlayerBulletMaxSizeScale;
}

struct BulletHaloStyle {
    float innerRadius;
    float innerAlpha;
    float outerRadius;
    float outerAlpha;
};

inline BulletHaloStyle GetBulletHaloStyle(const Bullet& bullet) {
    const float r = 4.8f * bullet.sizeScale;
    float innerRadius = r * 3.1f;
    float innerAlpha = 0.20f;
    if (bullet.crescentBlade) {
        const float grow = std::min(3.8f, 1.0f + bullet.traveled / 160.0f);
        innerRadius = r * 4.0f * grow + 20.0f;
        innerAlpha = 0.10f;
    }
    return { innerRadius, innerAlpha, innerRadius * 2.0f, innerAlpha * 0.35f };
}
