#include "RealtimeMidiQueue.h"
#include <cassert>
#include <cmath>

int main()
{
    fengyin::RealtimeMidiQueue<4> queue;
    const std::uint8_t noteOn[] { 0x90, 60, 100 };
    const std::uint8_t breath[] { 0xb0, 11, 72 };
    assert(queue.push(noteOn, 3, 10.125));
    assert(queue.push(breath, 3, 10.250));

    fengyin::RealtimeMidiEvent event;
    assert(queue.pop(event));
    assert(event.size == 3 && event.bytes[1] == 60);
    assert(std::abs(event.timestampSeconds - 10.125) < 0.000001);
    assert(queue.pop(event));
    assert(event.bytes[1] == 11);
    assert(std::abs(event.timestampSeconds - 10.250) < 0.000001);
    assert(! queue.pop(event));

    assert(queue.push(noteOn, 3, 1.0));
    assert(queue.push(noteOn, 3, 2.0));
    assert(queue.push(noteOn, 3, 3.0));
    assert(! queue.push(noteOn, 3, 4.0));
    assert(queue.droppedCount() == 1);
    return 0;
}
