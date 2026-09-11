#include "../native/Source/ControllerDetector.h"
#include <cassert>

int main()
{
    fengyin::ControllerDetector detector;
    assert(detector.bestContinuousController() == -1);

    for (const auto value : { 0, 15, 40, 72, 104, 127 })
        detector.observe(2, value);
    detector.observe(64, 0);
    detector.observe(64, 127);
    detector.observe(64, 0);
    detector.observe(64, 127);
    detector.observe(64, 0);
    assert(detector.bestContinuousController() == 2);

    detector.reset();
    for (const auto value : { 30, 32, 31, 33, 32 })
        detector.observe(11, value);
    assert(detector.bestContinuousController() == -1);
}

