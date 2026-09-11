#include "../native/Source/BreathMapper.h"
#include <cassert>
#include <cmath>

int main()
{
    fengyin::BreathMapper mapper;
    mapper.setSettings({ 0.05f, 1.0f, 1.0f });

    assert(mapper.processMidiValue(0) == 0.0f);
    assert(mapper.processMidiValue(4) == 0.0f);
    const auto maximum = mapper.processMidiValue(127);
    assert(std::abs(maximum - 1.0f) < 0.0001f);

    mapper.reset();
    mapper.setSettings({ 0.0f, 1.0f, 0.25f });
    const auto first = mapper.processMidiValue(127);
    const auto second = mapper.processMidiValue(127);
    assert(first > 0.0f && first < second && second < 1.0f);

    mapper.setSettings({ -1.0f, 10.0f, 3.0f });
    const auto clamped = mapper.getSettings();
    assert(clamped.threshold == 0.0f);
    assert(clamped.curve == 4.0f);
    assert(clamped.smoothing == 1.0f);
}

