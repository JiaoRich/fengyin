#pragma once
#include <cstdint>
namespace fengyin {
constexpr int recordingMaxSeconds=30*60;
constexpr bool recordingLimitReached(std::int64_t elapsed100ns,int seconds=recordingMaxSeconds){
    return elapsed100ns>=std::int64_t(seconds)*10000000;
}
}
