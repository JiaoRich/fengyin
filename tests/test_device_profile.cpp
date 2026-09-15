#include "../native/Source/DeviceProfile.h"
#include <cassert>

int main()
{
    using fengyin::DeviceProfileMatcher;
    assert(DeviceProfileMatcher::match("Roland Aerophone AE-30").id == "roland-aerophone");
    assert(DeviceProfileMatcher::match("YAMAHA YDS-150").id == "yamaha-yds");
    const auto yds = DeviceProfileMatcher::match("YAMAHA YDS-150");
    assert(yds.breathController == 11);
    assert(! yds.hasBiteSensor);
    assert(yds.hasThumbController);
    assert(yds.hasAssignableButtons);
    assert(DeviceProfileMatcher::match("Akai EWI USB").id == "akai-ewi");
    assert(DeviceProfileMatcher::match("Aodyo Sylphyo Link").id == "aodyo-sylphyo");
    const auto generic = DeviceProfileMatcher::match("USB MIDI Device");
    assert(generic.id == "generic-wind-controller");
    assert(generic.breathController == 2);
}
