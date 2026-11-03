// Print the processor's parameters as JSON lines: id, name, kind, range / choices, default.
#include "PluginProcessor.h"
#include <cstdio>
#include <memory>
int main()
{
    auto p = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
    for (auto* base : p->getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(base);
        juce::String kind = "float", extra;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(base)) { kind = "choice"; extra = "\"choices\":[";
            for (int i = 0; i < c->choices.size(); ++i) extra << (i ? "," : "") << juce::JSON::toString(c->choices[i]); extra << "],"; }
        else if (dynamic_cast<juce::AudioParameterBool*>(base)) kind = "bool";
        else if (dynamic_cast<juce::AudioParameterInt*>(base)) kind = "int";
        auto range = r->getNormalisableRange();
        std::printf("{\"id\":%s,\"name\":%s,\"kind\":\"%s\",%s\"min\":%g,\"max\":%g,\"step\":%g,\"skew\":%g,\"def\":%g,\"label\":%s}\n",
            juce::JSON::toString(r->getParameterID()).toRawUTF8(), juce::JSON::toString(r->getName(64)).toRawUTF8(), kind.toRawUTF8(), extra.toRawUTF8(),
            range.start, range.end, range.interval, range.skew, range.convertFrom0to1(r->getDefaultValue()), juce::JSON::toString(r->getLabel()).toRawUTF8());
    }
}
