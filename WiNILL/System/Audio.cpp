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

    // ── 효과음 파일 ──
    // 한 번 PCM 으로 디코드해 두고, 같은 PCM 을 가리키는 보이스 여러 개로 겹쳐 재생한다.
    constexpr int MAX_VOICES = 8;
    constexpr int MAX_FILES  = 8;
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

    // ── 게임 이벤트 (Cue) ──
    //   minMs : 최소 재생 간격(ms). 도배 방지 (연사/대량처치)
    //   voices: 파일 하나당 동시 재생 수
    //   duck  : 큰 순간(T1) — 재생될 때 BGM 을 잠깐 낮춘다
    struct CueDef {
        unsigned minMs; int voices; bool duck; bool loop; int nFiles;
        const char* files[MAX_FILES];
    };
    const CueDef CUE_DEFS[(int)Audio::Cue::COUNT] = {
#define CUE(name, minMs, voices, duck, loop, n, ...) { minMs, voices, duck, loop, n, { __VA_ARGS__ } },
#include "AudioCues.inc"
#undef CUE
    };
    Clip g_clips[(int)Audio::Cue::COUNT][MAX_FILES];
    unsigned long long g_cueLastMs[(int)Audio::Cue::COUNT] = {0};
    int g_cueLastFile[(int)Audio::Cue::COUNT];
    ma_sound* g_loopSnd[(int)Audio::Cue::COUNT] = {nullptr};

    const Audio::Cue SFX_TO_CUE[(int)Audio::Sfx::COUNT] = {
        Audio::Cue::Shoot, Audio::Cue::Kill, Audio::Cue::Hurt, Audio::Cue::Death,
        Audio::Cue::UiHover, Audio::Cue::UiConfirm, Audio::Cue::UiDeny, Audio::Cue::LevelUp,
        Audio::Cue::CardFlip, Audio::Cue::Dash, Audio::Cue::EnemyHit, Audio::Cue::ExplodeM,
        Audio::Cue::SfxChainZap, Audio::Cue::Pickup, Audio::Cue::SkillCast, Audio::Cue::UiBack,
    };

    // ── BGM ──
    struct BgmDef { const char* path; bool loop; ma_uint64 loopBeg, loopEnd; };
    const BgmDef BGM_DEFS[(int)Audio::Bgm::COUNT] = {
#define BGM(name, path, loop, beg, end) { path, loop, beg, end },
#include "AudioBgm.inc"
#undef BGM
    };
    // 두 칸을 번갈아 써서 크로스페이드. 나가는 곡은 페이드가 끝난 뒤 Update 에서 정리
    struct BgmSlot {
        ma_decoder dec;
        ma_sound snd;
        bool decOk = false;
        bool sndOk = false;
        Audio::Bgm id = Audio::Bgm::None;
        unsigned long long releaseAt = 0;   // 0 = 재생 중인 곡
    };
    BgmSlot g_bgm[2];
    int g_bgmCur = -1;

    // 덕킹: 큰 순간에 BGM 을 -5 dB 로 50ms 만에 내리고 0.8초에 걸쳐 되돌린다
    constexpr float DUCK_GAIN = 0.5623f;   // -5 dB
    constexpr unsigned DUCK_HOLD_MS = 600;
    constexpr float DUCK_ATTACK_MS = 50.0f;
    constexpr float DUCK_RELEASE_MS = 800.0f;
    unsigned long long g_duckUntil = 0;
    float g_duck = 1.0f;
    unsigned long long g_lastUpdateMs = 0;

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

    void ReleaseSlot(BgmSlot& s) {
        if (s.sndOk) { ma_sound_uninit(&s.snd); s.sndOk = false; }
        if (s.decOk) { ma_decoder_uninit(&s.dec); s.decOk = false; }
        s.id = Audio::Bgm::None;
        s.releaseAt = 0;
    }

    void ReleaseSoundObjects() {
        for (BgmSlot& s : g_bgm) ReleaseSlot(s);
        g_bgmCur = -1;
        for (int i = 0; i < (int)Audio::Cue::COUNT; ++i) {
            g_loopSnd[i] = nullptr;
            for (Clip& c : g_clips[i]) ReleaseClip(c);
        }
    }

    bool InitializeEngine(bool mono) {
        ma_engine_config config = ma_engine_config_init();
        config.channels = mono ? 1 : 2;
        return ma_engine_init(&config, &g_engine) == MA_SUCCESS;
    }

    void LoadClip(Clip& c, const char* path, int voices) {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_uint64 frames = 0;
        void* pcm = nullptr;
        std::string full = FullPath(path);
        if (ma_decode_file(full.c_str(), &cfg, &frames, &pcm) != MA_SUCCESS || !pcm || frames == 0) {
            if (pcm) ma_free(pcm, nullptr);
            return;   // 파일이 없으면 조용히 무시
        }
        c.pcm = pcm;
        // 앞 무음 잘라내기 — 타격음이 늦게 들리지 않게
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
        c.nVoices = voices < MAX_VOICES ? voices : MAX_VOICES;
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
        for (int i = 0; i < (int)Audio::Cue::COUNT; ++i) {
            const CueDef& d = CUE_DEFS[i];
            g_cueLastFile[i] = -1;
            for (int f = 0; f < d.nFiles && f < MAX_FILES; ++f)
                LoadClip(g_clips[i][f], d.files[f], d.loop ? 1 : d.voices);
        }
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

    // 변형이 여러 개면 무작위로, 직전과 같은 파일은 피한다
    Clip* PickClip(int i) {
        const int n = CUE_DEFS[i].nFiles < MAX_FILES ? CUE_DEFS[i].nFiles : MAX_FILES;
        if (n <= 0) return nullptr;
        int f = n > 1 ? std::rand() % n : 0;
        if (n > 1 && f == g_cueLastFile[i]) f = (f + 1) % n;
        for (int k = 0; k < n; ++k) {
            Clip& c = g_clips[i][(f + k) % n];
            if (c.ok) { g_cueLastFile[i] = (f + k) % n; return &c; }
        }
        return nullptr;
    }

    void StartVoice(ma_sound* snd, bool loop) {
        ma_sound_stop(snd);
        ma_sound_reset_stop_time_and_fade(snd);
        ma_sound_reset_start_time(snd);
        ma_sound_seek_to_pcm_frame(snd, 0);   // 재트리거 = 처음부터
        ma_sound_set_looping(snd, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(snd, g_sfxScale);
        ma_sound_start(snd);
    }

    float BgmGain() { return g_bgmScale * g_duck; }

    void ApplyBgmVolume() {
        for (BgmSlot& s : g_bgm)
            if (s.sndOk) ma_sound_set_volume(&s.snd, BgmGain());
    }

    bool OpenSlot(BgmSlot& s, Audio::Bgm id) {
        const BgmDef& d = BGM_DEFS[(int)id];
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        std::string full = FullPath(d.path);
        if (ma_decoder_init_file(full.c_str(), &cfg, &s.dec) == MA_SUCCESS) {
            s.decOk = true;
        } else if (id == Audio::Bgm::Main) {
            // 개발 중 Resource 폴더 없이 실행할 때를 위한 예비 (예전 내장 곡)
            EmbeddedResourceView embedded;
            if (LoadEmbeddedResource("AUDIO_BGM_MAIN", embedded) &&
                ma_decoder_init_memory(embedded.data, (size_t)embedded.size, &cfg, &s.dec) == MA_SUCCESS)
                s.decOk = true;
        }
        if (!s.decOk) return false;
        if (d.loop && d.loopEnd > d.loopBeg) {
            // MP3 앞 지연 구간을 빼고 정확한 마디로 반복
            ma_data_source_set_loop_point_in_pcm_frames(&s.dec, d.loopBeg, d.loopEnd);
            ma_data_source_seek_to_pcm_frame(&s.dec, d.loopBeg);
        }
        if (ma_sound_init_from_data_source(&g_engine, &s.dec, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                           nullptr, &s.snd) != MA_SUCCESS) {
            ma_decoder_uninit(&s.dec);
            s.decOk = false;
            return false;
        }
        s.sndOk = true;
        s.id = id;
        s.releaseAt = 0;
        ma_sound_set_looping(&s.snd, d.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&s.snd, BgmGain());
        return true;
    }

    void FadeOutSlot(BgmSlot& s, unsigned fadeMs) {
        if (!s.sndOk || s.releaseAt) return;
        if (fadeMs == 0) { ReleaseSlot(s); return; }
        ma_sound_set_fade_in_milliseconds(&s.snd, -1.0f, 0.0f, fadeMs);
        s.releaseAt = NowMs() + fadeMs + 50;
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

void Audio::Update() {
    if (!g_inited) return;
    const unsigned long long now = NowMs();
    const float dt = g_lastUpdateMs ? (float)(now - g_lastUpdateMs) : 16.0f;
    g_lastUpdateMs = now;
    // 덕킹: 붙잡는 동안 목표까지 빠르게, 끝나면 천천히 되돌림
    const float target = now < g_duckUntil ? DUCK_GAIN : 1.0f;
    const float tau = target < g_duck ? DUCK_ATTACK_MS : DUCK_RELEASE_MS;
    const float k = 1.0f - std::exp(-(dt > 100.0f ? 100.0f : dt) * 3.0f / tau);
    const float before = g_duck;
    g_duck += (target - g_duck) * k;
    if (std::fabs(g_duck - target) < 0.002f) g_duck = target;
    if (g_duck != before) ApplyBgmVolume();
    // 페이드아웃이 끝난 곡, 끝까지 재생된 징글 정리
    for (int i = 0; i < 2; ++i) {
        BgmSlot& s = g_bgm[i];
        if (!s.sndOk) continue;
        if ((s.releaseAt && now >= s.releaseAt) || ma_sound_at_end(&s.snd)) {
            ReleaseSlot(s);
            if (g_bgmCur == i) g_bgmCur = -1;
        }
    }
}

void Audio::Play(Cue cue) {
    if (!g_inited || !g_enabled || !g_sfxEnabled) return;
    int i = (int)cue;
    if (i < 0 || i >= (int)Cue::COUNT) return;
    const CueDef& def = CUE_DEFS[i];
    if (def.loop) { StartLoop(cue); return; }
    const unsigned long long now = NowMs();
    // 쓰로틀 — 도배 방지 (연사/대량처치)
    if (def.minMs > 0) {
        if (now - g_cueLastMs[i] < def.minMs) return;
        g_cueLastMs[i] = now;
    }
    Clip* c = PickClip(i);
    if (!c) return;
    Voice* vo = AcquireVoice(*c);
    if (!vo) return;
    StartVoice(&vo->snd, false);
    if (def.duck) g_duckUntil = now + DUCK_HOLD_MS;
}

void Audio::PlaySfx(Sfx s) {
    int i = (int)s;
    if (i < 0 || i >= (int)Sfx::COUNT) return;
    Play(SFX_TO_CUE[i]);
}

void Audio::StartLoop(Cue cue) {
    if (!g_inited || !g_enabled || !g_sfxEnabled) return;
    int i = (int)cue;
    if (i < 0 || i >= (int)Cue::COUNT) return;
    if (g_loopSnd[i] && ma_sound_is_playing(g_loopSnd[i])) return;
    Clip* c = PickClip(i);
    if (!c || !c->voices[0].ok) return;
    g_loopSnd[i] = &c->voices[0].snd;
    StartVoice(g_loopSnd[i], true);
    ma_sound_set_fade_in_milliseconds(g_loopSnd[i], 0.0f, 1.0f, 120);
}

void Audio::StopLoop(Cue cue) {
    int i = (int)cue;
    if (!g_inited || i < 0 || i >= (int)Cue::COUNT || !g_loopSnd[i]) return;
    const ma_uint64 now = ma_engine_get_time_in_milliseconds(&g_engine);
    ma_sound_set_stop_time_with_fade_in_milliseconds(g_loopSnd[i], now, 150);
    g_loopSnd[i] = nullptr;
}

void Audio::PlayBgm(Bgm b, unsigned fadeMs) {
    if (!g_inited || !g_enabled || !g_bgmEnabled) return;
    if ((int)b < 0 || (int)b >= (int)Bgm::COUNT) return;
    if (g_bgmCur >= 0 && g_bgm[g_bgmCur].id == b && !g_bgm[g_bgmCur].releaseAt) return;  // 이미 같은 곡
    const int next = g_bgmCur == 0 ? 1 : 0;
    ReleaseSlot(g_bgm[next]);   // 아직 페이드아웃 중이던 옛 곡이 있으면 바로 정리
    if (g_bgmCur >= 0) FadeOutSlot(g_bgm[g_bgmCur], fadeMs);
    g_bgmCur = -1;
    if (!OpenSlot(g_bgm[next], b)) return;
    if (fadeMs > 0) ma_sound_set_fade_in_milliseconds(&g_bgm[next].snd, 0.0f, 1.0f, fadeMs);
    ma_sound_start(&g_bgm[next].snd);
    g_bgmCur = next;
}

Audio::Bgm Audio::CurrentBgm() {
    return g_bgmCur >= 0 ? g_bgm[g_bgmCur].id : Bgm::None;
}

void Audio::PlayBgmMain() { PlayBgm(Bgm::Main); }

void Audio::StopBgm() {
    for (BgmSlot& s : g_bgm) ReleaseSlot(s);
    g_bgmCur = -1;
}

void Audio::FadeOutBgm(unsigned fadeMs) {
    if (g_bgmCur < 0) return;
    FadeOutSlot(g_bgm[g_bgmCur], fadeMs);
    if (!g_bgm[g_bgmCur].sndOk) g_bgmCur = -1;
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
        for (int i = 0; i < (int)Cue::COUNT; ++i) {
            g_loopSnd[i] = nullptr;
            for (Clip& c : g_clips[i])
                for (int v = 0; v < c.nVoices; ++v)
                    if (c.voices[v].ok) ma_sound_stop(&c.voices[v].snd);
        }
    }
}

bool Audio::SetMonoOutput(bool mono) {
    if (mono == g_monoOutput) return true;
    if (!g_inited) {
        g_monoOutput = mono;
        return true;
    }

    const Bgm resume = CurrentBgm();
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
    if (resume != Bgm::None && g_enabled && g_bgmEnabled)
        PlayBgm(resume, 0);
    if (!g_enabled) ma_engine_stop(&g_engine);
    return g_monoOutput == mono;
}

bool Audio::IsMonoOutput() { return g_monoOutput; }

void Audio::SetBgmVolume(float v) {
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    g_bgmScale = v;
    if (g_inited) ApplyBgmVolume();
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
