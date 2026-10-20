#include "BackingMixer.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace chompi;

static bool Near(float actual, float expected)
{
    return std::fabs(actual - expected) < 1e-6f;
}

int main()
{
    BackingMixGains gains;
    gains.track = 0.f;
    gains.line = 0.f;
    gains.click = 1.f;
    gains.headphones = .5f;
    gains.main = 1.f;

    const BackingStereoFrame silence = {0.f, 0.f};
    const BackingStereoFrame click = {.4f, -.6f};
    BackingQuadFrame output = BackingMixFrame(silence, silence, click, gains);
    assert(Near(output.headphone_left, .2f));
    assert(Near(output.headphone_right, -.3f));
    assert(output.main_left == 0.f);
    assert(output.main_right == 0.f);

    gains.track = .5f;
    gains.line = .25f;
    gains.click = 0.f;
    gains.headphones = 1.f;
    gains.main = .5f;
    const BackingStereoFrame track = {.8f, -.4f};
    const BackingStereoFrame line = {-.4f, .8f};
    output = BackingMixFrame(track, line, click, gains);
    assert(Near(output.headphone_left, .3f));
    assert(Near(output.headphone_right, 0.f));
    assert(Near(output.main_left, .15f));
    assert(Near(output.main_right, 0.f));

    gains.track = 1.f;
    gains.line = 1.f;
    gains.main = 1.f;
    const BackingStereoFrame hot = {2.f, -2.f};
    output = BackingMixFrame(hot, hot, silence, gains);
    assert(output.main_left == 1.f);
    assert(output.main_right == -1.f);

    std::puts("backing_mixer_test: all assertions passed");
    return 0;
}