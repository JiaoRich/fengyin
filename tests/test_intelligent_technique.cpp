#include "IntelligentTechniqueProcessor.h"

#include <cassert>

int main()
{
    using namespace fengyin;
    IntelligentTechniqueProcessor processor;
    processor.noteStarted(0.0);

    // 标准档固定在 89% 进入、80% 退出；效果强度不改变触发阈值。
    assert(processor.process(PerformanceTechnique::growl, 1.0f, 0.5f, 20.0) == 0.0f);
    const auto entered = processor.process(PerformanceTechnique::growl, 1.0f, 0.5f, 105.0);
    assert(entered > 0.0f && entered < 0.73f);

    // 回落阈值低于进入阈值，避免在边界附近忽隐忽现。
    const auto held = processor.process(PerformanceTechnique::growl, 0.85f, 0.5f, 260.0);
    assert(held > 0.0f);
    const auto releasing = processor.process(PerformanceTechnique::growl, 0.70f, 0.5f, 400.0);
    assert(releasing < held);

    processor.reset();
    processor.setGrowlThresholds(0.94f, 0.86f);
    processor.noteStarted(500.0);
    assert(processor.process(PerformanceTechnique::growl, 0.90f, 1.0f, 600.0) == 0.0f);

    processor.noteEnded();
    const auto off = processor.process(PerformanceTechnique::growl, 1.0f, 0.5f, 600.0);
    assert(off < releasing);

    processor.reset();
    processor.noteStarted(1000.0);
    const auto earlyVibrato = processor.process(PerformanceTechnique::vibrato, 0.7f, 0.5f, 1200.0);
    auto lateVibrato = 0.0f;
    for (int step = 0; step < 50; ++step)
        lateVibrato = processor.process(PerformanceTechnique::vibrato, 0.7f, 0.5f, 1450.0 + step * 20.0);
    assert(earlyVibrato < 0.02f);
    assert(lateVibrato > 0.12f);
    processor.noteEnded();
    assert(processor.process(PerformanceTechnique::vibrato, 0.7f, 0.5f, 2600.0) < lateVibrato);

    assert(IntelligentTechniqueProcessor::canUseBreath(PerformanceTechnique::vibrato));
    assert(IntelligentTechniqueProcessor::canUseBreath(PerformanceTechnique::bowPressure));
    assert(! IntelligentTechniqueProcessor::canUseBreath(PerformanceTechnique::pizzicato));
    return 0;
}
