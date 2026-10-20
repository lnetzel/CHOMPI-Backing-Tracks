/** @file OrganPreset.h
 *  @brief SD-card preset model for CHOMPI-PIPES: one bank of 15 slots,
 *  versioned JSON, host-testable (no Daisy dependencies).
 *
 *  One small file per slot (PIPES/P01.json .. P15.json on the SD card,
 *  ~300 B each). All 15 slots are cached in RAM; load/save/copy/erase act
 *  on the cache instantly and PresetStorage writes dirty slots out from
 *  the main loop, so the audio path never waits for the card.
 *
 *  Field ranges and defaults mirror the OrganEngine setters. Parsing
 *  clamps to those ranges and fills missing keys with the boot defaults,
 *  so older and newer files are tolerated; unknown keys are ignored.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core_json.h"
#include "ParameterSteps.h"

namespace chompi
{

struct OrganPreset
{
    static constexpr int kNumPipes = 4;
    static constexpr int kVersion  = 2;

    /* field ranges (mirror the OrganEngine setters) */
    static constexpr int kMaxLevel       = 32;
    static constexpr int kMaxStep        = 32;
    static constexpr int kMaxEnv         = 32;
    static constexpr int kMaxRotary      = 32;
    static constexpr int kMaxDrive       = 32;
    static constexpr int kMaxReverbMix   = 32;
    static constexpr int kMaxMod         = 32;
    static constexpr int kMaxPercussion  = 1;   /* 0/1 toggle */

    /* boot defaults (mirror OrganEngine) */
    static constexpr int kDefaultLevel       = 16; /* 50% */
    static constexpr int kDefaultCharacter   = 0;  /* clean sine, shaping
                                                    * bypassed */
    static constexpr int kDefaultEnv         = 0;  /* 5 ms */
    static constexpr int kDefaultVibrato     = 0;
    static constexpr int kDefaultVibratoDelay = 0; /* delay off */
    static constexpr int kDefaultRotary      = 0;  /* rotary off */
    static constexpr int kDefaultDrive       = 0;
    static constexpr int kDefaultTone        = 32;
    static constexpr int kDefaultReverbMix   = 0;
    static constexpr int kDefaultReverbDecay = 0;  /* 0.2 s */
    static constexpr int kDefaultMod         = 0;
    static constexpr int kDefaultLength      = 16; /* nominal footage */
    static constexpr int kDefaultPercussion  = 0;  /* percussion off */
    static constexpr int kDefaultDelayMix    = 0;  /* delay dry */
    static constexpr int kDefaultDelayTime   = 15; /* 256 ms */

    uint8_t level[kNumPipes];     /**< 0..32 pipe levels */
    uint8_t character_step[kNumPipes]; /**< 0..32 Character positions */
    uint8_t attack[kNumPipes];    /**< 0..32 attack steps */
    uint8_t release[kNumPipes];   /**< 0..32 release steps */
    uint8_t mute[kNumPipes];      /**< 0/1 pipe mutes */
    uint8_t length_step[kNumPipes]; /**< 0..32 retained pipe-length position */
    uint8_t vibrato_depth_step;   /**< 0..32 positions across 0..50 cents */
    uint8_t vibrato_delay_step;   /**< 0..32 positions across 0..5 s */
    uint8_t rotary;               /**< 0..32 rotary amount */
    uint8_t drive;                /**< 0..32 fuzz amount */
    uint8_t drive_tone_step;      /**< 0..32 positions across 0..100% */
    uint8_t reverb_mix;           /**< 0..32 plate mix steps */
    uint8_t reverb_decay_step;    /**< 0..32 log-spaced plate decay positions */
    uint8_t vol_mod[kNumPipes];   /**< 0..32 volume-modulation amounts */
    uint8_t char_mod[kNumPipes];  /**< 0..32 Character-modulation amounts */
    uint8_t length_mod[kNumPipes]; /**< 0..32 pipe-length modulation amounts */
    uint8_t percussion;           /**< 0/1 percussion toggle */
    uint8_t delay_mix_step;       /**< 0..32 combined mix/feedback positions */
    uint8_t delay_time_step;      /**< 0..32 positions across 40..500 ms */
    uint8_t drive_mod;            /**< 0..32 GLOBAL distortion modulation
                                   *   amount (one value, not per pipe) */
    uint8_t delay_mod;            /**< 0..32 GLOBAL delay modulation
                                   *   amount (one value, not per pipe) */

    void SetDefaults()
    {
        for(int p = 0; p < kNumPipes; p++)
        {
            level[p]     = kDefaultLevel;
            character_step[p] = kDefaultCharacter;
            attack[p]    = kDefaultEnv;
            release[p]   = kDefaultEnv;
            mute[p]      = 0;
            length_step[p] = kDefaultLength;
            vol_mod[p]   = kDefaultMod;
            char_mod[p]  = kDefaultMod;
            length_mod[p] = kDefaultMod;
        }
        vibrato_depth_step = kDefaultVibrato;
        vibrato_delay_step = kDefaultVibratoDelay;
        rotary       = kDefaultRotary;
        drive        = kDefaultDrive;
        drive_tone_step = kDefaultTone;
        reverb_mix   = kDefaultReverbMix;
        reverb_decay_step = kDefaultReverbDecay;
        percussion   = kDefaultPercussion;
        delay_mix_step = kDefaultDelayMix;
        delay_time_step = kDefaultDelayTime;
        drive_mod    = kDefaultMod; /* distortion modulation off */
        delay_mod    = kDefaultMod; /* delay modulation off */
    }

    /** Init patch (SHIFT + OVERDUB held for 4 s): the same sound values
     *  as the boot defaults, applied live. Master volume, fine tune,
     *  octave, latch state, page selections and the wheel mode are NOT
     *  touched (the same things presets don't save). */
    void SetInit() { SetDefaults(); }
};

/** Serialize as compact versioned JSON. Returns the number of bytes
 *  written (excluding the terminator), or 0 when the buffer is too
 *  small (never happens with the 768-byte PresetStorage buffer; the
 *  worst-case document with every field at its widest value is well
 *  under 450 bytes - see test_serialized_size_bound). */
inline size_t Serialize(const OrganPreset &p, char *buf, size_t size)
{
    const int n = snprintf(
        buf,
        size,
        "{\"version\":%d,"
        "\"level\":[%u,%u,%u,%u],"
        "\"character_step\":[%u,%u,%u,%u],"
        "\"attack\":[%u,%u,%u,%u],"
        "\"release\":[%u,%u,%u,%u],"
        "\"mute\":[%u,%u,%u,%u],"
        "\"length_step\":[%u,%u,%u,%u],"
        "\"vibrato_depth_step\":%u,\"vibrato_delay_step\":%u,\"rotary\":%u,\"drive\":%u,"
        "\"drive_tone_step\":%u,\"reverb_mix\":%u,\"reverb_decay_step\":%u,"
        "\"vol_mod\":[%u,%u,%u,%u],"
        "\"char_mod\":[%u,%u,%u,%u],"
        "\"length_mod\":[%u,%u,%u,%u],"
        "\"percussion\":%u,\"delay_mix_step\":%u,\"delay_time_step\":%u,"
        "\"drive_mod\":%u,\"delay_mod\":%u}",
        OrganPreset::kVersion,
        (unsigned)p.level[0], (unsigned)p.level[1], (unsigned)p.level[2],
        (unsigned)p.level[3],
        (unsigned)p.character_step[0], (unsigned)p.character_step[1],
        (unsigned)p.character_step[2], (unsigned)p.character_step[3],
        (unsigned)p.attack[0], (unsigned)p.attack[1], (unsigned)p.attack[2],
        (unsigned)p.attack[3],
        (unsigned)p.release[0], (unsigned)p.release[1],
        (unsigned)p.release[2], (unsigned)p.release[3],
        (unsigned)p.mute[0], (unsigned)p.mute[1], (unsigned)p.mute[2],
        (unsigned)p.mute[3],
        (unsigned)p.length_step[0], (unsigned)p.length_step[1],
        (unsigned)p.length_step[2], (unsigned)p.length_step[3],
        (unsigned)p.vibrato_depth_step,
        (unsigned)p.vibrato_delay_step,
        (unsigned)p.rotary,
        (unsigned)p.drive,
        (unsigned)p.drive_tone_step,
        (unsigned)p.reverb_mix,
        (unsigned)p.reverb_decay_step,
        (unsigned)p.vol_mod[0], (unsigned)p.vol_mod[1],
        (unsigned)p.vol_mod[2], (unsigned)p.vol_mod[3],
        (unsigned)p.char_mod[0], (unsigned)p.char_mod[1],
        (unsigned)p.char_mod[2], (unsigned)p.char_mod[3],
        (unsigned)p.length_mod[0], (unsigned)p.length_mod[1],
        (unsigned)p.length_mod[2], (unsigned)p.length_mod[3],
        (unsigned)p.percussion,
        (unsigned)p.delay_mix_step,
        (unsigned)p.delay_time_step,
        (unsigned)p.drive_mod,
        (unsigned)p.delay_mod);
    if(n < 0 || static_cast<size_t>(n) >= size)
        return 0;
    return static_cast<size_t>(n);
}

/** Read one integer value for `query`, clamped to [lo, hi]. Missing
 *  keys and non-numeric values yield `fallback`. The buffer is non-const
 *  because coreJSON's JSON_Search takes a char* (it never writes). */
inline int QueryInt(char *      buf,
                    size_t      len,
                    const char *query,
                    int         fallback,
                    int         lo,
                    int         hi)
{
    char * value      = nullptr;
    size_t value_len  = 0;
    if(JSON_Search(buf, len, query, strlen(query), &value, &value_len)
       != JSONSuccess)
        return fallback;
    /* the value is not null-terminated: copy it out for strtol */
    char tmp[12];
    if(value_len >= sizeof(tmp))
        value_len = sizeof(tmp) - 1;
    memcpy(tmp, value, value_len);
    tmp[value_len] = '\0';
    char *end = tmp;
    const long x = strtol(tmp, &end, 10);
    if(end == tmp)
        return fallback; /* not a number (true, "str", ...) */
    if(x < lo)
        return lo;
    if(x > hi)
        return hi;
    return static_cast<int>(x);
}

/** Parse v1 legacy or v2 step presets. Invalid JSON and unsupported
 *  versions leave `p` untouched; unknown keys in supported schemas are
 *  ignored. */
inline bool Parse(char *buf, size_t len, OrganPreset &p)
{
    if(JSON_Validate(buf, len) != JSONSuccess)
        return false;

    const int version = QueryInt(buf, len, "version", 1, 0, 255);
    if(version != 1 && version != OrganPreset::kVersion)
        return false;

    OrganPreset out;
    out.SetDefaults();

    char query[24];
    for(int i = 0; i < OrganPreset::kNumPipes; i++)
    {
        snprintf(query, sizeof(query), "level[%d]", i);
        out.level[i] = (uint8_t)QueryInt(buf, len, query,
                                         OrganPreset::kDefaultLevel, 0,
                                         OrganPreset::kMaxLevel);
        if(version == 1)
        {
            snprintf(query, sizeof(query), "character[%d]", i);
            const int old_value = QueryInt(buf, len, query, 0, 0, 64);
            out.character_step[i] = static_cast<uint8_t>(
                ParameterSteps::LegacyPosition(old_value, 0, 64));
        }
        else
        {
            snprintf(query, sizeof(query), "character_step[%d]", i);
            out.character_step[i] = static_cast<uint8_t>(QueryInt(
                buf, len, query, OrganPreset::kDefaultCharacter, 0,
                OrganPreset::kMaxStep));
        }
        snprintf(query, sizeof(query), "attack[%d]", i);
        out.attack[i] = (uint8_t)QueryInt(buf, len, query,
                                          OrganPreset::kDefaultEnv, 0,
                                          OrganPreset::kMaxEnv);
        snprintf(query, sizeof(query), "release[%d]", i);
        out.release[i] = (uint8_t)QueryInt(buf, len, query,
                                           OrganPreset::kDefaultEnv, 0,
                                           OrganPreset::kMaxEnv);
        snprintf(query, sizeof(query), "mute[%d]", i);
        out.mute[i] = (uint8_t)QueryInt(buf, len, query, 0, 0, 1);
        if(version == 1)
        {
            snprintf(query, sizeof(query), "length[%d]", i);
            const int octave = QueryInt(buf, len, query, 0, -2, 2);
            out.length_step[i] = static_cast<uint8_t>((octave + 2) * 8);
        }
        else
        {
            snprintf(query, sizeof(query), "length_step[%d]", i);
            out.length_step[i] = static_cast<uint8_t>(QueryInt(
                buf, len, query, OrganPreset::kDefaultLength, 0,
                OrganPreset::kMaxStep));
        }
        snprintf(query, sizeof(query), "vol_mod[%d]", i);
        out.vol_mod[i] = (uint8_t)QueryInt(buf, len, query,
                                           OrganPreset::kDefaultMod, 0,
                                           OrganPreset::kMaxMod);
        snprintf(query, sizeof(query), "char_mod[%d]", i);
        out.char_mod[i] = (uint8_t)QueryInt(buf, len, query,
                                            OrganPreset::kDefaultMod, 0,
                                            OrganPreset::kMaxMod);
        snprintf(query, sizeof(query), "length_mod[%d]", i);
        out.length_mod[i] = (uint8_t)QueryInt(buf, len, query,
                                              OrganPreset::kDefaultMod, 0,
                                              OrganPreset::kMaxMod);
    }
    if(version == 1)
    {
        out.vibrato_depth_step = static_cast<uint8_t>(ParameterSteps::LegacyPosition(
            QueryInt(buf, len, "vibrato", 0, 0, 50), 0, 50));
        out.vibrato_delay_step = static_cast<uint8_t>(ParameterSteps::LegacyPosition(
            QueryInt(buf, len, "vibrato_delay", 0, 0, 50), 0, 50));
    }
    else
    {
        out.vibrato_depth_step = static_cast<uint8_t>(QueryInt(
            buf, len, "vibrato_depth_step", 0, 0, OrganPreset::kMaxStep));
        out.vibrato_delay_step = static_cast<uint8_t>(QueryInt(
            buf, len, "vibrato_delay_step", 0, 0, OrganPreset::kMaxStep));
    }
    out.rotary  = (uint8_t)QueryInt(buf, len, "rotary",
                                    OrganPreset::kDefaultRotary, 0,
                                    OrganPreset::kMaxRotary);
    out.drive   = (uint8_t)QueryInt(buf, len, "drive",
                                    OrganPreset::kDefaultDrive, 0,
                                    OrganPreset::kMaxDrive);
    out.drive_tone_step = static_cast<uint8_t>(version == 1
        ? ParameterSteps::LegacyPosition(
              QueryInt(buf, len, "drive_tone", 100, 0, 100), 0, 100)
        : QueryInt(buf, len, "drive_tone_step", OrganPreset::kDefaultTone,
                   0, OrganPreset::kMaxStep));
    out.reverb_mix = (uint8_t)QueryInt(buf, len, "reverb_mix",
                                       OrganPreset::kDefaultReverbMix, 0,
                                       OrganPreset::kMaxReverbMix);
    out.reverb_decay_step = static_cast<uint8_t>(version == 1
        ? ParameterSteps::LegacyPosition(
              QueryInt(buf, len, "reverb_decay", 0, 0, 100), 0, 100)
        : QueryInt(buf, len, "reverb_decay_step",
                   OrganPreset::kDefaultReverbDecay, 0,
                   OrganPreset::kMaxStep));
    out.percussion = (uint8_t)QueryInt(buf, len, "percussion",
                                       OrganPreset::kDefaultPercussion, 0,
                                       OrganPreset::kMaxPercussion);
        out.delay_mix_step = static_cast<uint8_t>(version == 1
          ? ParameterSteps::LegacyPosition(
              QueryInt(buf, len, "delay_mix", 0, 0, 100), 0, 100)
          : QueryInt(buf, len, "delay_mix_step", OrganPreset::kDefaultDelayMix,
                 0, OrganPreset::kMaxStep));
        out.delay_time_step = static_cast<uint8_t>(version == 1
          ? ParameterSteps::LegacyDelayPosition(
              QueryInt(buf, len, "delay_time_ms", 250, 40, 500))
          : QueryInt(buf, len, "delay_time_step",
                 OrganPreset::kDefaultDelayTime, 0,
                 OrganPreset::kMaxStep));
    out.drive_mod = (uint8_t)QueryInt(buf, len, "drive_mod",
                                      OrganPreset::kDefaultMod, 0,
                                      OrganPreset::kMaxMod);
    out.delay_mod = (uint8_t)QueryInt(buf, len, "delay_mod",
                                      OrganPreset::kDefaultMod, 0,
                                      OrganPreset::kMaxMod);
    p = out;
    return true;
}

/** Snapshot the engine state into a preset. Templated so host tests can
 *  pass a fake engine with the same getters. */
template <typename Engine>
void CaptureFromEngine(OrganPreset &p, const Engine &e)
{
    for(int i = 0; i < OrganPreset::kNumPipes; i++)
    {
        p.level[i]     = (uint8_t)e.GetPipeStep(i);
        p.character_step[i] = (uint8_t)e.GetPipeCharacterStep(i);
        p.attack[i]    = (uint8_t)e.GetPipeAttackStep(i);
        p.release[i]   = (uint8_t)e.GetPipeReleaseStep(i);
        p.mute[i]      = e.GetPipeMuted(i) ? 1 : 0;
        p.length_step[i] = (uint8_t)e.GetPipeLengthStep(i);
        p.vol_mod[i]   = (uint8_t)e.GetPipeVolModStep(i);
        p.char_mod[i]  = (uint8_t)e.GetPipeCharModStep(i);
        p.length_mod[i] = (uint8_t)e.GetPipeLengthModStep(i);
    }
    p.vibrato_depth_step = (uint8_t)e.GetVibratoDepthStep();
    p.vibrato_delay_step = (uint8_t)e.GetVibratoDelayStep();
    p.rotary       = (uint8_t)e.GetRotaryStep();
    p.drive        = (uint8_t)e.GetDriveStep();
    p.drive_tone_step = (uint8_t)e.GetDriveToneStep();
    p.reverb_mix   = (uint8_t)e.GetReverbMixStep();
    p.reverb_decay_step = (uint8_t)e.GetReverbDecayStep();
    p.percussion   = e.GetPercussionEnabled() ? 1 : 0;
    p.delay_mix_step = (uint8_t)e.GetDelayMixStep();
    p.delay_time_step = (uint8_t)e.GetDelayTimeStep();
    p.drive_mod    = (uint8_t)e.GetDriveModStep();
    p.delay_mod    = (uint8_t)e.GetDelayModStep();
}

/** Push a preset into the engine through the regular setters (all ramped
 *  or phase-continuous, so held and latched notes keep sounding
 *  click-free). The modulation amounts go first so the level/Character
 *  setters derive their final targets with the loaded amounts. */
template <typename Engine>
void ApplyToEngine(const OrganPreset &p, Engine &e)
{
    for(int i = 0; i < OrganPreset::kNumPipes; i++)
    {
        e.SetPipeVolModStep(i, p.vol_mod[i]);
        e.SetPipeCharModStep(i, p.char_mod[i]);
        e.SetPipeLengthModStep(i, p.length_mod[i]);
        e.SetPipeStep(i, p.level[i]);
        e.SetPipeCharacterStep(i, p.character_step[i]);
        e.SetPipeAttackStep(i, p.attack[i]);
        e.SetPipeReleaseStep(i, p.release[i]);
        e.SetPipeMuted(i, p.mute[i] != 0);
        e.SetPipeLengthStep(i, p.length_step[i]);
    }
    e.SetVibratoDepthStep(p.vibrato_depth_step);
    e.SetVibratoDelayStep(p.vibrato_delay_step);
    e.SetRotaryStep(p.rotary);
    e.SetDriveStep(p.drive);
    e.SetDriveToneStep(p.drive_tone_step);
    e.SetReverbMixStep(p.reverb_mix);
    e.SetReverbDecayStep(p.reverb_decay_step);
    e.SetPercussionEnabled(p.percussion != 0);
    e.SetDelayMixStep(p.delay_mix_step);
    e.SetDelayTimeStep(p.delay_time_step);
    e.SetDriveModStep(p.drive_mod);
    e.SetDelayModStep(p.delay_mod);
}

/** The RAM cache of all 15 slots. The audio ISR mutates it (user
 *  gestures); the main-loop storage reads/snapshots it. A dirty bit per
 *  slot marks content that still needs to hit the SD card: set for
 *  save/copy/erase, cleared by the storage once the slot's file write
 *  (or delete) has finished. A dirty but invalid slot means "delete the
 *  file". */
class OrganPresetBank
{
  public:
    static constexpr int kNumSlots = 15;

    void Init()
    {
        for(int i = 0; i < kNumSlots; i++)
        {
            slots_[i].SetDefaults();
            valid_[i] = false;
        }
        dirty_mask_ = 0;
    }

    bool IsValid(int slot) const
    {
        return slot >= 0 && slot < kNumSlots && valid_[slot];
    }

    const OrganPreset &Get(int slot) const { return slots_[slot]; }

    void Save(int slot, const OrganPreset &p)
    {
        if(slot < 0 || slot >= kNumSlots)
            return;
        slots_[slot] = p;
        valid_[slot] = true;
        dirty_mask_ |= Bit(slot);
    }

    void Copy(int src, int dst)
    {
        if(src < 0 || src >= kNumSlots || dst < 0 || dst >= kNumSlots
           || src == dst || !valid_[src])
            return;
        slots_[dst] = slots_[src];
        valid_[dst] = true;
        dirty_mask_ |= Bit(dst);
    }

    void Erase(int slot)
    {
        if(slot < 0 || slot >= kNumSlots)
            return;
        valid_[slot] = false;
        dirty_mask_ |= Bit(slot); /* pending file delete */
    }

    /** Boot/remount load from the card: fills a slot WITHOUT dirtying it
     *  (the card already holds this content). */
    void LoadSlot(int slot, const OrganPreset &p)
    {
        if(slot < 0 || slot >= kNumSlots)
            return;
        slots_[slot] = p;
        valid_[slot] = true;
    }

    int NextDirtySlot() const
    {
        for(int i = 0; i < kNumSlots; i++)
            if(dirty_mask_ & Bit(i))
                return i;
        return -1;
    }

    bool IsDirty(int slot) const
    {
        return slot >= 0 && slot < kNumSlots && (dirty_mask_ & Bit(slot));
    }

    /** Dirty-bit maintenance for the storage: main loop only, with IRQs
     *  off (the ISR's |= is safe on its own - the main loop can never
     *  interrupt it). */
    void MarkDirty(int slot) { dirty_mask_ |= Bit(slot); }
    void ClearDirty(int slot) { dirty_mask_ &= ~Bit(slot); }

  private:
    static constexpr uint32_t Bit(int slot) { return 1u << slot; }

    OrganPreset slots_[kNumSlots];
    bool        valid_[kNumSlots];
    uint32_t    dirty_mask_;
};

} // namespace chompi
