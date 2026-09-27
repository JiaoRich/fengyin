#pragma once
#include "SwamToneProfile.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <cmath>
#include <cstdlib>

namespace fengyin
{
// Message-thread only. Resolve through the plugin's own display conversion;
// never interpret a physical target as a normalised VST value.
struct SwamToneApplication
{
    static juce::String key(juce::String name)
    { return name.toLowerCase().removeCharacters(" ._-/\r\n").trim(); }

    static std::optional<double> number(const juce::String& text)
    {
        const auto value = text.trim();
        const auto* start = value.toRawUTF8();
        char* end = nullptr;
        const double n = std::strtod(start, &end);
        if (end == start || ! std::isfinite(n)) return {};
        const auto unit = juce::String(end).trim().toLowerCase();
        if (unit.isNotEmpty() && unit != "db" && unit != "hz" && unit != "ms"
            && unit != "s" && unit != "%") return {};
        return n;
    }
    static bool matches(const juce::String& actual, const juce::String& target)
    {
        if (actual.trim().equalsIgnoreCase(target.trim())) return true;
        const auto a = number(actual), b = number(target);
        return a && b && std::abs(*a - *b) <= 0.0051;
    }
    static std::optional<float> resolve(juce::AudioProcessorParameter& p, const juce::String& target)
    {
        const auto valid = [&](float n) { return std::isfinite(n) && n >= 0 && n <= 1
            && matches(p.getText(n, 256), target); };
        const float converted = p.getValueForText(target);
        if (valid(converted)) return converted;
        const auto wanted = number(target), low = number(p.getText(0, 256)), high = number(p.getText(1, 256));
        if (wanted && low && high && std::abs(*low - *high) > 1.0e-12)
        {
            if (*wanted < std::min(*low, *high) || *wanted > std::max(*low, *high)) return {};
            float left = 0, right = 1;
            for (int i = 0; i < 32; ++i)
            {
                const float mid = (left + right) * 0.5f;
                if (valid(mid)) return mid;
                const auto displayed = number(p.getText(mid, 256));
                if (! displayed) break;
                if ((*displayed < *wanted) == (*low < *high)) left = mid; else right = mid;
            }
        }
        // Enumerations / wrappers without text parsing; only accept a display match.
        const int steps = juce::jlimit(2, 4097, p.getNumSteps());
        for (int i = 0; i < steps; ++i)
        {
            const float n = static_cast<float>(i) / static_cast<float>(steps - 1);
            if (valid(n)) return n;
        }
        return {};
    }
    struct Entry
    {
        juce::String name, target, status, actual;
        juce::AudioProcessorParameter* parameter = nullptr;
        float before = 0, desired = 0;
    };
    std::vector<Entry> entries;
    int verified = 0;
    bool rolledBack = false;

    int apply(const juce::Array<juce::AudioProcessorParameter*>& parameters, const SwamToneProfile& profile)
    {
        entries.clear(); verified = 0; rolledBack = false;
        bool ready = true;
        for (const auto& target : profile.displayTargets)
        {
            Entry e; e.name = target.name; e.target = target.display;
            int count = 0;
            for (auto* p : parameters)
                if (p != nullptr && key(p->getName(256)) == key(e.name)) { e.parameter = p; ++count; }
            if (count != 1) { e.status = count == 0 ? "missing" : "ambiguous"; ready = false; }
            else if (auto value = resolve(*e.parameter, e.target))
            { e.desired = *value; e.before = e.parameter->getValue(); e.status = "resolved"; }
            else { e.status = "unrepresentable"; ready = false; }
            entries.push_back(e);
        }
        if (! ready) return 0; // No partial profile on an incompatible version.
        for (auto& e : entries)
        {
            // Avoid retriggering reload-sensitive switches that are already correct.
            if (! matches(e.parameter->getCurrentValueAsText(), e.target))
            {
                e.parameter->beginChangeGesture();
                e.parameter->setValueNotifyingHost(e.desired);
                e.parameter->endChangeGesture();
            }
        }
        for (auto& e : entries)
        {
            e.actual = e.parameter->getCurrentValueAsText();
            e.status = matches(e.actual, e.target) ? "verified" : "readback-mismatch";
            if (e.status == "verified") ++verified;
        }
        if (verified != static_cast<int>(entries.size()))
        {
            for (auto& e : entries)
            {
                e.parameter->beginChangeGesture(); e.parameter->setValueNotifyingHost(e.before);
                e.parameter->endChangeGesture();
            }
            rolledBack = true; verified = 0;
        }
        return verified;
    }
    juce::var diagnostic() const
    {
        auto* result = new juce::DynamicObject();
        result->setProperty("expected", static_cast<int>(entries.size()));
        result->setProperty("verifiedAtApply", verified);
        result->setProperty("rolledBack", rolledBack);
        juce::Array<juce::var> rows;
        for (const auto& e : entries)
        {
            auto* row = new juce::DynamicObject();
            row->setProperty("name", e.name); row->setProperty("targetDisplay", e.target);
            row->setProperty("statusAtApply", e.status); row->setProperty("displayAtApply", e.actual);
            rows.add(juce::var(row));
        }
        result->setProperty("parameters", rows);
        return juce::var(result);
    }
};
}
