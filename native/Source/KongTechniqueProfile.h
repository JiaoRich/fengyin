#pragma once
namespace fengyin {
// Portable, explicit routing captured with the tone, not a guessed first match
// in a sampler's articulation list. Legacy tones leave enabled=false.
struct KongTechniqueProfile {
    bool enabled=false;
    int vibratoCC=-1; // Only Qin's verified native CC1 route is accepted.
    int otherChannel=0;
    int otherKeyswitchIndex=-1;
    bool operator==(const KongTechniqueProfile&) const = default;
};
}
