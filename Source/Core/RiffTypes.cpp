#include "RiffTypes.h"
#include <algorithm>

namespace foundry
{

bool RiffSketch::isEmpty() const noexcept
{
    for (const auto& s : slots)
        if (! s.isEmpty())
            return false;
    return true;
}

juce::var RiffSketch::toVar() const
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("format", "foundry-riff");
    root->setProperty ("version", 1);
    root->setProperty ("grid", gridSteps);
    root->setProperty ("seed", juce::String (lastSeed));

    juce::Array<juce::var> arr;
    for (const auto& s : slots)
    {
        auto o = juce::DynamicObject::Ptr (new juce::DynamicObject());
        o->setProperty ("path", s.path);
        o->setProperty ("offset", s.offsetSeconds);
        o->setProperty ("length", s.lengthSteps);
        o->setProperty ("gain", s.gain);
        o->setProperty ("locked", s.locked);
        arr.add (juce::var (o.get()));
    }
    root->setProperty ("slots", arr);
    return juce::var (root.get());
}

RiffSketch RiffSketch::fromVar (const juce::var& v)
{
    RiffSketch r;

    const int grid = (int) v.getProperty ("grid", 4);
    r.gridSteps = std::find (gridChoices().begin(), gridChoices().end(), grid) != gridChoices().end() ? grid : 4;
    r.lastSeed = v.getProperty ("seed", "0").toString().getLargeIntValue();

    if (auto* arr = v.getProperty ("slots", {}).getArray())
    {
        for (int i = 0; i < juce::jmin (kRiffSlots, arr->size()); ++i)
        {
            const auto& o = arr->getReference (i);
            auto& s = r.slots[(size_t) i];
            s.path          = o.getProperty ("path", {}).toString();
            s.offsetSeconds = juce::jmax (0.0, (double) o.getProperty ("offset", 0.0));
            s.lengthSteps   = juce::jlimit (1, kMaxSlotSteps, (int) o.getProperty ("length", 4));
            s.gain          = juce::jlimit (0.0f, 2.0f, (float) (double) o.getProperty ("gain", 1.0));
            s.locked        = (bool) o.getProperty ("locked", false);
        }
    }

    return r;
}

juce::String RiffSketch::toJson() const
{
    return juce::JSON::toString (toVar(), false);
}

bool RiffSketch::fromJson (const juce::String& json, RiffSketch& out)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject() || v.getProperty ("format", {}).toString() != "foundry-riff")
        return false;

    out = fromVar (v);
    return true;
}

juce::String stepsName (int steps)
{
    switch (steps)
    {
        case 1:  return "1/16";
        case 2:  return "1/8";
        case 4:  return "1/4";
        case 8:  return "1/2";
        case 12: return "3/4";
        case 16: return "1 bar";
        default: break;
    }

    // Reduce n/16 (3/16, 3/8, 5/16, ...).
    int num = steps, den = 16;
    while (num % 2 == 0 && den > 1)
    {
        num /= 2;
        den /= 2;
    }
    return juce::String (num) + "/" + juce::String (den);
}

const std::array<int, 4>& gridChoices() noexcept
{
    static const std::array<int, 4> choices { 1, 2, 4, 8 };
    return choices;
}

juce::File riffSidecarFor (const juce::File& wav)
{
    return wav.withFileExtension (kRiffSidecarExtension);
}

} // namespace foundry
