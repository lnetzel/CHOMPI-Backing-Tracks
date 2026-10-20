#pragma once

#include <atomic>
#include <stddef.h>
#include <stdint.h>

namespace chompi
{

inline float BackingS16ToFloat(int16_t sample)
{
    return static_cast<float>(sample) * (1.f / 32768.f);
}

class BackingRing
{
  public:
    bool Init(int16_t *storage, uint32_t capacity_frames)
    {
        if(storage == NULL || capacity_frames < 2
           || (capacity_frames & (capacity_frames - 1u)) != 0)
            return false;
        storage_ = storage;
        capacity_frames_ = capacity_frames;
        mask_ = capacity_frames - 1u;
        read_frame_.store(0, std::memory_order_relaxed);
        write_frame_.store(0, std::memory_order_relaxed);
        return true;
    }

    uint32_t AvailableToRead() const
    {
        const uint32_t write = write_frame_.load(std::memory_order_acquire);
        const uint32_t read = read_frame_.load(std::memory_order_relaxed);
        return write - read;
    }

    uint32_t AvailableToWrite() const
    {
        const uint32_t write = write_frame_.load(std::memory_order_relaxed);
        const uint32_t read = read_frame_.load(std::memory_order_acquire);
        return capacity_frames_ - (write - read);
    }

    uint32_t Push(const int16_t *interleaved, uint32_t frames)
    {
        uint32_t write = write_frame_.load(std::memory_order_relaxed);
        const uint32_t read = read_frame_.load(std::memory_order_acquire);
        uint32_t count = capacity_frames_ - (write - read);
        if(count > frames)
            count = frames;
        for(uint32_t frame = 0; frame < count; ++frame)
        {
            const uint32_t index = ((write + frame) & mask_) * 2u;
            storage_[index] = interleaved[frame * 2u];
            storage_[index + 1u] = interleaved[frame * 2u + 1u];
        }
        write += count;
        write_frame_.store(write, std::memory_order_release);
        return count;
    }

    bool Pop(float &left, float &right)
    {
        const uint32_t read = read_frame_.load(std::memory_order_relaxed);
        const uint32_t write = write_frame_.load(std::memory_order_acquire);
        if(read == write)
            return false;
        const uint32_t index = (read & mask_) * 2u;
        left = BackingS16ToFloat(storage_[index]);
        right = BackingS16ToFloat(storage_[index + 1u]);
        read_frame_.store(read + 1u, std::memory_order_release);
        return true;
    }

    void Reset()
    {
        read_frame_.store(0, std::memory_order_relaxed);
        write_frame_.store(0, std::memory_order_relaxed);
    }

  private:
    int16_t *storage_ = NULL;
    uint32_t capacity_frames_ = 0;
    uint32_t mask_ = 0;
    alignas(4) std::atomic<uint32_t> read_frame_{0};
    alignas(4) std::atomic<uint32_t> write_frame_{0};
};

class BackingTransport
{
  public:
    enum class State : uint8_t
    {
        Stopped,
        Loading,
        Ready,
        Playing,
        Paused,
        PendingResume,
        Error
    };

    void BeginLoad(int song)
    {
        selected_song_.store(song, std::memory_order_relaxed);
        consumed_frames_.store(0, std::memory_order_relaxed);
        underrun_.store(false, std::memory_order_relaxed);
        state_.store(State::Loading, std::memory_order_release);
    }

    void SetReady()
    {
        State expected = State::Loading;
        state_.compare_exchange_strong(expected, State::Ready,
                                       std::memory_order_release,
                                       std::memory_order_relaxed);
    }

    void Play()
    {
        State current = state_.load(std::memory_order_relaxed);
        if(current == State::Ready)
            state_.store(State::Playing, std::memory_order_release);
        else if(current == State::Paused)
            state_.store(State::PendingResume, std::memory_order_release);
        else if(current == State::PendingResume)
            state_.store(State::Paused, std::memory_order_release);
    }

    void Pause()
    {
        State current = state_.load(std::memory_order_relaxed);
        if(current == State::Playing || current == State::PendingResume)
            state_.store(State::Paused, std::memory_order_release);
    }

    void ResumeAtBoundary()
    {
        State expected = State::PendingResume;
        state_.compare_exchange_strong(expected, State::Playing,
                                       std::memory_order_release,
                                       std::memory_order_relaxed);
    }

    void Stop()
    {
        state_.store(State::Stopped, std::memory_order_release);
        consumed_frames_.store(0, std::memory_order_relaxed);
        underrun_.store(false, std::memory_order_relaxed);
    }

    void Complete()
    {
        state_.store(State::Stopped, std::memory_order_release);
    }

    void Fail() { state_.store(State::Error, std::memory_order_release); }

    void ConsumeFrame(bool available)
    {
        if(state_.load(std::memory_order_acquire) != State::Playing)
            return;
        if(available)
            consumed_frames_.fetch_add(1, std::memory_order_relaxed);
        else
            underrun_.store(true, std::memory_order_relaxed);
    }

    State GetState() const { return state_.load(std::memory_order_acquire); }
    int SelectedSong() const
    {
        return selected_song_.load(std::memory_order_relaxed);
    }
    uint32_t ConsumedFrames() const
    {
        return consumed_frames_.load(std::memory_order_relaxed);
    }
    bool Underrun() const { return underrun_.load(std::memory_order_relaxed); }

  private:
    std::atomic<State> state_{State::Stopped};
    std::atomic<int> selected_song_{-1};
    std::atomic<uint32_t> consumed_frames_{0};
    std::atomic<bool> underrun_{false};
};

} // namespace chompi