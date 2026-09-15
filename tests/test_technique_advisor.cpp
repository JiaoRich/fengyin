#include "../native/Source/TechniqueAdvisor.h"
#include <cassert>
#include <string>

int main()
{
    using namespace fengyin;
    const auto yds = DeviceProfileMatcher::match("Yamaha YDS-150");
    const auto aerophone = DeviceProfileMatcher::match("Roland Aerophone AE-30");

    const auto ydsVibrato = TechniqueAdvisor::advise(yds, SwamFamily::saxophone, PerformanceTechnique::vibrato);
    assert(ydsVibrato.relevantToInstrument);
    assert(ydsVibrato.hardwareAvailable);
    assert(std::string(ydsVibrato.recommendedSource) != "吹嘴咬合");

    const auto rolandVibrato = TechniqueAdvisor::advise(aerophone, SwamFamily::saxophone, PerformanceTechnique::vibrato);
    assert(std::string(rolandVibrato.recommendedSource) == "吹嘴咬合");

    const auto stringGrowl = TechniqueAdvisor::advise(yds, SwamFamily::strings, PerformanceTechnique::growl);
    assert(! stringGrowl.relevantToInstrument);
    const auto stringFlutter = TechniqueAdvisor::advise(yds, SwamFamily::strings, PerformanceTechnique::flutter);
    assert(! stringFlutter.relevantToInstrument);
    const auto stringVibrato = TechniqueAdvisor::advise(yds, SwamFamily::strings, PerformanceTechnique::vibrato);
    assert(stringVibrato.relevantToInstrument);
}
