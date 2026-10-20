/** @file organ_preset_test.cpp
 *  @brief Native tests for OrganPreset.h - the SD-card preset model:
 *  versioned JSON serialize/parse with clamping + defaults, the 15-slot
 *  RAM bank with dirty tracking, and engine capture/apply (against a
 *  fake engine with the OrganEngine API).
 *
 *  Build & run: make -C firmware/chompi-pipes/code/tests test
 */
#include "OrganPreset.h"
#include <cassert>
#include <cstring>
#include <cstdio>

using chompi::OrganPreset;
using chompi::OrganPresetBank;

/** Fake engine mirroring the OrganEngine preset-facing API (with the
 *  same clamping behavior). */
struct FakeEngine
{
    static constexpr int kNumPipes = 4;

    int  level[4];
    int  character_step[4];
    int  attack[4];
    int  release[4];
    bool muted[4];
    int  length_step[4];
    int  vol_mod[4];
    int  char_mod[4];
    int  length_mod[4];
    int  vibrato_depth_step;
    int  vibrato_delay_step;
    int  rotary;
    int  drive;
    int  drive_tone_step;
    int  reverb_mix;
    int  reverb_decay_step;
    bool percussion;
    int  delay_mix_step;
    int  delay_time_step;
    int  drive_mod;
    int  delay_mod;

    static int Clamp(int v, int lo, int hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    void SetDefaults()
    {
        for(int p = 0; p < 4; p++)
        {
            level[p]     = 16;
            character_step[p] = 0;
            attack[p]    = 0;
            release[p]   = 0;
            muted[p]     = false;
            length_step[p] = 16;
            vol_mod[p]   = 0;
            char_mod[p]  = 0;
            length_mod[p] = 0;
        }
        vibrato_depth_step = 0;
        vibrato_delay_step = 0;
        rotary       = 0;
        drive        = 0;
        drive_tone_step = 32;
        reverb_mix   = 0;
        reverb_decay_step = 0;
        percussion   = false;
        delay_mix_step = 0;
        delay_time_step = 15;
        drive_mod    = 0;
        delay_mod    = 0;
    }

    void SetPipeStep(int p, int s) { level[p] = Clamp(s, 0, 32); }
    int  GetPipeStep(int p) const { return level[p]; }
    void SetPipeCharacterStep(int p, int s) { character_step[p] = Clamp(s, 0, 32); }
    int  GetPipeCharacterStep(int p) const { return character_step[p]; }
    void SetPipeAttackStep(int p, int s) { attack[p] = Clamp(s, 0, 32); }
    int  GetPipeAttackStep(int p) const { return attack[p]; }
    void SetPipeReleaseStep(int p, int s) { release[p] = Clamp(s, 0, 32); }
    int  GetPipeReleaseStep(int p) const { return release[p]; }
    void SetPipeMuted(int p, bool m) { muted[p] = m; }
    bool GetPipeMuted(int p) const { return muted[p]; }
    void SetPipeLengthStep(int p, int s) { length_step[p] = Clamp(s, 0, 32); }
    int  GetPipeLengthStep(int p) const { return length_step[p]; }
    void SetPipeVolModStep(int p, int s) { vol_mod[p] = Clamp(s, 0, 32); }
    int  GetPipeVolModStep(int p) const { return vol_mod[p]; }
    void SetPipeCharModStep(int p, int s) { char_mod[p] = Clamp(s, 0, 32); }
    int  GetPipeCharModStep(int p) const { return char_mod[p]; }
    void SetPipeLengthModStep(int p, int s) { length_mod[p] = Clamp(s, 0, 32); }
    int  GetPipeLengthModStep(int p) const { return length_mod[p]; }
    void SetVibratoDepthStep(int s) { vibrato_depth_step = Clamp(s, 0, 32); }
    int  GetVibratoDepthStep() const { return vibrato_depth_step; }
    void SetVibratoDelayStep(int s) { vibrato_delay_step = Clamp(s, 0, 32); }
    int  GetVibratoDelayStep() const { return vibrato_delay_step; }
    void SetRotaryStep(int s) { rotary = Clamp(s, 0, 32); }
    int  GetRotaryStep() const { return rotary; }
    void SetDriveStep(int s) { drive = Clamp(s, 0, 32); }
    int  GetDriveStep() const { return drive; }
    void SetDriveToneStep(int s) { drive_tone_step = Clamp(s, 0, 32); }
    int  GetDriveToneStep() const { return drive_tone_step; }
    void SetReverbMixStep(int s) { reverb_mix = Clamp(s, 0, 32); }
    int  GetReverbMixStep() const { return reverb_mix; }
    void SetReverbDecayStep(int s) { reverb_decay_step = Clamp(s, 0, 32); }
    int  GetReverbDecayStep() const { return reverb_decay_step; }
    void SetPercussionEnabled(bool on) { percussion = on; }
    bool GetPercussionEnabled() const { return percussion; }
    void SetDelayMixStep(int s) { delay_mix_step = Clamp(s, 0, 32); }
    int  GetDelayMixStep() const { return delay_mix_step; }
    void SetDelayTimeStep(int s) { delay_time_step = Clamp(s, 0, 32); }
    int  GetDelayTimeStep() const { return delay_time_step; }
    void SetDriveModStep(int s) { drive_mod = Clamp(s, 0, 32); }
    int  GetDriveModStep() const { return drive_mod; }
    void SetDelayModStep(int s) { delay_mod = Clamp(s, 0, 32); }
    int  GetDelayModStep() const { return delay_mod; }
};

static bool SamePreset(const OrganPreset &a, const OrganPreset &b)
{
    for(int p = 0; p < OrganPreset::kNumPipes; p++)
    {
        if(a.level[p] != b.level[p] || a.character_step[p] != b.character_step[p]
           || a.attack[p] != b.attack[p] || a.release[p] != b.release[p]
           || a.mute[p] != b.mute[p] || a.length_step[p] != b.length_step[p]
           || a.vol_mod[p] != b.vol_mod[p]
           || a.char_mod[p] != b.char_mod[p]
           || a.length_mod[p] != b.length_mod[p])
            return false;
    }
        return a.vibrato_depth_step == b.vibrato_depth_step
            && a.vibrato_delay_step == b.vibrato_delay_step
           && a.rotary == b.rotary
           && a.drive == b.drive && a.drive_tone_step == b.drive_tone_step
           && a.reverb_mix == b.reverb_mix
           && a.reverb_decay_step == b.reverb_decay_step
           && a.percussion == b.percussion
           && a.delay_mix_step == b.delay_mix_step
           && a.delay_time_step == b.delay_time_step
           && a.drive_mod == b.drive_mod && a.delay_mod == b.delay_mod;
}

static OrganPreset MakeAssortedPreset()
{
    OrganPreset p;
    p.SetDefaults();
    p.level[0] = 0;
    p.level[1] = 17;
    p.level[2] = 32;
    p.level[3] = 5;
    p.character_step[0] = 0;
    p.character_step[1] = 32;
    p.character_step[2] = 1;
    p.character_step[3] = 31;
    p.attack[0] = 32;
    p.release[1] = 32;
    p.attack[3]  = 7;
    p.mute[2]    = 1;
    p.length_step[0] = 0;
    p.length_step[1] = 32;
    p.length_step[3] = 8;
    p.vibrato_depth_step = 32;
    p.vibrato_delay_step = 22;
    p.rotary       = 0;
    p.drive        = 32;
    p.drive_tone_step = 0;
    p.reverb_mix   = 32;
    p.reverb_decay_step = 31;
    p.vol_mod[0]   = 32;
    p.char_mod[3]  = 31;
    p.length_mod[1] = 27;
    p.percussion   = 1;
    p.delay_mix_step = 28;
    p.delay_time_step = 32;
    p.drive_mod    = 23;
    p.delay_mod    = 27;
    return p;
}

static void test_roundtrip()
{
    char buf[768];
    const OrganPreset p = MakeAssortedPreset();
    const size_t n = chompi::Serialize(p, buf, sizeof(buf));
    assert(n > 0 && n < sizeof(buf));
    assert(strstr(buf, "\"version\":2") != nullptr);

    OrganPreset q;
    q.SetDefaults();
    assert(chompi::Parse(buf, n, q));
    assert(SamePreset(p, q));
}

static void test_roundtrip_defaults()
{
    char buf[768];
    OrganPreset p;
    p.SetDefaults();
    const size_t n = chompi::Serialize(p, buf, sizeof(buf));
    OrganPreset q;
    assert(chompi::Parse(buf, n, q));
    assert(SamePreset(p, q));
}

static void test_defaults_for_missing_keys()
{
    /* an empty object parses to the boot defaults */
    OrganPreset p;
    p.SetDefaults();
    p.level[0] = 1; /* dirty it to prove Parse overwrites */
    char doc[] = "{}";
    assert(chompi::Parse(doc, strlen(doc), p));
    OrganPreset def;
    def.SetDefaults();
    assert(SamePreset(p, def));

    /* a partial legacy doc keeps unit-correct v1 defaults for absent keys */
    char doc2[] = "{\"version\":1,\"level\":[1,2,3,4]}";
    assert(chompi::Parse(doc2, strlen(doc2), p));
    assert(p.level[0] == 1 && p.level[1] == 2 && p.level[2] == 3
           && p.level[3] == 4);
    assert(p.character_step[0] == OrganPreset::kDefaultCharacter);
    assert(p.drive_tone_step == OrganPreset::kDefaultTone);
    assert(p.vol_mod[2] == 0 && p.char_mod[1] == 0);
    assert(p.length_mod[0] == 0 && p.length_mod[3] == 0);
    assert(p.mute[0] == 0);
    assert(p.length_step[0] == 16 && p.length_step[3] == 16);
    /* percussion and the delay fields default to off / dry / 250 ms for
     * older presets that predate them */
    assert(p.percussion == 0);
    assert(p.delay_mix_step == 0);
    assert(p.delay_time_step == OrganPreset::kDefaultDelayTime);
    /* drive_mod defaults to off for older presets that predate it */
    assert(p.drive_mod == 0);
    /* delay_mod defaults to off for older presets that predate it */
    assert(p.delay_mod == 0);
    /* vibrato_delay defaults to off for older presets that predate it */
    assert(p.vibrato_delay_step == 0);
}

static void test_clamping()
{
    OrganPreset p;
    char doc[]
        = "{\"version\":1,"
          "\"level\":[99,-3,32,0],"
          "\"character\":[128,64,-1,0],"
          "\"attack\":[33,0,0,0],\"release\":[0,0,0,99],"
          "\"mute\":[2,0,1,-1],"
          "\"length\":[-3,2,5,0],"
          "\"vibrato\":51,\"vibrato_delay\":99,\"rotary\":-2,\"drive\":64,"
          "\"drive_tone\":101,\"reverb_mix\":-50,\"reverb_decay\":1000,"
          "\"vol_mod\":[40,0,0,0],\"char_mod\":[0,0,0,-9],"
          "\"length_mod\":[0,33,-1,7],"
          "\"percussion\":2,\"delay_mix\":111,\"delay_time_ms\":4,"
          "\"drive_mod\":99,\"delay_mod\":-7}";
    assert(chompi::Parse(doc, strlen(doc), p));
    assert(p.level[0] == 32 && p.level[1] == 0 && p.level[2] == 32
           && p.level[3] == 0);
        assert(p.character_step[0] == 32 && p.character_step[1] == 32
            && p.character_step[2] == 0);
    assert(p.attack[0] == 32 && p.release[3] == 32);
    assert(p.mute[0] == 1 && p.mute[1] == 0 && p.mute[2] == 1
           && p.mute[3] == 0);
    /* pipe length clamps to -2..+2 */
        assert(p.length_step[0] == 0 && p.length_step[1] == 32
            && p.length_step[2] == 32 && p.length_step[3] == 16);
        assert(p.vibrato_depth_step == 32 && p.rotary == 0 && p.drive == 32);
        assert(p.vibrato_delay_step == 32);
        assert(p.drive_tone_step == 32 && p.reverb_mix == 0
            && p.reverb_decay_step == 32);
    assert(p.vol_mod[0] == 32 && p.char_mod[3] == 0);
    assert(p.length_mod[0] == 0 && p.length_mod[1] == 32
           && p.length_mod[2] == 0 && p.length_mod[3] == 7);
    /* percussion stays binary; delay and time migrate to the step range */
    assert(p.percussion == 1);
    assert(p.delay_mix_step == 32);
    assert(p.delay_time_step == 0);
    /* drive mod clamps to 0..32 */
    assert(p.drive_mod == 32);
    /* delay mod clamps to 0..32 */
    assert(p.delay_mod == 0);
}

static void test_legacy_unit_migration()
{
    OrganPreset p;
    char doc[]
        = "{\"version\":1,\"character\":[0,32,64,16],"
          "\"length\":[-2,-1,0,1],\"vibrato\":25,"
          "\"vibrato_delay\":25,\"drive_tone\":50,"
          "\"reverb_decay\":50,\"delay_mix\":50,"
          "\"delay_time_ms\":250}";
    assert(chompi::Parse(doc, strlen(doc), p));
    assert(p.character_step[0] == 0 && p.character_step[1] == 16
           && p.character_step[2] == 32 && p.character_step[3] == 8);
    assert(p.length_step[0] == 0 && p.length_step[1] == 8
           && p.length_step[2] == 16 && p.length_step[3] == 24);
    assert(p.vibrato_depth_step == 16 && p.vibrato_delay_step == 16);
    assert(p.drive_tone_step == 16 && p.reverb_decay_step == 16);
    assert(p.delay_mix_step == 16 && p.delay_time_step == 15);
}

static void test_garbage_values()
{
    /* valid JSON, wrong value types: booleans/strings/floats that strtol
     * can't start on fall back to defaults; floats truncate */
    OrganPreset p;
    char doc[]
        = "{\"version\":1,\"level\":[true,\"x\",12.7,16],"
          "\"vibrato\":\"loud\",\"rotary\":false}";
    assert(chompi::Parse(doc, strlen(doc), p));
    assert(p.level[0] == OrganPreset::kDefaultLevel); /* true -> default */
    assert(p.level[1] == OrganPreset::kDefaultLevel); /* "x" -> default */
    assert(p.level[2] == 12);                         /* 12.7 -> 12 */
    assert(p.level[3] == 16);
    assert(p.vibrato_depth_step == OrganPreset::kDefaultVibrato);
    assert(p.rotary == OrganPreset::kDefaultRotary);

    /* invalid JSON fails and leaves the preset untouched */
    p.SetDefaults();
    const OrganPreset before = p;
    char bad[] = "{not json";
    assert(!chompi::Parse(bad, strlen(bad), p));
    assert(SamePreset(p, before));
}

static void test_version_handling()
{
    OrganPreset p;
    /* unsupported future versions fail without mutating the destination */
    char doc[] = "{\"version\":99,\"drive\":7,\"future\":[1,2,3]}";
    p.SetDefaults();
    const OrganPreset before = p;
    assert(!chompi::Parse(doc, strlen(doc), p));
    assert(SamePreset(p, before));
    /* missing version = tolerated (treated like v1) */
    char doc2[] = "{\"drive\":8}";
    assert(chompi::Parse(doc2, strlen(doc2), p));
    assert(p.drive == 8);
    /* a nonnumeric version falls back to the legacy schema */
    char doc3[] = "{\"version\":\"x\",\"drive\":9}";
    assert(chompi::Parse(doc3, strlen(doc3), p));
    assert(p.drive == 9);
}

static void test_unknown_keys_ignored()
{
    OrganPreset p;
    char doc[] = "{\"version\":1,\"volume\":99,\"octave\":-2,"
                 "\"drive_type\":2,\"drive\":5}";
    assert(chompi::Parse(doc, strlen(doc), p));
    assert(p.drive == 5);
}

static void test_bank_ops()
{
    OrganPresetBank bank;
    bank.Init();
    for(int i = 0; i < OrganPresetBank::kNumSlots; i++)
    {
        assert(!bank.IsValid(i));
        assert(!bank.IsDirty(i));
    }
    assert(bank.NextDirtySlot() == -1);

    /* save marks valid + dirty */
    const OrganPreset p = MakeAssortedPreset();
    bank.Save(3, p);
    assert(bank.IsValid(3));
    assert(bank.IsDirty(3));
    assert(bank.NextDirtySlot() == 3);
    assert(SamePreset(bank.Get(3), p));

    /* clearing the dirty bit leaves the content */
    bank.ClearDirty(3);
    assert(!bank.IsDirty(3));
    assert(bank.IsValid(3));

    /* copy writes the destination only, requires a valid source */
    bank.Copy(3, 10);
    assert(bank.IsValid(10) && bank.IsDirty(10));
    assert(SamePreset(bank.Get(10), p));
    bank.Copy(3, 3); /* self-copy is a no-op */
    assert(!bank.IsDirty(3));
    bank.Erase(5);
    assert(!bank.IsValid(5) && bank.IsDirty(5)); /* pending delete */
    bank.Copy(5, 6); /* invalid source: no-op */
    assert(!bank.IsValid(6) && !bank.IsDirty(6));

    /* erase clears valid, keeps the dirty bit for the file delete */
    bank.Erase(10);
    assert(!bank.IsValid(10) && bank.IsDirty(10));

    /* lowest dirty slot first */
    bank.Save(12, p);
    assert(bank.NextDirtySlot() == 5);
    bank.ClearDirty(5);
    assert(bank.NextDirtySlot() == 10);
    bank.ClearDirty(10);
    assert(bank.NextDirtySlot() == 12);
    bank.ClearDirty(12);
    assert(bank.NextDirtySlot() == -1);

    /* LoadSlot fills content WITHOUT dirtying (boot load) */
    bank.LoadSlot(7, p);
    assert(bank.IsValid(7) && !bank.IsDirty(7));

    /* out-of-range slots are rejected */
    bank.Save(-1, p);
    bank.Save(15, p);
    bank.Erase(99);
    assert(bank.NextDirtySlot() == -1);
    assert(!bank.IsValid(-1) && !bank.IsValid(15));
}

static void test_capture_apply()
{
    FakeEngine e;
    e.SetDefaults();
    e.SetPipeStep(0, 3);
    e.SetPipeCharacterStep(1, 30);
    e.SetPipeAttackStep(2, 20);
    e.SetPipeReleaseStep(3, 11);
    e.SetPipeMuted(1, true);
    e.SetPipeLengthStep(0, 0);
    e.SetPipeLengthStep(3, 23);
    e.SetPipeVolModStep(0, 9);
    e.SetPipeCharModStep(2, 12);
    e.SetPipeLengthModStep(1, 21);
    e.SetVibratoDepthStep(16);
    e.SetVibratoDelayStep(22);
    e.SetRotaryStep(5);
    e.SetDriveStep(16);
    e.SetDriveToneStep(22);
    e.SetReverbMixStep(13);
    e.SetReverbDecayStep(18);
    e.SetPercussionEnabled(true);
    e.SetDelayMixStep(21);
    e.SetDelayTimeStep(21);
    e.SetDriveModStep(18);
    e.SetDelayModStep(21);

    OrganPreset p;
    chompi::CaptureFromEngine(p, e);
        assert(p.level[0] == 3 && p.character_step[1] == 30 && p.attack[2] == 20
           && p.release[3] == 11 && p.mute[1] == 1 && p.mute[0] == 0);
        assert(p.length_step[0] == 0 && p.length_step[3] == 23
            && p.length_step[1] == 16);
    assert(p.vol_mod[0] == 9 && p.char_mod[2] == 12
           && p.length_mod[1] == 21);
        assert(p.vibrato_depth_step == 16 && p.vibrato_delay_step == 22
            && p.rotary == 5
           && p.drive == 16
            && p.drive_tone_step == 22 && p.reverb_mix == 13
            && p.reverb_decay_step == 18);
        assert(p.percussion == 1 && p.delay_mix_step == 21
            && p.delay_time_step == 21);
    assert(p.drive_mod == 18);
    assert(p.delay_mod == 21);

    /* scramble the engine, then apply the preset back */
    e.SetDefaults();
    e.SetDriveToneStep(1);
    e.SetPipeMuted(1, false);
    e.SetPercussionEnabled(true); /* init/off default must clear it */
    e.SetDriveModStep(31);        /* must be overwritten by the apply */
    e.SetDelayModStep(7);         /* must be overwritten by the apply */
    chompi::ApplyToEngine(p, e);
    assert(e.GetPipeStep(0) == 3 && e.GetPipeCharacterStep(1) == 30);
    assert(e.GetPipeAttackStep(2) == 20 && e.GetPipeReleaseStep(3) == 11);
    assert(e.GetPipeMuted(1) && !e.GetPipeMuted(0));
        assert(e.GetPipeLengthStep(0) == 0 && e.GetPipeLengthStep(3) == 23
            && e.GetPipeLengthStep(1) == 16);
    assert(e.GetPipeVolModStep(0) == 9 && e.GetPipeCharModStep(2) == 12
           && e.GetPipeLengthModStep(1) == 21);
        assert(e.GetVibratoDepthStep() == 16 && e.GetVibratoDelayStep() == 22
           && e.GetRotaryStep() == 5);
        assert(e.GetDriveStep() == 16 && e.GetDriveToneStep() == 22);
        assert(e.GetReverbMixStep() == 13 && e.GetReverbDecayStep() == 18);
        assert(e.GetPercussionEnabled() && e.GetDelayMixStep() == 21
            && e.GetDelayTimeStep() == 21);
    assert(e.GetDriveModStep() == 18);
    assert(e.GetDelayModStep() == 21);

    /* capture -> serialize -> parse -> apply is lossless */
    char buf[768];
    const size_t n = chompi::Serialize(p, buf, sizeof(buf));
    OrganPreset q;
    assert(chompi::Parse(buf, n, q));
    e.SetDefaults();
    chompi::ApplyToEngine(q, e);
    /* pipe 2 was never changed: it round-trips at its 50% default */
        assert(e.GetDriveToneStep() == 22
           && e.GetPipeStep(2) == OrganPreset::kDefaultLevel);
        assert(e.GetReverbDecayStep() == 18);
}

static void test_set_init()
{
    /* SetInit on a fully dirtied preset restores the init sound */
    OrganPreset p = MakeAssortedPreset();
    p.SetInit();
    for(int i = 0; i < OrganPreset::kNumPipes; i++)
    {
        assert(p.level[i] == 16);    /* 50% */
        assert(p.character_step[i] == 0); /* clean sine, shaping bypassed */
        assert(p.attack[i] == 0);    /* 5 ms, the fastest */
        assert(p.release[i] == 0);
        assert(p.mute[i] == 0);      /* mute off */
        assert(p.length_step[i] == 16); /* nominal footage */
        assert(p.vol_mod[i] == 0);
        assert(p.char_mod[i] == 0);
        assert(p.length_mod[i] == 0);
    }
    assert(p.vibrato_depth_step == 0);
    assert(p.vibrato_delay_step == 0); /* vibrato delay off */
    assert(p.rotary == 0);
    assert(p.drive == 0);
    assert(p.drive_tone_step == 32); /* neutral */
    assert(p.reverb_mix == 0);
    assert(p.reverb_decay_step == 0); /* 0.2 s, the shortest */
    assert(p.percussion == 0);   /* percussion off */
    assert(p.delay_mix_step == 0); /* delay dry */
    assert(p.delay_time_step == 15); /* 256 ms on the shared grid */
    assert(p.drive_mod == 0);    /* distortion modulation off */
    assert(p.delay_mod == 0);    /* delay modulation off */

    /* the init sound IS the boot sound: Character and rotary boot at 0 */
    assert(OrganPreset::kDefaultCharacter == 0);
    assert(OrganPreset::kDefaultRotary == 0);
    OrganPreset def;
    def.SetDefaults();
    assert(SamePreset(p, def));
}

static void test_serialized_size_bound()
{
    /* every field at its widest value: the serialized document must fit
     * both the documented caller buffers and PresetStorage::kJsonSize
     * (768 bytes) with margin */
    OrganPreset p;
    for(int i = 0; i < OrganPreset::kNumPipes; i++)
    {
        p.level[i] = 32;
        p.character_step[i] = 32;
        p.attack[i] = 32;
        p.release[i] = 32;
        p.mute[i] = 1;
        p.length_step[i] = 32;
        p.vol_mod[i] = 32;
        p.char_mod[i] = 32;
        p.length_mod[i] = 32;
    }
    p.vibrato_depth_step = 32;
    p.vibrato_delay_step = 32;
    p.rotary = 32;
    p.drive = 32;
    p.drive_tone_step = 32;
    p.reverb_mix = 32;
    p.reverb_decay_step = 32;
    p.percussion = 1;
    p.delay_mix_step = 32;
    p.delay_time_step = 32;
    p.drive_mod = 32;
    p.delay_mod = 32;
    char buf[768];
    const size_t n = chompi::Serialize(p, buf, sizeof(buf));
    assert(n > 0 && n < sizeof(buf));
}

int main()
{
    test_roundtrip();
    test_roundtrip_defaults();
    test_defaults_for_missing_keys();
    test_clamping();
    test_legacy_unit_migration();
    test_garbage_values();
    test_version_handling();
    test_unknown_keys_ignored();
    test_bank_ops();
    test_capture_apply();
    test_set_init();
    test_serialized_size_bound();
    printf("organ_preset_test: all tests passed\n");
    return 0;
}
