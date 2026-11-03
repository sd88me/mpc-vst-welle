// MPC port shim: only juce::MidiMessageCollector (from juce_audio_devices, minus its MidiInputCallback side) is used.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include "juce_MidiMessageCollector.h"
