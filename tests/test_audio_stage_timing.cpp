#include "../native/Source/AudioStageTiming.h"
#include <cassert>
int main() {
    fengyin::AudioStageTiming timing;
    timing.record(fengyin::AudioStageTiming::graph,.001,.002);
    timing.record(fengyin::AudioStageTiming::graph,.003,.002);
    timing.record(fengyin::AudioStageTiming::master,-1,.002);
    const auto result=timing.take();
    assert(result[0].blocks==2 && result[0].totalUs==4000);
    assert(result[0].maxUs==3000 && result[0].overBudget==1);
    assert(result[1].blocks==0 && result[3].blocks==1 && result[3].totalUs==0);
    const auto cleared=timing.take();
    for(const auto& value:cleared)assert(value.blocks==0 && value.totalUs==0 && value.maxUs==0 && value.overBudget==0);
}
