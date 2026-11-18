#include "Patterns.h"

namespace foundry
{

bool PatternFile::hasSamples() const noexcept
{
    for (const auto& s : samples)
        if (s.isNotEmpty())
            return true;
    return false;
}

juce::String PatternFile::toJson() const
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("format", "foundry-pattern");
    root->setProperty ("version", 1);
    root->setProperty ("kind", kind == PatternKind::Bass ? "bass" : "drums");
    root->setProperty ("bars", grid.bars);
    root->setProperty ("bpm", bpm);
    root->setProperty ("genre", genreName (genre));

    juce::Array<juce::var> rows, smp;
    for (size_t r = 0; r < 4; ++r)
    {
        // Compact and readable: "1000001000000010" per row.
        juce::String line;
        for (auto v : grid.rows[r])
            line << (v != 0 ? '1' : '0');
        rows.add (line);
        smp.add (samples[r]);
    }
    root->setProperty ("rows", rows);
    root->setProperty ("rowNames", juce::var (patternRowNames (kind)));
    root->setProperty ("samples", smp);
    return juce::JSON::toString (juce::var (root.get()), false);
}

bool PatternFile::fromJson (const juce::String& json, PatternFile& out)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject() || v.getProperty ("format", {}).toString() != "foundry-pattern")
        return false;

    PatternFile p;
    p.kind = v.getProperty ("kind", "drums").toString() == "bass" ? PatternKind::Bass : PatternKind::Drums;
    p.grid.setBars (juce::jlimit (1, 8, (int) v.getProperty ("bars", 1)));
    p.grid.clear();
    p.bpm = juce::jmax (0.0, (double) v.getProperty ("bpm", 0.0));
    p.genre = genreFromName (v.getProperty ("genre", "Riddim").toString());

    if (auto* rows = v.getProperty ("rows", {}).getArray())
    {
        for (int r = 0; r < juce::jmin (4, rows->size()); ++r)
        {
            const auto line = rows->getReference (r).toString();
            for (int s = 0; s < juce::jmin (p.grid.numSteps(), line.length()); ++s)
                p.grid.setStep (r, s, line[s] == '1');
        }
    }

    if (auto* smp = v.getProperty ("samples", {}).getArray())
        for (int r = 0; r < juce::jmin (4, smp->size()); ++r)
            p.samples[(size_t) r] = smp->getReference (r).toString();

    out = std::move (p);
    return true;
}

bool PatternFile::save (const juce::File& file) const
{
    if (! file.getParentDirectory().isDirectory())
        file.getParentDirectory().createDirectory();
    return file.replaceWithText (toJson());
}

bool PatternFile::load (const juce::File& file, PatternFile& out)
{
    return file.existsAsFile() && fromJson (file.loadFileAsString(), out);
}

juce::File patternSidecarFor (const juce::File& audio)
{
    return audio.withFileExtension (kPatternExtension);
}

juce::String patternKindName (PatternKind k)
{
    return k == PatternKind::Bass ? "Bass" : "Drums";
}

juce::StringArray patternRowNames (PatternKind k)
{
    if (k == PatternKind::Bass)
        return { "A", "B", "C", "D" };
    return { "KICK", "CLAP", "HAT", "RIDE" };
}

} // namespace foundry
