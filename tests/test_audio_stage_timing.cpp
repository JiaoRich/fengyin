#include "../native/Source/AudioStageTiming.h"
#include "../native/Source/AudioCallbackTrace.h"
#include <cassert>
int main() {
    using namespace fengyin::audioTrace;
    Accumulator detailed;int instrument=0;detailed.instrument.store(&instrument);
    detailed.record(Event::pluginCall,&instrument,1234);
    auto sink=detailed.sink();
    assert(current==nullptr);
    {
        Session session(&sink);assert(current==&sink);
        {Session disabled(nullptr);Scope noTrace(Event::pluginCall);assert(current==nullptr);}
        assert(current==&sink);
        Scope scope(Event::techniques);scope.finish();scope.finish();
    }
    assert(current==nullptr);
    const auto snapshot=detailed.take();
    assert(snapshot[static_cast<unsigned>(Event::pluginCall)*3+1].calls==1);
    assert(snapshot[static_cast<unsigned>(Event::pluginCall)*3+1].totalNs==1234);
    assert(snapshot[static_cast<unsigned>(Event::techniques)*3].calls==1);
    for(const auto& entry:detailed.take())assert(entry.calls==0&&entry.totalNs==0&&entry.maxNs==0);
    Cadence cadence;cadence.restart();
    cadence.record(1000000,128);cadence.record(3666667,256);cadence.record(9000000,128);
    const auto intervals=cadence.take();assert(intervals.calls==3&&intervals.minFrames==128&&intervals.maxFrames==256);
    assert(intervals.maxGapNs==5333333);
    cadence.restart();cadence.record(1000000000,256);
    assert(cadence.take().maxGapNs==0 && cadence.epoch==2);
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
