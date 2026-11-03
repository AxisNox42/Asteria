// ─────────────────────────────────────────────────────────────
//  Audio — miniaudio 구현부 (이 .cpp 에서만 IMPLEMENTATION 정의)
// ─────────────────────────────────────────────────────────────
#define _CRT_SECURE_NO_WARNINGS
#define MINIAUDIO_IMPLEMENTATION
// 안 쓰는 백엔드/기능 비활성 → 컴파일 가볍게
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "miniaudio.h"
#include "Audio.h"
#include "EmbeddedResource.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>
#ifdef _WIN32
#  include <windows.h>
#endif

namespace {
    ma_engine g_engine;
    bool g_inited  = false;
    bool g_enabled = true;
    bool g_bgmEnabled = true;
    bool g_sfxEnabled = true;
    bool g_monoOutput = false;
    float g_masterVolume = 1.0f;
    float g_bgmScale = 1.0f;
    float g_sfxScale = 1.0f;

    std::string g_base;
    void ResolveBase() {
        // PlatformChdirToExeDir() 이후 상대경로(Resource/...) 사용
        g_base = "";
    }
    std::string FullPath(const char* rel) { return g_base + rel; }

    unsigned long long NowMs() {
#ifdef _WIN32
        return GetTickCount64();
#else
        using clock = std::chrono::steady_clock;
        return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count();
#endif
    }

    // ── 원본 파일 (Sfx) ──
    // 한 번 PCM 으로 디코드해 두고, 같은 PCM 을 가리키는 보이스 여러 개로
    // 겹쳐 재생한다. 그래서 같은 소리를 다른 피치로 동시에 낼 수 있다.
    constexpr int MAX_VOICES = 8;
    struct Voice {
        ma_audio_buffer_ref ref;
        ma_sound snd;
        bool ok = false;
    };
    struct Clip {
        void* pcm = nullptr;
        Voice voices[MAX_VOICES];
        int nVoices = 0;
        int next = 0;
        bool ok = false;
    };
    Clip g_clips[(int)Audio::Sfx::COUNT];

    struct SfxDef { const char* path; const char* resName; float vol; int voices; };
    // 원본 테이블. 효과음은 Freesound CC0 후보 499개 중 서로 비슷한 느낌으로 고른 16개를
    // scripts/build_sfx.py 로 다듬은 것이고
    // 역할별 라우드니스까지 파일에 맞춰 두었으므로 게인은 모두 1.0 — 상대 크기는 Cue 의 vol 로만 조절.
    const SfxDef SFX_DEFS[(int)Audio::Sfx::COUNT] = {
        { "Resource/Audio/sfx_shoot.wav",           "AUDIO_SFX_SHOOT",     1.0f, 8 },
        { "Resource/Audio/sfx_kill.wav",            "AUDIO_SFX_KILL",      1.0f, 6 },
        { "Resource/Audio/sfx_hurt.wav",            "AUDIO_SFX_HURT",      1.0f, 2 },
        { "Resource/Audio/sfx_death.wav",           "AUDIO_SFX_DEATH",     1.0f, 1 },
        { "Resource/Audio/ui_hover.wav",            "AUDIO_UI_HOVER",      1.0f, 3 },
        { "Resource/Audio/ui_confirm.wav",          "AUDIO_UI_CONFIRM",    1.0f, 3 },
        { "Resource/Audio/ui_deny.wav",             "AUDIO_UI_DENY",       1.0f, 2 },
        { "Resource/Audio/sfx_levelup.wav",         "AUDIO_SFX_LEVELUP",   1.0f, 3 },
        { "Resource/Audio/aug_flip.wav",            "AUDIO_AUG_FLIP",      1.0f, 6 },
        { "Resource/Audio/sfx_dash.wav",            "AUDIO_SFX_DASH",      1.0f, 3 },
        { "Resource/Audio/enemy_hit.wav",           "AUDIO_ENEMY_HIT",     1.0f, 8 },
        { "Resource/Audio/sfx_explode_m.wav",       "AUDIO_SFX_EXPLODE_M", 1.0f, 6 },
        { "Resource/Audio/sfx_chain_zap.wav",       "AUDIO_SFX_ZAP",       1.0f, 6 },
        { "Resource/Audio/sfx_pickup_stardust.wav", "AUDIO_SFX_PICKUP",    1.0f, 6 },
        { "Resource/Audio/skill_cast.wav",          "AUDIO_SKILL_CAST",    1.0f, 4 },
        { "Resource/Audio/ui_back.wav",             "AUDIO_UI_BACK",       1.0f, 3 },
    };

    // ── 게임 이벤트 (Cue) ──
    //   step   : 원본 1개를 반음(semi)·볼륨·지연(delayMs)·컷(cutMs)으로 바꿔 재생
    //   randSemi: 매번 ±반음 랜덤 — 사격·피격·처치처럼 자주 반복되는 소리가 기관총처럼 똑같이 들리지 않게
    //   semi 는 작게: 원본이 이미 무게 처리로 낮춰져 있어, 크게 내리면 둔하게 뭉갠다
    //   크기 ↔ 음높이 (Parise & Spence 2009): 작은 적 처치는 원음, 엘리트·보스는 낮게
    //   minMs  : 최소 재생 간격(ms). 도배 방지 (연사/대량처치)
    using S = Audio::Sfx;
    struct Step { S sfx; float semi; float vol; unsigned delayMs; unsigned cutMs; };
    struct CueDef { Step steps[4]; int n; float randSemi; unsigned minMs; };
    const CueDef CUE_DEFS[(int)Audio::Cue::COUNT] = {
        /* UiHover    */ {{{S::UiHover,    0, 1.0f, 0, 0}}, 1, 0.0f, 40},
        /* UiConfirm  */ {{{S::UiConfirm,  0, 1.0f, 0, 0}}, 1, 0.0f, 60},
        /* UiBack     */ {{{S::UiBack,     0, 1.0f, 0, 0}}, 1, 0.0f, 60},
        /* UiDeny     */ {{{S::UiDeny,     0, 1.0f, 0, 0}}, 1, 0.0f, 120},
        /* Pause      */ {{{S::SkillCast, -3, 0.6f, 0, 0}}, 1, 0.0f, 100},
        /* Resume     */ {{{S::SkillCast,  2, 0.6f, 0, 0}}, 1, 0.0f, 100},
        /* Shoot      */ {{{S::Shoot,      0, 1.0f, 0, 0}}, 1, 1.0f, 25},
        /* FieldPulse */ {{{S::Zap,        4, 0.35f, 0, 100}}, 1, 1.0f, 200},
        /* Dash       */ {{{S::Dash,       0, 1.0f, 0, 0}}, 1, 0.0f, 60},
        /* Hurt       */ {{{S::Hurt,       0, 1.0f, 0, 0}}, 1, 0.0f, 150},
        /* Death      */ {{{S::Death,      0, 1.0f, 0, 0}}, 1, 0.0f, 0},
        /* Pickup     */ {{{S::Pickup,     0, 1.0f, 0, 0}}, 1, 0.0f, 35},
        /* LevelUp    */ {{{S::LevelUp,    0, 1.0f, 0, 0}}, 1, 0.0f, 100},
        /* ShopBuy    */ {{{S::LevelUp,    7, 0.7f, 0, 300}, {S::Pickup, 7, 0.5f, 60, 0},
                           {S::Pickup,     5, 0.5f, 120, 0}, {S::Pickup, 0, 0.5f, 180, 0}}, 4, 0.0f, 100},
        /* CardDeal   */ {{{S::CardFlip,   7, 0.6f, 0, 0}}, 1, 0.0f, 60},
        /* CardFlip   */ {{{S::CardFlip,   0, 1.0f, 0, 0}}, 1, 0.0f, 70},
        /* CardPick   */ {{{S::SkillCast,  7, 0.8f, 0, 0}, {S::Dash, 3, 0.5f, 120, 0},
                           {S::SkillCast, 12, 0.6f, 450, 0}}, 3, 0.0f, 200},
        /* WaveStart  */ {{{S::SkillCast, -3, 1.0f, 0, 0}}, 1, 0.0f, 300},
        /* WaveClear  */ {{{S::LevelUp,   -2, 0.8f, 0, 0}, {S::SkillCast, 0, 0.6f, 0, 0}}, 2, 0.0f, 300},
        /* EnemyHit   */ {{{S::EnemyHit,   0, 0.75f, 0, 0}}, 1, 0.0f, 30},  // 피치 랜덤 없음(낮아지면 어색함), 약 -2.5 dB (요청: 살짝 작게)
        /* Kill       */ {{{S::EnemyHit,   0, 0.75f, 0, 0}}, 1, 0.0f, 55},  // 처치음 = 로터 피격음 (요청)
        /* EliteKill  */ {{{S::Explosion, -2, 1.0f, 0, 0}, {S::LevelUp, -5, 0.4f, 0, 0}}, 2, 0.0f, 200},
        /* BossDeath  */ {{{S::Explosion, -2, 1.0f, 0, 0}, {S::Explosion, -3, 1.0f, 400, 0},
                           {S::Explosion, -5, 1.0f, 900, 0}}, 3, 0.0f, 1000},
        /* SkillCast  */ {{{S::SkillCast,  0, 1.0f, 0, 0}}, 1, 0.0f, 50},
        /* ExplodeS   */ {{{S::Kill,      -2, 0.9f, 0, 0}}, 1, 0.0f, 50},
        /* ExplodeM   */ {{{S::Explosion,  0, 0.8f, 0, 0}}, 1, 1.0f, 80},
        /* ExplodeL   */ {{{S::Explosion, -3, 1.0f, 0, 0}, {S::Explosion, 0, 0.6f, 0, 0}}, 2, 0.0f, 150},
        /* Lightning  */ {{{S::Zap,       -3, 0.9f, 0, 0}, {S::Explosion, 5, 0.5f, 0, 0}}, 2, 1.0f, 70},
    };
    unsigned long long g_cueLastMs[(int)Audio::Cue::COUNT] = {0};

    // 별가루 연속 획득: 1초 안에 이어지면 5음 음계로 한 칸씩 (E6 기준 E F# A B D E …, 최대 한 옥타브 반)
    constexpr int PICKUP_LADDER[] = { 0, 2, 5, 7, 10, 12, 14, 17, 19 };
    constexpr int PICKUP_STEPS = (int)(sizeof(PICKUP_LADDER) / sizeof(PICKUP_LADDER[0]));
    constexpr unsigned PICKUP_CHAIN_MS = 1000;
    unsigned long long g_pickupLastMs = 0;
    int g_pickupStep = 0;

    ma_sound g_bgm;
    ma_decoder g_bgmDecoder;
    bool     g_bgmDecoderActive = false;
    bool     g_bgmActive = false;
    char     g_bgmPath[64] = "";

    void ReleaseClip(Clip& c) {
        for (int v = 0; v < c.nVoices; ++v) {
            if (c.voices[v].ok) {
                ma_sound_uninit(&c.voices[v].snd);
                ma_audio_buffer_ref_uninit(&c.voices[v].ref);
                c.voices[v].ok = false;
            }
        }
        if (c.pcm) { ma_free(c.pcm, nullptr); c.pcm = nullptr; }
        c.nVoices = 0;
        c.next = 0;
        c.ok = false;
    }

    void ReleaseSoundObjects() {
        if (g_bgmActive) { ma_sound_uninit(&g_bgm); g_bgmActive = false; }
        if (g_bgmDecoderActive) {
            ma_decoder_uninit(&g_bgmDecoder);
            g_bgmDecoderActive = false;
        }
        g_bgmPath[0] = 0;
        for (int i = 0; i < (int)Audio::Sfx::COUNT; ++i) ReleaseClip(g_clips[i]);
    }

    bool InitializeEngine(bool mono) {
        ma_engine_config config = ma_engine_config_init();
        config.channels = mono ? 1 : 2;
        return ma_engine_init(&config, &g_engine) == MA_SUCCESS;
    }

    void LoadClip(int i) {
        Clip& c = g_clips[i];
        const SfxDef& def = SFX_DEFS[i];
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_uint64 frames = 0;
        void* pcm = nullptr;
        // exe 내장 리소스가 우선, 없으면 Resource/ 파일
        EmbeddedResourceView embedded;
        bool decoded = LoadEmbeddedResource(def.resName, embedded) &&
            ma_decode_memory(embedded.data, (size_t)embedded.size,
                             &cfg, &frames, &pcm) == MA_SUCCESS;
        if (!decoded) {
            cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
            std::string full = FullPath(def.path);
            decoded = ma_decode_file(full.c_str(), &cfg, &frames, &pcm) == MA_SUCCESS;
        }
        if (!decoded || !pcm || frames == 0) {
            if (pcm) ma_free(pcm, nullptr);
            return;   // 파일이 없으면 조용히 무시
        }
        c.pcm = pcm;
        // 앞 무음 잘라내기 — 타격음이 늦게 들리지 않게 (enemy_hit 은 44ms)
        const float* f = static_cast<const float*>(pcm);
        ma_uint64 lead = 0;
        while (lead < frames) {
            bool loud = false;
            for (ma_uint32 ch = 0; ch < cfg.channels; ++ch)
                if (std::fabs(f[lead * cfg.channels + ch]) > 0.003f) loud = true;
            if (loud) break;
            ++lead;
        }
        const ma_uint64 backoff = cfg.sampleRate / 500;   // 2ms 여유
        lead = lead > backoff ? lead - backoff : 0;
        if (lead >= frames) lead = 0;
        const float* start = f + lead * cfg.channels;
        frames -= lead;
        c.nVoices = def.voices < MAX_VOICES ? def.voices : MAX_VOICES;
        for (int v = 0; v < c.nVoices; ++v) {
            Voice& vo = c.voices[v];
            if (ma_audio_buffer_ref_init(cfg.format, cfg.channels, start, frames,
                                         &vo.ref) != MA_SUCCESS) continue;
            vo.ref.sampleRate = cfg.sampleRate;
            if (ma_sound_init_from_data_source(&g_engine, &vo.ref,
                    MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &vo.snd) != MA_SUCCESS) {
                ma_audio_buffer_ref_uninit(&vo.ref);
                continue;
            }
            vo.ok = true;
            c.ok = true;
        }
    }

    void LoadSoundEffects() {
        for (int i = 0; i < (int)Audio::Sfx::COUNT; ++i) LoadClip(i);
    }

    Voice* AcquireVoice(Clip& c) {
        for (int k = 0; k < c.nVoices; ++k) {
            int v = (c.next + k) % c.nVoices;
            if (c.voices[v].ok && !ma_sound_is_playing(&c.voices[v].snd)) {
                c.next = (v + 1) % c.nVoices;
                return &c.voices[v];
            }
        }
        // 모두 재생 중이면 가장 많이 재생된 보이스를, 꼬리(75 % 이후)일 때만 재사용.
        // 몸통이 남은 소리를 끊으면 '뚝' 끊기므로 그때는 새 소리를 버린다
        Voice* best = nullptr;
        float bestPos = 0.75f;
        int bestV = -1;
        for (int v = 0; v < c.nVoices; ++v) {
            if (!c.voices[v].ok) continue;
            ma_uint64 cur = 0, len = 0;
            ma_sound_get_cursor_in_pcm_frames(&c.voices[v].snd, &cur);
            ma_sound_get_length_in_pcm_frames(&c.voices[v].snd, &len);
            const float pos = len ? (float)cur / (float)len : 1.0f;
            if (pos >= bestPos) { bestPos = pos; best = &c.voices[v]; bestV = v; }
        }
        if (best) c.next = (bestV + 1) % c.nVoices;
        return best;
    }

    void PlayStep(const Step& st, float extraSemi) {
        Clip& c = g_clips[(int)st.sfx];
        if (!c.ok) return;
        Voice* vo = AcquireVoice(c);
        if (!vo) return;
        ma_sound* snd = &vo->snd;
        ma_sound_stop(snd);
        ma_sound_reset_stop_time_and_fade(snd);
        ma_sound_reset_start_time(snd);
        ma_sound_seek_to_pcm_frame(snd, 0);   // 재트리거 = 처음부터
        // 반음 → 배율 2^(n/12). 피치를 내리면 길이도 같은 비율로 늘어남
        ma_sound_set_pitch(snd, std::pow(2.0f, (st.semi + extraSemi) / 12.0f));
        ma_sound_set_volume(snd, SFX_DEFS[(int)st.sfx].vol * st.vol * g_sfxScale);
        const ma_uint64 now = ma_engine_get_time_in_milliseconds(&g_engine);
        if (st.delayMs > 0)
            ma_sound_set_start_time_in_milliseconds(snd, now + st.delayMs);
        if (st.cutMs > 0)
            ma_sound_set_stop_time_with_fade_in_milliseconds(
                snd, now + st.delayMs + st.cutMs, 30);
        ma_sound_start(snd);
    }

    float RandomSemi(float range) {
        if (range <= 0.0f) return 0.0f;
        const int steps = (int)range;   // 정수 반음: -range..+range
        return steps > 0 ? (float)(std::rand() % (2 * steps + 1) - steps) : 0.0f;
    }

    void StartBgm(const char* path, float vol) {
        if (!g_inited || !g_enabled || !g_bgmEnabled) return;
        if (g_bgmActive && std::strcmp(g_bgmPath, path) == 0) return;  // 이미 같은 곡
        if (g_bgmActive) {
            ma_sound_uninit(&g_bgm);
            g_bgmActive = false;
        }
        if (g_bgmDecoderActive) {
            ma_decoder_uninit(&g_bgmDecoder);
            g_bgmDecoderActive = false;
        }
        g_bgmPath[0] = 0;

        EmbeddedResourceView embedded;
        if (LoadEmbeddedResource("AUDIO_BGM_MAIN", embedded) &&
            ma_decoder_init_memory(embedded.data, (size_t)embedded.size,
                                   nullptr, &g_bgmDecoder) == MA_SUCCESS) {
            g_bgmDecoderActive = true;
            if (ma_sound_init_from_data_source(&g_engine, &g_bgmDecoder,
                                               MA_SOUND_FLAG_STREAM, nullptr,
                                               &g_bgm) == MA_SUCCESS) {
                ma_sound_set_looping(&g_bgm, MA_TRUE);
                ma_sound_set_volume(&g_bgm, vol * g_bgmScale);
                ma_sound_start(&g_bgm);
                g_bgmActive = true;
                std::strncpy(g_bgmPath, path, 63);
                g_bgmPath[63] = 0;
                return;
            }
            ma_decoder_uninit(&g_bgmDecoder);
            g_bgmDecoderActive = false;
        }

        std::string full = FullPath(path);
        if (ma_sound_init_from_file(&g_engine, full.c_str(), MA_SOUND_FLAG_STREAM,
                                    NULL, NULL, &g_bgm) == MA_SUCCESS) {
            ma_sound_set_looping(&g_bgm, MA_TRUE);
            ma_sound_set_volume(&g_bgm, vol * g_bgmScale);
            ma_sound_start(&g_bgm);
            g_bgmActive = true;
            std::strncpy(g_bgmPath, path, 63);
            g_bgmPath[63] = 0;
        }
    }
}

void Audio::Init(bool monoOutput) {
    if (g_inited) return;
    g_monoOutput = monoOutput;
    if (!InitializeEngine(g_monoOutput)) {
        if (!g_monoOutput || !InitializeEngine(false)) return;
        g_monoOutput = false;
    }
    g_inited = true;
    ResolveBase();
    LoadSoundEffects();
}

void Audio::Shutdown() {
    if (!g_inited) return;
    ReleaseSoundObjects();
    ma_engine_uninit(&g_engine);
    g_inited = false;
}

void Audio::Play(Cue cue) {
    if (!g_inited || !g_enabled || !g_sfxEnabled) return;
    int i = (int)cue;
    if (i < 0 || i >= (int)Cue::COUNT) return;
    const CueDef& def = CUE_DEFS[i];
    const unsigned long long now = NowMs();
    // 쓰로틀 — 도배 방지 (연사/대량처치)
    if (def.minMs > 0) {
        if (now - g_cueLastMs[i] < def.minMs) return;
        g_cueLastMs[i] = now;
    }
    float extra = RandomSemi(def.randSemi);
    if (cue == Cue::Pickup) {
        const bool chained = now - g_pickupLastMs < PICKUP_CHAIN_MS;
        g_pickupStep = chained ? (g_pickupStep < PICKUP_STEPS - 1 ? g_pickupStep + 1 : PICKUP_STEPS - 1) : 0;
        g_pickupLastMs = now;
        extra += (float)PICKUP_LADDER[g_pickupStep];
    }
    for (int s = 0; s < def.n; ++s) PlayStep(def.steps[s], extra);
}

void Audio::PlaySfx(Sfx s) {
    if (!g_inited || !g_enabled || !g_sfxEnabled) return;
    int i = (int)s;
    if (i < 0 || i >= (int)Sfx::COUNT) return;
    PlayStep(Step{ s, 0.0f, 1.0f, 0, 0 }, 0.0f);
}

void Audio::PlayBgmMain() { StartBgm("Resource/Audio/bgm_main.mp3", 0.50f); }

void Audio::StopBgm() {
    if (g_bgmActive) { ma_sound_uninit(&g_bgm); g_bgmActive = false; }
    if (g_bgmDecoderActive) {
        ma_decoder_uninit(&g_bgmDecoder);
        g_bgmDecoderActive = false;
    }
    g_bgmPath[0] = 0;
}

void Audio::SetEnabled(bool on) {
    if (g_enabled == on) return;
    g_enabled = on;
    if (!on) {
        StopBgm();
        if (g_inited) ma_engine_stop(&g_engine);
    } else if (g_inited) {
        ma_engine_start(&g_engine);
    }
}
bool Audio::IsEnabled() { return g_enabled; }

void Audio::SetBgmEnabled(bool on) {
    g_bgmEnabled = on;
    if (!on) StopBgm();
}

void Audio::SetSfxEnabled(bool on) {
    if (g_sfxEnabled == on) return;
    g_sfxEnabled = on;
    if (!on && g_inited) {
        for (int i = 0; i < (int)Sfx::COUNT; ++i)
            for (int v = 0; v < g_clips[i].nVoices; ++v)
                if (g_clips[i].voices[v].ok) ma_sound_stop(&g_clips[i].voices[v].snd);
    }
}

bool Audio::SetMonoOutput(bool mono) {
    if (mono == g_monoOutput) return true;
    if (!g_inited) {
        g_monoOutput = mono;
        return true;
    }

    const bool resumeBgm = g_bgmActive;
    char bgmPath[sizeof(g_bgmPath)] = {};
    std::strncpy(bgmPath, g_bgmPath, sizeof(bgmPath) - 1);
    const bool previousMono = g_monoOutput;
    ReleaseSoundObjects();
    ma_engine_uninit(&g_engine);
    g_inited = false;

    if (!InitializeEngine(mono)) {
        if (!InitializeEngine(previousMono)) return false;
        g_monoOutput = previousMono;
    } else {
        g_monoOutput = mono;
    }
    g_inited = true;
    LoadSoundEffects();
    ma_engine_set_volume(&g_engine, g_masterVolume);
    if (resumeBgm && bgmPath[0] && g_enabled && g_bgmEnabled)
        StartBgm(bgmPath, 0.50f);
    if (!g_enabled) ma_engine_stop(&g_engine);
    return g_monoOutput == mono;
}

bool Audio::IsMonoOutput() { return g_monoOutput; }

void Audio::SetBgmVolume(float v) {
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    g_bgmScale = v;
    if (g_inited && g_bgmActive) ma_sound_set_volume(&g_bgm, 0.50f * v);
}

void Audio::SetSfxVolume(float v) {
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    g_sfxScale = v;
}

void Audio::SetVolume(float v) {
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    g_masterVolume = v;
    if (g_inited) ma_engine_set_volume(&g_engine, v);
}
