#include "PitchKey.h"

#include <cassert>

int main()
{
    using fengyin::PitchKey::transposeFromTo;
    assert(transposeFromTo(0, 0) == 0);   // C -> C
    assert(transposeFromTo(3, 9) == 6);   // 降 E 吹管 -> A 调
    assert(transposeFromTo(10, 0) == 2);  // 降 B 吹管 -> C 调
    assert(transposeFromTo(0, 11) == -1); // C 吹管 -> B 调
    assert(transposeFromTo(9, 3) == -6);  // A 吹管 -> 降 E 调
    return 0;
}
