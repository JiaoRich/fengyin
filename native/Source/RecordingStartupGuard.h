#pragma once
namespace fengyin {
// A startup deadline belongs to one handshake, not the lifetime of a recording.
class RecordingStartupGuard {
public:
    void reset() noexcept { acknowledged=false; }
    void observe(bool expectedState) noexcept { acknowledged |= expectedState; }
    bool timedOut(double elapsedMs, bool finishing, bool terminal, bool alive) const noexcept {
        return !acknowledged && !finishing && !terminal && alive && elapsedMs>20000;
    }
private:
    bool acknowledged=false;
};
}
