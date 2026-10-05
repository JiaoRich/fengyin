#include "Tone.h"
#include <cassert>
#include <algorithm>
int main()
{
    double peak = 0;
    for (unsigned i = 0; i < 97000; ++i) { peak = std::max(peak, std::abs(tone::sample(i))); if (i >= 96000) assert(tone::sample(i) == 0); }
    assert(peak > 0.03 && peak < 0.032);
    unsigned char data[10] {};
    tone::encode(data, 16, 0.5); assert(data[0] == 0 && data[1] == 64);
    tone::encode(data, 0, 0.5); assert(data[0] == 64 && data[1] == 0);
    tone::encode(data, 18, -0.5); assert(data[3] == 0xc0);
    tone::encode(data, 19, 0.5); float f; std::memcpy(&f, data, 4); assert(f == 0.5f);
    tone::encode(data, 20, 0.5); double d; std::memcpy(&d, data, 8); assert(d == 0.5);
    for (int t : {0,1,2,3,4,8,9,10,11,16,17,18,19,20,24,25,26,27})
    { std::memset(data, 0xaa, 10); tone::encode(data, t, 0); for(int i=0;i<tone::bytes(t);++i) assert(data[i]==0); assert(data[tone::bytes(t)]==0xaa); }
    assert(tone::bytes(32) == 0);
}
