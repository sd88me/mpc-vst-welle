#pragma once

#include <juce_core/juce_core.h>

namespace wave::firmware
{
class Bundle
{
public:
    enum class Authenticity
    {
        missing,
        recognisedCompatible,
        verifiedOs1700
    };

    struct Report
    {
        Authenticity authenticity = Authenticity::missing;
        juce::String summary;
        juce::String detail;
        juce::String masterSha256;
        juce::String voiceSha256;
        juce::String version;

        [[nodiscard]] bool hasBothImages() const noexcept
        {
            return authenticity != Authenticity::missing;
        }
    };

    Report load(const juce::File& directoryOrImage);
    Report loadImages(const juce::MemoryBlock& master, const juce::MemoryBlock& voice,
                      const juce::File& sourceDirectory = {});
    void clear();

    [[nodiscard]] const Report& getReport() const noexcept { return report; }
    [[nodiscard]] const juce::MemoryBlock& getMasterImage() const noexcept { return masterImage; }
    [[nodiscard]] const juce::MemoryBlock& getVoiceImage() const noexcept { return voiceImage; }
    [[nodiscard]] juce::File getDirectory() const noexcept { return directory; }

    static constexpr auto knownMasterHash =
        "4282457d9bf7d70da2e2aa4d6a1e69467c178d0d8d8be0a27f524ab8d62e3286";
    static constexpr auto knownVoiceHash =
        "bdf379b07785af313068191961ee5ef408e267740c1b598290732477bf9d3f68";

private:
    static juce::File findImage(const juce::File& root, const juce::String& name);
    static bool containsAscii(const juce::MemoryBlock& image, const char* text);
    static juce::String sha256(const juce::MemoryBlock& image);
    static juce::String detectVersion(const juce::MemoryBlock& image);
    Report validateLoadedImages();

    juce::File directory;
    juce::MemoryBlock masterImage;
    juce::MemoryBlock voiceImage;
    Report report;
};
} // namespace wave::firmware
