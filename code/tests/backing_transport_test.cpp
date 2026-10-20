#include "BackingTransport.h"
#include "BackingFeedback.h"

#include <cassert>
#include <cstdio>

int main()
{
    int16_t storage[8] = {};
    chompi::BackingRing ring;
    assert(ring.Init(storage, 4));
    const int16_t samples[] = {32767, -32768, 16384, -16384, 0, 1, 2, 3};
    assert(ring.Push(samples, 4) == 4);
    assert(ring.AvailableToRead() == 4);
    assert(ring.AvailableToWrite() == 0);
    float left = 0.f;
    float right = 0.f;
    assert(ring.Pop(left, right));
    assert(left > .99f && right == -1.f);
    assert(ring.Push(samples, 1) == 1);
    assert(ring.AvailableToRead() == 4);
    ring.Reset();
    assert(ring.AvailableToRead() == 0);

    chompi::BackingTransport transport;
    transport.BeginLoad(2);
    assert(transport.GetState() == chompi::BackingTransport::State::Loading);
    transport.Play();
    assert(transport.GetState() == chompi::BackingTransport::State::Loading);
    transport.SetReady();
    transport.Play();
    transport.ConsumeFrame(true);
    transport.Pause();
    const uint64_t frozen = transport.ConsumedFrames();
    transport.ConsumeFrame(true);
    assert(transport.ConsumedFrames() == frozen);
    transport.Play();
    assert(transport.GetState() == chompi::BackingTransport::State::PendingResume);
    transport.Play();
    assert(transport.GetState() == chompi::BackingTransport::State::Paused);
    transport.Play();
    transport.ResumeAtBoundary();
    assert(transport.GetState() == chompi::BackingTransport::State::Playing);
    transport.Stop();
    assert(transport.GetState() == chompi::BackingTransport::State::Stopped);
    assert(transport.ConsumedFrames() == 0);

        assert(chompi::BackingLevelBar(0) == 0);
        assert(chompi::BackingLevelBar(1) == 1);
        assert(chompi::BackingLevelBar(16) == 8);
        assert(chompi::BackingLevelBar(32) == 15);
        assert(chompi::BackingLevelBrightness(0) == 0.f);
        assert(chompi::BackingLevelBrightness(16) == .5f);
        assert(chompi::BackingLevelBrightness(32) == 1.f);
        for(uint8_t step = 1; step <= 32; ++step)
        {
         assert(chompi::BackingLevelBar(step)
             >= chompi::BackingLevelBar(step - 1));
         assert(chompi::BackingLevelBrightness(step)
             > chompi::BackingLevelBrightness(step - 1));
        }
        assert(chompi::BackingLevelVisible(2499, 1000));
        assert(!chompi::BackingLevelVisible(2500, 1000));
        assert(chompi::BackingLevelVisible(20, UINT32_MAX - 10));
        assert(chompi::BackingBeatVisible(1079, 1000));
        assert(!chompi::BackingBeatVisible(1080, 1000));
        assert(chompi::BackingBeatVisible(20, UINT32_MAX - 10));
        using State = chompi::BackingTransport::State;
        const State states[] = {State::Stopped, State::Loading, State::Ready,
                    State::Playing, State::Paused,
                    State::PendingResume, State::Error};
        for(State state : states)
        {
         assert(chompi::BackingStopLight(state) == (state == State::Stopped));
         const bool paused = state == State::Paused
                    || state == State::PendingResume;
         assert(chompi::BackingPlayLight(state, 0)
             == (paused || state == State::Playing));
         assert(chompi::BackingPlayLight(state, 250) == paused);
         assert(chompi::BackingPlayLight(state, 500)
             == (paused || state == State::Playing));
        }

    std::puts("backing_transport_test: all assertions passed");
    return 0;
}