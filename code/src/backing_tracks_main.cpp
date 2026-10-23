#include "hardware.h"
#include "temp_led_stuff.h"
#include "BackingMixer.h"
#include "BackingFeedback.h"
#include "BackingMidi.h"
#include "BackingSet.h"
#include "BackingTiming.h"
#include "BackingTransport.h"
#include "BackingBpmEditor.h"
#include "BackingSettings.h"
#include "BackingWav.h"
#include "diskio.h"
#include "fatfs.h"
#include "util/sd_diskio.h"
#include "hid/midi.h"

#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

using namespace daisy;
using namespace chompi;

static Hardware hardware;
static MidiUartHandler midi_uart;
static MidiUsbHandler usb_midi;
static SdmmcHandler sdmmc;
static FatFSInterface fatfs;
static BackingSet backing_set;
static BackingRing playback_ring;
static BackingTransport transport;
static BackingTiming timing;
static BackingBpmEditor bpm_editor;
static BackingMidi midi_output;
static BackingClick metronome_click;
static FIL set_file;
static FIL wave_file;
static FIL settings_file;
static char set_json[kBackingMaxSetBytes + 1];
static char settings_bytes[kBackingSettingsMaxBytes + 1];
static char settings_verify_bytes[kBackingSettingsMaxBytes + 1];
static const char kBackingDirectory[] = "BACKING-TRACKS/";
static char wave_path[kBackingMaxPath + sizeof(kBackingDirectory)];
static int16_t DSY_SDRAM_BSS playback_samples[2 * 262144];
static int16_t read_buffer[8192];

static constexpr uint32_t kPlaybackRingFrames = 262144;
static constexpr uint32_t kReadChunkFrames = 4096;
static constexpr uint32_t kPrebufferFrames = 48000;
static constexpr int kKeyCount = 15;
static constexpr int kMidiClockToggleKey
    = static_cast<int>(Hardware::SwId::KEY_16);
static constexpr int kMidiClockModifierKey
    = static_cast<int>(Hardware::SwId::KEY_26);
static constexpr uint8_t kMidiClockLed = 0;
static constexpr int kAdvanceToggleKey
    = static_cast<int>(Hardware::SwId::KEY_17);
static constexpr uint8_t kAdvanceModeLed = 1;
static const int kBpmSelectorKeys[5] = {
    static_cast<int>(Hardware::SwId::KEY_18),
    static_cast<int>(Hardware::SwId::KEY_19),
    static_cast<int>(Hardware::SwId::KEY_20),
    static_cast<int>(Hardware::SwId::KEY_21),
    static_cast<int>(Hardware::SwId::KEY_22)};
static const uint8_t kBpmSelectorLeds[5] = {2, 3, 4, 5, 6};
static constexpr uint8_t kEncoder1Led = 1;
static constexpr uint8_t kEncoder2Led = 2;
static constexpr uint8_t kEncoder3Led = 3;
static constexpr uint8_t kEncoder4Led = 4;
static constexpr uint8_t kLoopLed = 8;
static const int kWhiteKeys[kKeyCount] = {
    static_cast<int>(Hardware::SwId::KEY_1),
    static_cast<int>(Hardware::SwId::KEY_2),
    static_cast<int>(Hardware::SwId::KEY_3),
    static_cast<int>(Hardware::SwId::KEY_4),
    static_cast<int>(Hardware::SwId::KEY_5),
    static_cast<int>(Hardware::SwId::KEY_6),
    static_cast<int>(Hardware::SwId::KEY_7),
    static_cast<int>(Hardware::SwId::KEY_8),
    static_cast<int>(Hardware::SwId::KEY_9),
    static_cast<int>(Hardware::SwId::KEY_10),
    static_cast<int>(Hardware::SwId::KEY_11),
    static_cast<int>(Hardware::SwId::KEY_12),
    static_cast<int>(Hardware::SwId::KEY_13),
    static_cast<int>(Hardware::SwId::KEY_14),
    static_cast<int>(Hardware::SwId::KEY_15)};
static const uint8_t kWhiteKeyLeds[kKeyCount] = {
    24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10};
static std::atomic<uint32_t> midi_entry_publication{1};
static std::atomic<bool> midi_clock_enabled_snapshot{true};
static std::atomic<uint8_t> advance_mode_snapshot{
    static_cast<uint8_t>(BackingAdvance::Wait)};
static std::atomic<uint32_t> advance_edit_request{0};
static std::atomic<uint32_t> handled_advance_edit_request{0};
static std::atomic<uint32_t> bpm_request_revision{0};
static std::atomic<uint32_t> bpm_ack_revision{0};
static std::atomic<uint32_t> bpm_request_generation{0};
static std::atomic<uint32_t> bpm_request_song{0};
static std::atomic<uint32_t> bpm_request_value{0};
static std::atomic<uint32_t> bpm_request_deadline{0};
static std::atomic<bool> bpm_ack_success{false};
static std::atomic<bool> bpm_ack_uncertain{false};
static std::atomic<uint32_t> bpm_set_generation{0};
static std::atomic<uint32_t> bpm_feedback{0};
static std::atomic<uint32_t> bpm_feedback_until{0};
static std::atomic<bool> bpm_editor_invalid_flash{false};
static std::atomic<bool> close_file_request{false};
static std::atomic<bool> finish_request{false};
static std::atomic<bool> reload_set_request{false};
static std::atomic<bool> clipping{false};
static std::atomic<bool> storage_error{false};
static std::atomic<bool> midi_error{false};
static std::atomic<bool> set_valid{false};
static std::atomic<int> failed_song{-1};
static std::atomic<bool> card_present{false};
static std::atomic<bool> settings_dirty{false};
static std::atomic<bool> settings_missing{true};
static std::atomic<bool> settings_error{false};
static std::atomic<bool> settings_saving{false};
static std::atomic<uint32_t> settings_revision{0};
static std::atomic<uint32_t> settings_changed_at{0};
static uint8_t level_steps[5] = {16, 16, 0, 16, 16};
static std::atomic<uint32_t> displayed_levels{0};
static std::atomic<int> displayed_control{-1};
static std::atomic<uint32_t> level_changed_at{0};
static std::atomic<uint32_t> beat_at{0};
static std::atomic<bool> beat_downbeat{false};
static std::atomic<bool> beat_seen{false};
static uint32_t settings_generation = 0;
static int active_settings_slot = -1;
static std::atomic<uint8_t> active_page{0};
static bool previous_play = false;
static bool previous_stop = false;
static bool previous_midi_toggle_key = false;
static bool previous_advance_toggle_key = false;
static size_t set_json_length = 0;
static bool previous_keys[kKeyCount] = {};
static bool timing_started = false;
static uint32_t applied_midi_entry_publication = UINT32_MAX;
static bool resume_phase_valid = false;
static uint64_t resume_phase = 0;
static uint32_t handled_bpm_ack_revision = 0;
static volatile uint32_t audio_ticks = 0;
static bool wave_open = false;
static bool wave_eof = false;
static uint32_t wave_frames_read = 0;
static uint32_t wave_file_bytes = 0;
static BackingWavInfo wave_info = {};

class GainSmoother
{
  public:
    void SetStep(uint8_t step) { target_ = static_cast<float>(step) / 32.f; }
    float Next()
    {
        current_ += (target_ - current_) * .002f;
        return current_;
    }
    void Init(uint8_t step)
    {
        current_ = target_ = static_cast<float>(step) / 32.f;
    }

  private:
    float current_ = 0.f;
    float target_ = 0.f;
};

static GainSmoother gains[5];

static int SelectedSongIndex()
{
    const uint32_t encoded_song
        = midi_entry_publication.load(std::memory_order_acquire) & 0x1fu;
    return encoded_song == 0 ? -1 : static_cast<int>(encoded_song - 1u);
}

static bool SendUsbMidiByte(uint8_t byte)
{
    usb_midi.SendMessage(&byte, 1);
    return true;
}

static bool SendMidiByte(uint8_t byte)
{
    const auto uart_send = [&](uint8_t data) {
        if((USART1->ISR & USART_ISR_TXE_TXFNF) == 0)
        {
            midi_error.store(true, std::memory_order_release);
            return false;
        }
        USART1->TDR = data;
        return true;
    };
    return SendMidiBytePair(byte, uart_send, SendUsbMidiByte);
}

static void PublishSelectedSong(int song, bool reset_override = false)
{
    uint32_t published = midi_entry_publication.load(std::memory_order_acquire);
    for(;;)
    {
        const uint32_t encoded_song = published & 0x1fu;
        const int current_song
            = encoded_song == 0 ? -1 : static_cast<int>(encoded_song - 1u);
        if(!reset_override && current_song == song)
            return;
        const uint32_t next_revision = (published & ~0x1fu) + 0x20u;
        const uint32_t next_song
            = song >= 0 ? static_cast<uint32_t>(song + 1) : 0u;
        const uint32_t next = next_revision | next_song;
        if(midi_entry_publication.compare_exchange_weak(
               published, next, std::memory_order_release,
               std::memory_order_acquire))
            return;
    }
}

static void PublishAdvanceEdit(int song, BackingAdvance advance)
{
    uint32_t published = advance_edit_request.load(std::memory_order_acquire);
    for(;;)
    {
        const uint32_t revision = (published & ~0x7fu) + 0x80u;
        const uint32_t next = revision
                              | (static_cast<uint32_t>(song + 1) << 2u)
                              | static_cast<uint32_t>(advance);
        if(advance_edit_request.compare_exchange_weak(
               published, next, std::memory_order_release,
               std::memory_order_acquire))
            return;
    }
}

static bool AdvanceEditPending()
{
    return advance_edit_request.load(std::memory_order_acquire)
           != handled_advance_edit_request.load(std::memory_order_acquire);
}

static void SyncMidiEntry()
{
    const uint32_t publication
        = midi_entry_publication.load(std::memory_order_acquire);
    if(publication == applied_midi_entry_publication)
        return;
    const uint32_t encoded_song = publication & 0x1fu;
    const int song
        = encoded_song == 0 ? -1 : static_cast<int>(encoded_song - 1u);
    const bool valid = set_valid.load(std::memory_order_acquire) && song >= 0
                       && static_cast<size_t>(song) < backing_set.count;
    const bool enabled = valid && backing_set.songs[song].midi_clock;
    const BackingAdvance advance
        = valid ? backing_set.songs[song].advance : BackingAdvance::Wait;
    if(!midi_output.ApplyEntry(enabled, SendMidiByte))
        midi_error.store(true, std::memory_order_release);
    midi_clock_enabled_snapshot.store(enabled, std::memory_order_release);
    advance_mode_snapshot.store(static_cast<uint8_t>(advance),
                                std::memory_order_release);
    applied_midi_entry_publication = publication;
}

static bool ResolveWavePath(const char *relative_path)
{
    const size_t prefix_length = sizeof(kBackingDirectory) - 1;
    const size_t relative_length = strlen(relative_path);
    if(relative_length == 0 || relative_length > kBackingMaxPath
       || prefix_length + relative_length + 1 > sizeof(wave_path))
        return false;
    memcpy(wave_path, kBackingDirectory, prefix_length);
    memcpy(wave_path + prefix_length, relative_path, relative_length);
    wave_path[prefix_length + relative_length] = '\0';
    return true;
}

static bool ReadWaveAt(void *context,
                       uint32_t offset,
                       uint8_t *destination,
                       size_t length)
{
    FIL *file = static_cast<FIL *>(context);
    UINT read = 0;
    return f_lseek(file, offset) == FR_OK
           && f_read(file, destination, static_cast<UINT>(length), &read)
                  == FR_OK
           && read == length;
}

static bool LoadSetFromCard()
{
    failed_song.store(-1, std::memory_order_release);
    if(disk_status(0) != RES_OK)
        return false;
    bool recovered_backup = false;
    if(f_open(&set_file, "BACKING-TRACKS/set.json", FA_READ) != FR_OK)
    {
        if(f_open(&set_file, "BACKING-TRACKS/set.json.BAK", FA_READ) != FR_OK)
            return false;
        recovered_backup = true;
    }
    const FSIZE_t file_size = f_size(&set_file);
    if(file_size == 0 || file_size > kBackingMaxSetBytes)
    {
        f_close(&set_file);
        return false;
    }
    UINT bytes_read = 0;
    const FRESULT result
        = f_read(&set_file, set_json, static_cast<UINT>(file_size), &bytes_read);
    f_close(&set_file);
    if(result != FR_OK || bytes_read != file_size)
        return false;
    set_json[bytes_read] = '\0';
    set_json_length = bytes_read;
    if(!ParseBackingSet(set_json, bytes_read, backing_set))
        return false;
    if(recovered_backup
       && f_rename("BACKING-TRACKS/set.json.BAK",
                   "BACKING-TRACKS/set.json") != FR_OK)
        return false;
    return true;
}

enum class SetSaveResult : uint8_t
{
    Success,
    FailedUnchanged,
    CommitUncertain
};

static SetSaveResult SaveSetJson()
{
    static const char temporary[] = "BACKING-TRACKS/set.json.TMP";
    static const char destination[] = "BACKING-TRACKS/set.json";
    static const char backup[] = "BACKING-TRACKS/set.json.BAK";
    if(set_json_length == 0 || set_json_length > kBackingMaxSetBytes)
        return SetSaveResult::FailedUnchanged;

    UINT bytes_written = 0;
    if(f_open(&set_file, temporary, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return SetSaveResult::FailedUnchanged;
    bool success = f_write(&set_file, set_json,
                           static_cast<UINT>(set_json_length), &bytes_written)
                       == FR_OK
                   && bytes_written == set_json_length
                   && f_sync(&set_file) == FR_OK;
    if(f_close(&set_file) != FR_OK)
        success = false;
    if(!success || f_open(&set_file, temporary, FA_READ) != FR_OK)
        return SetSaveResult::FailedUnchanged;

    const FSIZE_t verify_size = f_size(&set_file);
    success = verify_size == set_json_length;
    size_t verified_bytes = 0;
    while(success && verified_bytes < set_json_length)
    {
        const size_t remaining = set_json_length - verified_bytes;
        const UINT chunk = static_cast<UINT>(
            remaining < sizeof(read_buffer) ? remaining : sizeof(read_buffer));
        UINT bytes_read = 0;
        success = f_read(&set_file, read_buffer, chunk, &bytes_read) == FR_OK
                  && bytes_read == chunk
                  && memcmp(read_buffer, set_json + verified_bytes, chunk) == 0;
        verified_bytes += success ? bytes_read : 0;
    }
    if(f_close(&set_file) != FR_OK)
        success = false;
    if(!success)
        return SetSaveResult::FailedUnchanged;

    const FRESULT remove_backup = f_unlink(backup);
    if(remove_backup != FR_OK && remove_backup != FR_NO_FILE)
        return SetSaveResult::FailedUnchanged;
    if(f_rename(destination, backup) != FR_OK)
        return SetSaveResult::CommitUncertain;
    if(f_rename(temporary, destination) != FR_OK)
    {
        return f_rename(backup, destination) == FR_OK
                   ? SetSaveResult::FailedUnchanged
                   : SetSaveResult::CommitUncertain;
    }
    f_unlink(backup);
    return SetSaveResult::Success;
}

static bool ReadSettingsSlot(int slot, BackingSettings &settings)
{
    const char *path = slot == 0 ? "BACKING-TRACKS/LEVEL0.json"
                                 : "BACKING-TRACKS/LEVEL1.json";
    UINT bytes_read = 0;
    if(f_open(&settings_file, path, FA_READ) != FR_OK)
        return false;
    const FSIZE_t file_size = f_size(&settings_file);
    const bool valid = file_size > 0
                       && file_size <= kBackingSettingsMaxBytes
                       && f_read(&settings_file, settings_verify_bytes,
                                 static_cast<UINT>(file_size), &bytes_read)
                              == FR_OK
                       && bytes_read == file_size
                       && DecodeBackingSettings(settings_verify_bytes,
                                                bytes_read, settings);
    f_close(&settings_file);
    return valid;
}

static void LoadSettings()
{
    BackingSettings first = {};
    BackingSettings second = {};
    const bool first_valid = ReadSettingsSlot(0, first);
    const bool second_valid = ReadSettingsSlot(1, second);
    if(!first_valid && !second_valid)
    {
        level_steps[0] = 16;
        level_steps[1] = 16;
        level_steps[2] = 0;
        level_steps[3] = 16;
        level_steps[4] = 16;
        settings_missing.store(true, std::memory_order_release);
        settings_error.store(false, std::memory_order_release);
        settings_generation = 0;
        active_settings_slot = -1;
        return;
    }
    const bool choose_second
        = second_valid
          && (!first_valid
              || BackingGenerationNewer(second.generation, first.generation));
    const BackingSettings &selected = choose_second ? second : first;
    for(size_t i = 0; i < kBackingLevelCount; ++i)
        level_steps[i] = selected.levels[i];
    settings_generation = selected.generation;
    active_settings_slot = choose_second ? 1 : 0;
    settings_missing.store(false, std::memory_order_release);
    settings_error.store(false, std::memory_order_release);
}

static void ServiceCardPresence()
{
    static uint32_t last_check = 0;
    const uint32_t now = System::GetNow();
    if(now - last_check < 1000)
        return;
    last_check = now;
    const bool present = disk_status(0) == RES_OK;
    const bool was_present = card_present.exchange(present,
                                                    std::memory_order_acq_rel);
    if(was_present && !present)
    {
        set_valid.store(false, std::memory_order_release);
        bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
        storage_error.store(true, std::memory_order_release);
        if(transport.GetState() != BackingTransport::State::Stopped)
            transport.Fail();
    }
    else if(!was_present && present)
    {
        set_valid.store(false, std::memory_order_release);
        bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
        storage_error.store(true, std::memory_order_release);
    }
}

static bool PreflightSet();

static void ServiceSetReload()
{
    if(!reload_set_request.load(std::memory_order_acquire)
       || transport.GetState() != BackingTransport::State::Stopped)
        return;
    reload_set_request.store(false, std::memory_order_release);
    bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
    advance_edit_request.store(0, std::memory_order_release);
    handled_advance_edit_request.store(0, std::memory_order_release);
    set_valid.store(false, std::memory_order_release);
    PublishSelectedSong(-1, true);
    if(disk_status(0) != RES_OK)
        disk_initialize(0);
    if(disk_status(0) != RES_OK)
    {
        storage_error.store(true, std::memory_order_release);
        return;
    }
    f_mount(&fatfs.GetSDFileSystem(), fatfs.GetSDPath(), 1);
    const bool valid = LoadSetFromCard() && PreflightSet();
    set_valid.store(valid, std::memory_order_release);
    storage_error.store(!valid, std::memory_order_release);
    if(valid)
    {
        PublishSelectedSong(0, true);
        LoadSettings();
        for(size_t i = 0; i < kBackingLevelCount; ++i)
            gains[i].Init(level_steps[i]);
    }
}

static void ServiceAdvanceEdit()
{
    const uint32_t request
        = advance_edit_request.load(std::memory_order_acquire);
    if(request == 0
       || request == handled_advance_edit_request.load(std::memory_order_acquire))
        return;

    const int song = static_cast<int>((request >> 2u) & 0x1fu) - 1;
    const BackingAdvance advance
        = static_cast<BackingAdvance>(request & 0x3u);
    const bool valid = transport.GetState() == BackingTransport::State::Stopped
                       && set_valid.load(std::memory_order_acquire)
                       && song >= 0
                       && static_cast<size_t>(song) < backing_set.count
                       && BackingAdvanceName(advance) != NULL;
    if(valid && backing_set.songs[song].advance != advance)
    {
        const BackingAdvance previous = backing_set.songs[song].advance;
        const bool rewritten = RewriteBackingAdvance(
            set_json, set_json_length, static_cast<size_t>(song), advance);
        const SetSaveResult save_result
            = rewritten ? SaveSetJson() : SetSaveResult::FailedUnchanged;
        if(save_result == SetSaveResult::Success)
        {
            __disable_irq();
            backing_set.songs[song].advance = advance;
            __enable_irq();
        }
        else
        {
            if(rewritten)
                RewriteBackingAdvance(set_json, set_json_length,
                                      static_cast<size_t>(song), previous);
            storage_error.store(true, std::memory_order_release);
            if(save_result == SetSaveResult::CommitUncertain)
            {
                set_valid.store(false, std::memory_order_release);
                bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
                PublishSelectedSong(-1, true);
            }
            const uint32_t latest
                = advance_edit_request.load(std::memory_order_acquire);
            if(latest == request && SelectedSongIndex() == song)
                advance_mode_snapshot.store(static_cast<uint8_t>(previous),
                                            std::memory_order_release);
        }
    }
    else if(!valid)
    {
        const uint32_t latest
            = advance_edit_request.load(std::memory_order_acquire);
        const int selected = SelectedSongIndex();
        if(latest == request && selected >= 0
           && static_cast<size_t>(selected) < backing_set.count)
            advance_mode_snapshot.store(
                static_cast<uint8_t>(backing_set.songs[selected].advance),
                std::memory_order_release);
    }
    handled_advance_edit_request.store(request, std::memory_order_release);
}

static bool BpmDeadlineExpired(uint32_t deadline)
{
    return static_cast<int32_t>(deadline - System::GetNow()) <= 0;
}

static void AcknowledgeBpmSave(uint32_t revision,
                               bool success,
                               bool uncertain)
{
    SD_ClearOperationDeadline();
    bpm_ack_success.store(success, std::memory_order_relaxed);
    bpm_ack_uncertain.store(uncertain, std::memory_order_relaxed);
    bpm_ack_revision.store(revision, std::memory_order_release);
}

static void ServiceBpmEdit()
{
    const uint32_t revision
        = bpm_request_revision.load(std::memory_order_acquire);
    if(revision == 0
       || revision == bpm_ack_revision.load(std::memory_order_acquire))
        return;

    const uint32_t deadline
        = bpm_request_deadline.load(std::memory_order_acquire);
    const uint32_t generation
        = bpm_request_generation.load(std::memory_order_relaxed);
    const uint32_t song
        = bpm_request_song.load(std::memory_order_relaxed);
    const uint16_t requested = static_cast<uint16_t>(
        bpm_request_value.load(std::memory_order_relaxed));

    const bool request_valid
        = set_valid.load(std::memory_order_acquire)
          && generation == bpm_set_generation.load(std::memory_order_acquire)
          && song < backing_set.count
          && SelectedSongIndex() == static_cast<int>(song)
          && transport.GetState() == BackingTransport::State::Stopped;
    if(!request_valid)
    {
        const bool uncertain = !set_valid.load(std::memory_order_acquire)
                               || SD_IsQuarantined();
        if(uncertain)
        {
            set_valid.store(false, std::memory_order_release);
            storage_error.store(true, std::memory_order_release);
        }
        AcknowledgeBpmSave(revision, false, uncertain);
        return;
    }

    if(SD_IsQuarantined())
    {
        set_valid.store(false, std::memory_order_release);
        storage_error.store(true, std::memory_order_release);
        AcknowledgeBpmSave(revision, false, true);
        return;
    }

    if(AdvanceEditPending()
       || settings_dirty.load(std::memory_order_acquire)
       || settings_saving.load(std::memory_order_acquire))
    {
        if(SD_IsQuarantined())
        {
            set_valid.store(false, std::memory_order_release);
            bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
            storage_error.store(true, std::memory_order_release);
            PublishSelectedSong(-1, true);
            AcknowledgeBpmSave(revision, false, true);
            return;
        }
        if(!BpmDeadlineExpired(deadline))
            return;
        storage_error.store(true, std::memory_order_release);
        AcknowledgeBpmSave(revision, false, false);
        return;
    }

    if(BpmDeadlineExpired(deadline))
    {
        storage_error.store(true, std::memory_order_release);
        AcknowledgeBpmSave(revision, false, false);
        return;
    }

    BackingBpmTokenBackup backup = {};
    const bool rewritten = RewriteBackingBpm(
        set_json, sizeof(set_json), set_json_length, song, requested, &backup);
    const SetSaveResult save_result
        = rewritten ? SaveSetJson() : SetSaveResult::FailedUnchanged;
    const bool success = save_result == SetSaveResult::Success
                         && !SD_IsQuarantined();
    bool uncertain = save_result == SetSaveResult::CommitUncertain
                     || SD_IsQuarantined();
    if(success)
    {
        __disable_irq();
        backing_set.songs[song].bpm_hundredths = requested;
        __enable_irq();
    }
    else
    {
        if(rewritten
           && !RestoreBackingBpm(set_json, sizeof(set_json),
                                 set_json_length, backup))
            uncertain = true;
        set_json[set_json_length] = '\0';
        storage_error.store(true, std::memory_order_release);
        if(uncertain)
        {
            set_valid.store(false, std::memory_order_release);
            bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
            PublishSelectedSong(-1, true);
        }
    }
    AcknowledgeBpmSave(revision, success, uncertain);
}

static void MarkSettingsDirty()
{
    settings_revision.fetch_add(1, std::memory_order_relaxed);
    settings_changed_at.store(System::GetNow(), std::memory_order_relaxed);
    settings_dirty.store(true, std::memory_order_release);
}

static bool SaveSettings()
{
    BackingSettings snapshot = {};
    uint32_t revision = 0;
    __disable_irq();
    revision = settings_revision.load(std::memory_order_relaxed);
    for(size_t i = 0; i < kBackingLevelCount; ++i)
        snapshot.levels[i] = level_steps[i];
    __enable_irq();
    snapshot.generation = settings_generation + 1u;
    size_t settings_length = 0;
    if(!EncodeBackingSettings(snapshot, settings_bytes,
                              sizeof(settings_bytes), settings_length))
        return false;

    const int target_slot = active_settings_slot == 0 ? 1 : 0;
    const char *temporary = target_slot == 0
                                ? "BACKING-TRACKS/LEVEL0.TMP"
                                : "BACKING-TRACKS/LEVEL1.TMP";
    const char *destination = target_slot == 0
                                  ? "BACKING-TRACKS/LEVEL0.json"
                                  : "BACKING-TRACKS/LEVEL1.json";
    UINT bytes_written = 0;
    if(f_open(&settings_file, temporary, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;
    bool success = f_write(&settings_file, settings_bytes,
                           static_cast<UINT>(settings_length), &bytes_written)
                       == FR_OK
                   && bytes_written == settings_length
                   && f_sync(&settings_file) == FR_OK;
    if(f_close(&settings_file) != FR_OK)
        success = false;
    if(!success)
        return false;

    BackingSettings verified = {};
    if(f_open(&settings_file, temporary, FA_READ) != FR_OK)
        return false;
    UINT bytes_read = 0;
    const FSIZE_t verify_size = f_size(&settings_file);
    success = verify_size > 0 && verify_size <= kBackingSettingsMaxBytes
              && f_read(&settings_file, settings_verify_bytes,
                        static_cast<UINT>(verify_size), &bytes_read)
                  == FR_OK
              && bytes_read == verify_size
              && DecodeBackingSettings(settings_verify_bytes, bytes_read,
                                       verified)
              && verified.generation == snapshot.generation;
    if(f_close(&settings_file) != FR_OK)
        success = false;
    if(!success)
        return false;

    f_unlink(destination);
    if(f_rename(temporary, destination) != FR_OK)
        return false;
    settings_generation = snapshot.generation;
    active_settings_slot = target_slot;
    settings_missing.store(false, std::memory_order_release);
    if(settings_revision.load(std::memory_order_relaxed) == revision)
        settings_dirty.store(false, std::memory_order_release);
    return true;
}

static void ServiceSettings()
{
    if(!settings_dirty.load(std::memory_order_acquire)
       || transport.GetState() != BackingTransport::State::Stopped
       || disk_status(0) != RES_OK)
        return;
    if(System::GetNow() - settings_changed_at.load(std::memory_order_relaxed)
       < 500)
        return;
    __disable_irq();
    const bool can_save
        = transport.GetState() == BackingTransport::State::Stopped;
    if(can_save)
        settings_saving.store(true, std::memory_order_release);
    __enable_irq();
    if(!can_save)
        return;
    if(!SaveSettings())
        settings_error.store(true, std::memory_order_release);
    else
        settings_error.store(false, std::memory_order_release);
    settings_saving.store(false, std::memory_order_release);
}

static void CloseWave()
{
    if(wave_open)
    {
        f_close(&wave_file);
        wave_open = false;
    }
    wave_eof = false;
    wave_frames_read = 0;
    wave_file_bytes = 0;
}

static bool OpenSelectedWave()
{
    const int index = SelectedSongIndex();
    if(!set_valid.load(std::memory_order_acquire) || index < 0
       || static_cast<size_t>(index) >= backing_set.count
    || !ResolveWavePath(backing_set.songs[index].file)
    || f_open(&wave_file, wave_path, FA_READ) != FR_OK)
        return false;
    const FSIZE_t size = f_size(&wave_file);
    if(size > UINT32_MAX
       || !ParseBackingWav(ReadWaveAt, &wave_file, static_cast<uint32_t>(size),
                           backing_set.songs[index].first_downbeat_ms,
                           wave_info))
    {
        f_close(&wave_file);
        return false;
    }
    wave_file_bytes = static_cast<uint32_t>(size);
    if(f_lseek(&wave_file, wave_info.data_offset) != FR_OK)
    {
        f_close(&wave_file);
        return false;
    }
    wave_open = true;
    wave_eof = false;
    wave_frames_read = 0;
    return true;
}

static bool PreflightSet()
{
    for(size_t index = 0; index < backing_set.count; ++index)
    {
        failed_song.store(static_cast<int>(index), std::memory_order_release);
        if(!ResolveWavePath(backing_set.songs[index].file)
           || f_open(&wave_file, wave_path, FA_READ) != FR_OK)
            return false;
        const FSIZE_t size = f_size(&wave_file);
        BackingWavInfo info = {};
        const bool valid
            = size <= UINT32_MAX
              && ParseBackingWav(
                  ReadWaveAt, &wave_file, static_cast<uint32_t>(size),
                  backing_set.songs[index].first_downbeat_ms, info);
        f_close(&wave_file);
        if(!valid)
            return false;
    }
    failed_song.store(-1, std::memory_order_release);
    return backing_set.count != 0;
}

static bool ReadWaveChunk()
{
    const uint32_t available = playback_ring.AvailableToWrite();
    const uint32_t remaining = wave_info.frame_count - wave_frames_read;
    uint32_t frames = available < kReadChunkFrames ? available : kReadChunkFrames;
    if(frames > remaining)
        frames = remaining;
    if(frames == 0)
        return true;
    const UINT bytes = frames * wave_info.block_align;
    UINT bytes_read = 0;
    if(f_read(&wave_file, read_buffer, bytes, &bytes_read) != FR_OK
       || bytes_read != bytes)
        return false;
    if(wave_info.channels == 1)
    {
        for(uint32_t frame = frames; frame > 0; --frame)
        {
            const int16_t sample = read_buffer[frame - 1];
            read_buffer[(frame - 1) * 2] = sample;
            read_buffer[(frame - 1) * 2 + 1] = sample;
        }
    }
    if(playback_ring.Push(read_buffer, frames) != frames)
        return false;
    wave_frames_read += frames;
    if(wave_frames_read == wave_info.frame_count)
        wave_eof = true;
    return true;
}

static void ServicePlayback()
{
    if(close_file_request.exchange(false, std::memory_order_acq_rel))
    {
        CloseWave();
        playback_ring.Reset();
    }

    if(finish_request.exchange(false, std::memory_order_acq_rel))
    {
        CloseWave();
        playback_ring.Reset();
        const int current = SelectedSongIndex();
        if(current >= 0 && static_cast<size_t>(current) < backing_set.count)
        {
            const BackingAdvanceDecision decision = BackingAdvanceAtEof(
                backing_set.songs[current].advance,
                static_cast<size_t>(current), backing_set.count);
            if(decision.selected_song != current)
                PublishSelectedSong(decision.selected_song);
            if(decision.begin_load)
                transport.BeginLoad(decision.selected_song);
        }
    }

    const BackingTransport::State state = transport.GetState();
    if(state == BackingTransport::State::Loading && !wave_open)
    {
        playback_ring.Reset();
        if(!OpenSelectedWave())
        {
            failed_song.store(SelectedSongIndex(),
                              std::memory_order_release);
            storage_error.store(true, std::memory_order_release);
            transport.Fail();
            return;
        }
    }

    if(wave_open && !wave_eof
       && state != BackingTransport::State::Stopped
       && state != BackingTransport::State::Error
       && !ReadWaveChunk())
    {
        storage_error.store(true, std::memory_order_release);
        transport.Fail();
        CloseWave();
        return;
    }

    if(transport.GetState() == BackingTransport::State::Loading && wave_open)
    {
        const uint32_t prebuffer
            = wave_info.frame_count < kPrebufferFrames
                  ? wave_info.frame_count
                  : kPrebufferFrames;
        if(playback_ring.AvailableToRead() >= prebuffer || wave_eof)
        {
            transport.SetReady();
            transport.Play();
        }
    }

    if(transport.GetState() == BackingTransport::State::Error)
        CloseWave();
}

static void SetSelectedSong(int song)
{
    if(!set_valid.load(std::memory_order_acquire) || song < 0
       || static_cast<size_t>(song) >= backing_set.count)
        return;
    PublishSelectedSong(song);
    storage_error.store(false, std::memory_order_release);
}

static void PublishBpmFeedback(uint8_t cue = 0)
{
    const bool new_cue = cue != 0;
    if(cue == 0 && bpm_editor.Active() && !bpm_editor.Saving())
    {
        const uint32_t previous
            = bpm_feedback.load(std::memory_order_acquire);
        const uint8_t previous_cue
            = static_cast<uint8_t>((previous >> 9u) & 0x3u);
        const uint32_t cue_until
            = bpm_feedback_until.load(std::memory_order_acquire);
        if(previous_cue == 1
           && static_cast<int32_t>(cue_until - System::GetNow()) > 0)
            cue = previous_cue;
    }
    uint32_t feedback = 0;
    if(bpm_editor.Active())
        feedback |= 1u;
    if(bpm_editor.Saving())
        feedback |= 1u << 1u;
    const uint32_t position
        = static_cast<uint32_t>(bpm_editor.Position());
    feedback |= position << 2u;
    const int selected = SelectedSongIndex();
    if(selected >= 0 && static_cast<size_t>(selected) < backing_set.count)
        feedback |= static_cast<uint32_t>(
                        bpm_editor.SelectedDigit(
                            backing_set.songs[selected].bpm_hundredths))
                    << 5u;
    feedback |= static_cast<uint32_t>(cue) << 9u;
    bpm_feedback.store(feedback, std::memory_order_release);
    if(new_cue)
        bpm_feedback_until.store(System::GetNow() + 300u,
                                 std::memory_order_release);
}

static void PollControls()
{
    const BackingTransport::State state = transport.GetState();
    const bool stopped = state == BackingTransport::State::Stopped;
    const bool shift_down
        = hardware.button_sr.State(kMidiClockModifierKey);
    bool sampled_white[kKeyCount] = {};
    for(int key = 0; key < kKeyCount; ++key)
        sampled_white[key] = hardware.button_sr.State(kWhiteKeys[key]);
    bool selector_down[5] = {};
    bool any_selector_down = false;
    for(size_t selector = 0; selector < 5; ++selector)
    {
        selector_down[selector]
            = hardware.button_sr.State(kBpmSelectorKeys[selector]);
        any_selector_down = any_selector_down || selector_down[selector];
    }
    const bool midi_key_down = hardware.button_sr.State(kMidiClockToggleKey);
    const bool advance_key_down
        = hardware.button_sr.State(kAdvanceToggleKey);
    const bool play_down
        = hardware.button_sr.State(static_cast<int>(Hardware::SwId::KEY_27));
    const bool stop_down
        = hardware.button_sr.State(static_cast<int>(Hardware::SwId::KEY_28));
    const int page_from_switch = hardware.GetToggleState() ? 0 : 1;
    const bool was_editor_locked = bpm_editor.Active();
    uint8_t editor_cue = 0;
    const uint32_t acknowledged
        = bpm_ack_revision.load(std::memory_order_acquire);
    if(acknowledged != 0 && acknowledged != handled_bpm_ack_revision
       && acknowledged
              == bpm_request_revision.load(std::memory_order_acquire))
    {
        const bool success
            = bpm_ack_success.load(std::memory_order_relaxed);
        const bool uncertain
            = bpm_ack_uncertain.load(std::memory_order_relaxed);
        if(bpm_editor.Saving())
            bpm_editor.CompleteSave(success);
        handled_bpm_ack_revision = acknowledged;
        if(success)
            editor_cue = 2;
        else if(!uncertain)
            editor_cue = 1;
    }

    const int current_song = SelectedSongIndex();
    const bool valid_entry
        = set_valid.load(std::memory_order_acquire) && current_song >= 0
          && static_cast<size_t>(current_song) < backing_set.count;
    BackingBpmEditorInput editor_input = {};
    editor_input.shift_down = shift_down;
    editor_input.stopped = stopped;
    editor_input.valid_entry = valid_entry;
    editor_input.clean_entry_chord = !midi_key_down && !advance_key_down;
    for(size_t selector = 0; selector < 5; ++selector)
        editor_input.selector_down[selector] = selector_down[selector];
    for(size_t key = 0; key < 10; ++key)
        editor_input.white_down[key] = sampled_white[key];

    if(!was_editor_locked)
        active_page.store(static_cast<uint8_t>(page_from_switch),
                          std::memory_order_release);
    if(was_editor_locked
       && (!set_valid.load(std::memory_order_acquire) || !stopped))
        bpm_editor.Initialize(editor_input);
    BackingBpmEditorAction editor_action = {};
    if(bpm_editor.Active() || !was_editor_locked)
    {
        const uint16_t bpm = valid_entry
                                 ? backing_set.songs[current_song].bpm_hundredths
                                 : 0;
        editor_action = bpm_editor.Scan(editor_input, bpm);
    }
    if(editor_action.submit)
    {
        uint32_t revision
            = bpm_request_revision.load(std::memory_order_relaxed) + 1u;
        if(revision == 0)
            ++revision;
        const uint32_t deadline = System::GetNow() + 4700u;
        bpm_request_song.store(static_cast<uint32_t>(current_song),
                               std::memory_order_relaxed);
        bpm_request_generation.store(
            bpm_set_generation.load(std::memory_order_acquire),
            std::memory_order_relaxed);
        bpm_request_value.store(editor_action.requested_hundredths,
                                std::memory_order_relaxed);
        bpm_request_deadline.store(deadline, std::memory_order_relaxed);
        SD_SetOperationDeadline(deadline);
        bpm_request_revision.store(revision, std::memory_order_release);
    }
    else if(editor_action.invalid_value)
        editor_cue = 1;

    if(was_editor_locked && !bpm_editor.Active())
        bpm_editor.Initialize(editor_input);

    const bool editor_locked = bpm_editor.Active();
    const bool had_editor_lock = was_editor_locked || editor_locked;
    const bool ambiguous_shortcut
        = shift_down && any_selector_down
          && (midi_key_down || advance_key_down);
    if(had_editor_lock)
    {
        for(int encoder = 0; encoder < 6; ++encoder)
            hardware.enc[encoder].Increment();
        for(int key = 0; key < kKeyCount; ++key)
            previous_keys[key] = sampled_white[key];
        previous_play = play_down;
        previous_stop = stop_down;
        previous_midi_toggle_key = midi_key_down;
        previous_advance_toggle_key = advance_key_down;
        if(!editor_locked)
            active_page.store(static_cast<uint8_t>(page_from_switch),
                              std::memory_order_release);
        PublishBpmFeedback(editor_cue);
        return;
    }

    static const uint8_t gain_encoder[5] = {3, 0, 1, 2, 5};
    uint32_t levels = 0;
    for(size_t control = 0; control < 5; ++control)
    {
        const int increment = hardware.enc[gain_encoder[control]].Increment();
        if(increment != 0)
        {
            displayed_control.store(static_cast<int>(control),
                                    std::memory_order_relaxed);
            level_changed_at.store(System::GetNow(), std::memory_order_release);
        }
        int value = static_cast<int>(level_steps[control]) + increment;
        if(value < 0)
            value = 0;
        if(value > 32)
            value = 32;
        if(value != level_steps[control])
            MarkSettingsDirty();
        level_steps[control] = static_cast<uint8_t>(value);
        gains[control].SetStep(level_steps[control]);
        levels |= static_cast<uint32_t>(level_steps[control]) << (control * 6u);
    }
    displayed_levels.store(levels, std::memory_order_release);

    if(stopped)
    {
        const int movement = hardware.enc[4].Increment();
        if(movement != 0)
            SetSelectedSong(SelectedSongIndex()
                            + movement);
        for(int key = 0; key < kKeyCount; ++key)
        {
            if(sampled_white[key] && !previous_keys[key])
                SetSelectedSong(
                    static_cast<int>(active_page.load(std::memory_order_relaxed))
                        * kKeyCount
                    + key);
            previous_keys[key] = sampled_white[key];
        }
    }
    else
    {
        for(int key = 0; key < kKeyCount; ++key)
            previous_keys[key] = sampled_white[key];
    }
    SyncMidiEntry();

    const int selected_for_advance = SelectedSongIndex();
    const bool valid_advance_entry
        = set_valid.load(std::memory_order_acquire)
          && selected_for_advance >= 0
          && static_cast<size_t>(selected_for_advance) < backing_set.count;
    if(ambiguous_shortcut)
        previous_advance_toggle_key = advance_key_down;
    else if(BackingMidiToggleOnPress(shift_down, advance_key_down,
                                     valid_advance_entry && stopped,
                                     previous_advance_toggle_key))
    {
        const BackingAdvance current = static_cast<BackingAdvance>(
            advance_mode_snapshot.load(std::memory_order_acquire));
        const BackingAdvance next = NextBackingAdvance(current);
        advance_mode_snapshot.store(static_cast<uint8_t>(next),
                                    std::memory_order_release);
        PublishAdvanceEdit(selected_for_advance, next);
    }

    if(play_down && !previous_play)
    {
        const BackingTransport::State current = transport.GetState();
        if(current == BackingTransport::State::Stopped)
        {
            if(!AdvanceEditPending()
               && !settings_saving.load(std::memory_order_acquire))
            {
                if(set_valid.load(std::memory_order_acquire))
                {
                    transport.BeginLoad(
                        SelectedSongIndex());
                    storage_error.store(false, std::memory_order_release);
                }
                else
                    reload_set_request.store(true, std::memory_order_release);
            }
        }
        else if(current == BackingTransport::State::Playing)
        {
            transport.Pause();
            if(!midi_output.Pause(SendMidiByte))
                midi_error.store(true, std::memory_order_release);
        }
        else if(current == BackingTransport::State::Paused)
        {
            resume_phase_valid = timing.PositionInBar(
                transport.ConsumedFrames(), resume_phase);
            transport.Play();
            if(!timing.Active())
                transport.ResumeAtBoundary();
        }
        else if(current == BackingTransport::State::PendingResume)
            transport.Play();
    }
    previous_play = play_down;

    if(stop_down && !previous_stop)
    {
        const bool midi_stop_succeeded = midi_output.Stop(SendMidiByte);
        if(!midi_stop_succeeded)
            midi_error.store(true, std::memory_order_release);
        else if((USART1->ISR & USART_ISR_TXE_TXFNF) != 0)
            midi_error.store(false, std::memory_order_release);
        transport.Stop();
        timing.Stop();
        beat_seen.store(false, std::memory_order_release);
        timing_started = false;
        resume_phase_valid = false;
        storage_error.store(false, std::memory_order_release);
        close_file_request.store(true, std::memory_order_release);
    }
    previous_stop = stop_down;

    const int selected = SelectedSongIndex();
    const bool valid_midi_entry
        = set_valid.load(std::memory_order_acquire) && selected >= 0
          && static_cast<size_t>(selected) < backing_set.count;
    if(ambiguous_shortcut)
        previous_midi_toggle_key = midi_key_down;
    else if(BackingMidiToggleOnPress(shift_down, midi_key_down,
                                     valid_midi_entry,
                                     previous_midi_toggle_key))
    {
        const bool enabled = !midi_output.Enabled();
        if(!midi_output.SetEnabled(enabled, timing.Active(), SendMidiByte))
            midi_error.store(true, std::memory_order_release);
        midi_clock_enabled_snapshot.store(enabled, std::memory_order_release);
    }
}

static void AudioCallback(AudioHandle::InputBuffer input,
                          AudioHandle::OutputBuffer output,
                          size_t size)
{
    ++audio_ticks;
    hardware.ProcessAllControls();
    SyncMidiEntry();
    PollControls();
    SyncMidiEntry();

    for(size_t frame = 0; frame < size; ++frame)
    {
        BackingTransport::State state = transport.GetState();
        if(state == BackingTransport::State::Error && timing_started)
        {
            if(!midi_output.Stop(SendMidiByte))
                midi_error.store(true, std::memory_order_release);
            timing.Stop();
            timing_started = false;
            resume_phase_valid = false;
        }
        if(state == BackingTransport::State::Playing && !timing_started)
        {
            const int index = SelectedSongIndex();
            timing.Start(backing_set.songs[index]);
            beat_seen.store(false, std::memory_order_release);
            timing_started = true;
            resume_phase_valid = false;
        }

        BackingTimingEvent timing_event = {};
        if(timing_started
           && (state == BackingTransport::State::Playing || timing.Active()))
            timing_event = timing.AdvanceFrame();
        if(state == BackingTransport::State::PendingResume
           && timing.Active() && resume_phase_valid
           && timing.CrossedBarPhase(resume_phase))
        {
            transport.ResumeAtBoundary();
            resume_phase_valid = false;
            if(!midi_output.Continue(SendMidiByte))
            {
                midi_output.Stop(SendMidiByte);
                transport.Fail();
                timing.Stop();
                timing_started = false;
                state = BackingTransport::State::Error;
            }
            state = transport.GetState();
        }
        if(!midi_output.Process(
               timing_event, state == BackingTransport::State::Playing,
               SendMidiByte))
        {
            midi_output.Stop(SendMidiByte);
            transport.Fail();
            timing.Stop();
            timing_started = false;
            resume_phase_valid = false;
            state = BackingTransport::State::Error;
        }
        if(timing_event.click)
        {
            metronome_click.Trigger(timing_event.downbeat);
            beat_downbeat.store(timing_event.downbeat, std::memory_order_relaxed);
            beat_at.store(System::GetNow(), std::memory_order_release);
            beat_seen.store(true, std::memory_order_release);
        }

        BackingStereoFrame track = {0.f, 0.f};
        if(state == BackingTransport::State::Playing)
        {
            const bool popped = playback_ring.Pop(track.left, track.right);
            transport.ConsumeFrame(popped);
            if(!popped)
            {
                transport.Fail();
                timing.Stop();
                timing_started = false;
                close_file_request.store(true, std::memory_order_release);
                if(!midi_output.Stop(SendMidiByte))
                    midi_error.store(true, std::memory_order_release);
                state = BackingTransport::State::Error;
            }
            if(popped && transport.ConsumedFrames() >= wave_info.frame_count
               && playback_ring.AvailableToRead() == 0)
            {
                if(!midi_output.Stop(SendMidiByte))
                    midi_error.store(true, std::memory_order_release);
                transport.Complete();
                timing.Stop();
                timing_started = false;
                finish_request.store(true, std::memory_order_release);
            }
        }

        const BackingStereoFrame line = {input[2][frame], input[3][frame]};
        const float click_sample = metronome_click.NextSample();
        const BackingStereoFrame click = {click_sample, click_sample};
        BackingMixGains mix_gains;
        mix_gains.track = gains[0].Next();
        mix_gains.line = gains[1].Next();
        mix_gains.click = gains[2].Next();
        mix_gains.headphones = gains[3].Next();
        mix_gains.main = gains[4].Next();
        const BackingQuadFrame mixed
            = BackingMixFrame(track, line, click, mix_gains);
        output[0][frame] = mixed.headphone_left;
        output[1][frame] = mixed.headphone_right;
        output[2][frame] = mixed.main_left;
        output[3][frame] = mixed.main_right;
        if(mixed.headphone_left == 1.f || mixed.headphone_left == -1.f
           || mixed.headphone_right == 1.f || mixed.headphone_right == -1.f
           || mixed.main_left == 1.f || mixed.main_left == -1.f
           || mixed.main_right == 1.f || mixed.main_right == -1.f)
            clipping.store(true, std::memory_order_relaxed);
    }
}

static void UpdateLeds()
{
    static uint32_t last_update = 0;
    const uint32_t now = System::GetNow();
    if(now - last_update < 16)
        return;
    last_update = now;

    for(int index = 0; index < 10; ++index)
        SetPthLed(index, 0, 0, 0);
    for(int i = 0; i < 25; ++i)
        SetSmtLed(i, 0, 0, 0);
    const bool blink = ((now / 250u) & 1u) != 0;
    const bool beat_blink = beat_seen.load(std::memory_order_acquire)
                            && BackingMetronomeBlinkVisible(
                                now, beat_at.load(std::memory_order_acquire));
    const bool valid_set = set_valid.load(std::memory_order_acquire);
    const int failed = failed_song.load(std::memory_order_acquire);
    const int current_page = active_page.load(std::memory_order_acquire);
    const int selected = SelectedSongIndex();
    const uint32_t editor_feedback
        = bpm_feedback.load(std::memory_order_acquire);
    const bool editor_active = (editor_feedback & 1u) != 0;
    const bool editor_saving = (editor_feedback & (1u << 1u)) != 0;
    const uint8_t editor_position
        = static_cast<uint8_t>((editor_feedback >> 2u) & 0x7u);
    const uint8_t editor_digit
        = static_cast<uint8_t>((editor_feedback >> 5u) & 0xfu);
    uint8_t editor_cue
        = static_cast<uint8_t>((editor_feedback >> 9u) & 0x3u);
    if(static_cast<int32_t>(
           bpm_feedback_until.load(std::memory_order_acquire) - now)
       <= 0)
        editor_cue = 0;
    for(int key = 0; key < kKeyCount; ++key)
    {
        if(editor_active && failed < 0)
            continue;
        const int index = current_page * kKeyCount + key;
        if(!valid_set || static_cast<size_t>(index) >= backing_set.count)
            continue;
        if(index == selected)
        {
            if(transport.GetState() == BackingTransport::State::Playing
               && beat_blink)
                continue;
            SetSmtLed(kWhiteKeyLeds[key], 0, 160, 255);
        }
        else
            SetSmtLed(kWhiteKeyLeds[key], 0, 80, 0);
    }

    const BackingTransport::State state = transport.GetState();
    const uint32_t levels = displayed_levels.load(std::memory_order_acquire);
    static const uint8_t level_leds[5] = {1, 2, 3, 4, 9};
    for(size_t control = 0; control < 5; ++control)
    {
        const uint8_t step = static_cast<uint8_t>((levels >> (control * 6u)) & 63u);
        const float brightness = BackingLevelBrightness(step);
        SetPthLedFloat(level_leds[control], brightness, brightness, brightness);
    }

    const uint32_t changed_at = level_changed_at.load(std::memory_order_acquire);
    const int control = displayed_control.load(std::memory_order_relaxed);
     if(!editor_active && control >= 0 && control < 5
         && BackingLevelVisible(now, changed_at))
    {
        const uint8_t step = static_cast<uint8_t>((levels >> (control * 6u)) & 63u);
        const uint8_t lit_keys = BackingLevelBar(step);
        uint8_t r = 255, g = 255, b = 255;
        switch(control)
        {
            case 0:
                r = 0; g = 160; b = 255; break;
            case 1:
                r = 255; g = 255; b = 0; break;
            case 2:
                r = 128; g = 0; b = 128; break;
            case 3:
                r = 0; g = 255; b = 0; break;
            default:
                r = 255; g = 255; b = 255; break;
        }
        for(int key = 0; key < kKeyCount; ++key)
        {
            if(key >= lit_keys)
                SetSmtLed(kWhiteKeyLeds[key], 0, 0, 0);
            else
                SetSmtLed(kWhiteKeyLeds[key], r, g, b);
        }
    }

    if(failed < 0 && editor_active)
    {
        if(editor_digit <= 9)
        {
            const int digit_key = BackingBpmWhiteKeyIndex(editor_digit);
            SetSmtLed(kWhiteKeyLeds[digit_key], 255, 255, 255);
        }
        if(editor_position < 5)
        {
            uint8_t red = 255;
            uint8_t green = 255;
            uint8_t blue = 255;
            if(editor_cue == 1)
            {
                green = 0;
                blue = 0;
            }
            else if(editor_saving)
            {
                green = 128;
                blue = 0;
            }
            SetSmtLed(kBpmSelectorLeds[editor_position], red, green, blue);
        }
    }
    else if(failed < 0 && editor_cue == 2 && editor_position < 5)
        SetSmtLed(kBpmSelectorLeds[editor_position], 0, 255, 0);
    else if(failed < 0 && editor_cue == 1 && editor_position < 5)
        SetSmtLed(kBpmSelectorLeds[editor_position], 255, 0, 0);

    SetPthLedFloat(kEncoder1Led, 0.f, 160.f / 255.f, 1.f);
    SetPthLedFloat(kEncoder2Led, 1.f, 1.f, 0.f);
    SetPthLedFloat(kEncoder3Led, .5f, 0.f, .5f);
    SetPthLedFloat(kEncoder4Led, 0.f, 1.f, 0.f);

    if(beat_seen.load(std::memory_order_acquire)
       && (state == BackingTransport::State::Playing
           || state == BackingTransport::State::Paused
           || state == BackingTransport::State::PendingResume)
       && BackingBeatVisible(now, beat_at.load(std::memory_order_acquire)))
    {
        if(beat_downbeat.load(std::memory_order_relaxed))
            SetPthLedFloat(5, 0.f, 1.f, 1.f);
        else
            SetPthLedFloat(6, 0.f, 1.f, 1.f);
    }
    if(BackingPlayLight(state, now))
        SetPthLedFloat(7, 0.f, 1.f, 0.f);
    if(BackingStopLight(state))
        SetPthLedFloat(kLoopLed, 1.f, 0.f, 0.f);

    if(failed >= 0)
    {
        for(int key = 0; key < kKeyCount; ++key)
        {
            const int index = current_page * kKeyCount + key;
            if(index == failed)
                SetSmtLed(kWhiteKeyLeds[key], blink ? 255 : 0, 0, 0);
            else if(!valid_set)
                SetSmtLed(kWhiteKeyLeds[key], 0, 0, 0);
        }
    }

    if(!editor_active && valid_set && selected >= 0
       && static_cast<size_t>(selected) < backing_set.count)
    {
        if(midi_clock_enabled_snapshot.load(std::memory_order_acquire))
            SetSmtLed(kMidiClockLed, 0, 255, 0);
        else
            SetSmtLed(kMidiClockLed, 255, 0, 0);

        switch(static_cast<BackingAdvance>(
            advance_mode_snapshot.load(std::memory_order_acquire)))
        {
            case BackingAdvance::Wait:
                SetSmtLed(kAdvanceModeLed, 255, 0, 0);
                break;
            case BackingAdvance::Auto:
                SetSmtLed(kAdvanceModeLed, 0, 0, 255);
                break;
            case BackingAdvance::Loop:
                SetSmtLed(kAdvanceModeLed, 255, 255, 0);
                break;
        }
    }

    const bool clipped = clipping.exchange(false, std::memory_order_acq_rel);
    if(storage_error.load(std::memory_order_acquire)
       || midi_error.load(std::memory_order_acquire)
       || !set_valid.load(std::memory_order_acquire)
       || transport.Underrun())
        SetPthLedFloat(0, 1.f, 0.f, 0.f);
    else if(clipped || settings_dirty.load(std::memory_order_acquire)
            || settings_error.load(std::memory_order_acquire)
            || settings_missing.load(std::memory_order_acquire))
        SetPthLedFloat(0, 1.f, .5f, 0.f);
    else if(state == BackingTransport::State::Playing)
        SetPthLedFloat(0, 0.f, 1.f, 0.f);
    else if(state == BackingTransport::State::Paused
            || state == BackingTransport::State::PendingResume)
        SetPthLedFloat(0, 1.f, .5f, 0.f);
    else
        SetPthLedFloat(0, 0.f, 0.f, .2f);
    fill_led_data();
}

static void MainLoop()
{
    static uint32_t previous_audio_ticks = 0;
    if(audio_ticks != previous_audio_ticks)
    {
        previous_audio_ticks = audio_ticks;
        IWDG1->KR = 0x0000AAAA;
    }
    ServiceCardPresence();
    ServiceSetReload();
    ServiceAdvanceEdit();
    ServicePlayback();
    ServiceSettings();
    ServiceBpmEdit();
    UpdateLeds();
    LedChainWatchdog();
    System::DelayUs(10);
}

int main(void)
{
    hardware.Init();
    MidiUartHandler::Config midi_config;
    midi_uart.Init(midi_config);

    MidiUsbHandler::Config usb_midi_config;
    usb_midi_config.transport_config.periph = MidiUsbTransport::Config::EXTERNAL;
    usb_midi_config.transport_config.tx_retry_count = 0;
    usb_midi.Init(usb_midi_config);
    usb_midi.Listen();

    hardware.MpWrite(0x0c, 0B01010001);
    hardware.MpReadAll();
    for(size_t i = 0; i < 10; ++i)
    {
        hardware.LowBatteryLockoutCheck();
        System::Delay(10);
    }

    SdmmcHandler::Config sd_config;
    sd_config.speed = SdmmcHandler::Speed::FAST;
    sd_config.width = SdmmcHandler::BusWidth::BITS_4;
    sdmmc.Init(sd_config);
    fatfs.Init(FatFSInterface::Config::MEDIA_SD);
    f_mount(&fatfs.GetSDFileSystem(), fatfs.GetSDPath(), 1);
    if(disk_status(0) == RES_OK)
        f_mkdir("BACKING-TRACKS");
    set_valid.store(LoadSetFromCard() && PreflightSet(),
                    std::memory_order_release);
    bpm_set_generation.fetch_add(1, std::memory_order_acq_rel);
    card_present.store(disk_status(0) == RES_OK, std::memory_order_release);
    if(disk_status(0) == RES_OK)
        LoadSettings();
    if(set_valid.load(std::memory_order_acquire))
        PublishSelectedSong(0, true);
    else
        storage_error.store(true, std::memory_order_release);

    playback_ring.Init(playback_samples, kPlaybackRingFrames);
    for(size_t i = 0; i < 5; ++i)
        gains[i].Init(level_steps[i]);

    LedSetup();
    uint32_t sleep_state = 0;
    for(int i = 0; i < 5000; ++i)
    {
        hardware.ProcessAllControls();
        sleep_state += hardware.button_sr.State(
                           static_cast<int>(Hardware::SwId::KEY_26))
                       && hardware.button_sr.State(
                           static_cast<int>(Hardware::SwId::KEY_27))
                       && hardware.button_sr.State(
                           static_cast<int>(Hardware::SwId::KEY_28));
        System::DelayUs(100);
    }
    if(sleep_state > 4000)
        hardware.MpWrite(0x08, 0B10111111);

    for(int key = 0; key < kKeyCount; ++key)
        previous_keys[key] = hardware.button_sr.State(kWhiteKeys[key]);
    previous_midi_toggle_key
        = hardware.button_sr.State(kMidiClockToggleKey);
    previous_advance_toggle_key
        = hardware.button_sr.State(kAdvanceToggleKey);
    previous_play = hardware.button_sr.State(
        static_cast<int>(Hardware::SwId::KEY_27));
    previous_stop = hardware.button_sr.State(
        static_cast<int>(Hardware::SwId::KEY_28));
    BackingBpmEditorInput initial_editor_input = {};
    initial_editor_input.shift_down
        = hardware.button_sr.State(kMidiClockModifierKey);
    initial_editor_input.stopped
        = transport.GetState() == BackingTransport::State::Stopped;
    const int initial_selected_song = SelectedSongIndex();
    initial_editor_input.valid_entry
        = set_valid.load(std::memory_order_acquire)
          && initial_selected_song >= 0
          && static_cast<size_t>(initial_selected_song) < backing_set.count;
    initial_editor_input.clean_entry_chord
        = !hardware.button_sr.State(kMidiClockToggleKey)
          && !hardware.button_sr.State(kAdvanceToggleKey);
    for(size_t selector = 0; selector < 5; ++selector)
        initial_editor_input.selector_down[selector]
            = hardware.button_sr.State(kBpmSelectorKeys[selector]);
    for(size_t key = 0; key < 10; ++key)
        initial_editor_input.white_down[key]
            = hardware.button_sr.State(kWhiteKeys[key]);
    bpm_editor.Initialize(initial_editor_input);

    hardware.StartAudio(AudioCallback);
    IWDG1->KR = 0x0000CCCC;
    IWDG1->KR = 0x00005555;
    IWDG1->PR = 4;
    IWDG1->RLR = 1000;
    while(IWDG1->SR != 0) {}
    IWDG1->KR = 0x0000AAAA;

    hardware.usb_sw.Write(false);
    System::Delay(1);
    hardware.MpWrite(0x0a, 0B00100100);
    System::Delay(1);
    hardware.usb_sw.Write(true);

    while(1)
        MainLoop();
}